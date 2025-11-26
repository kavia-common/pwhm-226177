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
#include "wld_radio.h"
#include "wld_accesspoint.h"
#include "wld_util.h"
#include "wld_mld.h"
#include "wld_epMld.h"
#include "wld_nl80211.h"
#include "test-toolbox/ttb_mockClock.h"
#include "test-toolbox/ttb_assert.h"
#include "../testHelper/wld_th_mockVendor.h"
#include "../testHelper/wld_th_radio.h"
#include "../testHelper/wld_th_vap.h"
#include "../testHelper/wld_th_dm.h"
#include "swl/ttb/swl_ttb.h"
#include "swla/ttb/swla_ttbVariant.h"
#include <test-toolbox/ttb.h>

#define ME "mld"

static wld_th_dm_t dm;

static void s_prepare11be() {
    T_Radio* pRad = NULL;
    wld_for_eachRad(pRad) {
        pRad->supportedStandards |= M_SWL_RADSTD_BE;
        pRad->operatingStandards |= M_SWL_RADSTD_BE;
        wld_rad_addSuppDrvCap(pRad, wld_rad_getFreqBand(pRad), "MLO");
    }
}
static int s_setupSuite(void** state _UNUSED) {
    assert_true(wld_th_dm_init(&dm));
    s_prepare11be();
    return 0;
}

static int s_teardownSuite(void** state _UNUSED) {
    wld_th_dm_destroy(&dm);
    return 0;
}

static void test_epMld_profile(void** state _UNUSED) {
    T_EndPoint* ep2g0 = wld_getEndpointByAlias("sta0");
    assert_non_null(ep2g0);
    T_SSID* ssid2g0 = ep2g0->pSSID;
    assert_non_null(ssid2g0);
    T_EndPoint* ep5g0 = wld_getEndpointByAlias("sta1");
    assert_non_null(ep5g0);
    T_SSID* ssid5g0 = ep5g0->pSSID;
    assert_non_null(ssid5g0);

    // WPA2 is used, SSID is not the same accross EPs, MLDUnit not configured
    assert_false(wld_epMld_isMloReady(ep2g0));

    // Apply MLDUnit=0 for ep2g0 and ep5g0
    amxd_trans_t trans;
    assert_int_equal(swl_object_prepareTransaction(&trans, ssid2g0->pBus), SWL_RC_OK);
    amxd_trans_set_int32_t(&trans, "MLDUnit", 0);
    assert_int_equal(swl_object_finalizeTransactionOnLocalDm(&trans), SWL_RC_OK);
    ttb_mockTimer_goToFutureMs(100);
    assert_int_equal(swl_object_prepareTransaction(&trans, ssid5g0->pBus), SWL_RC_OK);
    amxd_trans_set_int32_t(&trans, "MLDUnit", 0);
    assert_int_equal(swl_object_finalizeTransactionOnLocalDm(&trans), SWL_RC_OK);
    ttb_mockTimer_goToFutureMs(100);

    amxd_object_t* prof0 = amxd_object_findf(ep2g0->pBus, "Profile.prof0.");
    assert_non_null(prof0);
    amxd_object_t* prof1 = amxd_object_findf(ep5g0->pBus, "Profile.prof1.");
    assert_non_null(prof1);

    assert_int_equal(swl_object_prepareTransaction(&trans, prof0), SWL_RC_OK);
    amxd_trans_set_cstring_t(&trans, "SSID", "testMLO");
    amxd_trans_select_pathf(&trans, ".Security");
    amxd_trans_set_cstring_t(&trans, "ModeEnabled", "WPA3-Personal");
    assert_int_equal(swl_object_finalizeTransactionOnLocalDm(&trans), SWL_RC_OK);
    ttb_mockTimer_goToFutureMs(100);
    assert_int_equal(swl_object_prepareTransaction(&trans, prof1), SWL_RC_OK);

    amxd_trans_set_cstring_t(&trans, "SSID", "testMLO");
    amxd_trans_select_pathf(&trans, ".Security");
    amxd_trans_set_cstring_t(&trans, "ModeEnabled", "WPA3-Personal");
    assert_int_equal(swl_object_finalizeTransactionOnLocalDm(&trans), SWL_RC_OK);
    ttb_mockTimer_goToFutureMs(100);

    // WPA3 is used, same SSID on two Profiles
    assert_true(wld_epMld_isMloReady(ep2g0));
}

int main(int argc _UNUSED, char* argv[] _UNUSED) {
    sahTraceOpen("testApp", TRACE_TYPE_STDERR);
    sahTraceSetLevel(TRACE_LEVEL_INFO);
    sahTraceAddZone(sahTraceLevel(), "ep");
    sahTraceAddZone(sahTraceLevel(), "mld");
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_epMld_profile),
    };

    int rc = cmocka_run_group_tests(tests, s_setupSuite, s_teardownSuite);
    sahTraceClose();
    return rc;
}
