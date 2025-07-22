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
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <debug/sahtrace.h>
#include <assert.h>



#include "wld.h"
#include "wld_util.h"
#include "wld_accesspoint.h"
#include "wld_ssid.h"
#include "wld_radio.h"
#include "wld_wps.h"
#include "wld_assocdev.h"
#include "swl/swl_assert.h"
#include "wld_ap_rssiMonitor.h"
#include "Utils/wld_autoCommitMgr.h"

#define ME "apSec"

const char* cstr_AP_EncryptionMode[] = {"Default", "AES", "TKIP", "TKIP-AES", 0};

const char* g_str_wld_ap_td[AP_TD_MAX] = {"WPA3-Personal", "SAE-PK", "WPA3-Enterprise", "EnhancedOpen"};


void wld_ap_sec_doSync(T_AccessPoint* pAP) {
    pAP->pFA->mfn_wvap_sec_sync(pAP, SET);
    wld_autoCommitMgr_notifyVapEdit(pAP);
}

static swl_security_apMode_m s_getFinalModesAvailable(swl_security_apMode_m modesSupportedMask, swl_security_apMode_m modesAvailableMask) {
    if(!modesAvailableMask) {
        modesAvailableMask = modesSupportedMask;
    } else if(modesSupportedMask) {
        modesAvailableMask &= modesSupportedMask;
    }
    return modesAvailableMask;
}

static swl_security_apMode_e s_getFinalModeEnabled(swl_security_apMode_m modesAvailableMask, swl_security_apMode_e modeEnabledId) {
    if(!(SWL_BIT_IS_SET(modesAvailableMask, modeEnabledId))) {
        if(SWL_BIT_IS_SET(modesAvailableMask, SWL_SECURITY_APMODE_WPA3_P) && (modeEnabledId == SWL_SECURITY_APMODE_WPA2_P)) {
            modeEnabledId = SWL_SECURITY_APMODE_WPA3_P;
        } else if(SWL_BIT_IS_SET(modesAvailableMask, SWL_SECURITY_APMODE_WPA2_P)) {
            modeEnabledId = SWL_SECURITY_APMODE_WPA2_P;
        } else if(SWL_BIT_IS_SET(modesAvailableMask, SWL_SECURITY_APMODE_WPA3_P)) {
            modeEnabledId = SWL_SECURITY_APMODE_WPA2_P;
        } else {
            return SWL_SECURITY_APMODE_UNKNOWN;
        }
    }
    return modeEnabledId;
}

bool wld_ap_sec_checkSecConfigParams(const char* oname, amxc_var_t* pParams, swl_security_apMode_m defModesSupported, T_Radio* pRad) {
    SAH_TRACEZ_IN(ME);
    int ret;

    swl_security_apMode_m modesSupportedMask = 0;
    const char* modesSupportedStr = GET_CHAR(pParams, "ModesSupported");
    if(!swl_str_isEmpty(modesSupportedStr)) {
        modesSupportedMask = swl_security_apModeMaskFromString(modesSupportedStr);
    }
    swl_security_apMode_m modesAvailableMask = 0;
    const char* modesAvailableStr = GET_CHAR(pParams, "ModesAvailable");
    if(!swl_str_isEmpty(modesAvailableStr)) {
        modesAvailableMask = swl_security_apModeMaskFromString(modesAvailableStr);
    }
    swl_security_apMode_e modeEnabledId = SWL_SECURITY_APMODE_UNKNOWN;
    const char* modeEnabled = GET_CHAR(pParams, "ModeEnabled");
    if(!swl_str_isEmpty(modeEnabled)) {
        modeEnabledId = swl_security_apModeFromString((char*) modeEnabled);
    }
    const char* wepKey = GET_CHAR(pParams, "WEPKey");
    const char* preSharedKey = GET_CHAR(pParams, "PreSharedKey");
    const char* keyPassPhrase = GET_CHAR(pParams, "KeyPassPhrase");
    const char* saePassphrase = GET_CHAR(pParams, "SAEPassphrase");
    const char* radiusSecret = GET_CHAR(pParams, "RadiusSecret");

    if(!modesSupportedMask) {
        modesSupportedMask = defModesSupported;
    }

    modesAvailableMask = s_getFinalModesAvailable(modesSupportedMask, modesAvailableMask);
    if(modesAvailableMask == 0) {
        SAH_TRACEZ_ERROR(ME, "%s: ModesAvailable (%s) not supported", oname, modesAvailableStr);
        if(pRad == NULL) {
            SAH_TRACEZ_WARNING(ME, "%s: ModesAvailable (%s) conf can not be processed as Security obj is not mapped to any Radio", oname, modesAvailableStr);
            return true;
        } else {
            return false;
        }
    }

    modeEnabledId = s_getFinalModeEnabled(modesAvailableMask, modeEnabledId);
    if(modeEnabledId == SWL_SECURITY_APMODE_UNKNOWN) {
        SAH_TRACEZ_WARNING(ME, "%s: ModeEnabled (%s) not available, can not revert to any WPAx", oname, modeEnabled);
        return false;
    } else if(modeEnabledId != swl_security_apModeFromString((char*) modeEnabled)) {
        SAH_TRACEZ_WARNING(ME, "%s: ModeEnabled (%s) not available: assume reverting to (%s)", oname, modeEnabled, swl_security_apMode_str[modeEnabledId]);
    }

    bool valid = true;
    switch(modeEnabledId) {
    case SWL_SECURITY_APMODE_NONE:
    case SWL_SECURITY_APMODE_OWE:
        break;
    case SWL_SECURITY_APMODE_WEP64:       /* 5/10 */
    case SWL_SECURITY_APMODE_WEP128:      /* 13/26 */
    case SWL_SECURITY_APMODE_WEP128IV:  { /* 16/32 */
        // allow valid wepkey and empty (leading to auto generated wep-128 key)
        swl_security_apMode_e wepModeFromKey = isValidWEPKey(wepKey);
        if(wepModeFromKey != modeEnabledId) {
            SAH_TRACEZ_ERROR(ME, "%s: invalid WEP key (%s) (len:%zu) for mode(%s) (expected(%s))",
                             oname, wepKey, swl_str_len(wepKey),
                             swl_security_apModeToString(modeEnabledId, SWL_SECURITY_APMODEFMT_DEFAULT),
                             swl_security_apModeToString(wepModeFromKey, SWL_SECURITY_APMODEFMT_DEFAULT));
            valid = false;
        }
        break;
    }
    case SWL_SECURITY_APMODE_WPA_P:
        if(!((!swl_str_isEmpty(preSharedKey) && isValidPSKKey(preSharedKey)) ||
             (!swl_str_isEmpty(keyPassPhrase) && isValidAESKey(keyPassPhrase, PSK_KEY_SIZE_LEN - 1)))) {
            SAH_TRACEZ_ERROR(ME, "%s: invalid Key(PSK:%s/AES:%s) in WPA mode", oname, preSharedKey, keyPassPhrase);
            valid = false;
        }
        break;
    case SWL_SECURITY_APMODE_WPA2_P:
    case SWL_SECURITY_APMODE_WPA_WPA2_P:
    case SWL_SECURITY_APMODE_WPA2_WPA3_P:
        if(swl_str_isEmpty(keyPassPhrase) || !isValidAESKey(keyPassPhrase, PSK_KEY_SIZE_LEN - 1)) {
            SAH_TRACEZ_ERROR(ME, "%s: invalid Key(AES:%s) in WPA2 mode", oname, keyPassPhrase);
            valid = false;
        }
        break;
    case SWL_SECURITY_APMODE_WPA3_P:
        if(swl_str_isEmpty(saePassphrase)) {
            if(swl_str_isEmpty(keyPassPhrase) || !isValidAESKey(keyPassPhrase, PSK_KEY_SIZE_LEN - 1)) {
                SAH_TRACEZ_ERROR(ME, "%s: invalid Key(AES:%s) in WPA3 mode", oname, keyPassPhrase);
                valid = false;
            }
        } else if(!isValidAESKey(saePassphrase, SAE_KEY_SIZE_LEN)) {
            SAH_TRACEZ_ERROR(ME, "%s: invalid Key(SAE:%s) in WPA3 mode", oname, saePassphrase);
            valid = false;
        }
        break;
    case SWL_SECURITY_APMODE_WPA_E:
        if(!((!swl_str_isEmpty(radiusSecret) && isValidPSKKey(radiusSecret)) ||
             (!swl_str_isEmpty(keyPassPhrase) && isValidAESKey(keyPassPhrase, PSK_KEY_SIZE_LEN - 1)))) {
            SAH_TRACEZ_ERROR(ME, "%s: invalid Key(RadSec:%s/AES:%s) in WPA-E mode", oname, radiusSecret, keyPassPhrase);
            valid = false;
        }
        break;
    case SWL_SECURITY_APMODE_WPA2_E:
    case SWL_SECURITY_APMODE_WPA_WPA2_E:
    case SWL_SECURITY_APMODE_WPA2_WPA3_E:
    case SWL_SECURITY_APMODE_WPA3_E:
        // The only technical limitation is that shared secrets must be greater than 0 in length!
        ret = swl_str_len(radiusSecret);
        if((ret < 8) || (ret >= 64)) {
            SAH_TRACEZ_ERROR(ME, "%s: invalid Radius secret(%s) length(%d) in WPA/2/3-E mode (req length(8-63))", oname, radiusSecret, ret);
            valid = false;
        }
        break;
    default:
        SAH_TRACEZ_ERROR(ME, "%s: Unknown mode %s", oname, modeEnabled);
        valid = false;
        break;
    }

    SAH_TRACEZ_OUT(ME);
    return valid;
}

amxd_status_t _wld_ap_validateSecurity_ovf(amxd_object_t* object,
                                           amxd_param_t* param _UNUSED,
                                           amxd_action_t reason _UNUSED,
                                           const amxc_var_t* const args _UNUSED,
                                           amxc_var_t* const retval _UNUSED,
                                           void* priv _UNUSED) {
    SAH_TRACEZ_IN(ME);
    amxd_object_t* parentObj = amxd_object_get_parent(object);
    ASSERTS_EQUALS(amxd_object_get_type(parentObj), amxd_object_instance, amxd_status_ok, ME, "Not instance");
    T_AccessPoint* pAP = wld_ap_fromObj(parentObj);
    T_Radio* pRad = pAP ? pAP->pRadio : NULL;
    const char* oname = ((pAP != NULL) ? pAP->alias : amxd_object_get_name(amxd_object_get_parent(object), AMXD_OBJECT_NAMED));
    swl_security_apMode_m defModesSupported = (pAP ? pAP->secModesSupported : 0);
    int ret;

    amxc_var_t params;
    amxc_var_init(&params);
    amxc_var_set_type(&params, AMXC_VAR_ID_HTABLE);
    swla_object_paramsToMap(&params, object);

    bool valid = wld_ap_sec_checkSecConfigParams(oname, &params, defModesSupported, pRad);
    amxd_status_t status = (valid ? amxd_status_ok : amxd_status_invalid_value);

    amxc_var_clean(&params);

    SAH_TRACEZ_OUT(ME);
    return status;
}

amxd_status_t _wld_ap_validateWEPKey_pvf(amxd_object_t* object _UNUSED,
                                         amxd_param_t* param,
                                         amxd_action_t reason _UNUSED,
                                         const amxc_var_t* const args,
                                         amxc_var_t* const retval _UNUSED,
                                         void* priv _UNUSED) {
    ASSERTS_FALSE(amxc_var_is_null(args), amxd_status_invalid_value, ME, "invalid");
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    const char* oname = ((pAP != NULL) ? pAP->alias : amxd_object_get_name(amxd_object_get_parent(object), AMXD_OBJECT_NAMED));
    amxd_status_t status = amxd_status_invalid_value;
    const char* currentValue = amxc_var_constcast(cstring_t, &param->value);
    ASSERT_NOT_NULL(currentValue, status, ME, "NULL");
    char* newValue = amxc_var_dyncast(cstring_t, args);
    // allow valid wepkey and empty (leading to auto generated wep-128 key)
    if(swl_str_matches(currentValue, newValue) || swl_str_isEmpty(newValue) || (isValidWEPKey(newValue) != SWL_SECURITY_APMODE_UNKNOWN)) {
        status = amxd_status_ok;
    } else {
        SAH_TRACEZ_ERROR(ME, "%s: invalid WEPKey (%s)", oname, newValue);
    }
    free(newValue);

    SAH_TRACEZ_OUT(ME);
    return status;
}

static void s_setWEPKey_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pAP, , ME, "Invalid AP Ctx");

    char* key = amxc_var_dyncast(cstring_t, newValue); // Get the Key
    ASSERT_NOT_NULL(key, , ME, "NULL");
    if(swl_str_isEmpty(key)) {
        /* No WEP key filled in... Generate a valid WEP-128 key! */
        get_randomstr((unsigned char*) pAP->WEPKey, 13);
        SAH_TRACEZ_INFO(ME, "%s: auto generated WEPKey (%s)", pAP->alias, pAP->WEPKey);
        swl_typeCharPtr_commitObjectParam(object, amxd_param_get_name(param), pAP->WEPKey);
    } else if(!swl_str_matches(pAP->WEPKey, key)) {
        SAH_TRACEZ_INFO(ME, "%s: new configured WEPKey (%s)", pAP->alias, key);
        swl_str_copy(pAP->WEPKey, sizeof(pAP->WEPKey), key);
    } else {
        free(key);
        return;
    }

    wld_ap_sec_doSync(pAP);
    free(key);

    SAH_TRACEZ_OUT(ME);
}

static void s_setModesAvailable_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pAP, , ME, "Invalid AP Ctx");

    const char* modesAvailable = amxc_var_constcast(cstring_t, newValue);
    swl_security_apMode_m apModes = swl_security_apModeMaskFromString(modesAvailable);
    swl_security_apMode_m finalApModes = s_getFinalModesAvailable(pAP->secModesSupported, apModes);
    if(finalApModes != apModes) {
        char TBuf[128] = {0};
        swl_security_apModeMaskToString(TBuf, sizeof(TBuf), SWL_SECURITY_APMODEFMT_LEGACY, pAP->secModesAvailable);
        swl_typeCharPtr_commitObjectParam(object, amxd_param_get_name(param), TBuf);
    }
    ASSERTI_NOT_EQUALS(finalApModes, pAP->secModesAvailable, , ME, "%s: EQUAL (0x%x)", pAP->alias, finalApModes);
    pAP->secModesAvailable = finalApModes;

    SAH_TRACEZ_INFO(ME, "%s: SecModesAvailable selected(0x%x) supported(0x%x) final(0x%x)", pAP->alias, apModes, pAP->secModesSupported, pAP->secModesAvailable);

    SAH_TRACEZ_OUT(ME);
}

static void s_setModeEnabled_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pAP, , ME, "Invalid AP Ctx");

    const char* mode = amxc_var_constcast(cstring_t, newValue);
    swl_security_apMode_e modeId = swl_security_apModeFromString((char*) mode);
    swl_security_apMode_e finalModeId = s_getFinalModeEnabled(pAP->secModesAvailable, modeId);
    if(swl_security_isApModeWEP(finalModeId)) {
        finalModeId = isValidWEPKey(pAP->WEPKey);
    }
    const char* finalModeStr = swl_security_apModeToString(finalModeId, SWL_SECURITY_APMODEFMT_LEGACY);
    if(finalModeId != modeId) {
        swl_typeCharPtr_commitObjectParam(object, amxd_param_get_name(param), (char*) finalModeStr);
    }
    ASSERTI_NOT_EQUALS(finalModeId, pAP->secModeEnabled, , ME, "%s: EQUAL mode %d", pAP->alias, finalModeId);
    pAP->secModeEnabled = finalModeId;

    SAH_TRACEZ_INFO(ME, "%s: Security Mode Enable %s %d", pAP->alias, finalModeStr, finalModeId);
    wld_ap_sec_doSync(pAP);
    if(pAP->pSSID) {
        wld_mld_setLinkConfigured(pAP->pSSID->pMldLink, false);
    }

    SAH_TRACEZ_OUT(ME);
}

amxd_status_t _wld_ap_validatePreSharedKey_pvf(amxd_object_t* object _UNUSED,
                                               amxd_param_t* param,
                                               amxd_action_t reason _UNUSED,
                                               const amxc_var_t* const args,
                                               amxc_var_t* const retval _UNUSED,
                                               void* priv _UNUSED) {
    ASSERTS_FALSE(amxc_var_is_null(args), amxd_status_invalid_value, ME, "invalid");
    SAH_TRACEZ_IN(ME);

    amxd_status_t status = amxd_status_invalid_value;
    const char* currentValue = amxc_var_constcast(cstring_t, &param->value);
    ASSERT_NOT_NULL(currentValue, status, ME, "NULL");
    char* newValue = amxc_var_dyncast(cstring_t, args);
    if(swl_str_isEmpty(newValue) || swl_str_matches(currentValue, newValue) || isValidPSKKey(newValue)) {
        status = amxd_status_ok;
    } else {
        SAH_TRACEZ_ERROR(ME, "invalid PreSharedKey (%s)", newValue);
    }
    free(newValue);

    SAH_TRACEZ_OUT(ME);
    return status;
}

amxd_status_t _wld_ap_validateKeyPassPhrase_pvf(amxd_object_t* object _UNUSED,
                                                amxd_param_t* param,
                                                amxd_action_t reason _UNUSED,
                                                const amxc_var_t* const args,
                                                amxc_var_t* const retval _UNUSED,
                                                void* priv _UNUSED) {
    ASSERTS_FALSE(amxc_var_is_null(args), amxd_status_invalid_value, ME, "invalid");
    amxd_status_t status = amxd_status_invalid_value;
    const char* currentValue = amxc_var_constcast(cstring_t, &param->value);
    ASSERT_NOT_NULL(currentValue, status, ME, "NULL");
    char* newValue = amxc_var_dyncast(cstring_t, args);
    ASSERT_NOT_NULL(newValue, status, ME, "NULL");
    if(swl_str_isEmpty(newValue) || swl_str_matches(currentValue, newValue) || isValidAESKey(newValue, PSK_KEY_SIZE_LEN - 1)) {
        status = amxd_status_ok;
    } else {
        SAH_TRACEZ_ERROR(ME, "Invalid AES Key (%s)", newValue);
    }
    free(newValue);
    return status;
}

static void s_setKeyPassPhrase_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pAP, , ME, "Invalid AP Ctx");

    char* newPassphrase = amxc_var_dyncast(cstring_t, newValue);
    SAH_TRACEZ_INFO(ME, "%s: WPA-AES %s", pAP->alias, newPassphrase);
    T_SSID* pSSID = pAP->pSSID;
    bool passUpdated = (pSSID && !wldu_key_matches(pSSID->SSID, newPassphrase, pAP->keyPassPhrase));
    swl_str_copy(pAP->keyPassPhrase, sizeof(pAP->keyPassPhrase), newPassphrase);
    if(passUpdated) {
        wld_ap_sec_doSync(pAP);
        wld_mld_setLinkConfigured(pSSID->pMldLink, false);
    } else {
        SAH_TRACEZ_INFO(ME, "%s: Same key is used, no need to sync %s", pAP->alias, newPassphrase);
    }
    free(newPassphrase);

    SAH_TRACEZ_OUT(ME);
}

amxd_status_t _wld_ap_validateSaePassphrase_pvf(amxd_object_t* object _UNUSED,
                                                amxd_param_t* param,
                                                amxd_action_t reason _UNUSED,
                                                const amxc_var_t* const args,
                                                amxc_var_t* const retval _UNUSED,
                                                void* priv _UNUSED) {
    ASSERTS_FALSE(amxc_var_is_null(args), amxd_status_invalid_value, ME, "invalid");
    amxd_status_t status = amxd_status_invalid_value;
    const char* currentValue = amxc_var_constcast(cstring_t, &param->value);
    ASSERT_NOT_NULL(currentValue, status, ME, "NULL");
    char* newValue = amxc_var_dyncast(cstring_t, args);
    ASSERT_NOT_NULL(newValue, status, ME, "NULL");
    if(swl_str_isEmpty(newValue) || swl_str_matches(currentValue, newValue) || isValidAESKey(newValue, SAE_KEY_SIZE_LEN)) {
        status = amxd_status_ok;
    } else {
        SAH_TRACEZ_ERROR(ME, "Invalid SAE Key (%s)", newValue);
    }
    free(newValue);
    return status;
}

static void s_setSaePassphrase_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pAP, , ME, "Invalid AP Ctx");

    char* newPassphrase = amxc_var_dyncast(cstring_t, newValue);
    SAH_TRACEZ_INFO(ME, "%s: WPA-SAE %s", pAP->alias, newPassphrase);
    T_SSID* pSSID = pAP->pSSID;
    bool passUpdated = (pSSID && !wldu_key_matches(pSSID->SSID, newPassphrase, pAP->saePassphrase));
    swl_str_copy(pAP->saePassphrase, sizeof(pAP->saePassphrase), newPassphrase);
    if(passUpdated) {
        wld_ap_sec_doSync(pAP);
        wld_mld_setLinkConfigured(pSSID->pMldLink, false);
    } else {
        SAH_TRACEZ_INFO(ME, "%s: Same key is used, no need to sync %s", pAP->alias, newPassphrase);
    }
    free(newPassphrase);

    SAH_TRACEZ_OUT(ME);
}

static void s_setCommonSecurityConf_ocf(void* priv _UNUSED, amxd_object_t* object, const amxc_var_t* const newParamValues) {
    SAH_TRACEZ_IN(ME);

    bool needSyncSec = false;
    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pAP, , ME, "Invalid AP Ctx");
    amxc_var_for_each(newValue, newParamValues) {
        char* valStr = NULL;
        const char* pname = amxc_var_key(newValue);
        if(swl_str_matches(pname, "PreSharedKey")) {
            valStr = amxc_var_dyncast(cstring_t, newValue);
            SAH_TRACEZ_INFO(ME, "WPA-TKIP Psk(%s)", valStr);
            swl_str_copy(pAP->preSharedKey, sizeof(pAP->preSharedKey), valStr);
        } else if(swl_str_matches(pname, "RadiusServerIPAddr")) {
            valStr = amxc_var_dyncast(cstring_t, newValue);
            swl_str_copy(pAP->radiusServerIPAddr, sizeof(pAP->radiusServerIPAddr), valStr);
        } else if(swl_str_matches(pname, "RadiusServerPort")) {
            pAP->radiusServerPort = amxc_var_dyncast(int32_t, newValue);
        } else if(swl_str_matches(pname, "RadiusSecret")) {
            valStr = amxc_var_dyncast(cstring_t, newValue);
            swl_str_copy(pAP->radiusSecret, sizeof(pAP->radiusSecret), valStr);
        } else if(swl_str_matches(pname, "RadiusDefaultSessionTimeout")) {
            pAP->radiusDefaultSessionTimeout = amxc_var_dyncast(int32_t, newValue);
        } else if(swl_str_matches(pname, "RadiusOwnIPAddress")) {
            valStr = amxc_var_dyncast(cstring_t, newValue);
            swl_str_copy(pAP->radiusOwnIPAddress, sizeof(pAP->radiusOwnIPAddress), valStr);
        } else if(swl_str_matches(pname, "RadiusNASIdentifier")) {
            valStr = amxc_var_dyncast(cstring_t, newValue);
            swl_str_copy(pAP->radiusNASIdentifier, sizeof(pAP->radiusNASIdentifier), valStr);
        } else if(swl_str_matches(pname, "RadiusCalledStationId")) {
            valStr = amxc_var_dyncast(cstring_t, newValue);
            swl_str_copy(pAP->radiusCalledStationId, sizeof(pAP->radiusCalledStationId), valStr);
        } else if(swl_str_matches(pname, "RadiusChargeableUserId")) {
            pAP->radiusChargeableUserId = amxc_var_dyncast(bool, newValue);
        } else if(swl_str_matches(pname, "OWETransitionInterface")) {
            valStr = amxc_var_dyncast(cstring_t, newValue);
            swl_str_copy(pAP->oweTransModeIntf, sizeof(pAP->oweTransModeIntf), valStr);
        } else if(swl_str_matches(pname, "MFPConfig")) {
            const char* mfpStr = amxc_var_constcast(cstring_t, newValue);
            pAP->mfpConfig = swl_security_mfpModeFromString(mfpStr);
        } else if(swl_str_matches(pname, "SHA256Enable")) {
            pAP->SHA256Enable = amxc_var_dyncast(bool, newValue);
        } else if(swl_str_matches(pname, "TransitionDisable")) {
            const char* tdStr = amxc_var_constcast(cstring_t, newValue);
            pAP->transitionDisable = swl_conv_charToMask(tdStr, g_str_wld_ap_td, AP_TD_MAX);
        } else if(swl_str_matches(pname, "RekeyingInterval")) {
            pAP->rekeyingInterval = amxc_var_dyncast(int32_t, newValue);
        } else if(swl_str_matches(pname, "EncryptionMode")) {
            const char* newMode = amxc_var_constcast(cstring_t, newValue);
            pAP->encryptionModeEnabled = swl_conv_charToEnum(newMode, cstr_AP_EncryptionMode, APEMI_MAX, APEMI_DEFAULT);
        } else if(swl_str_matches(pname, "SPPAmsdu")) {
            pAP->sppAmsdu = amxc_var_dyncast(int32_t, newValue);
        } else {
            continue;
        }
        char* pvalue = swl_typeCharPtr_fromVariantDef(newValue, NULL);
        SAH_TRACEZ_INFO(ME, "%s: setting %s = %s", pAP->alias, pname, pvalue);
        free(pvalue);
        free(valStr);
        needSyncSec = true;
    }

    if(needSyncSec) {
        wld_ap_sec_doSync(pAP);
        if(pAP->pSSID) {
            wld_mld_setLinkConfigured(pAP->pSSID->pMldLink, false);
        }
    }

    SAH_TRACEZ_OUT(ME);
}

SWLA_DM_HDLRS(sApSecDmHdlrs,
              ARR(SWLA_DM_PARAM_HDLR("ModesAvailable", s_setModesAvailable_pwf),
                  SWLA_DM_PARAM_HDLR("WEPKey", s_setWEPKey_pwf),
                  SWLA_DM_PARAM_HDLR("KeyPassPhrase", s_setKeyPassPhrase_pwf),
                  SWLA_DM_PARAM_HDLR("SAEPassphrase", s_setSaePassphrase_pwf),
                  SWLA_DM_PARAM_HDLR("ModeEnabled", s_setModeEnabled_pwf),
                  ),
              .objChangedCb = s_setCommonSecurityConf_ocf,
              );

void _wld_ap_security_setConf_ocf(const char* const sig_name,
                                  const amxc_var_t* const data,
                                  void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sApSecDmHdlrs, sig_name, data, priv);
}

struct {
    swl_security_apMode_e secModeMixed;
    swl_security_apMode_e secModeShared;
    swl_security_apMode_e secModeCompl;
} sMixedSecModesCombs[] = {
    /* partial match */
    {SWL_SECURITY_APMODE_WPA_WPA2_P, SWL_SECURITY_APMODE_WPA_P, SWL_SECURITY_APMODE_WPA2_P, },
    {SWL_SECURITY_APMODE_WPA_WPA2_P, SWL_SECURITY_APMODE_WPA2_P, SWL_SECURITY_APMODE_WPA_P, },
    {SWL_SECURITY_APMODE_WPA_WPA2_P, SWL_SECURITY_APMODE_WPA_WPA2_P, SWL_SECURITY_APMODE_WPA2_P, },
    {SWL_SECURITY_APMODE_WPA2_WPA3_P, SWL_SECURITY_APMODE_WPA2_P, SWL_SECURITY_APMODE_WPA3_P, },
    {SWL_SECURITY_APMODE_WPA2_WPA3_P, SWL_SECURITY_APMODE_WPA3_P, SWL_SECURITY_APMODE_WPA2_P, },
    {SWL_SECURITY_APMODE_WPA2_WPA3_P, SWL_SECURITY_APMODE_WPA2_WPA3_P, SWL_SECURITY_APMODE_WPA3_P, },
};

/*
 * compare the security modes of two APs and identify the shared and the complementary modes
 */
static bool s_compareSecModes(T_AccessPoint* pAP1, T_AccessPoint* pAP2, swl_security_apMode_e* pSecModeShared, swl_security_apMode_e* pSecModeCompl) {
    W_SWL_SETPTR(pSecModeShared, SWL_SECURITY_APMODE_UNSUPPORTED);
    W_SWL_SETPTR(pSecModeCompl, SWL_SECURITY_APMODE_UNSUPPORTED);
    if((pAP1 == NULL) || (pAP1->pSSID == NULL) || (pAP2 == NULL) || (pAP1->pSSID == NULL)) {
        return false;
    }
    swl_security_apMode_e secMode1 = pAP1->secModeEnabled;
    swl_security_apMode_e secMode2 = pAP2->secModeEnabled;
    for(uint32_t i = 0; i < SWL_ARRAY_SIZE(sMixedSecModesCombs); i++) {
        if(((secMode1 == sMixedSecModesCombs[i].secModeMixed) && (secMode2 == sMixedSecModesCombs[i].secModeShared)) ||
           ((secMode2 == sMixedSecModesCombs[i].secModeMixed) && (secMode1 == sMixedSecModesCombs[i].secModeShared))) {
            W_SWL_SETPTR(pSecModeShared, sMixedSecModesCombs[i].secModeShared);
            W_SWL_SETPTR(pSecModeCompl, sMixedSecModesCombs[i].secModeCompl);
            return true;
        }
    }
    if(secMode1 == secMode2) {
        W_SWL_SETPTR(pSecModeShared, secMode1);
        W_SWL_SETPTR(pSecModeCompl, SWL_SECURITY_APMODE_UNKNOWN);
        return true;
    }
    return false;
}

/*
 * @brief check whether two APs are sharing same security configurations (secMode, keypass)
 * This can be used to check applicable sec conf over APMLD links
 */
bool wld_ap_sec_checkSharedSecConfigs(T_AccessPoint* pAP1, T_AccessPoint* pAP2) {
    swl_security_apMode_e secModeShared;
    swl_security_apMode_e secModeCompl;
    bool modeMatch = s_compareSecModes(pAP1, pAP2, &secModeShared, &secModeCompl);
    ASSERTI_TRUE(modeMatch, modeMatch, ME, "No shared sec mode");
    T_AccessPoint* pApRef = (pAP1->secModeEnabled == secModeShared) ? pAP1 : ((pAP2->secModeEnabled == secModeShared) ? pAP2 : NULL);
    ASSERTI_NOT_NULL(pApRef, false, ME, "No AP (%s/%s) using shared sec Mode %s", pAP1->name, pAP2->name, swl_security_apMode_str[secModeShared]);
    bool keyMatch = false;
    switch(secModeShared) {
    case SWL_SECURITY_APMODE_NONE:
    case SWL_SECURITY_APMODE_OWE: {
        keyMatch = true;
        break;
    }
    case SWL_SECURITY_APMODE_WPA_P:
    case SWL_SECURITY_APMODE_WPA2_P:
    case SWL_SECURITY_APMODE_WPA_WPA2_P:
    case SWL_SECURITY_APMODE_WPA3_P:
    case SWL_SECURITY_APMODE_WPA2_WPA3_P: {
        if((secModeShared == SWL_SECURITY_APMODE_WPA3_P) ||
           (secModeCompl == SWL_SECURITY_APMODE_WPA3_P)) {
            if(!swl_str_isEmpty(pApRef->saePassphrase)) {
                keyMatch = swl_str_matches(pAP1->saePassphrase, pAP2->saePassphrase);
            } else if(!swl_str_isEmpty(pApRef->keyPassPhrase)) {
                keyMatch = swl_str_matches(pAP1->keyPassPhrase, pAP2->keyPassPhrase);
            }
        } else {
            if(!swl_str_isEmpty(pApRef->keyPassPhrase)) {
                keyMatch = swl_str_matches(pAP1->keyPassPhrase, pAP2->keyPassPhrase);
            } else if(!swl_str_isEmpty(pApRef->preSharedKey)) {
                keyMatch = swl_str_matches(pAP1->preSharedKey, pAP2->preSharedKey);
            }
        }
        break;
    }
    /* TODO: manage WPA enterprise sec modes */
    case SWL_SECURITY_APMODE_UNKNOWN:
    case SWL_SECURITY_APMODE_UNSUPPORTED:
    default: {
        modeMatch = false;
        break;
    }
    }

    return (modeMatch && keyMatch);
}

