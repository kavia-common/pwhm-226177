/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2022 SoftAtHome
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
#include "wld.h"
#include "wld_radio.h"
#include "wld_util.h"
#include "wld_wps.h"
#include "wld_endpoint.h"
#include "swl/map/swl_mapCharFmt.h"
#include "wld_wpaSupp_cfgManager.h"
#include "wld_wpaSupp_cfgManager_priv.h"
#include "wld_wpaSupp_cfgFile.h"

#define ME "wSupCfg"

static const char* wld_wpsConfigMethods[] = { "usba", "ethernet", "label", "display", "ext_nfc_token", "int_nfc_token",
    "nfc_interface", "push_button", "keypad", "virtual_display", "physical_display", "virtual_push_button",
    "physical_push_button", "PIN",
    0};

/**
 * @brief set the global configuration of the wpa_supplicant
 *
 * @param pEP an endpoint
 * @param config a pointer to the configuration of the wpa_supplicant
 *
 * @return void
 */
static swl_rc_ne s_setWpaSuppGlobalConfig(T_EndPoint* pEP, wld_wpaSupp_config_t* config) {
    T_Radio* pRad = pEP->pRadio;
    ASSERTS_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    swl_mapChar_t* global = wld_wpaSupp_getGlobalConfig(config);
    ASSERTS_NOT_NULL(global, SWL_RC_ERROR, ME, "NULL");

    swl_mapChar_add(global, "update_config", "1");
    swl_mapChar_add(global, "ctrl_interface", (pEP->wpaSupp ? pEP->wpaSupp->ctrlIfaceDir : WPA_SUPPLICANT_CTRL_IFACE_DIR));
    swl_mapChar_add(global, "manufacturer", pRad->wpsConst->Manufacturer);
    swl_mapChar_add(global, "model_name", pRad->wpsConst->ModelName);
    swl_mapChar_add(global, "model_number", pRad->wpsConst->ModelNumber);
    swl_mapChar_add(global, "serial_number", pRad->wpsConst->SerialNumber);
    swl_mapChar_add(global, "uuid", pRad->wpsConst->UUID);
    int tmpver[4];
    sscanf(pRad->wpsConst->OsVersion, "%i.%i.%i.%i", &tmpver[0], &tmpver[1], &tmpver[2], &tmpver[3]);
    swl_mapCharFmt_addValStr(global, "os_version", "%.8x", ((unsigned int) (tmpver[0] << 24 | tmpver[1] << 16 | tmpver[2] << 8 | tmpver[3])));
    swl_mapChar_add(global, "device_type", "6-0050F204-1");
    char configMethodsStr[128] = {0};
    swl_conv_maskToCharSep(configMethodsStr, sizeof(configMethodsStr), pEP->WPS_ConfigMethodsEnabled, wld_wpsConfigMethods, SWL_ARRAY_SIZE(wld_wpsConfigMethods), ' ');
    if(strlen(configMethodsStr) > 0) {
        swl_mapChar_add(global, "config_methods", configMethodsStr);
    }

    /* By default for endpoint, wps_cred_processing should be set to 2
     * to process received credentials internally and pass them over ctrl_iface
     * to external program */
    swl_mapChar_add(global, "wps_cred_processing", "2");

    T_EndPointProfile* epProfile = pEP->currentProfile;
    if((epProfile != NULL) &&
       ((epProfile->secModeEnabled == SWL_SECURITY_APMODE_WPA2_WPA3_P) ||
        (epProfile->secModeEnabled == SWL_SECURITY_APMODE_WPA3_P))) {
        swl_mapChar_add(global, "sae_pwe", "2");
    }

    char freqListStr[320] = {'\0'};
    for(int i = 0; i < pRad->nrPossibleChannels; i++) {
        uint32_t freqMHz = 0;
        swl_chanspec_t chanspec = SWL_CHANSPEC_NEW(pRad->possibleChannels[i], SWL_BW_20MHZ, pRad->operatingFrequencyBand);
        if((SWL_RC_OK == swl_chanspec_channelToMHz(&chanspec, &freqMHz))) {
            swl_strlst_catFormat(freqListStr, sizeof(freqListStr), " ", "%d", freqMHz);
        }
    }
    if(!swl_str_isEmpty(freqListStr)) {
        swl_mapChar_add(global, "freq_list", freqListStr);
    }

    return SWL_RC_OK;
}

static swl_rc_ne s_setPsk(swl_mapChar_t* network, T_EndPointProfile* epProfile, bool hashKeyPassPhrase) {
    char* keyPassPhrase = NULL;
    char md5Key[PSK_KEY_SIZE_LEN * 2 - 1] = {'\0'};
    char temp[PSK_KEY_SIZE_LEN + 2] = {0};
    swl_rc_ne ret = SWL_RC_ERROR;
    if(hashKeyPassPhrase && (!swl_str_isEmpty(epProfile->keyPassPhrase)) && (swl_str_len(epProfile->keyPassPhrase) != (PSK_KEY_SIZE_LEN - 1))) {
        wldu_convCreds2MD5(epProfile->SSID, epProfile->keyPassPhrase, md5Key, PSK_KEY_SIZE_LEN * 2 - 1);
        md5Key[PSK_KEY_SIZE_LEN - 1] = '\0';
        keyPassPhrase = md5Key;
    } else {
        swl_str_catFormat(temp, sizeof(temp), "\"%s\"", epProfile->keyPassPhrase);
        keyPassPhrase = temp;
    }
    if(!swl_str_isEmpty(keyPassPhrase)) {
        swl_mapChar_add(network, "psk", keyPassPhrase);
        ret = SWL_RC_OK;
    }
    return ret;
}

static swl_rc_ne s_setSaePassword(swl_mapChar_t* network, T_EndPointProfile* epProfile) {
    ASSERTS_FALSE(swl_str_isEmpty(epProfile->saePassphrase), SWL_RC_ERROR, ME, "saePassphrase is empty");

    char saePassphrase[SAE_KEY_SIZE_LEN + 2];
    memset(saePassphrase, 0, sizeof(saePassphrase));
    swl_str_catFormat(saePassphrase, sizeof(saePassphrase), "\"%s\"", epProfile->saePassphrase);
    swl_mapChar_add(network, "sae_password", saePassphrase);

    return SWL_RC_OK;
}

/**
 * @brief set the global configuration of the wpa_supplicant
 *
 * @param pEP an endpoint
 * @param config a pointer to the configuration of the wpa_supplicant
 *
 * @return void
 */
static swl_rc_ne s_setWpaSuppNetworkConfig(T_EndPoint* pEP, wld_wpaSupp_config_t* config) {
    T_EndPointProfile* epProfile = pEP->currentProfile;
    ASSERTS_NOT_NULL(epProfile, SWL_RC_INVALID_PARAM, ME, "NULL");
    T_Radio* pRad = pEP->pRadio;
    ASSERTS_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    swl_mapChar_t* network = wld_wpaSupp_getNetworkConfig(config);
    ASSERTS_NOT_NULL(network, SWL_RC_INVALID_PARAM, ME, "NULL");

    bool ret = swl_str_isEmpty(epProfile->SSID);
    ASSERT_FALSE(ret, SWL_RC_ERROR, ME, "empty SSID");

    swl_mapChar_add(network, "scan_ssid", "1");
    char ssid[SSID_NAME_LEN + 2] = {0};
    swl_str_catFormat(ssid, sizeof(ssid), "\"%s\"", epProfile->SSID);
    swl_mapChar_add(network, "ssid", ssid);

    swl_macBin_t epBssidBin;
    bool hasTarget = wld_endpoint_getTargetBssid(pEP, &epBssidBin);
    if(hasTarget) {
        swl_macChar_t epBssidStr;
        swl_mac_binToChar(&epBssidStr, &epBssidBin);
        swl_mapChar_add(network, "bssid", epBssidStr.cMac);
    }

    if(!swl_str_isEmpty(pEP->WPS_ClientPIN)) {
        swl_mapChar_add(network, "pin", pEP->WPS_ClientPIN);
    }

    swl_security_mfpMode_e mfp = swl_security_getTargetMfpMode(epProfile->secModeEnabled, epProfile->mfpConfig);

    switch(epProfile->secModeEnabled) {
    case SWL_SECURITY_APMODE_NONE:
    {
        swl_mapChar_add(network, "key_mgmt", "NONE");
    }
    break;
    case SWL_SECURITY_APMODE_WEP64:
    case SWL_SECURITY_APMODE_WEP128:
    case SWL_SECURITY_APMODE_WEP128IV:
    {
        swl_mapChar_add(network, "key_mgmt", "NONE");
        char WEPKEYCONV[36] = {0};
        convASCII_WEPKey(epProfile->WEPKey, WEPKEYCONV, sizeof(WEPKEYCONV));
        swl_mapChar_add(network, "wep_key0", WEPKEYCONV);
        swl_mapChar_add(network, "wep_key1", WEPKEYCONV);
        swl_mapChar_add(network, "wep_key2", WEPKEYCONV);
        swl_mapChar_add(network, "wep_key3", WEPKEYCONV);
        swl_mapChar_add(network, "wep_tx_keyidx", "0");
    }
    break;
    case SWL_SECURITY_APMODE_WPA_P:
    {
        swl_mapChar_add(network, "proto", "WPA");
        swl_mapCharFmt_addValStr(network, "key_mgmt", "%s%s",
                                 ((mfp != SWL_SECURITY_MFPMODE_REQUIRED) ? "WPA-PSK " : ""),
                                 ((mfp == SWL_SECURITY_MFPMODE_REQUIRED) ? "WPA-PSK-SHA256 " : ""));
        swl_mapChar_add(network, "key_mgmt", "WPA-PSK");
        swl_mapChar_add(network, "pairwise", "TKIP");
        swl_mapChar_add(network, "group", "TKIP");
        swl_mapChar_add(network, "auth_alg", "OPEN");
        ASSERT_EQUALS(s_setPsk(network, epProfile, true), SWL_RC_OK, SWL_RC_ERROR, ME,
                      "%s: fail to set psk", pEP->Name);
    }
    break;
    case SWL_SECURITY_APMODE_WPA2_P:
    {
        swl_mapChar_add(network, "proto", "RSN");
        swl_mapCharFmt_addValStr(network, "key_mgmt", "%s%s",
                                 ((mfp != SWL_SECURITY_MFPMODE_REQUIRED) ? "WPA-PSK " : ""),
                                 ((mfp == SWL_SECURITY_MFPMODE_REQUIRED) ? "WPA-PSK-SHA256 " : ""));
        swl_mapChar_add(network, "pairwise", "CCMP");
        swl_mapChar_add(network, "group", "CCMP TKIP");
        swl_mapChar_add(network, "auth_alg", "OPEN");
        ASSERT_EQUALS(s_setPsk(network, epProfile, true), SWL_RC_OK, SWL_RC_ERROR, ME,
                      "%s: fail to set psk", pEP->Name);
    }
    break;
    case SWL_SECURITY_APMODE_WPA_WPA2_P:
    {
        swl_mapChar_add(network, "proto", "RSN");
        swl_mapCharFmt_addValStr(network, "key_mgmt", "%s%s",
                                 ((mfp != SWL_SECURITY_MFPMODE_REQUIRED) ? "WPA-PSK " : ""),
                                 ((mfp == SWL_SECURITY_MFPMODE_REQUIRED) ? "WPA-PSK-SHA256 " : ""));
        swl_mapChar_add(network, "pairwise", "CCMP TKIP");
        swl_mapChar_add(network, "group", "TKIP");
        swl_mapChar_add(network, "auth_alg", "OPEN");
        ASSERT_EQUALS(s_setPsk(network, epProfile, true), SWL_RC_OK, SWL_RC_ERROR, ME,
                      "%s: fail to set psk", pEP->Name);
    }
    break;
    case SWL_SECURITY_APMODE_WPA2_WPA3_P:
    {
        swl_mapChar_add(network, "proto", "RSN");
        swl_mapCharFmt_addValStr(network, "key_mgmt", "%s%s%s",
                                 ((mfp != SWL_SECURITY_MFPMODE_REQUIRED) ? "WPA-PSK " : ""),
                                 ((mfp == SWL_SECURITY_MFPMODE_REQUIRED) ? "WPA-PSK-SHA256 " : ""),
                                 (wld_rad_checkEnabledRadStd(pRad, SWL_RADSTD_BE) ? "SAE SAE-EXT-KEY" : "SAE"));
        swl_mapChar_add(network, "pairwise", wld_rad_checkEnabledRadStd(pRad, SWL_RADSTD_BE) ? "CCMP GCMP-256" : "CCMP");
        swl_mapChar_add(network, "group", "CCMP");
        swl_mapChar_add(network, "auth_alg", "OPEN");
        bool hashKeyPassPhrase = (s_setSaePassword(network, epProfile) == SWL_RC_OK);
        ASSERT_EQUALS(s_setPsk(network, epProfile, hashKeyPassPhrase), SWL_RC_OK, SWL_RC_ERROR, ME,
                      "%s: fail to set psk", pEP->Name);
    }
    break;
    case SWL_SECURITY_APMODE_WPA3_P:
    {
        swl_mapChar_add(network, "proto", "RSN");
        swl_mapChar_add(network, "key_mgmt", wld_rad_checkEnabledRadStd(pRad, SWL_RADSTD_BE) ? "SAE SAE-EXT-KEY" : "SAE");
        swl_mapChar_add(network, "pairwise", wld_rad_checkEnabledRadStd(pRad, SWL_RADSTD_BE) ? "CCMP GCMP-256" : "CCMP");
        swl_mapChar_add(network, "group", "CCMP");
        swl_mapChar_add(network, "auth_alg", "OPEN");
        if(s_setSaePassword(network, epProfile) < SWL_RC_OK) {
            SAH_TRACEZ_INFO(ME, "%s: fail to set SAE password", pEP->Name);
            ASSERT_EQUALS(s_setPsk(network, epProfile, false), SWL_RC_OK, SWL_RC_ERROR, ME,
                          "%s: fail to set psk", pEP->Name);
        }
    }
    break;
    case SWL_SECURITY_APMODE_OWE:
    {
        swl_mapChar_add(network, "proto", "RSN");
        swl_mapChar_add(network, "key_mgmt", "OWE");
        swl_mapChar_add(network, "pairwise", "CCMP");
        swl_mapChar_add(network, "group", "CCMP");
        swl_mapChar_add(network, "auth_alg", "OPEN");
    }
    break;
    default:
        SAH_TRACEZ_ERROR(ME, "%s: not supported security mode %d", pEP->Name, epProfile->secModeEnabled);
        break;
    }

    swl_mapCharFmt_addValInt32(network, "ieee80211w", mfp);

    if(pEP->multiAPEnable) {
        swl_mapChar_add(network, "multi_ap_backhaul_sta", "1");
    }
    swl_mapChar_add(network, "beacon_int", "100");
    return SWL_RC_OK;
}

/**
 * @brief create a configuration file for the wpaSupplicant
 *
 * @param pEP the endpoint
 * @param cfgFileName the wpaSupplicant configuration file name
 *
 * @return void
 */
swl_rc_ne wld_wpaSupp_cfgFile_create(T_EndPoint* pEP, char* cfgFileName) {
    ASSERTS_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_STR(cfgFileName, SWL_RC_INVALID_PARAM, ME, "Empty path");

    wld_wpaSupp_config_t* config;
    bool ret = wld_wpaSupp_createConfig(&config);
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "Bad config");

    s_setWpaSuppGlobalConfig(pEP, config);

    s_setWpaSuppNetworkConfig(pEP, config);

    pEP->pFA->mfn_wendpoint_updateConfigMaps(pEP, config);

    /* multi_ap_profile has been set to the global section of wpaSupplicant configuration
     * in some vendor modules. It conflicts with generic implementation where multi_ap_profile
     * is a part of network section. */
    swl_mapChar_t* network = wld_wpaSupp_getNetworkConfig(config);
    swl_mapChar_t* global = wld_wpaSupp_getGlobalConfig(config);
    if((!swl_map_has(global, "multi_ap_profile")) && (pEP->multiAPProfile > MULTIAP_NOT_SUPPORTED)
       && (swl_map_size(network) > 0)) {
        swl_mapCharFmt_addValInt32(network, "multi_ap_profile", pEP->multiAPProfile);
    }

    ret = wld_wpaSupp_writeConfig(config, cfgFileName);
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "Write config error");

    ret = wld_wpaSupp_deleteConfig(config);
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "Delete config error");

    return SWL_RC_OK;
}

swl_rc_ne wld_wpaSupp_cfgFile_createExt(T_EndPoint* pEP) {
    ASSERTS_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pEP->wpaSupp, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_wpaSupp_cfgFile_create(pEP, pEP->wpaSupp->cfgFile);
}

/**
 * @brief update a configuration file for the wpa_supplicant
 *
 * @param configPath the path of the config file
 * @param key the param to update
 * @param value the key value to set
 * @return void
 */
swl_rc_ne s_cfgFile_update(char* configPath, const char* key, const char* value, bool isGlobal) {
    SAH_TRACEZ_INFO(ME, "%s: updateConfig key:%s, value:%s", configPath, key, value);

    wld_wpaSupp_config_t* config;
    bool ret = wld_wpaSupp_loadConfig(&config, configPath);
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "Bad config");

    ret = wld_wpaSupp_addConfigParam(config, key, value, isGlobal);
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "Update config error");

    ret = wld_wpaSupp_writeConfig(config, configPath);
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "Write config error");

    ret = wld_wpaSupp_deleteConfig(config);
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "Delete config error");

    return SWL_RC_OK;
}

swl_rc_ne wld_wpaSupp_cfgFile_globalConfigUpdate(char* configPath, const char* key, const char* value) {
    ASSERT_STR(configPath, SWL_RC_INVALID_PARAM, ME, "Empty path");
    ASSERT_STR(key, SWL_RC_INVALID_PARAM, ME, "Empty key");
    ASSERT_NOT_NULL(value, SWL_RC_INVALID_PARAM, ME, "NULL");
    return s_cfgFile_update(configPath, key, value, true);
}

swl_rc_ne wld_wpaSupp_cfgFile_networkConfigUpdate(char* configPath, const char* key, const char* value) {
    ASSERT_STR(configPath, false, ME, "Empty path");
    ASSERT_STR(key, false, ME, "Empty key");
    ASSERT_NOT_NULL(value, false, ME, "NULL");
    return s_cfgFile_update(configPath, key, value, false);
}
