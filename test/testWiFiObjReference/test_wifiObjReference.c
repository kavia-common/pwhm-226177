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

static wld_th_dm_t dm;

static int s_setupSuite(void** state _UNUSED) {
    assert_true(wld_th_dm_init(&dm));
    wld_util_setReferencePathPrefix("");
    return 0;
}

static int s_teardownSuite(void** state _UNUSED) {
    wld_th_dm_destroy(&dm);
    return 0;
}

typedef struct {
    const char* inRefPath;
    amxd_object_t* inRefObj;
    swl_rc_ne outRc;
    const char* outRefPath;
} test_referenceInfo_t;

static void checkReference(test_referenceInfo_t* testData) {
    char outBuf[128] = {0};
    swl_rc_ne rc = wld_util_getRealReferencePath(outBuf, sizeof(outBuf), testData->inRefPath, testData->inRefObj);
    printf("in path(%s) obj(%p) / out path(%s) rc(%d) / expec path(%s) rc(%d)\n",
           testData->inRefPath, testData->inRefObj,
           outBuf, rc,
           testData->outRefPath, testData->outRc);
    assert_int_equal(rc, testData->outRc);
    assert_string_equal(outBuf, testData->outRefPath);
}

static void test_radioReference(void** state _UNUSED) {
    T_Radio* pRad2 = dm.bandList[SWL_FREQ_BAND_2_4GHZ].rad;
    T_Radio* pRad5 = dm.bandList[SWL_FREQ_BAND_5GHZ].rad;
    T_Radio* pRad6 = dm.bandList[SWL_FREQ_BAND_6GHZ].rad;
    test_referenceInfo_t tests[] = ARR(
        //success cases
        ARR("", pRad2->pBus, SWL_RC_OK, "WiFi.Radio.1."),
        ARR("WiFi.Radio.wifi1", NULL, SWL_RC_OK, "WiFi.Radio.wifi1."),
        ARR("WiFi.Radio.1.", pRad2->pBus, SWL_RC_OK, "WiFi.Radio.1."),
        ARR("WiFi.Radio.wifi0", pRad2->pBus, SWL_RC_OK, "WiFi.Radio.wifi0."),
        ARR("WiFi.Radio.wifi1.", pRad5->pBus, SWL_RC_OK, "WiFi.Radio.wifi1."),
        ARR("WiFi.Radio.wifi2", pRad6->pBus, SWL_RC_OK, "WiFi.Radio.wifi2."),
        ARR("Device.WiFi.Radio.1.", pRad2->pBus, SWL_RC_OK, "Device.WiFi.Radio.1."),
        ARR("Device.WiFi.Radio.1", pRad2->pBus, SWL_RC_OK, "Device.WiFi.Radio.1."),
        ARR("Device.WiFi.Radio.3.", pRad6->pBus, SWL_RC_OK, "Device.WiFi.Radio.3."),
        // override cases
        ARR("Device.WiFi.Radio.", pRad6->pBus, SWL_RC_OK, "WiFi.Radio.3."),
        ARR("Radio.wifi2", pRad6->pBus, SWL_RC_OK, "WiFi.Radio.3."),
        ARR("WiFi.Radio.wifi1", pRad2->pBus, SWL_RC_OK, "WiFi.Radio.1."),
        // error cases
        ARR(NULL, NULL, SWL_RC_ERROR, ""),
        ARR("", NULL, SWL_RC_ERROR, ""),
        ARR("WiFi.Radio.", NULL, SWL_RC_ERROR, ""),
        ARR("WiFi.Radio.10", NULL, SWL_RC_ERROR, ""),
        );
    for(uint32_t i = 0; i < SWL_ARRAY_SIZE(tests); i++) {
        checkReference(&tests[i]);
    }
}

static void test_ssidReference(void** state _UNUSED) {
    T_SSID* pSsidPriv2 = dm.bandList[SWL_FREQ_BAND_2_4GHZ].vapPriv->pSSID;
    T_SSID* pSsidPriv5 = dm.bandList[SWL_FREQ_BAND_5GHZ].vapPriv->pSSID;
    T_SSID* pSsidPriv6 = dm.bandList[SWL_FREQ_BAND_6GHZ].vapPriv->pSSID;

    T_SSID* pSsidEp2 = dm.bandList[SWL_FREQ_BAND_2_4GHZ].ep->pSSID;
    T_SSID* pSsidEp5 = dm.bandList[SWL_FREQ_BAND_5GHZ].ep->pSSID;
    T_SSID* pSsidEp6 = dm.bandList[SWL_FREQ_BAND_6GHZ].ep->pSSID;
    test_referenceInfo_t tests[] = ARR(
        //success cases
        ARR(NULL, pSsidPriv2->pBus, SWL_RC_OK, "WiFi.SSID.1."),
        ARR("", pSsidEp2->pBus, SWL_RC_OK, "WiFi.SSID.4."),
        ARR("WiFi.SSID.1.", pSsidPriv2->pBus, SWL_RC_OK, "WiFi.SSID.1."),
        ARR("WiFi.SSID.wlan0", pSsidPriv2->pBus, SWL_RC_OK, "WiFi.SSID.wlan0."),
        ARR("WiFi.SSID.wlan1.", pSsidPriv5->pBus, SWL_RC_OK, "WiFi.SSID.wlan1."),
        ARR("WiFi.SSID.wlan2", NULL, SWL_RC_OK, "WiFi.SSID.wlan2."),
        ARR("WiFi.SSID.sta0", pSsidEp2->pBus, SWL_RC_OK, "WiFi.SSID.sta0."),
        ARR("WiFi.SSID.sta1", NULL, SWL_RC_OK, "WiFi.SSID.sta1."),
        ARR("Device.WiFi.SSID.1.", pSsidPriv2->pBus, SWL_RC_OK, "Device.WiFi.SSID.1."),
        ARR("Device.WiFi.SSID.2", pSsidPriv5->pBus, SWL_RC_OK, "Device.WiFi.SSID.2."),
        ARR("Device.WiFi.SSID.3.", pSsidPriv6->pBus, SWL_RC_OK, "Device.WiFi.SSID.3."),
        ARR("Device.WiFi.SSID.6.", pSsidEp6->pBus, SWL_RC_OK, "Device.WiFi.SSID.6."),
        // override cases
        ARR("Device.WiFi.SSID.", pSsidPriv5->pBus, SWL_RC_OK, "WiFi.SSID.2."),
        ARR("SSID.wlan2", pSsidPriv6->pBus, SWL_RC_OK, "WiFi.SSID.3."),
        ARR("WiFi.SSID.sta1", pSsidEp2->pBus, SWL_RC_OK, "WiFi.SSID.4."),
        ARR("Device.WiFi.SSID.sta2", pSsidEp5->pBus, SWL_RC_OK, "WiFi.SSID.5."),
        // error cases
        ARR(NULL, NULL, SWL_RC_ERROR, ""),
        ARR("", NULL, SWL_RC_ERROR, ""),
        ARR("WiFi.SSID.", NULL, SWL_RC_ERROR, ""),
        ARR("WiFi.SSID.10", NULL, SWL_RC_ERROR, ""),
        );
    for(uint32_t i = 0; i < SWL_ARRAY_SIZE(tests); i++) {
        checkReference(&tests[i]);
    }
}

static void test_referencePrefixed(void** state _UNUSED) {
    T_Radio* pRad2 = dm.bandList[SWL_FREQ_BAND_2_4GHZ].rad;
    T_Radio* pRad5 = dm.bandList[SWL_FREQ_BAND_5GHZ].rad;
    T_Radio* pRad6 = dm.bandList[SWL_FREQ_BAND_6GHZ].rad;

    T_SSID* pSsidPriv2 = dm.bandList[SWL_FREQ_BAND_2_4GHZ].vapPriv->pSSID;
    T_SSID* pSsidPriv5 = dm.bandList[SWL_FREQ_BAND_5GHZ].vapPriv->pSSID;
    T_SSID* pSsidPriv6 = dm.bandList[SWL_FREQ_BAND_6GHZ].vapPriv->pSSID;


    test_referenceInfo_t tests[] = ARR(
        //success cases, empty prefix
        ARR("", pRad2->pBus, SWL_RC_OK, "WiFi.Radio.1."),
        ARR("WiFi.Radio.wifi1", NULL, SWL_RC_OK, "WiFi.Radio.wifi1."),
        ARR("WiFi.Radio.1.", pRad2->pBus, SWL_RC_OK, "WiFi.Radio.1."),
        ARR("WiFi.Radio.wifi0", pRad2->pBus, SWL_RC_OK, "WiFi.Radio.wifi0."),
        ARR("WiFi.Radio.wifi1.", pRad5->pBus, SWL_RC_OK, "WiFi.Radio.wifi1."),
        ARR("WiFi.Radio.wifi2", pRad6->pBus, SWL_RC_OK, "WiFi.Radio.wifi2."),
        ARR(NULL, pSsidPriv2->pBus, SWL_RC_OK, "WiFi.SSID.1."),
        ARR("WiFi.SSID.1.", pSsidPriv2->pBus, SWL_RC_OK, "WiFi.SSID.1."),
        ARR("WiFi.SSID.wlan0", pSsidPriv2->pBus, SWL_RC_OK, "WiFi.SSID.wlan0."),
        ARR("WiFi.SSID.wlan1.", pSsidPriv5->pBus, SWL_RC_OK, "WiFi.SSID.wlan1."),
        ARR("WiFi.SSID.wlan2", NULL, SWL_RC_OK, "WiFi.SSID.wlan2."),
        ARR("Device.WiFi.SSID.3.", pSsidPriv6->pBus, SWL_RC_OK, "Device.WiFi.SSID.3."),
        );

    wld_util_setReferencePathPrefix("");
    for(uint32_t i = 0; i < SWL_ARRAY_SIZE(tests); i++) {
        checkReference(&tests[i]);
    }

    const char* overFlow = "WiFi.SSID.3.InitialPathIsTooLongItWillCauseAnErrorInsideLibSWLCFunction_swl_str_cat_if_it_is_longer_than_128_bytes_probably";

    test_referenceInfo_t testsPrefixed[] = ARR(
        //success cases, prefix set to Device. : add prefix
        ARR("", pRad2->pBus, SWL_RC_OK, "Device.WiFi.Radio.1."),
        ARR("WiFi.Radio.wifi1", NULL, SWL_RC_OK, "Device.WiFi.Radio.wifi1."),
        ARR("WiFi.Radio.1.", pRad2->pBus, SWL_RC_OK, "Device.WiFi.Radio.1."),
        ARR("WiFi.Radio.wifi0", pRad2->pBus, SWL_RC_OK, "Device.WiFi.Radio.wifi0."),
        ARR("WiFi.Radio.wifi1.", pRad5->pBus, SWL_RC_OK, "Device.WiFi.Radio.wifi1."),
        ARR("WiFi.Radio.wifi2", pRad6->pBus, SWL_RC_OK, "Device.WiFi.Radio.wifi2."),
        ARR(NULL, pSsidPriv2->pBus, SWL_RC_OK, "Device.WiFi.SSID.1."),
        ARR("WiFi.SSID.1.", pSsidPriv2->pBus, SWL_RC_OK, "Device.WiFi.SSID.1."),
        ARR("WiFi.SSID.wlan0", pSsidPriv2->pBus, SWL_RC_OK, "Device.WiFi.SSID.wlan0."),
        ARR("WiFi.SSID.wlan1.", pSsidPriv5->pBus, SWL_RC_OK, "Device.WiFi.SSID.wlan1."),
        ARR("WiFi.SSID.wlan2", NULL, SWL_RC_OK, "Device.WiFi.SSID.wlan2."),
        ARR("WiFi.SSID.wlan0", pSsidPriv2->pBus, SWL_RC_OK, "Device.WiFi.SSID.wlan0."),
        ARR("WiFi.SSID.wlan1.", pSsidPriv5->pBus, SWL_RC_OK, "Device.WiFi.SSID.wlan1."),
        ARR("Device.WiFi.SSID.3.", pSsidPriv6->pBus, SWL_RC_OK, "Device.WiFi.SSID.3."),
        // success, even with a too long value for currRefPath arg : function will use currRefObj argument preferentially
        ARR(overFlow, pSsidPriv6->pBus, SWL_RC_OK, "Device.WiFi.SSID.3."),
        // failure : overflow
        ARR(overFlow, NULL, SWL_RC_ERROR, ""),
        );

    wld_util_setReferencePathPrefix("Device.");
    for(uint32_t i = 0; i < SWL_ARRAY_SIZE(testsPrefixed); i++) {
        checkReference(&testsPrefixed[i]);
    }
    wld_util_setReferencePathPrefix("");
}

int main(int argc _UNUSED, char* argv[] _UNUSED) {
    sahTraceSetLevel(TRACE_LEVEL_INFO);
    sahTraceAddZone(TRACE_LEVEL_APP_INFO, "radRef");
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_radioReference),
        cmocka_unit_test(test_ssidReference),
        cmocka_unit_test(test_referencePrefixed),
    };
    int rc = cmocka_run_group_tests(tests, s_setupSuite, s_teardownSuite);
    sahTraceClose();
    return rc;
}

