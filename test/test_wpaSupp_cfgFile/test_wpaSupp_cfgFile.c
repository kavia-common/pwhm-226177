/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2025 SoftAtHome
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
#include "wld_wpaSupp_cfgFile.h"
#include "nl80211/wld_wpaSupp_cfgManager_priv.h"
#include "../testHelper/wld_th_dm.h"

#define WPA_SUPP_CFG_FILE "test_wpaSupp_cfgFile.conf"

static wld_th_dm_t dm;

static int setup_suite(void** state _UNUSED) {
    assert_true(wld_th_dm_init(&dm));
    return 0;
}

static int teardown_suite(void** state _UNUSED) {
    wld_th_dm_destroy(&dm);
    return 0;
}

static int teardown(void** state _UNUSED) {
    return unlink(WPA_SUPP_CFG_FILE);
}

static void test_ep_freq_list(T_EndPoint* pEp, const char* expected_freq_list, const char* expected_scan_freq) {
    wld_wpaSupp_config_t* conf = NULL;
    assert_int_equal(SWL_RC_OK, wld_wpaSupp_cfgFile_create(pEp, WPA_SUPP_CFG_FILE));
    assert_int_equal(true, wld_wpaSupp_loadConfig(&conf, WPA_SUPP_CFG_FILE));
    if(expected_freq_list) {
        assert_int_equal(true, swl_map_has(&conf->global, "freq_list"));
        assert_string_equal(expected_freq_list, swl_map_get(&conf->global, "freq_list"));
    } else {
        assert_int_equal(false, swl_map_has(&conf->global, "freq_list"));
    }
    if(expected_scan_freq) {
        assert_int_equal(true, swl_map_has(&conf->network, "scan_freq"));
        assert_string_equal(expected_scan_freq, swl_map_get(&conf->network, "scan_freq"));
    } else {
        assert_int_equal(false, swl_map_has(&conf->network, "scan_freq"));
    }
    wld_wpaSupp_deleteConfig(conf);
}

static void test_6g_freq_list_when_OnlyScanPscChannels_disabled(void** state _UNUSED) {
    const char* expected_freq_list = "5955 5975 5995 6015 6035 6055 6075 6095 6115 6135 6155 6175 6195 6215 6235 6255 6275 6295 6315 6335 6355 6375 6395 6415 6435 6455 6475 6495 6515 6535 6555 6575 6595 6615 6635 6655 6675 6695 6715 6735 6755 6775 6795 6815 6835 6855 6875 6895 6915 6935 6955 6975 6995 7015 7035 7055 7075 7095 7115";
    const char* expected_scan_freq = "5955 5975 5995 6015 6035 6055 6075 6095 6115 6135 6155 6175 6195 6215 6235 6255 6275 6295 6315 6335 6355 6375 6395 6415 6435 6455 6475 6495 6515 6535 6555 6575 6595 6615 6635 6655 6675 6695 6715 6735 6755 6775 6795 6815 6835 6855 6875 6895 6915 6935 6955 6975 6995 7015 7035 7055 7075 7095 7115";
    test_ep_freq_list(dm.bandList[SWL_FREQ_BAND_6GHZ].ep, expected_freq_list, expected_scan_freq);
}

static void test_6g_freq_list_when_OnlyScanPscChannels_enabled(void** state _UNUSED) {
    const char* expected_freq_list = "5955 5975 5995 6015 6035 6055 6075 6095 6115 6135 6155 6175 6195 6215 6235 6255 6275 6295 6315 6335 6355 6375 6395 6415 6435 6455 6475 6495 6515 6535 6555 6575 6595 6615 6635 6655 6675 6695 6715 6735 6755 6775 6795 6815 6835 6855 6875 6895 6915 6935 6955 6975 6995 7015 7035 7055 7075 7095 7115";
    const char* expected_scan_freq = "5975 6055 6135 6215 6295 6375 6455 6535 6615 6695 6775 6855 6935 7015 7095";
    dm.bandList[SWL_FREQ_BAND_6GHZ].rad->scanState.cfg.onlyScanPscChannels = 1;
    test_ep_freq_list(dm.bandList[SWL_FREQ_BAND_6GHZ].ep, expected_freq_list, expected_scan_freq);
}

int main(int argc _UNUSED, char* argv[] _UNUSED) {
    sahTraceSetLevel(TRACE_LEVEL_INFO);
    sahTraceAddZone(sahTraceLevel(), "rad");
    sahTraceAddZone(sahTraceLevel(), "wSupCfg");
    sahTraceAddZone(sahTraceLevel(), "fileMgr");
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_teardown(test_6g_freq_list_when_OnlyScanPscChannels_disabled, teardown),
        cmocka_unit_test_teardown(test_6g_freq_list_when_OnlyScanPscChannels_enabled, teardown),
    };
    return cmocka_run_group_tests(tests, setup_suite, teardown_suite);
}
