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
#include "wld_types.h"
#include "wld_radio.h"
#include "wld_hostapd_cfgFile.h"
#include <test-toolbox/ttb.h>
#include "test-toolbox/ttb_mockTimer.h"
#include "../testHelper/wld_th_mockVendor.h"
#include "../testHelper/wld_th_ep.h"
#include "../testHelper/wld_th_radio.h"
#include "../testHelper/wld_th_vap.h"
#include "../testHelper/wld_th_dm.h"
#include "../testHelper/wld_th_types.h"
#include "swl/ttb/swl_ttb.h"

static wld_th_dm_t dm;

static int setup_suite(void** state _UNUSED) {
    assert_true(wld_th_dm_init(&dm));
    return 0;
}

static int teardown_suite(void** state _UNUSED) {
    wld_th_dm_destroy(&dm);
    return 0;
}

swl_freqBand_e bandTab[] = {SWL_FREQ_BAND_2_4GHZ, SWL_FREQ_BAND_5GHZ, SWL_FREQ_BAND_6GHZ};

static T_Radio* getRadioBand(swl_freqBand_e band) {
    return (dm.bandList[band].rad);
}

static ttb_object_t* getStaticPuncturingChildObject(swl_freqBand_e band) {
    return (ttb_object_getChildObject(dm.bandList[band].radObj, "StaticPuncturing"));
}

static void test_StaticPuncturing_disabledSubChannels(void** state _UNUSED) {
    T_Radio* pRad = getRadioBand(SWL_FREQ_BAND_5GHZ);
    assert_non_null(pRad);
    ttb_object_t* radObj = getStaticPuncturingChildObject(SWL_FREQ_BAND_5GHZ);
    assert_non_null(radObj);

    assert_true(swl_typeCharPtr_commitObjectParam(radObj, "DisabledSubChannels", "40,44"));
    ttb_mockTimer_goToFutureMs(1);

    char* data = swl_typeCharPtr_fromObjectParamDef(radObj, "DisabledSubChannels", NULL);
    assert_string_equal("40,44", data);
    free(data);

    assert_int_equal(40, pRad->disabledSubchannels[0]);
    assert_int_equal(44, pRad->disabledSubchannels[1]);

    // Testing failure case when one or more channels do not belong to PossibleChannels
    assert_false(swl_typeCharPtr_commitObjectParam(radObj, "DisabledSubChannels", "39"));
    ttb_mockTimer_goToFutureMs(1);
}

static void test_StaticPuncturing_entries(void** state _UNUSED) {
    T_Radio* pRad = getRadioBand(SWL_FREQ_BAND_5GHZ);
    assert_non_null(pRad);
    ttb_object_t* radObj = getStaticPuncturingChildObject(SWL_FREQ_BAND_5GHZ);
    assert_non_null(radObj);

    // Testing case when duplicate entries are present
    assert_true(swl_typeCharPtr_commitObjectParam(radObj, "DisabledSubChannels", "44,44"));
    ttb_mockTimer_goToFutureMs(1);

    char* data = swl_typeCharPtr_fromObjectParamDef(radObj, "DisabledSubChannels", NULL);
    assert_string_equal("44,44", data);
    free(data);

    assert_int_equal(1, pRad->nrDisabledSubChannels);
    assert_int_equal(44, pRad->disabledSubchannels[0]);
}

int main(int argc _UNUSED, char* argv[] _UNUSED) {
    sahTraceSetLevel(TRACE_LEVEL_INFO);
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_StaticPuncturing_disabledSubChannels),
        cmocka_unit_test(test_StaticPuncturing_entries),
    };
    int rc = cmocka_run_group_tests(tests, setup_suite, teardown_suite);
    sahTraceClose();
    return rc;
}

