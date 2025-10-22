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
#include <ctype.h>
#include <debug/sahtrace.h>
#include <assert.h>



#include "wld.h"
#include "wld_util.h"
#include "wld_accesspoint.h"
#include "wld_ssid.h"
#include "wld_radio.h"
#include "wld_wps.h"
#include "wld_assocdev.h"
#include "swl/swl_common.h"
#include "swl/swl_assert.h"
#include "swl/swl_string.h"
#include "swl/swl_ieee802_1x_defs.h"
#include "swl/swl_hex.h"
#include "swl/swl_genericFrameParser.h"
#include "swl/swl_staCap.h"
#include "wld_ap_rssiMonitor.h"
#include "wld_eventing.h"
#include "Utils/wld_autoCommitMgr.h"
#include "Utils/wld_autoNeighAdd.h"
#include "wld_ap_nl80211.h"
#include "wld_hostapd_ap_api.h"
#include "wld_dm_trans.h"
#include "Features/wld_persist.h"
#include "wld_extMod.h"

#define ME "ap"

/* FIX ME. Strings must be const STATIC? */
const char* cstr_AP_status[] = {"Disabled", "Enabled", "Error_Misconfigured", "Error",
    0};

const char* cstr_DEVICE_TYPES[] = {"Video", "Data", "Guest", 0};



const char* cstr_AP_MFMode[] = {"Off", "WhiteList", "BlackList",
    0};

const char* cstr_MultiAPType[] = {"FronthaulBSS", "BackhaulBSS", "BackhaulSTA", 0};

const char* wld_apRole_str[] = {"Off", "Main", "Relay", "Remote", 0};

const char* wld_vendorIe_frameType_str[] = {"Beacon", "ProbeResp", "AssocResp", "AuthResp", "ProbeReq", "AssocReq", "AuthReq"};

const char* g_str_wld_ap_dm[] = {"Default", "Disabled", "RNR", "UPR", "FILSDiscovery"};

SWL_TUPLE_TYPE_NEW(assocTable, ARR(swl_type_macBin, swl_type_macBin, swl_type_charPtr, swl_type_timeReal, swl_type_uint16))
static const char* s_assocTableNames[] = {"mac", "bssid", "frame", "timestamp", "request_type"};
SWL_ASSERT_STATIC(SWL_ARRAY_SIZE(s_assocTableNames) == SWL_ARRAY_SIZE(assocTableTypes), "s_assocTableNames not correctly defined");

static bool s_finalizeApCreation(T_AccessPoint* pAP);

static void s_sendChangeEvent(T_AccessPoint* pAP, wld_vap_changeEvent_e changeType, void* data) {
    wld_vap_changeEvent_t change = {
        .vap = pAP,
        .changeType = changeType,
        .data = data
    };
    wld_event_trigger_callback(gWld_queue_vap_onChangeEvent, &change);

}

void wld_vap_sendActionEvent(T_AccessPoint* pAP, wld_vap_actionEvent_e actionEvent, void* data) {
    wld_vap_actionEvent_t change = {
        .vap = pAP,
        .changeType = actionEvent,
        .data = data
    };
    wld_event_trigger_callback(gWld_queue_vap_onAction, &change);
}

T_AccessPoint* wld_ap_fromObj(amxd_object_t* apObj) {
    ASSERTS_EQUALS(amxd_object_get_type(apObj), amxd_object_instance, NULL, ME, "Not instance");
    amxd_object_t* parentObj = amxd_object_get_parent(apObj);
    ASSERT_EQUALS(get_wld_object(), amxd_object_get_parent(parentObj), NULL, ME, "wrong location");
    const char* parentName = amxd_object_get_name(parentObj, AMXD_OBJECT_NAMED);
    ASSERT_TRUE(swl_str_matches(parentName, "AccessPoint"), NULL, ME, "invalid parent obj(%s)", parentName);
    T_AccessPoint* pAP = (T_AccessPoint*) apObj->priv;
    ASSERTS_TRUE(pAP, NULL, ME, "NULL");
    ASSERT_TRUE(debugIsVapPointer(pAP), NULL, ME, "INVALID");
    return pAP;
}

static void s_setDefaults(T_AccessPoint* pAP, T_Radio* pRad, const char* vapName, uint32_t idx) {
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    pAP->debug = VAP_POINTER;
    pAP->index = 0;
    pAP->enable = 0;
    pAP->ref_index = idx;

    pAP->pRadio = pRad;
    pAP->pFA = pRad->pFA;
    swl_str_copy(pAP->name, sizeof(pAP->name), vapName);
    swl_str_copy(pAP->alias, sizeof(pAP->alias), vapName);
    pAP->fsm.FSM_SyncAll = TRUE;

    swl_str_copy(pAP->keyPassPhrase, sizeof(pAP->keyPassPhrase), "password");
    swl_str_copy(pAP->WEPKey, sizeof(pAP->WEPKey), "123456789ABCDEF0123456789A");
    sprintf(pAP->preSharedKey, "password_%d", idx);
    sprintf(pAP->radiusSecret, "RadiusPassword_%d", idx);
    swl_str_copy(pAP->radiusServerIPAddr, sizeof(pAP->radiusServerIPAddr), "127.0.0.1");
    pAP->radiusServerPort = 1812;
    pAP->radiusDefaultSessionTimeout = 0;
    pAP->radiusOwnIPAddress[0] = '\0';
    pAP->radiusNASIdentifier[0] = '\0';
    pAP->radiusCalledStationId[0] = '\0';
    pAP->radiusChargeableUserId = 0;
    pAP->rekeyingInterval = WLD_SEC_PER_H;
    pAP->retryLimit = 3;
    pAP->SSIDAdvertisementEnabled = 1;
    pAP->status = APSTI_DISABLED;
    pAP->secModesSupported = M_SWL_SECURITY_APMODE_NONE;
    pAP->MaxStations = pRad->maxNrHwSta;

    pAP->UAPSDCapability = pAP->pFA->mfn_misc_has_support(pAP->pRadio, pAP, "UAPSD", 0);
    pAP->WMMCapability = pAP->pFA->mfn_misc_has_support(pAP->pRadio, pAP, "WME", 0);
    pAP->secModesSupported |= (pAP->pFA->mfn_misc_has_support(pAP->pRadio, pAP, "WEP", 0)) ?
        (M_SWL_SECURITY_APMODE_WEP64 | M_SWL_SECURITY_APMODE_WEP128 | M_SWL_SECURITY_APMODE_WEP128IV) : 0;
    pAP->secModesSupported |= (pAP->pFA->mfn_misc_has_support(pAP->pRadio, pAP, "AES", 0)) ?
        (M_SWL_SECURITY_APMODE_WPA_P | M_SWL_SECURITY_APMODE_WPA2_P | M_SWL_SECURITY_APMODE_WPA_WPA2_P |
         M_SWL_SECURITY_APMODE_WPA_E | M_SWL_SECURITY_APMODE_WPA2_E | M_SWL_SECURITY_APMODE_WPA_WPA2_E) : 0;
    if(pAP->pFA->mfn_misc_has_support(pAP->pRadio, pAP, "SAE", 0)) {
        pAP->secModesSupported |= M_SWL_SECURITY_APMODE_WPA3_P;
        if(pAP->secModesSupported & M_SWL_SECURITY_APMODE_WPA2_P) {
            pAP->secModesSupported |= M_SWL_SECURITY_APMODE_WPA2_WPA3_P;
        }
    }
    pAP->secModesSupported |= (pAP->pFA->mfn_misc_has_support(pAP->pRadio, pAP, "OWE", 0)) ?
        (M_SWL_SECURITY_APMODE_OWE) : 0;
    if(!pAP->MCEnable) {
        /* Init this interface... */
        T_Radio* pR = (T_Radio*) pAP->pRadio;
        pAP->MCEnable = (pR->operatingFrequencyBand == SWL_FREQ_BAND_EXT_5GHZ) ? TRUE : FALSE;
        pAP->doth = (pR->operatingFrequencyBand == SWL_FREQ_BAND_EXT_5GHZ) ? TRUE : FALSE; //marianna: hardcoded false on 2.4GHz and true on 5GHz, not on WiFiATH
    }

    if(pAP->pRadio->operatingFrequencyBand == SWL_FREQ_BAND_EXT_6GHZ) {
        pAP->secModeEnabled = SWL_SECURITY_APMODE_WPA3_P;
    } else {
        pAP->secModeEnabled = SWL_SECURITY_APMODE_WPA2_P;
    }
    pAP->secModesAvailable = pAP->secModesSupported;
    pAP->mfpConfig = SWL_SECURITY_MFPMODE_DISABLED;
    pAP->MF_AddressList = NULL;
    pAP->MF_AddressListBlockSync = false;

    /* In case we've support for them, enable it by default. */
    pAP->UAPSDEnable = (pAP->UAPSDCapability) ? 1 : 0;
    pAP->WMMEnable = (pAP->WMMCapability) ? 1 : 0;
    pAP->WPS_ConfigMethodsSupported = (M_WPS_CFG_MTHD_LABEL | M_WPS_CFG_MTHD_DISPLAY_ALL | M_WPS_CFG_MTHD_PBC_ALL | M_WPS_CFG_MTHD_PIN);
    pAP->WPS_ConfigMethodsEnabled = (M_WPS_CFG_MTHD_PBC | M_WPS_CFG_MTHD_PIN);
    pAP->WPS_ApSetupLocked = FALSE;
    pAP->WPS_Configured = TRUE;
    pAP->WPS_Enable = 0; /* Disable by default. Only '1' VAP can enable the WPS */
    pAP->WPS_Status = APWPS_DISABLED;
    pAP->defaultDeviceType = DEVICE_TYPE_DATA;
    memset(pAP->NASIdentifier, 0, NAS_IDENTIFIER_MAX_LEN);
    get_randomhexstr((unsigned char*) pAP->NASIdentifier, NAS_IDENTIFIER_MAX_LEN - 1);
    memset(pAP->R0KHKey, 0, KHKEY_MAX_LEN);
    get_randomhexstr((unsigned char*) pAP->R0KHKey, KHKEY_MAX_LEN - 1);
    pAP->addRelayApCredentials = false;
    pAP->wpsRestartOnRequest = false;
    pAP->cfg11u.interworkingEnable = false;
    memset(pAP->cfg11u.qosMapSet, 0, QOS_MAP_SET_MAX_LEN);
}

/**
 * @brief s_deinitAP
 *
 * Clear out resources of the accesspoint internal context
 *
 * @param accesspoint context
 */
static void s_deinitAP(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, , ME, "NULL");
    SAH_TRACEZ_IN(ME);
    //assume disable ap
    pAP->enable = false;
    T_Radio* pR = pAP->pRadio;
    T_SSID* pSSID = pAP->pSSID;
    SAH_TRACEZ_WARNING(ME, "DELETE %s %p", pAP->name, pR);
    if(pR) {
        if(pAP->ActiveAssociatedDeviceNumberOfEntries > 0) {
            /* Deauthentify all stations */
            SAH_TRACEZ_INFO(ME, "%s: Deauth all stations", pAP->alias);
            pAP->pFA->mfn_wvap_kick_sta_reason(pAP, "ff:ff:ff:ff:ff:ff", 17, SWL_IEEE80211_DEAUTH_REASON_UNABLE_TO_HANDLE_STA);
        }
        wld_mld_unregisterLink(pSSID);
        s_sendChangeEvent(pAP, WLD_VAP_CHANGE_EVENT_DEINIT, NULL);
        SAH_TRACEZ_WARNING(ME, "DELETE HOOK %s", pAP->name);
        /* Destroy vap*/
        pR->pFA->mfn_wvap_destroy_hook(pAP);
        /* Sync Rad */
        wld_rad_doSync(pR);
        if(pAP->index > 0) {
            /* Try to delete the requested interface by calling the HW function */
            pR->pFA->mfn_wrad_delvapif(pR, pAP->alias);
        }
        pAP->alias[0] = 0;
        pAP->index = 0;
        /* Take EP also out the Radio */
        amxc_llist_it_take(&pAP->it);
        pAP->pRadio = NULL;
        pAP->pFA = NULL;
    }
    for(int i = pAP->AssociatedDeviceNumberOfEntries - 1; i >= 0; i--) {
        wld_ad_destroy_associatedDevice(pAP, i);
    }
    pAP->ActiveAssociatedDeviceNumberOfEntries = 0;
    W_SWL_FREE(pAP->AssociatedDevice);
    wld_ad_cleanAp(pAP);
    wld_ap_rssiMonDestroy(pAP);

    swl_circTable_destroy(&(pAP->lastAssocReq));

    free(pAP->dbgOutput);
    pAP->dbgOutput = NULL;

    if(pSSID != NULL) {
        memset(pSSID->MACAddress, 0, ETHER_ADDR_LEN);
        pSSID->AP_HOOK = NULL;
        if(pR) {
            pR->pFA->mfn_sync_ssid(pSSID->pBus, pSSID, SET);
        }
        pAP->pSSID = NULL;
    }
    SAH_TRACEZ_OUT(ME);
}

/**
 * @brief s_initAp
 *
 * reserve and initialize resources for accesspoint internal context
 *
 * @param accesspoint context
 */
static bool s_initAp(T_AccessPoint* pAP, T_Radio* pRad, const char* vapName, uint32_t idx) {
    ASSERT_NOT_NULL(pAP, false, ME, "NULL");
    ASSERT_NOT_NULL(pRad, false, ME, "NULL");
    ASSERT_STR(vapName, false, ME, "No vap name");
    if(pAP->AssociatedDevice == NULL) {
        pAP->AssociatedDevice = calloc(MAXNROF_STAENTRY, sizeof(pAP->AssociatedDevice[0]));
        ASSERT_NOT_NULL(pAP->AssociatedDevice, false, ME, "%s: fail to alloc assocDev array", vapName);
    }
    s_setDefaults(pAP, pRad, vapName, idx);
    /* Add pAP on linked list of pR */
    amxc_llist_append(&pRad->llAP, &pAP->it);

    wld_ap_rssiMonInit(pAP);
    wld_ad_initAp(pAP);

    swl_circTable_init(&(pAP->lastAssocReq), &assocTable, 20);

    return true;
}

static T_AccessPoint* s_createAp(T_Radio* pRad, const char* vapName, uint32_t idx, amxd_object_t* vapObj) {
    ASSERT_NOT_NULL(pRad, NULL, ME, "NULL");
    T_AccessPoint* pAP = calloc(1, sizeof(T_AccessPoint));
    ASSERT_NOT_NULL(pAP, NULL, ME, "NULL");

    wld_extMod_initDataList(&pAP->extDataList);

    pAP->pBus = vapObj;
    if(!s_initAp(pAP, pRad, vapName, idx)) {
        free(pAP);
        return NULL;
    }

    ASSERTW_NOT_NULL(vapObj, pAP, ME, "%s: created local VAP(%s) context with no mapped dm object", pRad->Name, pAP->alias);
    vapObj->priv = pAP;

    pAP->wpsSessionInfo.intfObj = vapObj;
    amxd_object_t* wpsinstance = amxd_object_get(vapObj, "WPS");
    ASSERTW_NOT_NULL(wpsinstance, pAP, ME, "%s: WPS subObj is not available", pAP->name);
    wpsinstance->priv = &pAP->wpsSessionInfo;

    s_sendChangeEvent(pAP, WLD_VAP_CHANGE_EVENT_CREATE, NULL);

    return pAP;
}

static amxd_status_t _linkApSsid(amxd_object_t* object, amxd_object_t* pSsidObj) {
    ASSERT_NOT_NULL(object, amxd_status_unknown_error, ME, "NULL");
    T_SSID* pSSID = NULL;
    if(pSsidObj) {
        pSSID = (T_SSID*) pSsidObj->priv;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) object->priv;
    if(pAP != NULL) {
        ASSERTI_NOT_EQUALS(pSSID, pAP->pSSID, amxd_status_ok, ME, "same ssid reference");
        s_deinitAP(pAP);
    }
    const char* vapName = amxd_object_get_name(object, AMXD_OBJECT_NAMED);
    ASSERT_NOT_NULL(vapName, amxd_status_unknown_error, ME, "NULL");
    ASSERTI_NOT_NULL(pSSID, amxd_status_ok, ME, "No SSID Ctx");
    T_Radio* pRad = pSSID->RADIO_PARENT;
    ASSERTI_NOT_NULL(pRad, amxd_status_ok, ME, "No Radio Ctx");
    SAH_TRACEZ_INFO(ME, "pSSID(%p) pRad(%p)", pSSID, pRad);
    uint32_t idx = amxc_llist_size(&pRad->llAP);
    uint32_t macRefIdx = wld_rad_countAPsByAutoMacSrc(pRad, WLD_AUTOMACSRC_RADIO_BASE);
    if(pAP != NULL) {
        bool ret = s_initAp(pAP, pRad, vapName, idx);
        ASSERT_EQUALS(ret, true, amxd_status_unknown_error, ME, "%s: fail to re-create accesspoint", vapName);
    } else {
        pAP = s_createAp(pRad, vapName, idx, object);
        ASSERT_NOT_NULL(pAP, amxd_status_unknown_error, ME, "%s: fail to create accesspoint", vapName);
    }

    SAH_TRACEZ_INFO(ME, "%s: add vap %s", pRad->Name, vapName);
    pSSID->AP_HOOK = pAP;
    pAP->pSSID = pSSID;
    pAP->pSSID->autoMacRefIndex = macRefIdx;

    SAH_TRACEZ_WARNING(ME, "CREATE HOOK %s", pAP->name);
    pAP->pRadio->pFA->mfn_wvap_create_hook(pAP);

    return amxd_status_ok;
}

static void s_syncApSSIDDm(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, , ME, "No mapped AP Ctx");
    ASSERTS_NOT_NULL(pAP->pRadio, , ME, "No mapped Rad Ctx");

    SAH_TRACEZ_IN(ME);

    if(wld_persist_writeApAtCreation()) {
        T_SSID* pSSID = pAP->pSSID;
        ASSERTS_NOT_NULL(pSSID, , ME, "No mapped SSID Ctx");
        pAP->pFA->mfn_sync_ap(pAP->pBus, pAP, GET);
        pAP->pFA->mfn_sync_ap(pAP->pBus, pAP, SET);
        pAP->pFA->mfn_sync_ssid(pSSID->pBus, pSSID, SET);
        pAP->initDone = true;
    }

    SAH_TRACEZ_OUT(ME);

}

/*
 * finalize AP interface creation and MAC/BSSID reading
 */
static bool s_finalizeApCreation(T_AccessPoint* pAP) {
    SAH_TRACEZ_IN(ME);
    ASSERT_NOT_NULL(pAP, false, ME, "NULL");
    SAH_TRACEZ_INFO(ME, "Finalize %s %p", pAP->name, pAP);

    ASSERTS_NOT_NULL(pAP, false, ME, "No mapped AP Ctx");
    T_SSID* pSSID = pAP->pSSID;
    ASSERT_NOT_NULL(pSSID, false, ME, "%s: No mapped SSID Ctx", pAP->alias);
    ASSERT_NOT_NULL(pAP->pRadio, false, ME, "No lower radio");

    if(!pAP->index) {
        SAH_TRACEZ_INFO(ME, "%s: initialize ap interface", pAP->alias);
        wld_ap_initializeVendor(pAP->pRadio, pAP, pSSID);
    }
    //Finalize SSID mapping
    pAP->pFA->mfn_sync_ssid(pSSID->pBus, pSSID, GET);

    wld_ad_initFastReconnectCounters(pAP);
    wld_mld_registerLink(pSSID, pSSID->mldUnit);
    s_sendChangeEvent(pAP, WLD_VAP_CHANGE_EVENT_CREATE_FINAL, NULL);

    pSSID->initDone = true;

    //delay sync AP and SSID Dm after all conf has been loaded
    swla_delayExec_add((swla_delayExecFun_cbf) s_syncApSSIDDm, pAP);

    SAH_TRACEZ_OUT(ME);
    return true;
}

amxd_status_t _wld_ap_delInstance_odf(amxd_object_t* object,
                                      amxd_param_t* param,
                                      amxd_action_t reason,
                                      const amxc_var_t* const args,
                                      amxc_var_t* const retval,
                                      void* priv) {
    SAH_TRACEZ_IN(ME);

    amxd_status_t status = amxd_action_object_destroy(object, param, reason, args, retval, priv);
    ASSERT_EQUALS(status, amxd_status_ok, status, ME, "Fail to destroy obj instance st:%d", status);
    ASSERTS_EQUALS(amxd_object_get_type(object), amxd_object_instance, status, ME, "obj is not instance");
    const char* name = amxd_object_get_name(object, AMXD_OBJECT_NAMED);
    SAH_TRACEZ_INFO(ME, "%s: destroy instance object(%p)", name, object);
    wld_ap_destroy(object->priv);

    SAH_TRACEZ_OUT(ME);
    return status;
}

/*
 * AP instance addition action handler
 * early handling to early create map with internal ssid ctx
 * Needed for smooth dm conf loading as ssid may require knowing relative AP/EP
 */
amxd_status_t _wld_ap_addInstance_oaf(amxd_object_t* object,
                                      amxd_param_t* param,
                                      amxd_action_t reason,
                                      const amxc_var_t* const args,
                                      amxc_var_t* const retval,
                                      void* priv) {
    const char* name = amxd_object_get_name(object, AMXD_OBJECT_NAMED);
    SAH_TRACEZ_INFO(ME, "add instance object(%p:%s)", object, name);
    amxd_status_t status = amxd_status_ok;
    status = amxd_action_object_add_inst(object, param, reason, args, retval, priv);
    ASSERTI_NOT_EQUALS(status, amxd_status_duplicate, status, ME, "override instance (%p:%s)", object, name);
    ASSERT_EQUALS(status, amxd_status_ok, status, ME, "Fail to create instance (%p:%s)", object, name);
    amxd_object_t* instance = amxd_object_get_instance(object, NULL, GET_UINT32(retval, "index"));
    ASSERT_NOT_NULL(instance, amxd_status_unknown_error, ME, "Fail to get instance");
    amxd_object_t* pSsidObj = amxd_object_findf(get_wld_object(), "SSID.%s", amxd_object_get_name(instance, AMXD_OBJECT_NAMED));
    status = _linkApSsid(instance, pSsidObj);
    return status;
}

/*
 * AP instance addition event handler
 * late handling only to finalize AP interface creation (using twin ssid instance)
 * when SSIDReference is not explicitely provided by user conf
 */
static void s_addApInst_oaf(void* priv _UNUSED, amxd_object_t* object, const amxc_var_t* const initialParamValues _UNUSED) {
    SAH_TRACEZ_IN(ME);

    if(swl_str_isEmpty(GET_CHAR(initialParamValues, "SSIDReference"))) {
        /*
         * As user conf does not include specific SSIDReference
         * then use the default twin ssid mapping already initiated in the early action handler
         */
        s_finalizeApCreation(object->priv);
    }

    SAH_TRACEZ_OUT(ME);
}

static void s_setSSIDRef_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    ASSERTI_EQUALS(amxd_object_get_type(object), amxd_object_instance, , ME, "Not instance");
    const char* ssidRef = amxc_var_constcast(cstring_t, newValue);
    const char* oname = amxd_object_get_name(object, AMXD_OBJECT_NAMED);
    SAH_TRACEZ_INFO(ME, "%s: ssidRef(%s)", oname, ssidRef);

    amxd_object_t* pSsidObj = swla_object_getReferenceObject(object, ssidRef);
    T_AccessPoint* pAP = object->priv;
    if((pAP != NULL) && (pAP->pSSID != NULL) && (pAP->pSSID->pBus == pSsidObj) && (pAP->index > 0)) {
        SAH_TRACEZ_NOTICE(ME, "%s: same reference ssid: no need for new finalization", oname);
        return;
    }
    ASSERT_EQUALS(_linkApSsid(object, pSsidObj), amxd_status_ok, , ME, "%s: fail to link Ap to SSID (%s)", oname, ssidRef);

    s_finalizeApCreation(object->priv);

    SAH_TRACEZ_OUT(ME);
}

int wld_ap_initializeVendor(T_Radio* pR, T_AccessPoint* pAP, T_SSID* pSSID) {
    ASSERT_NOT_NULL(pR, SWL_RC_INVALID_PARAM, ME, "NULL");
    int ret = pR->pFA->mfn_wrad_addVapExt(pR, pAP);

    if(ret == SWL_RC_NOT_IMPLEMENTED) {
        char vapnamebuf[32] = {0};
        swl_str_copy(vapnamebuf, sizeof(vapnamebuf), pAP->alias);
        /* Try to create the requested interface by calling the HW function */
        ret = pR->pFA->mfn_wrad_addvapif(pR, vapnamebuf, sizeof(vapnamebuf));
        if(ret >= 0) {
            pAP->index = ret;
            snprintf(pAP->alias, sizeof(pAP->alias), "%s", vapnamebuf);
        }
    }
    ASSERTS_FALSE(ret < SWL_RC_OK, ret, ME, "fail to add vap iface");

    // Plugin has create the BSSID, be sure we update/select that one.
    pAP->pFA->mfn_wvap_bssid(NULL, pAP, NULL, 0, GET);
    memcpy(pSSID->MACAddress, pSSID->BSSID, sizeof(pSSID->MACAddress));

    return SWL_RC_OK;
}

void wld_ap_doSync(T_AccessPoint* pAP) {
    pAP->pFA->mfn_wvap_sync(pAP, SET);
    wld_autoCommitMgr_notifyVapEdit(pAP);
}

void wld_ap_doWpsSync(T_AccessPoint* pAP) {
    pAP->pFA->mfn_wvap_wps_enable(pAP, pAP->WPS_Enable, SET);
    wld_autoCommitMgr_notifyVapEdit(pAP);
}

static void s_saveMaxStations(T_AccessPoint* pAP) {
    ASSERT_TRUE(swl_typeUInt32_commitObjectParam(pAP->pBus, "MaxAssociatedDevices", pAP->MaxStations), ,
                ME, "%s: fail to commit maxAssocDevices (%d)", pAP->alias, pAP->MaxStations);
}

static void s_setMaxStations_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    int flag = amxc_var_dyncast(int32_t, newValue);
    SAH_TRACEZ_INFO(ME, "%s: set MaxStations %d", pAP->alias, flag);
    ASSERTS_NOT_EQUALS(pAP->MaxStations, flag, , ME, "same value");
    T_Radio* pRad = pAP->pRadio;
    int maxNrSta = wld_getMaxNrSta();
    if(pRad != NULL) {
        maxNrSta = SWL_MAX(pRad->maxStations, maxNrSta);
    }
    pAP->MaxStations = (flag > maxNrSta || flag <= 0) ? maxNrSta : flag;
    wld_ap_doSync(pAP);
    if(pAP->MaxStations != flag) {
        swla_delayExec_add((swla_delayExecFun_cbf) s_saveMaxStations, pAP);
    }

    SAH_TRACEZ_OUT(ME);
}

/**
 * The function is used to map an interface on a bridge. Some vendor deamons
 * depends on it for proper functionality.
 */
static void s_setBridgeInterface_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    const char* bridgeName = amxc_var_constcast(cstring_t, newValue);
    ASSERT_NOT_NULL(bridgeName, , ME, "NULL");
    ASSERTI_FALSE(swl_str_matches(bridgeName, pAP->bridgeName), , ME, "%s: same bridgeName %s", pAP->alias, bridgeName);
    SAH_TRACEZ_INFO(ME, "%s: set BridgeInterface %s", pAP->alias, bridgeName);
    swl_str_copy(pAP->bridgeName, sizeof(pAP->bridgeName), bridgeName);
    pAP->pFA->mfn_on_bridge_state_change(pAP->pRadio, pAP, SET);

    SAH_TRACEZ_OUT(ME);
}

/**
 * The function sets the default device type of new devices connected to the requested AP.
 * The value of the defaultDeviceType will only affect future devices connecting to the AP.
 * It will NOT be retroactively set to all connected devices.
 */
static void s_setDefaultDeviceType_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    const char* deviceTypeStr = amxc_var_constcast(cstring_t, newValue);
    ASSERT_NOT_NULL(deviceTypeStr, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: set DefaultDeviceType %s", pAP->alias, deviceTypeStr);
    swl_enum_e newDeviceType = swl_conv_charToEnum(deviceTypeStr, cstr_DEVICE_TYPES, DEVICE_TYPE_MAX, DEVICE_TYPE_DATA);
    ASSERTI_NOT_EQUALS(pAP->defaultDeviceType, (int) newDeviceType, , ME, "%s: same bridgeName %s", pAP->alias, deviceTypeStr);
    SAH_TRACEZ_INFO(ME, "%s: Updating device type from %u to %u", pAP->alias, pAP->defaultDeviceType, newDeviceType);
    pAP->defaultDeviceType = newDeviceType;

    SAH_TRACEZ_OUT(ME);
}

/**
 * Enables debugging on third party software modules.
 * The goal of this is only for developpers!
 * We need a way to have/enable a better view how things are managed by some deamons.
 */
static void s_setDbgEnable_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    bool dbgAPEnable = amxc_var_dyncast(bool, newValue);
    SAH_TRACEZ_INFO(ME, "%s: setdbgAPEnable %d", pAP->alias, dbgAPEnable);

    SAH_TRACEZ_OUT(ME);
}

static void s_setDbgFile_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    const char* dbgFile = amxc_var_constcast(cstring_t, newValue);
    SAH_TRACEZ_INFO(ME, "%s: setdbgAPFile %s", pAP->alias, dbgFile);
    swl_str_copyMalloc(&pAP->dbgOutput, dbgFile);

    SAH_TRACEZ_OUT(ME);
}

static void s_set80211kEnabled_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    bool enable = amxc_var_dyncast(bool, newValue);
    SAH_TRACEZ_INFO(ME, "%s: setAp80211kEnable %d", pAP->alias, enable);
    pAP->IEEE80211kEnable = enable;
    wld_ap_doSync(pAP);

    SAH_TRACEZ_OUT(ME);
}

amxd_status_t _wld_ap_validateIEEE80211rEnabled_pvf(amxd_object_t* object _UNUSED,
                                                    amxd_param_t* param _UNUSED,
                                                    amxd_action_t reason _UNUSED,
                                                    const amxc_var_t* const args,
                                                    amxc_var_t* const retval _UNUSED,
                                                    void* priv _UNUSED) {
    ASSERTS_FALSE(amxc_var_is_null(args), amxd_status_invalid_value, ME, "invalid");
    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERTI_NOT_NULL(pAP, amxd_status_ok, ME, "No AP mapped");
    T_Radio* pRad = (T_Radio*) pAP->pRadio;
    ASSERTI_NOT_NULL(pRad, amxd_status_ok, ME, "No Radio mapped");
    bool flag = amxc_var_dyncast(bool, args);
    if((!flag) || (pRad->IEEE80211rSupported)) {
        return amxd_status_ok;
    }
    SAH_TRACEZ_ERROR(ME, "%s: invalid IEEE80211rEnabled %d", pRad->Name, flag);
    return amxd_status_invalid_value;
}

static void s_set80211rEnabled_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    bool enabled = amxc_var_dyncast(bool, newValue);
    SAH_TRACEZ_INFO(ME, "%s: set 80211rEnable %d", pAP->alias, enabled);
    pAP->IEEE80211rEnable = enabled;
    wld_ap_doSync(pAP);

    SAH_TRACEZ_OUT(ME);
}

static void s_setFTOverDSEnable_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    bool new_value = amxc_var_dyncast(bool, newValue);
    SAH_TRACEZ_INFO(ME, "%s: set FTOverDSEnable %d", pAP->alias, new_value);
    pAP->IEEE80211rFTOverDSEnable = new_value;
    wld_ap_doSync(pAP);

    SAH_TRACEZ_OUT(ME);
}

static void s_setMobilityDomain_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");
    uint16_t mobilityDomain = amxc_var_dyncast(uint16_t, newValue);
    ASSERTI_NOT_EQUALS(pAP->mobilityDomain, mobilityDomain, , ME, "%s: same value %d", pAP->alias, mobilityDomain);
    SAH_TRACEZ_INFO(ME, "%s: setMobilityDomain %d", pAP->alias, mobilityDomain);

    pAP->mobilityDomain = mobilityDomain;
    wld_ap_sec_doSync(pAP);

    SAH_TRACEZ_OUT(ME);
}

SWLA_DM_HDLRS(sAp11rDmHdlrs,
              ARR(SWLA_DM_PARAM_HDLR("FTOverDSEnable", s_setFTOverDSEnable_pwf),
                  SWLA_DM_PARAM_HDLR("MobilityDomain", s_setMobilityDomain_pwf),
                  SWLA_DM_PARAM_HDLR("Enabled", s_set80211rEnabled_pwf),
                  ));

void _wld_ap_11r_setConf_ocf(const char* const sig_name,
                             const amxc_var_t* const data,
                             void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sAp11rDmHdlrs, sig_name, data, priv);
}

static void s_setInterworkingEnable_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    bool enabled = amxc_var_dyncast(bool, newValue);
    SAH_TRACEZ_INFO(ME, "%s: set InterworkingEnable %d", pAP->alias, enabled);
    pAP->cfg11u.interworkingEnable = enabled;
    wld_ap_doSync(pAP);

    SAH_TRACEZ_OUT(ME);
}

static void s_setQosMapSet_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    char* qosMapStr = amxc_var_dyncast(cstring_t, newValue);
    ASSERT_NOT_NULL(qosMapStr, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: set QoS Map %s", pAP->alias, qosMapStr);
    if(!swl_str_matches(pAP->cfg11u.qosMapSet, qosMapStr)) {
        swl_str_copy(pAP->cfg11u.qosMapSet, QOS_MAP_SET_MAX_LEN, qosMapStr);
        wld_ap_doSync(pAP);
    }
    free(qosMapStr);

    SAH_TRACEZ_OUT(ME);
}

SWLA_DM_HDLRS(sAp11uDmHdlrs,
              ARR(SWLA_DM_PARAM_HDLR("QoSMapSet", s_setQosMapSet_pwf),
                  SWLA_DM_PARAM_HDLR("InterworkingEnable", s_setInterworkingEnable_pwf),
                  ));

void _wld_ap_11u_setConf_ocf(const char* const sig_name,
                             const amxc_var_t* const data,
                             void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sAp11uDmHdlrs, sig_name, data, priv);
}

static void s_setSsidAdvEnabled_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");
    T_SSID* pSSID = pAP->pSSID;
    /* SSIDAdvertisementEnabled - Needs also a resync of SSID */
    bool newEnable = amxc_var_dyncast(bool, newValue);
    pAP->SSIDAdvertisementEnabled = newEnable;
    if(debugIsSsidPointer(pSSID)) {
        pAP->pFA->mfn_wvap_ssid(pAP, (char*) pSSID->SSID, strlen(pSSID->SSID), SET);
        wld_autoCommitMgr_notifyVapEdit(pAP);
    }
    wld_ap_doSync(pAP);

    SAH_TRACEZ_OUT(ME);
}

static void s_setApCommonParam_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue _UNUSED) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");
    SAH_TRACEZ_INFO(ME, "%s: set Param %s", pAP->alias, amxd_param_get_name(param));
    wld_ap_doSync(pAP);

    SAH_TRACEZ_OUT(ME);
}


void SyncData_AP2OBJ(amxd_object_t* object, T_AccessPoint* pAP, int set) {
    char TBuf[256];
    amxc_var_t variant;
    amxc_var_init(&variant);

    SAH_TRACEZ_IN(ME);
    if(!(object && pAP && debugIsVapPointer(pAP))) {
        /* Missing data pointers... */
        return;
    }

    amxd_object_t* secObj = amxd_object_findf(object, "Security");
    amxd_object_t* wpsObj = amxd_object_findf(object, "WPS");
    if(set & SET) {
        amxd_trans_t trans;
        ASSERT_TRANSACTION_INIT(object, &trans, , ME, "%s : trans init failure", pAP->name);

        amxc_string_t TBufStr;
        amxc_string_init(&TBufStr, 0);

        /* Set AP data in mapped OBJ structure */
        /* object, must be the one mapping on the pAP structure (/WIFI/AccessPoint/xxx */

        amxd_trans_set_cstring_t(&trans, "Status", cstr_AP_status[pAP->status]);
        amxd_trans_set_uint32_t(&trans, "Index", pAP->index);

        amxd_trans_set_cstring_t(&trans, "Alias", pAP->alias);

        wld_util_initCustomAlias(&trans, object);

        TBuf[0] = 0;
        if(pAP->pSSID != NULL) {
            char* currSsidRef = amxd_object_get_cstring_t(pAP->pBus, "SSIDReference", NULL);
            wld_util_getRealReferencePath(TBuf, sizeof(TBuf), currSsidRef, pAP->pSSID->pBus);
            free(currSsidRef);
        }
        amxd_trans_set_cstring_t(&trans, "SSIDReference", TBuf);
        TBuf[0] = 0;
        if(pAP->pRadio) {
            char* path = amxd_object_get_path(pAP->pRadio->pBus, AMXD_OBJECT_NAMED);
            swl_str_copy(TBuf, sizeof(TBuf), path);
            free(path);
        }
        amxd_trans_set_cstring_t(&trans, "RadioReference", TBuf);

        amxd_trans_set_int32_t(&trans, "RetryLimit", pAP->retryLimit);
        amxd_trans_set_bool(&trans, "WMMCapability", pAP->WMMCapability);
        amxd_trans_set_bool(&trans, "UAPSDCapability", ((pAP->WMMCapability) ? pAP->UAPSDCapability : false));
        amxd_trans_set_bool(&trans, "WMMEnable", ((pAP->WMMCapability) ? pAP->WMMEnable : false));
        amxd_trans_set_bool(&trans, "UAPSDEnable", ((pAP->UAPSDCapability && pAP->WMMCapability) ? pAP->UAPSDEnable : false));
        amxd_trans_set_bool(&trans, "MCEnable", pAP->MCEnable);
        amxd_trans_set_bool(&trans, "APBridgeDisable", pAP->APBridgeDisable);

        amxd_trans_set_bool(&trans, "IsolationEnable", pAP->clientIsolationEnable);

        amxd_trans_set_bool(&trans, "IEEE80211kEnabled", pAP->IEEE80211kEnable);

        amxd_trans_set_bool(&trans, "WDSEnable", pAP->wdsEnable);

        swl_conv_transParamSetMask(&trans, "MultiAPType", pAP->multiAPType, cstr_MultiAPType, MULTIAP_MAX);

        amxd_trans_set_uint8_t(&trans, "MultiAPProfile", pAP->multiAPProfile);
        amxd_trans_set_uint16_t(&trans, "MultiAPVlanId", pAP->multiAPVlanId);

        amxd_trans_set_int32_t(&trans, "AssociatedDeviceNumberOfEntries", pAP->AssociatedDeviceNumberOfEntries);
        amxd_trans_set_int32_t(&trans, "ActiveAssociatedDeviceNumberOfEntries", pAP->ActiveAssociatedDeviceNumberOfEntries);
        if(amxd_object_get_int32_t(object, "MaxAssociatedDevices", NULL) <= 0) {
            amxd_trans_set_uint32_t(&trans, "MaxAssociatedDevices", pAP->MaxStations);
        }

        /** IEEE80211r part */

        amxd_object_t* obj11r = amxd_object_findf(object, "IEEE80211r");
        amxd_trans_select_object(&trans, obj11r);
        amxd_trans_set_cstring_t(&trans, "NASIdentifier", pAP->NASIdentifier);
        amxd_trans_set_cstring_t(&trans, "R0KHKey", pAP->R0KHKey);

        /** Security part */

        amxd_trans_select_object(&trans, secObj);

        swl_security_apModeMaskToString(TBuf, sizeof(TBuf), SWL_SECURITY_APMODEFMT_LEGACY, pAP->secModesSupported);
        amxd_trans_set_cstring_t(&trans, "ModesSupported", TBuf);

        swl_security_apModeMaskToString(TBuf, sizeof(TBuf), SWL_SECURITY_APMODEFMT_LEGACY, pAP->secModesAvailable);
        amxd_trans_set_cstring_t(&trans, "ModesAvailable", TBuf);
        amxd_trans_set_cstring_t(&trans, "ModeEnabled", swl_security_apModeToString(pAP->secModeEnabled, SWL_SECURITY_APMODEFMT_LEGACY));
        amxd_trans_set_cstring_t(&trans, "MFPConfig", swl_security_mfpModeToString(pAP->mfpConfig));
        amxd_trans_set_int32_t(&trans, "SPPAmsdu", pAP->sppAmsdu);


        if(isValidWEPKey(pAP->WEPKey)) {
            amxd_trans_set_cstring_t(&trans, "WEPKey", pAP->WEPKey);
        }

        if(isValidPSKKey(pAP->preSharedKey)) {
            amxd_trans_set_cstring_t(&trans, "PreSharedKey", pAP->preSharedKey);
        }

        if(isValidAESKey(pAP->keyPassPhrase, PSK_KEY_SIZE_LEN - 1)) {
            amxd_trans_set_cstring_t(&trans, "KeyPassPhrase", pAP->keyPassPhrase);
            SAH_TRACEZ_INFO(ME, "%s - SET %s", pAP->name, pAP->keyPassPhrase);
        }

        if(isValidAESKey(pAP->saePassphrase, SAE_KEY_SIZE_LEN)) {
            amxd_trans_set_cstring_t(&trans, "SAEPassphrase", pAP->saePassphrase);
        }

        amxd_trans_set_cstring_t(&trans, "EncryptionMode", cstr_AP_EncryptionMode[pAP->encryptionModeEnabled]);
        amxd_trans_set_uint32_t(&trans, "RekeyingInterval", pAP->rekeyingInterval);
        amxd_trans_set_cstring_t(&trans, "OWETransitionInterface", pAP->oweTransModeIntf);

        TBuf[0] = 0;
        swl_conv_maskToChar(TBuf, sizeof(TBuf), pAP->transitionDisable, g_str_wld_ap_td, AP_TD_MAX);
        amxd_trans_set_cstring_t(&trans, "TransitionDisable", TBuf);

        amxd_trans_set_cstring_t(&trans, "RadiusServerIPAddr", pAP->radiusServerIPAddr);
        amxd_trans_set_uint32_t(&trans, "RadiusServerPort", pAP->radiusServerPort);
        amxd_trans_set_cstring_t(&trans, "RadiusSecret", pAP->radiusSecret);
        amxd_trans_set_int32_t(&trans, "RadiusDefaultSessionTimeout", pAP->radiusDefaultSessionTimeout);
        amxd_trans_set_cstring_t(&trans, "RadiusOwnIPAddress", pAP->radiusOwnIPAddress);
        amxd_trans_set_cstring_t(&trans, "RadiusNASIdentifier", pAP->radiusNASIdentifier);
        amxd_trans_set_cstring_t(&trans, "RadiusCalledStationId", pAP->radiusCalledStationId);
        amxd_trans_set_int32_t(&trans, "RadiusChargeableUserId", pAP->radiusChargeableUserId);

        /** WPS Part */

        if(wpsObj != NULL) {
            wpsObj->priv = &pAP->wpsSessionInfo;
        }

        amxd_trans_select_object(&trans, wpsObj);

        amxd_trans_set_int32_t(&trans, "Enable", pAP->WPS_Enable);
        amxd_trans_set_cstring_t(&trans, "Status", cstr_AP_WPS_Status[pAP->WPS_Status]);
        amxd_trans_set_cstring_t(&trans, "SelfPIN", g_wpsConst.DefaultPin);

        wld_wps_ConfigMethods_mask_to_string(&TBufStr, pAP->WPS_ConfigMethodsSupported);

        amxd_trans_set_cstring_t(&trans, "ConfigMethodsSupported", TBufStr.buffer);
        wld_wps_ConfigMethods_mask_to_string(&TBufStr, pAP->WPS_ConfigMethodsEnabled);

        amxd_trans_set_cstring_t(&trans, "ConfigMethodsEnabled", TBufStr.buffer);
        amxd_trans_set_int32_t(&trans, "Configured", pAP->WPS_Configured);
        amxd_trans_set_cstring_t(&trans, "UUID", g_wpsConst.UUID);

        TBuf[0] = 0;
        swl_str_catFormat(TBuf, sizeof(TBuf), "%.1f", (float) g_wpsConst.wpsSupVer);
        amxd_trans_set_cstring_t(&trans, "Version", TBuf);

        amxc_string_clean(&TBufStr);

        ASSERT_TRANSACTION_LOCAL_DM_END(&trans, , ME, "%s : trans apply failure", pAP->name);

    } else {
        /* Get AP data from OBJ to AP */
        int32_t tmp_int32 = 0;
        uint32_t tmp_uint32 = 0;
        bool tmp_bool = false;
        bool commit = false;

        // For enable just update the state.
        pAP->enable = amxd_object_get_bool(object, "Enable", NULL);

        tmp_bool = amxd_object_get_bool(object, "SSIDAdvertisementEnabled", NULL);
        if(pAP->SSIDAdvertisementEnabled != tmp_bool) {
            pAP->SSIDAdvertisementEnabled = tmp_bool;
            if(!pAP->SSIDAdvertisementEnabled) {   // Hiding SSID? Backup our current WPS state?
                pAP->WPS_Status = pAP->WPS_Enable; // Bug 32192 comment #10 !
            }
            commit = true;
        }

        tmp_bool = amxd_object_get_bool(object, "WMMEnable", NULL);
        if(pAP->WMMEnable != tmp_bool) {
            pAP->WMMEnable = tmp_bool;
            pAP->pFA->mfn_wvap_enable_wmm(pAP, pAP->WMMEnable, SET);
            commit = true;
        }

        tmp_bool = amxd_object_get_bool(object, "UAPSDEnable", NULL);
        if(pAP->UAPSDEnable != tmp_bool) {
            pAP->UAPSDEnable = tmp_bool;
            pAP->pFA->mfn_wvap_enable_uapsd(pAP, pAP->UAPSDEnable, SET);
            commit = true;
        }

        tmp_bool = amxd_object_get_bool(object, "MCEnable", NULL);
        if(pAP->MCEnable != tmp_bool) {
            pAP->MCEnable = tmp_bool;
            commit = true;
        }

        tmp_bool = amxd_object_get_bool(object, "APBridgeDisable", NULL);
        if(pAP->APBridgeDisable != tmp_bool) {
            pAP->APBridgeDisable = tmp_bool;
            commit = true;
        }

        tmp_bool = amxd_object_get_bool(object, "IsolationEnable", NULL);
        if(pAP->clientIsolationEnable != tmp_bool) {
            pAP->clientIsolationEnable = tmp_bool;
            pAP->pFA->mfn_wvap_sync(pAP, SET);
        }

        tmp_bool = amxd_object_get_bool(object, "IEEE80211kEnabled", NULL);
        if(pAP->IEEE80211kEnable != tmp_bool) {
            pAP->IEEE80211kEnable = tmp_bool;
            commit = true;
        }

        tmp_int32 = amxd_object_get_int32_t(object, "MaxAssociatedDevices", NULL);
        if((tmp_int32 > 0) && (pAP->MaxStations != tmp_int32)) {
            pAP->MaxStations = tmp_int32;
            commit = true;
        }

        char* mode = amxd_object_get_cstring_t(secObj, "ModeEnabled", NULL);
        const char* curSecMode = swl_security_apModeToString(pAP->secModeEnabled, SWL_SECURITY_APMODEFMT_LEGACY);
        if(!swl_str_nmatches(curSecMode, mode, swl_str_len(curSecMode))) {
            /* We still must update the pAP->secModeEnabled value */
            swl_security_apMode_e idx = swl_security_apModeFromString(mode);
            if(swl_security_isApModeValid(idx)) {
                pAP->secModeEnabled = idx;
                wld_ap_sec_doSync(pAP);
            }
        }
        free(mode);

        char* mfp = amxd_object_get_cstring_t(secObj, "MFPConfig", NULL);
        const char* curMfpMode = swl_security_mfpModeToString(pAP->mfpConfig);
        if(!swl_str_nmatches(mfp, curMfpMode, swl_str_len(curMfpMode))) {
            /* We still must update the pAP->secModeEnabled value */
            swl_security_mfpMode_e idx = swl_security_mfpModeFromString(mfp);
            if(idx != pAP->mfpConfig) {
                pAP->mfpConfig = idx;
                wld_ap_sec_doSync(pAP);
            }
        }
        free(mfp);

        tmp_int32 = amxd_object_get_int32_t(secObj, "SPPAmsdu", NULL);
        if(pAP->sppAmsdu != tmp_int32) {
            pAP->sppAmsdu = tmp_int32;
            wld_ap_sec_doSync(pAP);
        }

        char* wepKey = amxd_object_get_cstring_t(secObj, "WEPKey", NULL);
        if(!swl_str_nmatches(pAP->WEPKey, wepKey, swl_str_len(pAP->WEPKey))) {
            if(isValidWEPKey(wepKey)) {
                swl_str_copy(pAP->WEPKey, sizeof(pAP->WEPKey), wepKey);
                wld_ap_sec_doSync(pAP);
            }
        }
        free(wepKey);

        char* pskKey = amxd_object_get_cstring_t(secObj, "PreSharedKey", NULL);
        if(!swl_str_nmatches(pAP->preSharedKey, pskKey, swl_str_len(pAP->preSharedKey))) {
            if(isValidPSKKey(pskKey)) {
                swl_str_copy(pAP->preSharedKey, sizeof(pAP->preSharedKey), pskKey);
                wld_ap_sec_doSync(pAP);
            }
        }
        free(pskKey);

        char* keyPassPhrase = amxd_object_get_cstring_t(secObj, "KeyPassPhrase", NULL);
        if(!swl_str_matches(pAP->keyPassPhrase, keyPassPhrase)) {
            if(isValidAESKey(keyPassPhrase, PSK_KEY_SIZE_LEN - 1)) {
                swl_str_copy(pAP->keyPassPhrase, sizeof(pAP->keyPassPhrase), keyPassPhrase);
                wld_ap_sec_doSync(pAP);
            }
        }
        free(keyPassPhrase);

        char* saePassphrase = amxd_object_get_cstring_t(secObj, "SAEPassphrase", NULL);
        if(!swl_str_nmatches(pAP->saePassphrase, saePassphrase, swl_str_len(pAP->saePassphrase))) {
            if(isValidAESKey(saePassphrase, SAE_KEY_SIZE_LEN)) {
                swl_str_copy(pAP->saePassphrase, sizeof(pAP->saePassphrase), saePassphrase);
                wld_ap_sec_doSync(pAP);
            }
        }
        free(saePassphrase);

        char* encryption = amxd_object_get_cstring_t(secObj, "EncryptionMode", NULL);
        if(!swl_str_nmatches(cstr_AP_EncryptionMode[pAP->encryptionModeEnabled], encryption,
                             swl_str_len(cstr_AP_EncryptionMode[pAP->encryptionModeEnabled]))) {
            pAP->encryptionModeEnabled = conv_strToEnum(cstr_AP_EncryptionMode, encryption, APEMI_MAX, APEMI_DEFAULT);
            wld_ap_sec_doSync(pAP);
        }
        free(encryption);

        tmp_int32 = amxd_object_get_int32_t(secObj, "RekeyingInterval", NULL);
        if(pAP->rekeyingInterval != tmp_int32) {
            pAP->rekeyingInterval = tmp_int32;
            wld_ap_sec_doSync(pAP);
        }

        char* radSvrIp = amxd_object_get_cstring_t(secObj, "RadiusServerIPAddr", NULL);
        if(!swl_str_nmatches(pAP->radiusServerIPAddr, radSvrIp, swl_str_len(pAP->radiusServerIPAddr))) {
            swl_str_copy(pAP->radiusServerIPAddr, sizeof(pAP->radiusServerIPAddr), radSvrIp);
            wld_ap_sec_doSync(pAP);
        }
        free(radSvrIp);

        tmp_int32 = amxd_object_get_int32_t(secObj, "RadiusServerPort", NULL);
        if(pAP->radiusServerPort != tmp_int32) {
            pAP->radiusServerPort = tmp_int32;
            wld_ap_sec_doSync(pAP);
        }

        char* radSecret = amxd_object_get_cstring_t(secObj, "RadiusSecret", NULL);
        if(!swl_str_nmatches(pAP->radiusSecret, radSecret, swl_str_len(pAP->radiusSecret))) {
            swl_str_copy(pAP->radiusSecret, sizeof(pAP->radiusSecret), radSecret);
            wld_ap_sec_doSync(pAP);
        }
        free(radSecret);

        tmp_int32 = amxd_object_get_int32_t(secObj, "RadiusDefaultSessionTimeout", NULL);
        if(pAP->radiusDefaultSessionTimeout != tmp_int32) {
            pAP->radiusDefaultSessionTimeout = tmp_int32;
            wld_ap_sec_doSync(pAP);
        }

        char* radOwnIp = amxd_object_get_cstring_t(secObj, "RadiusOwnIPAddress", NULL);
        if(!swl_str_nmatches(pAP->radiusOwnIPAddress, radOwnIp, swl_str_len(pAP->radiusOwnIPAddress))) {
            swl_str_copy(pAP->radiusOwnIPAddress, sizeof(pAP->radiusOwnIPAddress), radOwnIp);
            wld_ap_sec_doSync(pAP);
        }
        free(radOwnIp);

        char* radNasId = amxd_object_get_cstring_t(secObj, "RadiusNASIdentifier", NULL);
        if(!swl_str_nmatches(pAP->radiusNASIdentifier, radNasId, swl_str_len(pAP->radiusNASIdentifier))) {
            swl_str_copy(pAP->radiusNASIdentifier, sizeof(pAP->radiusNASIdentifier), radNasId);
            wld_ap_sec_doSync(pAP);
        }
        free(radNasId);

        char* radStaId = amxd_object_get_cstring_t(secObj, "RadiusCalledStationId", NULL);
        if(!swl_str_nmatches(pAP->radiusCalledStationId, radStaId, swl_str_len(pAP->radiusCalledStationId))) {
            swl_str_copy(pAP->radiusCalledStationId, sizeof(pAP->radiusCalledStationId), radStaId);
            wld_ap_sec_doSync(pAP);
        }
        free(radStaId);

        tmp_int32 = amxd_object_get_int32_t(secObj, "RadiusChargeableUserId", NULL);
        if(pAP->radiusChargeableUserId != tmp_int32) {
            pAP->radiusChargeableUserId = tmp_int32;
            wld_ap_sec_doSync(pAP);
        }

        tmp_bool = amxd_object_get_bool(wpsObj, "Enable", NULL);
        if(pAP->WPS_Enable != tmp_bool) {
            pAP->WPS_Enable = tmp_bool;
            wld_ap_doWpsSync(pAP);
        }

        char* cfgMethods = amxd_object_get_cstring_t(wpsObj, "ConfigMethodsEnabled", NULL);
        wld_wps_ConfigMethods_string_to_mask(&tmp_uint32, cfgMethods, ',');
        if(pAP->WPS_ConfigMethodsEnabled != tmp_uint32) {
            pAP->WPS_ConfigMethodsEnabled = tmp_uint32;
            wld_ap_doWpsSync(pAP);
            commit = true;
        }
        free(cfgMethods);

        tmp_int32 = amxd_object_get_int32_t(wpsObj, "Configured", NULL);
        if(pAP->WPS_Configured != tmp_int32) {
            pAP->WPS_Configured = tmp_int32;
            wld_ap_sec_doSync(pAP);
        }

        char* oweTransIntf = amxd_object_get_cstring_t(secObj, "OWETransitionInterface", NULL);
        if(!swl_str_nmatches(pAP->oweTransModeIntf, oweTransIntf, swl_str_len(pAP->oweTransModeIntf))) {
            swl_str_copy(pAP->oweTransModeIntf, sizeof(pAP->oweTransModeIntf), oweTransIntf);
            wld_ap_sec_doSync(pAP);
        }
        free(oweTransIntf);

        char* tdStr = amxd_object_get_cstring_t(secObj, "TransitionDisable", NULL);
        wld_ap_td_m transitionDisable = swl_conv_charToMask(tdStr, g_str_wld_ap_td, AP_TD_MAX);
        if(pAP->transitionDisable != transitionDisable) {
            pAP->transitionDisable = transitionDisable;
            wld_ap_sec_doSync(pAP);
        }
        free(tdStr);
        if(commit) {
            wld_ap_doSync(pAP);
        }
    }
    amxc_var_clean(&variant);
    SAH_TRACEZ_OUT(ME);
}

/* Be sure that our attached memory structure is cleared */
void t_destroy_handler_AP (amxd_object_t* object) {
    T_AccessPoint* pAP = (T_AccessPoint*) object->priv;
    ASSERT_TRUE(debugIsVapPointer(pAP), , ME, "INVALID");

    SAH_TRACEZ_INFO(ME, "%s : destroy AP", pAP->alias);
    free(pAP);
    object->priv = NULL;
}


amxd_status_t _validateWPSenable(amxd_param_t* parameter) {
    T_AccessPoint* pAP;
    amxd_object_t* wifiVap;
    amxc_var_t value;
    amxc_var_init(&value);
    amxd_param_get_value(parameter, &value);
    bool wps_enable = amxc_var_get_bool(&value);
    amxc_var_clean(&value);
    wifiVap = amxd_object_get_parent(amxd_param_get_owner(parameter));
    pAP = wifiVap->priv;

    if(!pAP) {
        return amxd_status_ok;
    }

    if(wps_enable && (pAP->MF_Mode == APMFM_WHITELIST) && !pAP->WPS_CertMode) {
        SAH_TRACEZ_ERROR(ME, "WPS cannot be enabled if MAC Filtering is set to WhiteList");
        return amxd_status_invalid_action;
    }

    return amxd_status_ok;
}

static void s_setWDSEnable_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    bool newEnable = amxc_var_dyncast(bool, newValue);
    SAH_TRACEZ_INFO(ME, "%s: WDSEnable %d", pAP->alias, newEnable);

    pAP->wdsEnable = newEnable;
    wld_ap_doSync(pAP);

    SAH_TRACEZ_OUT(ME);
}

static void s_setMBOEnable_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    bool newEnable = amxc_var_dyncast(bool, newValue);
    SAH_TRACEZ_INFO(ME, "%s: MBOEnable %d", pAP->alias, newEnable);

    if(newEnable != pAP->mboEnable) {
        pAP->mboEnable = newEnable;
        wld_ap_doSync(pAP);
    }

    SAH_TRACEZ_OUT(ME);
}

static void s_setMBOAssocDisallowReason_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);
    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    const char* mboDenyReasonStr = amxc_var_constcast(cstring_t, newValue);
    ASSERT_NOT_NULL(mboDenyReasonStr, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: set MBO deny reason %s", pAP->alias, mboDenyReasonStr);
    wld_mbo_denyReason_e mboDenyReason =
        swl_conv_charToEnum(mboDenyReasonStr, swl_ieee802_mboAssocDisallowReason_str,
                            SWL_IEEE802_MBO_ASSOC_DISALLOW_REASON_MAX,
                            SWL_IEEE802_MBO_ASSOC_DISALLOW_REASON_OFF);

    ASSERTI_NOT_EQUALS(pAP->mboDenyReason, mboDenyReason, , ME, "%s: same reason %s", pAP->alias, mboDenyReasonStr);
    SAH_TRACEZ_INFO(ME, "%s: Updating mbo deny reason %u to %u", pAP->alias, pAP->mboDenyReason, mboDenyReason);
    pAP->mboDenyReason = mboDenyReason;
    pAP->pFA->mfn_wvap_setMboDenyReason(pAP);

    SAH_TRACEZ_OUT(ME);
}


static void s_setMultiAPType_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    const char* multiApTypeStr = amxc_var_constcast(cstring_t, newValue);
    ASSERT_NOT_NULL(multiApTypeStr, , ME, "NULL");
    wld_multiap_type_m multiApTypeMask = swl_conv_charToMask(multiApTypeStr, cstr_MultiAPType, MULTIAP_MAX);
    ASSERTI_NOT_EQUALS(multiApTypeMask, pAP->multiAPType, , ME, "%s: same MultiAPType %s", pAP->alias, multiApTypeStr);
    pAP->multiAPType = multiApTypeMask;
    pAP->pFA->mfn_wvap_multiap_update_type(pAP);
    wld_autoCommitMgr_notifyVapEdit(pAP);

    SAH_TRACEZ_OUT(ME);
}

static void s_setMultiAPProfile_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    uint8_t multiApProfile = amxc_var_dyncast(uint8_t, newValue);
    if(multiApProfile != pAP->multiAPProfile) {
        pAP->multiAPProfile = multiApProfile;
        pAP->pFA->mfn_wvap_multiap_update_profile(pAP);
        wld_autoCommitMgr_notifyVapEdit(pAP);
    }

    SAH_TRACEZ_OUT(ME);
}

static void s_setMultiAPVlanId_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    uint16_t multiApVlanId = amxc_var_dyncast(uint16_t, newValue);
    if(multiApVlanId != pAP->multiAPVlanId) {
        pAP->multiAPVlanId = multiApVlanId;
        pAP->pFA->mfn_wvap_multiap_update_vlanid(pAP);
        wld_autoCommitMgr_notifyVapEdit(pAP);
    }

    SAH_TRACEZ_OUT(ME);
}


static void s_setApRole_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    const char* apRoleStr = amxc_var_constcast(cstring_t, newValue);
    ASSERT_NOT_NULL(apRoleStr, , ME, "NULL");
    wld_apRole_e newApRole = swl_conv_charToEnum(apRoleStr, wld_apRole_str, AP_ROLE_MAX, AP_ROLE_OFF);
    ASSERTI_NOT_EQUALS(pAP->apRole, newApRole, , ME, "%s: same AP Role %s", pAP->alias, apRoleStr);
    pAP->apRole = newApRole;
    pAP->pFA->mfn_wvap_set_ap_role(pAP);

    SAH_TRACEZ_OUT(ME);
}

static void s_setReferenceApRelay_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    const char* referenceApRelay = amxc_var_constcast(cstring_t, newValue);
    ASSERT_NOT_NULL(referenceApRelay, , ME, "NULL");
    T_AccessPoint* relayAP = wld_ap_fromObj(amxd_object_findf(amxd_dm_get_root(get_wld_plugin_dm()), "%s", referenceApRelay));
    pAP->pReferenceApRelay = relayAP;

    SAH_TRACEZ_OUT(ME);
}

static void s_enableVendorIEs_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pAP, , ME, "VAP Object is NULL");

    pAP->enableVendorIEs = amxc_var_dyncast(bool, newValue);
    pAP->pFA->mfn_wvap_enab_vendor_ie(pAP, pAP->enableVendorIEs);

    SAH_TRACEZ_OUT(ME);
}

SWLA_DM_HDLRS(sApVendorIEsDmHdlrs,
              ARR(SWLA_DM_PARAM_HDLR("Enable", s_enableVendorIEs_pwf)),
              );

void _wld_ap_setVendorIEsConf_ocf(const char* const sig_name,
                                  const amxc_var_t* const data,
                                  void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sApVendorIEsDmHdlrs, sig_name, data, priv);
}

static bool s_isVendorIEValid(const char* oui, const char* data) {
    swl_oui_t ouiBin;
    ASSERT_TRUE(swl_typeOui_fromChar(&ouiBin, oui), false, ME, "Invalid OUI(%s)", oui);
    size_t dataLen = swl_str_len(data);
    ASSERT_FALSE(((dataLen <= 2) || (dataLen > WLD_VENDORIE_T_DATA_SIZE) || (dataLen % 2 != 0)), false,
                 ME, "Invalid data len(%zu) (%s)", dataLen, data);
    ASSERT_TRUE(swl_hex_isHexChar(data, dataLen), false, ME, "Invalid hex data (%s)", data);
    return true;
}

static wld_vendorIe_t* s_getVendorIE(amxd_object_t* templateObj, const char* oui, const char* data, amxd_object_t* optExclObj) {
    ASSERTS_NOT_NULL(templateObj, NULL, ME, "NULL");
    ASSERTS_STR(oui, NULL, ME, "NULL");
    ASSERTS_STR(data, NULL, ME, "NULL");
    amxd_object_for_each(instance, it, templateObj) {
        amxd_object_t* vIeObj = amxc_container_of(it, amxd_object_t, it);
        if(optExclObj == vIeObj) {
            continue;
        }
        char* entryOui = amxd_object_get_cstring_t(vIeObj, "OUI", NULL);
        char* entryData = amxd_object_get_cstring_t(vIeObj, "Data", NULL);
        bool match = (!swl_str_isEmpty(entryOui) && !swl_str_isEmpty(entryData) &&
                      swl_str_matchesIgnoreCase(entryOui, oui) && swl_str_matchesIgnoreCase(entryData, data));
        free(entryOui);
        free(entryData);
        if(match) {
            return vIeObj->priv;
        }
    }

    return NULL;
}

static void s_updateVendorIE (T_AccessPoint* pAP, wld_vendorIe_t* vendorIe) {
    ASSERTI_FALSE(swl_str_isEmpty(vendorIe->oui), , ME, "empty oui");
    ASSERTI_FALSE(swl_str_isEmpty(vendorIe->data), , ME, "empty data");
    ASSERTI_NOT_EQUALS(vendorIe->frame_type, 0, , ME, "no frame type mask");
    pAP->pFA->mfn_wvap_add_vendor_ie(pAP, vendorIe);
    SAH_TRACEZ_INFO(ME, "%s: Updated vendorIE", pAP->alias);
}

static void s_removeVendorIE (T_AccessPoint* pAP, wld_vendorIe_t* vendorIe) {
    ASSERTI_FALSE(swl_str_isEmpty(vendorIe->oui), , ME, "empty oui");
    ASSERTI_FALSE(swl_str_isEmpty(vendorIe->data), , ME, "empty data");
    ASSERTI_NOT_EQUALS(vendorIe->frame_type, 0, , ME, "no frame type mask");
    pAP->pFA->mfn_wvap_del_vendor_ie(pAP, vendorIe);
    SAH_TRACEZ_INFO(ME, "%s: Deleted vendorIE", pAP->alias);
}

amxd_status_t _wld_ap_validateVendorIE_ovf(amxd_object_t* object,
                                           amxd_param_t* param _UNUSED,
                                           amxd_action_t reason _UNUSED,
                                           const amxc_var_t* const args _UNUSED,
                                           amxc_var_t* const retval _UNUSED,
                                           void* priv _UNUSED) {
    SAH_TRACEZ_IN(ME);
    ASSERTS_EQUALS(amxd_object_get_type(object), amxd_object_instance, amxd_status_ok, ME, "Not instance");

    amxc_var_t params;
    amxc_var_init(&params);
    amxc_var_set_type(&params, AMXC_VAR_ID_HTABLE);
    swla_object_paramsToMap(&params, object);

    bool valid = false;
    const char* ouiStr = GET_CHAR(&params, "OUI");
    const char* dataStr = GET_CHAR(&params, "Data");
    size_t dataLen = swl_str_len(dataStr);
    const char* frameTypeMaskStr = GET_CHAR(&params, "FrameType");

    swl_oui_t ouiBin;
    if(!swl_str_isEmpty(ouiStr) && !swl_typeOui_fromChar(&ouiBin, (char*) ouiStr)) {
        SAH_TRACEZ_ERROR(ME, "Invalid OUI(%s)", ouiStr);
    } else if(!swl_str_isEmpty(dataStr) && ((dataLen <= 2) || (dataLen > WLD_VENDORIE_T_DATA_SIZE) || (dataLen % 2 != 0) || (!swl_hex_isHexChar((char*) dataStr, dataLen)))) {
        SAH_TRACEZ_ERROR(ME, "Invalid Data (len:%zu / (%s))", dataLen, dataStr);
    } else if(!swl_str_isEmpty(frameTypeMaskStr) && !swl_conv_charToMask(frameTypeMaskStr, wld_vendorIe_frameType_str, VENDOR_IE_MAX)) {
        SAH_TRACEZ_ERROR(ME, "Invalid FrameType Mask (%s))", frameTypeMaskStr);
    } else {
        amxd_object_t* tmplObj = amxd_object_get_parent(object);
        wld_vendorIe_t* duplVdrIe = s_getVendorIE(tmplObj, ouiStr, dataStr, object);
        if(duplVdrIe != NULL) {
            SAH_TRACEZ_ERROR(ME, "vendorIE[%d] info oui(%s)/Data(%s) already exist", amxd_object_get_index(duplVdrIe->object), ouiStr, dataStr);
        } else {
            valid = true;
        }
    }
    amxc_var_clean(&params);

    SAH_TRACEZ_OUT(ME);
    return (valid ? amxd_status_ok : amxd_status_invalid_value);
}

static void s_addVendorIE_oaf(void* priv _UNUSED, amxd_object_t* object, const amxc_var_t* const intialParamValues _UNUSED) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(amxd_object_get_parent(amxd_object_get_parent(object))));
    ASSERT_NOT_NULL(pAP, , ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: Adding Vendor IE", pAP->alias);
    wld_vendorIe_t* newVendorIe = calloc(1, sizeof(wld_vendorIe_t));
    ASSERTS_NOT_NULL(newVendorIe, , ME, "NULL");

    amxc_llist_it_init(&newVendorIe->it);
    amxc_llist_append(&pAP->vendorIEs, &newVendorIe->it);
    newVendorIe->object = object;
    object->priv = newVendorIe;

    SAH_TRACEZ_OUT(ME);
}

static void s_setVendorIEConf_ocf(void* priv _UNUSED, amxd_object_t* object, const amxc_var_t* const newParamValues) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(amxd_object_get_parent(amxd_object_get_parent(object))));
    ASSERT_NOT_NULL(pAP, , ME, "NULL");

    wld_vendorIe_t* vendorIe = object->priv;
    ASSERT_NOT_NULL(vendorIe, , ME, "NULL");

    s_removeVendorIE(pAP, vendorIe);

    amxc_var_for_each(newValue, newParamValues) {
        char* valStr = NULL;
        const char* pname = amxc_var_key(newValue);
        if(swl_str_matches(pname, "OUI")) {
            valStr = amxc_var_dyncast(cstring_t, newValue);
            SAH_TRACEZ_INFO(ME, "set vendorIE OUI = %s", valStr);
            swl_str_copy(vendorIe->oui, SWL_OUI_STR_LEN, valStr);
        } else if(swl_str_matches(pname, "Data")) {
            valStr = amxc_var_dyncast(cstring_t, newValue);
            SAH_TRACEZ_INFO(ME, "set vendorIE Data = %s", valStr);
            swl_str_copy(vendorIe->data, WLD_VENDORIE_T_DATA_SIZE, valStr);
        } else if(swl_str_matches(pname, "FrameType")) {
            valStr = amxc_var_dyncast(cstring_t, newValue);
            vendorIe->frame_type = swl_conv_charToMask(valStr, wld_vendorIe_frameType_str, VENDOR_IE_MAX);
            SAH_TRACEZ_INFO(ME, "set vendorIE FrameType mask(0x%x)(%s)", vendorIe->frame_type, valStr);
        } else {
            continue;
        }
        free(valStr);
    }

    s_updateVendorIE(pAP, vendorIe);

    SAH_TRACEZ_OUT(ME);
}

SWLA_DM_HDLRS(sApVendorIeDmHdlrs,
              ARR(),
              .instAddedCb = s_addVendorIE_oaf,
              .objChangedCb = s_setVendorIEConf_ocf,
              );

void _wld_ap_setVendorIE_ocf(const char* const sig_name,
                             const amxc_var_t* const data,
                             void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sApVendorIeDmHdlrs, sig_name, data, priv);
}

amxd_status_t _wld_ap_delVendorIE_odf(amxd_object_t* object,
                                      amxd_param_t* param,
                                      amxd_action_t reason,
                                      const amxc_var_t* const args,
                                      amxc_var_t* const retval,
                                      void* priv) {
    SAH_TRACEZ_IN(ME);

    amxd_status_t status = amxd_action_object_destroy(object, param, reason, args, retval, priv);
    ASSERT_EQUALS(status, amxd_status_ok, status, ME, "Fail to destroy vendorIe entry st:%d", status);
    ASSERTS_EQUALS(amxd_object_get_type(object), amxd_object_instance, status, ME, "obj is not instance");

    wld_vendorIe_t* vendorIe = (wld_vendorIe_t*) object->priv;
    object->priv = NULL;
    ASSERTS_NOT_NULL(vendorIe, amxd_status_ok, ME, "No internal ctx");
    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(amxd_object_get_parent(amxd_object_get_parent(object))));
    ASSERT_NOT_NULL(pAP, amxd_status_unknown_error, ME, "NULL");

    amxc_llist_it_take(&(vendorIe->it));

    s_removeVendorIE(pAP, vendorIe);
    free(vendorIe);

    SAH_TRACEZ_OUT(ME);
    return status;
}

amxd_status_t _createVendorIE(amxd_object_t* obj,
                              amxd_function_t* func _UNUSED,
                              amxc_var_t* args,
                              amxc_var_t* ret _UNUSED) {
    SAH_TRACEZ_IN(ME);

    const char* apName = amxd_object_get_name(amxd_object_get_parent(obj), AMXD_OBJECT_NAMED);
    const char* oui = GET_CHAR(args, "oui");
    const char* data = GET_CHAR(args, "data");
    ASSERT_TRUE(s_isVendorIEValid(oui, data), amxd_status_unknown_error, ME, "invalid args");

    amxd_trans_t trans;
    amxd_object_t* templateObj = amxd_object_findf(obj, "VendorIE");
    wld_vendorIe_t* vendorIe = s_getVendorIE(templateObj, oui, data, NULL);

    if(vendorIe != NULL) {
        SAH_TRACEZ_INFO(ME, "VendorIE with oui[%s]/data[%s] found in VdrIEs list", oui, data);
        ASSERT_TRANSACTION_INIT(vendorIe->object, &trans, amxd_status_unknown_error, ME, "%s : trans init failure", apName);
    } else {
        SAH_TRACEZ_INFO(ME, "Create new VendorIE with oui[%s]/data[%s]", oui, data);
        ASSERT_TRANSACTION_INIT(templateObj, &trans, amxd_status_unknown_error, ME, "%s : trans init failure", apName);
        amxd_trans_add_inst(&trans, 0, NULL);
        amxd_trans_set_value(cstring_t, &trans, "OUI", oui);
        amxd_trans_set_value(cstring_t, &trans, "Data", data);
    }

    SAH_TRACEZ_INFO(ME, "Updating VendorIE object with provided arguments");
    amxc_var_for_each(newValue, args) {
        char* valStr = NULL;
        const char* pname = amxc_var_key(newValue);
        if(swl_str_matches(pname, "frame_type")) {
            valStr = amxc_var_dyncast(cstring_t, newValue);
            amxd_trans_set_value(cstring_t, &trans, "FrameType", valStr);
        } else {
            continue;
        }
        free(valStr);
    }

    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, amxd_status_unknown_error, ME, "%s : trans apply failure", apName);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _deleteVendorIE(amxd_object_t* obj,
                              amxd_function_t* func _UNUSED,
                              amxc_var_t* args,
                              amxc_var_t* ret _UNUSED) {
    SAH_TRACEZ_IN(ME);

    amxd_status_t status = amxd_status_unknown_error;
    const char* apName = amxd_object_get_name(amxd_object_get_parent(obj), AMXD_OBJECT_NAMED);
    const char* oui = GET_CHAR(args, "oui");
    const char* data = GET_CHAR(args, "data");
    ASSERT_TRUE(s_isVendorIEValid(oui, data), status, ME, "%s: invalid args", apName);

    amxd_object_t* templateObj = amxd_object_findf(obj, "VendorIE");
    wld_vendorIe_t* vendorIe = s_getVendorIE(templateObj, oui, data, NULL);

    if(vendorIe == NULL) {
        SAH_TRACEZ_INFO(ME, "%s: Could not find VendorIE with oui(%s)/data(%s)", apName, oui, data);
        status = amxd_status_ok;
    } else {
        status = swl_object_delInstWithTransOnLocalDm(vendorIe->object);
    }

    SAH_TRACEZ_OUT(ME);
    return status;
}

static void s_setHotSpotEnable_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pAP, , ME, "Invalid AP Ctx");

    bool flag = amxc_var_dyncast(bool, newValue);
    SAH_TRACEZ_INFO(ME, "%s: set HotSpotEnable %d", pAP->alias, flag);
    ASSERTI_NOT_EQUALS(pAP->HotSpot2.enable, flag, , ME, "EQUALS");
    pAP->HotSpot2.enable = flag;
    pAP->pFA->mfn_hspot_enable(pAP, flag, SET);

    SAH_TRACEZ_OUT(ME);
}

amxd_status_t _wld_ap_validateHotSpot2Requirements(amxd_object_t* object,
                                                   amxd_param_t* param _UNUSED,
                                                   amxd_action_t reason _UNUSED,
                                                   const amxc_var_t* const args,
                                                   amxc_var_t* const retval _UNUSED,
                                                   void* priv _UNUSED) {
    ASSERTS_FALSE(amxc_var_is_null(args), amxd_status_invalid_value, ME, "invalid");
    amxd_object_t* pAPObj = amxd_object_get_parent(object);
    ASSERTS_EQUALS(amxd_object_get_type(pAPObj), amxd_object_instance, amxd_status_ok, ME, "obj is not instance");

    SAH_TRACEZ_IN(ME);

    amxd_status_t status = amxd_status_invalid_value;
    T_AccessPoint* pAP = wld_ap_fromObj(pAPObj);
    ASSERTI_NOT_NULL(pAP, amxd_status_ok, ME, "VAP Object is NULL");

    bool flag = amxc_var_dyncast(bool, args);
    if(!flag) {
        status = amxd_status_ok;
    } else if(!pAP->enable) {
        SAH_TRACEZ_INFO(ME, "%s: Access point is disabled", pAP->alias);
    } else if(pAP->secModeEnabled != SWL_SECURITY_APMODE_WPA2_E) {
        /* Broadcom requires that wpa2 is used when starting Hotspot. This only applies for 4.12.
         *  Later version don't have this requirement */
        SAH_TRACEZ_INFO(ME, "%s: secModeEnabled is not WPA2_E ", pAP->alias);
    } else {
        status = amxd_status_ok;
    }

    SAH_TRACEZ_OUT(ME);
    return status;
}

static void s_setHotSpotConf_ocf(void* priv _UNUSED, amxd_object_t* object, const amxc_var_t* const newParamValues _UNUSED) {
    SAH_TRACEZ_IN(ME);

    bool needSyncHS = false;
    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pAP, , ME, "Invalid AP Ctx");

    amxc_var_for_each(newValue, newParamValues) {
        const char* pname = amxc_var_key(newValue);
        if(swl_str_matches(pname, "DgafDisable")) {
            pAP->HotSpot2.dgaf_disable = amxc_var_dyncast(bool, newValue);
        } else if(swl_str_matches(pname, "L2TrafficInspect")) {
            const char* valStr = amxc_var_constcast(cstring_t, newValue);
            pAP->HotSpot2.l2_traffic_inspect = !swl_str_isEmpty(valStr);
        } else if(swl_str_matches(pname, "IcmpV4Echo")) {
            pAP->HotSpot2.icmpv4_echo = amxc_var_dyncast(bool, newValue);
        } else if(swl_str_matches(pname, "Interworking")) {
            pAP->HotSpot2.interworking = amxc_var_dyncast(bool, newValue);
        } else if(swl_str_matches(pname, "Internet")) {
            pAP->HotSpot2.internet = amxc_var_dyncast(bool, newValue);
        } else if(swl_str_matches(pname, "Hs2Ie")) {
            pAP->HotSpot2.hs2_ie = amxc_var_dyncast(bool, newValue);
        } else if(swl_str_matches(pname, "P2PEnable")) {
            pAP->HotSpot2.p2p_enable = amxc_var_dyncast(bool, newValue);
        } else if(swl_str_matches(pname, "GasDelay")) {
            pAP->HotSpot2.gas_delay = amxc_var_dyncast(int32_t, newValue);
        } else if(swl_str_matches(pname, "AccessNetworkType")) {
            pAP->HotSpot2.access_network_type = amxc_var_dyncast(int8_t, newValue);
        } else if(swl_str_matches(pname, "VenueType")) {
            pAP->HotSpot2.venue_type = amxc_var_dyncast(int8_t, newValue);
        } else if(swl_str_matches(pname, "VenueGroup")) {
            pAP->HotSpot2.venue_group = amxc_var_dyncast(int8_t, newValue);
        } else {
            continue;
        }
        needSyncHS = true;
    }

    if(needSyncHS) {
        pAP->pFA->mfn_hspot_config(pAP, SET);
    }

    SAH_TRACEZ_OUT(ME);
}

SWLA_DM_HDLRS(sApHotSpotDmHdlrs,
              ARR(SWLA_DM_PARAM_HDLR("Enable", s_setHotSpotEnable_pwf)),
              .objChangedCb = s_setHotSpotConf_ocf,
              );

void _wld_ap_setHotSpotConf_ocf(const char* const sig_name,
                                const amxc_var_t* const data,
                                void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sApHotSpotDmHdlrs, sig_name, data, priv);
}

static bool s_chechRnrEnabledOnOtherRadios(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, false, ME, "NULL");
    T_Radio* pOtherRad;
    wld_for_eachRad(pOtherRad) {
        if((swl_str_matches(pRad->Name, pOtherRad->Name)) ||
           (pOtherRad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_6GHZ)) {
            continue;
        }
        T_AccessPoint* pAPTmp;
        wld_rad_forEachAp(pAPTmp, pOtherRad) {
            if(pAPTmp->discoveryMethod & M_AP_DM_RNR) {
                return true;
            }
        }
    }
    return false;
}

amxd_status_t _wld_ap_validateDiscoveryMethod_pvf(amxd_object_t* object,
                                                  amxd_param_t* param,
                                                  amxd_action_t reason _UNUSED,
                                                  const amxc_var_t* const args,
                                                  amxc_var_t* const retval _UNUSED,
                                                  void* priv _UNUSED) {
    ASSERTS_FALSE(amxc_var_is_null(args), amxd_status_invalid_value, ME, "invalid");
    ASSERTS_EQUALS(amxd_object_get_type(object), amxd_object_instance, amxd_status_ok, ME, "obj is not instance");
    SAH_TRACEZ_IN(ME);

    amxd_status_t status = amxd_status_invalid_value;
    const char* oname = amxd_object_get_name(object, AMXD_OBJECT_NAMED);
    const char* currentValue = amxc_var_constcast(cstring_t, &param->value);
    ASSERT_NOT_NULL(currentValue, status, ME, "NULL");
    const char* dmStr = amxc_var_constcast(cstring_t, args);
    if(swl_str_matches(currentValue, dmStr)) {
        return amxd_status_ok;
    }
    wld_ap_dm_m discoveryMethod = swl_conv_charToMask(dmStr, g_str_wld_ap_dm, AP_DM_MAX);
    ASSERT_NOT_EQUALS(discoveryMethod, 0, status, ME, "%s: invalid value (%s)", oname, dmStr);
    ASSERT_FALSE((discoveryMethod & M_AP_DM_DISABLED) && (discoveryMethod & ~M_AP_DM_DISABLED), status, ME, "%s: Can not set disabled with other methods", oname);
    ASSERT_FALSE((discoveryMethod & M_AP_DM_UPR) && (discoveryMethod & M_AP_DM_FD), status, ME, "%s: Unsolicited Probe Response and FILS Discovery must not be enabled at the same time", oname);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERTI_NOT_NULL(pAP, amxd_status_ok, ME, "%s: no AP ctx", oname);
    T_Radio* pRad = (T_Radio*) pAP->pRadio;
    ASSERTI_NOT_NULL(pAP, amxd_status_ok, ME, "%s: no mapped Radio ctx", oname);

    /* Discovery method (UPR/FILS) is mandatory on 6GHz if no discovery
     * method is present on other bands (2.4/5GHz). Check if RNR is present
     */
    bool rnrEnabled = s_chechRnrEnabledOnOtherRadios(pRad);
    ASSERT_FALSE(rnrEnabled && (pRad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_6GHZ) && (discoveryMethod == M_AP_DM_DISABLED), status, ME, "%s: Disabled is not valid for 6GHz AP", oname);
    ASSERT_FALSE((pRad->operatingFrequencyBand != SWL_FREQ_BAND_EXT_6GHZ) && (discoveryMethod & M_AP_DM_UPR), status, ME, "%s: Unsolicted Probe Responses method is 6GHz AP only", oname);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

void wld_ap_updateDiscoveryMethod(T_AccessPoint* pAP) {
    ASSERTI_NOT_NULL(pAP, , ME, "NULL");
    pAP->pFA->mfn_wvap_set_discovery_method(pAP);
}

wld_ap_dm_m wld_ap_getDiscoveryMethod(T_AccessPoint* pAP) {
    ASSERTI_NOT_NULL(pAP, M_AP_DM_DEFAULT, ME, "NULL");
    wld_ap_dm_m dm = pAP->discoveryMethod;
    if(pAP->pRadio->operatingFrequencyBand != SWL_FREQ_BAND_EXT_6GHZ) {
        return dm == M_AP_DM_DEFAULT ? M_AP_DM_RNR : dm;
    }

    char macStr[SWL_MAC_CHAR_LEN] = {0};
    SWL_MAC_BIN_TO_CHAR(macStr, pAP->pSSID->BSSID);

    T_Radio* pRad;
    wld_for_eachRad(pRad) {
        if(pRad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_6GHZ) {
            continue;
        }
        T_AccessPoint* pApTmp = NULL;
        wld_rad_forEachAp(pApTmp, pRad) {
            T_ApNeighbour* neigh = wld_ap_findNeigh(pApTmp, macStr);
            if((pApTmp->status == APSTI_ENABLED) &&
               (wld_ap_getDiscoveryMethod(pApTmp) == M_AP_DM_RNR) &&
               (neigh != NULL) &&
               neigh->colocatedAp) {
                return dm == M_AP_DM_DEFAULT ? M_AP_DM_DISABLED : dm;
            }
        }
    }
    return dm == M_AP_DM_DEFAULT ? M_AP_DM_UPR : dm;
}

static void s_setDiscoveryMethod_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "pAP NULL");
    T_Radio* pRad = (T_Radio*) pAP->pRadio;
    ASSERT_NOT_NULL(pRad, , ME, "No mapped radio");

    const char* dmStr = amxc_var_constcast(cstring_t, newValue);
    wld_ap_dm_m discoveryMethod = swl_conv_charToMask(dmStr, g_str_wld_ap_dm, AP_DM_MAX);

    /* Discovery method (UPR/FILS) is mandatory on 6GHz if no discovery
     * method is present on other bands (2.4/5GHz). Check if RNR is present
     */
    bool rnrEnabled = s_chechRnrEnabledOnOtherRadios(pRad);
    ASSERT_FALSE(rnrEnabled && (pRad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_6GHZ) && (discoveryMethod == M_AP_DM_DISABLED), , ME, "%s: Disabled is not valid for 6GHz AP", pAP->alias);
    ASSERT_FALSE((pRad->operatingFrequencyBand != SWL_FREQ_BAND_EXT_6GHZ) && (discoveryMethod & M_AP_DM_UPR), , ME, "%s: Unsolicted Probe Responses method is 6GHz AP only", pAP->alias);

    pAP->discoveryMethod = discoveryMethod;
    SAH_TRACEZ_INFO(ME, "%s: Configure discovery method: %d", pAP->alias, pAP->discoveryMethod);

    wld_autoNeighAdd_vapReloadNeighbourAP(pAP);

    pAP->pFA->mfn_wvap_set_discovery_method(pAP);

    if(pAP->pRadio->operatingFrequencyBand != SWL_FREQ_BAND_EXT_6GHZ) {
        wld_rad_updateDiscoveryMethod6GHz();
    }

    SAH_TRACEZ_OUT(ME);
}

static void s_setApEnable_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param, const amxc_var_t* const newValue _UNUSED) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");


    amxc_var_t myVar;
    amxc_var_init(&myVar);
    amxd_status_t status = amxd_param_get_value(param, &myVar);
    ASSERT_EQUALS(status, amxd_status_ok, , ME, "%s: fail to receive latest enable value", pAP->name);
    bool newEnable = amxc_var_dyncast(bool, &myVar);
    amxc_var_clean(&myVar);

    SAH_TRACEZ_INFO(ME, "%s: set enable %d -> %d", pAP->alias, pAP->enable, newEnable);

    ASSERTI_NOT_EQUALS(newEnable, pAP->enable, , ME, "%s: set to same enable %d", pAP->alias, newEnable);


    pAP->pFA->mfn_wvap_enable(pAP, newEnable, SET);
    pAP->enable = newEnable;
    wld_autoCommitMgr_notifyVapEdit(pAP);
    wld_ssid_syncEnable(pAP->pSSID, false);

    SAH_TRACEZ_OUT(ME);
}

static void s_setMACAddressControlEnabled_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");

    bool newMACAddressControlEnabled = amxc_var_dyncast(bool, newValue);
    SAH_TRACEZ_INFO(ME, "%s: set MACAddressControlEnabled %d", pAP->alias, newMACAddressControlEnabled);

    amxd_object_t* mfObject = amxd_object_get(pAP->pBus, "MACFiltering");
    if((pAP->MF_Mode == APMFM_WHITELIST) || (pAP->MF_Mode == APMFM_OFF)) {
        swl_typeCharPtr_commitObjectParam(mfObject, "Mode", newMACAddressControlEnabled ? (char*) cstr_AP_MFMode[APMFM_WHITELIST] : (char*) cstr_AP_MFMode[APMFM_OFF]);
    }

    SAH_TRACEZ_OUT(ME);
}

static void s_setStaInactivityTimeout_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "INVALID");
    uint16_t StaInactivityTimeout = amxc_var_dyncast(uint16_t, newValue);
    SAH_TRACEZ_INFO(ME, "%s: set StaInactivityTimeout %u", pAP->alias, StaInactivityTimeout);
    pAP->StaInactivityTimeout = StaInactivityTimeout;
    wld_ap_doSync(pAP);

    SAH_TRACEZ_OUT(ME);
}

T_AccessPoint* wld_ap_create(T_Radio* pRad, const char* vapName, uint32_t idx) {
    return s_createAp(pRad, vapName, idx, NULL);
}

/**
 * This function Destroys the access point
 * first deauthentify all associated stations, destroy vap then deletes the accesspoint.
 */
void wld_ap_destroy(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, , ME, "NULL");

    s_sendChangeEvent(pAP, WLD_VAP_CHANGE_EVENT_DESTROY, NULL);
    wld_extMod_cleanupDataList(&pAP->extDataList, pAP);

    s_deinitAP(pAP);

    if(pAP->pBus != NULL) {
        pAP->pBus->priv = NULL;
    }

    free(pAP);
}


static void s_kickSta(T_AccessPoint* pAP, const char* mac, int32_t reason) {
    int result = -1;

    if(reason < 0) {
        result = pAP->pFA->mfn_wvap_kick_sta(pAP, (char*) mac, strlen(mac), SET);
    } else {
        result = pAP->pFA->mfn_wvap_kick_sta_reason(pAP, (char*) mac, strlen(mac), reason);
    }

    wld_ap_kickAction_t action = {
        .mac = mac,
        .reason = reason,
        .result = result
    };
    wld_vap_sendActionEvent(pAP, WLD_VAP_ACTION_KICK, &action);

}

// This function will kick a STA with given MAC from the VAP interface.
amxd_status_t _kickStation(amxd_object_t* obj_AP,
                           amxd_function_t* func _UNUSED,
                           amxc_var_t* args,
                           amxc_var_t* ret _UNUSED) {

    const char* macStr = GET_CHAR(args, "macaddress");
    T_AccessPoint* pAP = NULL;

    SAH_TRACEZ_IN(ME);
    /* Check our input data */
    pAP = obj_AP->priv;

    if(pAP && debugIsVapPointer(pAP)) {
        if(macStr) {
            s_kickSta(pAP, (char*) macStr, -1);
        } else { /* Kick all */
            s_kickSta(pAP, "ff:ff:ff:ff:ff:ff", -1);
        }

        SAH_TRACEZ_OUT(ME);
        return amxd_status_ok;
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_unknown_error;
}


// This function will kick a STA with given MAC from the VAP interface.
amxd_status_t _kickStationReason(amxd_object_t* obj_AP,
                                 amxd_function_t* func _UNUSED,
                                 amxc_var_t* args,
                                 amxc_var_t* ret _UNUSED) {

    const char* macStr = GET_CHAR(args, "macaddress");
    int reason = GET_INT32(args, "reason");

    T_AccessPoint* pAP = NULL;

    SAH_TRACEZ_IN(ME);
    /* Check our input data */
    pAP = obj_AP->priv;

    if(pAP && debugIsVapPointer(pAP)) {
        if(macStr != NULL) {
            s_kickSta(pAP, macStr, reason);
        }
        SAH_TRACEZ_OUT(ME);
        return amxd_status_ok;
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_unknown_error;
}

// This function will cleanup a STA with given MAC from the VAP interface.
amxd_status_t _cleanStation(amxd_object_t* obj_AP,
                            amxd_function_t* func _UNUSED,
                            amxc_var_t* args,
                            amxc_var_t* ret _UNUSED) {

    ASSERT_NOT_NULL(obj_AP, amxd_status_unknown_error, ME, "NULL");
    T_AccessPoint* pAP = obj_AP->priv;
    ASSERT_NOT_NULL(pAP, amxd_status_unknown_error, ME, "NULL");
    ASSERT_TRUE(debugIsVapPointer(pAP), amxd_status_unknown_error, ME, "NULL");
    const char* macStr = GET_CHAR(args, "macaddress");
    ASSERT_TRUE((swl_mac_charIsBroadcast((swl_macChar_t*) macStr)) || (swl_mac_charIsValidStaMac((swl_macChar_t*) macStr)),
                amxd_status_invalid_value, ME, "macStr is invalid");

    SAH_TRACEZ_IN(ME);
    pAP->pFA->mfn_wvap_clean_sta(pAP, (char*) macStr, strlen(macStr));

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t __VerifyAccessPoint(amxd_object_t* object _UNUSED,
                                  amxd_function_t* func _UNUSED,
                                  amxc_var_t* args _UNUSED,
                                  amxc_var_t* ret _UNUSED) {
    return amxd_status_ok;
}

amxd_status_t __CommitAccessPoint(amxd_object_t* object _UNUSED,
                                  amxd_function_t* func _UNUSED,
                                  amxc_var_t* args _UNUSED,
                                  amxc_var_t* ret _UNUSED) {
    return amxd_status_ok;
}

swl_rc_ne wld_vap_saveAssocReq(T_AccessPoint* pAP, swl_bit8_t* frameBin, size_t frameLen) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    swl_80211_mgmtFrame_t* frame = swl_80211_getMgmtFrame(frameBin, frameLen);
    ASSERT_NOT_NULL(frame, SWL_RC_INVALID_PARAM, ME, "NULL");
    uint8_t mgtFrameType = swl_80211_mgtFrameType(&frame->fc);
    if((mgtFrameType != SWL_80211_MGT_FRAME_TYPE_ASSOC_REQUEST) &&
       (mgtFrameType != SWL_80211_MGT_FRAME_TYPE_REASSOC_REQUEST)) {
        SAH_TRACEZ_ERROR(ME, "%s: not (re)assoc frame type (0x%x)", pAP->alias, frame->fc.subType);
        return SWL_RC_INVALID_PARAM;
    }
    char frameStr[(frameLen * 2) + 1];
    bool ret = swl_hex_fromBytesSep(frameStr, sizeof(frameStr), frameBin, frameLen, false, 0, NULL);
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: fail to hex dump frame type %d (len:%zu)", pAP->alias, frame->fc.subType, frameLen);
    swl_timeMono_t timestamp = swl_time_getMonoSec();
    wld_vap_assocTableStruct_t tuple = {frame->transmitter, frame->bssid, frameStr, timestamp, frame->fc.subType};
    swl_circTable_addValues(&(pAP->lastAssocReq), &tuple);
    SAH_TRACEZ_INFO(ME, "%s: add/update assocReq entry for station "SWL_MAC_FMT, pAP->alias,
                    SWL_MAC_ARG(frame->transmitter.bMac));
    return SWL_RC_OK;
}

/**
 * Called to notify the receive of 80211 action frame
 */
swl_rc_ne wld_vap_notifyActionFrame(T_AccessPoint* pAP, const char* frame) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pAP->pBus, SWL_RC_INVALID_PARAM, ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: notify action frame received, frame=%s", pAP->alias, frame);

    amxc_var_t notifMap;
    amxc_var_init(&notifMap);
    amxc_var_set_type(&notifMap, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &notifMap, "frame", frame);
    amxd_object_trigger_signal(pAP->pBus, "MgmtActionFrameReceived", &notifMap);
    amxc_var_clean(&notifMap);

    return SWL_RC_OK;
}

swl_rc_ne wld_vap_notifyDeauthDisassocFrame(T_AccessPoint* pAP, const char* eventName, swl_macBin_t* staMacAddress, swl_IEEE80211deauthReason_ne reason, bool isTxFrame) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pAP->pBus, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(eventName, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(staMacAddress, SWL_RC_INVALID_PARAM, ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: notify %s %s reason=%u", pAP->alias, isTxFrame ? "transmitted" : "received", eventName, reason);

    swl_macChar_t staMac;
    swl_mac_binToChar(&staMac, staMacAddress);
    amxc_var_t notifMap;
    amxc_var_init(&notifMap);
    amxc_var_set_type(&notifMap, AMXC_VAR_ID_HTABLE);
    amxc_var_t* tmpMap = amxc_var_add_key(amxc_htable_t, &notifMap, "Data", NULL);
    amxc_var_add_key(cstring_t, tmpMap, "station", staMac.cMac);
    amxc_var_add_key(uint16_t, tmpMap, "reason", reason);
    amxc_var_add_key(bool, tmpMap, "TxFrame", isTxFrame);
    amxd_object_trigger_signal(pAP->pBus, eventName, &notifMap);
    amxc_var_clean(&notifMap);

    return SWL_RC_OK;
}

swl_rc_ne wld_vap_sync_device(T_AccessPoint* pAP, T_AssociatedDevice* pAD) {
    SAH_TRACEZ_IN(ME);

    ASSERT_NOT_NULL(wld_ad_getOrCreateObject(pAP, pAD), SWL_RC_ERROR, ME, "Fail to get AD object");
    wld_ad_syncInfo(pAD);
    wld_ad_syncStats(pAD);

    SAH_TRACEZ_OUT(ME);
    return SWL_RC_CONTINUE;
}

void wld_vap_syncNrDev(T_AccessPoint* pAP) {
    int active = 0;

    for(int i = 0; i < pAP->AssociatedDeviceNumberOfEntries; i++) {
        if(pAP->AssociatedDevice[i]->Active) {
            active++;
        }
    }

    SAH_TRACEZ_INFO(ME, "%s: sync Assocdev %u %u", pAP->alias,
                    active, pAP->ActiveAssociatedDeviceNumberOfEntries);

    amxd_trans_t trans;
    ASSERT_TRANSACTION_INIT(pAP->pBus, &trans, , ME, "%s : trans init failure", pAP->alias);
    amxd_trans_set_value(int32_t, &trans, "AssociatedDeviceNumberOfEntries", pAP->AssociatedDeviceNumberOfEntries);
    pAP->ActiveAssociatedDeviceNumberOfEntries = active;
    amxd_trans_set_value(int32_t, &trans, "ActiveAssociatedDeviceNumberOfEntries", pAP->ActiveAssociatedDeviceNumberOfEntries);

    wld_rad_updateActiveDevices((T_Radio*) pAP->pRadio, &trans);
    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, , ME, "%s : trans apply failure", pAP->alias);

}

/* update the datamodel with the T_AccessPoint.AssociatedDevice[] */
bool wld_vap_sync_assoclist(T_AccessPoint* pAP) {

    SAH_TRACEZ_INFO(ME, "%s: sync, there are %d devices", pAP->alias, pAP->AssociatedDeviceNumberOfEntries);

    for(int i = 0; i < pAP->AssociatedDeviceNumberOfEntries; i++) {
        T_AssociatedDevice* pAD = pAP->AssociatedDevice[i];

        swl_rc_ne ret = wld_vap_sync_device(pAP, pAD);
        if(ret < SWL_RC_OK) {
            return false;
        } else if(ret == SWL_RC_DONE) {
            i--;
        }
    }

    wld_vap_syncNrDev(pAP);

    SAH_TRACEZ_INFO(ME, "%s: done", pAP->alias);
    return true;
}

/* retrieve the AssociatedDevice with given MAC from T_AccessPoint.AssociatedDevice[] */
T_AssociatedDevice* wld_vap_get_existing_station(T_AccessPoint* pAP, swl_macBin_t* macAddress) {
    ASSERTS_NOT_NULL(pAP, NULL, ME, "NULL");
    ASSERTS_NOT_NULL(macAddress, NULL, ME, "NULL");
    int AssociatedDeviceIdx;
    T_AssociatedDevice* EntryWithMatchingMac = NULL;
    T_AssociatedDevice* pAD;

    for(AssociatedDeviceIdx = 0; AssociatedDeviceIdx < pAP->AssociatedDeviceNumberOfEntries; AssociatedDeviceIdx++) {
        pAD = pAP->AssociatedDevice[AssociatedDeviceIdx];
        if(!pAD) {
            SAH_TRACEZ_ERROR(ME, "NULL pointer in AssociatedDevice[]!");
            SAH_TRACEZ_OUT(ME);
            return NULL;
        }
        if(!memcmp(macAddress->bMac, pAD->MACAddress, ETHER_ADDR_LEN)) {
            EntryWithMatchingMac = pAP->AssociatedDevice[AssociatedDeviceIdx];
            break;
        }
    }

    return EntryWithMatchingMac;
}

swl_rc_ne wld_ap_doForEachLinkedStation(T_AccessPoint* pAP, swl_trl_e onlyAfSta, wld_ap_linkedStaHdlr_f hdlr, void* userData) {
    ASSERTS_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    T_Radio* pRad;
    wld_for_eachRad(pRad) {
        T_AccessPoint* pTmpAP = NULL;
        wld_rad_forEachAp(pTmpAP, pRad) {
            for(int i = 0; i < pTmpAP->AssociatedDeviceNumberOfEntries; i++) {
                wld_affiliatedSta_t* afSta = NULL;
                T_AssociatedDevice* pAD = pTmpAP->AssociatedDevice[i];
                if(pAD && pAD->Active &&
                   (((onlyAfSta != SWL_TRL_FALSE) && ((afSta = wld_ad_getAffiliatedSta(pAD, pAP)) != NULL) && afSta->active) ||
                    ((onlyAfSta != SWL_TRL_TRUE) && (pTmpAP == pAP)))) {
                    if((hdlr != NULL) && (hdlr(userData, pAP, pAD) == true)) {
                        return SWL_RC_DONE;
                    }
                }
            }
        }
    }
    return SWL_RC_OK;
}

static bool s_countLinkedStationsHdlr(void* userData, T_AccessPoint* pAP _UNUSED, T_AssociatedDevice* pAD _UNUSED) {
    ASSERTS_NOT_NULL(userData, false, ME, "NULL");
    uint32_t* pCount = (uint32_t*) userData;
    *pCount += 1;
    return false;
}

uint32_t wld_ap_countLinkedStations(T_AccessPoint* pAP, swl_trl_e onlyAfSta) {
    uint32_t count = 0;
    wld_ap_doForEachLinkedStation(pAP, onlyAfSta, s_countLinkedStationsHdlr, &count);
    return count;
}

static bool s_hasLinkedStationHdlr(void* userData, T_AccessPoint* pAP _UNUSED, T_AssociatedDevice* pAD _UNUSED) {
    ASSERTS_NOT_NULL(userData, false, ME, "NULL");
    bool* pFlag = (bool*) userData;
    *pFlag = true;
    return true;
}

bool wld_ap_hasLinkedStation(T_AccessPoint* pAP, swl_trl_e onlyAfSta) {
    bool flag = false;
    wld_ap_doForEachLinkedStation(pAP, onlyAfSta, s_hasLinkedStationHdlr, &flag);
    return flag;
}

/**
 * free the T_AssociatedDevice and remove it from the list
 * @Deprecated
 *  please use wld_ad_destroy_associatedDevice instead
 **/
void wld_destroy_associatedDevice(T_AccessPoint* pAP, int index) {
    wld_ad_destroy_associatedDevice(pAP, index);
}

/**
 * create T_AssociatedDevice and populate MACAddress and Name fields
 * @Deprecated
 *  please use wld_ad_destroy_associatedDevice instead
 **/
T_AssociatedDevice* wld_create_associatedDevice(T_AccessPoint* pAP, swl_macBin_t* macAddress) {
    return wld_ad_create_associatedDevice(pAP, macAddress);
}

/*
 * @brief compare assoc dev entries with their latestStateChangeTime values
 * @param e1 entry 1
 * @param e2 entry 2
 * @return < 0 when e1 is selected (ie *e1 not null and oldest against *e2)
 *         > 0 when e2 is selected (ie *e2 not null and oldest against *e1)
 *         = 0 when e1 and e2 are equivalent
 */
static int s_inactAdCmp(const void* e1, const void* e2) {
    T_AssociatedDevice** ppSta1 = (T_AssociatedDevice**) e1;
    T_AssociatedDevice** ppSta2 = (T_AssociatedDevice**) e2;
    //not null first
    ASSERT_NOT_NULL(ppSta1, ((ppSta2 != NULL) && (*ppSta2 != NULL)), ME, "NULL");
    ASSERT_NOT_NULL(ppSta2, -(*ppSta1 != NULL), ME, "NULL");
    ASSERT_NOT_NULL(*ppSta1, (*ppSta2 != NULL), ME, "NULL");
    ASSERT_NOT_NULL(*ppSta2, -1, ME, "NULL");
    //oldest first
    return ((*ppSta1)->latestStateChangeTime - (*ppSta2)->latestStateChangeTime);
}

/* clean up stations who failed authentication */
bool wld_vap_cleanup_stationlist(T_AccessPoint* pAP) {
    int inactiveStaCount = 0;

    /* Allow two devices with Authenticationstate==false
     * The first is one to keep around (to diagnose failed connection attempts)
     * The second is necessary to allow a client to go through
     * AuthenticationState==false => AuthenticationState==true when connecting
     * Since we should call this function each time a sta DC's, we use a simple
     * cleanup algorithm instead of doing the effort of sorting and cleaning.
     */
    T_AssociatedDevice* inactiveStaList[pAP->AssociatedDeviceNumberOfEntries + 1];
    for(int assocIndex = 0; assocIndex < pAP->AssociatedDeviceNumberOfEntries; assocIndex++) {
        T_AssociatedDevice* pAD = pAP->AssociatedDevice[assocIndex];
        if((pAD != NULL) && (!pAD->Active)) {
            inactiveStaList[inactiveStaCount] = pAD;
            inactiveStaCount++;
        }
    }
    if(inactiveStaCount <= NR_OF_STICKY_UNAUTHORIZED_STATIONS) {
        return true;
    }
    //sort inactive stations by latestStateChangeTime
    qsort(inactiveStaList, inactiveStaCount, sizeof(T_AssociatedDevice*), s_inactAdCmp);
    for(int i = 0; i < (inactiveStaCount - NR_OF_STICKY_UNAUTHORIZED_STATIONS); i++) {
        T_AssociatedDevice* inactiveSta = inactiveStaList[i];
        if(inactiveSta == NULL) {
            continue;
        }

        if(wld_ad_hasDelayedDisassocNotif(inactiveSta)) {
            SAH_TRACEZ_INFO(ME, "skip inactive sta(%s) having pending disc notif", inactiveSta->Name);
            continue;
        }

        SAH_TRACEZ_INFO(ME, "Destroying oldest failed-auth entry (%s) out of %d",
                        inactiveSta->Name,
                        inactiveStaCount);
        if(!wld_ad_destroy(pAP, inactiveSta)) {
            SAH_TRACEZ_ERROR(ME, "Fail to destroy dm entry of (%s)", inactiveSta->Name);
            return false;
        }
    }
    return true;
}

bool wld_vap_assoc_update_cuid(T_AccessPoint* pAP, swl_macBin_t* mac, char* cuid, int len _UNUSED) {

    T_AssociatedDevice* pAD = wld_vap_find_asociatedDevice(pAP, mac);
    ASSERT_NOT_NULL(pAD, false, ME, "NULL");

    memset(pAD->Radius_CUID, 0, sizeof(pAD->Radius_CUID));
    memcpy(pAD->Radius_CUID, cuid, sizeof(pAD->Radius_CUID));

    wld_vap_sync_device(pAP, pAD);
    return true;
}

void wld_ap_sendPairingNotification(T_AccessPoint* pAP, uint32_t type, const char* reason, const char* macAddress) {
    wld_wps_sendPairingNotification(pAP->pBus, type, reason, macAddress, NULL);
}

T_AccessPoint* wld_ap_fromIt(amxc_llist_it_t* it) {
    ASSERTS_NOT_NULL(it, NULL, ME, "NULL");
    return amxc_llist_it_get_data(it, T_AccessPoint, it);
}

/****************
 * DEBUG SUPPORT
 ***************/

amxd_status_t _dbgClearInactiveEntries(amxd_object_t* object,
                                       amxd_function_t* func _UNUSED,
                                       amxc_var_t* args _UNUSED,
                                       amxc_var_t* ret _UNUSED) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, amxd_status_ok, ME, "NULL");

    for(int AssociatedDeviceIdx = 0; AssociatedDeviceIdx < pAP->AssociatedDeviceNumberOfEntries; AssociatedDeviceIdx++) {
        T_AssociatedDevice* dev = pAP->AssociatedDevice[AssociatedDeviceIdx];
        if((dev != NULL) && !dev->Active) {
            wld_ad_destroy(pAP, dev);
        }
    }
    wld_vap_sync_assoclist(pAP);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

T_AccessPoint* wld_vap_get_vap(const char* ifname) {
    T_Radio* pRad;
    wld_for_eachRad(pRad) {
        T_AccessPoint* pAP;
        wld_rad_forEachAp(pAP, pRad) {
            if(pAP && swl_str_matches(pAP->alias, ifname)) {
                return pAP;
            }
        }
    }

    return NULL;
}

T_AccessPoint* wld_ap_getVapByName(const char* name) {
    T_Radio* pRad;
    wld_for_eachRad(pRad) {
        T_AccessPoint* pAP;
        wld_rad_forEachAp(pAP, pRad) {
            if(pAP && swl_str_matches(pAP->name, name)) {
                return pAP;
            }
        }
    }

    return NULL;
}

T_AccessPoint* wld_ap_getVapByBssid(swl_macBin_t* bssid) {
    T_SSID* pSSID = wld_ssid_getSsidByMacAddress(bssid);
    if((pSSID != NULL) && (pSSID->RADIO_PARENT != NULL) && (pSSID->AP_HOOK != NULL) && (pSSID->AP_HOOK->pRadio == pSSID->RADIO_PARENT)) {
        return pSSID->AP_HOOK;
    }
    return NULL;
}

uint32_t wld_ap_getMaxNbrSta(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, 0, ME, "NULL");
    T_Radio* pRad = pAP->pRadio;
    ASSERT_NOT_NULL(pRad, 0, ME, "NULL");
    /*
     * Compare how many stations are still allowed on the access point and its radio.
     * In case the number of remaining allowed stations on the radio is smaller than the
     * number of remaining allowed stations on the access point, we set the BSS max_num_sta
     * to honor the radio limit. This is necessary because hostapd does not have a limit per
     * radio, only per access point.
     */
    int32_t curMaxNumSta = (pAP->MaxStations < 0) ? (int32_t) pRad->maxNrHwSta : pAP->MaxStations;
    SAH_TRACEZ_INFO(ME, "%s: pRad->maxStations = %d pRad->currentStations = %d", pRad->Name, pRad->maxStations, pRad->currentStations);
    SAH_TRACEZ_INFO(ME, "%s: pAP->MaxStations = %d  pAP->ActiveAssociatedDeviceNumberOfEntries = %d", pAP->alias, curMaxNumSta, pAP->ActiveAssociatedDeviceNumberOfEntries);
    if((pRad->maxStations > 0) && (pRad->maxStations - pRad->currentStations < curMaxNumSta - pAP->ActiveAssociatedDeviceNumberOfEntries)) {
        curMaxNumSta = pAP->ActiveAssociatedDeviceNumberOfEntries + pRad->maxStations - pRad->currentStations;
    }
    return curMaxNumSta < 0 ? 0 : (uint32_t) curMaxNumSta;
}

static void s_vap_updateState(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, , ME, "NULL");

    T_Radio* pRad = (T_Radio*) pAP->pRadio;
    T_SSID* pSSID = (T_SSID*) pAP->pSSID;

    wld_intfStatus_e oldVapStatus = pAP->status;
    wld_status_e oldSsidStatus = pSSID ? pSSID->status : RST_UNKNOWN;

    wld_status_e newSsidStatus;

    int status = pAP->pFA->mfn_wvap_status(pAP);
    if(status > 0) {
        pAP->status = APSTI_ENABLED;
    } else {
        pAP->status = APSTI_DISABLED;
    }

    if((pRad == NULL) || (pRad->status == RST_ERROR)) {
        newSsidStatus = RST_ERROR;
    } else if((pAP->status == APSTI_DISABLED) || (pRad->status == RST_DOWN)) {
        newSsidStatus = RST_DOWN;
    } else if(pRad->status == RST_DORMANT) {
        newSsidStatus = RST_LLDOWN;
    } else {
        newSsidStatus = RST_UP;
    }

    SAH_TRACEZ_INFO(ME, "%s: ap %u -> %u / ssid %u -> %u", pAP->alias, oldVapStatus, pAP->status, oldSsidStatus, newSsidStatus);

    //update DM Status of DM configurable VAPs
    ASSERT_TRUE((pAP->pBus == NULL) ||
                swl_typeCharPtr_commitObjectParam(pAP->pBus, "Status", (char*) cstr_AP_status[pAP->status]), ,
                ME, "%s: fail to commit status (%s)", pAP->alias, cstr_AP_status[pAP->status]);

    ASSERT_TRUE((pSSID == NULL) || (pSSID->pBus == NULL) ||
                swl_typeCharPtr_commitObjectParam(pSSID->pBus, "Status", (char*) Rad_SupStatus[newSsidStatus]), ,
                ME, "%s: fail to commit status (%s)", pAP->name, Rad_SupStatus[newSsidStatus]);

    ASSERTI_FALSE((oldVapStatus == pAP->status) && (oldSsidStatus == newSsidStatus), ,
                  ME, "%s: status not changed", pAP->alias);

    pAP->lastStatusChange = swl_time_getMonoSec();
    wld_ssid_setStatus(pSSID, newSsidStatus, true);
    wld_vap_status_change_event_t vapUpdate;
    vapUpdate.vap = pAP;
    vapUpdate.oldSsidStatus = oldSsidStatus;
    vapUpdate.oldVapStatus = oldVapStatus;
    wld_event_trigger_callback(gWld_queue_vap_onStatusChange, &vapUpdate);

    wld_wps_updateState(pAP);
    wld_apRssiMon_updateEnable(pAP);
}

void wld_vap_updateState(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    s_vap_updateState(pAP);
    T_SSID* pSSID = (T_SSID*) pAP->pSSID;
    ASSERT_NOT_NULL(pSSID, , ME, "%s: SSID reference is NULL", pAP->alias);
    wld_mldLink_t* pLink = pSSID->pMldLink;
    if(pLink != NULL) {
        /* if part of an MLD, update all links */
        wld_for_eachNeighMldLink_safe(pNgLink, pLink) {
            T_SSID* pNgSSID = wld_mld_getLinkSsid(pNgLink);
            if(pNgSSID && pNgSSID->AP_HOOK && (pNgSSID->AP_HOOK != pAP)) {
                s_vap_updateState(pNgSSID->AP_HOOK);
            }
        }
    }
}

/**
 * Return whether the AP must be up (true) or down (false) regarding radio state:
 * - Check whether AP is enabled
 * - Check whether radio is up
 */
bool wld_ap_getDesiredState(T_AccessPoint* pAp) {
    ASSERT_NOT_NULL(pAp, false, ME, "Null AP");
    T_Radio* pR = pAp->pRadio;
    ASSERT_NOT_NULL(pR, false, ME, "Null radio");
    if(!pAp->enable) {
        return false;
    }
    if(!wld_rad_isUpExt(pR)) {
        return false;
    }
    return true;
}

static void s_setApDriverConfig_ocf(void* priv _UNUSED, amxd_object_t* object, const amxc_var_t* const newParamValues _UNUSED) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(object));
    ASSERTI_NOT_NULL(pAP, , ME, "NULL");

    wld_vap_driverCfgChange_m params = 0;
    amxc_var_for_each(newValue, newParamValues) {
        const char* pname = amxc_var_key(newValue);
        if(swl_str_matches(pname, "BssMaxIdlePeriod")) {
            int32_t tmpInt32 = amxc_var_dyncast(int32_t, newValue);
            if(tmpInt32 != pAP->driverCfg.bssMaxIdlePeriod) {
                pAP->driverCfg.bssMaxIdlePeriod = tmpInt32;
                params |= M_WLD_VAP_DRIVER_CFG_CHANGE_BSS_MAX_IDLE_PERIOD;
            }
        }
    }
    if(params) {
        SAH_TRACEZ_INFO(ME, "%s update driver config (0x%x)", pAP->alias, params);
        pAP->pFA->mfn_wvap_set_config_driver(pAP, params);
    }

    SAH_TRACEZ_OUT(ME);
}

SWLA_DM_HDLRS(sApDriverConfigDmHdlrs, ARR(), .objChangedCb = s_setApDriverConfig_ocf);

void _wld_ap_setDriverConfig_ocf(const char* const sig_name,
                                 const amxc_var_t* const data,
                                 void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sApDriverConfigDmHdlrs, sig_name, data, priv);
}

/**
 * @brief return the last assoc/reassoc frame sent by a station to a vap
 * @param pAP the accesspoint receiving the assoc/reassoc frame
 * @param macStation the station mac address
 * @param data contains the last assoc/reassoc tuple associated to macStation if found
 * @return - SWL_RC_OK: if the last assoc/reassoc frame is found for the given station
 *         - SWL_RC_INVALID_PARAM if pAP = NULL or macStation = NULL or data = NULL
 *         - SWL_RC_ERROR if no assoc/reassoc tuple associated to macStation is found
 */

swl_rc_ne wld_ap_getLastAssocReq(T_AccessPoint* pAP, const char* macStation, wld_vap_assocTableStruct_t** data) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_STR(macStation, SWL_RC_INVALID_PARAM, ME, "empty mac");
    ASSERT_NOT_NULL(data, SWL_RC_INVALID_PARAM, ME, "NULL");

    swl_macBin_t bMac;
    SWL_MAC_CHAR_TO_BIN(&bMac, macStation);

    *data = swl_circTable_getMatchingTupleInRange(&(pAP->lastAssocReq), 0, &bMac, -1, 0, -1);
    ASSERT_NOT_NULL(*data, SWL_RC_ERROR, ME, "NULL");
    return SWL_RC_OK;
}



swl_rc_ne wld_vap_registerExtModData(T_AccessPoint* pAP, uint32_t extModId, void* extModData, wld_extMod_deleteData_dcf deleteHandler) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_extMod_registerData(&pAP->extDataList, extModId, extModData, deleteHandler);
}

void* wld_vap_getExtModData(T_AccessPoint* pAP, uint32_t extModId) {
    ASSERT_NOT_NULL(pAP, NULL, ME, "NULL");
    return wld_extMod_getData(&pAP->extDataList, extModId);
}

swl_rc_ne wld_vap_unregisterExtModData(T_AccessPoint* pAP, uint32_t extModId) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_extMod_unregisterData(&pAP->extDataList, extModId);
}

/**
 * Certain vendors need dummy vaps, which are not relevant for configuration.
 * This is to allow inspection whether a vap is a dummy.
 *
 */
bool wld_vap_isDummyVap(T_AccessPoint* pAP) {
    return pAP->pBus == NULL;
}

static void s_syncVapNetdevIndex(char* apCtxName) {
    T_AccessPoint* pAP = wld_vap_get_vap(apCtxName);
    free(apCtxName);
    ASSERTI_NOT_NULL(pAP, , ME, "NULL");
    swl_typeUInt32_commitObjectParam(pAP->pBus, "Index", pAP->index);

    T_SSID* pSSID = pAP->pSSID;
    if(pSSID != NULL) {
        swl_typeUInt32_commitObjectParam(pSSID->pBus, "Index", pAP->index);
    }

    T_Radio* pRad = pAP->pRadio;
    if(pRad != NULL) {
        swl_typeUInt32_commitObjectParam(pRad->pBus, "Index", pRad->index);
    }
    wld_vap_updateState(pAP);
}

void wld_vap_setNetdevIndex(T_AccessPoint* pAP, int32_t netDevIndex) {
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: update NetDevIndex from %d to %d", pAP->alias, pAP->index, netDevIndex);

    pAP->index = netDevIndex;

    ASSERT_NOT_NULL(pAP->alias, , ME, "NULL");
    swla_delayExec_add((swla_delayExecFun_cbf) s_syncVapNetdevIndex, strdup(pAP->alias));
}


static amxd_status_t s_getLastAssocReq(T_AccessPoint* pAP, const char* macStation, amxc_var_t* retval) {
    wld_vap_assocTableStruct_t* tuple = NULL;
    swl_rc_ne ret = wld_ap_getLastAssocReq(pAP, macStation, &tuple);
    ASSERT_TRUE(swl_rc_isOk(ret), amxd_status_unknown_error, ME, "Error during execution");

    amxc_var_init(retval);
    amxc_var_set_type(retval, AMXC_VAR_ID_HTABLE);
    for(size_t i = 0; i < assocTable.nrTypes; i++) {
        swl_type_t* type = assocTable.types[i];
        swl_typeData_t* tmpValue = swl_tupleType_getValue(&assocTable, tuple, i);
        amxc_var_t* tmpVar = amxc_var_add_new_key(retval, s_assocTableNames[i]);
        swl_type_toVariant(type, tmpVar, tmpValue);
    }
    return amxd_status_ok;
}

/**
 * @brief return the last assoc/reassoc frame sent by a station to a vap
 * @param pAP the accesspoint receiving the assoc/reassoc frame
 * @param mac the station mac address
 * @return - a variant map containing the sent assoc/reassoc frame by a station
 *         - Otherwise
 *              - amxd_status_parameter_not_found if mac is missing
 *              - amxd_status_unknown_error if no assoc/reassoc frame associated to mac is found
 */
amxd_status_t _AccessPoint_getLastAssocReq(amxd_object_t* object,
                                           amxd_function_t* func _UNUSED,
                                           amxc_var_t* args,
                                           amxc_var_t* retval) {
    T_AccessPoint* pAP = object->priv;
    ASSERT_TRUE(debugIsVapPointer(pAP), amxd_status_unknown_error, ME, "invalid AP");
    const char* macStation = GET_CHAR(args, "mac");
    ASSERT_NOT_NULL(macStation, amxd_status_parameter_not_found, ME, "No mac station given");
    return s_getLastAssocReq(pAP, macStation, retval);
}

amxd_status_t _AssociatedDevice_getLastAssocReq(amxd_object_t* object,
                                                amxd_function_t* func _UNUSED,
                                                amxc_var_t* args _UNUSED,
                                                amxc_var_t* retval) {
    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(amxd_object_get_parent(object)));
    ASSERT_NOT_NULL(pAP, amxd_status_unknown_error, ME, "invalid AP parent");
    char* macStation = amxd_object_get_cstring_t(object, "MACAddress", NULL);
    ASSERT_NOT_NULL(macStation, amxd_status_parameter_not_found, ME, "No mac station given");
    amxd_status_t status = s_getLastAssocReq(pAP, macStation, retval);
    free(macStation);
    return status;
}

/**
 * @brief Manually disassociates a station
 * @param mac The MAC address of the station to disassociate.
 * @return amxd_status_t
 *         - amxd_status_ok if the station was successfully disassociated.
 *         - amxd_status_parameter_not_found if the MAC address is missing.
 *         - A corresponding amxd status error if a hostapd error occurred.
 */
amxd_status_t _AccessPoint_disassocStation(amxd_object_t* object _UNUSED,
                                           amxd_function_t* func _UNUSED,
                                           amxc_var_t* args,
                                           amxc_var_t* retval _UNUSED) {

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, amxd_status_unknown_error, ME, "Not mapped to AP ctx");

    const char* staMacStr = GET_CHAR(args, "mac");
    swl_macBin_t staMac;

    uint32_t reason = GET_UINT32(args, "reason");
    // Convert MAC string to MAC binary
    ASSERT_NOT_NULL(staMacStr, amxd_status_parameter_not_found, ME, "No MAC given");
    bool success = SWL_MAC_CHAR_TO_BIN(&staMac, staMacStr);
    ASSERTS_TRUE(success, amxd_status_invalid_value, ME, "Fail to convert mac");

    swl_rc_ne result = pAP->pFA->mfn_wvap_disassoc_sta_reason(pAP, &staMac, reason);

    ASSERT_EQUALS(result, SWL_RC_OK, amxd_status_unknown_error, ME, "Fail to disassociate station");

    return amxd_status_ok;
}

amxd_status_t _AccessPoint_debug(amxd_object_t* obj,
                                 amxd_function_t* func _UNUSED,
                                 amxc_var_t* args,
                                 amxc_var_t* retMap) {
    T_AccessPoint* pAP = wld_ap_fromObj(obj);
    ASSERT_NOT_NULL(pAP, amxd_status_ok, ME, "No AP Ctx");

    const char* feature = GET_CHAR(args, "op");
    ASSERT_NOT_NULL(feature, amxd_status_unknown_error, ME, "No argument given");

    amxc_var_init(retMap);
    amxc_var_set_type(retMap, AMXC_VAR_ID_HTABLE);

    if(!strcasecmp(feature, "RssiMon")) {
        wld_ap_rssiEv_debug(pAP, retMap);
    } else if(!strcasecmp(feature, "recentDcs")) {
        amxc_var_set_type(retMap, AMXC_VAR_ID_LIST);
        wld_ad_listRecentDisconnects(pAP, retMap);
    } else if(!strcasecmp(feature, "SSIDAdvertisementEnabled")) {
        bool enable = GET_BOOL(args, "enable");
        wld_ap_hostapd_setSSIDAdvertisement(pAP, enable);
    } else if(!strcasecmp(feature, "setKey")) {
        const char* key = GET_CHAR(args, "key");
        const char* value = GET_CHAR(args, "value");
        if(!strcasecmp(key, "wep_key")) {
            swl_str_copy(pAP->WEPKey, sizeof(pAP->WEPKey), value);
        } else if(!strcasecmp(key, "wpa_psk")) {
            swl_str_copy(pAP->preSharedKey, sizeof(pAP->preSharedKey), value);
        } else if(!strcasecmp(key, "wpa_passphrase")) {
            swl_str_copy(pAP->keyPassPhrase, sizeof(pAP->keyPassPhrase), value);
        } else if(!strcasecmp(key, "sae_password")) {
            swl_str_copy(pAP->saePassphrase, sizeof(pAP->saePassphrase), value);
        }
        wld_ap_hostapd_setSecretKey(pAP);
    } else if(!strcasecmp(feature, "nl80211IfaceInfo")) {
        wld_nl80211_ifaceInfo_t ifaceInfo;
        if(wld_ap_nl80211_getInterfaceInfo(pAP, &ifaceInfo) < SWL_RC_OK) {
            amxc_var_add_key(cstring_t, retMap, "Error", "Fail to get nl80211 iface info");
        } else {
            wld_nl80211_dumpIfaceInfo(&ifaceInfo, retMap);
        }
    } else if(swl_str_matchesIgnoreCase(feature, "nl80211StationInfo")) {
        swl_rc_ne rc;
        const char* sta = GET_CHAR(args, "sta");
        if(sta && sta[0]) {
            wld_nl80211_stationInfo_t stationInfo;
            swl_macBin_t bMac;
            if(!swl_mac_charToBin(&bMac, (swl_macChar_t*) sta)) {
                rc = SWL_RC_INVALID_PARAM;
            } else if((rc = wld_ap_nl80211_getStationInfo(pAP, &bMac, &stationInfo)) >= SWL_RC_OK) {
                wld_nl80211_dumpStationInfo(&stationInfo, retMap);
            }
        } else {
            wld_nl80211_stationInfo_t* allStationInfo = NULL;
            uint32_t nStations = 0;
            if((rc = wld_ap_nl80211_getAllStationsInfo(pAP, &allStationInfo, &nStations)) >= SWL_RC_OK) {
                wld_nl80211_dumpAllStationInfo(allStationInfo, nStations, retMap);
            }
            free(allStationInfo);
        }
        if(rc < SWL_RC_OK) {
            amxc_var_add_key(cstring_t, retMap, "Error", swl_rc_toString(rc));
        }
    } else if(!strcasecmp(feature, "kickSta")) {
        const char* sta = GET_CHAR(args, "sta");
        uint32_t reason = GET_UINT32(args, "reason");
        if(reason == 0) {
            reason = SWL_IEEE80211_DEAUTH_REASON_UNSPECIFIED;
        }
        swl_macBin_t bMac;

        SWL_MAC_CHAR_TO_BIN(&bMac, sta);

        wld_ap_hostapd_kickStation(pAP, &bMac, (swl_IEEE80211deauthReason_ne) reason);
    } else if(swl_str_matchesIgnoreCase(feature, "clientIsolation")) {
        int ap_isolate = GET_INT32(args, "ap_isolate");
        pAP->clientIsolationEnable = ap_isolate;
        wld_secDmn_action_rc_ne ret = wld_ap_hostapd_setClientIsolation(pAP);
        wld_ap_hostapd_updateBeacon(pAP, "ap_isolate");
        if(ret == SECDMN_ACTION_ERROR) {
            amxc_var_add_key(cstring_t, retMap, "Error", "WLD_KO");
        } else {
            amxc_var_add_key(cstring_t, retMap, "OK", "WLD_OK");
        }
    } else if(swl_str_matchesIgnoreCase(feature, "clearMacAcl")) {
        swl_rc_ne ret = wld_ap_hostapd_delAllMacFilteringEntries(pAP);
        amxc_var_add_key(cstring_t, retMap, "Result", swl_rc_toString(ret));
    } else if(swl_str_matchesIgnoreCase(feature, "setMacAcl")) {
        swl_rc_ne ret = wld_ap_hostapd_setMacFilteringList(pAP);
        amxc_var_add_key(cstring_t, retMap, "Result", swl_rc_toString(ret));
    } else if(swl_str_matchesIgnoreCase(feature, "addMacEntry")) {
        const char* macStr = GET_CHAR(args, "macStr");
        swl_rc_ne ret = wld_ap_hostapd_addMacFilteringEntry(pAP, (char*) macStr);
        amxc_var_add_key(cstring_t, retMap, "Result", swl_rc_toString(ret));
    } else if(swl_str_matchesIgnoreCase(feature, "delMacEntry")) {
        const char* macStr = GET_CHAR(args, "macStr");
        swl_rc_ne ret = wld_ap_hostapd_delMacFilteringEntry(pAP, (char*) macStr);
        amxc_var_add_key(cstring_t, retMap, "Result", swl_rc_toString(ret));
    } else if(!strcasecmp(feature, "writeSta")) {
        swl_print_args_t tmpArgs = g_swl_print_json;
        FILE* fp = fopen("/tmp/vapStaDump.txt", "w");
        if(fp == NULL) {
            SAH_TRACEZ_ERROR(ME, "Failure to open file");
            return amxd_status_file_not_found;
        }
        swl_type_arrayToFilePrint(&gtWld_associatedDevicePtr.type, fp, pAP->AssociatedDevice,
                                  pAP->AssociatedDeviceNumberOfEntries, false, &tmpArgs);
        fclose(fp);
    } else {
        amxc_var_add_key(cstring_t, retMap, "Error", "Unknown command");
    }

    return amxd_status_ok;
}

SWLA_DM_HDLRS(sApDmHdlrs,
              ARR(SWLA_DM_PARAM_HDLR("SSIDReference", s_setSSIDRef_pwf),
                  SWLA_DM_PARAM_HDLR("SSIDAdvertisementEnabled", s_setSsidAdvEnabled_pwf),
                  SWLA_DM_PARAM_HDLR("RetryLimit", s_setApCommonParam_pwf),
                  SWLA_DM_PARAM_HDLR("WMMEnable", s_setApCommonParam_pwf),
                  SWLA_DM_PARAM_HDLR("RetryLimit", s_setApCommonParam_pwf),
                  SWLA_DM_PARAM_HDLR("UAPSDEnable", s_setApCommonParam_pwf),
                  SWLA_DM_PARAM_HDLR("MCEnable", s_setApCommonParam_pwf),
                  SWLA_DM_PARAM_HDLR("APBridgeDisable", s_setApCommonParam_pwf),
                  SWLA_DM_PARAM_HDLR("IsolationEnable", s_setApCommonParam_pwf),
                  SWLA_DM_PARAM_HDLR("BridgeInterface", s_setBridgeInterface_pwf),
                  SWLA_DM_PARAM_HDLR("DefaultDeviceType", s_setDefaultDeviceType_pwf),
                  SWLA_DM_PARAM_HDLR("IEEE80211kEnabled", s_set80211kEnabled_pwf),
                  SWLA_DM_PARAM_HDLR("WDSEnable", s_setWDSEnable_pwf),
                  SWLA_DM_PARAM_HDLR("MBOEnable", s_setMBOEnable_pwf),
                  SWLA_DM_PARAM_HDLR("MBOAssocDisallowReason", s_setMBOAssocDisallowReason_pwf),
                  SWLA_DM_PARAM_HDLR("MultiAPType", s_setMultiAPType_pwf),
                  SWLA_DM_PARAM_HDLR("MultiAPProfile", s_setMultiAPProfile_pwf),
                  SWLA_DM_PARAM_HDLR("MultiAPVlanId", s_setMultiAPVlanId_pwf),
                  SWLA_DM_PARAM_HDLR("ApRole", s_setApRole_pwf),
                  SWLA_DM_PARAM_HDLR("ReferenceApRelay", s_setReferenceApRelay_pwf),
                  SWLA_DM_PARAM_HDLR("MaxAssociatedDevices", s_setMaxStations_pwf),
                  SWLA_DM_PARAM_HDLR("DiscoveryMethodEnabled", s_setDiscoveryMethod_pwf),
                  SWLA_DM_PARAM_HDLR("dbgAPEnable", s_setDbgEnable_pwf),
                  SWLA_DM_PARAM_HDLR("dbgAPFile", s_setDbgFile_pwf),
                  SWLA_DM_PARAM_HDLR("MACFilterAddressList", wld_apMacFilter_setAddressList_pwf),
                  SWLA_DM_PARAM_HDLR("Enable", s_setApEnable_pwf),
                  SWLA_DM_PARAM_HDLR("MACAddressControlEnabled", s_setMACAddressControlEnabled_pwf),
                  SWLA_DM_PARAM_HDLR("StaInactivityTimeout", s_setStaInactivityTimeout_pwf),
                  ),
              .instAddedCb = s_addApInst_oaf, );

void _wld_ap_setConf_ocf(const char* const sig_name,
                         const amxc_var_t* const data,
                         void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sApDmHdlrs, sig_name, data, priv);
}

static void s_updateMloStats(amxd_object_t* const obj, wld_mloStats_t* stats) {
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "LinkID", stats->linkid);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "PacketsSent", stats->txPackets);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "PacketsReceived", stats->rxPackets);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "UnicastBytesSent", stats->txUbyte);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "UnicastBytesReceived", stats->rxUbyte);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "ErrorsSent", stats->txEbyte);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "ErrorsReceived", stats->rxEbyte);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "MulticastBytesSent", stats->txMbyte);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "MulticastBytesReceived", stats->rxMbyte);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "BroadcastBytesSent", stats->txBbyte);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "BroadcastBytesReceived", stats->rxBbyte);
}

amxd_status_t _wld_ap_getMloStats_orf(amxd_object_t* const object,
                                      amxd_param_t* const param,
                                      amxd_action_t reason,
                                      const amxc_var_t* const args,
                                      amxc_var_t* const action_retval,
                                      void* priv) {
    SAH_TRACEZ_IN(ME);
    if(reason != action_object_read) {
        return amxd_status_function_not_implemented;
    }
    T_AccessPoint* pAP = (T_AccessPoint*) amxd_object_get_parent(object)->priv;
    ASSERT_NOT_NULL(pAP, amxd_status_object_not_found, ME, "AccessPoint object not found!");
    wld_mloStats_t mloStats;
    memset(&mloStats, 0, sizeof(wld_mloStats_t));
    if(pAP->pFA->mfn_wvap_getMloStats(pAP, &mloStats) >= SWL_RC_OK) {
        s_updateMloStats(object, &mloStats);
    }
    return amxd_action_object_read(object, param, reason, args, action_retval, priv);
}

