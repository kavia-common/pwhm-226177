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
/*
 * This file implements wrapper functions to use nl80211 generic apis with T_Radio context
 */

#include <math.h>
#include "wld_rad_nl80211.h"
#include "wld_ap_nl80211.h"
#include "wld_ep_nl80211.h"
#include "wld_ssid_nl80211_priv.h"
#include "wld_nl80211_utils.h"
#include "wld_linuxIfUtils.h"
#include "swl/swl_common.h"
#include "swl/swl_common_time.h"
#include "wld_radio.h"
#include "wld_util.h"

#define ME "nlRad"
#define MAX_FACTOR            1
#define DEFAULT_NOISE_DB     -95
#define DEFAULT_TIME_BUSY    0
#define NOISE_SCALE_DIV      5.0f
#define DBM_TO_POWER_DIV     10.0f
#define LN_10                2.3025851f
#define LN_2                 0.6931472f
#define TEN_POW_6            1000000.0
static inline float pow10_approx(float x) {
    return expf(x * LN_10); // Computes 10^x
}

static inline float pow2_approx(float x) {
    return expf(x * LN_2);  // Computes 2^x
}
/* The survey interference factor is defined as the ratio of the
 * observed busy time over the time we spent on the channel,
 * this value is then amplified by the observed noise floor on
 * the channel in comparison to the lowest noise floor observed
 * on the entire band.
 *
 * This corresponds to:
 * ---
 * (busy time - tx time) / (active time - tx time) * 2^(chan_nf + band_min_nf)
 * ---
 *
 * The coefficient of 2 reflects the way power in "far-field"
 * radiation decreases as the square of distance from the antenna [1].
 * What this does is it decreases the observed busy time ratio if the
 * noise observed was low but increases it if the noise was high,
 * proportionally to the way "far field" radiation changes over
 * distance.
 *
 * If channel busy time is not available the fallback is to use channel RX time.
 *
 * Since noise floor is in dBm it is necessary to convert it into Watts so that
 * combined channel interference (e.g., HT40, which uses two channels) can be
 * calculated easily.
 * ---
 * (busy time - tx time) / (active time - tx time) *
 *    2^(10^(chan_nf/10) + 10^(band_min_nf/10))
 * ---
 *
 * However to account for cases where busy/rx time is 0 (channel load is then
 * 0%) channel noise floor signal power is combined into the equation so a
 * channel with lower noise floor is preferred. The equation becomes:
 * ---
 * 10^(chan_nf/5) + (busy time - tx time) / (active time - tx time) *
 *    2^(10^(chan_nf/10) + 10^(band_min_nf/10))
 * ---
 */
#define NOISE_FACTOR(noise_dbm) \
    (pow10_approx(((float) (noise_dbm)) / NOISE_SCALE_DIV))

#define NOISE_POWER_DIFF(noise_dbm, min_noise_dbm) \
    (pow10_approx(((float) (noise_dbm)) / DBM_TO_POWER_DIV) - \
     pow10_approx(((float) (min_noise_dbm)) / DBM_TO_POWER_DIV))

#define INTERFERENCE_FACTOR(noise_dbm, busy_time, total_time, min_noise_dbm) \
    (NOISE_FACTOR((noise_dbm)) + \
     (((float) (total_time)) > 0.0f ? ((float) (busy_time) / (float) (total_time)) : 0) * \
     pow2_approx(NOISE_POWER_DIFF((noise_dbm), (min_noise_dbm))))

swl_rc_ne wld_rad_nl80211_setEvtListener(T_Radio* pRadio, void* pData, const wld_nl80211_evtHandlers_cb* const handlers) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    swl_rc_ne rc = wld_nl80211_delEvtListener(&pRadio->nl80211Listener);
    if((rc == SWL_RC_OK) && (handlers == NULL)) {
        return rc;
    }
    uint32_t wiphy = pRadio->wiphy;
    ASSERTI_NOT_EQUALS(wiphy, WLD_NL80211_ID_UNDEF, SWL_RC_INVALID_PARAM, ME, "%s: undefined wiphy id", pRadio->Name);
    uint32_t ifIndex = WLD_NL80211_ID_ANY;
    SAH_TRACEZ_INFO(ME, "rad(%s): add evt listener wiphy(%d)/ifIndex(%d)", pRadio->Name, wiphy, ifIndex);
    wld_nl80211_state_t* state = wld_nl80211_getSharedState();
    pRadio->nl80211Listener = wld_nl80211_addEvtListener(state, wiphy, ifIndex, pRadio, pData, handlers);
    ASSERT_NOT_NULL(pRadio->nl80211Listener, SWL_RC_ERROR,
                    ME, "rad(%s): fail to add evt listener wiphy(%d)/ifIndex(%d)", pRadio->Name, wiphy, ifIndex);
    return SWL_RC_OK;
}

swl_rc_ne wld_rad_nl80211_delEvtListener(T_Radio* pRadio) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_nl80211_delEvtListener(&pRadio->nl80211Listener);
}

swl_rc_ne wld_rad_nl80211_getInterfaceInfo(T_Radio* pRadio, wld_nl80211_ifaceInfo_t* pIfaceInfo) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    T_SSID* pSSID = wld_ssid_getSsidByIfName(pRadio->Name);
    if(pSSID != NULL) {
        return wld_ssid_nl80211_getInterfaceInfo(pSSID, pIfaceInfo);
    }
    return wld_nl80211_getInterfaceInfo(wld_nl80211_getSharedState(), pRadio->index, pIfaceInfo);
}

swl_rc_ne wld_rad_nl80211_setAp(T_Radio* pRadio) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_nl80211_setInterfaceType(wld_nl80211_getSharedState(), pRadio->index, true, false);
}

swl_rc_ne wld_rad_nl80211_setSta(T_Radio* pRadio) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_nl80211_setInterfaceType(wld_nl80211_getSharedState(), pRadio->index, false, true);
}

swl_rc_ne wld_rad_nl80211_set4Mac(T_Radio* pRadio, bool use4Mac) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_nl80211_setInterfaceUse4Mac(wld_nl80211_getSharedState(), pRadio->index, use4Mac);
}

static swl_rc_ne s_addInterface(T_Radio* pRadio, const char* ifname, swl_macBin_t* mac, bool isSta, wld_nl80211_ifaceInfo_t* pOutIfInfo) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_nl80211_newIfaceConf_t ifaceConf;
    memset(&ifaceConf, 0, sizeof(ifaceConf));
    ifaceConf.type = isSta ? NL80211_IFTYPE_STATION : NL80211_IFTYPE_AP;
    if(mac) {
        ifaceConf.mac = *mac;
    }
    wld_nl80211_ifaceInfo_t newIfInfo;
    wld_nl80211_ifaceInfo_t* pNewIfInfo = pOutIfInfo ? : &newIfInfo;
    memset(pNewIfInfo, 0, sizeof(*pNewIfInfo));
    swl_rc_ne rc = wld_nl80211_newWiphyInterface(wld_nl80211_getSharedState(), pRadio->wiphy, ifname, &ifaceConf, pNewIfInfo);
    ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "%s: fail to create iface %s", pRadio->Name, ifname);
    return rc;
}

swl_rc_ne wld_rad_nl80211_addInterface(T_Radio* pRadio, const char* ifname, swl_macBin_t* pMac, bool isSta, wld_nl80211_ifaceInfo_t* pOutIfInfo) {
    wld_nl80211_ifaceInfo_t ifaceInfo;
    memset(&ifaceInfo, 0, sizeof(ifaceInfo));
    W_SWL_SETPTR(pOutIfInfo, ifaceInfo);
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_STR(ifname, SWL_RC_INVALID_PARAM, ME, "missing ifname");
    int ifIndex = -1;
    swl_rc_ne rc;
    if(swl_str_matches(pRadio->Name, ifname) && (pRadio->index > 0)) {
        ifIndex = pRadio->index;
        rc = isSta ? wld_rad_nl80211_setSta(pRadio) : wld_rad_nl80211_setAp(pRadio);
        ASSERT_TRUE(swl_rc_isOk(rc), rc, ME, "fail to set rad(%s) iface(%s) type isSta(%d)", pRadio->Name, ifname, isSta);
    }
    if(ifIndex <= 0) {
        wld_linuxIfUtils_getIfIndex(wld_rad_getSocket(pRadio), (char*) ifname, &ifIndex);
    }
    if(ifIndex > 0) {
        rc = wld_nl80211_getInterfaceInfo(wld_nl80211_getSharedState(), ifIndex, &ifaceInfo);
        if((!swl_rc_isOk(rc)) || (!swl_str_matches(ifname, ifaceInfo.name)) || (ifaceInfo.isAp == isSta)) {
            SAH_TRACEZ_ERROR(ME, "unmatched existing iface(%s) ifIndex(%d) isSta(%d)", ifname, ifIndex, isSta);
            return SWL_RC_ERROR;
        }
    } else if((rc = s_addInterface(pRadio, ifname, pMac, isSta, &ifaceInfo)) < SWL_RC_OK) {
        return rc;
    }
    bool setMac = (pMac && !swl_mac_binIsBroadcast(pMac) && !swl_mac_binIsNull(pMac));
    if(setMac && !swl_mac_binMatches(&ifaceInfo.mac, pMac)) {
        wld_linuxIfUtils_updateMac(wld_rad_getSocket(pRadio), ifaceInfo.name, pMac);
        wld_linuxIfUtils_getMac(wld_rad_getSocket(pRadio), ifaceInfo.name, &ifaceInfo.mac);
    }
    W_SWL_SETPTR(pOutIfInfo, ifaceInfo);
    return rc;
}

swl_rc_ne wld_rad_nl80211_addVapInterface(T_Radio* pRadio, T_AccessPoint* pAP) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_NOT_NULL(pRadio, rc, ME, "NULL");
    ASSERT_NOT_NULL(pAP, rc, ME, "NULL");
    const char* ifname = pAP->alias;
    ASSERT_STR(ifname, rc, ME, "Empty ifname");

    wld_nl80211_ifaceInfo_t ifaceInfo;
    swl_macBin_t* pMac = NULL;
    if((pAP->pSSID != NULL) && (!swl_mac_binIsNull((swl_macBin_t*) pAP->pSSID->MACAddress))) {
        pMac = (swl_macBin_t*) pAP->pSSID->MACAddress;
    }
    rc = wld_rad_nl80211_addInterface(pRadio, ifname, pMac, false, &ifaceInfo);
    ASSERT_FALSE(rc < SWL_RC_ERROR, rc, ME, "fail to add VAP(%s) on radio(%s)", ifname, pRadio->Name);
    SAH_TRACEZ_INFO(ME, "add VAP(%s)(ifIdx:%d) on radio(%s)", ifaceInfo.name, ifaceInfo.ifIndex, pRadio->Name);
    pAP->index = ifaceInfo.ifIndex;
    pAP->wDevId = ifaceInfo.wDevId;
    return rc;
}

swl_rc_ne wld_rad_nl80211_addEpInterface(T_Radio* pRadio, T_EndPoint* pEP) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_NOT_NULL(pRadio, rc, ME, "NULL");
    ASSERT_NOT_NULL(pEP, rc, ME, "NULL");
    const char* ifname = pEP->Name;
    ASSERT_STR(ifname, rc, ME, "Empty ifname");

    wld_nl80211_ifaceInfo_t ifaceInfo;
    swl_macBin_t* pMac = NULL;
    if((pEP->pSSID != NULL) && (!swl_mac_binIsNull((swl_macBin_t*) pEP->pSSID->MACAddress))) {
        pMac = (swl_macBin_t*) pEP->pSSID->MACAddress;
    }
    rc = wld_rad_nl80211_addInterface(pRadio, ifname, pMac, true, &ifaceInfo);
    ASSERT_FALSE(rc < SWL_RC_ERROR, rc, ME, "fail to add EP(%s) on radio(%s)", ifname, pRadio->Name);
    SAH_TRACEZ_INFO(ME, "add EP(%s)(ifIdx:%d) on radio(%s)", ifaceInfo.name, ifaceInfo.ifIndex, pRadio->Name);
    pEP->index = ifaceInfo.ifIndex;
    pEP->wDevId = ifaceInfo.wDevId;
    return rc;
}

uint8_t wld_rad_nl80211_addRadios(vendor_t* vendor,
                                  const uint32_t maxWiphys, const uint32_t maxWlIfaces,
                                  wld_nl80211_ifaceInfo_t wlIfacesInfo[maxWiphys][maxWlIfaces]) {
    uint8_t index = 0;
    ASSERT_NOT_NULL(vendor, index, ME, "NULL");
    uint32_t isSingleWiphy = (wld_nl80211_countWiphyFromFS() == 1);
    for(uint32_t i = 0; i < (isSingleWiphy ? maxWlIfaces : 1) && index < maxWiphys; i++) {
        for(uint32_t j = 0; j < maxWiphys && index < maxWiphys; j++) {
            wld_nl80211_ifaceInfo_t* pIface = &wlIfacesInfo[j][i];
            if(pIface->ifIndex <= 0) {
                continue;
            }
            T_Radio* pRad = wld_rad_get_radio(pIface->name);
            if(pRad == NULL) {
                SAH_TRACEZ_WARNING(ME, "Interface %s handled by %s", pIface->name, vendor->name);
                wld_addRadio(pIface->name, vendor, -1);
            } else {
                SAH_TRACEZ_WARNING(ME, "Interface %s already handled by %s", pIface->name, pRad->vendor->name);
            }
            index++;
        }
    }
    ASSERTW_NOT_EQUALS(index, 0, index, ME, "NO nl80211 Wireless interfaces found");
    return index;
}

swl_rc_ne wld_rad_nl80211_getWiphyInfo(T_Radio* pRadio, wld_nl80211_wiphyInfo_t* pWiphyInfo) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_nl80211_getWiphyInfo(wld_nl80211_getSharedState(), pRadio->index, pWiphyInfo);
}

swl_rc_ne wld_rad_nl80211_getSurveyInfo(T_Radio* pRadio, wld_nl80211_channelSurveyInfo_t** ppChanSurveyInfo, uint32_t* pnrChanSurveyInfo) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    uint32_t ifIndex = wld_rad_getFirstEnabledIfaceIndex(pRadio);
    wld_nl80211_channelSurveyParam_t config = {
        .selectFreqBand = pRadio->operatingFrequencyBand,
        .radioChannel = pRadio->channel,
    };
    return wld_nl80211_getSurveyInfoExt(wld_nl80211_getSharedState(), ifIndex, &config, ppChanSurveyInfo, pnrChanSurveyInfo);
}

/*
 * lowest period since last air stats calculation in ms
 * to avoid too small diff counters
 */
#define MIN_AIR_STATS_REFRESH_PERIOD_MS 500
static bool s_checkAirStatsTooRecentMs(wld_airStats_t* pStats, uint32_t nowTsMsU32) {
    return (pStats && DELTA32(nowTsMsU32, pStats->timestamp) <= MIN_AIR_STATS_REFRESH_PERIOD_MS);
}
static bool s_checkAirStatsTooRecent(wld_airStats_t* pStats) {
    swl_timeSpecMono_t nowTs;
    swl_timespec_getMono(&nowTs);
    uint32_t nowTsMsU32 = swl_timespec_toMs(&nowTs);
    return s_checkAirStatsTooRecentMs(pStats, nowTsMsU32);
}
swl_rc_ne wld_rad_nl80211_getAirStatsFromSurveyInfo(T_Radio* pRadio, wld_airStats_t* pStats, wld_nl80211_channelSurveyInfo_t* pChanSurveyInfo) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pStats, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pChanSurveyInfo, SWL_RC_INVALID_PARAM, ME, "NULL");
    if(!pChanSurveyInfo->inUse) {
        return SWL_RC_CONTINUE;
    }
    if((pChanSurveyInfo->timeOn == 0) ||
       (pChanSurveyInfo->timeBusy > pChanSurveyInfo->timeOn)) {
        SAH_TRACEZ_ERROR(ME, "\t%s: Center Freq %dMHz: wrong time activity values",
                         pRadio->Name, pChanSurveyInfo->frequencyMHz);
        return SWL_RC_ERROR;
    }

    swl_timeSpecMono_t nowTs;
    swl_timespec_getMono(&nowTs);
    uint32_t nowTsMsU32 = swl_timespec_toMs(&nowTs);

    if(pRadio->pLastAirStats == NULL) {
        pRadio->pLastAirStats = calloc(1, sizeof(wld_airStats_t));
        if(pRadio->pLastAirStats == NULL) {
            SAH_TRACEZ_ERROR(ME, "%s: fail to alloc air stats cache", pRadio->Name);
            return SWL_RC_ERROR;
        }
    } else if(s_checkAirStatsTooRecentMs(pRadio->pLastAirStats, nowTsMsU32)) {
        SAH_TRACEZ_INFO(ME, "%s: too frequent polling: return cached air stats", pRadio->Name);
        memcpy(pStats, pRadio->pLastAirStats, sizeof(*pStats));
        return SWL_RC_OK;
    }

    /*
     * cache last active chan survey info, per radio
     * as nl80211 survey info include cumulative time values
     * => for a proper runtime channel load calculation,
     * we need to diff with last meas before
     */
    if(pRadio->pLastSurvey == NULL) {
        pRadio->pLastSurvey = calloc(1, sizeof(wld_nl80211_channelSurveyInfo_t));
        if(pRadio->pLastSurvey == NULL) {
            SAH_TRACEZ_ERROR(ME, "%s: fail to alloc survey cache", pRadio->Name);
            return SWL_RC_ERROR;
        }
    }
    wld_nl80211_channelSurveyInfo_t* pLastActiveChanSurveyInfo = pRadio->pLastSurvey;
    wld_nl80211_channelSurveyInfo_t currChanSurveyInfo;
    memcpy(&currChanSurveyInfo, pChanSurveyInfo, sizeof(*pChanSurveyInfo));
    if((pLastActiveChanSurveyInfo->frequencyMHz == currChanSurveyInfo.frequencyMHz) &&
       (pLastActiveChanSurveyInfo->timeOn > 0)) {
        if((pLastActiveChanSurveyInfo->timeOn == currChanSurveyInfo.timeOn)) {
            SAH_TRACEZ_NOTICE(ME, "%s: Current and last survey timeOn must be different", pRadio->Name);
            memcpy(pStats, pRadio->pLastAirStats, sizeof(*pStats));
            return SWL_RC_OK;
        }
        if(pLastActiveChanSurveyInfo->timeOn < currChanSurveyInfo.timeOn) {
            currChanSurveyInfo.timeOn -= pLastActiveChanSurveyInfo->timeOn;
            currChanSurveyInfo.timeBusy -= pLastActiveChanSurveyInfo->timeBusy;
            currChanSurveyInfo.timeExtBusy -= pLastActiveChanSurveyInfo->timeExtBusy;
            currChanSurveyInfo.timeRx -= pLastActiveChanSurveyInfo->timeRx;
            currChanSurveyInfo.timeTx -= pLastActiveChanSurveyInfo->timeTx;
            currChanSurveyInfo.timeScan -= pLastActiveChanSurveyInfo->timeScan;
            currChanSurveyInfo.timeRxInBss -= pLastActiveChanSurveyInfo->timeRxInBss;
        }
    }
    memcpy(pLastActiveChanSurveyInfo, pChanSurveyInfo, sizeof(*pChanSurveyInfo));

    pChanSurveyInfo = &currChanSurveyInfo;
    pStats->timestamp = nowTsMsU32;
    pStats->noise = pChanSurveyInfo->noiseDbm;
    //use ratio, when timeOn value is beyond total_time variable max type value
    uint64_t base = SWL_MIN(pChanSurveyInfo->timeOn, SWL_BIT_SHIFT(SWL_BIT_SIZE(pStats->total_time)) - 1);

    pStats->total_time = base; // eqv. all pChanSurveyInfo->timeOn;
    pStats->bss_transmit_time = (pChanSurveyInfo->timeTx * base) / pChanSurveyInfo->timeOn;
    pStats->bss_receive_time = (pChanSurveyInfo->timeRxInBss * base) / pChanSurveyInfo->timeOn;
    pStats->other_bss_time = ((pChanSurveyInfo->timeRx - pChanSurveyInfo->timeRxInBss) * base) / pChanSurveyInfo->timeOn;
    uint64_t wifiTime = pChanSurveyInfo->timeTx + pChanSurveyInfo->timeRx + pChanSurveyInfo->timeScan;
    if(pChanSurveyInfo->timeBusy > wifiTime) {
        pStats->other_time = ((pChanSurveyInfo->timeBusy - wifiTime) * base) / pChanSurveyInfo->timeOn;
    }
    pStats->free_time = ((pChanSurveyInfo->timeOn - pChanSurveyInfo->timeBusy) * base) / pChanSurveyInfo->timeOn;
    pStats->load = SWL_MAX((bool) (pChanSurveyInfo->timeBusy > 0), SWL_MIN((pChanSurveyInfo->timeBusy * 100) / pChanSurveyInfo->timeOn, 100U));
    memcpy(pRadio->pLastAirStats, pStats, sizeof(*pStats));
    return SWL_RC_OK;
}

swl_rc_ne wld_rad_nl80211_getAirstats(T_Radio* pRadio, wld_airStats_t* pStats) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pStats, SWL_RC_INVALID_PARAM, ME, "NULL");

    if(s_checkAirStatsTooRecent(pRadio->pLastAirStats)) {
        SAH_TRACEZ_INFO(ME, "%s: too recent measurement: return cached air stats", pRadio->Name);
        memcpy(pStats, pRadio->pLastAirStats, sizeof(*pStats));
        return SWL_RC_OK;
    }

    memset(pStats, 0, sizeof(*pStats));
    uint32_t nChanSurveyInfo = 0;
    wld_nl80211_channelSurveyInfo_t* pChanSurveyInfoList = NULL;
    swl_rc_ne retVal = wld_rad_nl80211_getSurveyInfo(pRadio, &pChanSurveyInfoList, &nChanSurveyInfo);
    if(retVal < SWL_RC_OK) {
        SAH_TRACEZ_ERROR(ME, "%s: fail to get survey info info", pRadio->Name);
        free(pChanSurveyInfoList);
        return retVal;
    }

    wld_nl80211_channelSurveyInfo_t* pChanSurveyInfo;
    for(uint32_t id = 0; id < nChanSurveyInfo; id++) {
        pChanSurveyInfo = &pChanSurveyInfoList[id];
        retVal = wld_rad_nl80211_getAirStatsFromSurveyInfo(pRadio, pStats, pChanSurveyInfo);
        if(retVal <= SWL_RC_OK) {
            break;
        }
    }
    free(pChanSurveyInfoList);
    return retVal;
}

static wld_nl80211_chanStatus_e s_findChannelStatus(wld_nl80211_bandDef_t* band, uint32_t channelFreq) {
    for(uint32_t i = 0; i < band->nChans; i++) {
        if(band->chans[i].ctrlFreq == channelFreq) {
            return band->chans[i].status;
        }
    }
    return WLD_NL80211_CHAN_STATUS_UNKNOWN;
}

static bool s_isChanSurveyInfoSufficient(wld_nl80211_channelSurveyInfo_t* pChanSurveyInfo) {
    if(!(pChanSurveyInfo->filled & SURVEY_HAS_NF)) {
        pChanSurveyInfo->noiseDbm = DEFAULT_NOISE_DB;
        SAH_TRACEZ_WARNING(ME, "Survey for freq %d is missing noise floor", pChanSurveyInfo->frequencyMHz);
    }
    if(!(pChanSurveyInfo->filled & SURVEY_HAS_CHAN_TIME)) {
        pChanSurveyInfo->timeBusy = DEFAULT_TIME_BUSY;
        SAH_TRACEZ_WARNING(ME, "Survey for freq %d is missing channel time", pChanSurveyInfo->frequencyMHz);
    }
    if(!(pChanSurveyInfo->filled & SURVEY_HAS_CHAN_TIME_BUSY) && !(pChanSurveyInfo->filled & SURVEY_HAS_CHAN_TIME_RX)) {
        SAH_TRACEZ_WARNING(ME, "Survey for freq %d is missing RX and busy time (at least one is required)", pChanSurveyInfo->frequencyMHz);
        return false;
    }
    return true;
}

static float s_computeInterferenceFactor(wld_nl80211_channelSurveyInfo_t* pChanSurveyInfo, int8_t min_nf) {
    float factor = MAX_FACTOR, busy, total;
    if(!s_isChanSurveyInfoSufficient(pChanSurveyInfo)) {
        SAH_TRACEZ_WARNING(ME, "%d: insufficient data", pChanSurveyInfo->frequencyMHz);
        return factor;
    }
    if(pChanSurveyInfo->filled & SURVEY_HAS_CHAN_TIME_BUSY) {
        busy = pChanSurveyInfo->timeBusy;
    } else if(pChanSurveyInfo->filled & SURVEY_HAS_CHAN_TIME_RX) {
        busy = pChanSurveyInfo->timeRx;
    } else {
        SAH_TRACEZ_WARNING(ME, "Survey data missing");
        return factor;
    }
    total = pChanSurveyInfo->timeOn;
    if(pChanSurveyInfo->filled & SURVEY_HAS_CHAN_TIME_TX) {
        if((busy >= pChanSurveyInfo->timeTx) && (total >= pChanSurveyInfo->timeTx)) {
            busy -= pChanSurveyInfo->timeTx;
            total -= pChanSurveyInfo->timeTx;
        } else {
            SAH_TRACEZ_ERROR(ME, "Tx time is bigger that busy or total time");
            return factor;
        }
    }
    factor = INTERFERENCE_FACTOR(pChanSurveyInfo->noiseDbm, busy, total, min_nf);
    return factor;
}

static float s_getChanSurveyModeInterferenceFactor(T_Radio* rad, wld_nl80211_channelSurveyInfo_t* pChanSurveyInfo, int8_t min_nf) {
    wld_nl80211_wiphyInfo_t wiphyInfo;
    swl_rc_ne rc;

    rc = wld_rad_nl80211_getWiphyInfo(rad, &wiphyInfo);
    ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "Fail to get nl80211 wiphy info");
    wld_nl80211_bandDef_t* pOperBand = &wiphyInfo.bands[rad->operatingFrequencyBand];

    swl_chanspec_t chanSpec = SWL_CHANSPEC_EMPTY;
    swl_chanspec_channelFromMHz(&chanSpec, pChanSurveyInfo->frequencyMHz);
    if(s_findChannelStatus(pOperBand, pChanSurveyInfo->frequencyMHz) == WLD_NL80211_CHAN_DISABLED) {
        SAH_TRACEZ_WARNING(ME, "channel %d (%d MHz) is disabled", chanSpec.channel, pChanSurveyInfo->frequencyMHz);
        return MAX_FACTOR;
    }
    if(s_findChannelStatus(pOperBand, pChanSurveyInfo->frequencyMHz) == WLD_NL80211_CHAN_UNAVAILABLE) {
        SAH_TRACEZ_WARNING(ME, "channel %d (%d MHz) is unavailable", chanSpec.channel, pChanSurveyInfo->frequencyMHz);
        return MAX_FACTOR;
    }
    if(!wld_rad_hasChannel(rad, chanSpec.channel)) {
        SAH_TRACEZ_WARNING(ME, "channel %d (%d MHz) not found", chanSpec.channel, pChanSurveyInfo->frequencyMHz);
        return MAX_FACTOR;
    }
    return s_computeInterferenceFactor(pChanSurveyInfo, min_nf);
}

static int8_t s_updateLowestNF(wld_nl80211_channelSurveyInfo_t* pChanSurveyInfoList, uint32_t nChanSurveyInfo) {
    int8_t lowest_nf = 0;
    for(uint32_t i = 0; i < nChanSurveyInfo; i++) {
        wld_nl80211_channelSurveyInfo_t* pChanSurveyInfo = &pChanSurveyInfoList[i];
        if(pChanSurveyInfo->noiseDbm < lowest_nf) {
            lowest_nf = pChanSurveyInfo->noiseDbm;
        }
    }
    return lowest_nf;
}

swl_rc_ne wld_rad_nl80211_updateChanSurveyReportFromSurveyInfo(T_Radio* rad, wld_nl80211_channelSurveyInfo_t* pChanSurveyInfoList, uint32_t nChanSurveyInfo) {
    swl_rc_ne rc;
    ASSERT_NOT_NULL(rad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pChanSurveyInfoList, SWL_RC_OK, ME, "Empty");

    int8_t lowest_nf = s_updateLowestNF(pChanSurveyInfoList, nChanSurveyInfo);

    wld_cleanupSurveyReport(&rad->scanState.lastSurveyReport);

    for(uint32_t i = 0; i < nChanSurveyInfo; i++) {
        wld_nl80211_channelSurveyInfo_t* pChanSurveyInfo = &pChanSurveyInfoList[i];
        wld_chanSurveyReportEntry_t* pSrEntry = calloc(1, sizeof(wld_chanSurveyReportEntry_t));
        if(pSrEntry == NULL) {
            wld_cleanupSurveyReport(&rad->scanState.lastSurveyReport);
            return SWL_RC_ERROR;
        }
        pSrEntry->frequencyMHz = pChanSurveyInfo->frequencyMHz;
        pSrEntry->interferenceFactor = (int32_t) (s_getChanSurveyModeInterferenceFactor(rad, pChanSurveyInfo, lowest_nf) * TEN_POW_6);
        SAH_TRACEZ_INFO(ME, "updateChanSurveyReport: Result : freq=%d timeOn=%lu timeBusy=%lu timeTx=%lu timeRx=%lu noiseDbm= %d lowest_nf= %d dBm inUse=%d interferenceFactor=%d",
                        pChanSurveyInfo->frequencyMHz, pChanSurveyInfo->timeOn, pChanSurveyInfo->timeBusy, pChanSurveyInfo->timeTx,
                        pChanSurveyInfo->timeRx, pChanSurveyInfo->noiseDbm, lowest_nf, pChanSurveyInfo->inUse, pSrEntry->interferenceFactor);
        amxc_llist_it_init(&pSrEntry->it);
        amxc_llist_append(&rad->scanState.lastSurveyReport.surveyReport, &pSrEntry->it);
    }
    return SWL_RC_OK;
}

swl_rc_ne wld_rad_nl80211_updateUsageStatsFromSurveyInfo(T_Radio* pRadio, amxc_llist_t* pOutSpectrumResults,
                                                         wld_nl80211_channelSurveyInfo_t* pChanSurveyInfoList, uint32_t nChanSurveyInfo) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pOutSpectrumResults, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pChanSurveyInfoList, SWL_RC_OK, ME, "Empty");

    for(uint32_t i = 0; i < nChanSurveyInfo; i++) {
        wld_spectrumChannelInfoEntry_t chanInfo;
        memset(&chanInfo, 0, sizeof(chanInfo));
        swl_chanspec_t chanSpec;
        wld_nl80211_channelSurveyInfo_t* pChanSurveyInfo = &pChanSurveyInfoList[i];
        if(swl_chanspec_channelFromMHz(&chanSpec, pChanSurveyInfo->frequencyMHz) < SWL_RC_OK) {
            continue;
        }

        chanInfo.channel = chanSpec.channel;
        chanInfo.bandwidth = SWL_BW_20MHZ;
        chanInfo.noiselevel = pChanSurveyInfo->noiseDbm;
        wld_airStats_t airStats;
        memset(&airStats, 0, sizeof(airStats));
        if(!pChanSurveyInfo->inUse) {
            if((pChanSurveyInfo->timeOn > 0) && (pChanSurveyInfo->timeBusy <= pChanSurveyInfo->timeOn)) {
                chanInfo.availability = SWL_MIN((((pChanSurveyInfo->timeOn - pChanSurveyInfo->timeBusy) * 100) / pChanSurveyInfo->timeOn), 100LLU);
            }
        } else if(wld_rad_nl80211_getAirStatsFromSurveyInfo(pRadio, &airStats, pChanSurveyInfo) == SWL_RC_OK) {
            if(airStats.total_time > 0) {
                uint64_t ourTime = airStats.bss_receive_time + airStats.bss_transmit_time + airStats.other_bss_time;
                chanInfo.ourUsage = SWL_MAX((bool) (ourTime > 0), SWL_MIN(((ourTime * 100) / airStats.total_time), 100U));
                chanInfo.availability = SWL_MIN(((airStats.free_time * 100) / airStats.total_time), 100);
            }
        }

        wld_util_addorUpdateSpectrumEntry(pOutSpectrumResults, &chanInfo);
    }

    return SWL_RC_OK;
}

swl_rc_ne wld_rad_nl80211_setAntennas(T_Radio* pRadio, uint32_t txMapAnt, uint32_t rxMapAnt) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_nl80211_setWiphyAntennas(wld_nl80211_getSharedState(), pRadio->index, txMapAnt, rxMapAnt);
}

swl_rc_ne wld_rad_nl80211_setTxPowerAuto(T_Radio* pRadio) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_nl80211_setTxPowerAuto(wld_nl80211_getSharedState(), pRadio->index);
}

swl_rc_ne wld_rad_nl80211_setTxPowerFixed(T_Radio* pRadio, int32_t dbm) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_nl80211_setTxPowerFixed(wld_nl80211_getSharedState(), pRadio->index, dbm);
}

swl_rc_ne wld_rad_nl80211_setTxPowerLimited(T_Radio* pRadio, int32_t dbm) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_nl80211_setTxPowerLimited(wld_nl80211_getSharedState(), pRadio->index, dbm);
}

swl_rc_ne wld_rad_nl80211_getTxPower(T_Radio* pRadio, int32_t* dbm) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    uint32_t ifIndex = wld_rad_getFirstEnabledIfaceIndex(pRadio);
    if(ifIndex <= 0) {
        SAH_TRACEZ_ERROR(ME, "%s: rad has no enabled iface", pRadio->Name);
        return SWL_RC_ERROR;
    }
    return wld_nl80211_getTxPower(wld_nl80211_getSharedState(), ifIndex, dbm);
}

swl_rc_ne wld_rad_nl80211_getMaxTxPowerdBm(T_Radio* pRad, uint16_t channel, int32_t* dbm) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    uint32_t curFreq;
    swl_chanspec_t spec = SWL_CHANSPEC_NEW(channel, SWL_BW_20MHZ, pRad->operatingFrequencyBand);
    ASSERT_FALSE(swl_chanspec_channelToMHz(&spec, &curFreq), SWL_RC_INVALID_PARAM, ME, "invalid chan %d", channel);
    return wld_nl80211_getMaxTxPowerdBm(wld_nl80211_getSharedState(), pRad->index, curFreq, dbm);
}


swl_rc_ne wld_rad_nl80211_getChanSpecFromIfaceInfo(swl_chanspec_t* pChanSpec, wld_nl80211_ifaceInfo_t* pIfaceInfo) {
    return wld_nl80211_getChanSpecFromIfaceInfo(pChanSpec, pIfaceInfo);
}

swl_rc_ne wld_rad_nl80211_getChannel(T_Radio* pRadio, swl_chanspec_t* pChanSpec) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pChanSpec, SWL_RC_INVALID_PARAM, ME, "NULL");
    swl_rc_ne rc;
    wld_nl80211_ifaceInfo_t ifaceInfo;
    /*
     * disabled main iface (usually matching radio iface) may not carry channel info
     * so fetch first vap iface having channel info
     */
    T_AccessPoint* pAP = NULL;
    wld_rad_forEachAp(pAP, pRadio) {
        if((wld_ssid_isEnabledWithRef(pAP->pSSID)) &&
           (wld_ap_nl80211_getInterfaceInfo(pAP, &ifaceInfo) >= SWL_RC_OK) &&
           ((rc = wld_nl80211_getChanSpecFromIfaceInfo(pChanSpec, &ifaceInfo)) >= SWL_RC_OK)) {
            return rc;
        }
    }
    T_EndPoint* pEP;
    wld_rad_forEachEp(pEP, pRadio) {
        if((wld_ssid_isEnabledWithRef(pEP->pSSID)) &&
           (wld_ep_nl80211_getInterfaceInfo(pEP, &ifaceInfo) >= SWL_RC_OK) &&
           ((rc = wld_nl80211_getChanSpecFromIfaceInfo(pChanSpec, &ifaceInfo)) >= SWL_RC_OK)) {
            return rc;
        }
    }
    if(((rc = wld_rad_nl80211_getInterfaceInfo(pRadio, &ifaceInfo)) >= SWL_RC_OK) &&
       ((rc = wld_nl80211_getChanSpecFromIfaceInfo(pChanSpec, &ifaceInfo)) >= SWL_RC_OK)) {
        return rc;
    }
    return rc;
}

swl_rc_ne wld_rad_nl80211_setRegDomain(T_Radio* pRadio, const char* alpha2) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_NOT_NULL(pRadio, rc, ME, "NULL");
    SAH_TRACEZ_WARNING(ME, "%s: setting reg domain %s", pRadio->Name, alpha2);
    rc = wld_nl80211_setRegDomain(wld_nl80211_getSharedState(), pRadio->wiphy, alpha2);
    return rc;
}

swl_rc_ne wld_rad_nl80211_getFirstEnabledVapLinkInfo(T_Radio* pRadio, int32_t* pIfIndex, int8_t* pLinkId) {
    int8_t ifMloLinkId = MLO_LINK_ID_UNKNOWN;
    int32_t index = 0;
    W_SWL_SETPTR(pIfIndex, index);
    W_SWL_SETPTR(pLinkId, ifMloLinkId);
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    T_AccessPoint* pMainAp = wld_rad_getFirstEnabledVap(pRadio);
    if(pMainAp != NULL) {
        index = wld_ssid_nl80211_getPrimaryLinkIfIndex(pMainAp->pSSID);
        ifMloLinkId = wld_ssid_nl80211_getMldLinkId(pMainAp->pSSID);
    }
    if(index <= 0) {
        index = wld_rad_getFirstEnabledIfaceIndex(pRadio);
    }
    W_SWL_SETPTR(pIfIndex, index);
    W_SWL_SETPTR(pLinkId, ifMloLinkId);
    return SWL_RC_OK;
}

swl_rc_ne wld_rad_nl80211_bgDfsStart(T_Radio* pRadio, wld_startBgdfsArgs_t* args) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(args, SWL_RC_INVALID_PARAM, ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: Starting BG_DFS %u/%s",
                    pRadio->Name, args->channel, swl_bandwidth_str[args->bandwidth]);

    swl_chanspec_t bgDfsChanspec = SWL_CHANSPEC_NEW(args->channel, args->bandwidth, pRadio->operatingFrequencyBand);
    int8_t ifMloLinkId = MLO_LINK_ID_UNKNOWN;
    int32_t index = 0;
    wld_rad_nl80211_getFirstEnabledVapLinkInfo(pRadio, &index, &ifMloLinkId);

    swl_rc_ne rc = wld_nl80211_bgDfsStart(wld_nl80211_getSharedState(), index, ifMloLinkId, bgDfsChanspec);
    ASSERT_TRUE(swl_rc_isOk(rc), rc, ME, "%s: fail to start bgDfs (index:%d, ifMloLinkId:%d, chSpec:%s) rc:%d",
                pRadio->Name, index, ifMloLinkId, swl_typeChanspecExt_toBuf32Ref(&bgDfsChanspec).buf, rc);
    return rc;
}

swl_rc_ne wld_rad_nl80211_bgDfsStop(T_Radio* pRadio) {
    ASSERT_NOT_NULL(pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: Stopping BG_DFS", pRadio->Name);

    int8_t ifMloLinkId = MLO_LINK_ID_UNKNOWN;
    int32_t index = 0;
    wld_rad_nl80211_getFirstEnabledVapLinkInfo(pRadio, &index, &ifMloLinkId);

    return wld_nl80211_bgDfsStop(wld_nl80211_getSharedState(), index, ifMloLinkId);
}

swl_rc_ne wld_rad_nl80211_registerFrame(T_Radio* pRadio, uint16_t type, const char* pattern, size_t patternLen) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_NOT_NULL(pRadio, rc, ME, "NULL");
    uint32_t ifIndex = wld_rad_getFirstActiveIfaceIndex(pRadio);
    return wld_nl80211_registerFrame(wld_nl80211_getSharedState(), ifIndex, type, pattern, patternLen);
}

swl_rc_ne wld_rad_nl80211_sendVendorSubCmd(T_Radio* pRadio, uint32_t oui, int subcmd, void* data, int dataLen,
                                           bool isSync, bool withAck, uint32_t flags, wld_nl80211_handler_f handler, void* priv) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_NOT_NULL(pRadio, rc, ME, "NULL");

    rc = wld_nl80211_sendVendorSubCmd(wld_nl80211_getSharedState(), oui, subcmd, data, dataLen, isSync, withAck,
                                      flags, pRadio->index, pRadio->wDevId, handler, priv);

    return rc;
}

swl_rc_ne wld_rad_nl80211_sendVendorSubCmdAttr(T_Radio* pRadio, uint32_t oui, int subcmd, wld_nl80211_nlAttr_t* vendorAttr,
                                               bool isSync, bool withAck, uint32_t flags, wld_nl80211_handler_f handler, void* priv) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_NOT_NULL(pRadio, rc, ME, "NULL");

    rc = wld_nl80211_sendVendorSubCmdAttr(wld_nl80211_getSharedState(), oui, subcmd, vendorAttr, isSync, withAck,
                                          flags, pRadio->index, pRadio->wDevId, handler, priv);

    return rc;
}
