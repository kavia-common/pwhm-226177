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
#include "wld_util.h"
#include "wld_mld.h"
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
    return 0;
}

static int s_teardownSuite(void** state _UNUSED) {
    wld_th_dm_destroy(&dm);
    return 0;
}

const char* getLinkName(wld_mldLink_t* pLink) {
    const char* name = "unknown";
    ASSERTS_NOT_NULL(pLink, name, ME, "NULL");
    ASSERTS_NOT_NULL(pLink->pSSID, name, ME, "NULL");
    return pLink->pSSID->Name;
}

swl_rc_ne wld_th_mld_reassignLinkIds(wld_mld_t* pMld) {

    ASSERTS_NOT_NULL(pMld, SWL_RC_INVALID_PARAM, ME, "Invalid MLD in reassignLinkIds");
    int linkCount = 0;

    // Define desired band order: 2.4GHz → 5GHz → 6GHz
    const swl_freqBand_e bandOrder[] = {SWL_FREQ_BAND_2_4GHZ, SWL_FREQ_BAND_5GHZ, SWL_FREQ_BAND_6GHZ};

    // For each band, iterate all links in MLD and assign incrementing link IDs
    for(size_t i = 0; i < SWL_ARRAY_SIZE(bandOrder); i++) {

        amxc_llist_for_each(it, &pMld->links) {
            wld_mldLink_t* pLink = amxc_container_of(it, wld_mldLink_t, it);
            ASSERTS_NOT_NULL(pLink, SWL_RC_INVALID_PARAM, ME, "NULL link in MLD list");
            ASSERTS_NOT_NULL(pLink->pSSID, SWL_RC_INVALID_PARAM, ME, "NULL pSSID in MLD link");
            ASSERTS_NOT_NULL(pLink->pSSID->RADIO_PARENT, SWL_RC_INVALID_PARAM, ME, "NULL RADIO_PARENT in MLD link");

            // Match link’s band to current band order position
            swl_freqBand_e band = wld_rad_getFreqBand(pLink->pSSID->RADIO_PARENT);
            if(band == bandOrder[i]) {
                SAH_TRACEZ_WARNING(ME, "Assigning LinkID=%d for link=%s (band=%s)", linkCount, getLinkName(pLink), swl_freqBand_str[band]);
                swl_rc_ne rc = wld_mld_setLinkId(pLink, linkCount);
                ASSERTS_TRUE(rc == SWL_RC_OK, SWL_RC_ERROR, ME, "Failed to set LinkID=%d for link=%s", linkCount, getLinkName(pLink));
                linkCount++;
            }
        }
    }
    SAH_TRACEZ_ERROR(ME, "Reassigned %d link IDs for MLD %p", linkCount, pMld);
    return SWL_RC_OK;
}

swl_rc_ne wld_th_mld_autoAssignLinkIds(wld_mldLink_t* pLink) {

    ASSERTS_NOT_NULL(pLink, SWL_RC_INVALID_PARAM, ME, "Invalid link");
    ASSERTS_NOT_NULL(pLink->pMld, SWL_RC_INVALID_PARAM, ME, "Link has no MLD");

    // After a link is registered or moved, recalculate all link IDs in its MLD
    return wld_th_mld_reassignLinkIds(pLink->pMld);
}

static void test_apmld_create(void** state _UNUSED) {

    T_SSID* pSSID0 = dm.bandList[SWL_FREQ_BAND_2_4GHZ].vapPrivSSID;
    T_SSID* pSSID1 = dm.bandList[SWL_FREQ_BAND_5GHZ].vapPrivSSID;
    T_SSID* pSSID2 = dm.bandList[SWL_FREQ_BAND_6GHZ].vapPrivSSID;

    assert_non_null(pSSID0);
    assert_non_null(pSSID1);
    assert_non_null(pSSID2);

    T_Radio* pRad0 = pSSID0->RADIO_PARENT;
    T_Radio* pRad1 = pSSID1->RADIO_PARENT;
    T_Radio* pRad2 = pSSID2->RADIO_PARENT;

    // Enable 11be support for all test radios
    pRad0->supportedStandards |= M_SWL_RADSTD_BE;
    pRad1->supportedStandards |= M_SWL_RADSTD_BE;
    pRad2->supportedStandards |= M_SWL_RADSTD_BE;

    // SSID Internal Context
    memcpy(pSSID0->BSSID, pSSID0->MACAddress, sizeof(pSSID0->MACAddress));
    memcpy(pSSID1->BSSID, pSSID1->MACAddress, sizeof(pSSID1->MACAddress));
    memcpy(pSSID2->BSSID, pSSID2->MACAddress, sizeof(pSSID2->MACAddress));

    pSSID0->mldUnit = 0;
    pSSID1->mldUnit = 0;
    pSSID2->mldUnit = 0;

    //Register the links
    wld_mldLink_t* pLink0 = wld_mld_registerLink(pSSID0, pSSID0->mldUnit);
    ttb_assert_non_null(pLink0);
    wld_th_mld_autoAssignLinkIds(pLink0);

    wld_mldLink_t* pLink1 = wld_mld_registerLink(pSSID1, pSSID1->mldUnit);
    ttb_assert_non_null(pLink1);
    wld_th_mld_autoAssignLinkIds(pLink1);

    wld_mldLink_t* pLink2 = wld_mld_registerLink(pSSID2, pSSID2->mldUnit);
    ttb_assert_non_null(pLink2);
    wld_th_mld_autoAssignLinkIds(pLink2);

    /* all three should point to the same internal MLD */
    wld_mld_t* pMld = pLink0->pMld;
    assert_non_null(pMld);
    assert_ptr_equal(pLink1->pMld, pMld);
    assert_ptr_equal(pLink2->pMld, pMld);

    /* internal list should have 3 links */
    assert_int_equal((int) amxc_llist_size(&pMld->links), 3);

    // Each link must reference the same MLD ID
    ttb_assert_int_eq(pMld->unit, pSSID0->mldUnit);

    /* APMLD.1 must exist */
    amxd_object_t* rootObj = get_wld_object();
    amxd_object_t* apmld_obj = amxd_object_findf(rootObj, "APMLD.1");
    assert_non_null(apmld_obj);
    assert_ptr_equal(apmld_obj->priv, pMld);

    /* check 3 AffiliatedAP objects exist under APMLD.1 */
    amxd_object_t* aff1 = amxd_object_findf(apmld_obj, "AffiliatedAP.1");
    amxd_object_t* aff2 = amxd_object_findf(apmld_obj, "AffiliatedAP.2");
    amxd_object_t* aff3 = amxd_object_findf(apmld_obj, "AffiliatedAP.3");
    assert_non_null(aff1);
    assert_non_null(aff2);
    assert_non_null(aff3);

    /* each link should have its AffObj pointing to the DM object */
    assert_ptr_equal(pLink0->AffObj, aff1);
    assert_ptr_equal(pLink1->AffObj, aff2);
    assert_ptr_equal(pLink2->AffObj, aff3);
}

static void test_apmld_deinit(void** state _UNUSED) {

    T_SSID* pSSID0 = dm.bandList[SWL_FREQ_BAND_2_4GHZ].vapPrivSSID;
    T_SSID* pSSID1 = dm.bandList[SWL_FREQ_BAND_5GHZ].vapPrivSSID;
    T_SSID* pSSID2 = dm.bandList[SWL_FREQ_BAND_6GHZ].vapPrivSSID;

    wld_mld_t* pMld = pSSID0->pMldLink->pMld;
    assert_non_null(pMld);
    assert_ptr_equal(pSSID1->pMldLink->pMld, pMld);
    assert_ptr_equal(pSSID2->pMldLink->pMld, pMld);

    /* internal list should have 3 links */
    assert_int_equal((int) amxc_llist_size(&pMld->links), 3);

    pSSID0->mldUnit = -1;
    pSSID1->mldUnit = -1;
    pSSID2->mldUnit = -1;

    wld_mldLink_t* pLink0 = wld_mld_registerLink(pSSID0, pSSID0->mldUnit);
    ttb_assert_null(pLink0);

    wld_mldLink_t* pLink1 = wld_mld_registerLink(pSSID1, pSSID1->mldUnit);
    ttb_assert_null(pLink1);

    wld_mldLink_t* pLink2 = wld_mld_registerLink(pSSID2, pSSID2->mldUnit);
    ttb_assert_null(pLink2);

    /* APMLD.1 must exist */
    amxd_object_t* rootObj = get_wld_object();
    amxd_object_t* apmld_obj1 = amxd_object_findf(rootObj, "APMLD.1");
    assert_non_null(apmld_obj1);

    /* check 3 AffiliatedAP objects exist under APMLD.1 */
    amxd_object_t* aff1 = amxd_object_findf(apmld_obj1, "AffiliatedAP.1");
    amxd_object_t* aff2 = amxd_object_findf(apmld_obj1, "AffiliatedAP.2");
    amxd_object_t* aff3 = amxd_object_findf(apmld_obj1, "AffiliatedAP.3");
    assert_null(aff1);
    assert_null(aff2);
    assert_null(aff3);
}

static void test_apmld_move_link_between_mlds(void** state _UNUSED) {

    T_SSID* pSSID0 = dm.bandList[SWL_FREQ_BAND_2_4GHZ].vapPrivSSID;
    T_SSID* pSSID1 = dm.bandList[SWL_FREQ_BAND_5GHZ].vapPrivSSID;
    T_SSID* pSSID2 = dm.bandList[SWL_FREQ_BAND_6GHZ].vapPrivSSID;

    assert_non_null(pSSID0);
    assert_non_null(pSSID1);
    assert_non_null(pSSID2);

    T_Radio* pRad0 = pSSID0->RADIO_PARENT;
    T_Radio* pRad1 = pSSID1->RADIO_PARENT;
    T_Radio* pRad2 = pSSID2->RADIO_PARENT;

    // Enable 11be support for all test radios
    pRad0->supportedStandards |= M_SWL_RADSTD_BE;
    pRad1->supportedStandards |= M_SWL_RADSTD_BE;
    pRad2->supportedStandards |= M_SWL_RADSTD_BE;

    // SSID Internal Context
    memcpy(pSSID0->BSSID, pSSID0->MACAddress, sizeof(pSSID0->MACAddress));
    memcpy(pSSID1->BSSID, pSSID1->MACAddress, sizeof(pSSID1->MACAddress));
    memcpy(pSSID2->BSSID, pSSID2->MACAddress, sizeof(pSSID2->MACAddress));

    pSSID0->mldUnit = 0;
    pSSID1->mldUnit = 0;
    pSSID2->mldUnit = 0;

    // Register the MLD links
    wld_mldLink_t* pLink0 = wld_mld_registerLink(pSSID0, pSSID0->mldUnit);
    ttb_assert_non_null(pLink0);
    wld_th_mld_autoAssignLinkIds(pLink0);

    wld_mldLink_t* pLink1 = wld_mld_registerLink(pSSID1, pSSID1->mldUnit);
    ttb_assert_non_null(pLink1);
    wld_th_mld_autoAssignLinkIds(pLink1);

    wld_mldLink_t* pLink2 = wld_mld_registerLink(pSSID2, pSSID2->mldUnit);
    ttb_assert_non_null(pLink2);
    wld_th_mld_autoAssignLinkIds(pLink2);

    /* all three should point to the same internal MLD */
    wld_mld_t* pMld = pLink0->pMld;
    assert_non_null(pMld);
    assert_ptr_equal(pLink1->pMld, pMld);
    assert_ptr_equal(pLink2->pMld, pMld);

    /* internal list should have 3 links */
    assert_int_equal((int) amxc_llist_size(&pMld->links), 3);

    // Each link must reference the same MLD ID
    ttb_assert_int_eq(pMld->unit, pSSID0->mldUnit);

    /* initial check: APMLD.1 has 3 affiliated APs */
    amxd_object_t* rootObj = get_wld_object();
    amxd_object_t* apmld1 = amxd_object_findf(rootObj, "APMLD.1");
    assert_non_null(apmld1);
    assert_ptr_equal(apmld1->priv, pMld);

    amxd_object_t* aff1 = amxd_object_findf(apmld1, "AffiliatedAP.1");
    amxd_object_t* aff2 = amxd_object_findf(apmld1, "AffiliatedAP.2");
    amxd_object_t* aff3 = amxd_object_findf(apmld1, "AffiliatedAP.3");
    assert_non_null(aff1);
    assert_non_null(aff2);
    assert_non_null(aff3);

    /* -------- Move link2 to MLD unit 1 -------- */
    pSSID2->mldUnit = 1;
    wld_mldLink_t* moved_link = wld_mld_registerLink(pSSID2, pSSID2->mldUnit);
    wld_th_mld_autoAssignLinkIds(moved_link);
    assert_non_null(moved_link);

    wld_mld_t* pMld_new = moved_link->pMld;
    assert_non_null(pMld_new);

    assert_int_equal((int) amxc_llist_size(&pMld->links), 2);
    assert_int_equal((int) amxc_llist_size(&pMld_new->links), 1);

    assert_int_equal((int) pMld_new->unit, pSSID2->mldUnit);

    /* DM must now have APMLD.2 with one AffiliatedAP */
    amxd_object_t* apmld2 = amxd_object_findf(rootObj, "APMLD.2");
    assert_non_null(apmld2);
    assert_ptr_equal(apmld2->priv, pMld_new);
    amxd_object_t* aff2_1 = amxd_object_findf(apmld2, "AffiliatedAP.1");
    assert_non_null(aff2_1);
    assert_ptr_equal(moved_link->AffObj, aff2_1);

    /* APMLD.1 should now have only 2 AffiliatedAP*/
    assert_non_null(amxd_object_findf(apmld1, "AffiliatedAP.1"));
    assert_non_null(amxd_object_findf(apmld1, "AffiliatedAP.2"));
    assert_null(amxd_object_findf(apmld1, "AffiliatedAP.3"));

    /* -------- Move link2 back to MLD unit 0 -------- */
    pSSID2->mldUnit = 0;
    wld_mldLink_t* moved_back = wld_mld_registerLink(pSSID2, pSSID2->mldUnit);
    wld_th_mld_autoAssignLinkIds(moved_back);
    assert_non_null(moved_back);

    assert_int_equal((int) amxc_llist_size(&pMld->links), 3);

    assert_int_equal((int) moved_back->pMld->unit, pSSID2->mldUnit);

    /* APMLD.1 should again have 3 AffiliatedAP */
    assert_non_null(amxd_object_findf(apmld1, "AffiliatedAP.1"));
    assert_non_null(amxd_object_findf(apmld1, "AffiliatedAP.2"));
    assert_non_null(amxd_object_findf(apmld1, "AffiliatedAP.3"));

    /* link2 should now be reattached to APMLD.1 */
    assert_ptr_equal(moved_back->AffObj, amxd_object_findf(apmld1, "AffiliatedAP.3"));
    amxd_object_t* aff_ap = amxd_object_findf(apmld2, "AffiliatedAP.1");
    assert_null(aff_ap);
}

int main(int argc _UNUSED, char* argv[] _UNUSED) {
    sahTraceOpen("testApp", TRACE_TYPE_STDERR);
    sahTraceSetLevel(TRACE_LEVEL_INFO);
    sahTraceAddZone(TRACE_LEVEL_APP_INFO, "mld");
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_apmld_create),
        cmocka_unit_test(test_apmld_deinit),
        cmocka_unit_test(test_apmld_move_link_between_mlds),
    };

    int rc = cmocka_run_group_tests(tests, s_setupSuite, s_teardownSuite);
    sahTraceClose();
    return rc;
}
