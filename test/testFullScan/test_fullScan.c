/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2023 SoftAtHome
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

#include <stdarg.h>    // needed for cmocka
#include <sys/types.h> // needed for cmocka
#include <setjmp.h>    // needed for cmocka
#include <cmocka.h>
#include "wld.h"
#include "wld_radio.h"
#include "wld_util.h"
#include "test-toolbox/ttb_mockTimer.h"
#include "../testHelper/wld_th_mockVendor.h"
#include "../testHelper/wld_th_radio.h"
#include "../testHelper/wld_th_dm.h"
#include "swl/ttb/swl_ttb.h"
#include "wld_eventing.h"

static wld_th_dm_t dm;

void fillFullScanResultsList(amxc_var_t* pScanResultList, T_Radio* pRad, wld_scanResults_t* pScanResults);

#define NB_NEIGH_DEVICES 6
#define NB_BSS_PER_DEVICE 3
#define NB_SCAN_RESULTS (NB_NEIGH_DEVICES * NB_BSS_PER_DEVICE)
#define NB_SURROUNDING_CHANNELS 5
#define NB_SSID_PER_BSSID 1
swl_channel_t scanResChannels[SWL_FREQ_BAND_MAX][NB_SURROUNDING_CHANNELS] = {{1, 3, 6, 8, 11}, {36, 48, 60, 100, 108}, {1, 33, 49, 65, 81}};
static int s_setupSuite(void** state _UNUSED) {
    assert_true(wld_th_dm_init(&dm));
    T_Radio* rad;
    wld_scanResults_t* results;
    wld_for_eachRad(rad) {
        results = &rad->scanState.lastScanResults;
        for(int i = 0; i < NB_SCAN_RESULTS; i++) {
            wld_scanResultSSID_t* item = calloc(1, sizeof(wld_scanResultSSID_t));
            assert_non_null(item);
            item->ssidLen = snprintf((char*) item->ssid, sizeof(item->ssid), "ssid_%d", i);
            int devIdx = i % NB_NEIGH_DEVICES;
            // set distinct 2 bytes per device to create own AP device instance in scan resuls
            item->bssid.bMac[3] = item->bssid.bMac[4] = devIdx;
            item->bssid.bMac[5] = (((rad->ref_index << 4) & 0xf0) | i) & 0xff;
            item->rssi = -40 + ((devIdx % 2) ? devIdx : -devIdx);  //around rssi -40
            item->noise = -80 - ((devIdx % 2) ? devIdx : -devIdx); //around noise -80
            item->channel = scanResChannels[rad->operatingFrequencyBand][devIdx % NB_SURROUNDING_CHANNELS];
            item->centreChannel = item->channel;
            item->operClass = 100;    //arbitrary value
            item->bandwidth = 20;
            item->operatingStandards = M_SWL_RADSTD_AUTO;
            if(rad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_6GHZ) {
                item->operatingStandards = M_SWL_RADSTD_AX;
            } else if(rad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_5GHZ) {
                item->operatingStandards = M_SWL_RADSTD_A;
                if(devIdx % 4) {
                    item->operatingStandards |= M_SWL_RADSTD_AX | M_SWL_RADSTD_AC | M_SWL_RADSTD_N;
                } else if(devIdx % 3) {
                    item->operatingStandards |= M_SWL_RADSTD_AC | M_SWL_RADSTD_N;
                } else if(devIdx % 2) {
                    item->operatingStandards |= M_SWL_RADSTD_N;
                }
            } else if(rad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_2_4GHZ) {
                item->operatingStandards = M_SWL_RADSTD_B | M_SWL_RADSTD_G;
                if(devIdx % 3) {
                    item->operatingStandards |= M_SWL_RADSTD_AX | M_SWL_RADSTD_N;
                } else if(devIdx % 2) {
                    item->operatingStandards |= M_SWL_RADSTD_N;
                }
            }
            amxc_llist_append(&results->ssids, &item->it);
        }
    }
    return 0;
}

static int s_teardownSuite(void** state _UNUSED) {
    wld_th_dm_destroy(&dm);
    return 0;
}
//Test that the FullScan fill a result list with the expected values
static void test_startAndGetScan(void** state _UNUSED) {
    T_Radio* pRad = dm.bandList[SWL_FREQ_BAND_2_4GHZ].rad;

    amxd_object_t* config = amxd_object_findf(pRad->pBus, "ScanConfig");
    amxd_trans_t trans;
    assert_int_equal(swl_object_prepareTransaction(&trans, config), SWL_RC_OK);
    amxd_trans_set_bool(&trans, "EnableScanResultsDm", true);
    assert_int_equal(swl_object_finalizeTransactionOnLocalDm(&trans), SWL_RC_OK);

    ttb_var_t* replyVar;
    ttb_var_t* args = ttb_object_createArgs();
    assert_non_null(args);
    amxc_var_set_type(args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(uint32_t, args, "DwellTime", 50);
    amxc_var_add_key(uint32_t, args, "DFDSDwellTime", 50);
    ttb_reply_t* reply = ttb_object_callFun(dm.ttbBus, pRad->pBus, "FullScan", &args, &replyVar);
    assert_true(reply != NULL);
    ttb_object_cleanReply(&reply, &replyVar);
    assert_true(wld_scan_isRunning(pRad));
    ttb_mockTimer_goToFutureMs(1000);

    wld_scan_done(pRad, true);


    wld_scanResults_t res;
    amxc_llist_init(&res.ssids);
    assert_true(pRad->pFA->mfn_wrad_scan_results(pRad, &res) != SWL_RC_NOT_IMPLEMENTED);

    amxc_var_t retMap;
    amxc_var_init(&retMap);
    amxc_var_set_type(&retMap, AMXC_VAR_ID_HTABLE);


    amxc_var_t* pScanResultList_amx = amxc_var_add_key(amxc_llist_t, &retMap, "ScanResult", NULL);
    fillFullScanResultsList(pScanResultList_amx, pRad, &res);
    //amxc_var_dump(pScanResultList_amx, STDOUT_FILENO);

    assert_int_equal(amxc_var_type_of(pScanResultList_amx), AMXC_VAR_ID_LIST);
    const amxc_llist_t* pScanResultListObject = amxc_var_constcast(amxc_llist_t, pScanResultList_amx);
    assert_int_equal(amxc_llist_size(pScanResultListObject), 1);

    amxc_llist_it_t* first_element_it = amxc_llist_get_first(pScanResultListObject);
    amxc_var_t* pScanResultObject = amxc_var_from_llist_it(first_element_it);
    assert_ptr_not_equal(pScanResultObject, NULL);

    amxc_var_t* opClassScan_amx = amxc_var_get_key(pScanResultObject, "OpClassScan", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(opClassScan_amx), AMXC_VAR_ID_LIST);

    const amxc_llist_t* opClassScan_object = amxc_var_constcast(amxc_llist_t, opClassScan_amx);
    assert_int_equal(amxc_llist_size(opClassScan_object), 1);
    amxc_llist_it_t* firstPpClassAmx = amxc_llist_get_first(opClassScan_object);
    amxc_var_t* firstOpClassObject = amxc_var_from_llist_it(firstPpClassAmx);

    //OperatingClass
    amxc_var_t* operatingClassAMX = amxc_var_get_key(firstOpClassObject, "OperatingClass", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(operatingClassAMX), AMXC_VAR_ID_UINT8);
    assert_true(amxc_var_dyncast(uint8_t, operatingClassAMX) == 100);

    //ChannelScan
    amxc_var_t* channelScanAmx = amxc_var_get_key(firstOpClassObject, "ChannelScan", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(channelScanAmx), AMXC_VAR_ID_LIST);
    const amxc_llist_t* channelScanObject = amxc_var_constcast(amxc_llist_t, channelScanAmx);
    assert_int_equal(amxc_llist_size(channelScanObject), 5); // for the first radio, 5 channels are scanned
    amxc_llist_it_t* firstChannelScanAmx = amxc_llist_get_first(channelScanObject);
    amxc_var_t* firstChannelScanObject = amxc_var_from_llist_it(firstChannelScanAmx);
    assert_ptr_not_equal(firstChannelScanObject, NULL);

    //Channel
    amxc_var_t* chanelObjectAmx = amxc_var_get_key(firstChannelScanObject, "Channel", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(chanelObjectAmx), AMXC_VAR_ID_UINT8);
    assert_true(amxc_var_dyncast(uint8_t, chanelObjectAmx) == 1);
    amxc_var_t* timeStampAmx = amxc_var_get_key(firstChannelScanObject, "TimeStamp", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(timeStampAmx), AMXC_VAR_ID_CSTRING);
    amxc_var_t* utiliatizationAmx = amxc_var_get_key(firstChannelScanObject, "Utilization", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(utiliatizationAmx), AMXC_VAR_ID_UINT8);
    amxc_var_t* noiseAmx = amxc_var_get_key(firstChannelScanObject, "Noise", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(noiseAmx), AMXC_VAR_ID_UINT8);

    //NeighborBSS
    amxc_var_t* neighborBssAmx = amxc_var_get_key(firstChannelScanObject, "NeighborBSS", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(neighborBssAmx), AMXC_VAR_ID_LIST);
    const amxc_llist_t* neighborBssObject = amxc_var_constcast(amxc_llist_t, neighborBssAmx);
    assert_int_equal(amxc_llist_size(neighborBssObject), 6);

    //first neighbor BSS
    amxc_llist_it_t* firstNeighborBssAmx = amxc_llist_get_first(neighborBssObject);
    amxc_var_t* firstNeighborBssObject = amxc_var_from_llist_it(firstNeighborBssAmx);
    assert_ptr_not_equal(firstNeighborBssObject, NULL);
    amxc_var_t* bssidAmx = amxc_var_get_key(firstNeighborBssObject, "BSSID", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(bssidAmx), AMXC_VAR_ID_CSTRING);
    amxc_var_t* ssidAmx = amxc_var_get_key(firstNeighborBssObject, "SSID", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(ssidAmx), AMXC_VAR_ID_CSTRING);
    amxc_var_t* signalStrengthAmx = amxc_var_get_key(firstNeighborBssObject, "SignalStrength", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(signalStrengthAmx), AMXC_VAR_ID_UINT8);
    amxc_var_t* channelBandwidthAmx = amxc_var_get_key(firstNeighborBssObject, "ChannelBandwidth", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(channelBandwidthAmx), AMXC_VAR_ID_CSTRING);
    amxc_var_t* channelUtilizationAmx = amxc_var_get_key(firstNeighborBssObject, "ChannelUtilization", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(channelUtilizationAmx), AMXC_VAR_ID_UINT8);
    amxc_var_t* stationCountAmx = amxc_var_get_key(firstNeighborBssObject, "StationCount", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(stationCountAmx), AMXC_VAR_ID_UINT32);
    amxc_var_t* securityModeEnabledAmx = amxc_var_get_key(firstNeighborBssObject, "SecurityModeEnabled", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(securityModeEnabledAmx), AMXC_VAR_ID_CSTRING);
    amxc_var_t* encryptionModeAmx = amxc_var_get_key(firstNeighborBssObject, "EncryptionMode", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(encryptionModeAmx), AMXC_VAR_ID_CSTRING);
    amxc_var_t* supportedStandardsAmx = amxc_var_get_key(firstNeighborBssObject, "SupportedStandards", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(supportedStandardsAmx), AMXC_VAR_ID_CSTRING);
    amxc_var_t* operatingStandardsAmx = amxc_var_get_key(firstNeighborBssObject, "OperatingStandards", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(operatingStandardsAmx), AMXC_VAR_ID_CSTRING);
    amxc_var_t* basicDataTransferRatesAmx = amxc_var_get_key(firstNeighborBssObject, "BasicDataTransferRates", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(basicDataTransferRatesAmx), AMXC_VAR_ID_CSTRING);
    amxc_var_t* supportedDataTransferRatesAmx = amxc_var_get_key(firstNeighborBssObject, "SupportedDataTransferRates", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(supportedDataTransferRatesAmx), AMXC_VAR_ID_CSTRING);
    amxc_var_t* SupportedNSSAmx = amxc_var_get_key(firstNeighborBssObject, "SupportedNSS", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(SupportedNSSAmx), AMXC_VAR_ID_UINT32);
    amxc_var_t* DtimPeriodAmx = amxc_var_get_key(firstNeighborBssObject, "DTIMPeriod", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(DtimPeriodAmx), AMXC_VAR_ID_UINT32);
    amxc_var_t* BeaconPeriodAmx = amxc_var_get_key(firstNeighborBssObject, "BeaconPeriod", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(BeaconPeriodAmx), AMXC_VAR_ID_UINT32);

    amxc_var_clean(&retMap);

    //Check that scan results are corrects
    amxc_llist_for_each(it, &res.ssids) {
        wld_scanResultSSID_t* pSSID = amxc_container_of(it, wld_scanResultSSID_t, it);
        assert_non_null(pSSID);
        assert_string_not_equal(pSSID->ssid, "");
        assert_true(wld_rad_hasChannel(pRad, pSSID->channel));
    }
    uint32_t totalResults = amxc_llist_size(&res.ssids);
    uint32_t match = 0;
    ttb_mockTimer_goToFutureMs(100);
    amxd_object_t* chan_template = amxd_object_findf(pRad->pBus, "ScanResults.SurroundingChannels");
    assert_int_equal(amxd_object_get_instance_count(chan_template), NB_SURROUNDING_CHANNELS);
    amxd_object_for_each(instance, it, chan_template) {
        amxd_object_t* chan_obj = amxc_llist_it_get_data(it, amxd_object_t, it);
        uint16_t channel = amxd_object_get_uint16_t(chan_obj, "Channel", NULL);
        amxd_object_t* ap_template = amxd_object_get(chan_obj, "Accesspoint");
        amxd_object_for_each(instance, itAp, ap_template) {
            amxd_object_t* ap_obj = amxc_llist_it_get_data(itAp, amxd_object_t, it);
            amxd_object_t* apSsid_template = amxd_object_get(ap_obj, "SSID");
            assert_int_equal(amxd_object_get_instance_count(apSsid_template), NB_SSID_PER_BSSID);
            amxd_object_for_each(instance, itApSsid, apSsid_template) {
                amxd_object_t* apSsid_obj = amxc_llist_it_get_data(itApSsid, amxd_object_t, it);
                char* ssid = amxd_object_get_cstring_t(apSsid_obj, "SSID", NULL);
                char* bssid = amxd_object_get_cstring_t(apSsid_obj, "BSSID", NULL);
                swl_macBin_t bssidBin = SWL_MAC_BIN_NEW();
                SWL_MAC_CHAR_TO_BIN(&bssidBin, bssid);
                amxc_llist_for_each(itScanSSID, &res.ssids) {
                    wld_scanResultSSID_t* pSSID = amxc_container_of(itScanSSID, wld_scanResultSSID_t, it);
                    if((swl_str_nmatches((char*) pSSID->ssid, ssid, pSSID->ssidLen)) &&
                       (pSSID->channel == channel) &&
                       (SWL_MAC_BIN_MATCHES(&pSSID->bssid, &bssidBin)) &&
                       (pSSID->operClass == 100)
                       ) {
                        match += 1;
                    }
                }
                free(bssid);
                free(ssid);
            }
        }
    }
    wld_scan_cleanupScanResults(&res);
    assert_int_equal(totalResults, match);
    assert_int_equal(totalResults, NB_SCAN_RESULTS);
}



static void test_filterScanResultsSSIDNotExisting(void** state _UNUSED) {
    T_Radio* pRad = dm.bandList[SWL_FREQ_BAND_2_4GHZ].rad;
    ttb_var_t* replyVar;
    ttb_var_t* args = ttb_object_createArgs();
    assert_non_null(args);
    amxc_var_set_type(args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(uint32_t, args, "DwellTime", 50);
    amxc_var_add_key(uint32_t, args, "DFDSDwellTime", 50);
    const char* ssidFilterValue = "non_existing_ssid";//just random existing value
    amxc_var_add_key(cstring_t, args, "SSID", ssidFilterValue);
    ttb_reply_t* reply = ttb_object_callFun(dm.ttbBus, pRad->pBus, "FullScan", &args, &replyVar);
    assert_true(reply != NULL);
    ttb_object_cleanReply(&reply, &replyVar);
    assert_true(wld_scan_isRunning(pRad));
    ttb_mockTimer_goToFutureMs(1000);

    wld_scan_done(pRad, true);


    wld_scanResults_t res;
    amxc_llist_init(&res.ssids);
    assert_true(pRad->pFA->mfn_wrad_scan_results(pRad, &res) != SWL_RC_NOT_IMPLEMENTED);

    amxc_var_t retMap;
    amxc_var_init(&retMap);
    amxc_var_set_type(&retMap, AMXC_VAR_ID_HTABLE);

    amxc_var_t* pScanResultList_amx = amxc_var_add_key(amxc_llist_t, &retMap, "ScanResult", NULL);
    fillFullScanResultsList(pScanResultList_amx, pRad, &res);
    //amxc_var_dump(pScanResultList_amx, STDOUT_FILENO);

    assert_int_equal(amxc_var_type_of(pScanResultList_amx), AMXC_VAR_ID_LIST);
    const amxc_llist_t* pScanResultListObject = amxc_var_constcast(amxc_llist_t, pScanResultList_amx);
    assert_int_equal(amxc_llist_size(pScanResultListObject), 1);

    amxc_llist_it_t* first_element_it = amxc_llist_get_first(pScanResultListObject);
    amxc_var_t* pScanResultObject = amxc_var_from_llist_it(first_element_it);
    assert_ptr_not_equal(pScanResultObject, NULL);

    amxc_var_t* opClassScan_amx = amxc_var_get_key(pScanResultObject, "OpClassScan", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(opClassScan_amx), AMXC_VAR_ID_LIST);

    const amxc_llist_t* opClassScan_object = amxc_var_constcast(amxc_llist_t, opClassScan_amx);
    assert_int_equal(amxc_llist_size(opClassScan_object), 0); // SSID does not match do shall be empty

    amxc_var_clean(&retMap);

}



static void test_filterScanResultsFilterSSID(void** state _UNUSED) {
    T_Radio* pRad = dm.bandList[SWL_FREQ_BAND_2_4GHZ].rad;
    ttb_var_t* replyVar;
    ttb_var_t* args = ttb_object_createArgs();
    assert_non_null(args);
    amxc_var_set_type(args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(uint32_t, args, "DwellTime", 50);
    amxc_var_add_key(uint32_t, args, "DFDSDwellTime", 50);
    const char* ssidFilterValue = "ssid_8";//just random existing value
    amxc_var_add_key(cstring_t, args, "SSID", ssidFilterValue);
    ttb_reply_t* reply = ttb_object_callFun(dm.ttbBus, pRad->pBus, "FullScan", &args, &replyVar);
    assert_true(reply != NULL);
    ttb_object_cleanReply(&reply, &replyVar);
    assert_true(wld_scan_isRunning(pRad));
    ttb_mockTimer_goToFutureMs(1000);

    wld_scan_done(pRad, true);


    wld_scanResults_t res;
    amxc_llist_init(&res.ssids);
    assert_true(pRad->pFA->mfn_wrad_scan_results(pRad, &res) != SWL_RC_NOT_IMPLEMENTED);

    amxc_var_t retMap;
    amxc_var_init(&retMap);
    amxc_var_set_type(&retMap, AMXC_VAR_ID_HTABLE);


    amxc_var_t* pScanResultList_amx = amxc_var_add_key(amxc_llist_t, &retMap, "ScanResult", NULL);
    fillFullScanResultsList(pScanResultList_amx, pRad, &res);
    amxc_var_dump(pScanResultList_amx, STDOUT_FILENO);

    assert_int_equal(amxc_var_type_of(pScanResultList_amx), AMXC_VAR_ID_LIST);
    const amxc_llist_t* pScanResultListObject = amxc_var_constcast(amxc_llist_t, pScanResultList_amx);
    assert_int_equal(amxc_llist_size(pScanResultListObject), 1);

    amxc_llist_it_t* first_element_it = amxc_llist_get_first(pScanResultListObject);
    amxc_var_t* pScanResultObject = amxc_var_from_llist_it(first_element_it);
    assert_ptr_not_equal(pScanResultObject, NULL);

    amxc_var_t* opClassScan_amx = amxc_var_get_key(pScanResultObject, "OpClassScan", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(opClassScan_amx), AMXC_VAR_ID_LIST);

    const amxc_llist_t* opClassScan_object = amxc_var_constcast(amxc_llist_t, opClassScan_amx);
    assert_int_equal(amxc_llist_size(opClassScan_object), 1);
    amxc_llist_it_t* firstPpClassAmx = amxc_llist_get_first(opClassScan_object);
    amxc_var_t* firstOpClassObject = amxc_var_from_llist_it(firstPpClassAmx);

    //OperatingClass
    amxc_var_t* operatingClassAMX = amxc_var_get_key(firstOpClassObject, "OperatingClass", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(operatingClassAMX), AMXC_VAR_ID_UINT8);
    assert_true(amxc_var_dyncast(uint8_t, operatingClassAMX) == 100);

    //ChannelScan
    amxc_var_t* channelScanAmx = amxc_var_get_key(firstOpClassObject, "ChannelScan", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(channelScanAmx), AMXC_VAR_ID_LIST);
    const amxc_llist_t* channelScanObject = amxc_var_constcast(amxc_llist_t, channelScanAmx);
    assert_int_equal(amxc_llist_size(channelScanObject), 1); // for the first radio, 5 channels are scanned
    amxc_llist_it_t* firstChannelScanAmx = amxc_llist_get_first(channelScanObject);
    amxc_var_t* firstChannelScanObject = amxc_var_from_llist_it(firstChannelScanAmx);
    assert_ptr_not_equal(firstChannelScanObject, NULL);

    //Channel
    amxc_var_t* chanelObjectAmx = amxc_var_get_key(firstChannelScanObject, "Channel", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(chanelObjectAmx), AMXC_VAR_ID_UINT8);
    assert_int_equal(amxc_var_dyncast(uint8_t, chanelObjectAmx), 6);
    amxc_var_t* timeStampAmx = amxc_var_get_key(firstChannelScanObject, "TimeStamp", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(timeStampAmx), AMXC_VAR_ID_CSTRING);
    amxc_var_t* utiliatizationAmx = amxc_var_get_key(firstChannelScanObject, "Utilization", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(utiliatizationAmx), AMXC_VAR_ID_UINT8);
    amxc_var_t* noiseAmx = amxc_var_get_key(firstChannelScanObject, "Noise", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(noiseAmx), AMXC_VAR_ID_UINT8);

    //NeighborBSS
    amxc_var_t* neighborBssAmx = amxc_var_get_key(firstChannelScanObject, "NeighborBSS", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(neighborBssAmx), AMXC_VAR_ID_LIST);
    const amxc_llist_t* neighborBssObject = amxc_var_constcast(amxc_llist_t, neighborBssAmx);
    assert_int_equal(amxc_llist_size(neighborBssObject), 1);

    //first neighbor BSS
    amxc_llist_it_t* firstNeighborBssAmx = amxc_llist_get_first(neighborBssObject);
    amxc_var_t* firstNeighborBssObject = amxc_var_from_llist_it(firstNeighborBssAmx);
    assert_ptr_not_equal(firstNeighborBssObject, NULL);
    amxc_var_t* bssidAmx = amxc_var_get_key(firstNeighborBssObject, "BSSID", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(bssidAmx), AMXC_VAR_ID_CSTRING);
    amxc_var_t* ssidAmx = amxc_var_get_key(firstNeighborBssObject, "SSID", AMXC_VAR_FLAG_DEFAULT);
    char* ssid = amxc_var_dyncast(cstring_t, ssidAmx);
    assert_string_equal(ssid, "ssid_8");
    assert_int_equal(amxc_var_type_of(ssidAmx), AMXC_VAR_ID_CSTRING);
    amxc_var_t* signalStrengthAmx = amxc_var_get_key(firstNeighborBssObject, "SignalStrength", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(signalStrengthAmx), AMXC_VAR_ID_UINT8);
    amxc_var_t* channelBandwidthAmx = amxc_var_get_key(firstNeighborBssObject, "ChannelBandwidth", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(channelBandwidthAmx), AMXC_VAR_ID_CSTRING);
    amxc_var_t* channelUtilizationAmx = amxc_var_get_key(firstNeighborBssObject, "ChannelUtilization", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(channelUtilizationAmx), AMXC_VAR_ID_UINT8);
    amxc_var_t* stationCountAmx = amxc_var_get_key(firstNeighborBssObject, "StationCount", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(stationCountAmx), AMXC_VAR_ID_UINT32);
    amxc_var_t* securityModeEnabledAmx = amxc_var_get_key(firstNeighborBssObject, "SecurityModeEnabled", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(securityModeEnabledAmx), AMXC_VAR_ID_CSTRING);
    amxc_var_t* encryptionModeAmx = amxc_var_get_key(firstNeighborBssObject, "EncryptionMode", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(encryptionModeAmx), AMXC_VAR_ID_CSTRING);
    amxc_var_t* supportedStandardsAmx = amxc_var_get_key(firstNeighborBssObject, "SupportedStandards", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(supportedStandardsAmx), AMXC_VAR_ID_CSTRING);
    amxc_var_t* operatingStandardsAmx = amxc_var_get_key(firstNeighborBssObject, "OperatingStandards", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(operatingStandardsAmx), AMXC_VAR_ID_CSTRING);
    amxc_var_t* basicDataTransferRatesAmx = amxc_var_get_key(firstNeighborBssObject, "BasicDataTransferRates", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(basicDataTransferRatesAmx), AMXC_VAR_ID_CSTRING);
    amxc_var_t* supportedDataTransferRatesAmx = amxc_var_get_key(firstNeighborBssObject, "SupportedDataTransferRates", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(supportedDataTransferRatesAmx), AMXC_VAR_ID_CSTRING);
    amxc_var_t* SupportedNSSAmx = amxc_var_get_key(firstNeighborBssObject, "SupportedNSS", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(SupportedNSSAmx), AMXC_VAR_ID_UINT32);
    amxc_var_t* DtimPeriodAmx = amxc_var_get_key(firstNeighborBssObject, "DTIMPeriod", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(DtimPeriodAmx), AMXC_VAR_ID_UINT32);
    amxc_var_t* BeaconPeriodAmx = amxc_var_get_key(firstNeighborBssObject, "BeaconPeriod", AMXC_VAR_FLAG_DEFAULT);
    assert_int_equal(amxc_var_type_of(BeaconPeriodAmx), AMXC_VAR_ID_UINT32);

    amxc_var_clean(&retMap);
    free(ssid);
    wld_scan_cleanupScanResults(&res);

}





int main(int argc _UNUSED, char* argv[] _UNUSED) {
    sahTraceSetLevel(TRACE_LEVEL_INFO);
    sahTraceAddZone(TRACE_LEVEL_APP_INFO, "ssid");
    ttb_util_setFilter();
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_startAndGetScan),
        cmocka_unit_test(test_filterScanResultsSSIDNotExisting),
        cmocka_unit_test(test_filterScanResultsFilterSSID)
    };
    int rc = cmocka_run_group_tests(tests, s_setupSuite, s_teardownSuite);
    sahTraceClose();
    return rc;
}

