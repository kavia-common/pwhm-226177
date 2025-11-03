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
#include "wld_accesspoint.h"
#include "wld_assocdev.h"
#include "wld_util.h"
#include "wld_chanmgt.h"
#include "wld_hostapd_cfgFile.h"
#include "wld_rad_hostapd_api.h"
#include "swl/swl_intf.h"
#include "test-toolbox/ttb_mockClock.h"
#include "test-toolbox/ttb_object.h"
#include "../testHelper/wld_th_mockVendor.h"
#include "../testHelper/wld_th_ep.h"
#include "../testHelper/wld_th_radio.h"
#include "../testHelper/wld_th_vap.h"
#include "../testHelper/wld_th_dm.h"
#include "swl/ttb/swl_ttb.h"
#include "../testHelper/wld_th_radio.h"

static wld_th_dm_t dm;
static T_Radio* pRad2 = NULL;
static T_Radio* pRad5 = NULL;
static T_Radio* pRad6 = NULL;

wld_th_radCap_t testCap2 = {
    .name = "wifi0",
    .operatingFrequencyBand = SWL_FREQ_BAND_EXT_2_4GHZ,
    .supportedFrequencyBands = M_SWL_FREQ_BAND_2_4GHZ,
    .possibleChannels = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13},
    .nrPossibleChannels = 13,
    .maxChannelBandwidth = SWL_BW_40MHZ,
    .operatingChannelBandwidth = SWL_RAD_BW_20MHZ,
    .channel = 1,
    .supportedStandards = M_SWL_RADSTD_B | M_SWL_RADSTD_G | M_SWL_RADSTD_N | M_SWL_RADSTD_AX | M_SWL_RADSTD_BE,
    .supportedChannelBandwidth = ( 1 << (SWL_BW_40MHZ + 1)) - 1,
    .cap = {
        .apCap7 = {
            .emlmrSupported = true,
            .emlsrSupported = true,
            .strSupported = true,
            .nstrSupported = true
        },
        .staCap7 = {
            .emlmrSupported = true,
            .emlsrSupported = false,
            .strSupported = true,
            .nstrSupported = false
        }
    }
};


wld_th_radCap_t testCap5 = {
    .name = "wifi1",
    .operatingFrequencyBand = SWL_FREQ_BAND_EXT_5GHZ,
    .supportedFrequencyBands = M_SWL_FREQ_BAND_5GHZ,
    .possibleChannels = {36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140},
    .nrPossibleChannels = 19,
    .maxChannelBandwidth = SWL_BW_160MHZ,
    .operatingChannelBandwidth = SWL_RAD_BW_160MHZ,
    .channel = 36,
    .supportedStandards = M_SWL_RADSTD_A | M_SWL_RADSTD_N | M_SWL_RADSTD_AC | M_SWL_RADSTD_AX | M_SWL_RADSTD_BE,
    .supportedChannelBandwidth = ( 1 << (SWL_BW_160MHZ + 1)) - 1,
    .cap = {
        .apCap7 = {
            .emlmrSupported = false,
            .emlsrSupported = true,
            .strSupported = true,
            .nstrSupported = false
        },
        .staCap7 = {
            .emlmrSupported = false,
            .emlsrSupported = true,
            .strSupported = false,
            .nstrSupported = true
        }
    }
};


wld_th_radCap_t testCap6 = {
    .name = "wifi2",
    .operatingFrequencyBand = SWL_FREQ_BAND_EXT_6GHZ,
    .supportedFrequencyBands = M_SWL_FREQ_BAND_6GHZ,
    .possibleChannels = {1, 5, 9, 13, 17, 21, 25, 29, 33, 37, 41, 45, 49, 53, 57, 61, 65, 69, 73, 77, 81, 85,
        89, 93, 97, 101, 105, 109, 113, 117, 121, 125, 129, 133, 137, 141, 145, 149, 153, 157, 161, 165, 169, 173, 177, 181, 185, 189, 193, 197, 201, 205, 209, 213, 217, 221, 225, 229, 233},
    .nrPossibleChannels = 59,
    .maxChannelBandwidth = SWL_BW_320MHZ,
    .operatingChannelBandwidth = SWL_RAD_BW_320MHZ1,
    .supportedChannelBandwidth = M_SWL_RAD_BW_ALL,
    .channel = 61,
    .supportedStandards = M_SWL_RADSTD_AX | M_SWL_RADSTD_BE,
    .cap = {
        .apCap7 = {
            .emlmrSupported = false,
            .emlsrSupported = true,
            .strSupported = true,
            .nstrSupported = false
        },
        .staCap7 = {
            .emlmrSupported = false,
            .emlsrSupported = true,
            .strSupported = false,
            .nstrSupported = true
        }
    }
};

static int s_setupSuite(void** state _UNUSED) {
    assert_true(wld_th_dmEnv_init(&dm));

    wld_th_radio_addCustomCap(&testCap2);
    wld_th_radio_addCustomCap(&testCap5);
    wld_th_radio_addCustomCap(&testCap6);
    testCap2.supportedDataTransmitRates = swl_conv_charToMask("1,2,5.5,6,9,11,12,18,24,36,48,54", swl_mcs_legacyStrList, SWL_MCS_LEGACY_LIST_SIZE);
    testCap5.supportedDataTransmitRates = swl_conv_charToMask("6,9,12,18,24,36,48,54", swl_mcs_legacyStrList, SWL_MCS_LEGACY_LIST_SIZE);
    testCap6.supportedDataTransmitRates = swl_conv_charToMask("6,9,12,18,24,36,48,54", swl_mcs_legacyStrList, SWL_MCS_LEGACY_LIST_SIZE);
    pRad2 = wld_th_radio_create(dm.ttbBus->bus_ctx, dm.mockVendor, "wifi0");
    pRad5 = wld_th_radio_create(dm.ttbBus->bus_ctx, dm.mockVendor, "wifi1");
    pRad6 = wld_th_radio_create(dm.ttbBus->bus_ctx, dm.mockVendor, "wifi2");
    pRad2->implicitBeamFormingSupported = true;
    pRad5->implicitBeamFormingSupported = true;
    pRad6->implicitBeamFormingSupported = true;

    amxp_sigmngr_trigger_signal(&dm.ttbBus->dm.sigmngr, "app:start", NULL);
    ttb_mockTimer_goToFutureMs(10000);
    return 0;
}

static int s_teardownSuite(void** state _UNUSED) {
    wld_th_dm_destroy(&dm);
    return 0;
}

static void test_radioStatus(void** state _UNUSED) {
    amxd_object_t* capObj2 = amxd_object_findf(pRad2->pBus, "Capabilities");
    amxd_object_t* capObj5 = amxd_object_findf(pRad5->pBus, "Capabilities");
    amxd_object_t* capObj6 = amxd_object_findf(pRad6->pBus, "Capabilities");

    ttb_object_assertPrintEqFile(capObj2, 0, "rad2_cap.txt");
    ttb_object_assertPrintEqFile(capObj5, 0, "rad5_cap.txt");
    ttb_object_assertPrintEqFile(capObj6, 0, "rad6_cap.txt");

    swl_chanspec_t cs2 = swl_chanspec_fromDm(1, SWL_RAD_BW_20MHZ, SWL_FREQ_BAND_EXT_2_4GHZ);
    wld_chanmgt_reportCurrentChanspec(pRad2, cs2, CHAN_REASON_INITIAL);

    swl_chanspec_t cs5 = swl_chanspec_fromDm(36, SWL_RAD_BW_80MHZ, SWL_FREQ_BAND_EXT_5GHZ);
    wld_chanmgt_reportCurrentChanspec(pRad5, cs5, CHAN_REASON_INITIAL);

    swl_chanspec_t cs6 = swl_chanspec_fromDm(1, SWL_RAD_BW_320MHZ1, SWL_FREQ_BAND_EXT_6GHZ);
    wld_chanmgt_reportCurrentChanspec(pRad6, cs6, CHAN_REASON_INITIAL);

    ttb_mockTimer_goToFutureMs(1000);

    ttb_object_assertPrintEqFile(pRad2->pBus, 2, "rad2_base.txt");
    ttb_object_assertPrintEqFile(pRad5->pBus, 2, "rad5_base.txt");
    ttb_object_assertPrintEqFile(pRad6->pBus, 2, "rad6_base.txt");
}

static void s_checkUpdateRadBw(T_Radio* pRad, swl_radBw_m expecAppRadBws, swl_radBw_e expecRunRadBw) {
    char* valStr;
    swl_radBw_m radBws;
    swl_radBw_e radBw;

    valStr = amxd_object_get_cstring_t(pRad->pBus, "ApplicableOperatingChannelBandwidths", NULL);
    radBws = swl_conv_charToMask(valStr, swl_radBw_str, SWL_RAD_BW_MAX);
    W_SWL_FREE(valStr);
    assert_int_equal(radBws, expecAppRadBws);

    amxd_object_t* pObj = amxd_object_findf(pRad->pBus, "ChannelMgt.TargetChanspec");
    valStr = amxd_object_get_cstring_t(pObj, "Bandwidth", NULL);
    radBw = swl_conv_charToEnum(valStr, swl_radBw_str, SWL_RAD_BW_MAX, SWL_RAD_BW_AUTO);
    W_SWL_FREE(valStr);
    assert_int_equal(radBw, expecRunRadBw);

    wld_chanmgt_reportCurrentChanspec(pRad, pRad->targetChanspec.chanspec, pRad->targetChanspec.reason);
    ttb_mockTimer_goToFutureMs(100);

    valStr = amxd_object_get_cstring_t(pRad->pBus, "CurrentOperatingChannelBandwidth", NULL);
    radBw = swl_conv_charToEnum(valStr, swl_radBw_str, SWL_RAD_BW_MAX, SWL_RAD_BW_AUTO);
    W_SWL_FREE(valStr);
    assert_int_equal(radBw, expecRunRadBw);
}

typedef struct testRadBw {
    T_Radio* pRad;
    const char* operStd;
    const char* cfgOperChBw;
    swl_radBw_m expecAppRadBws;
    swl_radBw_e expecRunRadBw;
} testRadBw_t;

static void s_setRadCfgAndcheckUpdateRadBw(testRadBw_t* tests, uint32_t nTests) {
    T_Radio* pRad;
    for(uint32_t i = 0; i < nTests; i++) {
        pRad = tests[i].pRad;
        amxd_trans_t trans;
        assert_int_equal(swl_object_prepareTransaction(&trans, pRad->pBus), SWL_RC_OK);
        amxd_trans_set_cstring_t(&trans, "OperatingStandards", tests[i].operStd);
        if(!swl_str_isEmpty(tests[i].cfgOperChBw)) {
            amxd_trans_set_cstring_t(&trans, "OperatingChannelBandwidth", tests[i].cfgOperChBw);
        }
        assert_int_equal(swl_object_finalizeTransactionOnLocalDm(&trans), SWL_RC_OK);
        ttb_mockTimer_goToFutureMs(100);
        s_checkUpdateRadBw(pRad, tests[i].expecAppRadBws, tests[i].expecRunRadBw);
    }
}

static void test_changeAutoAppRadBws(void** state _UNUSED) {
    testRadBw_t tests[] = {
        {pRad6, "be", "", M_SWL_RAD_BW_ALL, SWL_RAD_BW_320MHZ1},
        {pRad6, "ax", "", M_SWL_RAD_BW_320MHZ1 - 1, SWL_RAD_BW_160MHZ},
        {pRad6, "be", "", M_SWL_RAD_BW_ALL, SWL_RAD_BW_320MHZ1},

        {pRad5, "be", "", SWL_BIT_SHIFT(SWL_RAD_BW_160MHZ + 1) - 1, SWL_RAD_BW_160MHZ},
        {pRad5, "ac", "", SWL_BIT_SHIFT(SWL_RAD_BW_160MHZ + 1) - 1, SWL_RAD_BW_160MHZ},
        {pRad5, "n", "", SWL_BIT_SHIFT(SWL_RAD_BW_40MHZ + 1) - 1, SWL_RAD_BW_40MHZ},
        {pRad5, "a", "", SWL_BIT_SHIFT(SWL_RAD_BW_20MHZ + 1) - 1, SWL_RAD_BW_20MHZ},
        {pRad5, "be", "", SWL_BIT_SHIFT(SWL_RAD_BW_160MHZ + 1) - 1, SWL_RAD_BW_160MHZ},

        {pRad2, "be", "", SWL_BIT_SHIFT(SWL_RAD_BW_40MHZ + 1) - 1, SWL_RAD_BW_40MHZ},
        {pRad2, "ax", "", SWL_BIT_SHIFT(SWL_RAD_BW_40MHZ + 1) - 1, SWL_RAD_BW_40MHZ},
        {pRad2, "n", "", SWL_BIT_SHIFT(SWL_RAD_BW_40MHZ + 1) - 1, SWL_RAD_BW_40MHZ},
        {pRad2, "g", "", SWL_BIT_SHIFT(SWL_RAD_BW_20MHZ + 1) - 1, SWL_RAD_BW_20MHZ},
    };
    uint32_t nTests = SWL_ARRAY_SIZE(tests);

    T_Radio* pRad;
    for(uint32_t i = 0; i < nTests; i++) {
        pRad = tests[i].pRad;
        amxd_trans_t trans;
        assert_int_equal(swl_object_prepareTransaction(&trans, pRad->pBus), SWL_RC_OK);
        amxd_trans_set_cstring_t(&trans, "OperatingChannelBandwidth", "Auto");
        amxd_trans_set_cstring_t(&trans, "AutoBandwidthSelectMode", "MaxAvailable");
        assert_int_equal(swl_object_finalizeTransactionOnLocalDm(&trans), SWL_RC_OK);
    }

    s_setRadCfgAndcheckUpdateRadBw(tests, nTests);
}

static void test_changeManuAppRadBws(void** state _UNUSED) {
    testRadBw_t tests[] = {
        {pRad6, "ax", "320MHz-1", M_SWL_RAD_BW_320MHZ1 - 1, SWL_RAD_BW_160MHZ},
        {pRad6, "be", "320MHz-1", M_SWL_RAD_BW_ALL, SWL_RAD_BW_320MHZ1},
        {pRad6, "ax", "", M_SWL_RAD_BW_320MHZ1 - 1, SWL_RAD_BW_160MHZ},
        {pRad6, "be", "80MHz", M_SWL_RAD_BW_ALL, SWL_RAD_BW_80MHZ},
        {pRad6, "ax", "", M_SWL_RAD_BW_320MHZ1 - 1, SWL_RAD_BW_80MHZ},
        {pRad6, "be", "", M_SWL_RAD_BW_ALL, SWL_RAD_BW_80MHZ},
        {pRad6, "be", "Auto", M_SWL_RAD_BW_ALL, SWL_RAD_BW_320MHZ1},
    };

    s_setRadCfgAndcheckUpdateRadBw(tests, SWL_ARRAY_SIZE(tests));
}

static int s_test_changeMldAppRadBws_setup(void** state _UNUSED) {
    /* add the MLO cap to require MLD conf for 11be */
    wld_rad_addSuppDrvCap(pRad6, wld_rad_getFreqBand(pRad6), "MLO");
    return 0;
}

static int s_test_changeMldAppRadBw_teardown(void** state _UNUSED) {
    wld_rad_clearSuppDrvCaps(pRad6);
    wld_rad_addSuppDrvCap(pRad6, wld_rad_getFreqBand(pRad6), "");
    return 0;
}

static void test_changeMldAppRadBws(void** state _UNUSED) {
    testRadBw_t testsPreMld[] = {
        {pRad6, "ax", "", M_SWL_RAD_BW_320MHZ1 - 1, SWL_RAD_BW_160MHZ},
        {pRad6, "be", "320MHz-1", M_SWL_RAD_BW_320MHZ1 - 1, SWL_RAD_BW_160MHZ},
    };
    s_setRadCfgAndcheckUpdateRadBw(testsPreMld, SWL_ARRAY_SIZE(testsPreMld));

    amxd_object_t* pSSIDObj = amxd_object_findf(get_wld_object(), "SSID.%s", "wlan2");
    assert_true(swl_typeInt32_commitObjectParam(pSSIDObj, "MLDUnit", 0));
    ttb_mockTimer_goToFutureMs(100);

    testRadBw_t testsPostMld[] = {
        {pRad6, "be", "", M_SWL_RAD_BW_ALL, SWL_RAD_BW_320MHZ1},
    };
    s_setRadCfgAndcheckUpdateRadBw(testsPostMld, SWL_ARRAY_SIZE(testsPreMld));

    assert_true(swl_typeInt32_commitObjectParam(pSSIDObj, "MLDUnit", -1));
    ttb_mockTimer_goToFutureMs(100);

    s_setRadCfgAndcheckUpdateRadBw(testsPreMld, SWL_ARRAY_SIZE(testsPreMld));
}

static void test_StaticPuncturing_hostapdConfig(void** state _UNUSED) {
    amxd_object_t* radObj = amxd_object_findf(pRad5->pBus, "StaticPuncturing");
    assert_non_null(radObj);

    assert_true(swl_typeCharPtr_commitObjectParam(radObj, "DisabledSubChannels", "40,44"));
    ttb_mockTimer_goToFutureMs(1);

    char* data = swl_typeCharPtr_fromObjectParamDef(radObj, "DisabledSubChannels", NULL);
    assert_string_equal("40,44", data);
    free(data);

    swl_mapChar_t cfgMap;
    swl_mapChar_init(&cfgMap);

    wld_hostapd_cfgFile_setRadioConfig(pRad5, &cfgMap);

    ttb_assert_addPrint("Failed to retrieve punct_bitmap");
    ttb_assert_true(swl_map_has(&cfgMap, "punct_bitmap"));
    ttb_assert_str_eq(swl_mapChar_get(&cfgMap, "punct_bitmap"), "6");
    ttb_assert_removeLastPrint();

    swl_mapChar_cleanup(&cfgMap);
}

static void test_getEHTOperations(void** state _UNUSED) {
    uint32_t eht_chwidth_cfg = pRad5->ehtOperationIE.ehtOpInfo.control_channel_width;
    uint32_t eht_centerfreq_cfg = pRad5->ehtOperationIE.ehtOpInfo.ccfs0;
    uint32_t eht_centerfreq1_cfg = pRad5->ehtOperationIE.ehtOpInfo.ccfs1;
    uint32_t eht_bitmap_cfg = pRad5->ehtOperationIE.ehtOpInfo.disabled_sub_channel_bitmap;
    uint32_t eht_mcs_nss_cfg = pRad5->ehtOperationIE.basic_eht_mcs_n_nss_set;

    uint32_t eht_operation_present_cfg = 0;
    if(eht_chwidth_cfg || eht_centerfreq_cfg || eht_centerfreq1_cfg || eht_bitmap_cfg) {
        eht_operation_present_cfg = 1;
    }
    uint32_t eht_bitmap_present_cfg = 0;
    if(eht_bitmap_cfg) {
        eht_bitmap_present_cfg = 1;
    }

    ttb_var_t* replyVar;
    ttb_reply_t* reply = ttb_object_callFun(dm.ttbBus, pRad5->pBus, "getEHTOperations", NULL, &replyVar);
    assert_true(ttb_object_replySuccess(reply));
    ttb_mockTimer_goToFutureMs(10);

    const amxc_htable_t* results = amxc_var_constcast(amxc_htable_t, replyVar);
    assert_non_null(results);
    amxc_htable_it_t* hit = NULL;
    amxc_var_t* ht_data = NULL;
    hit = amxc_htable_get(results, "Control Channel Width");
    ht_data = amxc_var_from_htable_it(hit);
    uint32_t eht_chwidth = amxc_var_dyncast(uint32_t, ht_data);
    ttb_assert_int_eq(eht_chwidth_cfg, eht_chwidth);

    hit = amxc_htable_get(results, "CCFS0");
    ht_data = amxc_var_from_htable_it(hit);
    uint32_t eht_centerfreq = amxc_var_dyncast(uint32_t, ht_data);
    ttb_assert_int_eq(eht_centerfreq_cfg, eht_centerfreq);

    hit = amxc_htable_get(results, "CCFS1");
    ht_data = amxc_var_from_htable_it(hit);
    uint32_t eht_centerfreq1 = amxc_var_dyncast(uint32_t, ht_data);
    ttb_assert_int_eq(eht_centerfreq1_cfg, eht_centerfreq1);

    hit = amxc_htable_get(results, "Disabled Subchannel Bitmap");
    ht_data = amxc_var_from_htable_it(hit);
    uint32_t eht_bitmap = amxc_var_dyncast(uint32_t, ht_data);
    ttb_assert_int_eq(eht_bitmap_cfg, eht_bitmap);

    hit = amxc_htable_get(results, "EHT Operation Information Present");
    ht_data = amxc_var_from_htable_it(hit);
    uint32_t eht_operation_present = amxc_var_dyncast(uint32_t, ht_data);
    ttb_assert_int_eq(eht_operation_present_cfg, eht_operation_present);

    hit = amxc_htable_get(results, "Disabled Subchannel Bitmap Present");
    ht_data = amxc_var_from_htable_it(hit);
    uint32_t eht_bitmap_present = amxc_var_dyncast(uint32_t, ht_data);
    ttb_assert_int_eq(eht_bitmap_present_cfg, eht_bitmap_present);

    hit = amxc_htable_get(results, "Basic EHT-MCS And Nss Set");
    ht_data = amxc_var_from_htable_it(hit);
    uint32_t eht_mcs_nss = amxc_var_dyncast(uint32_t, ht_data);
    ttb_assert_int_eq(eht_mcs_nss_cfg, eht_mcs_nss);

    ttb_object_cleanReply(&reply, &replyVar);
}

int main(int argc _UNUSED, char* argv[] _UNUSED) {
    sahTraceSetLevel(TRACE_LEVEL_INFO);
    sahTraceAddZone(sahTraceLevel(), "rad");
    sahTraceAddZone(sahTraceLevel(), "radOStd");
    sahTraceAddZone(sahTraceLevel(), "chanMgt");
    sahTraceAddZone(sahTraceLevel(), "mld");
    sahTraceAddZone(sahTraceLevel(), "ssid");
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_radioStatus),
        cmocka_unit_test(test_changeAutoAppRadBws),
        cmocka_unit_test(test_changeManuAppRadBws),
        cmocka_unit_test(test_StaticPuncturing_hostapdConfig),
        cmocka_unit_test(test_getEHTOperations),
        cmocka_unit_test_setup_teardown(test_changeMldAppRadBws, s_test_changeMldAppRadBws_setup, s_test_changeMldAppRadBw_teardown),
    };
    int rc = cmocka_run_group_tests(tests, s_setupSuite, s_teardownSuite);
    sahTraceClose();
    return rc;
}

