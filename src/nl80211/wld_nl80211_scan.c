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
 * This file implements wrapper functions to use nl80211 generic scan apis with T_Radio context
 */

#include "wld_rad_nl80211.h"
#include "wld_nl80211_utils.h"
#include "wld_linuxIfUtils.h"
#include "swl/swl_common.h"
#include "swl/swl_common_time.h"
#include "wld_radio.h"
#include "wld_util.h"

#define ME "nlScan"

typedef struct {
    amxc_llist_it_t it;
    T_Radio* pRad;
    wld_nl80211_scanFlags_t flags;
    bool isStarted;
} scanInfo_t;

static amxc_llist_t sScanPool = {NULL, NULL};

static scanInfo_t* s_findScan(T_Radio* pRad) {
    amxc_llist_for_each(it, &sScanPool) {
        scanInfo_t* pScanInfo = amxc_container_of(it, scanInfo_t, it);
        if(pScanInfo->pRad == pRad) {
            return pScanInfo;
        }
    }
    return NULL;
}

static scanInfo_t* s_checkScan(scanInfo_t* pScanInfo) {
    amxc_llist_for_each(it, &sScanPool) {
        if(pScanInfo == amxc_container_of(it, scanInfo_t, it)) {
            return pScanInfo;
        }
    }
    return NULL;
}

static scanInfo_t* s_addScan(T_Radio* pRad, wld_nl80211_scanFlags_t* pFlags) {
    ASSERTS_NOT_NULL(pRad, NULL, ME, "NULL");
    scanInfo_t* pScanInfo = s_findScan(pRad);
    if(pScanInfo == NULL) {
        pScanInfo = calloc(1, sizeof(*pScanInfo));
        ASSERT_NOT_NULL(pScanInfo, NULL, ME, "NULL");
        amxc_llist_it_init(&pScanInfo->it);
        pScanInfo->pRad = pRad;
        amxc_llist_append(&sScanPool, &pScanInfo->it);
    }
    if(pFlags) {
        pScanInfo->flags = *pFlags;
    }
    return pScanInfo;
}

static int s_delScan(T_Radio* pRad) {
    scanInfo_t* pScanInfo = s_findScan(pRad);
    ASSERTS_NOT_NULL(pScanInfo, -1, ME, "NULL");
    int pos = amxc_llist_it_index_of(&pScanInfo->it);
    amxc_llist_it_take(&pScanInfo->it);
    free(pScanInfo);
    return pos;
}

static scanInfo_t* s_getNextScan(uint32_t wiphy) {
    amxc_llist_for_each(it, &sScanPool) {
        scanInfo_t* pScanInfo = amxc_container_of(it, scanInfo_t, it);
        if((debugIsRadPointer(pScanInfo->pRad)) && (pScanInfo->pRad->wiphy == wiphy)) {
            return pScanInfo;
        }
    }
    return NULL;
}

static void s_runScan(scanInfo_t* pScanInfo) {
    ASSERT_NOT_NULL(s_checkScan(pScanInfo), , ME, "Unknown scanInfo ctx %p", pScanInfo);
    T_Radio* pRad = pScanInfo->pRad;
    ASSERT_TRUE(debugIsRadPointer(pRad), , ME, "Invalid rad ctx");
    SAH_TRACEZ_INFO(ME, "%s: starting next scan", pRad->Name);
    if(wld_rad_nl80211_startScanExt(pRad, &pScanInfo->flags) < SWL_RC_OK) {
        SAH_TRACEZ_ERROR(ME, "%s: report scan async failure", pRad->Name);
        wld_scan_done(pRad, false);
    }
}

swl_rc_ne wld_rad_nl80211_scheduleNextScan(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    s_delScan(pRad);
    scanInfo_t* pNextScanInfo = s_getNextScan(pRad->wiphy);
    ASSERTI_NOT_NULL(pNextScanInfo, SWL_RC_NOT_AVAILABLE, ME, "%s: No pending scan for wiphy id:%d", pRad->Name, pRad->wiphy);
    SAH_TRACEZ_INFO(ME, "%s: scheduling next scan", pNextScanInfo->pRad->Name);
    swla_delayExec_add((swla_delayExecFun_cbf) s_runScan, pNextScanInfo);
    return SWL_RC_OK;
}

static void s_setScanDuration(T_Radio* pRadio, wld_nl80211_scanParams_t* params) {
    ASSERTS_NOT_NULL(params, , ME, "NULL");
    params->measDuration = 0;
    params->measDurationMandatory = false;
    ASSERTS_NOT_NULL(pRadio, , ME, "NULL");
    wld_scan_config_t* pCfg = &pRadio->scanState.cfg;
    bool suppScanDwell = pRadio->pFA->mfn_misc_has_support(pRadio, NULL, "SCAN_DWELL", 0);
    ASSERTI_TRUE(suppScanDwell, , ME, "%s: nl80211 driver does not support setting scan dwell", pRadio->Name);

    //consider passive scan for freqBand 6GHz
    if((pRadio->operatingFrequencyBand != SWL_FREQ_BAND_EXT_6GHZ) && (swl_unLiList_size(&params->ssids) > 0)) {
        if((pCfg->activeChannelTime >= SCAN_ACTIVE_DWELL_MIN) && (pCfg->activeChannelTime <= SCAN_ACTIVE_DWELL_MAX)) {
            params->measDuration = pCfg->activeChannelTime;
            params->measDurationMandatory = true;
        } else if(pCfg->activeChannelTime == -1) {
            params->measDuration = SCAN_ACTIVE_DWELL_DEFAULT;
        }
    } else if((pCfg->passiveChannelTime >= SCAN_PASSIVE_DWELL_MIN) && (pCfg->passiveChannelTime <= SCAN_PASSIVE_DWELL_MAX)) {
        params->measDuration = pCfg->passiveChannelTime;
        params->measDurationMandatory = true;
    } else if(pCfg->passiveChannelTime == -1) {
        params->measDuration = SCAN_PASSIVE_DWELL_DEFAULT;
    }
}

static bool s_addScanFreq(T_Radio* pRadio, swl_channel_t chan, swl_unLiList_t* pFreqs) {
    ASSERT_NOT_NULL(pRadio, false, ME, "NULL");
    if(!wld_rad_hasChannel(pRadio, chan)) {
        SAH_TRACEZ_ERROR(ME, "rad(%s) does not support scan chan(%d)",
                         pRadio->Name, chan);
        return false;
    }
    swl_chanspec_t chanspec = swl_chanspec_fromDm(chan,
                                                  pRadio->operatingChannelBandwidth,
                                                  pRadio->operatingFrequencyBand);
    uint32_t freq = wld_channel_getFrequencyOfChannel(chanspec);
    swl_unLiList_add(pFreqs, &freq);
    return true;
}

bool wld_nl80211_hasStartedScan(void* pRef, uint32_t wiphyId, int32_t ifIndex) {
    if(amxc_llist_is_empty(&sScanPool)) {
        ASSERTS_TRUE(debugIsRadPointer(pRef), false, ME, "invalid");
        return wld_scan_isRunning(pRef);
    }
    amxc_llist_for_each(it, &sScanPool) {
        scanInfo_t* pScanInfo = amxc_container_of(it, scanInfo_t, it);
        if((pScanInfo->isStarted) &&
           (pScanInfo->pRad != NULL) && (pScanInfo->pRad->wiphy == wiphyId) &&
           ((pRef == NULL) || (pScanInfo->pRad == (T_Radio*) pRef)) &&
           ((ifIndex < 0) || (wld_rad_hasLinkIfIndex(pScanInfo->pRad, ifIndex)))) {
            return true;
        }
    }
    return false;
}

swl_rc_ne wld_rad_nl80211_startScanExt(T_Radio* pRadio, wld_nl80211_scanFlags_t* pFlags) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    scanInfo_t* pScanInfo = NULL;
    if(pRadio == NULL) {
        SAH_TRACEZ_ERROR(ME, "invalid param");
        goto scan_exit;
    }

    /*
     * start_scan command has to be sent to enabled interface (UP)
     * (even when secondary VAP, while primary is disabled)
     */
    int index = wld_rad_getFirstEnabledIfaceIndex(pRadio);
    if(index <= 0) {
        rc = SWL_RC_INVALID_STATE;
        SAH_TRACEZ_ERROR(ME, "%s: rad has no enabled iface", pRadio->Name);
        goto scan_exit;
    }

    if(wld_rad_countWiphyRads(pRadio->wiphy) > 1) {
        pScanInfo = s_addScan(pRadio, pFlags);
        scanInfo_t* pNextScanInfo = s_getNextScan(pRadio->wiphy);
        ASSERTW_EQUALS(pScanInfo, pNextScanInfo, SWL_RC_CONTINUE,
                       ME, "%s: add scan request to pending scan pool", pRadio->Name);
    }

    wld_nl80211_state_t* state = wld_nl80211_getSharedState();
    wld_nl80211_scanParams_t params;
    memset(&params, 0, sizeof(wld_nl80211_scanParams_t));
    swl_unLiList_init(&params.ssids, sizeof(char*));
    swl_unLiList_init(&params.freqs, sizeof(uint32_t));
    params.iesLen = 0;
    wld_scanArgs_t* args = &pRadio->scanState.cfg.scanArguments;
    for(int i = 0; i < args->chanCount; i++) {
        if(!s_addScanFreq(pRadio, args->chanlist[i], &params.freqs)) {
            SAH_TRACEZ_ERROR(ME, "rad(%s) does not support scan chan(%d)",
                             pRadio->Name, args->chanlist[i]);
            goto scan_error;
        }
    }
    if(args->ssid[0] && (args->ssidLen > 0)) {
        char* ssid = args->ssid;
        swl_unLiList_add(&params.ssids, &ssid);
    }
    if(swl_mac_binIsNull(&args->bssid) == false) {
        memcpy(&params.bssid, &args->bssid, SWL_MAC_BIN_LEN);
    }
    if(pFlags != NULL) {
        memcpy(&params.flags, pFlags, sizeof(wld_nl80211_scanFlags_t));
    }
    s_setScanDuration(pRadio, &params);

    rc = wld_nl80211_startScan(state, index, &params);

scan_error:
    swl_unLiList_destroy(&params.ssids);
    swl_unLiList_destroy(&params.freqs);
scan_exit:
    if(!swl_rc_isOk(rc)) {
        wld_rad_nl80211_scheduleNextScan(pRadio);
    } else if(pScanInfo != NULL) {
        pScanInfo->isStarted = true;
    }
    return rc;
}

swl_rc_ne wld_rad_nl80211_startScan(T_Radio* pRadio) {
    wld_scanArgs_t* args = &pRadio->scanState.cfg.scanArguments;
    wld_nl80211_scanFlags_t flags = {.flush = args->enableFlush};
    return wld_rad_nl80211_startScanExt(pRadio, &flags);
}

swl_rc_ne wld_rad_nl80211_abortScan(T_Radio* pRadio) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_NOT_NULL(pRadio, rc, ME, "NULL");
    rc = SWL_RC_OK;
    scanInfo_t* pScanInfo = s_findScan(pRadio);
    if(wld_rad_isUpExt(pRadio) &&
       ((amxc_llist_is_empty(&sScanPool) && (wld_scan_isRunning(pRadio))) ||
        ((pScanInfo != NULL) && pScanInfo->isStarted))) {
        /*
         * abort_scan command has to be sent to enabled interface (UP)
         * (even when secondary VAP, while primary is disabled)
         */
        uint32_t ifIndex = wld_rad_getFirstEnabledIfaceIndex(pRadio);
        rc = wld_nl80211_abortScan(wld_nl80211_getSharedState(), ifIndex);
        if(swl_rc_isOk(rc)) {
            rc = SWL_RC_CONTINUE;
        }
    }

    /*
     * Initiating a new scan before receiving the NL80211_CMD_SCAN_ABORTED event results in the driver returning -EBUSY (errno 16).
     * Schedule the next scan upon receiving the scan aborted event.
     */
    if(((pScanInfo != NULL) && !pScanInfo->isStarted)) {
        s_delScan(pRadio);
    } else if(rc != SWL_RC_CONTINUE) {
        wld_rad_nl80211_scheduleNextScan(pRadio);
    }

    return rc;
}

struct getScanResultsData_s {
    T_Radio* pRadio;
    scanResultsCb_f fScanResultsCb;
    void* priv;
};

static void s_filterInvalidResults(T_Radio* pRad, wld_scanResults_t* pResults) {
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    ASSERTS_NOT_NULL(pResults, , ME, "NULL");
    amxc_llist_for_each(it, &pResults->ssids) {
        wld_scanResultSSID_t* pResult = amxc_container_of(it, wld_scanResultSSID_t, it);
        if((!wld_rad_hasChannel(pRad, pResult->channel)) ||
           (((pRad->operatingFrequencyBand != SWL_FREQ_BAND_EXT_AUTO) &&
             (pResult->operClass > 0) &&
             (pRad->operatingFrequencyBand != swl_chanspec_operClassToFreq(pResult->operClass))))) {
            SAH_TRACEZ_WARNING(ME, "%s: filter out invalid scan result with opClass %d chan %d, not part of the tgt freqBand %s",
                               pRad->Name, pResult->operClass, pResult->channel, swl_freqBandExt_str[pRad->operatingFrequencyBand]);
            amxc_llist_it_take(&pResult->it);
            free(pResult);
        }
    }
}
static void s_scanResultsCb(void* priv, swl_rc_ne rc, wld_scanResults_t* pResults) {
    struct getScanResultsData_s* pScanResultsData = (struct getScanResultsData_s*) priv;
    ASSERTS_NOT_NULL(pScanResultsData, , ME, "NULL");
    T_Radio* pRad = pScanResultsData->pRadio;
    if(pScanResultsData->fScanResultsCb != NULL) {
        s_filterInvalidResults(pRad, pResults);
        pScanResultsData->fScanResultsCb(pScanResultsData->priv, rc, pResults);
    }
    free(pScanResultsData);
    wld_rad_nl80211_scheduleNextScan(pRad);
}

swl_rc_ne wld_rad_nl80211_getScanResults(T_Radio* pRadio, void* priv, scanResultsCb_f fScanResultsCb) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_NOT_NULL(pRadio, rc, ME, "NULL");
    struct getScanResultsData_s* pScanResultsData = calloc(1, sizeof(struct getScanResultsData_s));
    if(pScanResultsData == NULL) {
        SAH_TRACEZ_ERROR(ME, "Fail to alloc getScanResults req data");
        rc = SWL_RC_ERROR;
        if(fScanResultsCb) {
            fScanResultsCb(priv, rc, NULL);
        }
        return rc;
    }
    pScanResultsData->pRadio = pRadio;
    pScanResultsData->fScanResultsCb = fScanResultsCb;
    pScanResultsData->priv = priv;
    return wld_nl80211_getScanResultsPerFreqBand(wld_nl80211_getSharedState(), pRadio->index, pScanResultsData, s_scanResultsCb, pRadio->operatingFrequencyBand);
}

