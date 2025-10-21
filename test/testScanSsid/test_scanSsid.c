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
#include "swla/ttb/swla_ttbVariant.h"

static wld_th_dm_t dm;

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
            for(uint32_t i = 0; i < 2; i++) {
                wld_vendorIe_t* vendorIe = calloc(1, sizeof(wld_vendorIe_t));
                vendorIe->frame_type = M_VENDOR_IE_BEACON;
                char* tmp = (i == 0 ? "AA:BB:00" : "AA:BB:11");
                memcpy(vendorIe->oui, tmp, sizeof(vendorIe->oui));
                amxc_llist_append(&item->vendorIEs, &vendorIe->it);
            }

            amxc_llist_append(&results->ssids, &item->it);
        }
        amxd_object_t* config = amxd_object_findf(rad->pBus, "ScanConfig");
        amxd_trans_t trans;
        assert_int_equal(swl_object_prepareTransaction(&trans, config), SWL_RC_OK);
        amxd_trans_set_bool(&trans, "EnableScanResultsDm", true);
        assert_int_equal(swl_object_finalizeTransactionOnLocalDm(&trans), SWL_RC_OK);
    }
    return 0;
}

static int s_teardownSuite(void** state _UNUSED) {
    wld_th_dm_destroy(&dm);
    return 0;
}

static void s_startScan(T_Radio* pRad, ttb_var_t** args) {
    ttb_var_t* replyVar;
    ttb_reply_t* reply = ttb_object_callFun(dm.ttbBus, pRad->pBus, "startScan", args, &replyVar);
    assert_true(ttb_object_replySuccess(reply));
    ttb_object_cleanReply(&reply, &replyVar);
    ttb_mockTimer_goToFutureMs(100);
    assert_true(wld_scan_isRunning(pRad));
    ttb_mockTimer_goToFutureMs(1000);
    wld_scan_done(pRad, true);
    ttb_mockTimer_goToFutureMs(100);
    assert_false(wld_scan_isRunning(pRad));
}

static void test_startAndGetScan(void** state _UNUSED) {
    T_Radio* pRad = dm.bandList[SWL_FREQ_BAND_2_4GHZ].rad;

    s_startScan(pRad, NULL);

    wld_scanResults_t res;
    amxc_llist_init(&res.ssids);
    assert_true(pRad->pFA->mfn_wrad_scan_results(pRad, &res) != SWL_RC_NOT_IMPLEMENTED);
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
                       (SWL_MAC_BIN_MATCHES(&pSSID->bssid, &bssidBin))) {
                        match += 1;
                    }
                }
                free(bssid);
                free(ssid);
            }
        }
    }
    wld_radio_scanresults_cleanup(&res);
    assert_int_equal(totalResults, match);
    assert_int_equal(totalResults, NB_SCAN_RESULTS);
}



static void test_filterScanResultsBasedOnSSID(void** state _UNUSED) {
    T_Radio* pRad = dm.bandList[SWL_FREQ_BAND_2_4GHZ].rad;

    ttb_var_t* args = ttb_object_createArgs();
    assert_non_null(args);
    amxc_var_set_type(args, AMXC_VAR_ID_HTABLE);
    const char* ssidFilterValue = "ssid_8";//just random existing value
    amxc_var_add_key(cstring_t, args, "SSID", ssidFilterValue);

    s_startScan(pRad, &args);

    ttb_var_t* scanResultsVar = NULL;
    ttb_reply_t* reply = ttb_object_callFun(dm.ttbBus, pRad->pBus, "getScanResults", NULL, &scanResultsVar);
    assert_true(ttb_object_replySuccess(reply));
    assert_non_null(scanResultsVar);

    const amxc_llist_t* allResults = amxc_var_constcast(amxc_llist_t, scanResultsVar);
    assert_non_null(allResults);
    amxc_llist_for_each(it, allResults) {
        const amxc_htable_t* ssidScanResults = amxc_var_constcast(amxc_htable_t, amxc_var_from_llist_it(it));
        assert_non_null(ssidScanResults);
        char* ssid_result = NULL;
        amxc_htable_it_t* hit = NULL;
        amxc_var_t* ht_data = NULL;
        hit = amxc_htable_get(ssidScanResults, "SSID");
        ht_data = amxc_var_from_htable_it(hit);
        ssid_result = amxc_var_dyncast(cstring_t, ht_data);
        assert_non_null(ssid_result);
        assert_string_equal(ssid_result, ssidFilterValue);
        free(ssid_result);
    }
    ttb_object_cleanReply(&reply, &scanResultsVar);
}


static void test_filterScanResultsBasedOnBSSID(void** state _UNUSED) {
    T_Radio* pRad = dm.bandList[SWL_FREQ_BAND_2_4GHZ].rad;

    ttb_var_t* args = ttb_object_createArgs();
    assert_non_null(args);
    amxc_var_set_type(args, AMXC_VAR_ID_HTABLE);
    const char* bssidFilterValue = "00:00:00:04:04:0A";//just random existing value
    amxc_var_add_key(cstring_t, args, "BSSID", bssidFilterValue);

    s_startScan(pRad, &args);

    ttb_var_t* scanResultsVar = NULL;
    ttb_reply_t* reply = ttb_object_callFun(dm.ttbBus, pRad->pBus, "getScanResults", NULL, &scanResultsVar);
    assert_true(ttb_object_replySuccess(reply));
    assert_non_null(scanResultsVar);

    const amxc_llist_t* allResults = amxc_var_constcast(amxc_llist_t, scanResultsVar);
    assert_non_null(allResults);
    amxc_llist_for_each(it, allResults) {
        const amxc_htable_t* ssidScanResults = amxc_var_constcast(amxc_htable_t, amxc_var_from_llist_it(it));
        assert_non_null(ssidScanResults);
        char* bssid_result = NULL;
        amxc_htable_it_t* hit = NULL;
        amxc_var_t* ht_data = NULL;
        hit = amxc_htable_get(ssidScanResults, "BSSID");
        ht_data = amxc_var_from_htable_it(hit);
        bssid_result = amxc_var_dyncast(cstring_t, ht_data);
        assert_non_null(bssid_result);
        assert_string_equal(bssid_result, bssidFilterValue);
        free(bssid_result);
    }
    ttb_object_cleanReply(&reply, &scanResultsVar);
}


static void test_filterScanResultsBasedOnChannel(void** state _UNUSED) {
    T_Radio* pRad = dm.bandList[SWL_FREQ_BAND_2_4GHZ].rad;
    ttb_var_t* args = ttb_object_createArgs();
    assert_non_null(args);
    amxc_var_set_type(args, AMXC_VAR_ID_HTABLE);
    const char* channelFilterValue = "1";//just random existing value
    amxc_var_add_key(cstring_t, args, "channels", channelFilterValue);

    s_startScan(pRad, &args);

    ttb_var_t* scanResultsVar = NULL;
    ttb_reply_t* reply = ttb_object_callFun(dm.ttbBus, pRad->pBus, "getScanResults", NULL, &scanResultsVar);
    assert_true(ttb_object_replySuccess(reply));
    assert_non_null(scanResultsVar);

    const amxc_llist_t* allResults = amxc_var_constcast(amxc_llist_t, scanResultsVar);
    assert_non_null(allResults);
    char* ssid_channel = NULL;
    amxc_llist_for_each(it, allResults) {
        const amxc_htable_t* ssidScanResults = amxc_var_constcast(amxc_htable_t, amxc_var_from_llist_it(it));
        assert_non_null(ssidScanResults);
        amxc_htable_it_t* hit = NULL;
        amxc_var_t* ht_data = NULL;
        hit = amxc_htable_get(ssidScanResults, "Channel");
        ht_data = amxc_var_from_htable_it(hit);
        ssid_channel = amxc_var_dyncast(cstring_t, ht_data);
        assert_string_equal(ssid_channel, channelFilterValue);
        free(ssid_channel);
    }
    ttb_object_cleanReply(&reply, &scanResultsVar);

}

/**
 * Initial list of mock devices
 */
static void test_resultsAll(void** state _UNUSED) {
    T_Radio* pRad = dm.bandList[SWL_FREQ_BAND_5GHZ].rad;

    s_startScan(pRad, NULL);

    amxd_object_t* dmResults = amxd_object_findf(pRad->pBus, "ScanResults");
    ttb_object_assertPrintEqFile(dmResults, 0, "ScanResults-5g-1.txt");
}

/**
 * Change RSSI of every device on the list
 */
static void test_resultsChangeRSSI(void** state _UNUSED) {
    T_Radio* pRad = dm.bandList[SWL_FREQ_BAND_5GHZ].rad;

    wld_scanResults_t* results;
    results = &pRad->scanState.lastScanResults;
    int number = 0;
    amxc_llist_for_each(itScanSSID, &results->ssids) {
        wld_scanResultSSID_t* pSSID = amxc_container_of(itScanSSID, wld_scanResultSSID_t, it);
        pSSID->rssi -= ++number;
    }

    s_startScan(pRad, NULL);

    amxd_object_t* dmResults = amxd_object_findf(pRad->pBus, "ScanResults");
    ttb_object_assertPrintEqFile(dmResults, 0, "ScanResults-5g-2.txt");
}

/**
 * Add new devices to the channel of the DUT's AP
 */
static void test_resultsAddDevicesToExistingChannel(void** state _UNUSED) {
    T_Radio* pRad = dm.bandList[SWL_FREQ_BAND_5GHZ].rad;

    wld_scanResults_t* results = &pRad->scanState.lastScanResults;

    for(int i = 0; i < 3; i++) {
        wld_scanResultSSID_t* item = calloc(1, sizeof(wld_scanResultSSID_t));
        assert_non_null(item);
        item->ssidLen = snprintf((char*) item->ssid, sizeof(item->ssid), "new-ssid_%d", i);
        int devIdx = i % NB_NEIGH_DEVICES;
        // set distinct 2 bytes per device to create own AP device instance in scan resuls
        item->bssid.bMac[3] = item->bssid.bMac[4] = devIdx + 0xa0;
        item->bssid.bMac[5] = (((pRad->ref_index << 4) & 0xf0) | i) & 0xff;
        item->rssi = -20 + ((devIdx % 2) ? devIdx : -devIdx);  // Around rssi -20
        item->noise = -90 - ((devIdx % 2) ? devIdx : -devIdx); // Around noise -90
        item->channel = pRad->channel;                         // Test device is on this channel
        item->centreChannel = item->channel;
        item->bandwidth = 20;
        item->operatingStandards = M_SWL_RADSTD_AUTO | M_SWL_RADSTD_AX | M_SWL_RADSTD_AC | M_SWL_RADSTD_N;
        for(uint32_t i = 0; i < 2; i++) {
            wld_vendorIe_t* vendorIe = calloc(1, sizeof(wld_vendorIe_t));
            vendorIe->frame_type = M_VENDOR_IE_BEACON;
            char* tmp = (i == 0 ? "CA:FE:00" : "CA:FE:11");
            memcpy(vendorIe->oui, tmp, sizeof(vendorIe->oui));
            amxc_llist_append(&item->vendorIEs, &vendorIe->it);
        }

        amxc_llist_append(&results->ssids, &item->it);
    }

    s_startScan(pRad, NULL);

    amxd_object_t* dmResults = amxd_object_findf(pRad->pBus, "ScanResults");
    ttb_object_assertPrintEqFile(dmResults, 0, "ScanResults-5g-3.txt");
}

/**
 * Move devices to a non-existing channel
 */
static void test_resultsMoveDevicesToNonExistingChannel(void** state _UNUSED) {
    T_Radio* pRad = dm.bandList[SWL_FREQ_BAND_5GHZ].rad;

    wld_scanResults_t* results = &pRad->scanState.lastScanResults;
    int channel = 120;
    amxc_llist_for_each(itScanSSID, &results->ssids) {
        wld_scanResultSSID_t* pSSID = amxc_container_of(itScanSSID, wld_scanResultSSID_t, it);
        if(swl_str_nmatches((char*) pSSID->ssid, "new-ssid", strlen("new-ssid"))) {
            pSSID->channel = channel;
            channel += 4;
        }
    }

    s_startScan(pRad, NULL);

    amxd_object_t* dmResults = amxd_object_findf(pRad->pBus, "ScanResults");
    ttb_object_assertPrintEqFile(dmResults, 0, "ScanResults-5g-4.txt");
}

/**
 * Move devices to an existing channel
 */
static void test_resultsMoveDevicesToExistingChannel(void** state _UNUSED) {
    T_Radio* pRad = dm.bandList[SWL_FREQ_BAND_5GHZ].rad;

    wld_scanResults_t* results = &pRad->scanState.lastScanResults;
    amxc_llist_for_each(itScanSSID, &results->ssids) {
        wld_scanResultSSID_t* pSSID = amxc_container_of(itScanSSID, wld_scanResultSSID_t, it);
        if(swl_str_nmatches((char*) pSSID->ssid, "new-ssid", strlen("new-ssid"))) {
            pSSID->channel = pRad->channel; // Move back to the channel of DUT's AP
        }
    }

    s_startScan(pRad, NULL);

    amxd_object_t* dmResults = amxd_object_findf(pRad->pBus, "ScanResults");
    ttb_object_assertPrintEqFile(dmResults, 0, "ScanResults-5g-5.txt");
}

/**
 * Delete some existing devices from the head of the list
 */
static void test_resultsDeleteDevices(void** state _UNUSED) {
    T_Radio* pRad = dm.bandList[SWL_FREQ_BAND_5GHZ].rad;

    wld_scanResults_t* results = &pRad->scanState.lastScanResults;
    for(int i = 0; i < 15; i++) {
        amxc_llist_it_t* it = amxc_llist_take_first(&results->ssids);
        wld_scanResultSSID_t* pSSID = amxc_llist_it_get_data(it, wld_scanResultSSID_t, it);
        amxc_llist_for_each(itVendor, &pSSID->vendorIEs) {
            wld_vendorIe_t* vendor = amxc_llist_it_get_data(itVendor, wld_vendorIe_t, it);
            free(vendor);
        }
        free(pSSID);
    }

    s_startScan(pRad, NULL);

    amxd_object_t* dmResults = amxd_object_findf(pRad->pBus, "ScanResults");
    ttb_object_assertPrintEqFile(dmResults, 0, "ScanResults-5g-6.txt");
}


int main(int argc _UNUSED, char* argv[] _UNUSED) {
    sahTraceSetLevel(TRACE_LEVEL_INFO);
    sahTraceAddZone(TRACE_LEVEL_APP_INFO, "ssid");
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_startAndGetScan),
        cmocka_unit_test(test_filterScanResultsBasedOnSSID),
        cmocka_unit_test(test_filterScanResultsBasedOnBSSID),
        cmocka_unit_test(test_filterScanResultsBasedOnChannel),
        cmocka_unit_test(test_resultsAll),
        cmocka_unit_test(test_resultsChangeRSSI),
        cmocka_unit_test(test_resultsAddDevicesToExistingChannel),
        cmocka_unit_test(test_resultsMoveDevicesToNonExistingChannel),
        cmocka_unit_test(test_resultsMoveDevicesToExistingChannel),
        cmocka_unit_test(test_resultsDeleteDevices)
    };
    int rc = cmocka_run_group_tests(tests, s_setupSuite, s_teardownSuite);
    sahTraceClose();
    return rc;
}
