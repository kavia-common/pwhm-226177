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
#include "wld_bStaMld.h"
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

static int s_setupSuite(void** state _UNUSED) {
    assert_true(wld_th_dm_init(&dm));
    wld_bStaMld_init();
    return 0;
}

static int s_teardownSuite(void** state _UNUSED) {
    wld_th_dm_destroy(&dm);
    return 0;
}

static void test_bSTAMLD_create(void** state _UNUSED) {
    T_EndPoint* pEP = wld_getEndpointByAlias("sta0");
    assert_non_null(pEP);
    T_SSID* pSSID = pEP->pSSID;
    assert_non_null(pSSID);

    pEP->pRadio->supportedStandards |= M_SWL_RADSTD_BE;

    // No bSTAMLD datamodel
    amxd_object_t* bStaMLD = amxd_object_findf(get_wld_object(), "bSTAMLD");
    assert_non_null(bStaMLD);
    assert_int_equal(amxd_object_get_instance_count(bStaMLD), 0);

    // Apply MLDUnit=0
    amxd_trans_t trans;
    assert_int_equal(swl_object_prepareTransaction(&trans, pSSID->pBus), SWL_RC_OK);
    amxd_trans_set_int32_t(&trans, "MLDUnit", 0);
    assert_int_equal(swl_object_finalizeTransactionOnLocalDm(&trans), SWL_RC_OK);
    ttb_mockTimer_goToFutureMs(100);

    // bSTAMLD new instance must be added, MLDID must be equals to 0
    assert_int_equal(amxd_object_get_instance_count(bStaMLD), 1);
    amxd_object_t* bStaInstance = amxd_object_findf(bStaMLD, "1");
    assert_non_null(bStaInstance);
    assert_int_equal(amxd_object_get_int32_t(bStaInstance, "MLDID", NULL), 0);
}

int main(int argc _UNUSED, char* argv[] _UNUSED) {
    sahTraceOpen("testApp", TRACE_TYPE_STDERR);
    sahTraceSetLevel(TRACE_LEVEL_INFO);
    sahTraceAddZone(sahTraceLevel(), "mld");
    sahTraceAddZone(sahTraceLevel(), "ssid");
    sahTraceAddZone(sahTraceLevel(), "bStaMld");
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_bSTAMLD_create),
    };

    int rc = cmocka_run_group_tests(tests, s_setupSuite, s_teardownSuite);
    sahTraceClose();
    return rc;
}
