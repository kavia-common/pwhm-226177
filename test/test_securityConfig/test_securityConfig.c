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

#include <string.h>
#include "wld.h"
#include "wld_radio.h"
#include <swl/swl_string.h>
#include <swl/swl_security.h>
#include <amxd/amxd_object.h>
#include "wld_hostapd_cfgFile.h"
#include "wld_rad_hostapd_api.h"
#include "nl80211/wld_hostapd_cfgManager_priv.h"
#include "wld_secDmn.h"
#include "wld_wpaCtrlInterface.h"



#include "../testHelper/wld_th_dm.h"
#include "../testHelper/wld_th_vap.h"
#include <test-toolbox/ttb.h>


#define ME "secCfg"

static wld_th_dm_t dm;

char* radNames[3] = {"wifi0", "wifi1", "wifi2"};
char* vapNames[3] = {"wlan0", "wlan1", "wlan2"};

const char* wpa3_cm_str = "WPA3-Personal-Compatibility";

typedef struct {
    char* key;
    char* value;
} testStruct_t;


static testStruct_t secCfgObj_24ghz[] = {
    {"wpa", "2"},
    {"wpa_key_mgmt", "WPA-PSK"},
    {"wpa_pairwise", "CCMP"},
    {"rsn_pairwise", "CCMP"},
    {"wpa_group_rekey", "0"},
    {"wpa_ptk_rekey", "0"},
    {"ieee80211w", "0"},
    {"rsn_override_key_mgmt", "SAE"},
    {"rsn_override_pairwise", "CCMP"},
    {"rsn_override_mfp", "2"},
    {"rsn_override_mfp_2", "2"},
    {"rsn_override_key_mgmt_2", "SAE-EXT-KEY"},
    {"rsn_override_pairwise_2", "GCMP-256"},
    {"rsn_override_omit_rsnxe", "1"},
    {"sae_require_mfp", "1"},
    {"sae_anti_clogging_threshold", "5"},
    {"sae_sync", "5"},
    {"sae_groups", "19 20 21"},
    {"sae_pwe", "2"},
};

static testStruct_t secCfgObj_5ghz[] = {
    {"wpa", "2"},
    {"wpa_key_mgmt", "WPA-PSK"},
    {"wpa_pairwise", "CCMP"},
    {"rsn_pairwise", "CCMP"},
    {"wpa_group_rekey", "0"},
    {"wpa_ptk_rekey", "0"},
    {"ieee80211w", "0"},
    {"rsn_override_key_mgmt", "SAE"},
    {"rsn_override_pairwise", "CCMP"},
    {"rsn_override_mfp", "2"},
    {"rsn_override_mfp_2", "2"},
    {"rsn_override_key_mgmt_2", "SAE-EXT-KEY"},
    {"rsn_override_pairwise_2", "GCMP-256"},
    {"rsn_override_omit_rsnxe", "1"},
    {"sae_require_mfp", "1"},
    {"sae_anti_clogging_threshold", "5"},
    {"sae_sync", "5"},
    {"sae_groups", "19 20 21"},
    {"sae_pwe", "2"},
};

static testStruct_t secCfgObj_6ghz[] = {
    {"wpa", "2"},
    {"wpa_key_mgmt", "SAE"},
    {"wpa_pairwise", "CCMP"},
    {"rsn_pairwise", "CCMP"},
    {"wpa_group_rekey", "0"},
    {"wpa_ptk_rekey", "0"},
    {"sae_require_mfp", "1"},
    {"sae_anti_clogging_threshold", "5"},
    {"sae_sync", "5"},
    {"sae_groups", "19 20 21"},
    {"ieee80211w", "2"},
    {"rsn_override_key_mgmt_2", "SAE-EXT-KEY"},
    {"rsn_override_pairwise_2", "GCMP-256"},
    {"rsn_override_mfp_2", "2"},
    {"sae_pwe", "1"},
};

static bool setup_internal_context_for_wpa3_cm(wld_th_dm_t* dm) {

    for(size_t i = 0; i < SWL_FREQ_BAND_MAX && i < SWL_ARRAY_SIZE(radNames); i++) {
        printf("** INIT WPA3CM for BAND %s, rad %s, vap %s\n", swl_freqBand_str[i],
               radNames[i], vapNames[i]);
        wld_th_dmBand_t* band = &dm->bandList[i];


        T_Radio* pRad = band->rad;
        T_AccessPoint* pAP = band->vapPriv;
        T_SSID* pSSID = band->vapPrivSSID;

        // Radio Internal Context
        pRad->supportedStandards |= M_SWL_RADSTD_BE;
        pRad->operatingStandards |= M_SWL_RADSTD_BE;
        wld_rad_addSuppDrvCap(pRad, wld_rad_getFreqBand(pRad), "MLO");

        // AccessPoint internal context
        pAP->secModesAvailable |= M_SWL_SECURITY_APMODE_WPA3_P_CM;
        ttb_assert_true(pAP->secModesAvailable & M_SWL_SECURITY_APMODE_WPA3_P_CM);

        if(pAP->secModeEnabled != SWL_SECURITY_APMODE_WPA3_P_CM) {
            pAP->secModeEnabled = SWL_SECURITY_APMODE_WPA3_P_CM;
        }
        SAH_TRACEZ_INFO(ME, "AP SecModeEnabled %s", swl_security_apModeToString(pAP->secModeEnabled, SWL_SECURITY_APMODEFMT_LEGACY));
        ttb_assert_str_eq(wpa3_cm_str, swl_security_apModeToString(pAP->secModeEnabled, SWL_SECURITY_APMODEFMT_LEGACY));

        // SSID Internal Context
        memcpy(band->vapPrivSSID->BSSID, band->vapPrivSSID->MACAddress, sizeof(band->vapPrivSSID->MACAddress));

        pSSID->mldUnit = (i + 3); // assign different MLD Units to avoid the complexity of link grouping
        wld_mld_registerLink(pSSID, pSSID->mldUnit);

        // RSN Override 2 configuration is added under this condition by wld_hosapd_cfgFile.c :: s_setVapCommonConfig()
        ttb_assert_true(wld_rad_is11beUsable(band->rad));

        char confFilePath[128] = {0};
        swl_str_catFormat(confFilePath, sizeof(confFilePath), "/tmp/%s_hapd.conf", pRad->Name);
        wld_secDmn_init(&pRad->hostapd, "hostapd", NULL, confFilePath, HOSTAPD_CTRL_IFACE_DIR);
        wld_wpaCtrlInterface_init(&pAP->wpaCtrlInterface, pAP->alias, pAP->pRadio->hostapd->ctrlIfaceDir);
        wld_wpaCtrlMngr_registerInterface(pRad->hostapd->wpaCtrlMngr, pAP->wpaCtrlInterface);

        const char* mrsnoParams[] = {
            "rsn_override_key_mgmt", "rsn_override_pairwise", "rsn_override_mfp",
            "rsn_override_key_mgmt_2", "rsn_override_pairwise_2", "rsn_override_mfp_2",
            "rsn_override_omit_rsnxe",
        };
        for(uint32_t i = 0; i < sizeof(mrsnoParams) / sizeof(mrsnoParams[0]); i++) {
            const char* param = mrsnoParams[i];
            wld_secDmn_setCfgParamSupp(pRad->hostapd, param, SWL_TRL_TRUE);
        }
        wld_rad_addSuppDrvCap(pRad, wld_rad_getFreqBand(pRad), "MRSNO");
    }
    return true;
}

static bool teardown_internal_context_for_wpa3_cm(wld_th_dm_t* dm) {
    for(size_t i = 0; i < SWL_FREQ_BAND_MAX && i < SWL_ARRAY_SIZE(radNames); i++) {
        wld_th_dmBand_t* band = &dm->bandList[i];

        T_Radio* pRad = band->rad;
        T_AccessPoint* pAP = band->vapPriv;
        wld_wpaCtrlInterface_cleanup(&pAP->wpaCtrlInterface);
        wld_secDmn_cleanup(&pRad->hostapd);
    }
    return true;
}

static int s_setupSuite(void** state _UNUSED) {
    assert_true(wld_th_dm_init(&dm));
    assert_true(setup_internal_context_for_wpa3_cm(&dm));
    return 0;
}

static int s_teardownSuite(void** state _UNUSED) {
    teardown_internal_context_for_wpa3_cm(&dm);
    wld_th_dm_destroy(&dm);
    return 0;
}

static void test_config_map_24_ghz(swl_mapChar_t* map) {
    for(size_t i = 0; i < SWL_ARRAY_SIZE(secCfgObj_24ghz); i++) {
        ttb_assert_addPrint("Failed security config 24GHz key [%s]", secCfgObj_24ghz[i].key);
        ttb_assert_true(swl_map_has(map, secCfgObj_24ghz[i].key));
        ttb_assert_str_eq(swl_mapChar_get(map, secCfgObj_24ghz[i].key), secCfgObj_24ghz[i].value);
        ttb_assert_removeLastPrint();
    }
}

static void test_config_map_5_ghz(swl_mapChar_t* map) {
    for(size_t i = 0; i < SWL_ARRAY_SIZE(secCfgObj_5ghz); i++) {
        ttb_assert_addPrint("Failed security config 5GHz key [%s]", secCfgObj_5ghz[i].key);
        ttb_assert_true(swl_map_has(map, secCfgObj_5ghz[i].key));
        ttb_assert_str_eq(swl_mapChar_get(map, secCfgObj_5ghz[i].key), secCfgObj_5ghz[i].value);
        ttb_assert_removeLastPrint();
    }
}

static void test_config_map_6_ghz(swl_mapChar_t* map) {
    for(size_t i = 0; i < SWL_ARRAY_SIZE(secCfgObj_6ghz); i++) {
        ttb_assert_addPrint("Failed security config 6GHz key [%s]", secCfgObj_6ghz[i].key);
        ttb_assert_true(swl_map_has(map, secCfgObj_6ghz[i].key));
        ttb_assert_str_eq(swl_mapChar_get(map, secCfgObj_6ghz[i].key), secCfgObj_6ghz[i].value);
        ttb_assert_removeLastPrint();
    }
    // For WPA3-CM 6GHz beacons should contain only RSN Override 2 IE
    ttb_assert_false(swl_map_has(map, "rsn_override_key_mgmt"));
    ttb_assert_false(swl_map_has(map, "rsn_override_pairwise"));
    ttb_assert_false(swl_map_has(map, "rsn_override_mfp"));
}

static void test_wpa3_compatibility_mode(void** state _UNUSED) {
    for(size_t i = 0; i < SWL_FREQ_BAND_MAX && i < SWL_ARRAY_SIZE(radNames); i++) {
        wld_th_dmBand_t* band = &dm.bandList[i];

        swl_mapChar_t cfgMap;
        swl_mapChar_init(&cfgMap);

        wld_hostapd_cfgFile_setVapConfig(band->vapPriv, &cfgMap, NULL);

        if(band->rad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_2_4GHZ) {
            test_config_map_24_ghz(&cfgMap);
        } else if(band->rad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_5GHZ) {
            test_config_map_5_ghz(&cfgMap);
        } else if(band->rad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_6GHZ) {
            test_config_map_6_ghz(&cfgMap);
        }

        swl_mapChar_cleanup(&cfgMap);
    }
}

static void test_setAndGetCfgParamsSupp(void** state _UNUSED) {
    wld_th_dmBand_t* band2 = &dm.bandList[SWL_FREQ_BAND_2_4GHZ];
    T_Radio* pRad = band2->rad;

    struct {
        const char* paramName;
        swl_trl_e supp;
    } entries[] = {
        {"param_sup1", SWL_TRL_TRUE, },
        {"param_unk1", SWL_TRL_UNKNOWN, },
        {"param_uns1", SWL_TRL_FALSE, },
        {"param_unk2", SWL_TRL_UNKNOWN, },
        {"param_uns2", SWL_TRL_FALSE, },
        {"param_sup2", SWL_TRL_TRUE, },
        {"param_sup3", SWL_TRL_TRUE, },
        {"param_sup4", SWL_TRL_TRUE, },
        {"param_unk3", SWL_TRL_TRUE, },
        {"param_uns3", SWL_TRL_FALSE, },
        {"param_unk4", SWL_TRL_TRUE, },
    };

    size_t maxLen = 512;
    char expParams[SWL_TRL_MAX][maxLen];
    for(uint32_t i = 0; i < SWL_TRL_MAX; i++) {
        expParams[i][0] = 0;
        wld_secDmn_getCfgParamsListBySuppVal(pRad->hostapd, expParams[i], maxLen, i);
    }
    for(uint32_t i = 0; i < SWL_ARRAY_SIZE(entries); i++) {
        wld_secDmn_setCfgParamSupp(pRad->hostapd, entries[i].paramName, entries[i].supp);
        swl_strlst_cat(expParams[entries[i].supp], maxLen, ",", entries[i].paramName);
    }

    char outBuf[maxLen];
    for(uint32_t i = 0; i < SWL_TRL_MAX; i++) {
        outBuf[0] = 0;
        uint32_t cnt = wld_secDmn_getCfgParamsListBySuppVal(pRad->hostapd, outBuf, maxLen, i);
        assert_int_equal(cnt, swl_str_countChar(outBuf, ',') + 1);
        assert_string_equal(outBuf, expParams[i]);
    }
}

static void test_edit_hostapd_conf(void** state _UNUSED) {
    for(size_t i = 0; i < SWL_FREQ_BAND_MAX && i < SWL_ARRAY_SIZE(radNames); i++) {
        wld_th_dmBand_t* band = &dm.bandList[i];
        T_Radio* pRad = band->rad;
        wld_hostapd_cfgFile_createExt(pRad);
        assert_int_equal(access(pRad->hostapd->cfgFile, F_OK), 0);
        assert_ptr_equal(band->vapPriv, wld_rad_hostapd_getSavedMainVap(pRad));
        assert_ptr_equal(band->vapPriv, wld_rad_hostapd_getCfgMainVap(pRad));
        char vapIface[32] = {0};
        swl_rc_ne rc = wld_ap_hostapd_getCfgInterface(band->vapPriv, vapIface, sizeof(vapIface));
        assert_true(swl_rc_isOk(rc));
        assert_string_equal(vapIface, band->vapPriv->alias);

        //add new vap
        char newApName[32] = {0};
        swl_str_catFormat(newApName, sizeof(newApName), "vap%sNew", swl_freqBandShort_str[pRad->operatingFrequencyBand]);
        T_AccessPoint* pNewAp = wld_th_vap_createVap(dm.ttbBus->bus_ctx, NULL, pRad, newApName);
        assert_non_null(pNewAp);

        //check new vap disable not added to hostapd conf
        wld_hostapd_cfgFile_createExt(pRad);
        rc = wld_ap_hostapd_getCfgInterface(pNewAp, vapIface, sizeof(vapIface));
        assert_int_equal(rc, SWL_RC_ERROR); //secondary bss not added to conf, as still disabled

        //enable new vap
        swl_typeUInt8_commitObjectParam(pNewAp->pBus, "Enable", 1);
        ttb_mockTimer_goToFutureMs(10);

        //check new vap enabled added to hostapd conf
        wld_hostapd_cfgFile_createExt(pRad);
        rc = wld_ap_hostapd_getCfgInterface(pNewAp, vapIface, sizeof(vapIface));
        assert_int_equal(rc, SWL_RC_OK); //secondary bss not added to conf, as still disabled
        assert_string_equal(vapIface, pNewAp->alias);

        //disable priv vap and enable new vap
        swl_typeUInt8_commitObjectParam(band->vapPriv->pBus, "Enable", 0);
        ttb_mockTimer_goToFutureMs(10);

        //check main iface changed
        wld_hostapd_cfgFile_createExt(pRad);
        assert_ptr_equal(pNewAp, wld_rad_hostapd_getSavedMainVap(pRad));
        assert_ptr_equal(pNewAp, wld_rad_hostapd_getCfgMainVap(pRad));

        //restore enabling priv vap
        swl_typeUInt8_commitObjectParam(band->vapPriv->pBus, "Enable", 1);
        ttb_mockTimer_goToFutureMs(10);
    }
}

int main(int argc _UNUSED, char* argv[] _UNUSED) {
    sahTraceSetLevel(TRACE_LEVEL_CALLSTACK);
    sahTraceAddZone(sahTraceLevel(), ME);

    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_wpa3_compatibility_mode),
        cmocka_unit_test(test_setAndGetCfgParamsSupp),
        cmocka_unit_test(test_edit_hostapd_conf),
    };

    int rc = cmocka_run_group_tests(tests, s_setupSuite, s_teardownSuite);

    sahTraceClose();
    return rc;
}

