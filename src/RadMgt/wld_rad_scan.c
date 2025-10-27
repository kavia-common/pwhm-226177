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

#include "wld_common.h"

#define ME "wldScan"

#define DIAG_SCAN_REASON "NeighboringDiag"
#define FULL_SCAN_REASON "FullScan"

typedef struct {
    swl_function_deferredInfo_t callInfo;
    amxc_llist_t runningRads;   /* list of radios running diag scan. */
    amxc_llist_t completedRads; /* list of radios reporting scan results. */
    amxc_llist_t canceledRads;  /* list of radios having cancelled scan. */
    amxc_llist_t failedRads;    /* list of radios having failed to scan (busy, not ready...). */
} neighboringWiFiDiagnosticInfo_t;

/**
 * global neighboringWiFiDiagnostic context.
 */
neighboringWiFiDiagnosticInfo_t g_neighWiFiDiag;

static amxd_object_t* s_scanStatsInitialize(T_Radio* pRad, const char* scanReason) {
    amxd_object_t* scanStatsTemp = amxd_object_findf(pRad->pBus, "ScanStats.ScanReason");
    ASSERTS_NOT_NULL(scanStatsTemp, NULL, ME, "ScanReason Obj NULL");
    amxd_object_t* obj = amxd_object_get(scanStatsTemp, scanReason);
    if(obj == NULL) {
        amxd_trans_t trans;
        ASSERT_TRANSACTION_INIT(scanStatsTemp, &trans, NULL, ME, "%s : trans init failure", pRad->Name);
        amxd_trans_add_inst(&trans, 0, scanReason);
        amxd_trans_set_value(cstring_t, &trans, "Name", scanReason);
        ASSERT_TRANSACTION_LOCAL_DM_END(&trans, NULL, ME, "%s : trans apply failure", pRad->Name);
        obj = amxd_object_get(scanStatsTemp, scanReason);
        ASSERT_NOT_NULL(obj, NULL, ME, "Failed to create object: %s", scanReason);
    }
    return obj;
}

static void s_setScanConfig_ocf(void* priv _UNUSED, amxd_object_t* object, const amxc_var_t* const newParamValues _UNUSED) {
    SAH_TRACEZ_IN(ME);

    T_Radio* pRad = wld_rad_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s update scan config", pRad->Name);

    pRad->scanState.cfg.homeTime = amxd_object_get_int32_t(object, "HomeTime", NULL);
    pRad->scanState.cfg.activeChannelTime = amxd_object_get_int32_t(object, "ActiveChannelTime", NULL);
    pRad->scanState.cfg.passiveChannelTime = amxd_object_get_int32_t(object, "PassiveChannelTime", NULL);
    pRad->scanState.cfg.maxChannelsPerScan = amxd_object_get_int32_t(object, "MaxChannelsPerScan", NULL);
    pRad->scanState.cfg.scanRequestInterval = amxd_object_get_int32_t(object, "ScanRequestInterval", NULL);
    pRad->scanState.cfg.scanChannelCount = amxd_object_get_int32_t(object, "ScanChannelCount", NULL);
    pRad->scanState.cfg.enableScanResultsDm = amxd_object_get_bool(object, "EnableScanResultsDm", NULL);

    if(pRad->scanState.cfg.fastScanReasons != NULL) {
        free(pRad->scanState.cfg.fastScanReasons);
        pRad->scanState.cfg.fastScanReasons = NULL;
    }
    pRad->scanState.cfg.fastScanReasons = amxd_object_get_cstring_t(object, "FastScanReasons", NULL);

    SAH_TRACEZ_OUT(ME);
}

SWLA_DM_HDLRS(sScanConfigDmHdlrs, ARR(), .objChangedCb = s_setScanConfig_ocf);

void _wld_rad_setScanConfig_ocf(const char* const sig_name,
                                const amxc_var_t* const data,
                                void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sScanConfigDmHdlrs, sig_name, data, priv);
}

static void s_updateScanStats(T_Radio* pRad) {
    amxd_object_t* objScanStats = amxd_object_findf(pRad->pBus, "ScanStats");
    ASSERTS_NOT_NULL(objScanStats, , ME, "ScanStats Obj NULL");

    amxd_trans_t trans;
    ASSERT_TRANSACTION_INIT(objScanStats, &trans, , ME, "%s : trans init failure", pRad->Name);

    amxd_trans_set_uint32_t(&trans, "NrScanRequested", pRad->scanState.stats.nrScanRequested);
    amxd_trans_set_uint32_t(&trans, "NrScanDone", pRad->scanState.stats.nrScanDone);
    amxd_trans_set_uint32_t(&trans, "NrScanError", pRad->scanState.stats.nrScanError);
    amxd_trans_set_uint32_t(&trans, "NrScanBlocked", pRad->scanState.stats.nrScanBlocked);

    amxc_llist_for_each(it, &(pRad->scanState.stats.extendedStat)) {
        wld_scanReasonStats_t* stats = amxc_llist_it_get_data(it, wld_scanReasonStats_t, it);
        amxd_trans_select_object(&trans, stats->object);
        amxd_trans_set_uint32_t(&trans, "NrScanRequested", stats->totalCount);
        amxd_trans_set_uint32_t(&trans, "NrScanDone", stats->successCount);
        amxd_trans_set_uint32_t(&trans, "NrScanError", stats->errorCount);
    }

    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, , ME, "%s : trans apply failure", pRad->Name);

}

static void s_updateScanProcState(T_Radio* pRad) {
    if((pRad->pBus != NULL) && (pRad->hasDmReady)) {
        swl_typeBool_commitObjectParam(amxd_object_get(pRad->pBus, "ScanResults"), "ScanInProgress", wld_scan_isRunning(pRad));
    }
}

void wld_radio_copySsidsToResult(wld_scanResults_t* results, amxc_llist_t* ssid_list) {
    ASSERTS_NOT_NULL(results, , ME, "NULL");
    ASSERTS_NOT_NULL(ssid_list, , ME, "NULL");

    amxc_llist_for_each(item, ssid_list) {
        wld_scanResultSSID_t* cur_ssid = amxc_llist_it_get_data(item, wld_scanResultSSID_t, it);
        wld_scanResultSSID_t* ssid_new = calloc(1, sizeof(wld_scanResultSSID_t));
        ASSERTS_NOT_NULL(ssid_new, , ME, "NULL");

        memcpy(ssid_new, cur_ssid, sizeof(wld_scanResultSSID_t));
        amxc_llist_it_init(&ssid_new->it);
        amxc_llist_append(&results->ssids, &ssid_new->it);
    }
}

//@deprecated. Please use wld_scan_cleanupScanResults
bool wld_radio_scanresults_cleanup(wld_scanResults_t* results) {
    SAH_TRACEZ_INFO(ME, "Cleanup scanresults");
    wld_scan_cleanupScanResults(results);
    return true;
}

bool wld_radio_scanresults_find(T_Radio* pR, const char* ssid, wld_scanResultSSID_t* output) {
    ASSERT_NOT_NULL(pR, -1, ME, "EP NULL");
    ASSERT_NOT_NULL(ssid, -1, ME, "EP NULL");

    wld_scanResults_t res;
    /*
     * over-size buffer assuming content is not printable (i.e fully displayed as hex string)
     */
    char ssidStr[(2 * SSID_NAME_LEN) + 1];
    memset(ssidStr, 0, sizeof(ssidStr));
    bool isEntryFound = false;

    amxc_llist_init(&res.ssids);

    /* get vendor specific scan results */
    if(pR->pFA->mfn_wrad_scan_results(pR, &res) < 0) {
        SAH_TRACEZ_ERROR(ME, "failed to get results");
        return false;
    }

    amxc_llist_it_t* it = amxc_llist_get_first(&res.ssids);
    while(it != NULL) {
        wld_scanResultSSID_t* SR_ssid = amxc_llist_it_get_data(it, wld_scanResultSSID_t, it);
        it = amxc_llist_it_get_next(it);

        convSsid2Str(SR_ssid->ssid, SR_ssid->ssidLen, ssidStr, sizeof(ssidStr));
        SAH_TRACEZ_INFO(ME, "searching for matching ssid : [%s] <-> [%s]", ssid, ssidStr);

        if(!strncmp(ssid, ssidStr, strlen(ssid))) {
            SAH_TRACEZ_INFO(ME, "Found matching ssid");
            memcpy(output, SR_ssid, sizeof(wld_scanResultSSID_t));
            isEntryFound = true;
            break;
        }
        amxc_llist_it_take(&SR_ssid->it);
    }

    wld_radio_scanresults_cleanup(&res);

    return isEntryFound;
}

wld_scanReasonStats_t* wld_getScanStatsByReason(T_Radio* pRad, const char* reason) {
    ASSERT_NOT_NULL(pRad, NULL, ME, "NULL");
    ASSERT_NOT_NULL(reason, NULL, ME, "NULL");

    amxc_llist_for_each(it, &(pRad->scanState.stats.extendedStat)) {
        wld_scanReasonStats_t* stats = amxc_llist_it_get_data(it, wld_scanReasonStats_t, it);
        if(swl_str_matches(stats->scanReason, reason)) {
            return stats;
        }
    }
    return NULL;
}

static wld_scanReasonStats_t* s_getOrCreateStats(T_Radio* pRad, const char* reason) {
    ASSERT_NOT_NULL(pRad, NULL, ME, "NULL");
    ASSERT_NOT_NULL(reason, NULL, ME, "NULL");
    wld_scanReasonStats_t* stats = wld_getScanStatsByReason(pRad, reason);
    if(stats == NULL) {
        amxc_llist_t* statList = &(pRad->scanState.stats.extendedStat);
        stats = calloc(1, sizeof(wld_scanReasonStats_t));
        ASSERT_NOT_NULL(stats, NULL, ME, "NULL");

        swl_str_copyMalloc(&(stats->scanReason), reason);
        SAH_TRACEZ_INFO(ME, "ScanReason %s added", stats->scanReason);
        amxc_llist_append(statList, &stats->it);
        stats->object = s_scanStatsInitialize(pRad, reason);

    }
    return stats;
}

static bool s_isExternalScanFilter(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, false, ME, "NULL");
    bool isfilter = false;

    swl_rc_ne error = pRad->pFA->mfn_wrad_getscanfilterinfo(pRad, &isfilter);

    return (error == SWL_RC_OK) ? isfilter : false;
}

static void s_notifyStartScan(T_Radio* pRad, wld_scan_type_e type, const char* scanReason, swl_rc_ne resultCode) {

    if(scanReason == NULL) {
        scanReason = "Unknown";
    }

    SAH_TRACEZ_INFO(ME, "%s: starting scan %s", pRad->Name, scanReason);

    pRad->scanState.stats.nrScanRequested++;
    wld_scanReasonStats_t* stats = s_getOrCreateStats(pRad, scanReason);
    ASSERTS_NOT_NULL(stats, , ME, "NULL");
    stats->totalCount++;

    if((resultCode == SWL_RC_OK) || (resultCode == SWL_RC_CONTINUE)) {
        wld_scanEvent_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.pRad = pRad;
        ev.start = true;
        ev.scanType = type;
        ev.scanReason = scanReason;
        wld_event_trigger_callback(gWld_queue_rad_onScan_change, &ev);

        pRad->scanState.scanType = type;
        pRad->scanState.lastScanTime = swl_time_getMonoSec();

        swl_str_copy(pRad->scanState.scanReason, sizeof(pRad->scanState.scanReason), scanReason);
        swla_delayExec_add((swla_delayExecFun_cbf) s_updateScanProcState, pRad);
    } else {
        pRad->scanState.stats.nrScanError++;
        stats->errorCount++;
    }

    swla_delayExec_add((swla_delayExecFun_cbf) s_updateScanStats, pRad);
}

static bool s_isScanRequestReady(T_Radio* pR) {
    if(wld_scan_isRunning(pR)) {
        SAH_TRACEZ_ERROR(ME, "%s: scan is already running", pR->Name);
        return false;
    }

    if(!wld_rad_isUpAndReady(pR)) {
        SAH_TRACEZ_ERROR(ME, "%s: rad not ready for scan (state:%s)",
                         pR->Name, cstr_chanmgt_rad_state[pR->detailedState]);
        return false;
    }
    return true;
}

static amxd_status_t s_startScan(amxd_object_t* object,
                                 amxd_function_t* func _UNUSED,
                                 amxc_var_t* args,
                                 amxc_var_t* ret _UNUSED,
                                 wld_scan_type_e scanType) {
    T_Radio* pR = object->priv;
    ASSERT_NOT_NULL(pR, amxd_status_unknown_error, ME, "NULL");
    amxd_status_t errorCode = amxd_status_unknown_error;

    if(!s_isScanRequestReady(pR)) {
        SAH_TRACEZ_ERROR(ME, "%s: not ready for scan", pR->Name);
        goto error;
    }

    const char* ssid = GET_CHAR(args, "SSID");
    const char* bssid = GET_CHAR(args, "BSSID");
    const char* channels = GET_CHAR(args, "channels");
    const char* scanReason = GET_CHAR(args, "scanReason");

    wld_scanArgs_t* scanArgs = &pR->scanState.cfg.scanArguments;
    size_t len = 0;

    memset(scanArgs, 0, sizeof(wld_scanArgs_t));

    scanArgs->enableFlush = true;

    if(channels != NULL) {
        // chanlist must have at most nrPossible elements,
        // otherwise there will be elements that are not part of possible channels.
        len = swl_type_arrayFromChar(swl_type_uint8, scanArgs->chanlist, WLD_MAX_POSSIBLE_CHANNELS, channels);
        if(!wld_rad_isChannelSubset(pR, scanArgs->chanlist, len) || (len == 0)) {
            SAH_TRACEZ_ERROR(ME, "%s: Invalid Channel list (%s)", pR->Name, channels);
            errorCode = amxd_status_invalid_function_argument;
            goto error;
        }
        scanArgs->chanCount = len;
    }
    if(!scanArgs->chanCount) {
        scanArgs->chanCount = SWL_MIN(pR->nrPossibleChannels, WLD_MAX_POSSIBLE_CHANNELS);
        memcpy(scanArgs->chanlist, pR->possibleChannels, scanArgs->chanCount * sizeof(uint8_t));
        SAH_TRACEZ_INFO(ME, "%s: force scan only on %d possible channels", pR->Name, scanArgs->chanCount);
    }

    if(ssid != NULL) {
        len = strlen(ssid);
        if(len >= SSID_NAME_LEN) {
            SAH_TRACEZ_ERROR(ME, "SSID (%s) too large", ssid);
            errorCode = amxd_status_invalid_function_argument;
            goto error;
        }

        swl_str_copy(scanArgs->ssid, SSID_NAME_LEN, ssid);
        scanArgs->ssidLen = len;
    }
    if(!swl_str_isEmpty(bssid)) {
        swl_macChar_t macChar = SWL_MAC_CHAR_NEW();
        memcpy(macChar.cMac, bssid, sizeof(swl_macChar_t));
        if(!swl_mac_charToBin(&scanArgs->bssid, &macChar) || (!swl_mac_charIsValidStaMac(&macChar))) {
            SAH_TRACEZ_ERROR(ME, "Invalid bssid (%s)", bssid);
            errorCode = amxd_status_invalid_function_argument;
            goto error;

        }
    }

    bool updateUsageData = GET_BOOL(args, "updateUsage");
    scanArgs->updateUsageStats = (updateUsageData || (scanType == SCAN_TYPE_COMBINED));

    const char* reason = scanReason ? : "ScanCall";
    bool hasForceFast = GET_BOOL(args, "forceFast");
    if(!hasForceFast) {
        scanArgs->fastScan = (swl_str_find(pR->scanState.cfg.fastScanReasons, reason) > 0);
    }

    snprintf(scanArgs->reason, sizeof(scanArgs->reason), "%s", reason);

    swl_rc_ne scanResult = wld_scan_start(pR, scanType, reason);
    if(scanResult < SWL_RC_OK) {
        errorCode = amxd_status_unknown_error;
        goto error;
    }

    SAH_TRACEZ_INFO(ME, "%s: starting scan %u: %s", pR->Name, scanType, reason);

    /* Ok */
    return amxd_status_deferred;

error:
    SAH_TRACEZ_ERROR(ME, "%s: error in starting scan", pR->Name);
    return errorCode;
}

amxd_status_t _stopScan(amxd_object_t* object,
                        amxd_function_t* func _UNUSED,
                        amxc_var_t* args _UNUSED,
                        amxc_var_t* ret _UNUSED) {
    T_Radio* pR = object->priv;
    ASSERT_NOT_NULL(pR, amxd_status_unknown_error, ME, "NULL");

    swl_rc_ne rc = wld_scan_stop(pR);
    if(rc == SWL_RC_INVALID_STATE) {
        SAH_TRACEZ_ERROR(ME, "%s: no scan running", pR->Name);
        return amxd_status_invalid_action;
    }

    if(rc < SWL_RC_OK) {
        SAH_TRACEZ_ERROR(ME, "Unable to stop scan");
        return amxd_status_unknown_error;
    }

    return amxd_status_ok;
}

/*
   static void scan_request_cancel_handler(function_call_t* fcall, void* userdata _UNUSED) {

    SAH_TRACEZ_INFO(ME, "Scan call cancelled");
    T_Radio* pR = fcall_userData(fcall);
    ASSERT_NOT_NULL(pR, , ME, "NULL");

    pR->scanState.fcall = NULL;
    pR->scanState.minRssi = INT32_MIN;
   }
 */

static amxd_status_t s_scanRequest(T_Radio* pR,
                                   amxd_object_t* object,
                                   amxd_function_t* func,
                                   amxc_var_t* args,
                                   amxc_var_t* retval,
                                   wld_scan_type_e scanType) {
    ASSERT_TRUE(scanType < SCAN_TYPE_MAX, amxd_status_invalid_function_argument,
                ME, "Invalid scan type index %u", scanType);

    if(s_isExternalScanFilter(pR) == true) {
        return amxd_status_permission_denied;
    }

    /* start the scan */
    amxd_status_t res = s_startScan(object, func, args, retval, scanType);
    ASSERT_EQUALS(res, amxd_status_deferred, res, ME, "%s: scan failed", pR->Name);

    pR->scanState.minRssi = INT32_MIN;
    amxc_var_t* minRssiVar = GET_ARG(args, "minRssi");
    if(minRssiVar != NULL) {
        pR->scanState.minRssi = amxc_var_dyncast(int32_t, minRssiVar);
    }

    if(func != NULL) {
        amxd_status_t success = swl_function_defer(&pR->scanState.scanFunInfo, func, retval);
        if(success != amxd_status_deferred) {
            SAH_TRACEZ_ERROR(ME, "%s: failure to defer scan %u", pR->Name, success);
        }
        return success;
    } else {
        return amxd_status_deferred;
    }
}

amxd_status_t _scan(amxd_object_t* object,
                    amxd_function_t* func,
                    amxc_var_t* args,
                    amxc_var_t* retval) {
    T_Radio* pR = object->priv;
    ASSERTS_NOT_NULL(pR, amxd_status_unknown_error, ME, "NULL");

    return s_scanRequest(pR, object, func, args, retval, SCAN_TYPE_SSID);
}

amxd_status_t _scanCombinedData(amxd_object_t* object,
                                amxd_function_t* func,
                                amxc_var_t* args,
                                amxc_var_t* retval) {
    T_Radio* pR = object->priv;
    ASSERTS_NOT_NULL(pR, amxd_status_unknown_error, ME, "NULL");

    return s_scanRequest(pR, object, func, args, retval, SCAN_TYPE_COMBINED);
}

amxd_status_t _Radio_startScan(amxd_object_t* object,
                               amxd_function_t* func _UNUSED,
                               amxc_var_t* args,
                               amxc_var_t* retval) {
    T_Radio* pR = object->priv;
    ASSERTS_NOT_NULL(pR, amxd_status_unknown_error, ME, "NULL");

    amxd_status_t res = s_scanRequest(pR, object, NULL, args, retval, SCAN_TYPE_SSID);
    /* return OK when deferred */
    return (res == amxd_status_deferred) ? amxd_status_ok : res;
}

/**
 * Filters out the scan Results if the scan was run with a matching filter
 *
 * Returns true if the result shall be filtered out.
 */
static bool s_filterScanResultsBasedOnArgs(T_Radio* pRad, wld_scanResultSSID_t* pResult) {
    wld_scanArgs_t* pScanArgs = &pRad->scanState.cfg.scanArguments;
    ASSERT_NOT_NULL(pResult, false, ME, "NULL");
    ASSERT_NOT_NULL(pScanArgs, false, ME, "NULL");

    if(pScanArgs->ssidLen > 0) {
        char ssidStr[SSID_NAME_LEN];
        memset(ssidStr, 0, sizeof(ssidStr));
        convSsid2Str(pResult->ssid, pResult->ssidLen, ssidStr, sizeof(ssidStr));
        if(swl_str_matches(ssidStr, pScanArgs->ssid) == false) {
            SAH_TRACEZ_INFO(ME, "Received scan result with SSID %s while the scan was started with param SSID %s", pScanArgs->ssid, ssidStr);
            return true;
        }
    }

    if(swl_mac_binIsNull(&pScanArgs->bssid) == false) {
        if(swl_mac_binMatches(&pResult->bssid, &pScanArgs->bssid) == false) {
            SAH_TRACEZ_INFO(ME, "Received scan result with BSSID "SWL_MAC_FMT "  while the scan was started with param BSSID "SWL_MAC_FMT "", SWL_MAC_ARG(pResult->bssid.bMac), SWL_MAC_ARG(pScanArgs->bssid.bMac));
            return true;

        }
    }

    if(pScanArgs->chanCount > 0) {
        if(swl_type_arrayContains(swl_type_uint8, pScanArgs->chanlist, pScanArgs->chanCount, &pResult->channel) == false) {
            SAH_TRACEZ_INFO(ME, "Received scan result with channel %d which is not part of the channels scan params", pResult->channel);
            return true;
        }
    }
    return false;
}



static bool s_getScanResults(T_Radio* pR, amxc_var_t* retval, int32_t minRssi) {
    wld_scanResults_t res;

    amxc_llist_init(&res.ssids);

    if(pR->pFA->mfn_wrad_scan_results(pR, &res) < 0) {
        SAH_TRACEZ_ERROR(ME, "Unable to retrieve scan results");
        return false;
    }

    amxc_var_set_type(retval, AMXC_VAR_ID_LIST);

    amxc_llist_for_each(it, &res.ssids) {
        wld_scanResultSSID_t* ssid = amxc_llist_it_get_data(it, wld_scanResultSSID_t, it);

        // Scan filter is normally handled by the vendor, but sometimes, for divers reasons, the filtering might fail.
        // So let's double-chek it
        if(s_filterScanResultsBasedOnArgs(pR, ssid) == true) {
            continue;
        }

        if(ssid->rssi < minRssi) {
            continue;
        }

        amxc_var_t* pEntry = amxc_var_add(amxc_htable_t, retval, NULL);
        if(pEntry == NULL) {
            break;
        }

        swl_macChar_t bssidStr = SWL_MAC_CHAR_NEW();
        swl_mac_binToChar(&bssidStr, &ssid->bssid);

        char* ssidStr = wld_ssid_to_string(ssid->ssid, ssid->ssidLen);
        amxc_var_add_key(cstring_t, pEntry, "SSID", ssidStr);
        free(ssidStr);

        amxc_var_add_key(cstring_t, pEntry, "BSSID", bssidStr.cMac);
        amxc_var_add_key(int32_t, pEntry, "SignalNoiseRatio", ssid->snr);
        amxc_var_add_key(int32_t, pEntry, "Noise", ssid->noise);
        amxc_var_add_key(int32_t, pEntry, "RSSI", ssid->rssi);
        amxc_var_add_key(int32_t, pEntry, "Channel", ssid->channel);
        amxc_var_add_key(int32_t, pEntry, "CentreChannel", ssid->centreChannel);
        amxc_var_add_key(int32_t, pEntry, "Bandwidth", ssid->bandwidth);
        amxc_var_add_key(int32_t, pEntry, "SignalStrength", ssid->rssi);
        if((ssid->channelUtilization != 0) || (ssid->stationCount != 0)) {
            amxc_var_add_key(uint8_t, pEntry, "ChannelUtilization", ssid->channelUtilization);
            amxc_var_add_key(uint16_t, pEntry, "StationCount", ssid->stationCount);
        }

        amxc_var_add_key(cstring_t, pEntry, "SecurityModeEnabled", swl_security_apModeToString(ssid->secModeEnabled, SWL_SECURITY_APMODEFMT_LEGACY));

        char wpsmethods[256] = {0};
        swl_wps_configMethodMaskToNames(wpsmethods, SWL_ARRAY_SIZE(wpsmethods), ssid->WPS_ConfigMethodsEnabled);
        amxc_var_add_key(cstring_t, pEntry, "WPSConfigMethodsSupported", wpsmethods);

        amxc_var_add_key(cstring_t, pEntry, "EncryptionMode", swl_security_encMode_str[ssid->encryptionMode]);

        char operatingStandardsChar[32] = "";
        swl_radStd_toChar(operatingStandardsChar, sizeof(operatingStandardsChar),
                          ssid->operatingStandards, SWL_RADSTD_FORMAT_STANDARD, 0);
        amxc_var_add_key(cstring_t, pEntry, "OperatingStandards", operatingStandardsChar);
    }

    wld_scan_cleanupScanResults(&res);

    return true;
}

static void s_prepareSpectrumOutputWithChanFilter(T_Radio* pR, amxc_var_t* pOutVar, uint8_t* filterChanList, size_t nFilterChans) {
    ASSERT_NOT_NULL(pR, , ME, "NULL");
    ASSERT_NOT_NULL(pOutVar, , ME, "NULL");

    amxc_llist_t* results = &pR->scanState.spectrumResults;

    amxc_var_set_type(pOutVar, AMXC_VAR_ID_LIST);
    amxc_llist_for_each(it, results) {
        wld_spectrumChannelInfoEntry_t* llEntry = amxc_llist_it_get_data(it, wld_spectrumChannelInfoEntry_t, it);
        if((nFilterChans > 0) && (!swl_typeUInt8_arrayContains(filterChanList, nFilterChans, llEntry->channel))) {
            continue;
        }
        amxc_var_t* varEntry = amxc_var_add(amxc_htable_t, pOutVar, NULL);
        amxc_var_add_key(uint32_t, varEntry, "channel", llEntry->channel);
        amxc_var_add_key(cstring_t, varEntry, "bandwidth", swl_bandwidth_str[llEntry->bandwidth]);
        amxc_var_add_key(uint32_t, varEntry, "availability", llEntry->availability);
        amxc_var_add_key(uint32_t, varEntry, "ourUsage", llEntry->ourUsage);
        amxc_var_add_key(int32_t, varEntry, "noiselevel", llEntry->noiselevel);
        amxc_var_add_key(uint32_t, varEntry, "accesspoints", llEntry->nrCoChannelAP);
    }
}

static bool s_getChanSurveyReport(T_Radio* pR, amxc_var_t* retval) {
    ASSERTI_NOT_NULL(pR, false, ME, "rad null");
    ASSERTI_NOT_NULL(retval, false, ME, "retval null");

    amxc_llist_t* results = &pR->scanState.lastSurveyReport.surveyReport;

    amxc_var_set_type(retval, AMXC_VAR_ID_LIST);
    amxc_llist_for_each(it, results) {
        wld_chanSurveyReportEntry_t* report = amxc_llist_it_get_data(it, wld_chanSurveyReportEntry_t, it);

        amxc_var_t* pEntry = amxc_var_add(amxc_htable_t, retval, NULL);
        if(pEntry == NULL) {
            return false;
        }

        swl_chanspec_t chanSpec = SWL_CHANSPEC_EMPTY;
        swl_chanspec_channelFromMHz(&chanSpec, report->frequencyMHz);
        amxc_var_add_key(int32_t, pEntry, "channel", chanSpec.channel);
        amxc_var_add_key(int32_t, pEntry, "interferenceFactor", report->interferenceFactor);
        SAH_TRACEZ_INFO(ME, "chanSurveyReport frequencyMHz %d channel %d interferenceFactor %d", report->frequencyMHz, chanSpec.channel, report->interferenceFactor);
    }
    return true;
}

static void s_prepareSpectrumOutput(T_Radio* pR, amxc_var_t* pOutVar) {
    return s_prepareSpectrumOutputWithChanFilter(pR, pOutVar, NULL, 0);
}

static bool s_getLatestSpectrumInfo(T_Radio* pRad, amxc_var_t* map) {
    s_prepareSpectrumOutput(pRad, map);
    return true;
}

static bool s_getAllScanMeasurements(T_Radio* pR, amxc_var_t* retval, int32_t minRssi) {
    ASSERTI_NOT_NULL(pR, false, ME, "rad null");
    ASSERTI_NOT_NULL(retval, false, ME, "retval null");
    amxc_var_set_type(retval, AMXC_VAR_ID_HTABLE);
    amxc_var_t* varScanResults = amxc_var_add_key(amxc_htable_t, retval, "BSS", NULL);
    s_getScanResults(pR, varScanResults, minRssi);
    amxc_var_t* varSpectrum = amxc_var_add_key(amxc_htable_t, retval, "Spectrum", NULL);
    s_getLatestSpectrumInfo(pR, varSpectrum);
    return true;
}

amxd_status_t _getScanResults(amxd_object_t* object,
                              amxd_function_t* func _UNUSED,
                              amxc_var_t* args,
                              amxc_var_t* retval) {

    T_Radio* pR = object->priv;

    ASSERT_NOT_NULL(pR, amxd_status_unknown_error, ME, "NULL");

    int32_t minRssi = INT32_MIN;
    amxc_var_t* minRssiVar = GET_ARG(args, "minRssi");
    if(minRssiVar != NULL) {
        minRssi = amxc_var_dyncast(int32_t, minRssiVar);
    }

    if(!s_getScanResults(pR, retval, minRssi)) {
        return amxd_status_unknown_error;
    }

    return amxd_status_ok;
}

amxd_status_t _getScanCombinedData(amxd_object_t* object,
                                   amxd_function_t* func _UNUSED,
                                   amxc_var_t* args,
                                   amxc_var_t* retval) {

    T_Radio* pR = wld_rad_fromObj(object);
    ASSERT_NOT_NULL(pR, amxd_status_unknown_error, ME, "rad NULL");

    int32_t minRssi = INT32_MIN;
    amxc_var_t* minRssiVar = GET_ARG(args, "minRssi");
    if(minRssiVar != NULL) {
        minRssi = amxc_var_dyncast(int32_t, minRssiVar);
    }

    amxc_var_set_type(retval, AMXC_VAR_ID_HTABLE);
    amxc_var_t* varScanResults = amxc_var_add_key(amxc_htable_t, retval, "BSS", NULL);
    s_getScanResults(pR, varScanResults, minRssi);

    wld_scanArgs_t* pScanArgs = &pR->scanState.cfg.scanArguments;
    amxc_var_t* varSpectrum = amxc_var_add_key(amxc_htable_t, retval, "Spectrum", NULL);
    s_prepareSpectrumOutputWithChanFilter(pR, varSpectrum, pScanArgs->chanlist, pScanArgs->chanCount);

    return amxd_status_ok;
}

amxd_status_t _getChanSurveyReport(amxd_object_t* object,
                                   amxd_function_t* func _UNUSED,
                                   amxc_var_t* args _UNUSED,
                                   amxc_var_t* retval) {
    T_Radio* pR = wld_rad_fromObj(object);
    ASSERT_NOT_NULL(pR, amxd_status_unknown_error, ME, "NULL");

    if(!s_getChanSurveyReport(pR, retval)) {
        return amxd_status_unknown_error;
    }
    return amxd_status_ok;
}

/**
 * Send ScanComplete notification which includes the Result("done" OR "error") to all subscribers
 **/
static void wld_rad_scan_SendDoneNotification(T_Radio* pR, bool success) {
    ASSERT_NOT_NULL(pR, , ME, "NULL");

    char* path = amxd_object_get_path(pR->pBus, AMXD_OBJECT_NAMED);
    amxc_var_t ret;
    amxc_var_init(&ret);
    amxc_var_set_type(&ret, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &ret, "Result", success ? "done" : "error");
    SAH_TRACEZ_INFO(ME, "%s: notif scan done @ path=%s %s, Result=%s}", pR->Name, path, "ScanComplete", success ? "done" : "error");
    amxd_object_trigger_signal(pR->pBus, "ScanComplete", &ret);
    amxc_var_clean(&ret);
    free(path);
}

void fillFullScanResultsList(amxc_var_t* pScanResultList, T_Radio* pRad, wld_scanResults_t* pScanResults) {
    ASSERTS_NOT_NULL(pScanResultList, , ME, "pScanResultList NULL");
    ASSERTS_NOT_NULL(pScanResults, , ME, "pScanResults NULL");
    ASSERTS_NOT_NULL(pRad, , ME, "pScanArgs NULL");

    wld_scanArgs_t* pScanArgs = &pRad->scanState.cfg.scanArguments;
    ASSERT_NOT_NULL(pScanArgs, , ME, "NULL");

    char* filterSSid = pScanArgs->ssidLen > 0 ? pScanArgs->ssid : NULL;

    typedef struct {
        amxc_llist_it_t it;
        uint8_t operatingClass;
        amxc_llist_t channelScanList;
    } op_class_scan_t;

    typedef struct {
        amxc_llist_it_t channelScanIt;
        uint8_t channel;
        swl_timeMono_t timeStamp;
        uint8_t utilization;
        uint8_t noise;
        amxc_llist_t neighborBssList;
    } channel_scan_t;

    typedef struct {
        amxc_llist_it_t neighborBssIt;
        swl_macBin_t bssid;
        char ssid[SSID_NAME_LEN];
        uint8_t signalStrength;
        int32_t channelBandwidth;
        uint8_t channelUtilization;
        uint StationCount;
        swl_security_apMode_e securityModeEnabled;
        swl_security_encMode_e encryptionMode;
        swl_radioStandard_m supportedStandards;
        swl_radioStandard_m operatingStandards;
        cstring_t basicDataTransferRates;
        cstring_t supportedDataTransferRates;
        uint supportedNss;
        uint dtimPeriod;
        uint beaconPeriod;
    } neighbor_bss_t;

    amxc_llist_t operatingClassList;
    amxc_llist_init(&operatingClassList);

    amxc_llist_for_each(it, &pScanResults->ssids) {
        wld_scanResultSSID_t* pSsid = amxc_container_of(it, wld_scanResultSSID_t, it);
        if(filterSSid != NULL) {
            char ssidStr[SSID_NAME_LEN];
            memset(ssidStr, 0, sizeof(ssidStr));
            convSsid2Str(pSsid->ssid, pSsid->ssidLen, ssidStr, sizeof(ssidStr));
            if(swl_str_matches(ssidStr, pScanArgs->ssid) == false) {
                SAH_TRACEZ_INFO(ME, "Received scan result with SSID %s while the scan was started with param SSID %s", ssidStr, pScanArgs->ssid);
                amxc_llist_it_take(&pSsid->it);
                free(pSsid);
                continue;
            }
        }

        op_class_scan_t* pOpClassScan = NULL;
        amxc_llist_for_each(it, &operatingClassList) {
            op_class_scan_t* pData = amxc_llist_it_get_data(it, op_class_scan_t, it);
            if(pData->operatingClass == pSsid->operClass) {
                pOpClassScan = pData;
                break;
            }
        }
        // no OpClassScan object --> create it
        if(pOpClassScan == NULL) {
            pOpClassScan = calloc(1, sizeof(op_class_scan_t));
            if(pOpClassScan == NULL) {
                SAH_TRACEZ_ERROR(ME, "fail to alloc sop_class_scan_t");
                return;
            }

            pOpClassScan->operatingClass = pSsid->operClass;

            amxc_llist_it_init(&pOpClassScan->it);
            amxc_llist_init(&pOpClassScan->channelScanList);
            amxc_llist_append(&operatingClassList, &pOpClassScan->it);
        }

        channel_scan_t* pChannelScan = NULL;
        amxc_llist_for_each(it, &pOpClassScan->channelScanList) {
            channel_scan_t* pData = amxc_llist_it_get_data(it, channel_scan_t, channelScanIt);
            if(pData->channel == pSsid->channel) {
                pChannelScan = pData;
                break;
            }
        }
        if(pChannelScan == NULL) {
            pChannelScan = calloc(1, sizeof(channel_scan_t));
            if(pChannelScan == NULL) {
                SAH_TRACEZ_ERROR(ME, "fail to alloc channel_scan_t");
                return;
            }
            pChannelScan->channel = pSsid->channel;
            swl_timeMono_t now = swl_time_getMonoSec();
            pChannelScan->timeStamp = now;
            pChannelScan->utilization = 0;  //TODO: from where to get it ?
            pChannelScan->noise = pSsid->noise;
            amxc_llist_it_init(&pChannelScan->channelScanIt);
            amxc_llist_init(&pChannelScan->neighborBssList);
            amxc_llist_append(&pOpClassScan->channelScanList, &pChannelScan->channelScanIt);
        }

        neighbor_bss_t* pNeighborBss = NULL;
        amxc_llist_for_each(it, &pChannelScan->neighborBssList) {
            neighbor_bss_t* pData = amxc_llist_it_get_data(it, neighbor_bss_t, neighborBssIt);
            if(swl_mac_binMatches(&pData->bssid, &pSsid->bssid)) {
                pNeighborBss = pData;
                break;
            }
        }
        // no NeighborBSS object --> create it
        if(pNeighborBss == NULL) {
            pNeighborBss = calloc(1, sizeof(neighbor_bss_t));
            if(pNeighborBss == NULL) {
                SAH_TRACEZ_ERROR(ME, "fail to alloc neighbor_bss_t");
                return;
            }

            pNeighborBss->bssid = pSsid->bssid;
            memset(pNeighborBss->ssid, 0, sizeof(pNeighborBss->ssid));
            convSsid2Str(pSsid->ssid, pSsid->ssidLen, pNeighborBss->ssid, sizeof(pNeighborBss->ssid));
            pNeighborBss->signalStrength = pSsid->rssi;
            pNeighborBss->channelBandwidth = pSsid->bandwidth;
            pNeighborBss->channelUtilization = 0;
            pNeighborBss->StationCount = 0;      // will be ftreated in  SSW-8679
            pNeighborBss->securityModeEnabled = pSsid->secModeEnabled;
            pNeighborBss->encryptionMode = pSsid->encryptionMode;

            pNeighborBss->supportedStandards = pSsid->operatingStandards; // will be ftreated in  SSW-8679
            pNeighborBss->operatingStandards = pSsid->operatingStandards;

            pNeighborBss->basicDataTransferRates = NULL;
            pNeighborBss->supportedDataTransferRates = NULL;
            pNeighborBss->supportedNss = 0; // will be ftreated in  SSW-8679
            pNeighborBss->dtimPeriod = 0;   // will be ftreated in  SSW-8679
            pNeighborBss->beaconPeriod = 0; // will be ftreated in  SSW-8679

            amxc_llist_it_init(&pNeighborBss->neighborBssIt);
            amxc_llist_append(&pChannelScan->neighborBssList, &pNeighborBss->neighborBssIt);
        }
    }

    //Fill the amxrt response
    swl_timeReal_t now = swl_time_getRealSec();
    char dateBuffer[64] = {'\0'};
    swl_typeTimeReal_toChar(dateBuffer, sizeof(dateBuffer), now);
    amxc_var_t* pScanResultObject = amxc_var_add(amxc_htable_t, pScanResultList, NULL);

    amxc_var_add_key(cstring_t, pScanResultObject, "TimeStamp", dateBuffer);

    amxc_var_t* opClassScanRoot = amxc_var_add_key(amxc_llist_t, pScanResultObject, "OpClassScan", NULL);

    amxc_llist_for_each(operatingClass_it, &operatingClassList) {
        op_class_scan_t* pOperatingClass = amxc_llist_it_get_data(operatingClass_it, op_class_scan_t, it);
        amxc_var_t* opClassScanObject = amxc_var_add(amxc_htable_t, opClassScanRoot, NULL);
        amxc_var_add_key(uint8_t, opClassScanObject, "OperatingClass", pOperatingClass->operatingClass);
        if(amxc_llist_size(&pOperatingClass->channelScanList) == 0) {
            continue;
        }
        amxc_var_t* pChannelScanList = amxc_var_add_key(amxc_llist_t, opClassScanObject, "ChannelScan", NULL);
        amxc_llist_for_each(it, &pOperatingClass->channelScanList) {
            channel_scan_t* pChannelScan = amxc_llist_it_get_data(it, channel_scan_t, channelScanIt);
            amxc_var_t* pChannelScanHashTable = amxc_var_add(amxc_htable_t, pChannelScanList, NULL);
            amxc_var_add_key(uint8_t, pChannelScanHashTable, "Channel", pChannelScan->channel);
            amxc_var_add_key(cstring_t, pChannelScanHashTable, "TimeStamp", dateBuffer);
            amxc_var_add_key(uint8_t, pChannelScanHashTable, "Utilization", pChannelScan->utilization);
            amxc_var_add_key(uint8_t, pChannelScanHashTable, "Noise", pChannelScan->noise);

            if(amxc_llist_size(&pChannelScan->neighborBssList) == 0) {
                continue;
            }
            amxc_var_t* pNeighborBssList = amxc_var_add_key(amxc_llist_t, pChannelScanHashTable, "NeighborBSS", NULL);
            amxc_llist_for_each(it, &pChannelScan->neighborBssList) {
                amxc_var_t* pNeighborBssHashTable = amxc_var_add(amxc_htable_t, pNeighborBssList, NULL);
                neighbor_bss_t* pNeighborBss = amxc_llist_it_get_data(it, neighbor_bss_t, neighborBssIt);
                amxc_var_add_key(cstring_t, pNeighborBssHashTable, "SSID", pNeighborBss->ssid);
                swl_macChar_t macStr;
                swl_mac_binToChar(&macStr, &pNeighborBss->bssid);
                amxc_var_add_key(cstring_t, pNeighborBssHashTable, "BSSID", macStr.cMac);
                amxc_var_add_key(cstring_t, pNeighborBssHashTable, "TimeStamp", dateBuffer);
                amxc_var_add_key(uint8_t, pNeighborBssHashTable, "SignalStrength", pNeighborBss->signalStrength);
                amxc_var_add_key(cstring_t, pNeighborBssHashTable, "ChannelBandwidth", Rad_SupBW[swl_chanspec_intToBw(pNeighborBss->channelBandwidth)]);
                amxc_var_add_key(uint8_t, pNeighborBssHashTable, "ChannelUtilization", pNeighborBss->channelUtilization);
                amxc_var_add_key(uint32_t, pNeighborBssHashTable, "StationCount", pNeighborBss->StationCount);  // pwhm does not receive StationCount as this stage
                amxc_var_add_key(cstring_t, pNeighborBssHashTable, "SecurityModeEnabled", swl_security_apModeToString(pNeighborBss->securityModeEnabled, SWL_SECURITY_APMODEFMT_LEGACY));
                amxc_var_add_key(cstring_t, pNeighborBssHashTable, "EncryptionMode", swl_security_encMode_str[ pNeighborBss->encryptionMode ]);

                char operatingStandardsChar[32] = "";
                swl_radStd_toChar(operatingStandardsChar, sizeof(operatingStandardsChar),
                                  pNeighborBss->operatingStandards, SWL_RADSTD_FORMAT_STANDARD, 0);
                amxc_var_add_key(cstring_t, pNeighborBssHashTable, "OperatingStandards", operatingStandardsChar);

                amxc_var_add_key(cstring_t, pNeighborBssHashTable, "SupportedStandards", operatingStandardsChar);

                amxc_var_add_key(cstring_t, pNeighborBssHashTable, "BasicDataTransferRates", "");
                amxc_var_add_key(cstring_t, pNeighborBssHashTable, "SupportedDataTransferRates", "");

                amxc_var_add_key(uint32_t, pNeighborBssHashTable, "SupportedNSS", pNeighborBss->supportedNss);
                amxc_var_add_key(uint32_t, pNeighborBssHashTable, "DTIMPeriod", pNeighborBss->dtimPeriod);
                amxc_var_add_key(uint32_t, pNeighborBssHashTable, "BeaconPeriod", pNeighborBss->beaconPeriod);

                amxc_llist_it_take(&pNeighborBss->neighborBssIt);
                free(pNeighborBss);

            }
            amxc_llist_it_take(&pChannelScan->channelScanIt);
            free(pChannelScan);
        }
        amxc_llist_it_take(&pOperatingClass->it);
        free(pOperatingClass);
    }
    amxc_llist_clean(&operatingClassList, NULL);

}

static void s_sendFullScanResult(T_Radio* pRadio, bool success) {
    if(success) {
        amxc_var_t retMap;
        amxc_var_init(&retMap);
        amxc_var_set_type(&retMap, AMXC_VAR_ID_HTABLE);

        amxc_var_t* pScanResultList = amxc_var_add_key(amxc_llist_t, &retMap, "ScanResult", NULL);

        wld_scanResults_t scanRes;
        amxc_llist_init(&scanRes.ssids);

        // Get data
        if(pRadio->pFA->mfn_wrad_scan_results(pRadio, &scanRes) >= SWL_RC_OK) {
            fillFullScanResultsList(pScanResultList, pRadio, &scanRes);
        } else {
            SAH_TRACEZ_ERROR(ME, "%s: Unable to retrieve scan results", pRadio->Name);
        }
        wld_scan_cleanupScanResults(&scanRes);
        swl_function_deferDone(&pRadio->scanState.scanFunInfo, amxd_status_ok, NULL, &retMap);
        amxc_var_clean(&retMap);
    } else {
        swl_function_deferDone(&pRadio->scanState.scanFunInfo, amxd_status_unknown_error, NULL, NULL);
    }
}

/**
 * Send scan results back to the caller and reset the scanState call_id.
 **/
static void wld_rad_scan_FinishScanCall(T_Radio* pR, bool* success) {
    ASSERT_NOT_NULL(pR, , ME, "NULL");
    ASSERT_TRUE(swl_function_deferIsActive(&pR->scanState.scanFunInfo), , ME, "%s no call_id", pR->Name);
    ASSERT_TRUE(wld_scan_isRunning(pR), , ME, "%s no scan", pR->Name);

    if(swl_str_matches(pR->scanState.scanReason, FULL_SCAN_REASON)) {
        s_sendFullScanResult(pR, *success);

    } else {
        amxc_var_t ret;
        amxc_var_init(&ret);

        switch(pR->scanState.scanType) {
        case SCAN_TYPE_SSID:
            if(!s_getScanResults(pR, &ret, pR->scanState.minRssi)) {
                *success = false;
            }
            break;
        case SCAN_TYPE_SPECTRUM:
            if(*success) {
                s_prepareSpectrumOutput(pR, &ret);
            }
            break;
        case SCAN_TYPE_COMBINED:
            s_getAllScanMeasurements(pR, &ret, pR->scanState.minRssi);
            break;
        default:
            SAH_TRACEZ_ERROR(ME, "%s scan type unknown %s", pR->Name, pR->scanState.scanReason);
            *success = false;
        }

        swl_function_deferDone(&pR->scanState.scanFunInfo, success ? amxd_status_ok : amxd_status_unknown_error, NULL, &ret);

        amxc_var_clean(&ret);
    }
    pR->scanState.minRssi = INT32_MIN;
}

/**
 * Look for a channel object within the ScanResults object.
 * If the requested channel object does not exist, a new channel object is created.
 * Returns null if the channel is 0.
 *
 * @param obj_scan
 *  the object of the scan results of the given radio
 * @param channel
 *  the channel for which we want the channel object
 * @param pChanObj
 *  the channel for which we want the channel object
 * @return
 *  if the channel object existed in the data model, we return that channel
 *  if the channel is zero, we return null
 *  otherwise we create a new channel object.
 */
static swl_rc_ne s_getOrAddChannelObj(amxd_object_t* objScan, uint16_t channel, amxd_object_t** pChanObj) {
    W_SWL_SETPTR(pChanObj, NULL);
    ASSERT_NOT_NULL(objScan, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_EQUALS(channel, 0, SWL_RC_INVALID_PARAM, ME, "null channel");

    amxd_object_t* chanTemplate = amxd_object_get(objScan, "SurroundingChannels");
    ASSERT_NOT_NULL(chanTemplate, SWL_RC_ERROR, ME, "Not Channel templ obj");
    amxd_object_t* chanObj = NULL;
    amxd_object_for_each(instance, it, chanTemplate) {
        chanObj = amxc_llist_it_get_data(it, amxd_object_t, it);
        if(amxd_object_get_uint16_t(chanObj, "Channel", NULL) == channel) {
            W_SWL_SETPTR(pChanObj, chanObj);
            return SWL_RC_OK;
        }
    }

    amxd_trans_t trans;
    ASSERT_TRANSACTION_INIT(chanTemplate, &trans, SWL_RC_ERROR, ME, "Fail to prepare scan channel %d trans", channel);

    amxd_object_t* lastInst = amxc_container_of(amxc_llist_get_last(&chanTemplate->instances), amxd_object_t, it);
    uint32_t newIdx = amxd_object_get_index(lastInst) + 1;
    amxd_trans_add_inst(&trans, newIdx, NULL);
    amxd_trans_set_value(uint16_t, &trans, "Channel", channel);

    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, SWL_RC_ERROR, ME, "Fail to create scan channel obj[%d] ch:%d", newIdx, channel);
    chanObj = amxd_object_get_instance(chanTemplate, NULL, newIdx);
    ASSERT_NOT_NULL(chanObj, SWL_RC_ERROR, ME, "fail to locate new channel obj[%d] ch:%d", newIdx, channel);
    W_SWL_SETPTR(pChanObj, chanObj);

    return SWL_RC_OK;
}

/**
 * Trigger an internal only scan to update chanim info.
 */
swl_rc_ne wld_scan_updateChanimInfo(T_Radio* pRad) {

    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");

    swl_rc_ne retVal = pRad->pFA->mfn_wrad_update_chaninfo(pRad);
    s_notifyStartScan(pRad, SCAN_TYPE_INTERNAL, "ChanimInfo", retVal);
    return retVal;
}

static void s_delSpectrumResultEntry(amxc_llist_it_t* it) {
    wld_spectrumChannelInfoEntry_t* llEntry = amxc_llist_it_get_data(it, wld_spectrumChannelInfoEntry_t, it);
    free(llEntry);
}

void wld_spectrum_cleanupResults(T_Radio* pR) {
    ASSERT_NOT_NULL(pR, , ME, "NULL");
    amxc_llist_t* results = &pR->scanState.spectrumResults;
    amxc_llist_clean(results, s_delSpectrumResultEntry);
}

amxd_status_t _getSpectrumInfo(amxd_object_t* object,
                               amxd_function_t* func _UNUSED,
                               amxc_var_t* args,
                               amxc_var_t* retval) {
    SAH_TRACEZ_IN(ME);
    T_Radio* pR = object->priv;
    ASSERT_NOT_NULL(pR, amxd_status_unknown_error, ME, "NULL");

    bool update = GET_BOOL(args, "update");

    if(wld_scan_isRunning(pR)) {
        SAH_TRACEZ_ERROR(ME, "%s: scan is already running", pR->Name);
        return amxd_status_invalid_action;
    }

    /* clean last spectrum result before start new one */
    wld_spectrum_cleanupResults(pR);

    amxc_llist_t* results = &pR->scanState.spectrumResults;

    swl_rc_ne retCode = pR->pFA->mfn_wrad_getspectruminfo(pR, update, results);
    if(retCode == SWL_RC_NOT_IMPLEMENTED) {
        SAH_TRACEZ_OUT(ME);
        return amxd_status_function_not_implemented;
    }

    /* asynchronous call */
    if(retCode == SWL_RC_CONTINUE) {
        s_notifyStartScan(pR, SCAN_TYPE_SPECTRUM, "getSpectrumInfo", retCode);
        swl_function_defer(&pR->scanState.scanFunInfo, func, retval);
        SAH_TRACEZ_OUT(ME);
        return amxd_status_deferred;
    }

    /* convert the amxc_llist_t to variant list for RPC return value (if any) */
    s_prepareSpectrumOutput(pR, retval);

    SAH_TRACEZ_OUT(ME);
    return (retCode < SWL_RC_OK) ? amxd_status_unknown_error : amxd_status_ok;
}

/**
 * Add or update an AP instance object.
 */
static swl_rc_ne s_AddOrUpdateAPInstance(amxd_object_t* object, wld_scanResultSSID_t* ssid) {
    swl_macChar_t bssidStr = SWL_MAC_CHAR_NEW();
    swl_mac_binToChar(&bssidStr, &ssid->bssid);

    // Add an instance to the template object, or update the existing instance.
    bool add = (amxd_object_get_type(object) == amxd_object_template);

    amxd_trans_t trans;
    ASSERT_TRANSACTION_INIT(object, &trans, SWL_RC_ERROR, ME, "trans init failure");

    if(add) {
        amxd_object_t* lastInst = amxc_container_of(amxc_llist_get_last(&object->instances), amxd_object_t, it);
        uint32_t newIdx = amxd_object_get_index(lastInst) + 1;
        amxd_trans_add_inst(&trans, newIdx, NULL);
    }

    amxd_trans_set_value(int16_t, &trans, "RSSI", ssid->rssi);
    amxd_trans_set_value(cstring_t, &trans, "BSSID", bssidStr.cMac);

    if(add) {
        amxd_trans_select_pathf(&trans, ".SSID");
        amxd_trans_add_inst(&trans, 0, NULL);
    } else {
        // There is only one SSID instance, with index 1
        amxd_object_t* ssidInst = amxd_object_get_instance(amxd_object_get(object, "SSID"), NULL, 1);
        amxd_trans_select_object(&trans, ssidInst);
    }

    char* ssidStr = wld_ssid_to_string(ssid->ssid, ssid->ssidLen);
    amxd_trans_set_value(cstring_t, &trans, "SSID", ssidStr);
    amxd_trans_set_value(cstring_t, &trans, "BSSID", bssidStr.cMac);
    amxd_trans_set_value(uint16_t, &trans, "Bandwidth", ssid->bandwidth);
    amxd_trans_set_value(uint8_t, &trans, "ChannelUtilization", ssid->channelUtilization);
    amxd_trans_set_value(uint16_t, &trans, "StationCount", ssid->stationCount);
    free(ssidStr);

    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, SWL_RC_ERROR, ME, "trans apply failure");
    return SWL_RC_OK;
}

static void s_removeAPObject(amxd_object_t* apObj) {
    amxd_trans_t trans;
    ASSERT_TRANSACTION_INIT(amxd_object_get_parent(apObj), &trans, , ME, "trans init failure");
    uint32_t index = amxd_object_get_index(apObj);
    amxd_trans_del_inst(&trans, index, NULL);
    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, , ME, "trans apply failure");
}

static int s_apCallback(amxd_object_t* object _UNUSED, amxd_object_t* mobject, void* priv) {
    uint16_t channel = amxd_object_get_uint16_t(amxd_object_get_parent(mobject), "Channel", NULL);

    amxd_object_for_each(instance, it, mobject) {
        amxd_object_t* apInst = amxc_llist_it_get_data(it, amxd_object_t, it);
        char* bssidChar = amxd_object_get_cstring_t(apInst, "BSSID", NULL);
        swl_macChar_t bssidStd;
        if(!swl_mac_charToStandard(&bssidStd, bssidChar)) {
            SAH_TRACEZ_ERROR(ME, "invalid BSSID : %s", bssidChar);
            free(bssidChar);
            continue;
        }
        swl_macBin_t bssidBin;
        swl_mac_charToBin(&bssidBin, &bssidStd);

        bool found = false;
        amxc_llist_t* ssids = priv;
        amxc_llist_for_each(it, ssids) {
            wld_scanResultSSID_t* ssid = amxc_container_of(it, wld_scanResultSSID_t, it);
            if(swl_typeMacBin_equals(bssidBin, ssid->bssid)) {
                found = true;
                if(channel == ssid->channel) {
                    if(!ssid->dmUpdated) {
                        // Update AP instance object
                        if(s_AddOrUpdateAPInstance(apInst, ssid) == SWL_RC_OK) {
                            ssid->dmUpdated = true;
                        }
                    } else {
                        // Duplicate AP object, should be removed from DM
                        s_removeAPObject(apInst);
                    }
                } else {
                    // AP has moved to another channel
                    s_removeAPObject(apInst);
                }
            }
        }

        if(!found) {
            // AP is not present in the scan result list
            s_removeAPObject(apInst);
        }

        free(bssidChar);
    }
    return 0;
}

/**
 * Iterate over DataModel and compare each AP instance object with an entry in the result list.
 * Depending on the result list, an AP instance might be added, removed, or updated.
 */
static void s_iterateDM(amxd_object_t* objScan, amxc_llist_t* ssids) {
    amxd_object_for_all(objScan, "SurroundingChannels.*.Accesspoint.", s_apCallback, ssids);
}

/**
 * Iterate over result list and add any AP instance that is still left unprocessed.
 */
static void s_iterateResultList(amxd_object_t* objScan _UNUSED, amxc_llist_t* ssids) {
    amxc_llist_for_each(it, ssids) {
        wld_scanResultSSID_t* ssid = amxc_container_of(it, wld_scanResultSSID_t, it);
        if(ssid->dmUpdated) {
            continue;
        }
        swl_macChar_t macChar;
        swl_mac_binToChar(&macChar, &ssid->bssid);
        amxd_object_t* chanObj = NULL;
        if(s_getOrAddChannelObj(objScan, ssid->channel, &chanObj) != SWL_RC_OK) {
            continue;
        }
        amxd_object_t* apTempl = amxd_object_get(chanObj, "Accesspoint");
        if(s_AddOrUpdateAPInstance(apTempl, ssid) == SWL_RC_OK) {
            ssid->dmUpdated = true;
        }
    }
}

/**
 * Count the number of access points that are on the same channel as the radio currently is.
 */
static void s_updateCountCochannel(T_Radio* pR, amxd_object_t* objScan) {
    uint16_t channel = amxd_object_get_uint16_t(pR->pBus, "Channel", NULL);

    uint16_t nrCoChannel = 0;
    amxd_object_t* chanTemplate = amxd_object_get(objScan, "SurroundingChannels");
    amxd_object_t* chanObj = NULL;

    amxd_object_for_each(instance, it, chanTemplate) {
        chanObj = amxc_llist_it_get_data(it, amxd_object_t, it);
        if(amxd_object_get_uint16_t(chanObj, "Channel", NULL) == channel) {
            amxd_object_t* apTemplate = amxd_object_get(chanObj, "Accesspoint");
            nrCoChannel = amxd_object_get_instance_count(apTemplate);
            break;
        }
    }
    swl_typeUInt16_commitObjectParam(objScan, "NrCoChannelAP", nrCoChannel);
}

void wld_scan_cleanupScanResultSSID(wld_scanResultSSID_t* ssid) {
    ASSERTS_NOT_NULL(ssid, , ME, "NULL");
    amxc_llist_it_take(&ssid->it);
    amxc_llist_for_each(it, &ssid->vendorIEs) {
        wld_vendorIe_t* vendorIE = amxc_container_of(it, wld_vendorIe_t, it);
        amxc_llist_it_take(&vendorIE->it);
        if(vendorIE->object != NULL) {
            vendorIE->object->priv = NULL;
            vendorIE->object = NULL;
        }
        free(vendorIE);
    }
    free(ssid);
}


void wld_scan_cleanupScanResults(wld_scanResults_t* res) {
    ASSERTS_NOT_NULL(res, , ME, "NULL");
    amxc_llist_for_each(it, &res->ssids) {
        wld_scanResultSSID_t* ssid = amxc_container_of(it, wld_scanResultSSID_t, it);
        wld_scan_cleanupScanResultSSID(ssid);
    }
}

void wld_cleanupSurveyReport(wld_surveyReport_t* res) {
    ASSERTS_NOT_NULL(res, , ME, "NULL");
    amxc_llist_for_each(it, &res->surveyReport) {
        wld_chanSurveyReportEntry_t* report = amxc_container_of(it, wld_chanSurveyReportEntry_t, it);
        amxc_llist_it_take(&report->it);
        free(report);
    }
}
/**
 * Request to update the scan results in the datamodel.
 * This will remove the currently stored scan results, and replace the with the results from the latest scan.
 * It will just retrieve the latest results, it will NOT perform a scan itself.
 */
static void s_updateScanResultObjs(T_Radio* pR) {
    wld_scanResults_t res;

    amxc_llist_init(&res.ssids);

    if(pR->pFA->mfn_wrad_scan_results(pR, &res) < 0) {
        SAH_TRACEZ_ERROR(ME, "failed to get results");
        return;
    }
    amxd_object_t* objScan = amxd_object_get(pR->pBus, "ScanResults");
    ASSERT_NOT_NULL(objScan, , ME, "No ScanResults obj template");

    s_iterateDM(objScan, &res.ssids);
    s_iterateResultList(objScan, &res.ssids);

    s_updateCountCochannel(pR, objScan);

    wld_scan_cleanupScanResults(&res);
}

/**
 * This function should be called by the plug-in when it receives the notification that a scan is
 * completed.
 */
void wld_scan_done(T_Radio* pR, bool success) {
    ASSERT_NOT_NULL(pR, , ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s : scan done %u Reason %s => run %u, deferred %u",
                    pR->Name, success, pR->scanState.scanReason, wld_scan_isRunning(pR),
                    swl_function_deferIsActive(&pR->scanState.scanFunInfo));

    if(pR->pFA->mfn_wrad_continue_external_scan(pR) == SWL_RC_OK) {
        // scan continued by external scan manager
        return;
    }

    if(!wld_scan_isRunning(pR)) {
        return;
    }

    if(success) {
        pR->scanState.stats.nrScanDone++;
    } else {
        pR->scanState.stats.nrScanError++;
    }

    wld_scanReasonStats_t* stats = wld_getScanStatsByReason(pR, pR->scanState.scanReason);
    if(stats != NULL) {
        if(success) {
            stats->successCount++;
        } else {
            stats->errorCount++;
        }
    }
    swla_delayExec_add((swla_delayExecFun_cbf) s_updateScanStats, pR);

    wld_scanEvent_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.pRad = pR;
    ev.start = false;
    ev.success = success;
    ev.scanType = pR->scanState.scanType;
    ev.scanReason = pR->scanState.scanReason;
    wld_event_trigger_callback(gWld_queue_rad_onScan_change, &ev);

    if(success && (pR->scanState.scanType > SCAN_TYPE_INTERNAL) && pR->scanState.cfg.enableScanResultsDm) {
        swla_delayExec_add((swla_delayExecFun_cbf) s_updateScanResultObjs, pR);
    }

    wld_rad_scan_SendDoneNotification(pR, success);
    if(swl_function_deferIsActive(&pR->scanState.scanFunInfo)) {
        wld_rad_scan_FinishScanCall(pR, &success);
    }

    /* Scan is done */
    swl_str_copy(pR->scanState.scanReason, sizeof(pR->scanState.scanReason), "None");
    pR->scanState.scanType = SCAN_TYPE_NONE;
    swla_delayExec_add((swla_delayExecFun_cbf) s_updateScanProcState, pR);
}

bool wld_scan_isRunning(T_Radio* pR) {
    return pR->scanState.scanType != SCAN_TYPE_NONE;
}

static void s_sendNeighboringWifiDiagnosticResult();

static void s_scanStatus_cbf(const wld_scanEvent_t* scanEvent) {
    ASSERT_NOT_NULL(scanEvent, , ME, "NULL");

    T_Radio* pRad = scanEvent->pRad;
    char* path = amxd_object_get_path(pRad->pBus, AMXD_OBJECT_NAMED);
    amxc_var_t map;
    amxc_var_init(&map);
    amxc_var_set_type(&map, AMXC_VAR_ID_HTABLE);
    amxc_var_t* tmpMap = amxc_var_add_key(amxc_htable_t, &map, "Event", NULL);
    amxc_var_add_key(bool, tmpMap, "IsRunning", scanEvent->start);
    amxc_var_add_key(bool, tmpMap, "Success", scanEvent->success);
    amxc_var_add_key(int32_t, tmpMap, "ScanType", scanEvent->scanType);
    amxc_var_add_key(cstring_t, tmpMap, "ScanReason", scanEvent->scanReason);
    SAH_TRACEZ_INFO(ME, "%s: notif scan status change @ path=%s %s}", pRad->Name, path, "ScanChange");
    amxd_object_trigger_signal(pRad->pBus, "ScanChange", &map);
    amxc_var_clean(&map);
    free(path);
}

static void s_radScanStatusUpdateCb(wld_scanEvent_t* event) {
    ASSERT_NOT_NULL(event, , ME, "NULL");
    T_Radio* pRadio = event->pRad;
    ASSERT_NOT_NULL(pRadio, , ME, "NULL");
    ASSERTS_TRUE(swl_str_matches(event->scanReason, DIAG_SCAN_REASON), , ME, "Not diag scan");
    if(event->start) {
        amxc_llist_add_string(&g_neighWiFiDiag.runningRads, pRadio->Name);
        return;
    }
    ASSERTI_FALSE(amxc_llist_is_empty(&g_neighWiFiDiag.runningRads), , ME, "No diag running");
    amxc_llist_for_each(it, &g_neighWiFiDiag.runningRads) {
        amxc_string_t* radName = amxc_string_from_llist_it(it);
        if(swl_str_matches(amxc_string_get(radName, 0), pRadio->Name)) {
            amxc_llist_it_take(it);
            if(event->success) {
                amxc_llist_append(&g_neighWiFiDiag.completedRads, it);
            } else {
                amxc_llist_append(&g_neighWiFiDiag.failedRads, it);
            }
            break;
        }
    }

    //all diag scans are answered: send deferred reply
    if(amxc_llist_is_empty(&g_neighWiFiDiag.runningRads)) {
        /* Send the Neighboring WiFi Diagnostic Result list */
        s_sendNeighboringWifiDiagnosticResult();
    }
}

static wld_event_callback_t s_radScanStatusCbContainer = {
    .callback = (wld_event_callback_fun) s_radScanStatusUpdateCb
};



amxd_status_t _Radio_FullScan(amxd_object_t* object,
                              amxd_function_t* func _UNUSED,
                              amxc_var_t* args,
                              amxc_var_t* retval) {

    SAH_TRACEZ_IN(ME);
    amxd_status_t res = amxd_status_ok;
    amxc_var_set_type(retval, AMXC_VAR_ID_HTABLE);
    swl_usp_cmdStatus_ne status = SWL_USP_CMD_STATUS_SUCCESS;

    T_Radio* pR = object->priv;
    if(pR == NULL) {
        status = SWL_USP_CMD_STATUS_ERROR_NOT_READY;
        SAH_TRACEZ_ERROR(ME, "%s: NULL", pR->Name);
        goto error;
    }

    if(s_isExternalScanFilter(pR) == true) {
        res = amxd_status_permission_denied;
        status = SWL_USP_CMD_STATUS_CANCELED;
        goto error;
    }

    if(!wld_rad_isActive(pR)) {
        SAH_TRACEZ_ERROR(ME, "%s: down!, status= %d", pR->Name, pR->status);
        status = SWL_USP_CMD_STATUS_ERROR_INTERFACE_DOWN;
        goto error;
    }

    if(!s_isScanRequestReady(pR)) {
        SAH_TRACEZ_ERROR(ME, "%s: not ready for scan", pR->Name);
        status = SWL_USP_CMD_STATUS_ERROR_NOT_READY;
        goto error;
    }

    if(swl_function_deferIsActive(&pR->scanState.scanFunInfo)) { //make sure the scan is not active on this radio
        SAH_TRACEZ_ERROR(ME, "%s: radio scan is active", pR->Name);
        status = SWL_USP_CMD_STATUS_ERROR_NOT_READY;
        goto error;
    }


    amxd_object_t* scanconfigObj = amxd_object_get(object, "ScanConfig");
    ASSERTS_NOT_NULL(scanconfigObj, amxd_status_unknown_error, ME, "NULL");
    uint32_t dwellTime = GET_INT32(args, "DwellTime");
    amxd_object_set_int32_t(scanconfigObj, "ActiveChannelTime", dwellTime);
    amxc_var_add_key(cstring_t, args, "scanReason", FULL_SCAN_REASON);
    const char* ssid = GET_CHAR(args, "SSID");
    if(ssid != NULL) {
        amxc_var_add_key(cstring_t, args, "SSID", ssid);
    }

    res = s_startScan(object, NULL, args, retval, SCAN_TYPE_SSID);

    if(res == amxd_status_invalid_function_argument) {
        status = SWL_USP_CMD_STATUS_ERROR_INVALID_INPUT;
        goto error;
    } else if(res != amxd_status_deferred) {
        status = SWL_USP_CMD_STATUS_ERROR_TIMEOUT;
        goto error;
    }

    amxc_var_add_key(cstring_t, retval, "Status",
                     swl_uspCmdStatus_toString(status));


    return swl_function_defer(&pR->scanState.scanFunInfo, func, retval);



error:
    amxc_var_add_key(cstring_t, retval, "Status",
                     swl_uspCmdStatus_toString(status));
    SAH_TRACEZ_OUT(ME);
    return res;

}


static void s_addDiagSingleResultToMap(amxc_var_t* pResultListMap, T_Radio* pRad, wld_scanResultSSID_t* pSsid) {
    ASSERTS_NOT_NULL(pResultListMap, , ME, "NULL");
    ASSERTS_NOT_NULL(pSsid, , ME, "NULL");

    amxc_var_t* resulMap = amxc_var_add(amxc_htable_t, pResultListMap, NULL);
    ASSERTS_NOT_NULL(resulMap, , ME, "NULL");

    // Set Elements
    swl_macChar_t bssidStr = SWL_MAC_CHAR_NEW();
    swl_mac_binToChar(&bssidStr, &pSsid->bssid);
    char* path = amxd_object_get_path(pRad->pBus, AMXD_OBJECT_INDEXED);
    amxc_var_add_key(cstring_t, resulMap, "Radio", path);
    free(path);
    char* ssidStr = wld_ssid_to_string(pSsid->ssid, pSsid->ssidLen);
    amxc_var_add_key(cstring_t, resulMap, "SSID", ssidStr);
    free(ssidStr);
    amxc_var_add_key(cstring_t, resulMap, "BSSID", bssidStr.cMac);
    amxc_var_add_key(uint32_t, resulMap, "Channel", pSsid->channel);
    amxc_var_add_key(int32_t, resulMap, "SignalStrength", pSsid->rssi);
    amxc_var_add_key(cstring_t, resulMap, "SecurityModeEnabled", swl_security_apModeToString(pSsid->secModeEnabled, SWL_SECURITY_APMODEFMT_ALTERNATE));
    amxc_var_add_key(cstring_t, resulMap, "EncryptionMode", swl_security_encMode_str[swl_security_encModeEnumFromSecModeEnum(pSsid->secModeEnabled)]);
    amxc_var_add_key(cstring_t, resulMap, "OperatingFrequencyBand", Rad_SupFreqBands[pRad->operatingFrequencyBand]);

    char operatingStandardsChar[32] = "";
    char supportedStandardsChar[32] = "";
    swl_radStd_toChar(operatingStandardsChar, sizeof(operatingStandardsChar), pSsid->operatingStandards, SWL_RADSTD_FORMAT_STANDARD, 0);
    swl_radStd_toChar(supportedStandardsChar, sizeof(supportedStandardsChar), pSsid->supportedStandards, SWL_RADSTD_FORMAT_STANDARD, 0);
    amxc_var_add_key(cstring_t, resulMap, "OperatingStandards", operatingStandardsChar);
    amxc_var_add_key(cstring_t, resulMap, "SupportedStandards", supportedStandardsChar);

    swl_chanspec_t chSpec = SWL_CHANSPEC_EMPTY;
    swl_chanspec_fromFreqCtrlCentre(&chSpec, pRad->operatingFrequencyBand, pSsid->channel, pSsid->centreChannel);
    amxc_var_add_key(cstring_t, resulMap, "OperatingChannelBandwidth", swl_radBw_str[swl_chanspec_toRadBw(&chSpec)]);

    amxc_var_add_key(int32_t, resulMap, "Noise", pSsid->noise);
    amxc_var_add_key(uint32_t, resulMap, "BeaconPeriod", pSsid->beaconInterval);
    amxc_var_add_key(uint32_t, resulMap, "DTIMPeriod", pSsid->dtimPeriod);
    amxc_var_add_key(cstring_t, resulMap, "Mode", pSsid->adhoc ? "AdHoc" : "Infrastructure");

    char basicRatesChar[64] = "";
    swl_conv_maskToCharSep(basicRatesChar, sizeof(basicRatesChar), pSsid->basicDataTransferRates, swl_mcs_legacyStrList, SWL_MCS_LEGACY_LIST_SIZE, ',');
    amxc_var_add_key(cstring_t, resulMap, "BasicDataTransferRates", basicRatesChar);

    char supportedRatesChar[64] = "";
    swl_conv_maskToCharSep(supportedRatesChar, sizeof(supportedRatesChar), pSsid->supportedDataTransferRates, swl_mcs_legacyStrList, SWL_MCS_LEGACY_LIST_SIZE, ',');
    amxc_var_add_key(cstring_t, resulMap, "SupportedDataTransferRates", supportedRatesChar);
}

static void s_addDiagRadioResultsToMap(amxc_var_t* pResultListMap, T_Radio* pRad, wld_scanResults_t* pScanResults) {
    ASSERTS_NOT_NULL(pResultListMap, , ME, "NULL");
    ASSERTS_NOT_NULL(pScanResults, , ME, "NULL");
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");

    amxc_llist_for_each(it, &pScanResults->ssids) {
        wld_scanResultSSID_t* pSsid = amxc_container_of(it, wld_scanResultSSID_t, it);
        s_addDiagSingleResultToMap(pResultListMap, pRad, pSsid);
    }
}

static void s_addDiagRadiosListToMap(amxc_var_t* retval, const char* listName, amxc_llist_t* listEntries) {
    ASSERTS_FALSE(amxc_llist_is_empty(listEntries), , ME, "Empty");
    amxc_string_t radios;
    amxc_string_init(&radios, 0);
    amxc_string_join_llist(&radios, listEntries, ',');
    amxc_var_add_key(cstring_t, retval, listName, amxc_string_get(&radios, 0));
    amxc_string_clean(&radios);
}

static void s_addDiagRadiosStatusToMap(amxc_var_t* retval) {
    if(!amxc_llist_is_empty(&g_neighWiFiDiag.runningRads)) {
        amxc_var_add_key(cstring_t, retval, "Status",
                         swl_uspCmdStatus_toString(SWL_USP_CMD_STATUS_ERROR));
        s_addDiagRadiosListToMap(retval, "FailedRadios", &g_neighWiFiDiag.runningRads);
        return;
    }
    /*
     * If at least one radio succeeds in making a scan:
     * ==> Status is "Complete"
     * ==> else, Status is "Error"
     */
    if(!amxc_llist_is_empty(&g_neighWiFiDiag.completedRads)) {
        amxc_var_add_key(cstring_t, retval, "Status",
                         swl_uspCmdStatus_toString(SWL_USP_CMD_STATUS_COMPLETE));
    } else if(!amxc_llist_is_empty(&g_neighWiFiDiag.canceledRads)) {
        amxc_var_add_key(cstring_t, retval, "Status",
                         swl_uspCmdStatus_toString(SWL_USP_CMD_STATUS_CANCELED));
    } else {
        amxc_var_add_key(cstring_t, retval, "Status",
                         swl_uspCmdStatus_toString(SWL_USP_CMD_STATUS_ERROR));
    }
    s_addDiagRadiosListToMap(retval, "ReportingRadios", &g_neighWiFiDiag.completedRads);
    s_addDiagRadiosListToMap(retval, "CanceledRadios", &g_neighWiFiDiag.canceledRads);
    s_addDiagRadiosListToMap(retval, "FailedRadios", &g_neighWiFiDiag.failedRads);
}

static void s_sendNeighboringWifiDiagnosticResult() {
    amxc_var_t retMap;
    amxc_var_init(&retMap);
    amxc_var_set_type(&retMap, AMXC_VAR_ID_HTABLE);

    s_addDiagRadiosStatusToMap(&retMap);
    amxc_var_t* pResultListMap = amxc_var_add_key(amxc_llist_t, &retMap, "Result", NULL);

    wld_scanResults_t scanRes;
    amxc_llist_init(&scanRes.ssids);

    T_Radio* pRadio = NULL;
    amxc_llist_for_each(it, &g_neighWiFiDiag.completedRads) {
        amxc_string_t* radName = amxc_string_from_llist_it(it);
        if(amxc_string_is_empty(radName)) {
            continue;
        }
        pRadio = wld_rad_get_radio(amxc_string_get(radName, 0));
        if(pRadio == NULL) {
            continue;
        }
        // Get data
        if(pRadio->pFA->mfn_wrad_scan_results(pRadio, &scanRes) >= SWL_RC_OK) {
            // Add data to map
            s_addDiagRadioResultsToMap(pResultListMap, pRadio, &scanRes);
        } else {
            SAH_TRACEZ_ERROR(ME, "%s: Unable to retrieve scan results", pRadio->Name);
        }
        wld_scan_cleanupScanResults(&scanRes);
    }

    swl_function_deferDone(&g_neighWiFiDiag.callInfo, amxd_status_ok, NULL, &retMap);
    amxc_var_clean(&retMap);

    wld_event_remove_callback(gWld_queue_rad_onScan_change, &s_radScanStatusCbContainer);
    amxc_llist_clean(&g_neighWiFiDiag.completedRads, amxc_string_list_it_free);
    amxc_llist_clean(&g_neighWiFiDiag.canceledRads, amxc_string_list_it_free);
    amxc_llist_clean(&g_neighWiFiDiag.failedRads, amxc_string_list_it_free);

    SAH_TRACEZ_OUT(ME);
}

amxd_status_t _NeighboringWiFiDiagnostic(amxd_object_t* pWifiObj,
                                         amxd_function_t* func,
                                         amxc_var_t* args _UNUSED,
                                         amxc_var_t* retval) {

    SAH_TRACEZ_IN(ME);

    if(swl_function_deferIsActive(&g_neighWiFiDiag.callInfo)) {
        SAH_TRACEZ_ERROR(ME, "WiFiDiagnostic is already running");
        goto error;
    }

    amxc_llist_init(&g_neighWiFiDiag.runningRads);
    amxc_llist_init(&g_neighWiFiDiag.completedRads);
    amxc_llist_init(&g_neighWiFiDiag.canceledRads);
    amxc_llist_init(&g_neighWiFiDiag.failedRads);

    amxd_status_t status = amxd_status_ok;
    amxc_var_t localArgs;
    amxc_var_init(&localArgs);
    amxc_var_set_type(&localArgs, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &localArgs, "scanReason", DIAG_SCAN_REASON);
    wld_event_add_callback(gWld_queue_rad_onScan_change, &s_radScanStatusCbContainer);
    // use radio obj instances order (isof radio device detection order)
    amxd_object_t* radTempl = amxd_object_get(pWifiObj, "Radio");
    amxd_object_for_each(instance, it, radTempl) {
        amxd_object_t* radObj = amxc_llist_it_get_data(it, amxd_object_t, it);
        if((radObj == NULL) || (radObj->priv == NULL)) {
            continue;
        }
        T_Radio* pRadio = radObj->priv;
        status = s_startScan(pRadio->pBus, func, &localArgs, retval, SCAN_TYPE_INTERNAL);
        if(status != amxd_status_deferred) {
            amxc_llist_add_string(&g_neighWiFiDiag.failedRads, pRadio->Name);
        }
    }
    amxc_var_clean(&localArgs);
    if(!amxc_llist_is_empty(&g_neighWiFiDiag.runningRads)) {
        return swl_function_defer(&g_neighWiFiDiag.callInfo, func, retval);
    }
    wld_event_remove_callback(gWld_queue_rad_onScan_change, &s_radScanStatusCbContainer);
error:
    amxc_var_set_type(retval, AMXC_VAR_ID_HTABLE);
    s_addDiagRadiosStatusToMap(retval);
    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

swl_rc_ne wld_scan_start(T_Radio* pRad, wld_scan_type_e type, const char* reason) {
    ASSERTS_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_TRUE(type < SCAN_TYPE_MAX, SWL_RC_INVALID_PARAM, ME, "invalid scan type");

    swl_rc_ne error = pRad->pFA->mfn_wrad_start_scan(pRad);
    ASSERT_TRUE(swl_rc_isOk(error), error, ME, "%s: scan start failed %i", pRad->Name, error);

    s_notifyStartScan(pRad, type, reason, error);
    return error;
}

swl_rc_ne wld_scan_stop(T_Radio* pRad) {
    ASSERTS_TRUE(wld_scan_isRunning(pRad), SWL_RC_INVALID_STATE, ME, "no internal scan running");
    if(pRad->pFA->mfn_wrad_stop_scan(pRad) < 0) {
        SAH_TRACEZ_ERROR(ME, "%s: Unable to stop scan", pRad->Name);
        return SWL_RC_ERROR;
    }
    wld_scan_done(pRad, false);
    return SWL_RC_OK;
}

static wld_event_callback_t s_scanStatus_cb = {
    .callback = (wld_event_callback_fun) s_scanStatus_cbf
};

void wld_scan_init(T_Radio* pRad _UNUSED) {
    wld_event_add_callback(gWld_queue_rad_onScan_change, &s_scanStatus_cb);
}

void wld_scan_destroy(T_Radio* pRad _UNUSED) {
    wld_event_remove_callback(gWld_queue_rad_onScan_change, &s_scanStatus_cb);
    if(swl_function_deferIsActive(&g_neighWiFiDiag.callInfo)) {
        amxd_function_deferred_remove(g_neighWiFiDiag.callInfo.callId);
    }
}

