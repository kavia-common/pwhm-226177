/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2022 SoftAtHome
**
** Redistribution and use in source and binary forms, with or
** without modification, are permitted provided that the following
** conditions are met:
**
** 1. Redistributions of source code must retain the above copyright
** notice, this list of conditions and the following disclaimer.
**
** 2. Redistributions in binary form must reproduce the above
** copyright notice, this list of conditions and the following
** disclaimer in the documentation and/or other materials provided
** with the distribution.
**
** Subject to the terms and conditions of this license, each
** copyright holder and contributor hereby grants to those receiving
** rights under this license a perpetual, worldwide, non-exclusive,
** no-charge, royalty-free, irrevocable (except for failure to
** satisfy the conditions of this license) patent license to make,
** have made, use, offer to sell, sell, import, and otherwise
** transfer this software, where such license applies only to those
** patent claims, already acquired or hereafter acquired, licensable
** by such copyright holder or contributor that are necessarily
** infringed by:
**
** (a) their Contribution(s) (the licensed copyrights of copyright
** holders and non-copyrightable additions of contributors, in
** source or binary form) alone; or
**
** (b) combination of their Contribution(s) with the work of
** authorship to which such Contribution(s) was added by such
** copyright holder or contributor, if, at the time the Contribution
** is added, such addition causes such combination to be necessarily
** infringed. The patent license shall not apply to any other
** combinations which include the Contribution.
**
** Except as expressly stated above, no rights or licenses from any
** copyright holder or contributor is granted under this license,
** whether expressly, by implication, estoppel or otherwise.
**
** DISCLAIMER
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
** CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
** INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
** MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
** CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
** USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
** AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
** ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
** POSSIBILITY OF SUCH DAMAGE.
**
****************************************************************************/

#include <net/if.h>
#include "swl/swl_common.h"
#include "swla/swla_chanspec.h"
#include "wld/wld_radio.h"
#include "wld/wld_channel.h"
#include "wld/wld_chanmgt.h"
#include "wld/wld_util.h"
#include "wld/wld_linuxIfUtils.h"
#include "wld/wld_linuxIfStats.h"
#include "wld/wld_rad_nl80211.h"
#include "wld/wld_rad_hostapd_api.h"
#include "wld/wld_eventing.h"
#include "wifiGen_rad.h"
#include "wifiGen_hapd.h"
#include "wifiGen_events.h"
#include "wifiGen_fsm.h"

#define ME "genHapd"



SWL_TABLE(sPowerTable,
          ARR(int8_t powPct; int32_t powReduction; ),
          ARR(swl_type_int8, swl_type_int32, ),
          ARR(
              {1, 20},
              {2, 17},
              {3, 15},
              {4, 14},
              {5, 13},
              {6, 12},
              {8, 11},
              {10, 10},
              {12, 9},
              {15, 8},
              {20, 7},
              {25, 6},
              {30, 5},
              {50, 3},
              {100, 0},
              {-1, 0},
              ));

static const char* s_defaultRegDomain = "DE";

static void s_radScanStatusUpdateCb(wld_scanEvent_t* event);
static wld_event_callback_t s_radScanStatusCbContainer = {
    .callback = (wld_event_callback_fun) s_radScanStatusUpdateCb
};

int wifiGen_rad_miscHasSupport(T_Radio* pRad, T_AccessPoint* pAp, char* buf, int bufsize) {
    ASSERTS_NOT_NULL(buf, 0, ME, "NULL");
    uint32_t band = 0;
    if((pRad != NULL) || ((pAp != NULL) && ((pRad = pAp->pRadio) != NULL))) {
        band = pRad->operatingFrequencyBand;
    }
    if(!buf[0]) {
        /* Get full list */
        swl_str_copy(buf, bufsize, wld_rad_getSuppDrvCaps(pRad, band));
        return 1;
    }
    bool ret = (wld_rad_findSuppDrvCap(pRad, band, buf) == SWL_TRL_TRUE);
    if(ret) {
        if(swl_str_matches(buf, "MLO")) {
            //Only consider MLO capability when running multiband single hostapd
            ret &= (wifiGen_hapd_countGrpMembers(pRad) > 1);
        }
    }
    return ret;
}

int wifiGen_rad_createHook(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    pRad->wlRadio_SK = wld_linuxIfUtils_getNetSock();
    if(pRad->wlRadio_SK < 0) {
        SAH_TRACE_ERROR("Unable to create socket");
        return SWL_RC_ERROR;
    }
    wifiGen_hapd_init(pRad);
    wifiGen_setRadEvtHandlers(pRad);
    wld_event_add_callback(gWld_queue_rad_onScan_change, &s_radScanStatusCbContainer);
    return SWL_RC_OK;
}

void wifiGen_rad_destroyHook(T_Radio* pRad) {
    wld_event_remove_callback(gWld_queue_rad_onScan_change, &s_radScanStatusCbContainer);
    wifiGen_hapd_cleanup(pRad);
    wld_rad_nl80211_delEvtListener(pRad);
    free(pRad->pLastSurvey);
    free(pRad->pLastAirStats);
    if(pRad->wlRadio_SK > 0) {
        close(pRad->wlRadio_SK);
        pRad->wlRadio_SK = -1;
    }
    amxp_timer_delete(&pRad->setMaxNumStaTimer);
}

static void s_updateNrAntenna(T_Radio* pRad, wld_nl80211_wiphyInfo_t* pWiphyInfo) {
    if(pWiphyInfo->nSSMax > 0) {
        pRad->nrAntenna[COM_DIR_TRANSMIT] = pRad->nrAntenna[COM_DIR_RECEIVE] = pWiphyInfo->nSSMax;
    } else {
        pRad->nrAntenna[COM_DIR_TRANSMIT] = pWiphyInfo->nrAntenna[COM_DIR_TRANSMIT];
        pRad->nrAntenna[COM_DIR_RECEIVE] = pWiphyInfo->nrAntenna[COM_DIR_RECEIVE];
    }
    pRad->nrActiveAntenna[COM_DIR_TRANSMIT] = pWiphyInfo->nrActiveAntenna[COM_DIR_TRANSMIT];
    pRad->nrActiveAntenna[COM_DIR_RECEIVE] = pWiphyInfo->nrActiveAntenna[COM_DIR_RECEIVE];
}

swl_channel_t s_getDefaultSupportedChannel(wld_nl80211_bandDef_t* pBand) {
    swl_channel_t defChan = SWL_CHANNEL_INVALID;
    ASSERTS_NOT_NULL(pBand, defChan, ME, "NULL");
    for(uint32_t i = 0; i < pBand->nChans; i++) {
        swl_chanspec_t chanSpec = SWL_CHANSPEC_EMPTY;
        if(swl_chanspec_channelFromMHz(&chanSpec, pBand->chans[i].ctrlFreq) != SWL_RC_OK) {
            continue;
        }
        defChan = wld_chanmgt_getBetterDefaultChannel((swl_freqBandExt_e) pBand->freqBand, defChan, chanSpec.channel);
    }
    return defChan;
}

#define SUITE(oui, id)  (((oui) << 8) | (id))
/* cipher suite selectors */
#define WLAN_CIPHER_SUITE_USE_GROUP     SUITE(0x000FAC, 0)
#define WLAN_CIPHER_SUITE_WEP40         SUITE(0x000FAC, 1)
#define WLAN_CIPHER_SUITE_TKIP          SUITE(0x000FAC, 2)
/* reserved:                            SUITE(0x000FAC, 3) */
#define WLAN_CIPHER_SUITE_CCMP          SUITE(0x000FAC, 4)
#define WLAN_CIPHER_SUITE_WEP104        SUITE(0x000FAC, 5)
#define WLAN_CIPHER_SUITE_AES_CMAC      SUITE(0x000FAC, 6)
#define WLAN_CIPHER_SUITE_GCMP          SUITE(0x000FAC, 8)
#define WLAN_CIPHER_SUITE_GCMP_256      SUITE(0x000FAC, 9)
#define WLAN_CIPHER_SUITE_CCMP_256      SUITE(0x000FAC, 10)
#define WLAN_CIPHER_SUITE_BIP_GMAC_128  SUITE(0x000FAC, 11)
#define WLAN_CIPHER_SUITE_BIP_GMAC_256  SUITE(0x000FAC, 12)
#define WLAN_CIPHER_SUITE_BIP_CMAC_256  SUITE(0x000FAC, 13)
SWL_TABLE(sCiphersMap,
          ARR(uint32_t cipherSuite; char* cipherName; ),
          ARR(swl_type_uint32, swl_type_charPtr, ),
          ARR({WLAN_CIPHER_SUITE_WEP40, "WEP"},
              {WLAN_CIPHER_SUITE_WEP104, "WEP"},
              {WLAN_CIPHER_SUITE_TKIP, "TKIP"},
              {WLAN_CIPHER_SUITE_CCMP, "AES"},
              {WLAN_CIPHER_SUITE_AES_CMAC, "AES_CCM"},
              {WLAN_CIPHER_SUITE_GCMP, "SAE"},
              {WLAN_CIPHER_SUITE_GCMP_256, "SAE"},
              ));

static void s_initialiseCapabilities(T_Radio* pRad, wld_nl80211_wiphyInfo_t* pWiphyInfo) {
    SAH_TRACEZ_IN(ME);
    SAH_TRACEZ_INFO(ME, "init cap %s", pRad->Name);
    wld_rad_init_cap(pRad);
    wld_rad_clearSuppDrvCaps(pRad);

    ASSERT_NOT_NULL(pWiphyInfo, , ME, "NULL");
    for(uint32_t i = 0; i < SWL_FREQ_BAND_MAX; i++) {
        wld_nl80211_bandDef_t* pBand = &pWiphyInfo->bands[i];
        if(pBand->nChans > 0) {
            SAH_TRACEZ_INFO(ME, "process band(%d) nchan(%d)", pBand->freqBand, pBand->nChans);
            if(SWL_BIT_IS_SET(pBand->chanWidthMask, SWL_BW_320MHZ)) {
                wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "320MHz");
            }
            if(SWL_BIT_IS_SET(pBand->chanWidthMask, SWL_BW_160MHZ)) {
                wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "160MHz");
            }
            if(pWiphyInfo->suppUAPSD) {
                wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "UAPSD");
            }
            for(uint32_t j = 0; j < pWiphyInfo->nCipherSuites; j++) {
                uint32_t cipherSuite = pWiphyInfo->cipherSuites[j];
                if((pBand->freqBand == SWL_FREQ_BAND_6GHZ) && (cipherSuite < WLAN_CIPHER_SUITE_GCMP)) {
                    continue;
                }
                wld_rad_addSuppDrvCap(pRad, pBand->freqBand, (char*) swl_table_getMatchingValue(&sCiphersMap, 1, 0, &cipherSuite));
            }
            if(((pBand->freqBand == SWL_FREQ_BAND_5GHZ) &&
                (pBand->bfCapsSupported[COM_DIR_TRANSMIT] & (M_RAD_BF_CAP_VHT_SU | M_RAD_BF_CAP_VHT_MU))) ||
               (pBand->bfCapsSupported[COM_DIR_TRANSMIT] & (M_RAD_BF_CAP_HE_SU | M_RAD_BF_CAP_HE_MU))) {
                wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "EXPL_BF");
            }
            if(((pBand->freqBand == SWL_FREQ_BAND_5GHZ) &&
                (pBand->bfCapsSupported[COM_DIR_RECEIVE] & (M_RAD_BF_CAP_VHT_SU | M_RAD_BF_CAP_VHT_MU))) ||
               (pBand->bfCapsSupported[COM_DIR_RECEIVE] & (M_RAD_BF_CAP_HE_SU | M_RAD_BF_CAP_HE_MU))) {
                wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "IMPL_BF");
            }
            if((pBand->radStdsMask & M_SWL_RADSTD_BE)) {
                if((pBand->bfCapsSupported[COM_DIR_TRANSMIT] &
                    (M_RAD_BF_CAP_EHT_SU | M_RAD_BF_CAP_EHT_MU_80MHZ | M_RAD_BF_CAP_EHT_MU_160MHZ | M_RAD_BF_CAP_EHT_MU_320MHZ))) {
                    wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "EXPL_BF");
                }
                if((pBand->bfCapsSupported[COM_DIR_RECEIVE] &
                    (M_RAD_BF_CAP_EHT_SU | M_RAD_BF_CAP_EHT_MU_80MHZ | M_RAD_BF_CAP_EHT_MU_160MHZ | M_RAD_BF_CAP_EHT_MU_320MHZ))) {
                    wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "IMPL_BF");
                }
            }
            if(pBand->bfCapsSupported[COM_DIR_TRANSMIT] &
               (M_RAD_BF_CAP_VHT_MU | M_RAD_BF_CAP_HE_MU | M_RAD_BF_CAP_EHT_MU_80MHZ | M_RAD_BF_CAP_EHT_MU_160MHZ | M_RAD_BF_CAP_EHT_MU_320MHZ)) {
                wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "MU_MIMO");
            }
            if(pWiphyInfo->suppFeatures.dfsOffload) {
                wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "DFS_OFFLOAD");
            }
            if(pWiphyInfo->suppFeatures.sae) {
                wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "SAE");
                wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "OWE");
            }
            if(pWiphyInfo->suppFeatures.sae_pwe) {
                wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "SAE_PWE");
            }
            if(pWiphyInfo->suppCmds.channelSwitch) {
                wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "CSA");
            }
            if(pWiphyInfo->suppCmds.survey) {
                wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "SURVEY");
            }
            if(pWiphyInfo->suppCmds.WMMCapability) {
                wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "WME");
            }
            if(pWiphyInfo->suppFeatures.scanDwell) {
                wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "SCAN_DWELL");
            }
            if(pWiphyInfo->suppFeatures.backgroundRadar) {
                wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "RADAR_BACKGROUND");
                wld_bgdfs_setAvailable(pRad, true);
            }
            if(pWiphyInfo->suppMlo) {
                wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "MLO");
            }
            if(pWiphyInfo->maxMbssidAdsIfaces > 0) {
                W_SWL_BIT_SET(pRad->suppMbssidAdsModes, MBSSID_ADVERTISEMENT_MODE_ON);
                if(pWiphyInfo->maxMbssidEmaPeriod > 0) {
                    wld_rad_addSuppDrvCap(pRad, pBand->freqBand, "EMA");
                    W_SWL_BIT_SET(pRad->suppMbssidAdsModes, MBSSID_ADVERTISEMENT_MODE_ENHANCED);
                }
            }
            SAH_TRACEZ_INFO(ME, "%s: Caps[%s]={%s}", pRad->Name, swl_freqBand_str[pBand->freqBand], wld_rad_getSuppDrvCaps(pRad, pBand->freqBand));
            if((pRad->channel == 0) && (wld_rad_getFreqBand(pRad) == pBand->freqBand)) {
                swl_channel_t defChan = s_getDefaultSupportedChannel(pBand);
                if(defChan) {
                    pRad->channel = defChan;
                    SAH_TRACEZ_INFO(ME, "%s: get first supported channel %u", pRad->Name, pRad->channel);
                }
            }
        }
    }
    SAH_TRACEZ_OUT(ME);
}

static void s_updateBandAndStandard(T_Radio* pRad, wld_nl80211_bandDef_t bands[], wld_nl80211_ifaceInfo_t* pIfaceInfo) {
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    pRad->supportedFrequencyBands = M_SWL_FREQ_BAND_EXT_NONE;
    pRad->operatingFrequencyBand = SWL_FREQ_BAND_EXT_NONE;
    pRad->supportedStandards = 0;
    pRad->runningChannelBandwidth = SWL_RAD_BW_AUTO;
    pRad->maxChannelBandwidth = SWL_BW_AUTO;
    ASSERT_NOT_NULL(bands, , ME, "NULL");
    for(uint32_t i = 0; i < SWL_FREQ_BAND_MAX; i++) {
        wld_nl80211_bandDef_t* pBand = &bands[i];
        if(pBand->nChans > 0) {
            W_SWL_BIT_CLEAR(pRad->supportedFrequencyBands, SWL_FREQ_BAND_EXT_NONE);
            W_SWL_BIT_SET(pRad->supportedFrequencyBands, pBand->freqBand);
            pRad->supportedStandards |= pBand->radStdsMask;
        }
    }
    if(pIfaceInfo) {
        swl_chanspec_t chanSpec;
        SAH_TRACEZ_INFO(ME, "convert freq:%d", pIfaceInfo->chanSpec.ctrlFreq);
        if(swl_chanspec_channelFromMHz(&chanSpec, pIfaceInfo->chanSpec.ctrlFreq) >= SWL_RC_OK) {
            SAH_TRACEZ_INFO(ME, "get band:%d chan:%d", chanSpec.band, chanSpec.channel);
            pRad->channel = chanSpec.channel;
            pRad->operatingFrequencyBand = chanSpec.band;
            W_SWL_BIT_CLEAR(pRad->supportedFrequencyBands, SWL_FREQ_BAND_EXT_NONE);
            W_SWL_BIT_SET(pRad->supportedFrequencyBands, pRad->operatingFrequencyBand);

            if(pIfaceInfo->chanSpec.chanWidth == 20) {
                // 20MHz needs to be handled separately as in this case centerFreq may not be set, which causes an error in swl_chanspec_fromFreqCtrlCentre
                pRad->runningChannelBandwidth = SWL_RAD_BW_20MHZ;
            } else {
                swl_chanspec_fromFreqCtrlCentre(&chanSpec, chanSpec.band, pIfaceInfo->chanSpec.ctrlFreq, pIfaceInfo->chanSpec.centerFreq1);
                pRad->runningChannelBandwidth = swl_chanspec_toRadBw(&chanSpec);
            }

        } else if((pIfaceInfo->chanSpec.chanWidth > 0) && (pIfaceInfo->chanSpec.chanWidth < 320)) {
            // At 320MHz, there are multiple possible runningChannelBw, so we need to be able to parse centre channel
            pRad->runningChannelBandwidth = (swl_radBw_e) swl_chanspec_intToBw(pIfaceInfo->chanSpec.chanWidth);
        }
    }
    if(pRad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_NONE) {
        swl_freqBandExt_m otherRadsFB = M_SWL_FREQ_BAND_EXT_NONE | M_SWL_FREQ_BAND_EXT_AUTO | wld_getAvailableFreqBands(pRad);
        swl_freqBandExt_m ownRadFB = pRad->supportedFrequencyBands & (~otherRadsFB);
        /*
         * If only one band supported take it.
         * If multiple bands are supported OR we don't know, fine tune selection based on available antennas
         * ans considering already registered radios (exclusive freq bands, except for 5ghz where multiple device can be detected
         */
        if(swl_bit32_getNrSet(ownRadFB) >= 1) {
            pRad->operatingFrequencyBand = swl_bit32_getHighest(ownRadFB);
        } else if((pRad->nrAntenna[COM_DIR_TRANSMIT] <= 2) && (SWL_BIT_IS_SET(ownRadFB, SWL_FREQ_BAND_EXT_2_4GHZ))) {
            pRad->operatingFrequencyBand = SWL_FREQ_BAND_EXT_2_4GHZ;
        } else if((pRad->nrAntenna[COM_DIR_TRANSMIT] > 2) && (SWL_BIT_IS_SET(ownRadFB, SWL_FREQ_BAND_EXT_6GHZ))) {
            pRad->operatingFrequencyBand = SWL_FREQ_BAND_EXT_6GHZ;
        } else if((pRad->nrAntenna[COM_DIR_TRANSMIT] >= 2) && (SWL_BIT_IS_SET(pRad->supportedFrequencyBands, SWL_FREQ_BAND_EXT_5GHZ))) {
            //allow multiple 5ghz radio device guessing
            pRad->operatingFrequencyBand = SWL_FREQ_BAND_EXT_5GHZ;
        } else {
            pRad->operatingFrequencyBand = SWL_FREQ_BAND_EXT_AUTO;
        }
        SAH_TRACEZ_INFO(ME, "%s: deduce freqBand %s",
                        pRad->Name,
                        swl_freqBandExt_unknown_str[pRad->operatingFrequencyBand]);
    }
    if(pRad->supportedStandards == 0) {
        if(SWL_BIT_IS_SET(pRad->supportedFrequencyBands, SWL_FREQ_BAND_EXT_2_4GHZ)) {
            W_SWL_BIT_SET(pRad->supportedStandards, SWL_RADSTD_B);
            W_SWL_BIT_SET(pRad->supportedStandards, SWL_RADSTD_G);
        }
        if(SWL_BIT_IS_SET(pRad->supportedFrequencyBands, SWL_FREQ_BAND_EXT_5GHZ)) {
            W_SWL_BIT_SET(pRad->supportedStandards, SWL_RADSTD_A);
        }
    }
    if(pRad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_2_4GHZ) {
        W_SWL_BIT_CLEAR(pRad->supportedStandards, SWL_RADSTD_A);
        W_SWL_BIT_CLEAR(pRad->supportedStandards, SWL_RADSTD_AC);
    } else if(pRad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_5GHZ) {
        W_SWL_BIT_CLEAR(pRad->supportedStandards, SWL_RADSTD_B);
        W_SWL_BIT_CLEAR(pRad->supportedStandards, SWL_RADSTD_G);
    } else if(pRad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_6GHZ) {
        W_SWL_BIT_CLEAR(pRad->supportedStandards, SWL_RADSTD_A);
        W_SWL_BIT_CLEAR(pRad->supportedStandards, SWL_RADSTD_B);
        W_SWL_BIT_CLEAR(pRad->supportedStandards, SWL_RADSTD_G);
        W_SWL_BIT_CLEAR(pRad->supportedStandards, SWL_RADSTD_N);
        W_SWL_BIT_CLEAR(pRad->supportedStandards, SWL_RADSTD_AC);
    }
    wld_nl80211_bandDef_t* pOperBand = &bands[pRad->operatingFrequencyBand];
    if(pOperBand->chanWidthMask > 0) {
        pRad->maxChannelBandwidth = swl_bit32_getHighest(pOperBand->chanWidthMask);
        if(pRad->runningChannelBandwidth == SWL_RAD_BW_AUTO) {
            pRad->runningChannelBandwidth = wld_chanmgt_getAutoBw(pRad, wld_rad_getSwlChanspec(pRad));
        }
    } else if(pRad->runningChannelBandwidth == SWL_RAD_BW_AUTO) {
        switch(pRad->operatingFrequencyBand) {
        case SWL_FREQ_BAND_EXT_5GHZ:
            pRad->runningChannelBandwidth = SWL_RAD_BW_80MHZ;
            break;
        case SWL_FREQ_BAND_EXT_6GHZ:
            pRad->runningChannelBandwidth = SWL_RAD_BW_160MHZ;
            break;
        default:
            pRad->runningChannelBandwidth = SWL_RAD_BW_20MHZ;
            break;
        }
        pRad->maxChannelBandwidth = swl_radBw_toBw[pRad->runningChannelBandwidth];
    }
    pRad->IEEE80211hSupported = (pRad->supportedFrequencyBands & M_SWL_FREQ_BAND_5GHZ) ? 1 : 0;
    pRad->IEEE80211kSupported = true;
    pRad->IEEE80211rSupported = true;
    if(SWL_BIT_IS_SET(pRad->supportedStandards, SWL_RADSTD_AX)) {
        pRad->heCapsSupported = M_HE_CAP_DL_OFDMA | M_HE_CAP_UL_OFDMA | M_HE_CAP_DL_MUMIMO;

        /*
         * Since 802.11ax supports both DL and UL MU-MIMO, we could theoretically set the
         * UL MU-MIMO flag directly. However, to verify whether the hardware truly supports
         * UL MU-MIMO, we rely on the HE Phy capabilities. If the hardware has full or partial
         * UL MU-MIMO support, the UL MU-MIMO flag will be set.
         */
        swl_80211_hePhyCapInfo_m hePhyCap = 0;
        memcpy(&hePhyCap, pOperBand->hePhyCapabilities.cap, sizeof(swl_80211_hePhyCapInfo_m));
        if(hePhyCap & (M_SWL_80211_HE_PHY_FULL_UL_MU_MIMO | M_SWL_80211_HE_PHY_PARTIAL_UL_MU_MIMO)) {
            pRad->heCapsSupported |= M_HE_CAP_UL_MUMIMO;
        }
    }
    //updating capabilities
    pRad->htCapabilities = pOperBand->htCapabilities;
    memcpy(pRad->htMcsSet, pOperBand->htMcsSet, sizeof(pRad->htMcsSet));
    pRad->vhtCapabilities = pOperBand->vhtCapabilities;
    memcpy(pRad->vhtMcsNssSet, pOperBand->vhtMcsNssSet, sizeof(pRad->vhtMcsNssSet));
    pRad->heMacCapabilities = pOperBand->heMacCapabilities;
    pRad->hePhyCapabilities = pOperBand->hePhyCapabilities;
    memcpy(pRad->heMcsCaps, pOperBand->heMcsCaps, sizeof(pRad->heMcsCaps));
    pRad->supportedDataTransmitRates = pOperBand->supportedDataTransmitRates;
}

void s_readChanInfo(T_Radio* pRad, wld_nl80211_bandDef_t* pOperBand) {
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    wld_channel_clear_flags(pRad);
    ASSERT_NOT_NULL(pOperBand, , ME, "NULL");
    for(uint32_t i = 0; i < pOperBand->nChans; i++) {
        wld_nl80211_chanDesc_t* pChan = &pOperBand->chans[i];
        swl_chanspec_t chanSpec;
        if(swl_chanspec_channelFromMHz(&chanSpec, pChan->ctrlFreq) < SWL_RC_OK) {
            continue;
        }
        wld_channel_mark_available_channel(chanSpec);
        if(pChan->isDfs) {
            SAH_TRACEZ_INFO(ME, "%s: mark radar req, channel [%d] clear_time [%d]", pRad->Name, chanSpec.channel, pChan->dfsCacTime);
            wld_channel_mark_radar_req_channel(chanSpec);
            wld_channel_set_channel_clear_time(chanSpec.channel, pChan->dfsCacTime);
        }
        switch(pChan->status) {
        case WLD_NL80211_CHAN_AVAILABLE: wld_channel_clear_passive_channel(chanSpec); break;
        case WLD_NL80211_CHAN_USABLE: wld_channel_mark_passive_channel(chanSpec); break;
        case WLD_NL80211_CHAN_UNAVAILABLE: wld_channel_mark_radar_detected_channel(chanSpec); break;
        default: break;
        }
    }
}

swl_rc_ne s_updateChannels(T_Radio* pRad, wld_nl80211_bandDef_t* pOperBand) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pOperBand, SWL_RC_INVALID_PARAM, ME, "NULL");
    pRad->nrPossibleChannels = 0;
    for(uint32_t i = 0; i < pOperBand->nChans; i++) {
        wld_nl80211_chanDesc_t* pChan = &pOperBand->chans[i];
        swl_chanspec_t chanSpec;
        if((swl_chanspec_channelFromMHz(&chanSpec, pChan->ctrlFreq) < SWL_RC_OK) ||
           (chanSpec.channel == 0)) {
            SAH_TRACEZ_WARNING(ME, "%s: skip unknown freq %d", pRad->Name, pChan->ctrlFreq);
            continue;
        }
        if(pRad->nrPossibleChannels == (int) SWL_ARRAY_SIZE(pRad->possibleChannels)) {
            SAH_TRACEZ_WARNING(ME, "%s: skip saving supported chan %d freq %d (out of limit)",
                               pRad->Name, chanSpec.channel, pChan->ctrlFreq);
            continue;
        }
        pRad->possibleChannels[pRad->nrPossibleChannels] = chanSpec.channel;
        pRad->nrPossibleChannels++;
    }

    uint32_t maxBwInt = 0;
    swl_bandwidth_e maxBw = SWL_BW_AUTO;
    swl_radBw_m suppChW = M_SWL_RAD_BW_AUTO;
    for(uint32_t radBw = SWL_RAD_BW_AUTO; radBw < SWL_RAD_BW_MAX; radBw++) {
        uint32_t radBwInt = swl_chanspec_radBwToInt(radBw);
        if(SWL_BIT_IS_SET(pOperBand->chanWidthMask, swl_chanspec_intToBw(radBwInt))) {
            if(radBwInt <= maxBwInt) {
                continue;
            }
            maxBwInt = radBwInt;
            maxBw = swl_chanspec_intToBw(maxBwInt);
            W_SWL_BIT_SET(suppChW, radBw);
            if(radBw == SWL_RAD_BW_320MHZ1) {
                //assume extension high is supported by standard
                W_SWL_BIT_SET(suppChW, SWL_RAD_BW_320MHZ2);
            }
        }
    }
    pRad->maxChannelBandwidth = maxBw;
    pRad->supportedChannelBandwidth = suppChW;

    wld_channel_init_channels(pRad);
    s_readChanInfo(pRad, pOperBand);
    return SWL_RC_OK;
}

int wifiGen_rad_poschans(T_Radio* pRad, uint8_t* buf _UNUSED, int bufsize _UNUSED) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_TRUE(pRad->operatingFrequencyBand < (uint32_t) SWL_FREQ_BAND_MAX, SWL_RC_INVALID_PARAM, ME, "Invalid");
    wld_nl80211_wiphyInfo_t wiphyInfo;
    swl_rc_ne rc = wld_rad_nl80211_getWiphyInfo(pRad, &wiphyInfo);
    ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "Fail to get nl80211 wiphy info");
    wld_nl80211_bandDef_t* pOperBand = &wiphyInfo.bands[pRad->operatingFrequencyBand];
    rc = s_updateChannels(pRad, pOperBand);
    wld_rad_write_possible_channels(pRad);
    wld_chanmgt_writeDfsChannels(pRad);
    return rc;
}

int wifiGen_rad_supports(T_Radio* pRad, char* buf _UNUSED, int bufsize _UNUSED) {
    SAH_TRACEZ_IN(ME);
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    pRad->index = if_nametoindex(pRad->Name);
    pRad->runningChannelBandwidth = SWL_RAD_BW_AUTO;
    pRad->operatingStandards = M_SWL_RADSTD_AUTO;
    /* Fill in all our RO fields... */
    pRad->autoChannelSupported = 1;
    pRad->enable = 1;            // By default it's on
    pRad->status = RST_UNKNOWN;  // It's hard to track odl is settings his default here.
    pRad->detailedState = CM_RAD_UNKNOWN;
    pRad->maxBitRate = 54;

    //toggle to force loading nl80211 wiphy caps
    int radState = wld_linuxIfUtils_getState(wld_rad_getSocket(pRad), pRad->Name);
    if(radState == 0) {
        wld_linuxIfUtils_setState(wld_rad_getSocket(pRad), pRad->Name, 1);
        wld_linuxIfUtils_setState(wld_rad_getSocket(pRad), pRad->Name, 0);
    }

    //set default regdomain to let driver update wiphy band channels
    //before getting wiphy info
    if(!swl_rc_isOk(wifiGen_hapd_getConfiguredCountryCode(pRad, pRad->regulatoryDomain, sizeof(pRad->regulatoryDomain)))) {
        swl_str_copy(pRad->regulatoryDomain, sizeof(pRad->regulatoryDomain), s_defaultRegDomain);
    }
    getCountryParam(pRad->regulatoryDomain, 0, &pRad->regulatoryDomainIdx);
    pRad->pFA->mfn_wrad_regdomain(pRad, NULL, 0, SET | DIRECT);

    wld_nl80211_wiphyInfo_t wiphyInfo;
    swl_rc_ne rc = wld_rad_nl80211_getWiphyInfo(pRad, &wiphyInfo);
    ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "Fail to get nl80211 wiphy info");
    wld_nl80211_ifaceInfo_t ifaceInfo;
    rc = wld_rad_nl80211_getInterfaceInfo(pRad, &ifaceInfo);
    ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "Fail to get nl80211 rad iface info");
    pRad->wiphy = ifaceInfo.wiphy;
    swl_str_copy(pRad->wiphyName, sizeof(pRad->wiphyName), wiphyInfo.name);
    pRad->wDevId = ifaceInfo.wDevId;
    wifiGen_setRadEvtHandlers(pRad);

    pRad->isAP = wiphyInfo.suppAp;
    if(pRad->isAP) {
        pRad->maxNrHwBss = wiphyInfo.nApMax;
        pRad->maxStations = wiphyInfo.nStaMax;
    }
    //memcpy(pRad->MACAddr, ifaceInfo.mac.bMac, ETHER_ADDR_LEN);

    s_updateNrAntenna(pRad, &wiphyInfo);
    s_updateBandAndStandard(pRad, wiphyInfo.bands, &ifaceInfo);

    wld_nl80211_bandDef_t* pOperBand = &wiphyInfo.bands[pRad->operatingFrequencyBand];
    pRad->bfCapsSupported[COM_DIR_TRANSMIT] = pOperBand->bfCapsSupported[COM_DIR_TRANSMIT];
    pRad->bfCapsSupported[COM_DIR_RECEIVE] = pOperBand->bfCapsSupported[COM_DIR_RECEIVE];
    s_initialiseCapabilities(pRad, &wiphyInfo);
    SAH_TRACEZ_INFO(ME, "%s: band:%d, chan:%d", pRad->Name, pRad->operatingFrequencyBand, pRad->channel);

    //rad->channelInUse;      /* 32 Bit pattern that will mark all channels? */
    if(pRad->channel == 0) {
        pRad->channel = swl_channel_defaults[wld_rad_getFreqBand(pRad)];
    }
    s_updateChannels(pRad, pOperBand);

    pRad->channelBandwidthChangeReason = CHAN_REASON_INITIAL;
    pRad->channelChangeReason = CHAN_REASON_INITIAL;
    pRad->autoChannelSupported = TRUE;
    pRad->autoChannelEnable = FALSE;
    pRad->autoChannelRefreshPeriod = FALSE;
    pRad->extensionChannel = REXT_AUTO;          /* Auto */
    pRad->guardInterval = SWL_SGI_AUTO;          /* Auto == 400NSec*/
    pRad->MCS = 10;
    if(pOperBand->radStdsMask & M_SWL_RADSTD_BE) {
        pRad->MCS = pOperBand->mcsStds[SWL_MCS_STANDARD_EHT].mcsIndex;
    } else if(pOperBand->radStdsMask & M_SWL_RADSTD_AX) {
        pRad->MCS = pOperBand->mcsStds[SWL_MCS_STANDARD_HE].mcsIndex;
    } else if(pOperBand->radStdsMask & M_SWL_RADSTD_AC) {
        pRad->MCS = pOperBand->mcsStds[SWL_MCS_STANDARD_VHT].mcsIndex;
    } else if(pOperBand->radStdsMask & M_SWL_RADSTD_N) {
        pRad->MCS = pOperBand->mcsStds[SWL_MCS_STANDARD_HT].mcsIndex;
    }

    swl_table_columnToArrayOffset(pRad->transmitPowerSupported, 64, &sPowerTable, 0, 0);

    pRad->transmitPower = 100;
    pRad->setRadio80211hEnable = (pRad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_5GHZ );
    pRad->m_multiAPTypesSupported = M_MULTIAP_ALL;

    wifiGen_hapd_initGlobDmnCap(pRad);

    /* First time force full config */
    pRad->fsmRad.FSM_SyncAll = TRUE;

    /* Enable WPS 2.0 -- Do this only when you know that WPS 2.0 is supported & needed! */
    pRad->wpsConst->wpsSupVer = 2;

    if(pOperBand->radStdsMask & M_SWL_RADSTD_BE) {
        pRad->cap.apCap7.emlmrSupported = wiphyInfo.extCapas.emlmrSupport[WLD_WIPHY_IFTYPE_AP];
        pRad->cap.staCap7.emlmrSupported = wiphyInfo.extCapas.emlmrSupport[WLD_WIPHY_IFTYPE_STATION];
        pRad->cap.apCap7.emlsrSupported = wiphyInfo.extCapas.emlsrSupport[WLD_WIPHY_IFTYPE_AP];
        pRad->cap.staCap7.emlsrSupported = wiphyInfo.extCapas.emlsrSupport[WLD_WIPHY_IFTYPE_STATION];
        /*
         * Simultaneous Transmit Receive (STR) and Non-Simulatenous Transmit Receive (NSTR) modes
         * are basically supported in wifi7, as for Enhanced Multi-Link Single Radio (EMLSR)
         */
        pRad->cap.apCap7.strSupported = pRad->cap.apCap7.nstrSupported = pRad->cap.apCap7.emlsrSupported;
        pRad->cap.staCap7.strSupported = pRad->cap.staCap7.nstrSupported = pRad->cap.staCap7.emlsrSupported;
    }

    SAH_TRACEZ_OUT(ME);
    return SWL_RC_OK;
}

int wifiGen_rad_status(T_Radio* pRad) {
    swl_chanspec_t currChanSpec;
    T_EndPoint* pActiveEp;
    if(!wld_rad_hasEnabledIface(pRad)) {
        SAH_TRACEZ_INFO(ME, "%s: has no enabled interface", pRad->Name);
        pRad->detailedState = CM_RAD_DOWN;
    } else if(wld_bgdfs_isRunning(pRad)) {
        // In this situation, there is a background dfs running.
        SAH_TRACEZ_INFO(ME, "%s: background dfs is running", pRad->Name);
    } else if(((pActiveEp = wld_rad_getFirstActiveEp(pRad)) != NULL) &&
              ((wld_linuxIfUtils_getLinkStateExt(pActiveEp->Name) > 0) || (!wld_rad_hasEnabledVap(pRad)))) {
        /*
         * if radio has active backhaul:
         *  - endpoint connected (iface LINK_UP)
         *  - or just activated EndPoint (iface UP), with no VAPs (inactive fronthaul)
         * then
         *  radio status may be considered UP even if passive
         */
        SAH_TRACEZ_INFO(ME, "%s: radio is up, used by enabled endpoint %s", pRad->Name, pActiveEp->Name);
        pRad->detailedState = CM_RAD_UP;
    } else if(pRad->pFA->mfn_wrad_getChanspec(pRad, &currChanSpec) < SWL_RC_OK) {
        SAH_TRACEZ_INFO(ME, "%s: has no available channel", pRad->Name);
        pRad->detailedState = CM_RAD_DOWN;
    } else if(!wld_channel_is_band_usable(currChanSpec)) {
        /*
         * In this situation, some channel clearing must be done, or is on going
         * The radio detailed status is updated through the driver/hostapd eventing.
         */
        SAH_TRACEZ_INFO(ME, "%s: curr band(c:%d/b:%d) is not yet usable",
                        pRad->Name, currChanSpec.channel, swl_chanspec_bwToInt(currChanSpec.bandwidth));
    } else if(wld_rad_hasActiveIface(pRad)) {
        /*
         * radio is considered up when:
         * 1) channel info available
         * 2) radio's main/child interface is operational (i.e running with lower up)
         * 3) current chanspec is usable (it is safety check, as net link may remain RUNNING even
         *    after a dfs radar detection)
         * 4) no background dfs is running
         */
        SAH_TRACEZ_INFO(ME, "%s: radio is up", pRad->Name);
        pRad->detailedState = CM_RAD_UP;
    }
    // All other intermediate states are handled with eventing (nl80211/wpactrl)
    return (pRad->detailedState != CM_RAD_DOWN);
}

static bool s_doRadDisable(T_Radio* pRad) {
    SAH_TRACEZ_INFO(ME, "%s: disable rad", pRad->Name);
    wld_linuxIfUtils_setState(wld_rad_getSocket(pRad), pRad->Name, false);
    return true;
}

static bool s_doRadEnable(T_Radio* pRad) {
    SAH_TRACEZ_INFO(ME, "%s: Enable rad", pRad->Name);
    if(pRad->isSTA && pRad->isSTASup) {
        wld_rad_nl80211_setSta(pRad);
        wld_rad_nl80211_set4Mac(pRad, true);
        if(wld_rad_hasEnabledEp(pRad) && (wld_vep_from_name(pRad->Name) == NULL)) {
            SAH_TRACEZ_WARNING(ME, "%s: rad managed iface remains passive", pRad->Name);
            return true;
        }
    } else if(pRad->isAP) {
        wld_rad_nl80211_set4Mac(pRad, false);
        wld_rad_nl80211_setAp(pRad);
    }
    wld_linuxIfUtils_setState(wld_rad_getSocket(pRad), pRad->Name, true);
    return true;
}

int wifiGen_rad_enable(T_Radio* rad, int val, int flag) {
    int ret;
    SAH_TRACEZ_INFO(ME, "%d --> %d -  %d", rad->enable, val, flag);
    if(flag & SET) {
        ret = val;
        if(flag & DIRECT) {
            if(val) {
                s_doRadEnable(rad);
            } else {
                s_doRadDisable(rad);
            }
        } else {
            setBitLongArray(rad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_ENABLE_RAD);
        }
    } else {
        /* GET */
        if(flag & DIRECT) {
            ret = (wld_linuxIfUtils_getState(wld_rad_getSocket(rad), rad->Name) == true);
        } else {
            ret = rad->enable;
        }
    }
    return ret;
}

int wifiGen_rad_sync(T_Radio* pRad, int set) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_TRUE(set & SET, SWL_RC_INVALID_PARAM, ME, "Get Only");
    SAH_TRACEZ_INFO(ME, "%s : set rad_sync", pRad->Name);
    // Rad sync: just toggles and re-apply current config with secDmn
    ASSERTI_FALSE(set & DIRECT, SWL_RC_OK, ME, "%s: no rad conf can be directly applied/synced out of secDmn", pRad->Name);
    setBitLongArray(pRad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_SYNC_RAD);
    // Rad MaxAssociatedDevices is implemented through AP MaxAssociatedDevices, so we need to set GEN_FSM_MOD_AP
    setBitLongArray(pRad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_MOD_AP);
    return SWL_RC_OK;
}

int wifiGen_rad_restart(T_Radio* pRad, int set) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_TRUE(set & SET, SWL_RC_INVALID_PARAM, ME, "Set Only");
    SAH_TRACEZ_INFO(ME, "%s : set rad_restart", pRad->Name);
    setBitLongArray(pRad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_STOP_HOSTAPD);
    setBitLongArray(pRad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_START_HOSTAPD);
    wld_rad_doCommitIfUnblocked(pRad);
    return SWL_RC_OK;
}

int wifiGen_rad_refresh(T_Radio* pRad, int set) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_TRUE(set & SET, SWL_RC_INVALID_PARAM, ME, "Set Only");
    SAH_TRACEZ_INFO(ME, "%s : set rad_refresh", pRad->Name);
    setBitLongArray(pRad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_UPDATE_HOSTAPD);
    wld_rad_doCommitIfUnblocked(pRad);
    return SWL_RC_OK;
}

int wifiGen_rad_toggle(T_Radio* pRad, int set) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_TRUE(set & SET, SWL_RC_INVALID_PARAM, ME, "Set Only");
    SAH_TRACEZ_INFO(ME, "%s : set rad_toggle", pRad->Name);
    setBitLongArray(pRad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_DISABLE_HOSTAPD);
    setBitLongArray(pRad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_ENABLE_HOSTAPD);
    wld_rad_doCommitIfUnblocked(pRad);
    return SWL_RC_OK;
}

swl_rc_ne wifiGen_rad_regDomain(T_Radio* pRad, char* val, int bufsize, int set) {
    if(set & SET) {
        const char* countryName = getShortCountryName(pRad->regulatoryDomainIdx);
        ASSERT_FALSE(swl_str_isEmpty(countryName), SWL_RC_INVALID_PARAM,
                     ME, "regulatoryDomainIdx %d not valid!", pRad->regulatoryDomainIdx);
        swl_str_copy(pRad->regulatoryDomain, sizeof(pRad->regulatoryDomain), countryName);
        if(set & DIRECT) {
            return wld_rad_nl80211_setRegDomain(pRad, countryName);
        }
        if(wifiGen_hapd_isAlive(pRad)) {
            wld_ap_hostapd_setParamValue(wld_rad_hostapd_getCfgMainVap(pRad), "country_code", pRad->regulatoryDomain, "updating regulatory domain");
        }
        setBitLongArray(pRad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_MOD_COUNTRYCODE);
    } else {
        swl_str_copy(val, bufsize, pRad->regulatoryDomain);
    }
    return SWL_RC_OK;
}

int32_t s_getMaxPow(T_Radio* pRad) {
    wld_nl80211_wiphyInfo_t pWiphyInfo;
    memset(&pWiphyInfo, 0, sizeof(wld_nl80211_wiphyInfo_t));

    swl_rc_ne ret = wld_rad_nl80211_getWiphyInfo(pRad, &pWiphyInfo);
    ASSERT_TRUE(ret == SWL_RC_OK, 0, ME, "Err %i", ret);
    uint32_t curFreq = 0;
    swl_chanspec_t spec = wld_rad_getSwlChanspec(pRad);
    swl_chanspec_channelToMHz(&spec, &curFreq);

    for(uint32_t i = 0; i < SWL_FREQ_BAND_MAX; i++) {
        if(pWiphyInfo.bands[i].nChans == 0) {
            continue;
        }
        for(uint32_t j = 0; j < pWiphyInfo.bands[i].nChans; j++) {
            if(pWiphyInfo.bands[i].chans[j].ctrlFreq == curFreq) {
                return (int32_t) pWiphyInfo.bands[i].chans[j].maxTxPwr;
            }
        }
    }
    return 0;

}

swl_rc_ne wifiGen_rad_getTxPowerdBm(T_Radio* rad, int32_t* dbm) {
    ASSERTS_TRUE(wld_rad_hasActiveIface(rad), SWL_RC_ERROR, ME, "%s not ready", rad->Name);
    return wld_rad_nl80211_getTxPower(rad, dbm);
}

swl_rc_ne wifiGen_rad_getMaxTxPowerdBm(T_Radio* pRad, uint16_t channel, int32_t* dbm) {
    return wld_rad_nl80211_getMaxTxPowerdBm(pRad, channel, dbm);
}

int wifiGen_rad_txpow(T_Radio* pRad, int val, int set) {
    if(set & SET) {
        pRad->transmitPower = val;
        ASSERTS_TRUE(wld_rad_hasActiveIface(pRad), SWL_RC_ERROR, ME, "%s not ready", pRad->Name);

        if(val == -1) {
            return wld_rad_nl80211_setTxPowerAuto(pRad);
        }

        int8_t srcVal = val;
        int32_t* tgtVal = (int32_t*) swl_table_getMatchingValue(&sPowerTable, 1, 0, &srcVal);
        ASSERT_NOT_NULL(tgtVal, SWL_RC_ERROR, ME, "%s: unknown txPow percentage %d", pRad->Name, val);


        int32_t maxPow = s_getMaxPow(pRad);
        ASSERT_NOT_EQUALS(maxPow, 0, SWL_RC_ERROR, ME, "%s maxPow unknown", pRad->Name);

        int32_t tgtPow = (*tgtVal > maxPow ? 0 : maxPow - *tgtVal);

        swl_rc_ne retVal = SWL_RC_OK;

        SAH_TRACEZ_INFO(ME, "%s: setPow %i / max %i diff %i => %i", pRad->Name, val, maxPow, *tgtVal, tgtPow);

        retVal = wld_rad_nl80211_setTxPowerLimited(pRad, tgtPow);

        return (retVal == SWL_RC_OK ? WLD_OK : WLD_ERROR);
    } else {
        ASSERTS_TRUE(wld_rad_hasActiveIface(pRad), pRad->transmitPower, ME, "%s not ready", pRad->Name);
        int32_t maxPow = s_getMaxPow(pRad);
        ASSERT_NOT_EQUALS(maxPow, 0, SWL_RC_ERROR, ME, "%s: maxPow unknown", pRad->Name);

        int32_t curDbm;
        swl_rc_ne retVal = wld_rad_nl80211_getTxPower(pRad, &curDbm);
        ASSERT_EQUALS(retVal, SWL_RC_OK, SWL_RC_ERROR, ME, "%s: fail to read curr txPow", pRad->Name);

        if(curDbm == 0) {
            //auto set as 0
            return -1;
        }
        int32_t diff = (maxPow - curDbm);

        int8_t* tgtVal = (int8_t*) swl_table_getMatchingValue(&sPowerTable, 0, 1, &diff);
        ASSERTI_NOT_NULL(tgtVal, pRad->transmitPower, ME, "%s: no tgtVal %i (%i/%i)", pRad->Name, diff, curDbm, maxPow);

        return *tgtVal;
    }

}

/*
 * @brief called to check and start ZW DFS, if enabled and needed, before setting
 * the new chanspec.
 *
 * @param pRad radio context
 * @param direct whether direct set chanspec
 *
 * @return SWL_RC_DONE in case of no ZW DFS needed, SWL_RC_CONTINUE otherwise.
 */
static swl_rc_ne s_checkAndStartZwDfs(T_Radio* pRad, bool direct) {
    if(!wld_rad_is_5ghz(pRad) ||
       !swl_channel_isDfs(pRad->targetChanspec.chanspec.channel) ||
       !wld_rad_isUpExt(pRad) ||
       (wld_chanmgt_getCurBw(pRad) > pRad->maxChannelBandwidth) ||
       (pRad->bgdfs_config.status == BGDFS_STATUS_OFF)) {
        return SWL_RC_DONE;
    }
    if(wld_channel_is_band_passive(pRad->targetChanspec.chanspec)) {
        swl_rc_ne rc = pRad->pFA->mfn_wrad_zwdfs_start(pRad, direct);
        if(rc >= SWL_RC_OK) {
            return SWL_RC_CONTINUE;
        }
    }
    return SWL_RC_DONE;
}

swl_rc_ne wifiGen_rad_setChanspec(T_Radio* pRad, bool direct) {

    SAH_TRACEZ_INFO(ME, "%s: direct:%d", pRad->Name, direct);

    swl_rc_ne rc = s_checkAndStartZwDfs(pRad, direct);
    SAH_TRACEZ_INFO(ME, "%s: ZW DFS status: %s", pRad->Name, swl_rc_toString(rc));
    if(rc != SWL_RC_DONE) {
        return rc;
    }

    if(!direct) {
        setBitLongArray(pRad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_MOD_CHANNEL);
        return SWL_RC_OK;
    }
    bool needCommit = (pRad->fsmRad.FSM_State != FSM_RUN);
    unsigned long* actionArray = (needCommit ? pRad->fsmRad.FSM_BitActionArray : pRad->fsmRad.FSM_AC_BitActionArray);
    if(wifiGen_hapd_isAlive(pRad)) {
        chanmgt_rad_state detState = pRad->detailedState;
        wifiGen_hapd_getRadState(pRad, &detState);
        bool isTgtChspecRunning = false;
        swl_chanspec_t runningChSpec = SWL_CHANSPEC_EMPTY;
        if((detState == CM_RAD_UP) || (detState == CM_RAD_FG_CAC)) {
            if(swl_rc_isOk(wld_rad_nl80211_getChannel(pRad, &runningChSpec))) {
                isTgtChspecRunning = (swl_typeChanspec_equals(runningChSpec, pRad->targetChanspec.chanspec));
            }
            if((pRad->pFA->mfn_misc_has_support(pRad, NULL, "CSA", 0)) &&
               ((wld_channel_is_band_usable(pRad->targetChanspec.chanspec)) ||
                (pRad->pFA->mfn_misc_has_support(pRad, NULL, "DFS_OFFLOAD", 0)))) {
                if(isTgtChspecRunning) {
                    SAH_TRACEZ_WARNING(ME, "%s: chanspec %s already running, nothing new to be warm applied",
                                       pRad->Name, swl_typeChanspecExt_toBuf32(pRad->targetChanspec.chanspec).buf);
                    return SWL_RC_DONE;
                }
                rc = wld_rad_hostapd_switchChannel(pRad);
                rc = (rc < SWL_RC_OK) ? SWL_RC_ERROR : SWL_RC_DONE;
                goto saveConf;
            }
        }
        if(isTgtChspecRunning && (pRad->autoChannelEnable == (wld_rad_hostapd_getCfgChannel(pRad) == 0))) {
            SAH_TRACEZ_WARNING(ME, "%s: chanspec %s already running, nothing new to be cold applied",
                               pRad->Name, swl_typeChanspecExt_toBuf32(pRad->targetChanspec.chanspec).buf);
            return SWL_RC_DONE;
        }
        /*
         * channel can not be warm applied (with CSA)
         * then hostapd must be toggled for cold applying (with DFS clearing if needed)
         */
        SAH_TRACEZ_WARNING(ME, "%s: connected cold applying of chanspec %s",
                           pRad->Name, swl_typeChanspecExt_toBuf32(pRad->targetChanspec.chanspec).buf);
        wld_rad_hostapd_setChannel(pRad);
        if(detState != CM_RAD_DOWN) {
            setBitLongArray(actionArray, FSM_BW, GEN_FSM_DISABLE_HOSTAPD);
        }
        setBitLongArray(actionArray, FSM_BW, GEN_FSM_ENABLE_HOSTAPD);
    } else {
        SAH_TRACEZ_WARNING(ME, "%s: disconnected, no applying of chanspec %s",
                           pRad->Name, swl_typeChanspecExt_toBuf32(pRad->targetChanspec.chanspec).buf);
    }
    rc = SWL_RC_OK;
saveConf:
    /* update conf file */
    setBitLongArray(actionArray, FSM_BW, GEN_FSM_MOD_HOSTAPD);
    if(needCommit) {
        wld_rad_doCommitIfUnblocked(pRad);
    }
    return rc;
}

swl_rc_ne wifiGen_rad_getChanspec(T_Radio* pRad, swl_chanspec_t* pChSpec) {
    return wld_rad_nl80211_getChannel(pRad, pChSpec);
}

uint32_t s_writeAntennaCtrl(T_Radio* pRad, com_dir_e comdir) {
    uint32_t maxMask = (1 << pRad->nrAntenna[comdir]) - 1;
    uint32_t setVal = (comdir == COM_DIR_TRANSMIT) ? pRad->txChainCtrl : pRad->rxChainCtrl;
    uint32_t val = pRad->actAntennaCtrl;
    if(setVal <= 0) {
        setVal = ((val <= 0) || (val > maxMask)) ? maxMask : val;
    }
    return setVal;
}
int wifiGen_rad_antennactrl(T_Radio* pRad, int val _UNUSED, int set) {
    ASSERT_TRUE(set & SET, WLD_ERROR_INVALID_PARAM, ME, "Get Only");
    uint32_t txMapAnt = s_writeAntennaCtrl(pRad, COM_DIR_TRANSMIT);
    uint32_t rxMapAnt = s_writeAntennaCtrl(pRad, COM_DIR_RECEIVE);

    swl_rc_ne ret = wld_rad_nl80211_setAntennas(pRad, txMapAnt, rxMapAnt);
    ASSERT_TRUE(swl_rc_isOk(ret), WLD_ERROR, ME, "Error");

    pRad->nrActiveAntenna[COM_DIR_TRANSMIT] = swl_bit32_getNrSet(txMapAnt);
    pRad->nrActiveAntenna[COM_DIR_RECEIVE] = swl_bit32_getNrSet(rxMapAnt);
    wld_radio_updateAntenna(pRad);
    SAH_TRACEZ_INFO(ME, "%s: enable %u, ctrl %i %i-%i ; ant %i/%i -%i/%i",
                    pRad->Name, pRad->enable, pRad->actAntennaCtrl, pRad->txChainCtrl, pRad->rxChainCtrl,
                    pRad->nrActiveAntenna[COM_DIR_TRANSMIT], pRad->nrAntenna[COM_DIR_TRANSMIT],
                    pRad->nrActiveAntenna[COM_DIR_RECEIVE], pRad->nrAntenna[COM_DIR_RECEIVE]);
    return WLD_OK;
}

int wifiGen_rad_staMode(T_Radio* pRad, int val, int set) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    if(set & SET) {
        pRad->isSTA = val;
        pRad->fsmRad.FSM_SyncAll = TRUE;
    }
    return pRad->isSTA;
}

int wifiGen_rad_supstd(T_Radio* pRad, swl_radioStandard_m radioStandards) {
    ASSERTS_NOT_NULL(pRad, -1, ME, "rad is NULL");
    ASSERT_TRUE(swl_radStd_isValid(radioStandards, "rad_supstd"), -1, ME,
                "%s rad_supstd : Invalid operatingStandards %#x",
                pRad->Name, radioStandards);

    SAH_TRACEZ_INFO(ME, "%s : Set standards %#x", pRad->Name, radioStandards);
    pRad->operatingStandards = radioStandards;
    setBitLongArray(pRad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_SYNC_RAD);
    return 1;
}

void wifiGen_rad_initBands(T_Radio* pRad) {
    int i = 0;
    for(i = 0; i < pRad->nrPossibleChannels; i++) {
        swl_chanspec_t chanspec = swl_chanspec_fromDm(pRad->possibleChannels[i], pRad->runningChannelBandwidth, pRad->operatingFrequencyBand);
        wld_channel_mark_available_channel(chanspec);
        if(swl_channel_isDfs(pRad->possibleChannels[i])) {
            wld_channel_mark_radar_req_channel(chanspec);
            wld_channel_mark_passive_channel(chanspec);
        }
    }
}

swl_rc_ne wifiGen_rad_stats(T_Radio* pRad) {
    swl_rc_ne ret;
    T_Stats stats;
    memset(&stats, 0, sizeof(stats));
    if(wld_linuxIfStats_getRadioStats(pRad, &stats)) {
        ret = SWL_RC_OK;
        memcpy(&pRad->stats, &stats, sizeof(stats));
    } else {
        SAH_TRACEZ_WARNING(ME, "%s: get stats for radio fail", pRad->Name);
        ret = SWL_RC_ERROR;
    }
    return ret;
}

int wifiGen_rad_delayedCommitUpdate(T_Radio* pRad) {
    if(pRad->fsmRad.FSM_State != FSM_IDLE) {
        // Do nothing... we're still in commit state.
        pRad->fsmRad.TODC = 10000;
        return 0;
    }
    wld_rad_updateState(pRad, false);
    wifiGen_hapd_syncVapStates(pRad);
    pRad->fsmRad.TODC = 0;
    return 0;
}

/*
 * fill missing noise of detected neigh BSS with spectrum results
 * already updated, after scan completed on all/selected channels
 */
static void s_updateScanResultsWithSpectrumInfo(wld_scanResults_t* pScanResults, wld_spectrumChannelInfoEntry_t* pSpectrumEntry) {
    ASSERTS_NOT_NULL(pScanResults, , ME, "NULL");
    ASSERTS_NOT_NULL(pSpectrumEntry, , ME, "NULL");
    ASSERTS_NOT_EQUALS(pSpectrumEntry->noiselevel, 0, , ME, "noise is null");
    amxc_llist_for_each(it, &pScanResults->ssids) {
        wld_scanResultSSID_t* pNeighBss = amxc_container_of(it, wld_scanResultSSID_t, it);
        if(pNeighBss->channel != pSpectrumEntry->channel) {
            continue;
        }
        pNeighBss->noise = pSpectrumEntry->noiselevel;
        pNeighBss->snr = pNeighBss->rssi - pNeighBss->noise;
        if(pNeighBss->snr < 0) {
            pNeighBss->snr = 0;
        }
    }
}

static void s_radScanStatusUpdateCb(wld_scanEvent_t* event) {
    ASSERT_NOT_NULL(event, , ME, "NULL");
    T_Radio* pRadio = event->pRad;
    ASSERT_NOT_NULL(pRadio, , ME, "NULL");
    if(event->start || !event->success) {
        return;
    }
    //refresh spectrum info based on last scan
    amxc_llist_t* pSpectrumResults = &pRadio->scanState.spectrumResults;
    swl_rc_ne rc = pRadio->pFA->mfn_wrad_getspectruminfo(pRadio, false, pSpectrumResults);
    ASSERT_FALSE(rc < SWL_RC_OK, , ME, "%s: fail to spectrum info", pRadio->Name);
    //fill missing noise and SNR values in ssid scan result entries
    wld_scanResults_t* pScanResults = &pRadio->scanState.lastScanResults;
    amxc_llist_for_each(it, pSpectrumResults) {
        wld_spectrumChannelInfoEntry_t* pSpectrumEntry = amxc_container_of(it, wld_spectrumChannelInfoEntry_t, it);
        s_updateScanResultsWithSpectrumInfo(pScanResults, pSpectrumEntry);
    }
}

swl_rc_ne wifiGen_rad_getScanResults(T_Radio* pRad, wld_scanResults_t* results) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(results, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_scan_cleanupScanResults(results);
    amxc_llist_for_each(it, &pRad->scanState.lastScanResults.ssids) {
        wld_scanResultSSID_t* pResult = amxc_container_of(it, wld_scanResultSSID_t, it);
        wld_scanResultSSID_t* pCopy = calloc(1, sizeof(wld_scanResultSSID_t));
        if(pCopy == NULL) {
            wld_scan_cleanupScanResults(results);
            return SWL_RC_ERROR;
        }
        memcpy(pCopy, pResult, sizeof(*pCopy));
        amxc_llist_it_init(&pCopy->it);
        amxc_llist_append(&results->ssids, &pCopy->it);
    }
    return SWL_RC_OK;
}

swl_rc_ne wifiGen_rad_getAirStats(T_Radio* pRad, wld_airStats_t* pStats) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pStats, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_rad_nl80211_getAirstats(pRad, pStats);
}


swl_rc_ne wifiGen_rad_getSpectrumInfo(T_Radio* rad, bool update, amxc_llist_t* llSpectrumChannelInfo) {
    swl_rc_ne rc;
    ASSERT_NOT_NULL(rad, SWL_RC_INVALID_PARAM, ME, "NULL");

    if(update) {
        //force scan to get updated whole spectrum info
        rc = wld_scan_start(rad, SCAN_TYPE_SPECTRUM, "ForcedSpectrumRefresh");
        ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "%s: fail to force spectrum scan", rad->Name);
        return SWL_RC_CONTINUE;
    }

    //just refresh current channel info
    uint32_t nChanSurveyInfo = 0;
    wld_nl80211_channelSurveyInfo_t* pChanSurveyInfoList = NULL;
    rc = wld_rad_nl80211_getSurveyInfo(rad, &pChanSurveyInfoList, &nChanSurveyInfo);
    ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "%s: fail to get survey info", rad->Name);
    rc = wld_rad_nl80211_updateChanSurveyReportFromSurveyInfo(rad, pChanSurveyInfoList, nChanSurveyInfo);
    if(rc < SWL_RC_OK) {
        SAH_TRACEZ_ERROR(ME, "%s: fail to update survey report", rad->Name);
    }
    rc = wld_rad_nl80211_updateUsageStatsFromSurveyInfo(rad, llSpectrumChannelInfo, pChanSurveyInfoList, nChanSurveyInfo);
    free(pChanSurveyInfoList);
    ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "%s: fail to update usage stats", rad->Name);
    amxc_llist_for_each(it, llSpectrumChannelInfo) {
        wld_spectrumChannelInfoEntry_t* pEntry = amxc_container_of(it, wld_spectrumChannelInfoEntry_t, it);
        pEntry->nrCoChannelAP = wld_util_countScanResultEntriesPerChannel(&rad->scanState.lastScanResults, pEntry->channel);
    }

    return rc;
}

swl_rc_ne wifiGen_rad_bgDfsStartExt(T_Radio* pRad, wld_startBgdfsArgs_t* args) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(args, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_TRUE(pRad->pFA->mfn_misc_has_support(pRad, NULL, "RADAR_BACKGROUND", 0), SWL_RC_ERROR, ME,
                "%s: radar background not supported", pRad->Name);
    ASSERT_TRUE((wld_rad_isUpExt(pRad) && !wld_bgdfs_isRunning(pRad)), SWL_RC_ERROR, ME, "%s: not ready", pRad->Name);

    swl_rc_ne rc = wld_rad_nl80211_bgDfsStart(pRad, args);
    ASSERT_TRUE(swl_rc_isOk(rc), rc, ME, "%s: fail to start bgDfs", pRad->Name);
    // let bgdfs start event update the datamodel
    return rc;
}

swl_rc_ne wifiGen_rad_bgDfsStop(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTI_TRUE(wld_bgdfs_isRunning(pRad), SWL_RC_OK, ME, "%s: bgDfs not running", pRad->Name);

    swl_rc_ne rc = wld_rad_nl80211_bgDfsStop(pRad);
    ASSERT_TRUE(swl_rc_isOk(rc), rc, ME, "%s: fail to stop bgDfs", pRad->Name);
    wld_bgdfs_notifyClearEnded(pRad, DFS_RESULT_OTHER);
    wld_rad_updateState(pRad, false);
    return rc;
}
