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
#include "wld.h"
#include "wld_radio.h"
#include "wld_accesspoint.h"
#include "wld_ssid.h"
#include "wld_util.h"
#include "swl/swl_hex.h"
#include "swl/map/swl_mapCharFmt.h"
#include "swl/swl_common.h"
#include "swl/swl_hash.h"
#include "swl/fileOps/swl_mapWriterKVP.h"
#include "wld_wpaSupp_parser.h"
#include "wld_hostapd_cfgManager.h"
#include "wld_hostapd_cfgFile.h"
#include "wld_rad_hostapd_api.h"
#include "wld_hostapd_ap_api.h"
#include "wld_chanmgt.h"
#include "wld_wpaCtrlMngr.h"
#include "wld_wpaCtrlInterface.h"
#include "wld_wpaCtrl_api.h"
#include "wld_secDmn.h"

#define ME "hapdCfg"

#define IEEE80211_MAX_RTS_THRESHOLD 2347 //MAX RTS Threshold, to make RTS/CTS handshake off

/*
 * @brief names of all WPS configuration methods supported by hostapd
 * matching WPS v2.0 Config Methods
 * */
static const char* s_hostapd_WPS_configMethods_str[] = {
    "usba",
    "ethernet",
    "label",
    "display",
    "ext_nfc_token",
    "int_nfc_token",
    "nfc_interface",
    "push_button",
    "keypad",
    "physical_push_button",
    "physical_display",
    "virtual_push_button",
    "virtual_display",
    0,
};
SWL_ASSERT_STATIC(SWL_ARRAY_SIZE(s_hostapd_WPS_configMethods_str) == (WPS_CFG_MTHD_MAX + 1), "s_hostapd_WPS_configMethods_str not correctly defined");

/*
 * List of rates (in 100 kbps) that are included in the basic rate set.
 * If this item is not included, default basic rate set is used.
 * */
static const char* s_hostapd_DataTransmitRates[] = {"10", "20", "55", "60", "90", "110", "120", "180", "240", "360", "480", "540", 0};

static bool s_checkSGI(T_Radio* pRad, int guardInterval _UNUSED) {
    ASSERTS_NOT_NULL(pRad, false, ME, "NULL");
    //check rad caps (sgi by mcs mode) and match with required guardInterval
    return ((pRad->guardInterval == SWL_SGI_AUTO) || (pRad->guardInterval == SWL_SGI_400));
}

/**
 * @brief convert comma separated string to space separated
 * convert "1,2,5,63" t0 "1 2 5 63"
 * @param commaStr comma separated string
 * @param spaceStr space separated string
 * @param spaceStrLen length of the spaceStr
 * @return void
 */
static void s_commaToSpaceSeparated(const char* commaStr, char* spaceStr, uint32_t spaceStrLen) {
    swl_str_copy(spaceStr, spaceStrLen, NULL);
    swl_str_replace(spaceStr, spaceStrLen, commaStr, ",", " ");
}

/*
 * Mapping of VHT Channel Width IDs from on hostapd.conf @ https://w1.fi/cgit/hostap/plain/hostapd/hostapd.conf
 * # 0 = 20 or 40 MHz operating Channel width
 * # 1 = 80 MHz channel width
 * # 2 = 160 MHz channel width
 * # 3 = 80+80 MHz channel width
 */
SWL_TABLE(sChWidthIDsMaps,
          ARR(uint32_t vhtChWidthIDs; uint32_t ehtChWidthIDs; swl_bandwidth_e swlBw; ),
          ARR(swl_type_uint32, swl_type_uint32, swl_type_uint32, ),
          ARR({0, 0, SWL_BW_20MHZ},
              {0, 0, SWL_BW_40MHZ},
              {1, 1, SWL_BW_80MHZ},
              {2, 2, SWL_BW_160MHZ},
              {2, 9, SWL_BW_320MHZ},
              ));

static swl_rc_ne s_checkAndSetParamValueStr(wld_wpaCtrlInterface_t* pIface, swl_mapChar_t* mapChar, const char* param, const char* valStr) {
    wld_secDmn_t* pSecDmn = wld_wpaCtrlMngr_getSecDmn(wld_wpaCtrlInterface_getMgr(pIface));
    ASSERTS_NOT_NULL(pSecDmn, SWL_RC_INVALID_STATE, ME, "no secDmn");
    ASSERT_STR(param, SWL_RC_INVALID_PARAM, ME, "empty param");
    ASSERT_STR(valStr, SWL_RC_INVALID_PARAM, ME, "empty value");
    swl_trl_e trl = wld_secDmn_getCfgParamSupp(pSecDmn, param);
    ASSERTS_NOT_EQUALS(trl, SWL_TRL_FALSE, SWL_RC_INVALID_PARAM, ME, "param %s not supported", param);
    int32_t ret = 0;
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    if(trl == SWL_TRL_UNKNOWN) {
        rc = wld_wpaCtrl_sendCmdFmtCheckResponse(pIface, "OK", "SET %s %s", param, valStr);
        if(rc == SWL_RC_OK) {
            trl = SWL_TRL_TRUE;
        } else if(rc == SWL_RC_ERROR) {
            trl = SWL_TRL_FALSE;
        }
        if((trl != SWL_TRL_UNKNOWN) || (rc == SWL_RC_INVALID_STATE)) {
            wld_secDmn_setCfgParamSupp(pSecDmn, param, trl);
        }
    }
    if(trl == SWL_TRL_TRUE) {
        ret = swl_mapChar_addOrSet(mapChar, (char*) param, (char*) valStr);
        rc = (ret ? SWL_RC_OK : SWL_RC_ERROR);
    }
    return rc;
}

static swl_rc_ne s_checkAndSetParamValueFmt(wld_wpaCtrlInterface_t* pIface, swl_mapChar_t* mapChar, char* param, const char* valFormat, ...) {
    wld_secDmn_t* pSecDmn = wld_wpaCtrlMngr_getSecDmn(wld_wpaCtrlInterface_getMgr(pIface));
    ASSERTS_NOT_NULL(pSecDmn, SWL_RC_INVALID_STATE, ME, "no secDmn");
    swl_trl_e trl = wld_secDmn_getCfgParamSupp(pSecDmn, param);
    ASSERTS_NOT_EQUALS(trl, SWL_TRL_FALSE, SWL_RC_INVALID_PARAM, ME, "param %s not supported", param);
    char valStr[512] = {0};
    va_list args;
    va_start(args, valFormat);
    int32_t ret = vsnprintf(valStr, sizeof(valStr), valFormat, args);
    va_end(args);
    ASSERT_FALSE(ret < 0, SWL_RC_ERROR, ME, "Fail to format value string");
    return s_checkAndSetParamValueStr(pIface, mapChar, param, valStr);
}

static swl_rc_ne s_checkAndSetParamValueInt32(wld_wpaCtrlInterface_t* pIface, swl_mapChar_t* mapChar, char* param, int32_t value) {
    return s_checkAndSetParamValueFmt(pIface, mapChar, param, "%d", value);
}

void s_filterEntries(swl_mapChar_t* configMap, const char* keys[], uint32_t nKeys) {
    for(uint32_t i = 0; i < nKeys; i++) {
        swl_mapEntry_t* entry;
        if((entry = swl_mapChar_getEntry(configMap, (char*) keys[i])) != NULL) {
            swl_map_deleteEntry(configMap, entry);
        }
    }
}

/**
 * @brief set the radio parameters
 *
 * @param pRad a radio 2.4/5/6GHz
 * @param cfgF a pointer to the configuration file of the hostapd
 *
 * @return void
 */
void wld_hostapd_cfgFile_setRadioConfig(T_Radio* pRad, swl_mapChar_t* radConfigMap) {
    ASSERTS_NOT_NULL(radConfigMap, , ME, "NULL");
    swl_mapChar_add(radConfigMap, "driver", "nl80211");
    if(wld_rad_is_24ghz(pRad)) {
        swl_mapChar_add(radConfigMap, "hw_mode", SWL_BIT_IS_ONLY_SET(pRad->operatingStandards, SWL_RADSTD_B) ? "b" : "g");
    } else {
        swl_mapChar_add(radConfigMap, "hw_mode", "a");
    }
    swl_chanspec_t tgtChspec = wld_chanmgt_getTgtChspec(pRad);

    /* force AcsBootChannel when Radio is down to the next up */
    if(!wld_rad_isUpExt(pRad) &&
       (pRad->autoChannelEnable || (pRad->externalAcsMgmt && pRad->autoChannelSetByUser)) &&
       (pRad->acsBootChannel != -1) &&
       ((tgtChspec.band != SWL_FREQ_BAND_EXT_5GHZ) || (swl_chanspec_isDfs(tgtChspec)))) {
        tgtChspec.channel = pRad->acsBootChannel ? : wld_chanmgt_getDefaultSupportedChannel(pRad);
        tgtChspec.bandwidth = wld_chanmgt_getDefaultSupportedBandwidth(pRad);
    }

    bool enableRad11be = wld_rad_is11beUsable(pRad);
    /*
     * hostapd: Enabling HE is mandatory to enable EHT mode
     * ref: https://git.w1.fi/cgit/hostap/commit/?id=8dcc2139ff8f9d767e46bc09276b55e33561cc35
     */
    bool enableRad11ax = (wld_rad_checkEnabledRadStd(pRad, SWL_RADSTD_AX) || wld_rad_checkEnabledRadStd(pRad, SWL_RADSTD_BE));
    bool enableRad11n = wld_rad_checkEnabledRadStd(pRad, SWL_RADSTD_N);
    bool enableRad11ac = wld_rad_checkEnabledRadStd(pRad, SWL_RADSTD_AC);

    swl_channel_t tgtChan = tgtChspec.channel;
    swl_mapCharFmt_addValInt32(radConfigMap, "channel", tgtChan);
    swl_mapCharFmt_addValInt32(radConfigMap, "op_class", swl_chanspec_getOperClass(&tgtChspec));
    swl_mapChar_add(radConfigMap, "country_code", pRad->regulatoryDomain);
    swl_mapChar_add(radConfigMap, "ieee80211d", "1");
    swl_mapCharFmt_addValInt32(radConfigMap, "ieee80211h", pRad->IEEE80211hSupported && pRad->setRadio80211hEnable);

    swl_bandwidth_e tgtChW = tgtChspec.bandwidth;
    SAH_TRACEZ_INFO(ME, "%s: operStd:0x%x suppStd:0x%x operChw:%d maxChW:%d tgtChW:%d chan:%d",
                    pRad->Name, pRad->operatingStandards, pRad->supportedStandards,
                    pRad->operatingChannelBandwidth, pRad->maxChannelBandwidth, tgtChW, tgtChan);
    char htCaps[256] = {0};
    if(SWL_BIT_IS_SET(pRad->supportedStandards, SWL_RADSTD_N)) {
        swl_mapCharFmt_addValInt32(radConfigMap, "ieee80211n", enableRad11n);
        /*
         * bw greater than 20MHz requires setting second channel offset htcap HT40
         * for 11n and all higer operating standards
         */
        if(wld_channel_hasChannelWidthCovered(tgtChspec, SWL_BW_40MHZ)) {
            wld_channel_extensionPos_e extChanPos = wld_channel_getExtensionChannel(tgtChspec, pRad->extensionChannel);
            if(extChanPos == WLD_CHANNEL_EXTENTION_POS_ABOVE) {
                swl_str_cat(htCaps, sizeof(htCaps), "[HT40+]");
            } else if(extChanPos == WLD_CHANNEL_EXTENTION_POS_BELOW) {
                swl_str_cat(htCaps, sizeof(htCaps), "[HT40-]");
            }
        }
    }
    if(enableRad11n) {
        if(wld_channel_hasChannelWidthCovered(tgtChspec, SWL_BW_40MHZ)) {
            if(s_checkSGI(pRad, SWL_SGI_400)) {
                swl_str_cat(htCaps, sizeof(htCaps), "[SHORT-GI-40]");
            }
            if(pRad->htCapabilities & M_SWL_80211_HTCAPINFO_MODE_40) {
                swl_str_cat(htCaps, sizeof(htCaps), "[DSSS_CCK_MODE_40]");
            }
        }
        if(s_checkSGI(pRad, SWL_SGI_400)) {
            swl_str_cat(htCaps, sizeof(htCaps), "[SHORT-GI-20]");
        }
        if(pRad->htCapabilities & M_SWL_80211_HTCAPINFO_LDPC) {
            swl_str_cat(htCaps, sizeof(htCaps), "[LDPC]");
        }
        if(pRad->htCapabilities & M_SWL_80211_HTCAPINFO_TX_STBC) {
            swl_str_cat(htCaps, sizeof(htCaps), "[TX-STBC]");
        }
        uint32_t n = (pRad->htCapabilities & M_SWL_80211_HTCAPINFO_RX_STBC);
        n >>= swl_bit32_getLowest(M_SWL_80211_HTCAPINFO_RX_STBC);
        if(n > 0) {
            swl_str_cat(htCaps, sizeof(htCaps), "[RX-STBC");
            for(uint32_t i = 1; (i <= n) && (i < 4); i++) {
                /* max supported by hostapd [RX-STBC123] */
                swl_str_catFormat(htCaps, sizeof(htCaps), "%d", i);
            }
            swl_str_cat(htCaps, sizeof(htCaps), "]");
        }
        if(pRad->htCapabilities & M_SWL_80211_HTCAPINFO_MAX_AMSDU) {
            swl_str_cat(htCaps, sizeof(htCaps), "[MAX-AMSDU-7935]");
        }
    }
    /*
     * only add ht_caps tags when they are really supported by the driver (nl80211 phy caps).
     */
    if(!swl_str_isEmpty(htCaps)) {
        swl_mapChar_add(radConfigMap, "ht_capab", htCaps);
    }
    uint32_t* pChWId = (uint32_t*) swl_table_getMatchingValue(&sChWidthIDsMaps, 0, 2, &tgtChW);
    uint32_t* pEhtChWId = (uint32_t*) swl_table_getMatchingValue(&sChWidthIDsMaps, 1, 2, &tgtChW);

    bool implicitBf = (pRad->implicitBeamFormingSupported && pRad->implicitBeamFormingEnabled);
    bool explicitBf = (pRad->explicitBeamFormingSupported && pRad->explicitBeamFormingEnabled);
    bool muMimo = (pRad->multiUserMIMOSupported && pRad->multiUserMIMOEnabled);
    if(SWL_BIT_IS_SET(pRad->supportedStandards, SWL_RADSTD_AC)) {
        swl_mapCharFmt_addValInt32(radConfigMap, "ieee80211ac", enableRad11ac);
    }
    if(enableRad11ac) {
        if(pChWId) {
            if(pEhtChWId && (*pChWId != *pEhtChWId)) {
                tgtChspec.bandwidth = *(uint32_t*) swl_table_getMatchingValue(&sChWidthIDsMaps, 2, 0, pChWId);
            }
            swl_channel_t centerChan = swl_chanspec_getCentreChannel(&tgtChspec);
            swl_mapCharFmt_addValInt32(radConfigMap, "vht_oper_chwidth", *pChWId);
            swl_mapCharFmt_addValInt32(radConfigMap, "vht_oper_centr_freq_seg0_idx", centerChan);
        }
        char vhtCaps[256] = {0};
        if(wld_channel_hasChannelWidthCovered(tgtChspec, SWL_BW_160MHZ)) {
            swl_str_cat(vhtCaps, sizeof(vhtCaps), "[VHT160]");
        }
        if(wld_channel_hasChannelWidthCovered(tgtChspec, SWL_BW_80MHZ) && s_checkSGI(pRad, SWL_SGI_400)) {
            swl_str_cat(vhtCaps, sizeof(vhtCaps), "[SHORT-GI-80]");
        }
        if(implicitBf) {
            if(SWL_BIT_IS_SET(pRad->bfCapsSupported[COM_DIR_RECEIVE], RAD_BF_CAP_VHT_SU) &&
               (SWL_BIT_IS_ONLY_SET(pRad->bfCapsEnabled[COM_DIR_RECEIVE], RAD_BF_CAP_DEFAULT) ||
                SWL_BIT_IS_SET(pRad->bfCapsEnabled[COM_DIR_RECEIVE], RAD_BF_CAP_VHT_SU))) {
                swl_str_cat(vhtCaps, sizeof(vhtCaps), "[SU-BEAMFORMEE]");
            }
        }
        if(explicitBf) {
            if(SWL_BIT_IS_SET(pRad->bfCapsSupported[COM_DIR_TRANSMIT], RAD_BF_CAP_VHT_SU) &&
               (SWL_BIT_IS_ONLY_SET(pRad->bfCapsEnabled[COM_DIR_TRANSMIT], RAD_BF_CAP_DEFAULT) ||
                SWL_BIT_IS_SET(pRad->bfCapsEnabled[COM_DIR_TRANSMIT], RAD_BF_CAP_VHT_SU))) {
                swl_str_cat(vhtCaps, sizeof(vhtCaps), "[SU-BEAMFORMER]");
            }
            if(SWL_BIT_IS_SET(pRad->bfCapsSupported[COM_DIR_TRANSMIT], RAD_BF_CAP_VHT_MU) && muMimo &&
               (SWL_BIT_IS_ONLY_SET(pRad->bfCapsEnabled[COM_DIR_TRANSMIT], RAD_BF_CAP_DEFAULT) ||
                SWL_BIT_IS_SET(pRad->bfCapsEnabled[COM_DIR_TRANSMIT], RAD_BF_CAP_VHT_MU))) {
                swl_str_cat(vhtCaps, sizeof(vhtCaps), "[MU-BEAMFORMER]");
            }
        }
        uint32_t n = (pRad->vhtCapabilities & M_SWL_80211_VHTCAPINFO_MAX_MPDU);
        if(n == 2) {
            swl_str_cat(vhtCaps, sizeof(vhtCaps), "[MAX-MPDU-11454]");
        } else if(n == 1) {
            swl_str_cat(vhtCaps, sizeof(vhtCaps), "[MAX-MPDU-7991]");
        }
        if(pRad->vhtCapabilities & M_SWL_80211_VHTCAPINFO_RX_LDPC) {
            swl_str_cat(vhtCaps, sizeof(vhtCaps), "[RXLDPC]");
        }
        if(pRad->vhtCapabilities & M_SWL_80211_VHTCAPINFO_TX_STBC) {
            swl_str_cat(vhtCaps, sizeof(vhtCaps), "[TX-STBC-2BY1]");
        }
        n = (pRad->vhtCapabilities & M_SWL_80211_VHTCAPINFO_RX_STBC);
        n >>= swl_bit32_getLowest(M_SWL_80211_VHTCAPINFO_RX_STBC);
        if(n > 0) {
            swl_str_cat(vhtCaps, sizeof(vhtCaps), "[RX-STBC-");
            for(uint32_t i = 1; (i <= n) && (i < 5); i++) {
                /* max supported by hostapd [RX-STBC-1234] */
                swl_str_catFormat(vhtCaps, sizeof(vhtCaps), "%d", i);
            }
            swl_str_cat(vhtCaps, sizeof(vhtCaps), "]");
        }
        if(pRad->vhtCapabilities & M_SWL_80211_VHTCAPINFO_TXOP_PS) {
            swl_str_cat(vhtCaps, sizeof(vhtCaps), "[VHT-TXOP-PS]");
        }
        if(pRad->vhtCapabilities & M_SWL_80211_VHTCAPINFO_HTC_CAP) {
            swl_str_cat(vhtCaps, sizeof(vhtCaps), "[HTC-VHT]");
        }
        n = (pRad->vhtCapabilities & M_SWL_80211_VHTCAPINFO_MAX_AMPDU_EXP);
        n >>= swl_bit32_getLowest(M_SWL_80211_VHTCAPINFO_MAX_AMPDU_EXP);
        if(n < 8) {
            swl_str_catFormat(vhtCaps, sizeof(vhtCaps), "[MAX-A-MPDU-LEN-EXP%d]", n);
        }
        if(pRad->vhtCapabilities & M_SWL_80211_VHTCAPINFO_RX_ANT_PAT_CONS) {
            swl_str_cat(vhtCaps, sizeof(vhtCaps), "[RX-ANTENNA-PATTERN]");
        }
        if(pRad->vhtCapabilities & M_SWL_80211_VHTCAPINFO_TX_ANT_PAT_CONS) {
            swl_str_cat(vhtCaps, sizeof(vhtCaps), "[TX-ANTENNA-PATTERN]");
        }
        /*
         * only add vht_caps tags when they are really supported by the driver (nl80211 phy caps).
         */
        if(!swl_str_isEmpty(vhtCaps)) {
            swl_mapChar_add(radConfigMap, "vht_capab", vhtCaps);
        }
    }
    if(SWL_BIT_IS_SET(pRad->supportedStandards, SWL_RADSTD_AX)) {
        swl_mapCharFmt_addValInt32(radConfigMap, "ieee80211ax", enableRad11ax);
    }
    if(enableRad11ax) {
        if(pChWId) {
            if(pEhtChWId && (*pChWId != *pEhtChWId)) {
                tgtChspec.bandwidth = *(uint32_t*) swl_table_getMatchingValue(&sChWidthIDsMaps, 2, 0, pChWId);
            }
            swl_channel_t centerChan = swl_chanspec_getCentreChannel(&tgtChspec);
            swl_mapCharFmt_addValInt32(radConfigMap, "he_oper_chwidth", *pChWId);
            swl_mapCharFmt_addValInt32(radConfigMap, "he_oper_centr_freq_seg0_idx", centerChan);
        }
        if(implicitBf) {
            if(SWL_BIT_IS_SET(pRad->bfCapsSupported[COM_DIR_RECEIVE], RAD_BF_CAP_HE_SU) &&
               (SWL_BIT_IS_ONLY_SET(pRad->bfCapsEnabled[COM_DIR_RECEIVE], RAD_BF_CAP_DEFAULT) ||
                SWL_BIT_IS_SET(pRad->bfCapsEnabled[COM_DIR_RECEIVE], RAD_BF_CAP_HE_SU))) {
                swl_mapCharFmt_addValInt32(radConfigMap, "he_su_beamformee", 1);
            }
        }
        if(explicitBf) {
            if(SWL_BIT_IS_SET(pRad->bfCapsSupported[COM_DIR_TRANSMIT], RAD_BF_CAP_HE_SU) &&
               (SWL_BIT_IS_ONLY_SET(pRad->bfCapsEnabled[COM_DIR_TRANSMIT], RAD_BF_CAP_DEFAULT) ||
                SWL_BIT_IS_SET(pRad->bfCapsEnabled[COM_DIR_TRANSMIT], RAD_BF_CAP_HE_SU))) {
                swl_mapCharFmt_addValInt32(radConfigMap, "he_su_beamformer", 1);
            }
            if(SWL_BIT_IS_SET(pRad->bfCapsSupported[COM_DIR_TRANSMIT], RAD_BF_CAP_HE_MU) && muMimo &&
               (SWL_BIT_IS_ONLY_SET(pRad->bfCapsEnabled[COM_DIR_TRANSMIT], RAD_BF_CAP_DEFAULT) ||
                SWL_BIT_IS_SET(pRad->bfCapsEnabled[COM_DIR_TRANSMIT], RAD_BF_CAP_HE_MU))) {
                swl_mapCharFmt_addValInt32(radConfigMap, "he_mu_beamformer", 1);
            }
        }

        /*
         * Bss Color
         * heBssColor=0 means hostapd is not configured and then it
         * is up to the driver to select the correct color.
         */
        if(pRad->cfg11ax.heBssColor != 0) {
            swl_mapCharFmt_addValInt32(radConfigMap, "he_bss_color", pRad->cfg11ax.heBssColor);
        }
        swl_mapCharFmt_addValInt32(radConfigMap, "he_bss_color_partial", pRad->cfg11ax.heBssColorPartial);

        /*
         * Spatial Reuse Parameter Set
         */
        wld_he_sr_control_m heSprSrControl = (pRad->cfg11ax.hePSRDisallowed << HE_SR_CONTROL_PSR_DISALLOWED) | (0 << HE_SR_CONTROL_NON_SRG_OBSS_PD_SR_DISALLOWED) |
        (pRad->cfg11ax.heNonSRGOffsetValid << HE_SR_CONTROL_NON_SRG_OFFSET_PRESENT) |
        (pRad->cfg11ax.heSRGInformationValid << HE_SR_CONTROL_SRG_INFORMATION_PRESENT) |
        (pRad->cfg11ax.heHESIGASpatialReuseValue15Allowed << HE_SR_CONTROL_HESIGA_SPATIAL_REUSE_VALUE15_ALLOWED);
        swl_mapCharFmt_addValInt32(radConfigMap, "he_spr_sr_control", heSprSrControl);

        if(pRad->cfg11ax.heNonSRGOffsetValid != 0) {
            swl_mapCharFmt_addValInt32(radConfigMap, "he_spr_non_srg_obss_pd_max_offset", pRad->cfg11ax.heSprNonSrgObssPdMaxOffset);
        }

        if(pRad->cfg11ax.heSRGInformationValid != 0) {
            swl_mapCharFmt_addValInt32(radConfigMap, "he_spr_srg_obss_pd_min_offset", pRad->cfg11ax.heSprSrgObssPdMinOffset);
            swl_mapCharFmt_addValInt32(radConfigMap, "he_spr_srg_obss_pd_max_offset", pRad->cfg11ax.heSprSrgObssPdMaxOffset);

            /*
             * SRGBSSColorBitmap and SRGPartialBSSIDBitmap have comma separated format in TR-181 data model.
             * But these parameters are space separated in hostapd.conf:
             * he_spr_srg_bss_colors=1 2 10 63
             * he_spr_srg_partial_bssid=0 1 3 63
             * So need to convert comma separated string to space separated.
             */
            if(pRad->cfg11ax.heSprSrgBssColors[0]) {
                char srgBSSColorBitmap[swl_str_len(pRad->cfg11ax.heSprSrgBssColors) + 1];
                s_commaToSpaceSeparated(pRad->cfg11ax.heSprSrgBssColors, srgBSSColorBitmap, sizeof(srgBSSColorBitmap));
                swl_mapChar_add(radConfigMap, "he_spr_srg_bss_colors", srgBSSColorBitmap);
            }
            if(pRad->cfg11ax.heSprSrgPartialBssid[0]) {
                char srgPartialBSSIDBitmap[swl_str_len(pRad->cfg11ax.heSprSrgPartialBssid) + 1];
                s_commaToSpaceSeparated(pRad->cfg11ax.heSprSrgPartialBssid, srgPartialBSSIDBitmap, sizeof(srgPartialBSSIDBitmap));
                swl_mapChar_add(radConfigMap, "he_spr_srg_partial_bssid", srgPartialBSSIDBitmap);
            }
        }
        if(pRad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_6GHZ) {
            /*
             *  Set default 6GHz power type to 0 - Indoor AP (required for FCC)
             */
            SAH_TRACEZ_INFO(ME, "%s: Adding 6GHz regulatory power type to 0", pRad->Name);
            swl_mapCharFmt_addValInt32(radConfigMap, "he_6ghz_reg_pwr_type", 0);
        }
    }
    if(SWL_BIT_IS_SET(pRad->supportedStandards, SWL_RADSTD_BE)) {
        swl_mapCharFmt_addValInt32(radConfigMap, "ieee80211be", enableRad11be);
    }
    if(enableRad11be) {
        /* ieee80211be: Whether IEEE 802.11be (EHT) is enabled
         * 0 = disabled (default)
         * 1 = enabled
         */

        uint32_t* pEhtChWId = (uint32_t*) swl_table_getMatchingValue(&sChWidthIDsMaps, 1, 2, &tgtChW);
        if(pEhtChWId) {
            if(pChWId && (*pChWId != *pEhtChWId)) {
                tgtChspec.bandwidth = *(uint32_t*) swl_table_getMatchingValue(&sChWidthIDsMaps, 2, 1, pEhtChWId);
            }
            swl_channel_t centerChan = swl_chanspec_getCentreChannel(&tgtChspec);
            swl_mapCharFmt_addValInt32(radConfigMap, "eht_oper_chwidth", *pEhtChWId);
            swl_mapCharFmt_addValInt32(radConfigMap, "eht_oper_centr_freq_seg0_idx", centerChan);
        }
        if(implicitBf) {
            if(SWL_BIT_IS_SET(pRad->bfCapsSupported[COM_DIR_RECEIVE], RAD_BF_CAP_EHT_SU) &&
               (SWL_BIT_IS_ONLY_SET(pRad->bfCapsEnabled[COM_DIR_RECEIVE], RAD_BF_CAP_DEFAULT) ||
                SWL_BIT_IS_SET(pRad->bfCapsEnabled[COM_DIR_RECEIVE], RAD_BF_CAP_EHT_SU))) {
                swl_mapCharFmt_addValInt32(radConfigMap, "eht_su_beamformee", 1);
            }
        }
        if(explicitBf) {
            if(SWL_BIT_IS_SET(pRad->bfCapsSupported[COM_DIR_TRANSMIT], RAD_BF_CAP_EHT_SU) &&
               (SWL_BIT_IS_ONLY_SET(pRad->bfCapsEnabled[COM_DIR_TRANSMIT], RAD_BF_CAP_DEFAULT) ||
                SWL_BIT_IS_SET(pRad->bfCapsEnabled[COM_DIR_TRANSMIT], RAD_BF_CAP_EHT_SU))) {
                swl_mapCharFmt_addValInt32(radConfigMap, "eht_su_beamformer", 1);
            }
            bool beamformerSupported = (tgtChW <= SWL_BW_80MHZ) ? SWL_BIT_IS_SET(pRad->bfCapsSupported[COM_DIR_TRANSMIT], RAD_BF_CAP_EHT_MU_80MHZ) :
                (tgtChW == SWL_BW_160MHZ) ? SWL_BIT_IS_SET(pRad->bfCapsSupported[COM_DIR_TRANSMIT], RAD_BF_CAP_EHT_MU_160MHZ) :
                (tgtChW == SWL_BW_320MHZ) ? SWL_BIT_IS_SET(pRad->bfCapsSupported[COM_DIR_TRANSMIT], RAD_BF_CAP_EHT_MU_320MHZ) : false;
            bool beamformerEnabled = (tgtChW <= SWL_BW_80MHZ) ? SWL_BIT_IS_SET(pRad->bfCapsEnabled[COM_DIR_TRANSMIT], RAD_BF_CAP_EHT_MU_80MHZ) :
                (tgtChW == SWL_BW_160MHZ) ? SWL_BIT_IS_SET(pRad->bfCapsEnabled[COM_DIR_TRANSMIT], RAD_BF_CAP_EHT_MU_160MHZ) :
                (tgtChW == SWL_BW_320MHZ) ? SWL_BIT_IS_SET(pRad->bfCapsEnabled[COM_DIR_TRANSMIT], RAD_BF_CAP_EHT_MU_320MHZ) :
                SWL_BIT_IS_ONLY_SET(pRad->bfCapsEnabled[COM_DIR_TRANSMIT], RAD_BF_CAP_DEFAULT);
            if(muMimo && beamformerSupported && beamformerEnabled) {
                swl_mapCharFmt_addValInt32(radConfigMap, "eht_mu_beamformer", 1);
            }
        }

        swl_bit32_t disabledSubchannelBitmap = wld_rad_calculateDisabledSubchannelsBitmap(pRad, &tgtChspec);
        if(disabledSubchannelBitmap > 0) {
            swl_mapCharFmt_addValInt32(radConfigMap, "punct_bitmap", disabledSubchannelBitmap);
        }
    }

    int32_t rtsThreshold = -1;
    if(pRad->rtsThreshold < IEEE80211_MAX_RTS_THRESHOLD) {
        rtsThreshold = pRad->rtsThreshold;
        swl_mapCharFmt_addValInt32(radConfigMap, "rts_threshold", rtsThreshold);
    }

    if(wld_rad_is_24ghz(pRad)) {
        bool enableShPreamble = (pRad->preambleType == PREAMBLE_TYPE_LONG) ? 0 : 1;
        swl_mapCharFmt_addValStr(radConfigMap, "preamble", "%u", enableShPreamble);
    }

    if((pRad->operationalDataTransmitRates != 0) && (pRad->supportedDataTransmitRates != pRad->operationalDataTransmitRates)) {
        char operationalDataTransmitRates[64] = "";
        swl_conv_maskToCharSep(operationalDataTransmitRates, sizeof(operationalDataTransmitRates),
                               pRad->operationalDataTransmitRates, s_hostapd_DataTransmitRates, SWL_ARRAY_SIZE(s_hostapd_DataTransmitRates), ' ');
        swl_mapCharFmt_addValStr(radConfigMap, "supported_rates", "%s", operationalDataTransmitRates);
    }

    if(pRad->basicDataTransmitRates != 0) {
        char basicDataTransmitRates[64] = "";
        swl_conv_maskToCharSep(basicDataTransmitRates, sizeof(basicDataTransmitRates),
                               pRad->basicDataTransmitRates, s_hostapd_DataTransmitRates, SWL_ARRAY_SIZE(s_hostapd_DataTransmitRates), ' ');
        swl_mapCharFmt_addValStr(radConfigMap, "basic_rates", "%s", basicDataTransmitRates);
    }

    wld_mbssidAdvertisement_mode_e mbssidAdsMode = wld_rad_getMbssidAdsMode(pRad);
    bool configMbssid = (mbssidAdsMode != MBSSID_ADVERTISEMENT_MODE_OFF) && (wld_rad_countEnabledVaps(pRad) > 1);
    if(configMbssid) {
        SAH_TRACEZ_INFO(ME, "%s: Setting MBSSID Advertisement %d", pRad->Name, mbssidAdsMode);
        swl_mapCharFmt_addValInt32(radConfigMap, "mbssid", (mbssidAdsMode == MBSSID_ADVERTISEMENT_MODE_ENHANCED) ? 2 : 1);
    }

    pRad->pFA->mfn_wrad_updateConfigMap(pRad, radConfigMap);

    if(!configMbssid) {
        const char* mbssidKeys[] = {"mbssid", "ema"};
        s_filterEntries(radConfigMap, mbssidKeys, SWL_ARRAY_SIZE(mbssidKeys));
    }

    if(pRad->externalAcsMgmt) {
        SAH_TRACEZ_INFO(ME, "%s: Restore radio channel config due to external ACS mgmt enabled", pRad->Name);
        swl_mapCharFmt_addValInt32(radConfigMap, "channel", tgtChan);
    }
}

/**
 * @brief set the IEEE80211r parameters of a vap
 *
 * @param pAP a vap
 * @param cfgF a pointer to the the configuration file of the hostapd
 *
 * @return void
 */
static void s_setVapIeee80211rConfig(T_AccessPoint* pAP, swl_mapChar_t* vapConfigMap) {
    ASSERTS_NOT_NULL(pAP, , ME, "NULL");
    T_SSID* pSSID = pAP->pSSID;
    ASSERTS_NOT_NULL(pSSID, , ME, "NULL");
    ASSERTS_TRUE(pAP->IEEE80211rEnable, , ME, "11r disabled");
    ASSERTS_NOT_NULL(vapConfigMap, , ME, "NULL");

    uint16_t mdid = ((((pAP->mobilityDomain) >> 8) & 0x00FF) | (((pAP->mobilityDomain) << 8) & 0xFF00));

    swl_mapCharFmt_addValStr(vapConfigMap, "ft_over_ds", "%u", pAP->IEEE80211rFTOverDSEnable);
    swl_mapChar_add(vapConfigMap, "ft_psk_generate_local", "1");
    swl_mapCharFmt_addValStr(vapConfigMap, "mobility_domain", "%04X", mdid);
    swl_mapChar_add(vapConfigMap, "nas_identifier", pAP->NASIdentifier);

    swl_macChar_t bssidWSep;
    swl_hex_fromBytes(bssidWSep.cMac, SWL_MAC_CHAR_LEN, pSSID->BSSID, ETHER_ADDR_LEN, true);
    swl_mapChar_add(vapConfigMap, "r1_key_holder", bssidWSep.cMac);

    ASSERTS_TRUE(pAP->IEEE80211rFTOverDSEnable, , ME, "FT over DS disabled");

    amxc_llist_for_each(it, &pAP->neighbours) {
        T_ApNeighbour* neighbour = amxc_llist_it_get_data(it, T_ApNeighbour, it);
        swl_macChar_t bssidStr;
        swl_mac_binToChar(&bssidStr, (swl_macBin_t*) neighbour->bssid);
        if(swl_str_isEmpty(bssidStr.cMac) || swl_str_isEmpty(neighbour->nasIdentifier) || swl_str_isEmpty(neighbour->r0khkey)) {
            SAH_TRACEZ_ERROR(ME, "Invalid 11r parameters");
            return;
        }
        swl_mapCharFmt_addValStr(vapConfigMap, "r0kh", "%s %s %s", bssidStr.cMac, neighbour->nasIdentifier, neighbour->r0khkey);
        swl_mapCharFmt_addValStr(vapConfigMap, "r1kh", "%s %s %s", bssidStr.cMac, bssidStr.cMac, neighbour->r0khkey);
    }
}

static void s_delConfMapEntry(swl_mapChar_t* configMap, const char* key) {
    swl_mapEntry_t* entry = swl_mapChar_getEntry(configMap, (char*) key);
    if(entry != NULL) {
        swl_mapChar_deleteEntry(configMap, entry);
    }
}
static void s_writeMfConfig(T_AccessPoint* vap, swl_mapChar_t* vapConfigMap) {
    wld_banlist_t banList;
    //hostapd/nl80211 do not support probe filtering, so assume PF entries in dynamic MF
    wld_apMacFilter_getBanList(vap, &banList, true);

    if((vap->MF_Mode == APMFM_OFF) && !banList.staToBan) {
        s_delConfMapEntry(vapConfigMap, "macaddr_acl");
        s_delConfMapEntry(vapConfigMap, "accept_mac_file");
        s_delConfMapEntry(vapConfigMap, "deny_mac_file");
        return;
    }

    char fileBuf[64];
    snprintf(fileBuf, sizeof(fileBuf), "/tmp/hostap_%s.acl", vap->alias);
    FILE* tmpFile = fopen(fileBuf, "w");
    ASSERT_NOT_NULL(tmpFile, , ME, "%s: fail to create acl file (%s)", vap->alias, fileBuf);
    if(vap->MF_Mode == APMFM_WHITELIST) {
        s_delConfMapEntry(vapConfigMap, "deny_mac_file");
        swl_mapChar_add(vapConfigMap, "macaddr_acl", "1");
        swl_mapChar_add(vapConfigMap, "accept_mac_file", fileBuf);
    } else {
        s_delConfMapEntry(vapConfigMap, "accept_mac_file");
        swl_mapChar_add(vapConfigMap, "macaddr_acl", "0");
        swl_mapChar_add(vapConfigMap, "deny_mac_file", fileBuf);
    }
    for(uint32_t i = 0; i < banList.staToBan; i++) {
        fprintf(tmpFile, "%s\n", swl_typeMacBin_toBuf32Ref(&banList.banList[i]).buf);
    }

    fclose(tmpFile);
}

/*
 * @brief Fix key negotiation and installation interop issues
 */
static void s_setSecKeyCacheConf(T_AccessPoint* pAP, swl_mapChar_t* vapConfigMap) {
    ASSERTS_NOT_NULL(pAP, , ME, "NULL");
    ASSERTS_NOT_NULL(vapConfigMap, , ME, "NULL");
    switch(pAP->secModeEnabled) {
    case SWL_SECURITY_APMODE_WPA_WPA2_P:
    case SWL_SECURITY_APMODE_WPA2_P:
    {
        // Disable PKMSA caching
        swl_mapCharFmt_addValInt32(vapConfigMap, "disable_pmksa_caching", 1);
        // Disable Opportunistic Key Caching (aka Proactive Key Caching)
        swl_mapCharFmt_addValInt32(vapConfigMap, "okc", 0);
        // Allow EAPOL key retries
        swl_mapCharFmt_addValInt32(vapConfigMap, "wpa_disable_eapol_key_retries", 0);
        break;
    }
    case SWL_SECURITY_APMODE_WPA2_WPA3_P:
    case SWL_SECURITY_APMODE_WPA3_P:
    {
        // Keep PKMSA caching (needed for SAE)
        swl_mapCharFmt_addValInt32(vapConfigMap, "disable_pmksa_caching", 0);
        // Enable Opportunistic Key Caching (aka Proactive Key Caching)
        swl_mapCharFmt_addValInt32(vapConfigMap, "okc", 1);
        // Allow EAPOL key retries
        swl_mapCharFmt_addValInt32(vapConfigMap, "wpa_disable_eapol_key_retries", 0);
        break;
    }
    default:
        break;
    }
}

#define M_HOSTAPD_MULTI_AP_BBSS     0x1
#define M_HOSTAPD_MULTI_AP_FBSS     0x2

#define MIN_VLAN_ID 1
#define MAX_VLAN_ID 4094

static void s_setVapMultiApConf(T_AccessPoint* pAP, swl_mapChar_t* vapConfigMap, swl_mapChar_t* multiAPConfig) {
    ASSERTS_NOT_NULL(pAP, , ME, "NULL");
    ASSERTS_NOT_NULL(vapConfigMap, , ME, "NULL");
    uint32_t hapdMultiApType = 0;          /* 0 = disabled (default) */
    if(SWL_BIT_IS_SET(pAP->multiAPType, MULTIAP_BACKHAUL_BSS)) {
        hapdMultiApType |= M_HOSTAPD_MULTI_AP_BBSS;
    }
    if(SWL_BIT_IS_SET(pAP->multiAPType, MULTIAP_FRONTHAUL_BSS)) {
        hapdMultiApType |= M_HOSTAPD_MULTI_AP_FBSS;
    }

    swl_mapCharFmt_addValInt32(vapConfigMap, "multi_ap", hapdMultiApType);

    //Multi-AP profile will be set only when MultiAPType is not 0(=disabled).
    if(hapdMultiApType && (pAP->multiAPProfile > MULTIAP_NOT_SUPPORTED)) {
        swl_mapCharFmt_addValInt32(vapConfigMap, "multi_ap_profile", pAP->multiAPProfile);
        if((pAP->multiAPVlanId >= MIN_VLAN_ID) && (pAP->multiAPVlanId <= MAX_VLAN_ID)) {
            swl_mapCharFmt_addValInt32(vapConfigMap, "multi_ap_vlanid", pAP->multiAPVlanId);
        }
    }

    char* wpsState = swl_mapChar_get(vapConfigMap, "wps_state");
    if((!swl_str_isEmpty(wpsState)) && (!swl_str_matches(wpsState, "0")) && (hapdMultiApType & M_HOSTAPD_MULTI_AP_FBSS)) {
        /*
         * Multi-AP backhaul BSS config: Used in WPS when multi_ap=2 or 3. (i.e. Fronthaul or Fronthaul + Backhaul)
         * It defines "backhaul BSS" credentials.
         * These are passed in WPS M8 instead of the normal (fronthaul) credentials
         * if the Enrollee has the Multi-AP subelement set. Backhaul SSID is formatted
         * like ssid2. The key is set like wpa_psk or wpa_passphrase.
         */

        swl_mapChar_t* sourceCfg = NULL;

        if(hapdMultiApType & M_HOSTAPD_MULTI_AP_BBSS) { // if BBSS flag is also set : combined FH/BH
            SAH_TRACEZ_INFO(ME, "VAP %s configured as FH and BH use self conf", pAP->alias);
            sourceCfg = vapConfigMap;
            // use self-config if hybrid mode enabled;
        } else {
            SAH_TRACEZ_INFO(ME, "VAP %s configured as pure FH try secondary config", pAP->alias);
            if(multiAPConfig != NULL) {
                sourceCfg = multiAPConfig;
                // use provided Multi-AP config if pure FH enabled
            } else {
                SAH_TRACEZ_WARNING(ME, "VAP %s configured as pure FH but no BH config provided", pAP->alias);
            }
        }
        if(sourceCfg != NULL) {
            SAH_TRACEZ_INFO(ME, "VAP %s configured as FH BSS, set BH creds as %s", pAP->alias, swl_mapChar_get(sourceCfg, "ssid"));

            swl_mapCharFmt_addValStr(vapConfigMap, "multi_ap_backhaul_ssid", "\"%s\"", swl_mapChar_get(sourceCfg, "ssid"));
            //hostapd requires backhaul_ssid included in double quotes

            if(!swl_mapChar_add(vapConfigMap, "multi_ap_backhaul_wpa_passphrase", swl_mapChar_get(sourceCfg, "wpa_passphrase"))) {
                swl_mapChar_add(vapConfigMap, "multi_ap_backhaul_wpa_psk", swl_mapChar_get(sourceCfg, "wpa_psk"));
            }
        }
    }
}

/**
 * @brief set the common parameters of a vap (ssid, secMode, keyPassphrase, bssid, ...)
 *
 * @param pAP a vap
 * @param cfgF a pointer to the the configuration file of the hostapd
 *
 * @return bool true when vap section is saved
 */
static bool s_setVapCommonConfig(T_AccessPoint* pAP, swl_mapChar_t* vapConfigMap) {
    T_SSID* pSSID = pAP->pSSID;
    ASSERTS_NOT_NULL(pSSID, false, ME, "NULL");
    T_Radio* pRad = pAP->pRadio;
    ASSERTS_NOT_NULL(pRad, false, ME, "NULL");
    ASSERTS_NOT_NULL(vapConfigMap, false, ME, "NULL");
    int tval = 0;
    const char* linkIfName = "";
    if(pAP == wld_rad_hostapd_getCfgMainVap(pRad)) {
        linkIfName = wld_hostapd_ap_selectApLinkIface(pAP);
        swl_mapChar_add(vapConfigMap, "interface", (char*) linkIfName);
    } else {
        if(!wld_hostapd_ap_needWpaCtrlIface(pAP)) {
            SAH_TRACEZ_WARNING(ME, "%s: skip disabled bss", pAP->alias);
            return false;
        }
        linkIfName = wld_hostapd_ap_selectApLinkIface(pAP);
        swl_mapChar_add(vapConfigMap, "bss", (char*) linkIfName);
    }
    swl_macChar_t bssidStr;
    SWL_MAC_BIN_TO_CHAR(&bssidStr, pSSID->BSSID);
    if(!swl_mac_charIsValidStaMac(&bssidStr)) {
        SAH_TRACEZ_WARNING(ME, "%s: skip due to invalid bssid", pAP->alias);
        return false;
    }
    swl_mapChar_add(vapConfigMap, "bssid", bssidStr.cMac);
    swl_mapChar_add(vapConfigMap, "use_driver_iface_addr", "1");
    if(strlen(pAP->bridgeName) > 0) {
        swl_mapChar_add(vapConfigMap, "bridge", pAP->bridgeName);
    }
    char* ctrlIfaceDir = HOSTAPD_CTRL_IFACE_DIR;
    if(pRad->hostapd && pRad->hostapd->ctrlIfaceDir) {
        ctrlIfaceDir = pRad->hostapd->ctrlIfaceDir;
    }
    swl_mapChar_add(vapConfigMap, "ctrl_interface", ctrlIfaceDir);
    swl_mapChar_add(vapConfigMap, "ssid", pSSID->SSID);
    swl_mapChar_add(vapConfigMap, "auth_algs", "1");
    if(pRad->beaconPeriod > 0) {
        swl_mapCharFmt_addValInt32(vapConfigMap, "beacon_int", pRad->beaconPeriod);
    }
    if(pRad->dtimPeriod > 0) {
        swl_mapCharFmt_addValInt32(vapConfigMap, "dtim_period", pRad->dtimPeriod);
    }
    swl_mapCharFmt_addValInt32(vapConfigMap, "ap_isolate", pAP->clientIsolationEnable);
    swl_mapChar_add(vapConfigMap, "ignore_broadcast_ssid", pAP->SSIDAdvertisementEnabled ? "0" : "2");
    if(!pAP->enable || !pRad->enable) {
        swl_mapChar_add(vapConfigMap, "start_disabled", "1");
    }

    s_setVapIeee80211rConfig(pAP, vapConfigMap);

    s_writeMfConfig(pAP, vapConfigMap);

    if(pAP->cfg11u.qosMapSet[0] && pAP->WMMCapability) {
        swl_mapChar_add(vapConfigMap, "qos_map_set", pAP->cfg11u.qosMapSet);
    }

    bool enableRad11be = wld_rad_is11beUsable(pRad);
    bool enableVap11be = enableRad11be;
    if(enableRad11be && wld_rad_isMloCapable(pRad)) {
        if(wld_mld_isLinkUsable(pSSID->pMldLink)) {
            /* AP MLD - Whether this AP is a part of an AP MLD
             * 0 = no (no MLO)
             * 1 = yes (MLO) */
            swl_mapCharFmt_addValInt32(vapConfigMap, "mld_ap", 1);
            swl_mapCharFmt_addValInt32(vapConfigMap, "disable_11be", 0);
            T_SSID* linkIfSSID = wld_ssid_getSsidByIfName(linkIfName);
            swl_macBin_t* linkIfMac = linkIfSSID ? (swl_macBin_t*) linkIfSSID->MACAddress : NULL;
            if(!swl_mac_binIsNull(linkIfMac)) {
                s_checkAndSetParamValueStr(pAP->wpaCtrlInterface, vapConfigMap, "mld_addr", swl_typeMacBin_toBuf32Ref(linkIfMac).buf);
            }
        } else {
            /* if no MLO, then no 11BE */
            swl_mapCharFmt_addValInt32(vapConfigMap, "disable_11be", 1);
            swl_mapCharFmt_addValInt32(vapConfigMap, "mld_ap", 0);
            enableVap11be = false;
        }
    }

    swl_security_mfpMode_e mfp = swl_security_getTargetMfpMode(pAP->secModeEnabled, pAP->mfpConfig);
    bool isH2E = pAP->pFA->mfn_misc_has_support(pAP->pRadio, pAP, "SAE_PWE", 0);
    bool is6g = (pAP->pRadio->operatingFrequencyBand == SWL_FREQ_BAND_EXT_6GHZ);
    char* wpa_key_str = ((strlen(pAP->keyPassPhrase) + 1) == PSK_KEY_SIZE_LEN) ? "wpa_psk" : "wpa_passphrase";
    switch(pAP->secModeEnabled) {
    case SWL_SECURITY_APMODE_WEP64:
    case SWL_SECURITY_APMODE_WEP128:
    case SWL_SECURITY_APMODE_WEP128IV:
    {
        swl_mapChar_add(vapConfigMap, "wpa", "0");
        swl_mapChar_add(vapConfigMap, "wep_default_key", "0");
        char WEPKEYCONV[36] = {0};
        convASCII_WEPKey(pAP->WEPKey, WEPKEYCONV, sizeof(WEPKEYCONV));
        swl_mapChar_add(vapConfigMap, "wep_key0", WEPKEYCONV);
        swl_mapChar_add(vapConfigMap, "wep_key1", WEPKEYCONV);
        swl_mapChar_add(vapConfigMap, "wep_key2", WEPKEYCONV);
        swl_mapChar_add(vapConfigMap, "wep_key3", WEPKEYCONV);
        break;
    }
    case SWL_SECURITY_APMODE_WPA_P:
    case SWL_SECURITY_APMODE_WPA2_P:
    case SWL_SECURITY_APMODE_WPA_WPA2_P: {
        mfp = (pAP->mboEnable ? SWL_SECURITY_MFPMODE_OPTIONAL : mfp);
        tval = (pAP->secModeEnabled - SWL_SECURITY_APMODE_WPA_P) + 1;
        swl_mapCharFmt_addValInt32(vapConfigMap, "wpa", tval);
        if(pAP->keyPassPhrase[0]) {    /* prefer AES key? ontop of TKIP */
            swl_mapCharFmt_addValStr(vapConfigMap, "wpa_key_mgmt", "%s%s%s",
                                     ((mfp != SWL_SECURITY_MFPMODE_REQUIRED) ? "WPA-PSK " : ""),
                                     ((mfp == SWL_SECURITY_MFPMODE_REQUIRED) ? "WPA-PSK-SHA256 " : ""),
                                     (pAP->IEEE80211rEnable ? "FT-PSK " : ""));
            /* If key pass phrase is set, we use the Key pass phrase */
            if(tval == 3) {
                swl_mapChar_add(vapConfigMap, "wpa_pairwise", "TKIP CCMP"); /* WPA_WPA2 */
                swl_mapChar_add(vapConfigMap, "rsn_pairwise", "TKIP CCMP"); /* WPA_WPA2 */
            } else {
                swl_mapChar_add(vapConfigMap, "wpa_pairwise", (tval == 1) ? "TKIP" : "CCMP");
                swl_mapChar_add(vapConfigMap, "rsn_pairwise", (tval == 1) ? "TKIP" : "CCMP");
            }
            swl_mapChar_add(vapConfigMap, wpa_key_str, pAP->keyPassPhrase);
        } else {
            /* Use the Pre Shared Key (PSK) */
            swl_mapCharFmt_addValStr(vapConfigMap, "wpa_key_mgmt", "%s%s",
                                     ((mfp != SWL_SECURITY_MFPMODE_REQUIRED) ? "WPA-PSK " : ""),
                                     ((mfp == SWL_SECURITY_MFPMODE_REQUIRED) ? "WPA-PSK-SHA256 " : ""));
            swl_mapChar_add(vapConfigMap, "wpa_pairwise", "TKIP"); /* WPA or WPA2 with TKIP */
            swl_mapChar_add(vapConfigMap, "rsn_pairwise", "TKIP"); /* WPA or WPA2 with TKIP */
            swl_mapChar_add(vapConfigMap, "wpa_psk", pAP->preSharedKey);
        }
        swl_mapCharFmt_addValInt32(vapConfigMap, "wpa_group_rekey", pAP->rekeyingInterval);
        swl_mapChar_add(vapConfigMap, "wpa_ptk_rekey", "0");
        swl_mapCharFmt_addValInt32(vapConfigMap, "ieee80211w", mfp);
        break;
    }
    case SWL_SECURITY_APMODE_WPA3_P_CM: {
        if(!is6g) { /* AKM:2 for 2.4/5GHz RSNE */

            // per WPA3CM spec: set only the MFPC to 1 if MBO Enabled
            if(pAP->mfpConfig == SWL_SECURITY_MFPMODE_DISABLED) {
                mfp = (pAP->mboEnable ? SWL_SECURITY_MFPMODE_OPTIONAL : SWL_SECURITY_MFPMODE_DISABLED);
            }

            swl_mapChar_add(vapConfigMap, "wpa", "2");

            swl_mapCharFmt_addValStr(vapConfigMap, "wpa_key_mgmt", "WPA-PSK");
            swl_mapChar_add(vapConfigMap, "wpa_pairwise", "CCMP");
            swl_mapChar_add(vapConfigMap, "rsn_pairwise", "CCMP");
            /* If key pass phrase is set, we use the Key pass phrase */
            swl_mapChar_add(vapConfigMap, wpa_key_str, pAP->keyPassPhrase);

            swl_mapCharFmt_addValInt32(vapConfigMap, "wpa_group_rekey", pAP->rekeyingInterval);
            swl_mapChar_add(vapConfigMap, "wpa_ptk_rekey", "0");
            swl_mapCharFmt_addValInt32(vapConfigMap, "ieee80211w", mfp);
        } else { /* AKM:8 for 6GHz RSNE */
            swl_mapChar_add(vapConfigMap, "wpa", "2");
            swl_mapCharFmt_addValStr(vapConfigMap, "wpa_key_mgmt", "SAE");

            swl_mapChar_add(vapConfigMap, "wpa_pairwise", "CCMP");
            swl_mapChar_add(vapConfigMap, "rsn_pairwise", "CCMP");
            swl_mapCharFmt_addValInt32(vapConfigMap, "wpa_group_rekey", pAP->rekeyingInterval);
            swl_mapChar_add(vapConfigMap, "wpa_ptk_rekey", "0");
            // If sae_password is set, wpa_passphrase is ignored by hostapd
            // otherwise wpa_passphrase is used
            if(!swl_str_isEmpty(pAP->saePassphrase)) {
                swl_mapChar_add(vapConfigMap, "sae_password", pAP->saePassphrase);
            }
            swl_mapChar_add(vapConfigMap, wpa_key_str, pAP->keyPassPhrase);

            swl_mapCharFmt_addValInt32(vapConfigMap, "ieee80211w", SWL_SECURITY_MFPMODE_REQUIRED);
        }

        if(!is6g) { /* AKM:8 for RSNO1; not advertised on 6GHz */
            s_checkAndSetParamValueStr(pAP->wpaCtrlInterface, vapConfigMap, "rsn_override_key_mgmt", "SAE");
            s_checkAndSetParamValueStr(pAP->wpaCtrlInterface, vapConfigMap, "rsn_override_pairwise", "CCMP");
            s_checkAndSetParamValueInt32(pAP->wpaCtrlInterface, vapConfigMap, "rsn_override_mfp", SWL_SECURITY_MFPMODE_REQUIRED);
        }
        if(enableVap11be) { /* AKM:24 for RSNO2 on all bands */
            s_checkAndSetParamValueStr(pAP->wpaCtrlInterface, vapConfigMap, "rsn_override_key_mgmt_2", "SAE-EXT-KEY");
            s_checkAndSetParamValueStr(pAP->wpaCtrlInterface, vapConfigMap, "rsn_override_pairwise_2", "GCMP-256");
            s_checkAndSetParamValueInt32(pAP->wpaCtrlInterface, vapConfigMap, "rsn_override_mfp_2", SWL_SECURITY_MFPMODE_REQUIRED);
        }
        if(!is6g) { /* The RSNXE is not advertised in the 2.4 and 5 GHz bands, replaced by RSNXE Override element */
            s_checkAndSetParamValueInt32(pAP->wpaCtrlInterface, vapConfigMap, "rsn_override_omit_rsnxe", true);
        }

        swl_mapChar_add(vapConfigMap, "sae_sync", "5");
        swl_mapChar_add(vapConfigMap, "sae_require_mfp", "1");
        swl_mapChar_add(vapConfigMap, "sae_anti_clogging_threshold", "5");
        swl_mapChar_add(vapConfigMap, "sae_groups", "19 20 21");

        if(enableVap11be) {
            swl_mapChar_add(vapConfigMap, "beacon_prot", "1");
        }
        swl_mapCharFmt_addValInt32(vapConfigMap, "sae_pwe", isH2E ? is6g ? 1 : 2 : 0);
        break;
    }
    case SWL_SECURITY_APMODE_WPA2_WPA3_P: {
        swl_mapChar_add(vapConfigMap, "wpa", "2");
        swl_mapCharFmt_addValStr(vapConfigMap, "wpa_key_mgmt", "%s%s%s",
                                 ((mfp != SWL_SECURITY_MFPMODE_REQUIRED) ? "WPA-PSK " : ""),
                                 ((mfp == SWL_SECURITY_MFPMODE_REQUIRED) ? "WPA-PSK-SHA256 " : ""),
                                 (enableVap11be ? "SAE SAE-EXT-KEY" : "SAE"));
        swl_mapChar_add(vapConfigMap, "wpa_pairwise", enableVap11be ? "CCMP GCMP-256" : "CCMP");
        swl_mapChar_add(vapConfigMap, "rsn_pairwise", enableVap11be ? "CCMP GCMP-256" : "CCMP");
        swl_mapCharFmt_addValInt32(vapConfigMap, "wpa_group_rekey", pAP->rekeyingInterval);
        swl_mapChar_add(vapConfigMap, "wpa_ptk_rekey", "0");
        swl_mapChar_add(vapConfigMap, wpa_key_str, pAP->keyPassPhrase);
        // If sae_password is set, hostapd will use the sae_password value
        // for WPA3 connection and wpa_passphrase for WPA-WPA2. If sae_password
        // is not set, wpa_passphrase will be used for WPA3 connection
        if(!swl_str_isEmpty(pAP->saePassphrase)) {
            swl_mapChar_add(vapConfigMap, "sae_password", pAP->saePassphrase);
        }
        swl_mapChar_add(vapConfigMap, "sae_require_mfp", "1");
        swl_mapChar_add(vapConfigMap, "sae_anti_clogging_threshold", "5");
        swl_mapChar_add(vapConfigMap, "sae_sync", "5");
        swl_mapChar_add(vapConfigMap, "sae_groups", "19 20 21");
        swl_mapCharFmt_addValInt32(vapConfigMap, "ieee80211w", mfp);
        swl_mapCharFmt_addValInt32(vapConfigMap, "sae_pwe", isH2E ? 2 : 0);
        if(enableVap11be) {
            swl_mapChar_add(vapConfigMap, "beacon_prot", "1");
        }
        break;
    }
    case SWL_SECURITY_APMODE_WPA3_P: {
        swl_mapChar_add(vapConfigMap, "wpa", "2");
        swl_mapCharFmt_addValStr(vapConfigMap, "wpa_key_mgmt", "%s%s",
                                 (enableVap11be ? "SAE SAE-EXT-KEY" : "SAE"),
                                 (pAP->IEEE80211rEnable ? (enableVap11be ? " FT-SAE FT-SAE-EXT-KEY" : " FT-SAE") : ""));
        swl_mapChar_add(vapConfigMap, "wpa_pairwise", enableVap11be ? "CCMP GCMP-256" : "CCMP");
        swl_mapChar_add(vapConfigMap, "rsn_pairwise", enableVap11be ? "CCMP GCMP-256" : "CCMP");
        swl_mapCharFmt_addValInt32(vapConfigMap, "wpa_group_rekey", pAP->rekeyingInterval);
        swl_mapChar_add(vapConfigMap, "wpa_ptk_rekey", "0");
        // If sae_password is set, wpa_passphrase is ignored by hostapd
        // otherwise wpa_passphrase is used
        if(!swl_str_isEmpty(pAP->saePassphrase)) {
            swl_mapChar_add(vapConfigMap, "sae_password", pAP->saePassphrase);
        }
        swl_mapChar_add(vapConfigMap, wpa_key_str, pAP->keyPassPhrase);
        swl_mapChar_add(vapConfigMap, "sae_require_mfp", "1");
        swl_mapChar_add(vapConfigMap, "sae_anti_clogging_threshold", "5");
        swl_mapChar_add(vapConfigMap, "sae_sync", "5");
        swl_mapChar_add(vapConfigMap, "sae_groups", "19 20 21");
        swl_mapChar_add(vapConfigMap, "ieee80211w", "2");
        swl_mapCharFmt_addValInt32(vapConfigMap, "sae_pwe", isH2E ? is6g ? 1 : 2 : 0);
        if(enableVap11be) {
            swl_mapChar_add(vapConfigMap, "beacon_prot", "1");
        }
        break;
    }
    case SWL_SECURITY_APMODE_OWE:
    {
        swl_mapChar_add(vapConfigMap, "wpa", "2");
        swl_mapChar_add(vapConfigMap, "wpa_pairwise", enableVap11be ? "CCMP GCMP-256" : "CCMP");
        swl_mapChar_add(vapConfigMap, "rsn_pairwise", enableVap11be ? "CCMP GCMP-256" : "CCMP");
        swl_mapChar_add(vapConfigMap, "wpa_key_mgmt", "OWE");
        swl_mapChar_add(vapConfigMap, "ieee80211w", "2");
        T_AccessPoint* transitionAp;
        if((!swl_str_isEmpty(pAP->oweTransModeIntf)) &&
           ((transitionAp = wld_ap_getVapByName(pAP->oweTransModeIntf)) != NULL)) {
            swl_mapChar_add(vapConfigMap, "owe_transition_ifname", transitionAp->alias);
        }
        if(enableVap11be) {
            swl_mapChar_add(vapConfigMap, "beacon_prot", "1");
        }
        break;
    }
    case SWL_SECURITY_APMODE_NONE:
    {
        swl_mapChar_add(vapConfigMap, "wpa", "0");
        T_AccessPoint* transitionAp;
        if((!swl_str_isEmpty(pAP->oweTransModeIntf)) &&
           ((transitionAp = wld_ap_getVapByName(pAP->oweTransModeIntf)) != NULL)) {
            swl_mapChar_add(vapConfigMap, "owe_transition_ifname", transitionAp->alias);
        }
        break;
    }
    case SWL_SECURITY_APMODE_WPA_E:
    case SWL_SECURITY_APMODE_WPA2_E:
    case SWL_SECURITY_APMODE_WPA_WPA2_E: {
        mfp = (pAP->mboEnable ? SWL_SECURITY_MFPMODE_OPTIONAL : mfp);
        tval = (pAP->secModeEnabled - SWL_SECURITY_APMODE_WPA_E) + 1;
        swl_mapCharFmt_addValInt32(vapConfigMap, "wpa", tval);
        swl_mapCharFmt_addValStr(vapConfigMap, "wpa_key_mgmt", "%s%s%s",
                                 ((mfp != SWL_SECURITY_MFPMODE_REQUIRED) ? "WPA-EAP " : ""),
                                 ((mfp == SWL_SECURITY_MFPMODE_REQUIRED) ? "WPA-EAP-SHA256 " : ""),
                                 (pAP->IEEE80211rEnable ? "FT-EAP " : ""));
        swl_mapCharFmt_addValStr(vapConfigMap, "wpa_pairwise", "%s %s", (tval & 1) ? "TKIP" : "", (tval & 2) ? "CCMP" : "");
        swl_mapCharFmt_addValStr(vapConfigMap, "rsn_pairwise", "%s %s", (tval & 1) ? "TKIP" : "", (tval & 2) ? "CCMP" : "");
        swl_mapChar_add(vapConfigMap, "auth_server_addr", *pAP->radiusServerIPAddr ? pAP->radiusServerIPAddr : "127.0.0.1");
        swl_mapCharFmt_addValInt32(vapConfigMap, "auth_server_port", pAP->radiusServerPort);

        if(!swl_str_isEmpty(pAP->radiusSecret)) {
            swl_mapChar_add(vapConfigMap, "auth_server_shared_secret", pAP->radiusSecret);
        }
        if(!swl_str_isEmpty(pAP->radiusOwnIPAddress)) {
            swl_mapChar_add(vapConfigMap, "own_ip_addr", pAP->radiusOwnIPAddress);
        }
        if(!swl_str_isEmpty(pAP->radiusNASIdentifier)) {
            swl_mapChar_add(vapConfigMap, "nas_identifier", pAP->radiusNASIdentifier);
        }
        if(!swl_str_isEmpty(pAP->radiusCalledStationId)) {
            swl_mapCharFmt_addValStr(vapConfigMap, "radius_auth_req_attr", "30:s:%s", pAP->radiusCalledStationId);
        }
        if(pAP->radiusDefaultSessionTimeout != 0) {
            swl_mapCharFmt_addValInt32(vapConfigMap, "auth_server_default_session_timeout", pAP->radiusDefaultSessionTimeout);
        }
        if(pAP->radiusChargeableUserId) {
            swl_mapChar_add(vapConfigMap, "radius_request_cui", "1");
        }
        swl_mapCharFmt_addValInt32(vapConfigMap, "ieee80211w", mfp);
        swl_mapCharFmt_addValInt32(vapConfigMap, "ieee8021x", 1);
        break;
    }
    default:
        break;
    }
    s_setSecKeyCacheConf(pAP, vapConfigMap);
    if(pAP->HotSpot2.enable) {
        swl_mapCharFmt_addValInt32(vapConfigMap, "hs20", true);
        swl_mapCharFmt_addValInt32(vapConfigMap, "disable_dgaf", pAP->HotSpot2.dgaf_disable);
        swl_mapCharFmt_addValInt32(vapConfigMap, "hs20_deauth_req_timeout", 0);
        swl_mapCharFmt_addValInt32(vapConfigMap, "osen", false);
    }
    if(pAP->WMMCapability) {
        swl_mapCharFmt_addValInt32(vapConfigMap, "wmm_enabled", pAP->WMMEnable);
    }
    swl_mapCharFmt_addValInt32(vapConfigMap, "uapsd_advertisement_enabled", pAP->UAPSDCapability && pAP->UAPSDEnable);
    swl_mapCharFmt_addValInt32(vapConfigMap, "interworking", pAP->cfg11u.interworkingEnable);
    //Temporarily disabled : triggering station disconnection
    swl_mapChar_add(vapConfigMap, "#bss_transition", "1");
    swl_mapChar_add(vapConfigMap, "notify_mgmt_frames", "1");
    if(pAP->transitionDisable != 0) {
        swl_mapCharFmt_addValInt32(vapConfigMap, "transition_disable", pAP->transitionDisable);
    }

    bool isIEEE80211k = pAP->IEEE80211kEnable && pRad->IEEE80211kSupported;
    // Enable neighbor report via radio measurements
    swl_mapCharFmt_addValInt32(vapConfigMap, "rrm_neighbor_report", isIEEE80211k && pAP->SSIDAdvertisementEnabled);
    // Enable beacon report via radio measurements
    swl_mapCharFmt_addValInt32(vapConfigMap, "rrm_beacon_report", isIEEE80211k);
    // set rnr is supported by hostapd
    bool isRnrEnabled = isIEEE80211k && (wld_ap_getDiscoveryMethod(pAP) == M_AP_DM_RNR);
    s_checkAndSetParamValueInt32(pAP->wpaCtrlInterface, vapConfigMap, "rnr", isRnrEnabled);
    // Multiband Operation (MBO)
    if(pAP->mboEnable) {
        swl_mapCharFmt_addValInt32(vapConfigMap, "mbo", pAP->mboEnable);
    }
    /* WDS */
    if(pAP->wdsEnable) {
        swl_mapCharFmt_addValInt32(vapConfigMap, "wds_sta", pAP->wdsEnable);
    }
    /*
     * If a station does not send anything in ap_max_inactivity seconds,
     * an empty data frame is sent to it in order to verify whether it is still in range.
     * If this frame is not ACKed, the station will be disassociated and then deauthenticated.
     */
    if(pAP->StaInactivityTimeout) {
        swl_mapCharFmt_addValInt32(vapConfigMap, "ap_max_inactivity", pAP->StaInactivityTimeout);
    }
    /* when kickoff roaming station is active, number of tries during 4-way handshak is increased*/
    if(pRad->kickRoamStaEnabled) {
        swl_mapCharFmt_addValInt32(vapConfigMap, "wpa_group_update_count", 8);
        swl_mapCharFmt_addValInt32(vapConfigMap, "wpa_pairwise_update_count", 8);
    }
    return true;
}

/**
 * @brief set the wps parameters of a vap
 *
 * @param pAP a vap
 * @param cfgF a pointer to the the configuration file of the hostapd
 *
 * @return void
 */
static void s_setVapWpsConfig(T_AccessPoint* pAP, swl_mapChar_t* vapConfigMap) {
    T_Radio* pRad = pAP->pRadio;
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    ASSERTS_NOT_NULL(vapConfigMap, , ME, "NULL");
    ASSERTS_NOT_NULL(pRad->wpsConst, , ME, "NULL");

    if(!pAP->WPS_Enable || (pAP->secModeEnabled == SWL_SECURITY_APMODE_WPA3_P)) {
        swl_mapChar_add(vapConfigMap, "wps_state", "0");
        return;
    }
    bool wps_enable = (pAP->WPS_Enable && pAP->SSIDAdvertisementEnabled &&
                       ((!pAP->secModeEnabled && pAP->WPS_CertMode) ||
                        (!swl_security_isApModeWEP(pAP->secModeEnabled)) ||
                        (!pAP->WPS_Configured)));
    swl_mapChar_add(vapConfigMap, "wps_state", wps_enable ? (pAP->WPS_Configured ? "2" : "1") : "0");
    swl_mapChar_add(vapConfigMap, "wps_independent", "1");
    /*
     * Setting the ap_setup_locked parameter in hostapd configuration file is only relevant when one of the PIN configuration methods is enabled.
     * AP Setup Locked Attribute will be included by default in WSC IE of the Beacon and probe response frames.
     * And the AP will refuse to allow an externel registrar to run registration protocol using the AP's PIN. (AP acting as an enrollee)
     * The AP Setup locked state will be reset when InitiateWPSPIN() is executed.
     * */
    if(pAP->WPS_ConfigMethodsEnabled & (M_WPS_CFG_MTHD_LABEL | M_WPS_CFG_MTHD_DISPLAY_ALL)) {
        swl_mapChar_add(vapConfigMap, "ap_setup_locked", "1");
    }
    swl_mapChar_add(vapConfigMap, "uuid", pRad->wpsConst->UUID);
    char* deviceName = swl_str_isEmpty(pRad->wpsConst->DevName) ? "unknownAp" : pRad->wpsConst->DevName;
    swl_mapChar_add(vapConfigMap, "device_name", deviceName);
    swl_mapChar_add(vapConfigMap, "manufacturer", pRad->wpsConst->Manufacturer);
    swl_mapChar_add(vapConfigMap, "model_name", pRad->wpsConst->ModelName);
    swl_mapChar_add(vapConfigMap, "model_number", pRad->wpsConst->ModelNumber);
    swl_mapChar_add(vapConfigMap, "serial_number", pRad->wpsConst->SerialNumber);
    int tmpver[4] = {0};
    sscanf(pRad->wpsConst->OsVersion, "%i.%i.%i.%i", &tmpver[0], &tmpver[1], &tmpver[2], &tmpver[3]);
    swl_mapCharFmt_addValStr(vapConfigMap, "os_version", "%.8x", ((unsigned int) (tmpver[0] << 24 | tmpver[1] << 16 | tmpver[2] << 8 | tmpver[3])));
    swl_mapChar_add(vapConfigMap, "device_type", "6-0050F204-1");

    char configMethodsStr[256] = {0};
    swl_conv_maskToCharSep(configMethodsStr, sizeof(configMethodsStr), pAP->WPS_ConfigMethodsEnabled, s_hostapd_WPS_configMethods_str, SWL_ARRAY_SIZE(s_hostapd_WPS_configMethods_str), ' ');
    if(strlen(configMethodsStr) > 0) {
        swl_mapChar_add(vapConfigMap, "config_methods", configMethodsStr);
    }

    swl_mapChar_add(vapConfigMap, "wps_cred_processing", pAP->WPS_Configured ? "2" : "0");
    //Label: STATIC 8 digit PIN, typically available on device.
    if(pAP->WPS_ConfigMethodsEnabled & (M_WPS_CFG_MTHD_LABEL)) {
        swl_mapChar_add(vapConfigMap, "ap_pin", pRad->wpsConst->DefaultPin);
    }
    swl_mapChar_add(vapConfigMap, "wps_rf_bands", "ag");
    swl_mapChar_add(vapConfigMap, "friendly_name", "WPS Access Point");
    swl_mapChar_add(vapConfigMap, "eap_server", "1");
}

/**
 * @brief generate md5 hash for a config map
 *
 * @param pHashStr Pointer output string (to be freed by caller)
 * @param configMap map char of config param/values
 *
 * @return SWL_RC_OK on success, error code otherwise
 */
swl_rc_ne wld_hostapd_cfgFile_genConfigHash(char** pHashStr, swl_mapChar_t* configMap) {
    ASSERTS_NOT_NULL(configMap, SWL_RC_INVALID_PARAM, ME, "NULL");

    // Generate the BSS config text
    char bssCfgStr[4096] = {0};
    bool success = swl_mapWriterKVP_writeToStr(configMap, bssCfgStr, SWL_ARRAY_SIZE(bssCfgStr));

    ASSERT_TRUE(success, SWL_RC_ERROR, ME, "fail to generate the config text");

    // Encode an MD5 hash from the BSS config
    swl_bit8_t bssCfgHash[MD5_DIGEST_LENGTH] = {'\0'};
    swl_rc_ne ret = swl_hash_rawToMD5(bssCfgHash, sizeof(bssCfgHash), (swl_bit8_t*) bssCfgStr, strlen((char*) bssCfgStr));

    ASSERT_FALSE(ret < SWL_RC_ERROR, ret, ME, "fail to encode an MD5 hash from config");

    char vapCfgId[MD5_DIGEST_LENGTH * 2 + 1] = {'\0'};
    swl_hex_fromBytes(vapCfgId, sizeof(vapCfgId), bssCfgHash, MD5_DIGEST_LENGTH, 0);
    ASSERT_TRUE(swl_str_copyMalloc(pHashStr, vapCfgId), SWL_RC_ERROR, ME, "Fail to copy Hash");

    return SWL_RC_OK;
}

static void s_saveVapConfigId(T_AccessPoint* pAP, swl_mapChar_t* vapConfigMap) {
    ASSERTS_NOT_NULL(pAP, , ME, "NULL");
    if(swl_mapChar_has(vapConfigMap, "config_id")) {
        swl_mapChar_delete(vapConfigMap, "config_id");
    }
    char* newValue = NULL;
    if(wld_hostapd_cfgFile_genConfigHash(&newValue, vapConfigMap) == SWL_RC_OK) {
        s_checkAndSetParamValueStr(pAP->wpaCtrlInterface, vapConfigMap, "config_id", newValue);
    }
    free(newValue);
}

/**
 * @brief set a vap parameters. A vap configuration could be either an interface or a bss
 *
 * @param pAP a vap
 * @param cfgF a pointer to the the configuration file of the hostapd; map refers to data type not multi-AP
 * @param multiAPCfg a pointer for backhaul config (ssid/passphrase)
 *
 * @return void
 */
void wld_hostapd_cfgFile_setVapConfig(T_AccessPoint* pAP, swl_mapChar_t* vapConfigMap, swl_mapChar_t* multiAPConfig) {
    ASSERTS_NOT_NULL(pAP, , ME, "NULL");
    ASSERTS_NOT_NULL(vapConfigMap, , ME, "NULL");

    ASSERTS_TRUE(s_setVapCommonConfig(pAP, vapConfigMap), , ME, "%s: skipped", pAP->alias);
    s_setVapWpsConfig(pAP, vapConfigMap);
    s_setVapMultiApConf(pAP, vapConfigMap, multiAPConfig);

    // rnr standard config
    bool isIEEE80211k = pAP->IEEE80211kEnable && pAP->pRadio->IEEE80211kSupported;
    bool rnrCfgStd = isIEEE80211k && (wld_ap_getDiscoveryMethod(pAP) == M_AP_DM_RNR);

    pAP->pFA->mfn_wvap_updateConfigMap(pAP, vapConfigMap);

    // if rnr conf has been changed by vdr, then prevent further overwriting by removing standard rnr conf setting
    if(wld_secDmn_getCfgParamSupp(pAP->pRadio->hostapd, "rnr") != SWL_TRL_FALSE) {
        // get final rnr config (may be customized by vdr module)
        bool rnrCfgFinal = swl_str_matches(swl_mapChar_get(vapConfigMap, "rnr"), "1");
        if(rnrCfgStd != rnrCfgFinal) {
            wld_secDmn_setCfgParamSupp(pAP->pRadio->hostapd, "rnr", SWL_TRL_FALSE);
        }
    }

    s_saveVapConfigId(pAP, vapConfigMap);
}

/**
 * @brief create a configuration file for the hostapd
 *
 * @param pRad the radio
 * @param cfgFileName the hostapd configuration file name
 *
 * @return void
 */
void wld_hostapd_cfgFile_create(T_Radio* pRad, char* cfgFileName) {
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    ASSERT_STR(cfgFileName, , ME, "Empty path");
    ASSERTS_FALSE(amxc_llist_is_empty(&pRad->llAP), , ME, "No vaps");
    wld_hostapd_config_t* config = NULL;
    bool ret = wld_hostapd_createConfig(&config, pRad);
    ASSERT_TRUE(ret, , ME, "Bad config");
    wld_hostapd_writeConfig(config, cfgFileName);
    wld_hostapd_deleteConfig(config);
}

void wld_hostapd_cfgFile_createExt(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    ASSERTS_NOT_NULL(pRad->hostapd, , ME, "NULL");
    wld_hostapd_cfgFile_create(pRad, pRad->hostapd->cfgFile);
}

/**
 * @brief update a configuration file for the hostapd
 *
 * @param configPath the path of the config file
 * @param interface bss name
 * @param key the param to update
 * @param value the key value to set
 * @return void
 */
bool wld_hostapd_cfgFile_update(char* configPath, const char* interface, const char* key, const char* value) {
    SAH_TRACEZ_IN(ME);
    ASSERT_STR(configPath, false, ME, "Empty path");
    ASSERT_STR(interface, false, ME, "Empty interface");
    ASSERT_STR(key, false, ME, "Empty key");
    ASSERT_NOT_NULL(value, false, ME, "NULL");
    wld_hostapd_config_t* config;
    bool ret = wld_hostapd_loadConfig(&config, configPath);
    ASSERTS_TRUE(ret, false, ME, "Bad config");
    SAH_TRACEZ_INFO(ME, "%s: updateConfig interface:%s, key:%s, value:%s", configPath, interface, key, value);
    ret = wld_hostapd_addConfigParam(config, interface, key, value);
    if(ret) {
        ret = wld_hostapd_writeConfig(config, configPath);
    }
    wld_hostapd_deleteConfig(config);
    SAH_TRACEZ_OUT(ME);
    return ret;
}
