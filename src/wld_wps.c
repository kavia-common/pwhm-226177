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
#include <stdlib.h>
#include <string.h>
#include <debug/sahtrace.h>
#include <assert.h>



#include "wld.h"
#include "wld_util.h"
#include "wld_wps.h"
#include "wld_radio.h"
#include "wld_accesspoint.h"
#include "wld_eventing.h"
#include "swl/swl_assert.h"
#include "swl/swl_string.h"
#include "swl/swl_uuid.h"
#include "Utils/wld_autoCommitMgr.h"

#define ME "wps"

/*
 * @brief names of all WPS configuration methods as per tr-181-2-15-1-usp
 * Device.WiFi.AccessPoint.{i}.WPS.ConfigMethodsSupported
 * */
const char* cstr_WPS_CM_Supported[] = {
    "USBFlashDrive",
    "Ethernet",
    "Label",
    "Display",
    "ExternalNFCToken",
    "IntegratedNFCToken",
    "NFCInterface",
    "PushButton",
    "PIN",
    "PhysicalPushButton",
    "PhysicalDisplay",
    "VirtualPushButton",
    "VirtualDisplay",
    0,
};
SWL_ASSERT_STATIC(SWL_ARRAY_SIZE(cstr_WPS_CM_Supported) == (WPS_CFG_MTHD_MAX + 1), "cstr_WPS_CM_Supported not correctly defined");

const char* cstr_AP_WPS_VERSUPPORTED[] = {"AUTO", "Auto", "auto", "1.0", "1", "2.0", "2",
    0};
const int ci_AP_WPS_VERSUPPORTED[] = {
    APWPSVERSI_AUTO, APWPSVERSI_AUTO, APWPSVERSI_AUTO,
    APWPSVERSI_10, APWPSVERSI_10,
    APWPSVERSI_20, APWPSVERSI_20,
    APWPSVERSI_UNKNOWN,
    0
};

const char* cstr_AP_WPS_Status[] = {"Disabled", "Error", "Configured", "Unconfigured", "SetupLocked", 0};
SWL_ASSERT_STATIC(SWL_ARRAY_SIZE(cstr_AP_WPS_Status) == (APWPS_STATUS_MAX + 1), "cstr_AP_WPS_Status not correctly defined");

static void s_sendStateChangeEvent(T_SSID* pSSID, const char* wpsState, const char* changeReason) {
    ASSERT_NOT_NULL(pSSID, , ME, "NULL");
    wld_wps_changeEvent_t changeEvent = {
        .pSSID = pSSID,
        .wpsState = wpsState,
        .changeReason = changeReason
    };
    wld_event_trigger_callback(gWld_queue_wps_onStateChange, &changeEvent);
}

bool wld_wps_isPairingTimerRunning(wld_wpsSessionInfo_t* pCtx) {
    ASSERTS_NOT_NULL(pCtx, false, ME, "NULL");
    amxp_timer_state_t timerSt = amxp_timer_get_state(pCtx->sessionTimer);
    return ((timerSt == amxp_timer_running) || (timerSt == amxp_timer_started));
}

amxd_status_t doCancelPairing(amxd_object_t* object) {
    ASSERTI_NOT_NULL(object, amxd_status_unknown_error, ME, "NULL");
    wld_wpsSessionInfo_t* pWpsSessionInfo = (wld_wpsSessionInfo_t*) object->priv;
    ASSERTI_NOT_NULL(pWpsSessionInfo, amxd_status_unknown_error, ME, "NULL");
    bool hasActiveTimer = wld_wps_isPairingTimerRunning(pWpsSessionInfo);
    wld_wps_clearPairingTimer(pWpsSessionInfo);
    T_AccessPoint* pAP = wld_ap_fromObj(pWpsSessionInfo->intfObj);
    ASSERTI_NOT_NULL(pAP, amxd_status_ok, ME, "Not ap wps");
    char strbuf[] = "STOP";
    // If a timer is running... stop and destroy it also.
    if(pAP->WPS_PBC_Delay.timer) {
        amxp_timer_delete(&pAP->WPS_PBC_Delay.timer);
        pAP->WPS_PBC_Delay.timer = NULL;
        pAP->WPS_PBC_Delay.intf.vap = NULL;
    }
    if(hasActiveTimer) {
        pAP->pFA->mfn_wvap_wps_sync(pAP, strbuf, SWL_ARRAY_SIZE(strbuf), SET);

        SAH_TRACEZ_ERROR(ME, "WPS cancel %s", pAP->alias);
    }
    return amxd_status_ok;
}

const char* wld_wps_ConfigMethod_to_string(wld_wps_cfgMethod_e value) {
    ASSERT_TRUE(value < WPS_CFG_MTHD_MAX, "", ME, "invalid %d", value);
    return cstr_WPS_CM_Supported[value];
}

bool wld_wps_ConfigMethods_mask_to_string(amxc_string_t* output, const wld_wps_cfgMethod_m configMethods) {
    ASSERTS_NOT_NULL(output, false, ME, "NULL");
    if(configMethods == 0) {
        amxc_string_clean(output);
        amxc_string_appendf(output, "None");
        return true;
    }
    char buffer[256] = {0};
    bool ret = swl_conv_maskToChar(buffer, sizeof(buffer), configMethods, cstr_WPS_CM_Supported, SWL_ARRAY_SIZE(cstr_WPS_CM_Supported));
    amxc_string_set(output, buffer);
    return ret;
}

bool wld_wps_ConfigMethods_string_to_mask(wld_wps_cfgMethod_m* output, const char* input, const char separator) {
    ASSERTS_NOT_NULL(output, false, ME, "NULL");
    *output = swl_conv_charToMaskSep(input, cstr_WPS_CM_Supported, WPS_CFG_MTHD_MAX, separator, NULL);
    return true;
}

static amxd_status_t s_setCommandReply(amxc_var_t* retval, swl_usp_cmdStatus_ne cmdStatus, amxd_status_t defRc) {
    amxd_status_t rc = amxd_status_ok;
    if(amxc_var_add_key(cstring_t, retval, "Status", swl_uspCmdStatus_toString(cmdStatus)) == NULL) {
        /*
         * If failing to add command status, then return amx error
         * otherwise return amx success to allow returning the filled retval variant
         * including the status (potentially error) description
         */
        rc = defRc;
    }
    return rc;
}

void wld_wps_pushButton_reply(uint64_t call_id, swl_usp_cmdStatus_ne cmdStatus) {
    amxc_var_t retval;
    amxc_var_init(&retval);
    amxc_var_set_type(&retval, AMXC_VAR_ID_HTABLE);
    amxd_status_t rc = s_setCommandReply(&retval, cmdStatus, amxd_status_unknown_error);
    amxd_function_deferred_done(call_id, rc, NULL, &retval);
    amxc_var_clean(&retval);
}

void wld_wps_clearPairingTimer(wld_wpsSessionInfo_t* pCtx) {
    ASSERTS_NOT_NULL(pCtx, , ME, "NULL");
    memset(pCtx->peerMac.bMac, 0, SWL_MAC_BIN_LEN);
    if(pCtx->sessionTimer != NULL) {
        amxp_timer_delete(&pCtx->sessionTimer);
        pCtx->sessionTimer = NULL;
    }
}

static void s_pairingTimeoutHandler(amxp_timer_t* timer _UNUSED, void* userdata) {
    wld_wpsSessionInfo_t* pCtx = (wld_wpsSessionInfo_t*) userdata;
    ASSERTS_NOT_NULL(pCtx, , ME, "NULL");
    ASSERT_NOT_NULL(pCtx->intfObj, , ME, "NULL");
    const char* name = amxd_object_get_name(pCtx->intfObj, AMXD_OBJECT_NAMED);
    SAH_TRACEZ_ERROR(ME, "%s: wps pairing timeout (triggered by safety timer)", name);
    doCancelPairing(amxd_object_get(pCtx->intfObj, "WPS"));
    wld_wps_sendPairingNotification(pCtx->intfObj, NOTIFY_PAIRING_DONE, WPS_CAUSE_TIMEOUT, NULL, NULL);
}

void s_startPairingTimer(wld_wpsSessionInfo_t* pCtx) {
    ASSERTS_NOT_NULL(pCtx, , ME, "NULL");
    if(pCtx->sessionTimer == NULL) {
        amxp_timer_new(&pCtx->sessionTimer, s_pairingTimeoutHandler, pCtx);
    }
    ASSERT_NOT_NULL(pCtx->sessionTimer, , ME, "NULL");
    amxp_timer_start(pCtx->sessionTimer, WPS_SESSION_TIMEOUT * 1000);
    memset(pCtx->peerMac.bMac, 0, SWL_MAC_BIN_LEN);
}

static void s_sendNotif(amxd_object_t* wps, const char* name, const char* reason, const char* macAddress, T_WPSCredentials* credentials) {
    amxc_var_t notifMap;
    amxc_var_init(&notifMap);
    amxc_var_set_type(&notifMap, AMXC_VAR_ID_HTABLE);

    amxc_var_add_key(cstring_t, &notifMap, SWL_WPS_NOTIF_PARAM_REASON, reason);
    if((macAddress != NULL) && swl_mac_charIsValidStaMac((swl_macChar_t*) macAddress)) {
        amxc_var_add_key(cstring_t, &notifMap, SWL_WPS_NOTIF_PARAM_MACADDRESS, macAddress);
    }
    if(credentials) {
        amxc_var_add_key(cstring_t, &notifMap, SWL_WPS_NOTIF_PARAM_SSID, credentials->ssid);
        amxc_var_add_key(cstring_t, &notifMap, SWL_WPS_NOTIF_PARAM_SECURITYMODE, swl_security_apModeToString(credentials->secMode, SWL_SECURITY_APMODEFMT_LEGACY));
        amxc_var_add_key(cstring_t, &notifMap, SWL_WPS_NOTIF_PARAM_KEYPASSPHRASE, credentials->key);
    }
    SAH_TRACEZ_WARNING(ME, "notif %s, reason=%s, macAddress=%s}", name, reason, macAddress);

    amxd_object_trigger_signal(wps, name, &notifMap);
    amxc_var_clean(&notifMap);
}

void wld_wps_sendPairingNotification(amxd_object_t* object, uint32_t type, const char* reason, const char* macAddress, T_WPSCredentials* credentials) {
    amxd_object_t* wps = amxd_object_get(object, "WPS");
    ASSERT_NOT_NULL(wps, , ME, "NULL");
    wld_wpsSessionInfo_t* pCtx = wps->priv;
    ASSERT_NOT_NULL(pCtx, , ME, "NULL");
    char* name = WPS_PAIRING_NO_MESSAGE;
    bool inProgress = false;
    bool prevInProgress = pCtx->WPS_PairingInProgress;

    switch(type) {
    case (NOTIFY_PAIRING_READY):
        name = WPS_PAIRING_READY;
        inProgress = true;
        break;
    case (NOTIFY_PAIRING_DONE):
        name = WPS_PAIRING_DONE;
        inProgress = false;
        break;
    case (NOTIFY_PAIRING_ERROR):
        name = WPS_PAIRING_ERROR;
        inProgress = false;
        break;
    }

    const char* intfName = amxd_object_get_name(object, AMXD_OBJECT_NAMED);
    if(!inProgress) {
        wld_wps_clearPairingTimer(pCtx);
        ASSERTI_FALSE(((type == NOTIFY_PAIRING_DONE) && !prevInProgress), , ME, "%s: received done(%s) while not in progress, ignore", intfName, reason);
    } else if(!prevInProgress) {
        s_startPairingTimer(pCtx);
    }

    s_sendNotif(wps, name, reason, macAddress, credentials);

    T_SSID* pSSID = NULL;
    if(debugIsVapPointer(object->priv)) {
        T_AccessPoint* pAP = (T_AccessPoint*) object->priv;
        pSSID = pAP->pSSID;
    } else if(debugIsEpPointer(object->priv)) {
        T_EndPoint* pEP = (T_EndPoint*) object->priv;
        pSSID = pEP->pSSID;
    }
    s_sendStateChangeEvent(pSSID, name, reason);

    if(prevInProgress != inProgress) {
        pCtx->WPS_PairingInProgress = inProgress;
        swl_typeUInt8_commitObjectParam(wps, "PairingInProgress", inProgress);
    }
}

void wld_sendPairingNotification(T_AccessPoint* pAP, uint32_t type, const char* reason, const char* macAddress) {
    wld_ap_sendPairingNotification(pAP, type, reason, macAddress);
}

/**
 * Sync updatable fields from `g_wpsConst` to `wps_DefParam`.
 *
 * Updatable fields are currently: DefaultPIN, UUID
 */
static void s_syncWpsConstToObject() {
    amxd_trans_t trans;
    /* Update wps_DefParam */
    amxd_object_t* pWPS_DefParam_obj = amxd_object_get(get_wld_object(), "wps_DefParam");
    ASSERT_TRANSACTION_INIT(pWPS_DefParam_obj, &trans, , ME, "trans init failure");
    amxd_trans_set_value(cstring_t, &trans, "DefaultPin", g_wpsConst.DefaultPin);
    amxd_trans_set_value(cstring_t, &trans, "UUID", g_wpsConst.UUID);

    /* Update updatable AccessPoint.WPS.* fields */
    T_Radio* pRad;
    wld_for_eachRad(pRad) {
        T_AccessPoint* pAP = NULL;
        wld_rad_forEachAp(pAP, pRad) {
            amxd_object_t* wpsObj = amxd_object_get(pAP->pBus, "WPS");
            //skip AP ctx not configurable through dm
            if(wpsObj == NULL) {
                continue;
            }
            amxd_trans_select_object(&trans, wpsObj);
            amxd_trans_set_value(cstring_t, &trans, "SelfPIN", pRad->wpsConst->DefaultPin);
            amxd_trans_set_value(cstring_t, &trans, "UUID", pRad->wpsConst->UUID);
        }
    }
    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, , ME, "trans apply failure");
}

/**
 *
 * This does not inform the driver.
 */
static void s_updateSelfPIN(const char* selfPIN) {
    ASSERT_NOT_NULL(selfPIN, , ME, "NULL");
    ASSERT_TRUE(strlen(selfPIN) < sizeof(g_wpsConst.DefaultPin), , ME, "PIN too long");

    swl_str_copy(g_wpsConst.DefaultPin, sizeof(g_wpsConst.DefaultPin), selfPIN);
    s_syncWpsConstToObject();

    SAH_TRACEZ_INFO(ME, "SelfPIN updated to %s", g_wpsConst.DefaultPin);
}

void genSelfPIN() {
    char defaultPin[WPS_PIN_LEN + 1] = "";
    /* Generate new PIN */
    wpsPinGen(defaultPin);
    s_updateSelfPIN(defaultPin);
}

void wld_wps_updateState(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    wld_wps_status_e oldApWpsStatus = pAP->WPS_Status;
    bool wpsEnable = (pAP->WPS_Enable && (pAP->status == APSTI_ENABLED) &&
                      (pAP->secModeEnabled && !swl_security_isApModeWEP(pAP->secModeEnabled) && (pAP->secModeEnabled != SWL_SECURITY_APMODE_WPA3_P)));

    if(wpsEnable) {
        if(pAP->WPS_ApSetupLocked) {
            pAP->WPS_Status = APWPS_SETUPLOCKED;
        } else {
            pAP->WPS_Status = pAP->WPS_Configured ? APWPS_CONFIGURED : APWPS_UNCONFIGURED;
        }
    } else {
        pAP->WPS_Status = APWPS_DISABLED;
    }

    ASSERTI_FALSE((oldApWpsStatus == pAP->WPS_Status), , ME, "%s: WPS status not changed", pAP->alias);
    amxd_object_t* apWpsObj = amxd_object_get(pAP->pBus, "WPS");
    ASSERT_NOT_NULL(apWpsObj, , ME, "NULL");
    swl_typeCharPtr_commitObjectParam(apWpsObj, "Status", (char*) cstr_AP_WPS_Status[pAP->WPS_Status]);
}

/**
 * Callback when the field `WPS.SelfPIN` is written to.
 */
static void s_setWpsSelfPIN_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERTS_NOT_NULL(pAP, , ME, "pAP NULL");
    uint32_t SelfPIN = amxc_var_dyncast(uint32_t, newValue);

    char defaultPIN[WPS_PIN_LEN + 1] = {0};
    if(SelfPIN > 0) {
        swl_str_catFormat(defaultPIN, sizeof(defaultPIN), "%d", SelfPIN);
    }

    if(swl_str_matches(defaultPIN, g_wpsConst.DefaultPin)) {
        return;
    }

    s_updateSelfPIN(defaultPIN);
    if(pAP->WPS_ConfigMethodsEnabled & (M_WPS_CFG_MTHD_LABEL | M_WPS_CFG_MTHD_DISPLAY_ALL)) {
        pAP->pFA->mfn_wvap_wps_label_pin(pAP, SET | DIRECT);
        SAH_TRACEZ_INFO(ME, "%s: set WPS SelfPIN %d", pAP->alias, SelfPIN);
        wld_autoCommitMgr_notifyVapEdit(pAP);
    }

    SAH_TRACEZ_OUT(ME);
}

amxd_status_t _generateSelfPIN(amxd_object_t* object,
                               amxd_function_t* func _UNUSED,
                               amxc_var_t* args _UNUSED,
                               amxc_var_t* retval) {
    amxd_object_t* obj_AP = NULL;
    T_AccessPoint* pAP = NULL;

    /* Check our input data */
    obj_AP = amxd_object_get_parent(object);

    pAP = obj_AP->priv;
    if(pAP && debugIsVapPointer(pAP)) {
        genSelfPIN();
        amxc_var_set(cstring_t, retval, g_wpsConst.DefaultPin);
        return amxd_status_ok;
    }

    return amxd_status_unknown_error;
}

/**
 * Callback for custom constraint of field `WPS.UUID`
 */
amxd_status_t _wld_wps_validateUUID_pvf(amxd_object_t* object _UNUSED,
                                        amxd_param_t* param,
                                        amxd_action_t reason _UNUSED,
                                        const amxc_var_t* const args,
                                        amxc_var_t* const retval _UNUSED,
                                        void* priv _UNUSED) {
    ASSERTS_FALSE(amxc_var_is_null(args), amxd_status_invalid_value, ME, "invalid");
    SAH_TRACEZ_IN(ME);

    amxd_status_t status = amxd_status_invalid_value;
    char* currentUuid = amxc_var_dyncast(cstring_t, &param->value);
    char* uuid = amxc_var_dyncast(cstring_t, args);
    if(swl_str_matches(currentUuid, uuid) || swl_uuid_isValidChar(uuid)) {
        status = amxd_status_ok;
    } else {
        SAH_TRACEZ_ERROR(ME, "invalid UUID (%s)", uuid);
    }
    free(currentUuid);
    free(uuid);

    SAH_TRACEZ_OUT(ME);
    return status;
}

static void s_setWpsRelayCredentialsEnable_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    bool new_addRelayApCredentials = amxc_var_dyncast(bool, newValue);
    if(new_addRelayApCredentials != pAP->addRelayApCredentials) {
        pAP->addRelayApCredentials = new_addRelayApCredentials;
    }

    SAH_TRACEZ_OUT(ME);
}

static void s_setWpsRestartOnRequest_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    pAP->wpsRestartOnRequest = amxc_var_dyncast(bool, newValue);

    SAH_TRACEZ_OUT(ME);
}

/**
 * Callback called when the field `WPS.UUID` is written to.
 */
static void s_setUUID_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERTS_NOT_NULL(pAP, , ME, "pAP NULL");

    char* uuid = amxc_var_dyncast(cstring_t, newValue);
    //Normalizes the UUID to lowercase.
    swl_str_toLower(uuid, swl_str_len(uuid));
    if(!swl_str_matches(uuid, g_wpsConst.UUID)) {
        swl_str_copy(g_wpsConst.UUID, sizeof(g_wpsConst.UUID), uuid);
        // Force a resync of the structure
        wld_ap_doWpsSync(pAP);
    }
    free(uuid);

    SAH_TRACEZ_OUT(ME);
}

static void s_setWPSEnable_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERTI_NOT_NULL(pAP, , ME, "INVALID");
    pAP->WPS_Enable = amxc_var_dyncast(bool, newValue);
    wld_ap_doWpsSync(pAP);

    SAH_TRACEZ_OUT(ME);
}

static void s_setWpsConfigMethodsEnabled_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERTS_NOT_NULL(pAP, , ME, "pAP NULL");

    const char* StrParm = amxc_var_constcast(cstring_t, newValue);
    SAH_TRACEZ_INFO(ME, "set WPS ConfigMethodsEnabled %s", StrParm);
    /* convert it to our bit value... */
    wld_wps_cfgMethod_m nv;
    wld_wps_ConfigMethods_string_to_mask(&nv, StrParm, ',');
    if(nv != pAP->WPS_ConfigMethodsEnabled) {
        /* Ignore comparison with virtual bit settings of WPS 2.0 */
        bool needSync = ((!nv) || ((nv & M_WPS_CFG_MTHD_WPS10_ALL) != (pAP->WPS_ConfigMethodsEnabled & M_WPS_CFG_MTHD_WPS10_ALL)));
        pAP->WPS_ConfigMethodsEnabled = nv;

        /**
         * The ap_setup_locked is set in hostapd configuration file when one of the Pin configuration methods is enabled.
         * In this case, the AP will be started in the setup locked state by default.
         */
        pAP->WPS_ApSetupLocked = pAP->WPS_ConfigMethodsEnabled & (M_WPS_CFG_MTHD_LABEL | M_WPS_CFG_MTHD_DISPLAY_ALL);

        if(needSync) {
            wld_ap_doWpsSync(pAP);
        }
    }

    SAH_TRACEZ_OUT(ME);
}

static void s_setWpsCertModeEnable_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERTI_NOT_NULL(pAP, , ME, "INVALID");
    bool flag = amxc_var_dyncast(bool, newValue);
    SAH_TRACEZ_INFO(ME, "%s: set WPS CertModeEnable %d", pAP->alias, flag);
    pAP->WPS_CertMode = flag;
    wld_ap_doWpsSync(pAP);

    SAH_TRACEZ_OUT(ME);
}

static void s_setWpsConfigured_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERTI_NOT_NULL(pAP, , ME, "INVALID");
    bool flag = amxc_var_dyncast(bool, newValue);
    ASSERTS_NOT_EQUALS(flag, pAP->WPS_Configured, , ME, "EQUALS");
    SAH_TRACEZ_INFO(ME, "%s: set WPS Configured %d", pAP->alias, flag);
    pAP->WPS_Configured = flag;
    wld_ap_doWpsSync(pAP);

    SAH_TRACEZ_OUT(ME);
}

SWLA_DM_HDLRS(sApWpsDmHdlrs,
              ARR(SWLA_DM_PARAM_HDLR("ConfigMethodsEnabled", s_setWpsConfigMethodsEnabled_pwf),
                  SWLA_DM_PARAM_HDLR("CertModeEnable", s_setWpsCertModeEnable_pwf),
                  SWLA_DM_PARAM_HDLR("Configured", s_setWpsConfigured_pwf),
                  SWLA_DM_PARAM_HDLR("SelfPIN", s_setWpsSelfPIN_pwf),
                  SWLA_DM_PARAM_HDLR("UUID", s_setUUID_pwf),
                  SWLA_DM_PARAM_HDLR("RelayCredentialsEnable", s_setWpsRelayCredentialsEnable_pwf),
                  SWLA_DM_PARAM_HDLR("RestartOnRequest", s_setWpsRestartOnRequest_pwf),
                  SWLA_DM_PARAM_HDLR("Enable", s_setWPSEnable_pwf),
                  ));

void _wld_ap_setWpsConf_ocf(const char* const sig_name,
                            const amxc_var_t* const data,
                            void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sApWpsDmHdlrs, sig_name, data, priv);
}

static void wps_pbc_delayed_time_handler(amxp_timer_t* timer, void* userdata) {
    T_AccessPoint* pAP = (T_AccessPoint*) userdata;
    T_Radio* pR = (T_Radio*) pAP->pRadio;

    SAH_TRACEZ_IN(ME);

    if(pR->pFA->mfn_wrad_fsm_state(pR) != FSM_IDLE) {
        // We're still not ready... start timer again maybe we must count this ?
        // 1 sec is clearly too short! some services are active after 5 sec...
        // Even the link is marked as active. Safety we take 10 sec!
        amxp_timer_start(timer, 10000);
        SAH_TRACEZ_WARNING(ME, "%s: WPS (%s) Start delayed again", pAP->alias, pAP->WPS_PBC_Delay.val);
        SAH_TRACEZ_OUT(ME);
        return;
    }

    swl_usp_cmdStatus_ne cmdStatus;
    if(((pR->status != RST_UP) || (pAP->status != APSTI_ENABLED))) {
        SAH_TRACEZ_ERROR(ME, "WPS PBC not executed because radio/AP is down");
        cmdStatus = SWL_USP_CMD_STATUS_ERROR_NOT_READY;
    } else {
        pAP->pFA->mfn_wvap_wps_sync(
            (T_AccessPoint*) pAP->WPS_PBC_Delay.intf.vap,
            (char*) pAP->WPS_PBC_Delay.val,
            pAP->WPS_PBC_Delay.bufsize,
            pAP->WPS_PBC_Delay.setAct);
        cmdStatus = SWL_USP_CMD_STATUS_SUCCESS;
    }

    amxp_timer_delete(&timer);
    pAP->WPS_PBC_Delay.timer = NULL;
    pAP->WPS_PBC_Delay.intf.vap = NULL;

    wld_wps_pushButton_reply(pAP->WPS_PBC_Delay.call_id, cmdStatus);
    SAH_TRACEZ_OUT(ME);
}

/**
 * This function will configure the WPS module in different supported modes.
 * The reason why we add this is for old devices that can't connect anymore due
 * compatibility changes between WPS 2.0 and WPS 1.0. Note this action will take
 * some time it's starting a commit session to set it all up!
 */
amxd_status_t _setCompatibilityWPS(amxd_object_t* object,
                                   amxd_function_t* func _UNUSED,
                                   amxc_var_t* args,
                                   amxc_var_t* retval) {
    amxd_object_t* obj_AP = NULL;
    T_AccessPoint* pAP = NULL;
    T_Radio* pR = NULL;
    int idx;

    SAH_TRACEZ_IN(ME);
    /* Check our input data */
    obj_AP = amxd_object_get_parent(object); // Now we've the AccessPoint object handle.
    const char* StrVal = GET_CHAR(args, "supVerWPS");
    pAP = obj_AP->priv;

    // We've in StrVal our WPS version tag...
    if(pAP && debugIsVapPointer(pAP)) {
        pR = pAP->pRadio;

        idx = conv_ModeIndexStr(cstr_AP_WPS_VERSUPPORTED, StrVal);
        if(idx >= 0) {
            /* We support the request */
            if(pR->wpsConst->wpsSupVer != ci_AP_WPS_VERSUPPORTED[idx]) {
                /* We must change the WPS mode...*/
                pR->wpsConst->wpsSupVer = pAP->pFA->mfn_wvap_wps_comp_mode(pAP, ci_AP_WPS_VERSUPPORTED[idx], SET);
            }
        } else {
            /* No valid value requested, set the default mode from vendor plugin */
            pR->wpsConst->wpsSupVer = pAP->pFA->mfn_wvap_wps_comp_mode(pAP, APWPSVERSI_AUTO, SET);
        }
    }

    /* Cleanup */
    amxc_var_set(uint32_t, retval, 0);
    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

/**
 * This function verifies whether wps is supported by the currently configured security mode.
 * (WPS is not supported in WPA3 or WEP)
 */
bool wld_wps_checkSecModeConditions(T_AccessPoint* pAP) {
    return (pAP && pAP->secModeEnabled &&
            !swl_security_isApModeWEP(pAP->secModeEnabled) &&
            (pAP->secModeEnabled != SWL_SECURITY_APMODE_WPA3_P));
}

/**
 * This function will start the WPS session
 * for given AP (vap) (WPS configuration methods already matched)
 */
static amxd_status_t s_initiateWPS(T_AccessPoint* pAP, amxc_var_t* retval, swl_rc_ne* pRc) {
    T_Radio* pR = pAP->pRadio;
    if(pR->status != RST_UP) {
        SAH_TRACEZ_ERROR(ME, "Radio is down");
        *pRc = SWL_RC_INVALID_STATE;
        return s_setCommandReply(retval, SWL_USP_CMD_STATUS_ERROR_NOT_READY, amxd_status_unknown_error);
    }
    if(!(pAP->enable && pAP->SSIDAdvertisementEnabled && pAP->WPS_Enable)) {
        SAH_TRACEZ_ERROR(ME, "%s VAP, SSIDADV or WPS enable are NOT TRUE (%d,%d,%d)",
                         pAP->alias,
                         pAP->enable,
                         pAP->SSIDAdvertisementEnabled,
                         pAP->WPS_Enable);
        *pRc = SWL_RC_ERROR;
        return s_setCommandReply(retval, SWL_USP_CMD_STATUS_ERROR_NOT_READY, amxd_status_unknown_error);
    }

    if(!wld_wps_checkSecModeConditions(pAP)) {
        SAH_TRACEZ_WARNING(ME, "%s: unmatching wps secMode conditions", pAP->alias);
        *pRc = SWL_RC_INVALID_STATE;
        return s_setCommandReply(retval, SWL_USP_CMD_STATUS_ERROR_NOT_READY, amxd_status_unknown_error);
    }

    // Is our FSM running? Yes delay the command by a timer..
    if(pR->pFA->mfn_wrad_fsm_state(pR) != FSM_IDLE) {
        if(pAP->WPS_PBC_Delay.timer == NULL) {
            amxp_timer_new(&pAP->WPS_PBC_Delay.timer, wps_pbc_delayed_time_handler, pAP);
        }
        amxp_timer_start(pAP->WPS_PBC_Delay.timer, 1000);
        pAP->WPS_PBC_Delay.intf.vap = (void*) pAP;
        pAP->WPS_PBC_Delay.call_id = amxc_var_constcast(uint64_t, retval);
        /* If we defer the execution don't return that the call is done! */
        SAH_TRACEZ_WARNING(ME, "%s: WPS (%s) Start delayed", pAP->alias, pAP->WPS_PBC_Delay.val);
        return amxd_status_deferred;
    }

    // If timer is running clean it now!
    if(pAP->WPS_PBC_Delay.timer) {
        amxp_timer_delete(&pAP->WPS_PBC_Delay.timer);
        wld_wps_pushButton_reply(pAP->WPS_PBC_Delay.call_id, SWL_USP_CMD_STATUS_ERROR_TIMEOUT);
        pAP->WPS_PBC_Delay.call_id = 0;
    }

    pAP->WPS_PBC_Delay.timer = NULL;
    pAP->WPS_PBC_Delay.intf.vap = NULL;

    /* Cancel the ongoing wps session if RestartOnRequest is true */
    if((pAP->wpsRestartOnRequest) && (pAP->wpsSessionInfo.WPS_PairingInProgress)) {
        char strbuf[] = "STOP";
        pAP->pFA->mfn_wvap_wps_sync(pAP, strbuf, sizeof(strbuf), SET);
        SAH_TRACEZ_WARNING(ME, "%s: WPS cancel", pAP->alias);
    }

    SAH_TRACEZ_INFO(ME, "%s: WPS (%s) Start now", pAP->alias, pAP->WPS_PBC_Delay.val);

    // Start WPS Action (PBC,Client|Self-PIN)
    *pRc = pAP->pFA->mfn_wvap_wps_sync(pAP,
                                       pAP->WPS_PBC_Delay.val,
                                       pAP->WPS_PBC_Delay.bufsize,
                                       TRUE);
    if(*pRc < SWL_RC_OK) {
        return s_setCommandReply(retval, SWL_USP_CMD_STATUS_ERROR_OTHER, amxd_status_unknown_error);
    }
    SAH_TRACEZ_WARNING(ME, "WPS Start nodelay %s", pAP->alias);
    return s_setCommandReply(retval, SWL_USP_CMD_STATUS_SUCCESS, amxd_status_ok);
}

static void s_updateRelayApCredentials(T_AccessPoint* pAP, amxc_var_t* relayVar) {
    pAP->wpsSessionInfo.addRelayApCredentials = amxc_var_dyncast(bool, relayVar) || amxd_object_get_bool(amxd_object_get(pAP->pBus, "WPS"), "RelayCredentialsEnable", NULL);
    pAP->wpsSessionInfo.pReferenceApRelay = pAP->pReferenceApRelay;
}

amxd_status_t _WPS_InitiateWPSPBC(amxd_object_t* object,
                                  amxd_function_t* func _UNUSED,
                                  amxc_var_t* args _UNUSED,
                                  amxc_var_t* retval) {
    amxd_object_t* pApObj = amxd_object_get_parent(object);
    T_AccessPoint* pAP = wld_ap_fromObj(pApObj);
    amxc_var_init(retval);
    amxc_var_set_type(retval, AMXC_VAR_ID_HTABLE);
    swl_rc_ne rc = SWL_RC_OK;
    amxd_status_t status = amxd_status_ok;
    if((pAP == NULL) || (pAP->pRadio == NULL)) {
        rc = SWL_RC_INVALID_PARAM;
        status = s_setCommandReply(retval, SWL_USP_CMD_STATUS_ERROR_OTHER, amxd_status_invalid_value);
        if(pAP == NULL) {
            wld_wps_sendPairingNotification(pApObj, NOTIFY_PAIRING_ERROR, WPS_FAILURE_START_PBC, NULL, NULL);
            return status;
        }
    } else if(!(pAP->WPS_ConfigMethodsEnabled & M_WPS_CFG_MTHD_PBC_ALL)) {
        SAH_TRACEZ_ERROR(ME, "%s: wps PushButton not supported", pAP->alias);
        rc = SWL_RC_INVALID_STATE;
        status = s_setCommandReply(retval, SWL_USP_CMD_STATUS_ERROR_OTHER, amxd_status_unknown_error);
    } else {
        pAP->WPS_PBC_Delay.bufsize = sizeof(pAP->WPS_PBC_Delay.val);
        snprintf(pAP->WPS_PBC_Delay.val, pAP->WPS_PBC_Delay.bufsize,
                 "%s",
                 wld_wps_ConfigMethod_to_string(WPS_CFG_MTHD_PBC));
        pAP->WPS_PBC_Delay.setAct = TRUE;
    }
    if(rc == SWL_RC_OK) {
        s_updateRelayApCredentials(pAP, GET_ARG(args, "isRelay"));
        status = s_initiateWPS(pAP, retval, &rc);
    }
    if(rc < SWL_RC_OK) {
        wld_sendPairingNotification(pAP, NOTIFY_PAIRING_ERROR, WPS_FAILURE_START_PBC, NULL);
    }
    return status;
}

amxd_status_t _WPS_InitiateWPSPIN(amxd_object_t* object,
                                  amxd_function_t* func _UNUSED,
                                  amxc_var_t* args,
                                  amxc_var_t* retval) {
    amxd_object_t* pApObj = amxd_object_get_parent(object);
    T_AccessPoint* pAP = wld_ap_fromObj(pApObj);
    amxc_var_init(retval);
    amxc_var_set_type(retval, AMXC_VAR_ID_HTABLE);
    char clientPIN[16] = {0};
    amxc_var_t* clientPINVar = GET_ARG(args, "clientPIN");
    char* clientPINStr = amxc_var_dyncast(cstring_t, clientPINVar);
    swl_str_copy(clientPIN, sizeof(clientPIN), clientPINStr);
    free(clientPINStr);
    swl_rc_ne rc = SWL_RC_OK;
    amxd_status_t status = amxd_status_ok;
    if((pAP == NULL) || (pAP->pRadio == NULL)) {
        rc = SWL_RC_INVALID_PARAM;
        status = s_setCommandReply(retval, SWL_USP_CMD_STATUS_ERROR_OTHER, amxd_status_invalid_value);
        if(pAP == NULL) {
            wld_wps_sendPairingNotification(pApObj, NOTIFY_PAIRING_ERROR, WPS_FAILURE_START_PIN, NULL, NULL);
            return status;
        }
    } else if(clientPINVar == NULL) {
        if((!(pAP->WPS_ConfigMethodsEnabled & (M_WPS_CFG_MTHD_LABEL | M_WPS_CFG_MTHD_DISPLAY_ALL))) ||
           (swl_str_isEmpty(pAP->pRadio->wpsConst->DefaultPin))) {
            SAH_TRACEZ_ERROR(ME, "%s: wps self PIN not enabled", pAP->alias);
            wld_sendPairingNotification(pAP, NOTIFY_PAIRING_ERROR, WPS_FAILURE_NO_SELF_PIN, NULL);
            return s_setCommandReply(retval, SWL_USP_CMD_STATUS_ERROR_OTHER, amxd_status_unknown_error);
        }
        SAH_TRACEZ_INFO(ME, "%s: Self PIN %s", pAP->alias, pAP->pRadio->wpsConst->DefaultPin);
        pAP->WPS_PBC_Delay.bufsize = sizeof(pAP->WPS_PBC_Delay.val);
        snprintf(pAP->WPS_PBC_Delay.val, pAP->WPS_PBC_Delay.bufsize,
                 "%s",
                 wld_wps_ConfigMethod_to_string(WPS_CFG_MTHD_DISPLAY));
        pAP->WPS_PBC_Delay.setAct = TRUE;
    } else if(!(pAP->WPS_ConfigMethodsEnabled & M_WPS_CFG_MTHD_PIN)) {
        SAH_TRACEZ_ERROR(ME, "%s: wps client PIN not enabled", pAP->alias);
        rc = SWL_RC_INVALID_STATE;
        status = s_setCommandReply(retval, SWL_USP_CMD_STATUS_ERROR_OTHER, amxd_status_unknown_error);
    } else {
        char clean_str[strlen(clientPIN) + 1];
        memset(clean_str, 0, sizeof(clean_str));
        swl_str_copy(clean_str, sizeof(clean_str), clientPIN);

        stripOutToken(clean_str, "-");
        stripOutToken(clean_str, " ");
        /* Check if client PIN is valid! */
        bool isPinValid = wldu_checkWpsPinStr(clean_str);
        if(!isPinValid) {
            SAH_TRACEZ_ERROR(ME, "Client PIN (%s) is not valid!", clean_str);
            wld_sendPairingNotification(pAP, NOTIFY_PAIRING_ERROR, WPS_FAILURE_INVALID_PIN, NULL);
            return s_setCommandReply(retval, SWL_USP_CMD_STATUS_ERROR_OTHER, amxd_status_invalid_value);
        }
        /* Client PIN method is used */
        SAH_TRACEZ_INFO(ME, "%s: Client PIN %s", pAP->alias, clean_str);
        pAP->WPS_PBC_Delay.bufsize = sizeof(pAP->WPS_PBC_Delay.val);
        snprintf(pAP->WPS_PBC_Delay.val, pAP->WPS_PBC_Delay.bufsize,
                 "%s=%s",
                 wld_wps_ConfigMethod_to_string(WPS_CFG_MTHD_PIN),
                 clean_str);
        pAP->WPS_PBC_Delay.setAct = TRUE;
    }
    if(rc == SWL_RC_OK) {
        s_updateRelayApCredentials(pAP, GET_ARG(args, "isRelay"));
        status = s_initiateWPS(pAP, retval, &rc);
    }
    if(rc < SWL_RC_OK) {
        wld_sendPairingNotification(pAP, NOTIFY_PAIRING_ERROR, WPS_FAILURE_START_PIN, NULL);
    }
    return status;
}

amxd_status_t _WPS_useRelayCredentials(amxd_object_t* object,
                                       amxd_function_t* func _UNUSED,
                                       amxc_var_t* args _UNUSED,
                                       amxc_var_t* retval _UNUSED) {
    amxd_object_t* pApObj = amxd_object_get_parent(object);
    T_AccessPoint* pAP = wld_ap_fromObj(pApObj);
    ASSERTS_NOT_NULL(pAP, amxd_status_ok, ME, "NULL");
    ASSERTI_TRUE(pAP->wpsSessionInfo.WPS_PairingInProgress, amxd_status_invalid_action, ME, "No WPS on going");
    const char* refApRelayPath = GET_CHAR(args, "refApRelayPath");
    T_AccessPoint* relayAP = pAP->wpsSessionInfo.pReferenceApRelay;
    if(refApRelayPath != NULL) {
        relayAP = wld_ap_fromObj(amxd_object_findf(amxd_dm_get_root(get_wld_plugin_dm()), "%s", refApRelayPath));
    }
    ASSERTS_NOT_NULL(relayAP, amxd_status_invalid_value, ME, "No AP relay specified");
    if(pAP->wpsSessionInfo.addRelayApCredentials && (pAP->wpsSessionInfo.pReferenceApRelay == relayAP)) {
        SAH_TRACEZ_INFO(ME, "%s: Ongoing WPS session is already using relay credentials", pAP->alias);
        return amxd_status_ok;
    }
    pAP->wpsSessionInfo.addRelayApCredentials = true;
    char wpsCmd[] = "UPDATE";
    pAP->pFA->mfn_wvap_wps_sync(pAP, wpsCmd, sizeof(wpsCmd), SET);
    SAH_TRACEZ_WARNING(ME, "%s: WPS use AP %s relay credentials", pAP->alias, relayAP->name);
    return amxd_status_ok;
}

amxd_status_t _WPS_cancelWPSPairing(amxd_object_t* object,
                                    amxd_function_t* func _UNUSED,
                                    amxc_var_t* args _UNUSED,
                                    amxc_var_t* retval _UNUSED) {
    return doCancelPairing(object);
}

