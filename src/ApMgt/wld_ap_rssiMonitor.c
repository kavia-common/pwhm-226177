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
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <debug/sahtrace.h>
#include <assert.h>


#include <net/if.h>
#include <sys/ioctl.h>

#include "wld.h"
#include "wld_util.h"
#include "wld_accesspoint.h"
#include "wld_ssid.h"
#include "wld_radio.h"
#include "wld_assocdev.h"
#include "swl/swl_assert.h"
#include "swla/swla_tupleType.h"
#include "swla/swla_table.h"
#include "swla/swla_mac.h"
#include "swla/swla_time_spec.h"
#include "wld_monitor.h"
#include "wld_ap_rssiMonitor.h"
#include "swla/types/swla_tupleTypeArray.h"
#include "swla/swla_mcs.h"

#define ME "apRssi"

char* historyNames[] = {
    "MeasurementTimestamps",
    "SignalStrengthHistory",
    "NoiseHistory",
    "DownlinkRateHistory",
    "UplinkRateHistory",
    "BytesSentHistory",
    "BytesReceivedHistory",
    "PacketsSentHistory",
    "PacketsReceivedHistory",
    "ErrorsSentHistory",
    "ErrorsReceivedHistory",
    "TxFrameCountHistory",
    "RxFrameCountHistory",
    "RetransCountHistory",
    "FailedRetransCountHistory",
    "RxRetransCountHistory",
    "RxFailedRetransCountHistory",
    "RetryCountHistory",
    "MultipleRetryCountHistory",
    "InactiveHistory",
    "PowerSaveHistory",
    "TxLinkBandwidthHistory",
    "TxSpatialStreamHistory",
    "TxRateStandardHistory",
    "TxMcsIndexHistory",
    "RxLinkBandwidthHistory",
    "RxSpatialStreamHistory",
    "RxRateStandardHistory",
    "RxMcsIndexHistory"
};

SWL_ASSERT_STATIC(SWL_ARRAY_SIZE(historyNames) == (gtWld_staHistory__max), "historyNames not correctly defined");

static void timeHandler(void* userdata) {
    T_AccessPoint* pAP = (T_AccessPoint*) userdata;

    T_RssiEventing* ev = &pAP->rssiEventing;


    SAH_TRACEZ_INFO(ME, "Time rssiMon %s", pAP->alias);

    swl_rc_ne result = pAP->pFA->mfn_wvap_update_rssi_stats(pAP);
    if(!swl_rc_isOk(result)) {
        SAH_TRACEZ_ERROR(ME, "%s: stopping rssi monitor, rssi stats failed", pAP->name);
        wld_mon_stop(&ev->monitor);
    }
    int i;
    T_AssociatedDevice* pAD;

    int nrUpdates = 0;

    amxc_var_t myList;
    amxc_var_init(&myList);
    amxc_var_set_type(&myList, AMXC_VAR_ID_LIST);

    for(i = 0; i < pAP->AssociatedDeviceNumberOfEntries; i++) {
        pAD = pAP->AssociatedDevice[i];
        if(!pAD) {
            SAH_TRACEZ_ERROR(ME, "AssociatedDevice[%d]==null", i);
            continue;
        }
        if((pAP->rssiEventing.rssiInterval == 0) || (pAD->noise == 0) || (pAD->SignalStrength == 0) || (pAD->SignalStrength < pAD->noise)) {
            pAD->rssiLevel = 0;
            continue;
        }

        pAD->rssiAccumulator = wld_util_performFactorStep(pAD->rssiAccumulator, pAD->SignalStrength, ev->averagingFactor);
        int32_t rssi_val = WLD_ACC_TO_VAL(pAD->rssiAccumulator);

        uint32_t diff = abs(pAD->rssiLevel - rssi_val);

        if(diff >= pAP->rssiEventing.rssiInterval) {
            pAD->rssiLevel = rssi_val;
            amxc_var_t myMap;
            amxc_var_init(&myMap);
            amxc_var_set_type(&myMap, AMXC_VAR_ID_HTABLE);
            amxc_var_add_key(int32_t, &myMap, "SignalStrength", rssi_val);
            amxc_var_add_key(int32_t, &myMap, "Noise", pAD->noise);
            char buffer[ETHER_ADDR_STR_LEN];
            wldu_convMac2Str(pAD->MACAddress, ETHER_ADDR_LEN, buffer, ETHER_ADDR_STR_LEN);

            amxc_var_add_key(cstring_t, &myMap, "MACAddress", buffer);
            amxc_var_add(amxc_htable_t, &myList, amxc_var_get_const_amxc_htable_t(&myMap));
            amxc_var_clean(&myMap);
            nrUpdates++;
        }
    }

    if(nrUpdates > 0) {
        amxd_object_t* eventObject = amxd_object_findf(pAP->pBus, "RssiEventing");
        wld_mon_sendUpdateNotification(eventObject, "RssiUpdate", &myList);
    }

    if(pAP->rssiEventing.historyEnable == 1) {
        wld_apRssiMon_updateStaHistoryAll(pAP);
        wld_apRssiMon_sendStaHistoryAll(pAP);
    }

    wld_ap_rssiSampleAction_t action = {
        .nrUpdates = nrUpdates,
        .updates = &myList
    };

    wld_vap_sendActionEvent(pAP, WLD_VAP_ACTION_RSSI_SAMPLE, &action);

    amxc_var_clean(&myList);
}

void wld_apRssiMon_updateEnable(T_AccessPoint* pAP) {
    T_SSID* pSSID = (T_SSID*) pAP->pSSID;
    T_RssiEventing* ev = &pAP->rssiEventing;

    wld_mon_writeActive(&ev->monitor, pSSID->status == RST_UP);
}

void wld_ap_rssiMonInit(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    amxd_object_t* rssiEventingObj = amxd_object_findf(pAP->pBus, "RssiEventing");
    ASSERTS_NOT_NULL(rssiEventingObj, , ME, "NULL");
    T_RssiEventing* ev = &pAP->rssiEventing;
    char name[AP_NAME_SIZE + 16] = {0};
    snprintf(name, sizeof(name), "%s_ApRssiMon", pAP->alias);

    amxc_var_t value;
    amxc_var_init(&value);

    amxd_param_get_value(amxd_object_get_param_def(rssiEventingObj, "RssiInterval"), &value);
    pAP->rssiEventing.rssiInterval = amxc_var_dyncast(uint32_t, &value);

    amxd_param_get_value(amxd_object_get_param_def(rssiEventingObj, "AveragingFactor"), &value);
    pAP->rssiEventing.averagingFactor = amxc_var_dyncast(uint32_t, &value);

    amxd_param_get_value(amxd_object_get_param_def(rssiEventingObj, "HistoryEnable"), &value);
    pAP->rssiEventing.historyEnable = amxc_var_dyncast(bool, &value);

    amxd_param_get_value(amxd_object_get_param_def(rssiEventingObj, "HistoryLen"), &value);
    pAP->rssiEventing.historyLen = amxc_var_dyncast(uint32_t, &value);

    amxd_param_get_value(amxd_object_get_param_def(rssiEventingObj, "HistoryIntervalCoeff"), &value);
    pAP->rssiEventing.historyIntervalCoeff = amxc_var_dyncast(uint32_t, &value);

    amxd_param_get_value(amxd_object_get_param_def(rssiEventingObj, "SendPeriodicEvent"), &value);
    pAP->rssiEventing.sendPeriodicEvent = amxc_var_dyncast(bool, &value);

    amxd_param_get_value(amxd_object_get_param_def(rssiEventingObj, "SendEventOnAssoc"), &value);
    pAP->rssiEventing.sendEventOnAssoc = amxc_var_dyncast(bool, &value);

    amxd_param_get_value(amxd_object_get_param_def(rssiEventingObj, "SendEventOnDisassoc"), &value);
    pAP->rssiEventing.sendEventOnDisassoc = amxc_var_dyncast(bool, &value);

    amxc_var_clean(&value);

    wld_mon_initMon(&ev->monitor, rssiEventingObj, name, pAP, timeHandler);
}

void wld_ap_rssiMonDestroy(T_AccessPoint* pAP) {
    wld_mon_destroyMon(&pAP->rssiEventing.monitor);
}

static void s_setApRssiEventing_ocf(void* priv _UNUSED, amxd_object_t* object, const amxc_var_t* const newParamValues) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pAP, , ME, "No AP Ctx");

    SAH_TRACEZ_INFO(ME, "%s: Update rssiMon", pAP->alias);

    T_RssiEventing* ev = &pAP->rssiEventing;
    ASSERT_NOT_NULL(ev, , ME, "NULL");

    bool cleanHist = false;
    bool updateHist = false;

    amxc_var_for_each(newValue, newParamValues) {
        const char* pname = amxc_var_key(newValue);
        if(swl_str_matches(pname, "RssiInterval")) {
            ev->rssiInterval = amxc_var_dyncast(uint32_t, newValue);
            SAH_TRACEZ_INFO(ME, "Update rssiInterval %d", ev->rssiInterval);
        } else if(swl_str_matches(pname, "AveragingFactor")) {
            ev->averagingFactor = amxc_var_dyncast(uint32_t, newValue);
            SAH_TRACEZ_INFO(ME, "Update averagingFactor %d", ev->averagingFactor);
        } else if(swl_str_matches(pname, "Interval")) {
            wld_mon_setInterval_pwf(&ev->monitor, newValue);
        } else if(swl_str_matches(pname, "SendPeriodicEvent")) {
            ev->sendPeriodicEvent = amxc_var_dyncast(bool, newValue);
            SAH_TRACEZ_INFO(ME, "Update sendPeriodicEvent %d", ev->sendPeriodicEvent);
        } else if(swl_str_matches(pname, "HistoryIntervalCoeff")) {
            uint32_t historyIntervalCoeff = amxc_var_dyncast(uint32_t, newValue);
            if(ev->historyIntervalCoeff != historyIntervalCoeff) {
                ev->historyIntervalCoeff = historyIntervalCoeff;
                SAH_TRACEZ_INFO(ME, "%s: Update historyIntervalCoeff to %d", pAP->alias, historyIntervalCoeff);
                cleanHist = true;
            }
        } else if(swl_str_matches(pname, "HistoryLen")) {
            uint32_t historyLen = amxc_var_dyncast(uint32_t, newValue);
            if(ev->historyLen != historyLen) {
                ev->historyLen = historyLen;
                SAH_TRACEZ_INFO(ME, "%s: Update historyLen to %d", pAP->alias, historyLen);
                updateHist = true;
            }
        } else if(swl_str_matches(pname, "SendEventOnAssoc")) {
            ev->sendEventOnAssoc = amxc_var_dyncast(bool, newValue);
            SAH_TRACEZ_INFO(ME, "Update sendEventOnAssoc %d", ev->sendEventOnAssoc);
        } else if(swl_str_matches(pname, "SendEventOnDisassoc")) {
            ev->sendEventOnDisassoc = amxc_var_dyncast(bool, newValue);
            SAH_TRACEZ_INFO(ME, "Update sendEventOnDisassoc %d", ev->sendEventOnDisassoc);
        } else if(swl_str_matches(pname, "HistoryEnable")) {
            bool historyEnable = amxc_var_dyncast(bool, newValue);
            if(ev->historyEnable != historyEnable) {
                ev->historyEnable = historyEnable;
                SAH_TRACEZ_INFO(ME, "Update historyEnable %d", ev->historyEnable);
                cleanHist = true;
                updateHist = ev->historyEnable;
            }
        }
    }

    if(cleanHist) {
        wld_apRssiMon_cleanStaHistoryAll(pAP);
    }
    if(updateHist) {
        wld_apRssiMon_updateHistoryLen(pAP);
    }

    amxc_var_t* val = GET_ARG(newParamValues, "Enable");
    if(val != NULL) {
        wld_mon_setEnable_pwf(&ev->monitor, val);
    }

    SAH_TRACEZ_OUT(ME);
}

static void s_setApRssiEventingEnable_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pAP, , ME, "No AP Ctx");
    T_RssiEventing* ev = &pAP->rssiEventing;
    ASSERT_NOT_NULL(ev, , ME, "NULL");

    wld_mon_setEnable_pwf(&ev->monitor, newValue);

    SAH_TRACEZ_OUT(ME);
}

SWLA_DM_HDLRS(sApRssiEventingConfigDmHdlrs,
              ARR(SWLA_DM_PARAM_HDLR("Enable", s_setApRssiEventingEnable_pwf),
                  ),
              .objChangedCb = s_setApRssiEventing_ocf
              );

void _wld_ap_setRssiEventing_ocf(const char* const sig_name,
                                 const amxc_var_t* const data,
                                 void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sApRssiEventingConfigDmHdlrs, sig_name, data, priv);
}

void wld_ap_rssiEv_debug(T_AccessPoint* pAP, amxc_var_t* retMap) {
    T_RssiEventing* ev = &pAP->rssiEventing;
    amxc_var_add_key(uint32_t, retMap, "RssiInterval", ev->rssiInterval);
    amxc_var_add_key(uint32_t, retMap, "AveragingFactor", ev->averagingFactor);
    wld_mon_debug(&ev->monitor, retMap);
}

void wld_apRssiMon_createStaHistory(T_AssociatedDevice* pAD, uint32_t historyLen) {
    ASSERT_NOT_NULL(pAD, , ME, "NULL");
    ASSERTS_TRUE(historyLen > 0, , ME, "No history samples");

    pAD->staHistory = (wld_assocDev_history_t*) calloc(1, sizeof(wld_assocDev_history_t));
    if(pAD->staHistory == NULL) {
        SAH_TRACEZ_ERROR(ME, "%s : Sta history calloc failed!", pAD->Name);
        return;
    }
    pAD->staHistory->samples = (wld_staHistory_t*) calloc(historyLen, sizeof(wld_staHistory_t));

    if(pAD->staHistory->samples == NULL) {
        SAH_TRACEZ_ERROR(ME, "%s: Samples calloc failed!", pAD->Name);
        return;
    }
}

void wld_apRssiMon_destroyStaHistory(T_AssociatedDevice* pAD) {
    ASSERT_NOT_NULL(pAD, , ME, "NULL");
    ASSERT_NOT_NULL(pAD->staHistory, , ME, "NULL");

    free(pAD->staHistory->samples);
    pAD->staHistory->samples = NULL;
    free(pAD->staHistory);
    pAD->staHistory = NULL;
}

void wld_apRssiMon_clearSendEventOnAssoc(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, , ME, "NULL");

    int i;
    T_AssociatedDevice* pAD;
    for(i = 0; i < pAP->AssociatedDeviceNumberOfEntries; i++) {
        pAD = pAP->AssociatedDevice[i];
        if(wld_apRssiMon_isReadyStaHistory(pAD) == false) {
            continue;
        }
    }
}

bool wld_apRssiMon_isReadyStaHistory(T_AssociatedDevice* pAD) {
    ASSERT_NOT_NULL(pAD, false, ME, "NULL");
    ASSERT_NOT_NULL(pAD->staHistory, false, ME, "NULL");
    return true;
}

void wld_apRssiMon_updateHistoryLen(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, , ME, "NULL");

    int i;
    pAP->historyCnt = 0;
    T_AssociatedDevice* pAD;
    for(i = 0; i < pAP->AssociatedDeviceNumberOfEntries; i++) {
        pAD = pAP->AssociatedDevice[i];
        if(wld_apRssiMon_isReadyStaHistory(pAD) == false) {
            continue;
        }
        free(pAD->staHistory->samples);
        pAD->staHistory->samples = NULL;
        free(pAD->staHistory);
        pAD->staHistory = NULL;

        pAD->staHistory = (wld_assocDev_history_t*) calloc(1, sizeof(wld_assocDev_history_t));
        if(pAD->staHistory == NULL) {
            SAH_TRACEZ_ERROR(ME, "%s: Null short history buffer", (char*) pAD->Name);
            continue;
        }
        pAD->staHistory->samples = (wld_staHistory_t*) calloc(pAP->rssiEventing.historyLen, sizeof(wld_staHistory_t));
        if(pAD->staHistory->samples == NULL) {
            SAH_TRACEZ_ERROR(ME, "%s: Null samples", (char*) pAD->Name);
        }
        pAD->staHistory->index_last_sample = 0;
        pAD->staHistory->nr_valid_samples = 0;
    }
}

void wld_apRssiMon_cleanStaHistory(wld_assocDev_history_t* staHistory, uint32_t historyLen) {
    ASSERT_NOT_NULL(staHistory, , ME, "NULL");

    staHistory->nr_valid_samples = 0;
    staHistory->index_last_sample = 0;

    swl_timespec_reset(&staHistory->measurementTimestampAssoc);
    staHistory->signalStrength = DEFAULT_BASE_RSSI;
    staHistory->noise = DEFAULT_BASE_RSSI;
    staHistory->signalNoiseRatio = 0;
    swl_timespec_reset(&staHistory->measurementTimestamp);
    staHistory->signalStrengthAssoc = DEFAULT_BASE_RSSI;
    staHistory->noiseAssoc = DEFAULT_BASE_RSSI;
    staHistory->signalNoiseRatioAssoc = 0;

    wld_apRssiMon_cleanStaHistorySamples(staHistory, historyLen);
}

void wld_apRssiMon_cleanStaHistorySamples(wld_assocDev_history_t* staHistory, uint32_t historyLen) {
    ASSERT_NOT_NULL(staHistory, , ME, "NULL");

    memset(staHistory->samples, 0, historyLen * sizeof(wld_staHistory_t));
}

void wld_apRssiMon_cleanStaHistoryAll(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, , ME, "NULL");

    int i;
    pAP->historyCnt = 0;
    T_AssociatedDevice* pAD;
    for(i = 0; i < pAP->AssociatedDeviceNumberOfEntries; i++) {
        pAD = pAP->AssociatedDevice[i];
        if(pAD == NULL) {
            SAH_TRACEZ_ERROR(ME, "%s: Null device %i", pAP->alias, i);
            continue;
        }
        wld_apRssiMon_cleanStaHistory(pAD->staHistory, pAP->rssiEventing.historyLen);
    }
}

wld_staHistory_t* wld_apRssiMon_getOldestStaSample(T_AccessPoint* pAP, T_AssociatedDevice* pAD) {
    ASSERT_NOT_NULL(pAD, NULL, ME, "NULL");
    ASSERTS_TRUE(pAP->rssiEventing.historyEnable, NULL, ME, "DISABLED");
    ASSERTS_TRUE(pAD->AuthenticationState, NULL, ME, "No auth");
    ASSERT_NOT_NULL(pAD->staHistory, NULL, ME, "NULL");


    uint32_t historyLen = pAP->rssiEventing.historyLen;
    uint8_t nbValidSample = pAD->staHistory->nr_valid_samples;
    uint8_t indexOldestSample = nbValidSample >= historyLen ? (pAD->staHistory->index_last_sample + 1) % historyLen : 0;
    return &pAD->staHistory->samples[indexOldestSample];
}

void wld_apRssiMon_updateStaHistoryAll(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, , ME, "NULL");

    int i = 0;
    T_AssociatedDevice* pAD;

    pAP->historyCnt = (pAP->historyCnt + 1) % (pAP->rssiEventing.historyLen * pAP->rssiEventing.historyIntervalCoeff);

    if((pAP->historyCnt % pAP->rssiEventing.historyIntervalCoeff) == 0) {
        if(pAP->pFA->mfn_wvap_get_station_stats(pAP) < SWL_RC_OK) {
            SAH_TRACEZ_ERROR(ME, "%s: get station stats failed", pAP->alias);
        }
    }

    for(i = 0; i < pAP->AssociatedDeviceNumberOfEntries; i++) {
        pAD = pAP->AssociatedDevice[i];
        wld_apRssiMon_updateStaHistory(pAP, pAD);
    }
}

bool wld_apRssiMon_getMinMaxSignal(T_AccessPoint* pAP, T_AssociatedDevice* pAD, wld_apRssiMon_signalRange_t* range) {
    ASSERT_NOT_NULL(pAD, false, ME, "NULL");
    ASSERTS_TRUE(pAP->rssiEventing.historyEnable, false, ME, "DISABLED");
    ASSERTS_TRUE(pAD->AuthenticationState, false, ME, "NULL");
    ASSERT_NOT_NULL(pAD->staHistory, false, ME, "NULL");


    uint8_t nbValidSample = pAD->staHistory->nr_valid_samples;
    if(nbValidSample == 0) {
        return false;
    }
    range->minRssi = pAD->staHistory->samples[0].signalStrength;
    range->maxRssi = pAD->staHistory->samples[0].signalStrength;
    range->minNoise = pAD->staHistory->samples[0].noise;
    range->maxNoise = pAD->staHistory->samples[0].noise;
    range->minSNR = pAD->staHistory->samples[0].signalStrength - pAD->staHistory->samples[0].noise;
    range->maxSNR = pAD->staHistory->samples[0].signalStrength - pAD->staHistory->samples[0].noise;

    for(uint8_t i = 1; i < nbValidSample; i++) {
        if(pAD->staHistory->samples[i].signalStrength < range->minRssi) {
            range->minRssi = pAD->staHistory->samples[i].signalStrength;
        } else if(pAD->staHistory->samples[i].signalStrength > range->maxRssi) {
            range->maxRssi = pAD->staHistory->samples[i].signalStrength;
        }
        if(pAD->staHistory->samples[i].noise < range->minNoise) {
            range->minNoise = pAD->staHistory->samples[i].noise;
        } else if(pAD->staHistory->samples[i].noise > range->maxNoise) {
            range->maxNoise = pAD->staHistory->samples[i].noise;
        }
        int32_t SNR = pAD->staHistory->samples[i].signalStrength - pAD->staHistory->samples[i].noise;
        if(SNR < range->minSNR) {
            range->minSNR = SNR;
        } else if(SNR > range->maxSNR) {
            range->maxSNR = SNR;
        }

    }

    return true;
}

void wld_apRssiMon_updateStaHistory(T_AccessPoint* pAP, T_AssociatedDevice* pAD) {
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(pAD, , ME, "NULL");

    uint32_t historyLen = pAP->rssiEventing.historyLen;
    uint32_t historyIntervalCoeff = pAP->rssiEventing.historyIntervalCoeff;
    if(pAD->AuthenticationState == 0) {
        wld_apRssiMon_cleanStaHistory(pAD->staHistory, historyLen);
        return;
    }

    swl_timeSpecReal_t ts;
    swl_timespec_getReal(&ts);
    pAD->staHistory->measurementTimestamp = ts;
    pAD->staHistory->signalStrength = pAD->SignalStrength;
    pAD->staHistory->noise = pAD->noise;
    pAD->staHistory->signalNoiseRatio = pAD->SignalNoiseRatio;

    if((pAD->staHistory->nr_valid_samples == 0) && (pAD->staHistory->measurementTimestampAssoc.tv_sec == 0)) {
        char buffer[50] = {'\0'};
        swl_timespec_realToDate(buffer, sizeof(buffer), &ts);
        pAD->staHistory->measurementTimestampAssoc = ts;
        pAD->staHistory->signalStrengthAssoc = pAD->SignalStrength;
        pAD->staHistory->noiseAssoc = pAD->noise;
        pAD->staHistory->signalNoiseRatioAssoc = pAD->SignalNoiseRatio;
    }

    if((pAP->historyCnt % historyIntervalCoeff) == 0) {

        bool firstFill = false;

        if(pAD->staHistory->nr_valid_samples > 0) {
            if(historyLen != 0) {
                pAD->staHistory->index_last_sample = (pAD->staHistory->index_last_sample + 1) % historyLen;
            }
        }
        if(pAD->staHistory->nr_valid_samples < historyLen) {
            pAD->staHistory->nr_valid_samples++;
            if(pAD->staHistory->nr_valid_samples == historyLen) {
                firstFill = true;
            }
        }


        wld_staHistory_t* sample = &pAD->staHistory->samples[pAD->staHistory->index_last_sample];

        sample->timestamp = swl_timespec_getMonoVal();
        sample->signalStrength = pAD->SignalStrength;
        sample->noise = pAD->noise;
        sample->dataDownlinkRate = pAD->LastDataDownlinkRate;
        sample->dataUplinkRate = pAD->LastDataUplinkRate;
        sample->txBytes = pAD->TxBytes;
        sample->rxBytes = pAD->RxBytes;
        sample->txPacketCount = pAD->TxPacketCount;
        sample->rxPacketCount = pAD->RxPacketCount;
        sample->txError = pAD->TxFailures;
        sample->rxError = pAD->RxFailures;
        sample->txFrameCount = pAD->TxFrameCount;
        sample->rxFrameCount = pAD->RxFrameCount;
        sample->tx_Retransmissions = pAD->Tx_Retransmissions;
        sample->tx_RetransmissionsFailed = pAD->Tx_RetransmissionsFailed;
        sample->txFrameCount = pAD->TxFrameCount;
        sample->rxFrameCount = pAD->RxFrameCount;
        sample->retryCount = pAD->retryCount;
        sample->multipleRetryCount = pAD->multipleRetryCount;
        sample->inactive = pAD->Inactive;
        sample->powerSave = pAD->powerSave;
        // mcs data
        sample->txLinkBandwidth = swl_chanspec_intToBw(pAD->DownlinkBandwidth);
        sample->txSpatialStream = pAD->downLinkRateSpec.numberOfSpatialStream;
        sample->txRateStandard = pAD->downLinkRateSpec.standard;
        sample->txMcsIndex = pAD->DownlinkMCS;
        sample->rxLinkBandwidth = swl_chanspec_intToBw(pAD->UplinkBandwidth);
        sample->rxSpatialStream = pAD->upLinkRateSpec.numberOfSpatialStream;
        sample->rxRateStandard = pAD->upLinkRateSpec.standard;
        sample->rxMcsIndex = pAD->UplinkMCS;

        if(firstFill) {
            amxc_var_t myMap;
            amxc_var_init(&myMap);
            amxc_var_set_type(&myMap, AMXC_VAR_ID_HTABLE);

            uint8_t nbValidSample = pAD->staHistory->nr_valid_samples;

            amxc_var_add_key(cstring_t, &myMap, "MACAddress", pAD->Name);
            amxc_var_add_key(uint8_t, &myMap, "Historylength", nbValidSample);
            if(nbValidSample > 0) {
                swl_tta_toMapOfChar(&myMap, historyNames, &gtWld_staHistory, pAD->staHistory->samples, (nbValidSample < historyLen) ? nbValidSample : historyLen);
            }

            wld_apRssiMon_sendHistoryOnAssocEvent(pAP, pAD, &myMap);
            amxc_var_clean(&myMap);
        }
    }
}


/**
 * Return the sta sample that is the "x oldest sample". So giving 0 will provide the oldest sample, providing -1 will
 * give the newest sample. Allowed range is -nr_valid_samples, nr_valid_samples -1. If not valid, NULL will be returned.
 */
wld_staHistory_t* wld_apRssiMon_getStaSampleIndexed(T_AccessPoint* pAP, T_AssociatedDevice* pAD, int32_t index) {
    ASSERT_NOT_NULL(pAD, NULL, ME, "NULL");
    ASSERTS_TRUE(pAP->rssiEventing.historyEnable, NULL, ME, "DISABLED");
    ASSERTS_TRUE(pAD->AuthenticationState, NULL, ME, "No auth");
    ASSERT_NOT_NULL(pAD->staHistory, NULL, ME, "NULL");


    uint32_t historyLen = pAP->rssiEventing.historyLen;
    uint8_t nbValidSample = pAD->staHistory->nr_valid_samples;
    uint8_t targetIndex = 0;

    if(index < 0) {
        if(index < -1 * nbValidSample) {
            return NULL;
        }
        if(nbValidSample == historyLen) {
            targetIndex = (pAD->staHistory->index_last_sample + 1 + historyLen + index) % historyLen;
        } else {
            targetIndex = nbValidSample + index;
        }
    } else {
        if(index >= nbValidSample) {
            return NULL;
        }
        if(nbValidSample == historyLen) {
            targetIndex = (pAD->staHistory->index_last_sample + 1 + historyLen + index) % historyLen;
        } else {
            targetIndex = index;
        }
    }

    return &pAD->staHistory->samples[targetIndex];
}

void wld_apRssiMon_getStaHistory(T_AccessPoint* pAP, const unsigned char macAddress[ETHER_ADDR_LEN], amxc_var_t* myMap) {
    ASSERT_NOT_NULL(pAP, , ME, "NULL");

    T_AssociatedDevice* pAD = NULL;
    pAD = wld_vap_find_asociatedDevice(pAP, (void*) macAddress);
    ASSERTS_NOT_NULL(pAD, , ME, "NULL");
    ASSERTS_TRUE(pAD->AuthenticationState > 0, , ME, "NULL");
    ASSERT_NOT_NULL(pAD->staHistory, , ME, "NULL");

    uint32_t historyLen = pAP->rssiEventing.historyLen;
    uint8_t nbValidSample = pAD->staHistory->nr_valid_samples;

    amxc_var_add_key(cstring_t, myMap, "MACAddress", pAD->Name);
    char buffer[50] = {'\0'};
    swl_timespec_realToDate(buffer, sizeof(buffer), &pAD->staHistory->measurementTimestamp);
    amxc_var_add_key(cstring_t, myMap, "MeasurementTimestamp", buffer);
    amxc_var_add_key(int32_t, myMap, "SignalStrength", pAD->staHistory->signalStrength);
    amxc_var_add_key(int32_t, myMap, "Noise", pAD->staHistory->noise);
    amxc_var_add_key(int32_t, myMap, "SignalNoiseRatio", pAD->staHistory->signalNoiseRatio);

    amxc_var_add_key(uint8_t, myMap, "Historylength", nbValidSample);

    if(nbValidSample > 0) {
        ssize_t offset = 0;
        if(nbValidSample == historyLen) {
            offset = (pAD->staHistory->index_last_sample + 1) % historyLen;
        }

        swl_tta_toMapOfCharOffset(myMap, historyNames, &gtWld_staHistory, pAD->staHistory->samples,
                                  (nbValidSample < historyLen) ? nbValidSample : historyLen, offset, -1);
    }
}

void wld_apRssiMon_sendHistoryOnAssocEvent(T_AccessPoint* pAP, T_AssociatedDevice* pAD, amxc_var_t* myVar) {
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(pAD, , ME, "NULL");

    bool enable = (pAP->rssiEventing.historyEnable == 1) &&
        (pAP->rssiEventing.sendEventOnAssoc == 1) &&
        (pAD->AuthenticationState == 1);

    if(enable == 0) {
        return;
    }

    amxd_object_t* eventObject = amxd_object_findf(pAP->pBus, "RssiEventing");

    char buffer[50] = {'\0'};
    swl_timespec_realToDate(buffer, sizeof(buffer), &pAD->staHistory->measurementTimestampAssoc);
    amxc_var_add_key(cstring_t, myVar, "MeasurementTimestamp", buffer);
    amxc_var_add_key(int32_t, myVar, "SignalStrength", pAD->staHistory->signalStrengthAssoc);
    amxc_var_add_key(int32_t, myVar, "Noise", pAD->staHistory->noiseAssoc);
    amxc_var_add_key(int32_t, myVar, "SignalNoiseRatio", pAD->staHistory->signalNoiseRatioAssoc);

    wld_mon_sendUpdateNotification(eventObject, "RssiUpdateShortHistoryAssoc", myVar);
    SAH_TRACEZ_INFO(ME, "%s: Short History Update after association", pAD->Name);
}

void wld_apRssiMon_sendHistoryOnDisassocEvent(T_AccessPoint* pAP, T_AssociatedDevice* pAD) {
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(pAD, , ME, "NULL");

    bool enable = (pAP->rssiEventing.historyEnable == 1) &&
        (pAP->rssiEventing.sendEventOnDisassoc == 1) &&
        (pAD->AuthenticationState == 1);

    if(enable) {
        amxc_var_t myVar;
        amxc_var_init(&myVar);
        amxc_var_set_type(&myVar, AMXC_VAR_ID_HTABLE);
        wld_apRssiMon_getStaHistory(pAP, pAD->MACAddress, &myVar);
        amxd_object_t* eventObject = amxd_object_findf(pAP->pBus, "RssiEventing");
        wld_mon_sendUpdateNotification(eventObject, "RssiUpdateShortHistoryDisassoc", &myVar);
        SAH_TRACEZ_INFO(ME, "%s: Short History update due to disassociation", pAD->Name);
        amxc_var_clean(&myVar);
    }
}

void wld_apRssiMon_sendStaHistoryAll(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, , ME, "NULL");

    if(pAP->rssiEventing.sendPeriodicEvent == 0) {
        return;
    }

    if(pAP->AssociatedDeviceNumberOfEntries == 0) {
        return;
    }

    if(pAP->historyCnt == 0) {
        amxc_var_t myList;
        amxc_var_init(&myList);
        amxc_var_set_type(&myList, AMXC_VAR_ID_LIST);
        int i;
        for(i = 0; i < pAP->AssociatedDeviceNumberOfEntries; i++) {
            T_AssociatedDevice* pAD = pAP->AssociatedDevice[i];
            if(pAD == NULL) {
                SAH_TRACEZ_ERROR(ME, "NULL");
                continue;
            }
            if(pAD->staHistory == NULL) {
                SAH_TRACEZ_ERROR(ME, "%s: Null short history buffer", (char*) pAD->Name);
                continue;
            }

            amxc_var_t varmap;
            amxc_var_init(&varmap);
            amxc_var_set_type(&varmap, AMXC_VAR_ID_HTABLE);
            if(pAD->staHistory->nr_valid_samples >= pAP->rssiEventing.historyLen) {
                wld_apRssiMon_getStaHistory(pAP, pAP->AssociatedDevice[i]->MACAddress, &varmap);
                amxc_var_add(amxc_htable_t, &myList, amxc_var_get_const_amxc_htable_t(&varmap));
            }
            amxc_var_clean(&varmap);
        }
        amxd_object_t* eventObject = amxd_object_findf(pAP->pBus, "RssiEventing");
        wld_mon_sendUpdateNotification(eventObject, "RssiUpdateShortHistory", &myList);
        SAH_TRACEZ_INFO(ME, "RssiUpdateShortHistorynotification sent for all stations");

        amxc_var_clean(&myList);
    }
}

/**
 * Gather the history information available for a station in the current system.
 */
amxd_status_t _getShortHistoryStats(amxd_object_t* obj_rssiEventing,
                                    amxd_function_t* func _UNUSED,
                                    amxc_var_t* args,
                                    amxc_var_t* retval) {
    SAH_TRACEZ_IN(ME);
    /* Check our input data */
    amxd_object_t* obj_vap = amxd_object_get_parent(obj_rssiEventing);
    T_AccessPoint* pAP = obj_vap->priv;

    ASSERT_TRUE(debugIsVapPointer(pAP), amxd_status_unknown_error, ME, "NULL");

    if(pAP->stationsStatsState.running) {
        SAH_TRACEZ_ERROR(ME, "A getShortHistoryStats is already running");
        SAH_TRACEZ_OUT(ME);
        return amxd_status_unknown_error;
    }

    const char* mac = GET_CHAR(args, "MAC");
    if(pAP->rssiEventing.historyEnable == 1) {
        if(mac != NULL) {
            swl_macBin_t mac_bin = SWL_MAC_BIN_NEW();
            SWL_MAC_CHAR_TO_BIN(&mac_bin, (swl_macBin_t*) mac);
            SAH_TRACEZ_INFO(ME, "MACBIN : <"SWL_MAC_FMT ">", SWL_MAC_ARG(mac_bin.bMac));
            amxc_var_init(retval);
            amxc_var_set_type(retval, AMXC_VAR_ID_HTABLE);
            wld_apRssiMon_getStaHistory(pAP, (unsigned char*) &mac_bin.bMac, retval);
        } else {
            amxc_var_init(retval);
            amxc_var_set_type(retval, AMXC_VAR_ID_LIST);
            int i;
            for(i = 0; i < pAP->AssociatedDeviceNumberOfEntries; i++) {
                amxc_var_t* varmap = amxc_var_add_new(retval);
                amxc_var_set_type(varmap, AMXC_VAR_ID_HTABLE);
                wld_apRssiMon_getStaHistory(pAP, pAP->AssociatedDevice[i]->MACAddress, varmap);
            }
        }
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

