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

#define ME "ep"

#include <stdlib.h>
#include <debug/sahtrace.h>


#include <string.h>
#include <swla/swla_mac.h>

#include "wld.h"
#include "wld_radio.h"
#include "wld_endpoint.h"
#include "wld_tinyRoam.h"
#include "wld_ssid.h"
#include "wld_wps.h"
#include "wld_util.h"
#include "swl/swl_assert.h"
#include "wld_eventing.h"
#include "wld_assocdev.h"
#include "wld_wpaSupp_cfgFile.h"
#include "wld_wpaSupp_ep_api.h"
#include "wld_wpaSupp_cfgManager.h"
#include "Utils/wld_autoCommitMgr.h"
#include "wld_epProfile.h"
#include "wld_dm_trans.h"
#include "wld_chanmgt.h"

/* Function prototypes for helpers. */
static void s_setEndpointStatus(T_EndPoint* pEP,
                                wld_intfStatus_e status,
                                wld_epConnectionStatus_e connectionStatus,
                                wld_epError_e error);
static void s_setProfileStatus(T_EndPointProfile* profile);
static void s_setMultiApInfo(T_EndPoint* pEP);
static void endpoint_wps_pbc_delayed_time_handler(amxp_timer_t* timer, void* userdata);
static swl_rc_ne endpoint_wps_start(T_EndPoint* pEP, uint64_t call_id, amxc_var_t* args);
static void endpoint_reconnect_handler(amxp_timer_t* timer, void* userdata);

/* Possible Endpoint Status Values */
const char* cstr_EndPoint_status[] = {"Disabled", "Enabled", "Error_Misconfigured", "Error", 0};

/* Possible Endpoint ConnectionStatus values */
const char* cstr_EndPoint_connectionStatus[] = {"Disabled", "Idle", "Discovering", "Connecting",
    "WPS_Pairing", "WPS_PairingDone", "WPS_Timeout", "Connected", "Disconnected", "Error",
    "Error_Misconfigured", NULL};

/* Possible Endpoint LastError values */
const char* cstr_EndPoint_lastError[] = {"None", "SSID_Not_Found", "Invalid_PassPhrase",
    "SecurityMethod_Unsupported", "WPS_Timeout", "WPS_Canceled", "Error_Misconfigured",
    "Association_Timeout", NULL};

/* Possible EndpointProfile Status values */
const char* cstr_EndPointProfile_status[] = {"Active", "Available", "Error", "Disabled", NULL};

T_EndPoint* wld_ep_fromObj(amxd_object_t* epObj) {
    ASSERTS_EQUALS(amxd_object_get_type(epObj), amxd_object_instance, NULL, ME, "Not instance");
    amxd_object_t* parentObj = amxd_object_get_parent(epObj);
    ASSERT_EQUALS(get_wld_object(), amxd_object_get_parent(parentObj), NULL, ME, "wrong location");
    const char* parentName = amxd_object_get_name(parentObj, AMXD_OBJECT_NAMED);
    ASSERT_TRUE(swl_str_matches(parentName, "EndPoint"), NULL, ME, "invalid parent obj(%s)", parentName);
    T_EndPoint* pEP = (T_EndPoint*) epObj->priv;
    ASSERTS_TRUE(pEP, NULL, ME, "NULL");
    ASSERT_TRUE(debugIsEpPointer(pEP), NULL, ME, "INVALID");
    return pEP;
}

int32_t wld_endpoint_isProfileIdentical(T_EndPointProfile* currentProfile, T_EndPointProfile* newProfile) {
    ASSERTS_NOT_NULL(currentProfile, LFALSE, ME, "currentProfile is NULL");
    ASSERTS_NOT_NULL(newProfile, LFALSE, ME, "newProfile is NULL");

    if(memcmp(currentProfile->BSSID, &wld_ether_null, ETHER_ADDR_LEN) != 0) {
        if(memcmp(currentProfile->BSSID, newProfile->BSSID, ETHER_ADDR_LEN) != 0) {
            return LFALSE;
        }
    }

    ASSERT_TRUE(swl_str_matches(currentProfile->SSID, newProfile->SSID), LFALSE, ME, "SSID not identical");
    ASSERT_TRUE(swl_str_matches(currentProfile->keyPassPhrase, newProfile->keyPassPhrase), LFALSE, ME, "keyPassPhrase not identical");
    ASSERT_TRUE(swl_str_matches(currentProfile->saePassphrase, newProfile->saePassphrase), LFALSE, ME, "saePassphrase not identical");
    ASSERT_TRUE((currentProfile->secModeEnabled == newProfile->secModeEnabled), LFALSE, ME, "secModeEnabled not identical");

    return LTRUE;
}

/**
 * @brief wld_endpoint_setRadToggleThreshold_pwf
 *
 * Parameter handler to handle Endpoint radio toggle parameter
 */
static void s_setRadToggleThreshold_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_EndPoint* pEP = wld_ep_fromObj(object);
    ASSERT_NOT_NULL(pEP, , ME, "INVALID");

    SAH_TRACEZ_INFO(ME, "%s: EndpointRadToggleThreshold changed", pEP->Name);
    pEP->reconnect_rad_trigger = amxc_var_dyncast(uint32_t, newValue);

    SAH_TRACEZ_OUT(ME);
}

/**
 * @brief wld_endpoint_setReconnectDelay_pwf
 *
 * Parameter handler to handle Endpoint reconnect delay time parameter
 */
static void s_setReconnectDelay_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_EndPoint* pEP = wld_ep_fromObj(object);
    ASSERT_NOT_NULL(pEP, , ME, "INVALID");

    pEP->reconnectDelay = amxc_var_dyncast(uint32_t, newValue);

    SAH_TRACEZ_OUT(ME);
}

/**
 * @brief wld_endpoint_setReconnectInterval_pwf
 *
 * Parameter handler to handle Endpoint reconnect interval time parameter
 */
static void s_setReconnectInterval_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_EndPoint* pEP = wld_ep_fromObj(object);
    ASSERT_NOT_NULL(pEP, , ME, "INVALID");

    SAH_TRACEZ_INFO(ME, "%s: EndpointReconnectInterval changed", pEP->Name);

    pEP->reconnectInterval = amxc_var_dyncast(uint32_t, newValue);
    amxp_timer_set_interval(pEP->reconnectTimer, pEP->reconnectInterval * 1000);

    SAH_TRACEZ_OUT(ME);
}

static void s_setMultiAPEnable_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_EndPoint* pEP = wld_ep_fromObj(object);
    ASSERT_NOT_NULL(pEP, , ME, "INVALID");

    bool multiAPEnable = amxc_var_dyncast(bool, newValue);
    ASSERTS_NOT_EQUALS(pEP->multiAPEnable, multiAPEnable, , ME, "EQUALS");
    pEP->multiAPEnable = multiAPEnable;
    pEP->pFA->mfn_wendpoint_multiap_enable(pEP);
    wld_autoCommitMgr_notifyEpEdit(pEP);

    SAH_TRACEZ_OUT(ME);
}

static void s_setMultiAPProfile_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_EndPoint* pEP = wld_ep_fromObj(object);
    ASSERT_NOT_NULL(pEP, , ME, "INVALID");

    uint8_t multiAPProfile = amxc_var_dyncast(uint8_t, newValue);
    ASSERTS_NOT_EQUALS(pEP->multiAPProfile, multiAPProfile, , ME, "EQUALS");
    pEP->multiAPProfile = multiAPProfile;
    pEP->pFA->mfn_wendpoint_update(pEP, SET);

    SAH_TRACEZ_OUT(ME);
}

/**
 * @brief wld_endpoint_setProfileReference_pwf
 *
 * Set the Current Profile
 */
static void s_setProfileReference_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_EndPoint* pEP = wld_ep_fromObj(object);
    ASSERT_NOT_NULL(pEP, , ME, "INVALID");

    const char* newProfileRef = amxc_var_constcast(cstring_t, newValue);
    ASSERTW_FALSE(swl_str_isEmpty(newProfileRef), , ME, "profile Ref is not yet set");

    SAH_TRACEZ_INFO(ME, "%s: setProfileReference - %s", pEP->Name, newProfileRef);

    bool credentialsChanged = false;
    T_EndPointProfile* oldProfile = pEP->currentProfile;
    pEP->currentProfile = NULL;

    amxd_object_t* newProfileObj = NULL;

    if(swl_str_countChar(newProfileRef, '.') > 0) {
        newProfileObj = amxd_object_findf(amxd_dm_get_root(wld_plugin_dm), "%s", newProfileRef);
    } else {
        newProfileObj = amxd_object_findf(pEP->pBus, "Profile.%s", newProfileRef);
    }
    ASSERT_NOT_NULL(newProfileObj, , ME, "No profile found matching the profileRef [%s]", newProfileRef);


    T_EndPointProfile* newProfile = (T_EndPointProfile*) newProfileObj->priv;
    int comparison = wld_endpoint_isProfileIdentical(oldProfile, newProfile);

    // First creation of a Profile matching the current connected SSID, avoid disconnect
    bool firstCreation = false;
    char tmpSsid[128] = {0};
    if(wld_wpaSupp_ep_getSsid(pEP, tmpSsid, sizeof(tmpSsid)) >= SWL_RC_OK) {
        if((oldProfile == NULL) && (pEP->connectionStatus == EPCS_CONNECTED) && (newProfile != NULL) && swl_str_matches(tmpSsid, newProfile->SSID)) {
            firstCreation = true;
        }
    }

    if((comparison < 0) && !firstCreation) {
        SAH_TRACEZ_INFO(ME, "Profile is not identical %i", comparison);
        credentialsChanged = true;
    } else {
        SAH_TRACEZ_INFO(ME, "Profile is identical, don't disconnect");
    }

    SAH_TRACEZ_INFO(ME, "Profile reference found - Setting Current Profile for [%s]", newProfileRef);
    pEP->currentProfile = newProfile;

    if((pEP->pSSID != NULL) && (newProfile != NULL) &&
       ((newProfile->status == EPPS_ACTIVE) || (newProfile->status == EPPS_AVAILABLE))) {
        swl_str_copy(pEP->pSSID->SSID, sizeof(pEP->pSSID->SSID), newProfile->SSID);
    }
    if(credentialsChanged) {
        wld_endpoint_reconfigure(pEP);
    }

    SAH_TRACEZ_OUT(ME);
}

/**
 * The function is used to map an interface on a bridge. Some vendor deamons
 * depends on it for proper functionallity.
 */
static void s_setBridgeInterface_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_EndPoint* pEP = wld_ep_fromObj(object);
    ASSERT_NOT_NULL(pEP, , ME, "INVALID");
    const char* pstr_BridgeName = amxc_var_constcast(cstring_t, newValue);
    SAH_TRACEZ_INFO(ME, "%s: set BridgeInterface %s", pEP->Name, pstr_BridgeName);
    ASSERTS_FALSE(swl_str_matches(pEP->bridgeName, pstr_BridgeName), , ME, "EQUALS");
    swl_str_copy(pEP->bridgeName, sizeof(pEP->bridgeName), pstr_BridgeName);
    wld_endpoint_reconfigure(pEP);

    SAH_TRACEZ_OUT(ME);
}

static void s_writeStats(T_EndPoint* pEP, uint64_t call_id _UNUSED, amxc_var_t* variant, T_EndPointStats* stats) {
    amxd_object_t* object = amxd_object_get(pEP->pBus, "Stats");
    wld_endpoint_updateStats(object, stats);
    amxc_var_t map;
    amxc_var_init(&map);
    amxc_var_set_type(&map, AMXC_VAR_ID_HTABLE);
    amxd_object_get_params(object, &map, amxd_dm_access_private);
    amxc_var_move(variant, &map);
    amxc_var_clean(&map);
}

static void s_writeAssocStats(T_EndPoint* pEP) {
    amxd_object_t* object = amxd_object_get(pEP->pBus, "AssocStats");
    amxd_trans_t trans;
    ASSERT_TRANSACTION_INIT(object, &trans, , ME, "%s : trans init failure", pEP->Name);
    amxd_trans_set_uint32_t(&trans, "NrAssocAttempts", pEP->assocStats.nrAssocAttempts);
    amxd_trans_set_uint32_t(&trans, "NrAssocAttempsSinceDc", pEP->assocStats.nrAssocAttemptsSinceDc);
    amxd_trans_set_uint32_t(&trans, "NrAssociations", pEP->assocStats.nrAssociations);
    amxd_trans_set_uint32_t(&trans, "AssocTime", pEP->assocStats.lastAssocTime);
    amxd_trans_set_uint32_t(&trans, "DisassocTime", pEP->assocStats.lastDisassocTime);
    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, , ME, "%s : trans apply failure", pEP->Name);
}

void wld_endpoint_writeStats(T_EndPoint* pEP, T_EndPointStats* stats, bool success) {
    ASSERT_NOT_NULL(pEP, , ME, "NULL");
    ASSERT_TRUE(pEP->statsCall.running, , ME, "%s no fcall", pEP->Name);
    pEP->statsCall.running = false;

    ASSERT_NOT_EQUALS(pEP->statsCall.call_id, 0, , ME, "%s no fcall", pEP->Name);

    amxc_var_t values;
    amxc_var_init(&values),
    amxc_var_set_type(&values, AMXC_VAR_ID_LIST);
    if(success) {
        s_writeStats(pEP, pEP->statsCall.call_id, &values, stats);
    }
    amxd_function_deferred_done(pEP->statsCall.call_id, success ? amxd_status_ok : amxd_status_unknown_error, NULL, success ? &values : NULL);
    amxc_var_clean(&values);
    pEP->statsCall.call_id = 0;
}

static void s_copyEpStats(T_Stats* pStats, T_EndPointStats* pEpStats) {
    ASSERTS_NOT_NULL(pStats, , ME, "NULL");
    ASSERTS_NOT_NULL(pEpStats, , ME, "NULL");
    pStats->BytesSent = pEpStats->txbyte;
    pStats->BytesReceived = pEpStats->rxbyte;
    pStats->PacketsSent = pEpStats->txPackets;
    pStats->PacketsReceived = pEpStats->rxPackets;
    pStats->RetransCount = pEpStats->Retransmissions;
    pStats->RetryCount = pEpStats->txRetries + pEpStats->rxRetries;
    pStats->noise = pEpStats->noise;
}

static void s_getOUIValue(amxc_string_t* output, swl_oui_list_t* vendorOui) {
    ASSERTI_TRUE(vendorOui->count != 0, , ME, "No OUI Vendor");
    char buffer[SWL_OUI_STR_LEN * WLD_MAX_OUI_NUM];
    memset(buffer, 0, sizeof(buffer));
    swl_typeOui_arrayToChar(buffer, SWL_OUI_STR_LEN * WLD_MAX_OUI_NUM, &vendorOui->oui[0], vendorOui->count);
    amxc_string_set(output, buffer);
}

void wld_endpoint_syncCapabilities(amxd_object_t* obj, wld_assocDev_capabilities_t* caps) {
    ASSERT_NOT_NULL(obj, , ME, "NULL");
    ASSERT_NOT_NULL(caps, , ME, "NULL");

    char mcsList[100];
    memset(mcsList, 0, sizeof(mcsList));
    swl_conv_uint8ArrayToChar(mcsList, sizeof(mcsList), caps->supportedMCS.mcs, caps->supportedMCS.mcsNbr);
    SWLA_OBJECT_SET_PARAM_CSTRING(obj, "SupportedMCS", mcsList);

    amxc_string_t TBufStr;
    amxc_string_init(&TBufStr, 0);
    s_getOUIValue(&TBufStr, &caps->vendorOUI);
    SWLA_OBJECT_SET_PARAM_CSTRING(obj, "VendorOUI", amxc_string_get(&TBufStr, 0));
    SWLA_OBJECT_SET_PARAM_CSTRING(obj, "SecurityModeEnabled", swl_security_apModeToString(caps->currentSecurity, SWL_SECURITY_APMODEFMT_LEGACY));
    SWLA_OBJECT_SET_PARAM_CSTRING(obj, "EncryptionMode", cstr_AP_EncryptionMode[caps->encryptMode]);

    SWLA_OBJECT_SET_PARAM_CSTRING(obj, "LinkBandwidth", swl_bandwidth_unknown_str[caps->linkBandwidth]);
    char buffer[256];
    swl_conv_maskToChar(buffer, sizeof(buffer), caps->htCapabilities, swl_staCapHt_str, SWL_STACAP_HT_MAX);
    SWLA_OBJECT_SET_PARAM_CSTRING(obj, "HtCapabilities", buffer);
    swl_conv_maskToChar(buffer, sizeof(buffer), caps->vhtCapabilities, swl_staCapVht_str, SWL_STACAP_VHT_MAX);
    SWLA_OBJECT_SET_PARAM_CSTRING(obj, "VhtCapabilities", buffer);
    swl_conv_maskToChar(buffer, sizeof(buffer), caps->heCapabilities, swl_staCapHe_str, SWL_STACAP_HE_MAX);
    SWLA_OBJECT_SET_PARAM_CSTRING(obj, "HeCapabilities", buffer);
    swl_conv_maskToChar(buffer, sizeof(buffer), caps->ehtCapabilities, swl_staCapEht_str, SWL_STACAP_EHT_MAX);
    SWLA_OBJECT_SET_PARAM_CSTRING(obj, "EhtCapabilities", buffer);

    char frequencyCapabilitiesStr[128] = {0};
    swl_conv_maskToChar(frequencyCapabilitiesStr, sizeof(frequencyCapabilitiesStr), caps->freqCapabilities, swl_freqBandExt_unknown_str, SWL_FREQ_BAND_EXT_MAX);
    SWLA_OBJECT_SET_PARAM_CSTRING(obj, "FrequencyCapabilities", frequencyCapabilitiesStr);
    amxc_string_clean(&TBufStr);
}

amxd_status_t wld_endpoint_stats2Obj(amxd_object_t* obj, T_EndPointStats* stats) {
    ASSERT_NOT_NULL(obj, amxd_status_unknown_error, ME, "NULL");
    ASSERT_NOT_NULL(stats, amxd_status_unknown_error, ME, "NULL");

    SWLA_OBJECT_SET_PARAM_UINT32(obj, "LastDataDownlinkRate", stats->LastDataDownlinkRate);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "LastDataUplinkRate", stats->LastDataUplinkRate);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "Retransmissions", stats->Retransmissions);
    SWLA_OBJECT_SET_PARAM_INT32(obj, "SignalStrength", stats->SignalStrength);
    SWLA_OBJECT_SET_PARAM_INT32(obj, "SignalNoiseRatio", stats->SignalNoiseRatio);
    SWLA_OBJECT_SET_PARAM_INT32(obj, "Noise", stats->noise);
    SWLA_OBJECT_SET_PARAM_INT32(obj, "RSSI", stats->RSSI);
    SWLA_OBJECT_SET_PARAM_UINT64(obj, "TxBytes", stats->txbyte);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "TxPacketCount", stats->txPackets);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "Tx_Retransmissions", stats->txRetries);
    SWLA_OBJECT_SET_PARAM_UINT64(obj, "RxBytes", stats->rxbyte);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "RxPacketCount", stats->rxPackets);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "Rx_Retransmissions", stats->rxRetries);

    SWLA_OBJECT_SET_PARAM_CSTRING(obj, "OperatingStandard", swl_radStd_unknown_str[stats->operatingStandard]);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "MaxRxSpatialStreamsSupported", stats->maxRxStream);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "MaxTxSpatialStreamsSupported", stats->maxTxStream);

    wld_endpoint_syncCapabilities(obj, &stats->assocCaps);


    return amxd_status_ok;
}
void wld_endpoint_resetStats(T_EndPoint* pEP) {
    ASSERTS_NOT_NULL(pEP, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: reset Endpoint Stats", pEP->alias);
    memset(&pEP->stats, 0, sizeof(T_EndPointStats));
}

static swl_rc_ne s_getEndpointStats(amxd_object_t* object) {
    T_EndPoint* pEP = wld_ep_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pEP, false, ME, "pEP is NULL, no device present");

    T_EndPointStats* epStats = &pEP->stats;
    if(pEP->pFA->mfn_wendpoint_stats(pEP, epStats) >= SWL_RC_OK) {
        /* Update statistics */
        s_copyEpStats(&pEP->pSSID->stats, epStats);
        wld_endpoint_stats2Obj(object, epStats);
    }

    return true;
}

amxd_status_t _wld_endpoint_getStats_orf(amxd_object_t* const object,
                                         amxd_param_t* const param,
                                         amxd_action_t reason,
                                         const amxc_var_t* const args,
                                         amxc_var_t* const action_retval,
                                         void* priv) {

    amxd_status_t status = amxd_status_ok;
    if((reason != action_object_read) && (reason != action_param_read)) {
        status = amxd_status_function_not_implemented;
        return status;
    }
    ASSERT_NOT_NULL(object, amxd_status_ok, ME, " obj is NULL");
    T_EndPoint* pEP = wld_ep_fromObj(amxd_object_get_parent(object));
    ASSERTI_NOT_NULL(pEP, amxd_status_ok, ME, "EndPoint not present");


    if(!pEP || !debugIsEpPointer(pEP)) {
        SAH_TRACEZ_INFO(ME, "EndPoint not present !");
        return amxd_status_unknown_error;
    }

    status = swla_dm_procObjActionRead(object, param, reason, args, action_retval, priv, &pEP->onActionReadCtx, s_getEndpointStats);
    return status;
}

/*
   static void s_stats_cancel_handler(function_call_t* fcall, void* userdata _UNUSED) {
    T_EndPoint* pEP = fcall_userData(fcall);
    ASSERT_TRUE(debugIsEpPointer(pEP), , ME, "INVALID");
    SAH_TRACEZ_INFO(ME, "%s: stats call cancelled", pEP->Name);
    pEP->statsCall.fcall = NULL;
   }
 */

/**
 * @brief getStats
 *
 * Collect EndPoint statistics and update Stats object
 * @return true on success, false otherwise
 */
amxd_status_t _getStats(amxd_object_t* object,
                        amxd_function_t* func _UNUSED,
                        amxc_var_t* args _UNUSED,
                        amxc_var_t* retval) {
    T_EndPoint* pEP = object->priv;
    ASSERT_NOT_NULL(pEP, amxd_status_unknown_error, ME, "NULL");
    ASSERT_FALSE(pEP->statsCall.running, amxd_status_unknown_error, ME, "Stats call running");

    uint64_t call_id = amxc_var_constcast(uint64_t, retval);

    swl_rc_ne retVal = pEP->pFA->mfn_wendpoint_stats(pEP, &pEP->stats);

    if(retVal == SWL_RC_OK) {
        s_writeStats(pEP, call_id, retval, &pEP->stats);
        return amxd_status_ok;
    } else if(retVal == SWL_RC_CONTINUE) {
        pEP->statsCall.running = true;
        pEP->statsCall.call_id = call_id;
        SAH_TRACEZ_OUT(ME);
        return amxd_status_deferred;
    } else {
        SAH_TRACEZ_ERROR(ME, "%s : failed stats", pEP->alias);
        return amxd_status_unknown_error;
    }
}

/**
 * @brief setEndpointCurrentProfile
 *
 * Set the current profile when a matching profile reference is set
 * Used when the profile is just created
 *
 * @param endpointObject Endpoint object
 * @param Profile new endpoint profile
 */
void wld_endpoint_setCurrentProfile(amxd_object_t* endpointObject, T_EndPointProfile* Profile) {
    ASSERT_NOT_NULL(endpointObject, , ME, "NULL");
    ASSERT_NOT_NULL(Profile, , ME, "NULL");

    T_EndPoint* EndPoint = (T_EndPoint*) endpointObject->priv;
    ASSERT_NOT_NULL(EndPoint, , ME, "NULL");

    char* profileRefStr = amxd_object_get_cstring_t(endpointObject, "ProfileReference", NULL);
    amxd_object_t* profileRefObj = swla_object_getReferenceObject(endpointObject, profileRefStr);
    if(profileRefObj == NULL) {
        SAH_TRACEZ_INFO(ME, "Profile Reference is not yet set - not setting currentProfile");
        free(profileRefStr);
        return;
    }
    const char* profileName = amxd_object_get_name(Profile->pBus, AMXD_OBJECT_NAMED);
    const char* profileRefName = amxd_object_get_name(profileRefObj, AMXD_OBJECT_NAMED);

    if(profileRefObj == Profile->pBus) {
        SAH_TRACEZ_NOTICE(ME, "Profile Instance name [%s] matched : setting as currentProfile",
                          profileName);
        EndPoint->currentProfile = Profile;
    } else {
        SAH_TRACEZ_NOTICE(ME, "Profile Instance name [%s] does not match the profileRefStr [%s] - not setting currentProfile",
                          profileName, profileRefName);
    }

    free(profileRefStr);
}

/**
 * @brief setEndPointDefaults
 *
 * Set some default settings for the Endpoint
 * Used when the endpoint is just created
 *
 * @param EndPoint pointer to the struct
 * @param endpointname name of the endpoint
 */
static void s_setDefaults(T_EndPoint* pEP, const char* endpointname) {
    ASSERT_NOT_NULL(pEP, , ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: Setting Endpoint Defaults", endpointname);


    swl_str_copy(pEP->alias, sizeof(pEP->alias), endpointname);

    pEP->status = APSTI_DISABLED;
    pEP->connectionStatus = EPCS_DISABLED;
    pEP->error = EPE_NONE;
    pEP->enable = APSTI_DISABLED;

    pEP->reconnectDelay = 15;
    pEP->reconnectInterval = 15;

    pEP->WPS_Enable = true;

    pEP->toggleBssOnReconnect = true;

    memset(&pEP->stats, 0, sizeof(T_EndPointStats));

    pEP->WPS_ConfigMethodsSupported = (M_WPS_CFG_MTHD_LABEL | M_WPS_CFG_MTHD_DISPLAY_ALL | M_WPS_CFG_MTHD_PBC_ALL | M_WPS_CFG_MTHD_PIN);
    pEP->WPS_ConfigMethodsEnabled = (M_WPS_CFG_MTHD_PBC | M_WPS_CFG_MTHD_DISPLAY);
    pEP->secModesSupported = M_SWL_SECURITY_APMODE_NONE;
    pEP->secModesSupported |= (pEP->pFA->mfn_misc_has_support(pEP->pRadio, NULL, "WEP", 0)) ?
        (M_SWL_SECURITY_APMODE_WEP64 | M_SWL_SECURITY_APMODE_WEP128 | M_SWL_SECURITY_APMODE_WEP128IV) : 0;
    pEP->secModesSupported |= (pEP->pFA->mfn_misc_has_support(pEP->pRadio, NULL, "AES", 0)) ?
        (M_SWL_SECURITY_APMODE_WPA_P | M_SWL_SECURITY_APMODE_WPA2_P | M_SWL_SECURITY_APMODE_WPA_WPA2_P) : 0;
    if(pEP->pFA->mfn_misc_has_support(pEP->pRadio, NULL, "SAE", 0)) {
        pEP->secModesSupported |= M_SWL_SECURITY_APMODE_WPA3_P;
        if(pEP->secModesSupported & M_SWL_SECURITY_APMODE_WPA2_P) {
            pEP->secModesSupported |= M_SWL_SECURITY_APMODE_WPA2_WPA3_P;
        }
    }
    if(pEP->pFA->mfn_misc_has_support(pEP->pRadio, NULL, "OWE", 0)) {
        pEP->secModesSupported |= M_SWL_SECURITY_APMODE_OWE;
    }
}

static void s_sendChangeEvent(T_EndPoint* pEP, wld_ep_changeEvent_e changeType, void* data) {
    wld_ep_changeEvent_t change = {
        .ep = pEP,
        .changeType = changeType,
        .data = data
    };
    wld_event_trigger_callback(gWld_queue_ep_onChangeEvent, &change);
}

/**
 * @brief s_deinitEP
 *
 * Clear out resources of the endpoint internal context
 *
 * @param endpoint context
 */
static void s_deinitEP(T_EndPoint* pEP) {
    ASSERTS_NOT_NULL(pEP, , ME, "NULL");
    SAH_TRACEZ_IN(ME);
    //assume disable ep
    pEP->enable = false;
    T_Radio* pR = pEP->pRadio;
    T_SSID* pSSID = pEP->pSSID;
    if(pR) {
        if(pEP->index > 0) {
            pR->pFA->mfn_wendpoint_disconnect(pEP);
            wld_mld_unregisterLink(pSSID);
            pR->pFA->mfn_wendpoint_enable(pEP, false);
            wld_rad_doRadioCommit(pR);
            wld_endpoint_reconfigure(pEP);
        }
        pR->pFA->mfn_wendpoint_destroy_hook(pEP);
        if(pEP->index > 0) {
            /* Try to delete the requested interface by calling the HW function */
            pR->pFA->mfn_wrad_delendpointif(pR, pEP->Name);
        }
        s_sendChangeEvent(pEP, WLD_EP_CHANGE_EVENT_DESTROY, NULL);
        pEP->Name[0] = 0;
        pEP->index = 0;
        /* Take EP also out the Radio */
        amxc_llist_it_take(&pEP->it);
        pEP->pRadio = NULL;
        pEP->pFA = NULL;
    }
    if(pSSID != NULL) {
        memset(pSSID->MACAddress, 0, ETHER_ADDR_LEN);
        pSSID->ENDP_HOOK = NULL;
        if(pR) {
            pR->pFA->mfn_sync_ssid(pSSID->pBus, pSSID, SET);
        }
        pEP->pSSID = NULL;
    }
    wld_tinyRoam_cleanup(pEP);
    amxp_timer_delete(&pEP->reconnectTimer);
    pEP->reconnectTimer = NULL;
    SAH_TRACEZ_OUT(ME);
}

/**
 * @brief s_initEp
 *
 * reserve and initialize resources for endpoint internal context
 *
 * @param endpoint context
 */
static bool s_initEp(T_EndPoint* pEP, T_Radio* pRad, const char* epName) {
    ASSERTS_NOT_NULL(pEP, false, ME, "NULL");
    ASSERTS_NOT_NULL(pRad, false, ME, "NULL");

    SAH_TRACEZ_IN(ME);
    pEP->debug = ENDP_POINTER;
    pEP->pRadio = pRad;
    pEP->pFA = pRad->pFA;
    s_setDefaults(pEP, epName);

    amxc_llist_append(&pRad->llEndPoints, &pEP->it);
    wld_endpoint_create_reconnect_timer(pEP);
    wld_tinyRoam_init(pEP);

    s_sendChangeEvent(pEP, WLD_EP_CHANGE_EVENT_CREATE, NULL);

    SAH_TRACEZ_OUT(ME);
    return true;
}

/**
 * @brief syncData_Object2EndPointProfile
 *
 * Sync Datamodel Endpoint Profile object parameters to
 * the T_EndpointProfile struct
 *
 * @param object Endpoint profile object instance
 * @return true if a parameter of the profile has changed
 * that should trigger a change in the connection. false
 * otherwise.
 */
bool syncData_Object2EndPointProfile(amxd_object_t* object) {
    T_EndPointProfile* pProfile = NULL;
    bool changed = false;
    uint8_t BSSID[6] = {'\0'};
    uint8_t tmp_priority = 0;

    char rollbackBuffer[256] = {'\0'};

    amxd_object_t* secObj = amxd_object_findf(object, "Security");

    pProfile = (T_EndPointProfile*) object->priv;
    if(!pProfile) {
        SAH_TRACEZ_ERROR(ME, "Failed to find Profile structure");
        return false;
    }

    bool tmpBool = amxd_object_get_bool(object, "Enable", NULL);
    if(tmpBool != pProfile->enable) {
        changed = true;
        pProfile->enable = tmpBool;
    }

    tmp_priority = amxd_object_get_uint8_t(object, "Priority", NULL);
    if(tmp_priority != pProfile->priority) {
        changed |= 1;
        pProfile->priority = tmp_priority;
    }

    char* ssid = amxd_object_get_cstring_t(object, "SSID", NULL);
    if(!swl_str_matches(ssid, pProfile->SSID)) {
        changed |= 1;
        swl_str_copy(pProfile->SSID, sizeof(pProfile->SSID), ssid);
    }
    free(ssid);

    char* bssid = amxd_object_get_cstring_t(object, "ForceBSSID", NULL);
    if((bssid == 0) || (bssid[0] == 0)
       || !wldu_convStr2Mac((unsigned char*) BSSID, sizeof(BSSID), bssid, strlen(bssid))) {
        memcpy(pProfile->BSSID, wld_ether_null, sizeof(pProfile->BSSID));
    } else {
        changed |= !!memcmp(pProfile->BSSID, BSSID, sizeof(pProfile->BSSID));
        memcpy(pProfile->BSSID, BSSID, sizeof(pProfile->BSSID));
    }
    free(bssid);

    //These 2 parameters have no effect on the actual connection and should not cause
    //a connection change.
    char* alias = amxd_object_get_cstring_t(object, "Alias", NULL);
    swl_str_copy(pProfile->alias,
                 sizeof(pProfile->alias),
                 alias);
    free(alias);
    char* location = amxd_object_get_cstring_t(object, "Location", NULL);
    swl_str_copy(pProfile->location,
                 sizeof(pProfile->location),
                 location);
    free(location);

    char* mode = amxd_object_get_cstring_t(secObj, "ModeEnabled", NULL);
    if(!strncmp(mode, rollbackBuffer, 256)) {
        rollbackBuffer[0] = '\0';
    } else {
        swl_str_copy(rollbackBuffer, sizeof(rollbackBuffer), mode);
    }
    free(mode);

    swl_security_apMode_e secMode = swl_security_apModeFromString(rollbackBuffer);
    if(secMode != pProfile->secModeEnabled) {
        changed = true;
        pProfile->secModeEnabled = secMode;
    }

    char* wepKey = amxd_object_get_cstring_t(secObj, "WEPKey", NULL);
    if(swl_security_isApModeWEP(pProfile->secModeEnabled)) {
        if(!isValidWEPKey(wepKey)) {
            SAH_TRACEZ_WARNING(ME, "Sync Failure - WEPKey is not in a valid format");
        } else {
            changed |= 1;
            swl_str_copy(pProfile->WEPKey, sizeof(pProfile->WEPKey), wepKey);
        }
    }
    free(wepKey);

    char* pskKey = amxd_object_get_cstring_t(secObj, "PreSharedKey", NULL);
    if(swl_security_isApModeWPAPersonal(pProfile->secModeEnabled) &&
       strncmp(pskKey, pProfile->preSharedKey, sizeof(pProfile->preSharedKey))) {
        if(!isValidPSKKey(pskKey)) {
            SAH_TRACEZ_WARNING(ME, "Sync Failure - PreSharedKey is not in a valid format");
        } else {
            changed |= 1;
            swl_str_copy(pProfile->preSharedKey, sizeof(pProfile->preSharedKey), pskKey);
        }
    }
    free(pskKey);

    char* keyPassPhrase = amxd_object_get_cstring_t(secObj, "KeyPassPhrase", NULL);
    if(swl_security_isApModeWPAPersonal(pProfile->secModeEnabled) &&
       strncmp(keyPassPhrase, pProfile->keyPassPhrase, sizeof(pProfile->keyPassPhrase))) {
        if(!isValidAESKey(keyPassPhrase, PSK_KEY_SIZE_LEN - 1)) {
            SAH_TRACEZ_WARNING(ME, "Sync Failure - KeyPassPhrase is not in a valid format");
        } else {
            changed |= !(wldu_key_matches(pProfile->SSID, pProfile->keyPassPhrase, keyPassPhrase));
            swl_str_copy(pProfile->keyPassPhrase, sizeof(pProfile->keyPassPhrase), keyPassPhrase);
        }
    }
    free(keyPassPhrase);

    char* saePassphrase = amxd_object_get_cstring_t(secObj, "SAEPassphrase", NULL);
    if(swl_security_isApModeWPA3Personal(pProfile->secModeEnabled) &&
       strncmp(saePassphrase, pProfile->saePassphrase, sizeof(pProfile->saePassphrase))) {
        if(!isValidAESKey(saePassphrase, SAE_KEY_SIZE_LEN)) {
            SAH_TRACEZ_WARNING(ME, "Sync Failure - SAEPassphrase is not in a valid format");
        } else {
            changed |= 1;
            swl_str_copy(pProfile->saePassphrase, sizeof(pProfile->saePassphrase), saePassphrase);
        }
    }
    free(saePassphrase);

    char* mfp = amxd_object_get_cstring_t(secObj, "MFPConfig", NULL);
    const char* curMfpMode = swl_security_mfpModeToString(pProfile->mfpConfig);
    if(!swl_str_nmatches(mfp, curMfpMode, swl_str_len(curMfpMode))) {
        swl_security_mfpMode_e idx = swl_security_mfpModeFromString(mfp);
        if(idx != pProfile->mfpConfig) {
            pProfile->mfpConfig = idx;
            changed |= 1;
        }
    }
    free(mfp);

    if(changed) {
        s_setProfileStatus(pProfile);
    }

    return changed;
}

/**
 * @brief writeEndpointProfile
 *
 * Write handler on the Endpoint Profile instance parameters
 *
 * Sync object data to the EndPontProfile structure
 *
 */
void wld_endpoint_setProfile_ocf(void* priv _UNUSED, amxd_object_t* object, const amxc_var_t* const newParamValues _UNUSED) {
    SAH_TRACEZ_IN(ME);

    T_EndPointProfile* pProfile = (T_EndPointProfile*) object->priv;
    ASSERT_NOT_NULL(pProfile, , ME, "Profile is not yet set");
    T_EndPoint* pEP = wld_ep_fromObj(amxd_object_get_parent(amxd_object_get_parent(object)));
    ASSERT_NOT_NULL(pEP, , ME, "NULL");
    ASSERT_EQUALS(pProfile->endpoint, pEP, , ME, "Failed to find Endpoint structure");

    bool profileChanged = syncData_Object2EndPointProfile(object);

    ASSERTI_EQUALS(pEP->currentProfile, pProfile, , ME, "Changed profile is not currently selected : done");
    ASSERTI_TRUE(profileChanged, , ME, "Profile enabled field and profile parameters not changed : done");

    wld_endpoint_reconfigure(pEP);

    SAH_TRACEZ_OUT(ME);
}

/**
 * @brief wld_endpoint_setProfileSecurity_pwf
 *
 * Write handler on the Endpoint Profile Security parameters
 * Sync object data to the EndPontProfile structure
 */
void wld_endpoint_setProfileSecurity_ocf(void* priv _UNUSED, amxd_object_t* object, const amxc_var_t* const newParamValues _UNUSED) {
    SAH_TRACEZ_IN(ME);

    amxd_object_t* profileObject = amxd_object_get_parent(object);
    ASSERT_NOT_NULL(profileObject, , ME, "invalid object");
    T_EndPointProfile* pProfile = (T_EndPointProfile*) profileObject->priv;
    ASSERT_NOT_NULL(pProfile, , ME, "Profile is not yet set");
    T_EndPoint* pEP = wld_ep_fromObj(amxd_object_get_parent(amxd_object_get_parent(profileObject)));
    ASSERT_NOT_NULL(pEP, , ME, "NULL");

    bool profileChanged = syncData_Object2EndPointProfile(profileObject);

    ASSERTI_EQUALS(pEP->currentProfile, pProfile, , ME, "Changed profile is not currently selected : done");
    ASSERTI_TRUE(profileChanged, , ME, "Profile enabled field and profile parameters not changed : done");

    wld_endpoint_reconfigure(pEP);

    SAH_TRACEZ_OUT(ME);
}

/**
 * @brief wld_endpoint_setWPSEnable_pwf
 *
 * Write handler on the Endpoint WPS "Enable" parameter
 */
static void s_setWpsEnable_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_EndPoint* pEP = wld_ep_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pEP, , ME, "INVALID");

    bool enable = amxc_var_dyncast(bool, newValue);

    SAH_TRACEZ_INFO(ME, "%s: EndpointWPSEnable changed => (%d)", pEP->Name, enable);
    pEP->WPS_Enable = enable;
    T_Radio* pR = pEP->pRadio;
    ASSERT_NOT_NULL(pR, , ME, "%s: no mapped Radio", pEP->Name);

    /* Ignore and give warning when not properly set */
    if(pR->isSTA && pR->isWPSEnrol) {
        pR->fsmRad.FSM_SyncAll = TRUE;
    } else {
        SAH_TRACEZ_INFO(ME, "ignore WPS change - Radio STA mode (%d) and Enrollee (%d) not correct set",
                        pR->isSTA,
                        pR->isWPSEnrol);
    }

    SAH_TRACEZ_OUT(ME);
}


/**
 * @brief wld_endpoint_setWPSConfigMethodsEnabled_pwf
 *
 * Write handler on the Endpoint WPS "ConfigMethodsEnabled" parameter
 */
static void s_setWPSConfigMethodsEnabled_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_EndPoint* pEP = wld_ep_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pEP, , ME, "INVALID");

    const char* StrParm = amxc_var_constcast(cstring_t, newValue);
    SAH_TRACEZ_INFO(ME, "%s: set WPS ConfigMethodsEnabled %s", pEP->Name, StrParm);
    wld_wps_cfgMethod_m wps_cfgm;
    wld_wps_ConfigMethods_string_to_mask(&wps_cfgm, StrParm, ',');
    pEP->WPS_ConfigMethodsEnabled = wps_cfgm;

    T_Radio* pR = pEP->pRadio;
    ASSERT_NOT_NULL(pR, , ME, "%s: no mapped Radio", pEP->Name);

    /* Ignore and give warning when not properly set */
    if(pR->isSTA && pR->isWPSEnrol) {
        pR->fsmRad.FSM_SyncAll = TRUE;
    } else {
        SAH_TRACEZ_INFO(ME, "ignore WPS change - Radio STA mode (%d) and Enrollee (%d) not correct set",
                        pR->isSTA,
                        pR->isWPSEnrol);
    }

    SAH_TRACEZ_OUT(ME);
}

SWLA_DM_HDLRS(sEpWpsDmHdlrs,
              ARR(SWLA_DM_PARAM_HDLR("Enable", s_setWpsEnable_pwf),
                  SWLA_DM_PARAM_HDLR("ConfigMethodsEnabled", s_setWPSConfigMethodsEnabled_pwf),
                  ),
              );

void _wld_ep_setWpsConf_ocf(const char* const sig_name,
                            const amxc_var_t* const data,
                            void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sEpWpsDmHdlrs, sig_name, data, priv);
}


/**
 * Check whether endpoint is ready for enable.
 * This means:
 * * Endpoint enabled
 * * Radio enabled
 * * Profile present
 * * Profile Enabled
 * * Profile Valid Conf
 */
bool wld_endpoint_isReady(T_EndPoint* pEP) {
    ASSERT_NOT_NULL(pEP, false, ME, "NULL");
    uint32_t mask = 0;
    T_Radio* pRad = pEP->pRadio;
    W_SWL_BIT_WRITE(mask, 0, !pEP->enable);
    W_SWL_BIT_WRITE(mask, 1, !pRad->enable);
    W_SWL_BIT_WRITE(mask, 2, pEP->currentProfile == NULL);
    if(pEP->currentProfile != NULL) {
        W_SWL_BIT_WRITE(mask, 3, !pEP->currentProfile->enable);
        W_SWL_BIT_WRITE(mask, 4, !wld_epProfile_hasValidConf(pEP->currentProfile));
    }
    SAH_TRACEZ_INFO(ME, "%s: check ready 0x%2x", pEP->Name, mask);

    return mask == 0;
}

/**
 * @brief wld_endpoint_setEnable_pwf
 *
 * Write handler on the Endpoint "Enable" parameter
 */
static void s_setEnable_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param, const amxc_var_t* const newValue _UNUSED) {
    SAH_TRACEZ_IN(ME);

    T_EndPoint* pEP = wld_ep_fromObj(object);
    ASSERT_NOT_NULL(pEP, , ME, "Endpoint is not yet set");

    amxc_var_t myVar;
    amxc_var_init(&myVar);
    amxd_status_t status = amxd_param_get_value(param, &myVar);
    ASSERT_EQUALS(status, amxd_status_ok, , ME, "%s: fail to receive latest enable value", pEP->alias);
    bool newEnable = amxc_var_dyncast(bool, &myVar);
    amxc_var_clean(&myVar);

    SAH_TRACEZ_INFO(ME, "%s: set enable %d -> %d", pEP->alias, pEP->enable, newEnable);

    ASSERTI_NOT_EQUALS(newEnable, pEP->enable, , ME, "%s: set to same enable %d", pEP->alias, newEnable);


    T_Radio* pRad = pEP->pRadio;
    ASSERT_NOT_NULL(pRad, , ME, "Failed to get the T_Radio struct from Endpoint [%s]", pEP->alias);

    /* Sync datamodel to internal endpoint struct */
    if(newEnable) {
        syncData_OBJ2EndPoint(object);
    }
    pEP->enable = newEnable;

    /* set enable flag */
    pRad->pFA->mfn_wendpoint_enable(pEP, newEnable);

    wld_endpoint_reconfigure(pEP);

    /* when idle : kick the FSM state machine to handle the new state */
    wld_autoCommitMgr_notifyEpEdit(pEP);
    wld_ssid_syncEnable(pEP->pSSID, false);

    SAH_TRACEZ_OUT(ME);
}

/**
 * @brief syncData_OBJ2EndPoint
 *
 * Read the Endpoint settings from the datamodel
 * and set them to the T_EndPoint struct
 *
 * @param object Endpoint object instance
 * @return
 *      - true  : on success
 *      - false : when a system error occured or an invalid configuration is set
 */
bool syncData_OBJ2EndPoint(amxd_object_t* object) {
    bool retval = true;

    if(!object || (amxd_object_get_type(object) != amxd_object_instance)) {
        SAH_TRACEZ_ERROR(ME, "Bad usage of function");
        return false;
    }

    T_EndPoint* pEP = (T_EndPoint*) object->priv;
    if(!pEP) {
        SAH_TRACEZ_ERROR(ME, "Failed to get the T_EndPoint struct");
        return false;
    }

    SAH_TRACEZ_INFO(ME, "Synchonize Endpoint Object");

    amxc_var_t value;
    amxc_var_init(&value);
    amxd_param_get_value(amxd_object_get_param_def(object, "Enable"), &value);
    pEP->enable = amxc_var_get_int32_t(&value);
    amxc_var_clean(&value);
    SAH_TRACEZ_INFO(ME, "Endpoint.enable=%d", pEP->enable);

    char* alias = amxd_object_get_cstring_t(object, "Alias", NULL);
    if(!swl_str_isEmpty(alias)) {
        swl_str_copy(pEP->alias, sizeof(pEP->alias), alias);
    }
    free(alias);

    /* WPS */
    amxd_object_t* const wpsobject = amxd_object_get(object, "WPS");
    if(!wpsobject) {
        SAH_TRACEZ_ERROR(ME, "WPS object not found");
        return false;
    }
    wpsobject->priv = &pEP->wpsSessionInfo;

    pEP->WPS_Enable = amxd_object_get_bool(wpsobject, "Enable", NULL);
    SAH_TRACEZ_INFO(ME, "Endpoint.WPS.enable = %d", pEP->WPS_Enable);

    char* WPS_ConfigMethodsEnabled = amxd_object_get_cstring_t(wpsobject, "ConfigMethodsEnabled", NULL);
    if(!wld_wps_ConfigMethods_string_to_mask((uint32_t*) &pEP->WPS_ConfigMethodsEnabled, WPS_ConfigMethodsEnabled, ',')) {
        SAH_TRACEZ_ERROR(ME, "Invalid WPS ConfigMethodsEnabled: '%s'", WPS_ConfigMethodsEnabled);
        retval = false;
    }
    free(WPS_ConfigMethodsEnabled);

    return retval;
}

/**
 * @brief syncData_EndPoint2OBJ
 *
 * Read out the internal T_EndPoint struct
 * and set it to the datamodel
 *
 * Note : NO COMMIT
 *
 * @param EndPoint T_EndPoint struct
 */
void syncData_EndPoint2OBJ(T_EndPoint* pEP) {
    char TBuf[256];

    if(!pEP) {
        SAH_TRACEZ_ERROR(ME, "Bad usage of function : EndPoint=NULL");
        return;
    }

    amxd_object_t* object = pEP->pBus;
    if(!object) {
        SAH_TRACEZ_ERROR(ME, "Endpoint datamodel object pointer is missing");
        return;
    }

    SAH_TRACEZ_INFO(ME, "%s: Syncing Endpoint to Datamodel", pEP->Name);

    amxd_trans_t trans;
    ASSERT_TRANSACTION_INIT(object, &trans, , ME, "%s : trans init failure", pEP->Name);

    amxd_trans_set_cstring_t(&trans, "Status", cstr_EndPoint_status[pEP->status]);
    amxd_trans_set_cstring_t(&trans, "ConnectionStatus",
                             cstr_EndPoint_connectionStatus[pEP->connectionStatus]);
    amxd_trans_set_cstring_t(&trans, "LastError", cstr_EndPoint_lastError[pEP->error]);
    amxd_trans_set_cstring_t(&trans, "Alias", pEP->alias);
    amxd_trans_set_uint32_t(&trans, "Index", pEP->index);
    amxd_trans_set_cstring_t(&trans, "IntfName", pEP->Name);

    wld_util_initCustomAlias(&trans, object);

    if(pEP->currentProfile) {
        char* curProfileStr = amxd_object_get_cstring_t(pEP->pBus, "ProfileReference", NULL);
        if((swl_str_isEmpty(curProfileStr) || (swl_str_countChar(curProfileStr, '.') > 0))) {
            char* profileRef = amxd_object_get_path(pEP->currentProfile->pBus, AMXD_OBJECT_INDEXED);
            amxd_trans_set_cstring_t(&trans, "ProfileReference", profileRef);
            free(profileRef);
        }
        free(curProfileStr);
    } else {
        uint32_t loadProfs = amxc_llist_size(&pEP->llProfiles);
        uint32_t savedProfs = amxd_object_get_instance_count(amxd_object_get(pEP->pBus, "Profile"));
        if(loadProfs != savedProfs) {
            SAH_TRACEZ_WARNING(ME, "%s: loaded profiles %d != saved profiles %d => skip resetting profileReference", pEP->Name, loadProfs, savedProfs);
        } else {
            SAH_TRACEZ_WARNING(ME, "%s: resetting profileReference", pEP->Name);
            amxd_trans_set_cstring_t(&trans, "ProfileReference", "");
        }
    }

    TBuf[0] = 0;
    if(pEP->pSSID != NULL) {
        T_Radio* pRad = pEP->pSSID->RADIO_PARENT;
        if((pRad != NULL) && (pRad->pBus != NULL)) {
            amxd_trans_select_object(&trans, pEP->pSSID->pBus);
            char* currRadRef = amxd_object_get_cstring_t(pEP->pSSID->pBus, "LowerLayers", NULL);
            wld_util_getRealReferencePath(TBuf, sizeof(TBuf), currRadRef, pRad->pBus);
            amxd_trans_set_cstring_t(&trans, "LowerLayers", TBuf);
            free(currRadRef);
            char* radObjPath = amxd_object_get_path(pRad->pBus, AMXD_OBJECT_NAMED);

            amxd_trans_select_object(&trans, pEP->pBus);
            amxd_trans_set_cstring_t(&trans, "RadioReference", radObjPath);
            free(radObjPath);
        }


        TBuf[0] = 0;
        char* currSsidRef = amxd_object_get_cstring_t(pEP->pBus, "SSIDReference", NULL);
        wld_util_getRealReferencePath(TBuf, sizeof(TBuf), currSsidRef, pEP->pSSID->pBus);
        free(currSsidRef);
    }
    amxd_trans_set_cstring_t(&trans, "SSIDReference", TBuf);



    amxd_object_t* secObj = amxd_object_findf(object, "Security");
    amxd_trans_select_object(&trans, secObj);
    swl_security_apModeMaskToString(TBuf, sizeof(TBuf), SWL_SECURITY_APMODEFMT_LEGACY, pEP->secModesSupported);
    SAH_TRACEZ_INFO(ME, "Security.ModesSupported=%s", TBuf);
    amxd_trans_set_cstring_t(&trans, "ModesSupported", TBuf);

    amxd_object_t* wpsObj = amxd_object_findf(object, "WPS");
    amxd_trans_select_object(&trans, wpsObj);
    amxd_trans_set_int32_t(&trans, "Enable", pEP->WPS_Enable);
    swl_conv_transParamSetMask(&trans, "ConfigMethodsSupported", pEP->WPS_ConfigMethodsSupported, cstr_WPS_CM_Supported, WPS_CFG_MTHD_MAX);
    swl_conv_transParamSetMask(&trans, "ConfigMethodsEnabled", pEP->WPS_ConfigMethodsEnabled, cstr_WPS_CM_Supported, WPS_CFG_MTHD_MAX);

    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, , ME, "%s : trans apply failure", pEP->Name);

}

void wld_endpoint_setConnectionStatus(T_EndPoint* pEP, wld_epConnectionStatus_e connectionStatus, wld_epError_e error) {
    wld_intfStatus_e status = APSTI_ENABLED;
    if(connectionStatus == EPCS_ERROR) {
        status = APSTI_ERROR;
    } else if(connectionStatus == EPCS_DISABLED) {
        status = APSTI_DISABLED;
    }

    bool connected = (connectionStatus == EPCS_CONNECTED);

    SAH_TRACEZ_INFO(ME, "%s: set status connStat %u(%u) err %u stat %u(%u) conn %u",
                    pEP->alias,
                    connectionStatus, pEP->connectionStatus,
                    error, status, pEP->status, connected);

    if(connected && (pEP->currentProfile != NULL) && !swl_str_isEmpty(pEP->currentProfile->SSID)) {
        //update enpoint's SSID  with the current connected profile.
        swl_str_copy(pEP->pSSID->SSID, sizeof(pEP->pSSID->SSID), pEP->currentProfile->SSID);
    }

    int previousStatus = pEP->connectionStatus;
    s_setEndpointStatus(pEP, status, connectionStatus, error);
    s_setProfileStatus(pEP->currentProfile);
    s_setMultiApInfo(pEP);

    if(!connected && wld_endpoint_isReady(pEP)) {
        //We are not connected, but we should be. Start the reconnection
        //timer.

        uint32_t targetInterval = (previousStatus == EPCS_CONNECTED) ? pEP->reconnectDelay * 1000 : pEP->reconnectInterval * 1000;
        amxp_timer_state_t timerState = amxp_timer_get_state(pEP->reconnectTimer);
        if((timerState != amxp_timer_running) && (timerState != amxp_timer_started)) {
            amxp_timer_start(pEP->reconnectTimer, targetInterval);
        }
    } else {
        pEP->reconnect_count = 0;
        amxp_timer_stop(pEP->reconnectTimer);
    }
}

/**
 * @brief wld_endpoint_sync_connection
 *
 * Will fill in the datamodel of the endpoint and the profile based on the
 * connection status. Will also start the reconnect timer if the profile and
 * the endpoint are enabled, but we are not connected.
 *
 * @param pEP The endpoint
 * @param connected The connected status of the endpoint.
 * @param error The current error of the endpoint (0 if no error present)
 */
void wld_endpoint_sync_connection(T_EndPoint* pEP, bool connected, wld_epError_e error) {
    wld_epConnectionStatus_e connectionStatus;

    SAH_TRACEZ_INFO(ME, "%s: enable %d; connected; %d error %d",
                    pEP->alias, pEP->enable, connected, error);

    T_Radio* pR = pEP->pRadio;

    if(!pEP->enable || !pR->enable) {
        connectionStatus = EPCS_DISABLED;
    } else if(error) {
        connectionStatus = (error == EPE_SSID_NOT_FOUND) || (error == EPE_ERROR_MISCONFIGURED) ? EPCS_IDLE : EPCS_ERROR;
    } else if(connected) {
        connectionStatus = EPCS_CONNECTED;
    } else {
        connectionStatus = EPCS_IDLE;
    }

    wld_endpoint_setConnectionStatus(pEP, connectionStatus, error);
}


void wld_endpoint_sendPairingNotification(T_EndPoint* pEP, uint32_t type, const char* reason, T_WPSCredentials* credentials) {
    wld_wps_sendPairingNotification(pEP->pBus, type, reason, swl_typeMacBin_toBuf32Ref(&pEP->wpsSessionInfo.peerMac).buf, credentials);
}

/**
 * @brief __EndPoint_WPS_pushButton
 *
 * Start the WPS pairing process as an enrollee for the endpoint.
 *
 * @param fcall function call context
 * @param args argument list
 *     - clientPIN: optional argument to use clientPIN, give a 4-digit or 8-digit WPS PIN code.
 *     - ssid: restrict the wps session to a certain ssid
 *     - bssid: restrict the wps session to the AP with this MAC
 * @param retval variant that must contain the return value
 * @return function execution state
 */
amxd_status_t _pushButton(amxd_object_t* obj,
                          amxd_function_t* func _UNUSED,
                          amxc_var_t* args,
                          amxc_var_t* retval) {
    amxd_object_t* epObject = amxd_object_get_parent(obj);
    T_EndPoint* pEP = epObject->priv;
    ASSERT_NOT_NULL(pEP, amxd_status_unknown_error, ME, "NULL");
    SAH_TRACEZ_INFO(ME, "pushButton called for EP %s", pEP->Name);

    T_Radio* pR = pEP->pRadio;
    if(!(pEP->enable && pEP->WPS_Enable && (wld_rad_hasOnlyActiveEP(pR) || (pR->status == RST_UP)))) {
        SAH_TRACEZ_ERROR(ME, "Radio not up, endpoint not enabled or WPS not enabled %u %u %u %u",
                         pEP->enable, pEP->WPS_Enable, wld_rad_hasOnlyActiveEP(pR), pR->status);
        return amxd_status_unknown_error;
    }

    uint64_t call_id;
    amxd_function_defer(func, &call_id, retval, NULL, NULL);
    if(pR->pFA->mfn_wrad_fsm_state(pR) != FSM_IDLE) {
        if(pEP->WPS_PBC_Delay.timer) {
            //Need to cancel the previous request and replace with the current one
            wld_wps_pushButton_reply(pEP->WPS_PBC_Delay.call_id, SWL_USP_CMD_STATUS_ERROR_TIMEOUT);
            pEP->WPS_PBC_Delay.call_id = call_id;
            pEP->WPS_PBC_Delay.args = args;
        } else {
            //Need to create a timer
            amxp_timer_new(&pEP->WPS_PBC_Delay.timer, endpoint_wps_pbc_delayed_time_handler, pEP);
            amxp_timer_start(pEP->WPS_PBC_Delay.timer, 1000);
            pEP->WPS_PBC_Delay.intf.pEP = pEP;
            pEP->WPS_PBC_Delay.call_id = call_id;
            pEP->WPS_PBC_Delay.args = args;
        }
    } else {
        if(pEP->WPS_PBC_Delay.timer) {
            amxp_timer_delete(&pEP->WPS_PBC_Delay.timer);
            wld_wps_pushButton_reply(pEP->WPS_PBC_Delay.call_id, SWL_USP_CMD_STATUS_ERROR_TIMEOUT);
            pEP->WPS_PBC_Delay.call_id = 0;
            pEP->WPS_PBC_Delay.args = NULL;
            pEP->WPS_PBC_Delay.timer = NULL;
            pEP->WPS_PBC_Delay.intf.pEP = NULL;
        }
    }

    if(pEP->WPS_PBC_Delay.call_id != 0) {
        return amxd_status_deferred;
    }

    swl_rc_ne ret = endpoint_wps_start(pEP, pEP->WPS_PBC_Delay.call_id, args);
    return swl_rc_isOk(ret) ? amxd_status_ok : amxd_status_unknown_error;
}

amxd_status_t _getDebug(amxd_object_t* epObject,
                        amxd_function_t* func _UNUSED,
                        amxc_var_t* args _UNUSED,
                        amxc_var_t* retval) {
    SAH_TRACEZ_IN(ME);
    T_EndPoint* pEP = epObject->priv;

    ASSERT_NOT_NULL(pEP, amxd_status_unknown_error, ME, "EP NULL");

    amxc_var_init(retval);
    amxc_var_set_type(retval, AMXC_VAR_ID_HTABLE);

    char bssid[ETHER_ADDR_STR_LEN];

    if(pEP->currentProfile == NULL) {
        amxc_var_add_key(cstring_t, retval, "CurrentProfile", "");
    } else {
        amxc_var_add_key(cstring_t, retval, "CurrentProfile", pEP->currentProfile->alias);
        amxc_var_add_key(cstring_t, retval, "CurrentProfileSSID", pEP->currentProfile->SSID);
        wldu_convMac2Str(pEP->currentProfile->BSSID, ETHER_ADDR_LEN, bssid, ETHER_ADDR_STR_LEN);
        amxc_var_add_key(cstring_t, retval, "CurrentProfileBSSID", bssid);
    }

    return amxd_status_ok;
}

/**
 * @brief __EndPoint_WPS_cancelPairing
 *
 * Cancel the WPS pairing process.
 *
 * @param fcall function call context
 * @param args argument list
 * @param retval variant that must contain the return value
 * @return function execution state
 */
amxd_status_t _cancelPairing(amxd_object_t* obj,
                             amxd_function_t* func _UNUSED,
                             amxc_var_t* args _UNUSED,
                             amxc_var_t* retval _UNUSED) {
    amxd_object_t* epObject = amxd_object_get_parent(obj);
    T_EndPoint* pEP = epObject->priv;
    ASSERT_NOT_NULL(pEP, amxd_status_unknown_error, ME, "NULL");

    SAH_TRACEZ_INFO(ME, "cancelPairing called for %s", pEP->Name);

    // If a timer is running... stop and destroy it also.
    if(pEP->WPS_PBC_Delay.timer) {
        amxp_timer_delete(&pEP->WPS_PBC_Delay.timer);
        wld_wps_pushButton_reply(pEP->WPS_PBC_Delay.call_id, SWL_USP_CMD_STATUS_ERROR_TIMEOUT);
        pEP->WPS_PBC_Delay.call_id = 0;
        pEP->WPS_PBC_Delay.args = NULL;
        pEP->WPS_PBC_Delay.timer = NULL;
        pEP->WPS_PBC_Delay.intf.pEP = NULL;
    }
    wld_wps_clearPairingTimer(&pEP->wpsSessionInfo);
    swl_rc_ne ret = pEP->pFA->mfn_wendpoint_wps_cancel(pEP);
    return swl_rc_isOk(ret) ? amxd_status_ok : amxd_status_unknown_error;
}

void wld_endpoint_create_reconnect_timer(T_EndPoint* pEP) {
    if(pEP->reconnectTimer == NULL) {
        amxp_timer_new(&pEP->reconnectTimer, endpoint_reconnect_handler, pEP);
    }
    amxp_timer_set_interval(pEP->reconnectTimer, pEP->reconnectInterval * 1000);
}

/*****************************************************************************************************/
/* Endpoint Helper Functions                                                                         */
/*****************************************************************************************************/

/**
 * @brief wld_update_endpoint_stats
 *
 * Set Endpoint stats to the datamodel
 *
 * @param obj Endpoint Stats object
 * @param stats T_EndPointStats of the endpoint instance
 * @return
 */
bool wld_endpoint_updateStats(amxd_object_t* obj, T_EndPointStats* stats) {
    if(!obj || !stats) {
        SAH_TRACEZ_ERROR(ME, "Bad usage of function : !obj||!stats");
        return false;
    }
    amxd_trans_t trans;
    ASSERT_TRANSACTION_INIT(obj, &trans, false, ME, "trans init failure");


    amxd_trans_set_uint32_t(&trans, "LastDataDownlinkRate", stats->LastDataDownlinkRate);
    amxd_trans_set_uint32_t(&trans, "LastDataUplinkRate", stats->LastDataUplinkRate);
    amxd_trans_set_uint32_t(&trans, "Retransmissions", stats->Retransmissions);
    amxd_trans_set_int32_t(&trans, "SignalStrength", stats->SignalStrength);
    amxd_trans_set_int32_t(&trans, "SignalNoiseRatio", stats->SignalNoiseRatio);
    amxd_trans_set_int32_t(&trans, "Noise", stats->noise);
    amxd_trans_set_int32_t(&trans, "RSSI", stats->RSSI);
    amxd_trans_set_uint64_t(&trans, "TxBytes", stats->txbyte);
    amxd_trans_set_uint32_t(&trans, "TxPacketCount", stats->txPackets);
    amxd_trans_set_uint32_t(&trans, "Tx_Retransmissions", stats->txRetries);
    amxd_trans_set_uint64_t(&trans, "RxBytes", stats->rxbyte);
    amxd_trans_set_uint32_t(&trans, "RxPacketCount", stats->rxPackets);
    amxd_trans_set_uint32_t(&trans, "Rx_Retransmissions", stats->rxRetries);

    amxd_trans_set_cstring_t(&trans, "OperatingStandard", swl_radStd_unknown_str[stats->operatingStandard]);
    amxd_trans_set_uint32_t(&trans, "MaxRxSpatialStreamsSupported", stats->maxRxStream);
    amxd_trans_set_uint32_t(&trans, "MaxTxSpatialStreamsSupported", stats->maxTxStream);
    wld_ad_syncCapabilities(&trans, &stats->assocCaps);

    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, false, ME, "trans apply failure");


    return true;
}

/**
 * @brief wld_endpoint_validate_profile
 *
 * Validate the Endpoint Profile
 *
 * @param Profile T_EndPointProfile struct
 * @return true when validated OK, false when validation failed
 */
bool wld_endpoint_validate_profile(const T_EndPointProfile* epProfile) {
    if(!epProfile) {
        SAH_TRACEZ_ERROR(ME, "Profile is NULL");
        return false;
    }

    /* check or the SSID is filled in */
    if((strlen(epProfile->SSID) == 0) || (epProfile->SSID[0] == '\0')) {
        SAH_TRACEZ_ERROR(ME, "SSID is missing");
        return false;
    }

    /* check or the security mode/keys are valid */

    if(epProfile->secModeEnabled == SWL_SECURITY_APMODE_UNKNOWN) {
        SAH_TRACEZ_ERROR(ME, "Invalid security mode");
        return false;
    }

    swl_security_apMode_e keyClassification = SWL_SECURITY_APMODE_NONE;
    if(swl_security_isApModeWEP(epProfile->secModeEnabled)) {
        keyClassification = isValidWEPKey(epProfile->WEPKey);
        if(epProfile->secModeEnabled != keyClassification) {
            SAH_TRACEZ_ERROR(ME, "Invalid WEP key [%s] (detected mode [%u]) for mode [%u]", epProfile->WEPKey, keyClassification, epProfile->secModeEnabled);
            return false;
        }
    } else if((epProfile->secModeEnabled == SWL_SECURITY_APMODE_WPA_WPA2_P)
              || (epProfile->secModeEnabled == SWL_SECURITY_APMODE_WPA_P)
              || (epProfile->secModeEnabled == SWL_SECURITY_APMODE_WPA_E)
              || (epProfile->secModeEnabled == SWL_SECURITY_APMODE_WPA_WPA2_E)
              ) {
        if(!epProfile->keyPassPhrase[0]) {
            SAH_TRACEZ_ERROR(ME, "Invalid KeyPassPhrase: [%s]", epProfile->keyPassPhrase);
            return false;
        }

        if(isValidPSKKey(epProfile->keyPassPhrase)) {
            keyClassification = SWL_SECURITY_APMODE_WPA_P;
        }


        if(SWL_SECURITY_APMODE_WPA2_P != keyClassification) {
            if(isValidAESKey(epProfile->keyPassPhrase, PSK_KEY_SIZE_LEN - 1)) {
                keyClassification = SWL_SECURITY_APMODE_WPA2_P;
            }

            if(SWL_SECURITY_APMODE_WPA2_P != keyClassification) {
                SAH_TRACEZ_ERROR(ME, "invalid KeyPassPhrase: [%s]", epProfile->keyPassPhrase);
                return false;
            }
        }
    } else if((epProfile->secModeEnabled == SWL_SECURITY_APMODE_WPA2_P) || (epProfile->secModeEnabled == SWL_SECURITY_APMODE_WPA2_E)) {
        if(isValidAESKey(epProfile->keyPassPhrase, PSK_KEY_SIZE_LEN - 1)) {
            keyClassification = SWL_SECURITY_APMODE_WPA2_P;
        }
        if(!(SWL_SECURITY_APMODE_WPA2_P == keyClassification)) {
            SAH_TRACEZ_ERROR(ME, "invalid KeyPassPhrase :[%s] (detected mode [%u]) for mode [%u]", epProfile->keyPassPhrase, keyClassification, epProfile->secModeEnabled);
            return false;
        }
    } else if((epProfile->secModeEnabled == SWL_SECURITY_APMODE_WPA3_P) || (epProfile->secModeEnabled == SWL_SECURITY_APMODE_WPA2_WPA3_P)
              || (epProfile->secModeEnabled == SWL_SECURITY_APMODE_WPA3_E) || (epProfile->secModeEnabled == SWL_SECURITY_APMODE_WPA2_WPA3_E)) {
        if(isValidAESKey(epProfile->saePassphrase, SAE_KEY_SIZE_LEN)) {
            keyClassification = SWL_SECURITY_APMODE_WPA3_P;
        } else if(isValidAESKey(epProfile->keyPassPhrase, PSK_KEY_SIZE_LEN - 1)) {
            keyClassification = SWL_SECURITY_APMODE_WPA2_P;
        }
        if(!((SWL_SECURITY_APMODE_WPA3_P == keyClassification) || (SWL_SECURITY_APMODE_WPA2_P == keyClassification))) {
            SAH_TRACEZ_ERROR(ME, "invalid saePassphrase :[%s] and keyPassPhrase :[%s] (detected mode [%u]) for mode [%u]",
                             epProfile->saePassphrase, epProfile->keyPassPhrase, keyClassification, epProfile->secModeEnabled);
            return false;
        }
    } else if(epProfile->secModeEnabled == SWL_SECURITY_APMODE_AUTO) {
        keyClassification = SWL_SECURITY_APMODE_AUTO;
    }

    if((epProfile->secModeEnabled > SWL_SECURITY_APMODE_NONE) && (keyClassification == SWL_SECURITY_APMODE_NONE)) {
        SAH_TRACEZ_ERROR(ME, "No key available for security method [%d]", epProfile->secModeEnabled);
        return false;
    }

    return true;
}

/**
 * @brief wld_endpoint_set_status
 *
 * Sets the status fields of the endpoint in the datamodel.
 *
 * @param pEP The endpoint
 * @param status The current status of the endpoint
 * @param connectionStatus The current connection status of the endpoint
 * @param error The current error of the endpoint (0 if no error present)
 */
static void s_setEndpointStatus(T_EndPoint* pEP,
                                wld_intfStatus_e status,
                                wld_epConnectionStatus_e connectionStatus,
                                wld_epError_e error) {
    SAH_TRACEZ_INFO(ME, "Set status %s %i %i %i", pEP->Name, status, connectionStatus, error);

    bool changed = false;
    amxd_object_t* object = pEP->pBus;
    wld_epConnectionStatus_e oldConnectionStatus = pEP->connectionStatus;
    wld_intfStatus_e oldStatus = pEP->status;

    amxd_trans_t trans;
    ASSERT_TRANSACTION_INIT(object, &trans, , ME, "%s : trans init failure", pEP->Name);
    if(pEP->status != status) {
        amxd_trans_set_value(cstring_t, &trans, "Status", cstr_EndPoint_status[status]);
        changed = true;
    }
    if(pEP->connectionStatus != connectionStatus) {
        amxd_trans_set_value(cstring_t, &trans, "ConnectionStatus", cstr_EndPoint_connectionStatus[connectionStatus]);
        changed = true;
    }
    if(pEP->error != error) {
        amxd_trans_set_value(cstring_t, &trans, "LastError", cstr_EndPoint_lastError[error]);
        changed = true;
    }
    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, , ME, "%s : trans apply failure", pEP->Name);
    pEP->status = status;
    pEP->connectionStatus = connectionStatus;
    pEP->error = error;

    wld_status_e ssidStatus = RST_DOWN;
    if(pEP->connectionStatus == EPCS_CONNECTED) {
        ssidStatus = RST_UP;
    } else if((pEP->connectionStatus == EPCS_IDLE) || (status == APSTI_ENABLED)) {
        ssidStatus = RST_DORMANT;
    } else if(pEP->connectionStatus == EPCS_ERROR) {
        ssidStatus = RST_ERROR;
    }
    wld_ssid_setStatus(pEP->pSSID, ssidStatus, false);

    char oldSsid[SSID_NAME_LEN];
    swl_macBin_t oldBssid;
    swl_str_copy(oldSsid, sizeof(oldSsid), pEP->pSSID->SSID);
    memcpy(oldBssid.bMac, pEP->pSSID->BSSID, SWL_MAC_BIN_LEN);

    pEP->pFA->mfn_sync_ssid(pEP->pSSID->pBus, pEP->pSSID, SET);

    ASSERTI_TRUE(changed, , ME, "%s: unchanged", pEP->Name);

    SAH_TRACEZ_INFO(ME, "%s: changed status %u -> %u ; conn %u -> %u",
                    pEP->Name,
                    oldStatus, pEP->status,
                    oldConnectionStatus, pEP->connectionStatus);

    T_SSID* ssid = pEP->pSSID;

    if(connectionStatus != oldConnectionStatus) {
        pEP->lastConnStatusChange = swl_time_getMonoSec();
        if(oldConnectionStatus == EPCS_CONNECTED) {
            SAH_TRACEZ_WARNING(ME, "%s: EP disconnect from %s <"SWL_MAC_FMT "> (nr %u, try %u)",
                               pEP->Name, oldSsid, SWL_MAC_ARG(oldBssid.bMac),
                               pEP->assocStats.nrAssociations, pEP->assocStats.nrAssocAttemptsSinceDc);
            pEP->assocStats.lastDisassocTime = pEP->lastConnStatusChange;
            pEP->assocStats.nrAssocAttemptsSinceDc = 0;
        }
        if(connectionStatus == EPCS_CONNECTED) {
            pEP->assocStats.lastAssocTime = pEP->lastConnStatusChange;
            pEP->assocStats.nrAssociations++;
            swl_macBin_t bssid;
            wld_endpoint_getBssidBin(pEP, &bssid);
            SAH_TRACEZ_WARNING(ME, "%s: EP connect to %s <"MAC_PRINT_FMT ">", pEP->Name, ssid->SSID, MAC_PRINT_ARG(bssid.bMac));
        }
        s_writeAssocStats(pEP);
    }

    wld_ep_status_change_event_t event;
    event.ep = pEP;
    event.oldConnectionStatus = oldConnectionStatus;
    event.oldStatus = oldStatus;
    wld_event_trigger_callback(gWld_queue_ep_onStatusChange, &event);
}

/**
 * @brief wld_endpoint_profile_set_status
 *
 * Sets the status field of the endpoint profile in the datamodel.
 *
 * @param profile The endpoint profile
 * @param connected Is the profile connected or not
 */
static void s_setProfileStatus(T_EndPointProfile* profile) {
    ASSERTS_NOT_NULL(profile, , ME, "NULL");

    amxd_object_t* object = profile->pBus;
    wld_epProfileStatus_e status;

    T_EndPoint* pEP = profile->endpoint;
    bool connected = (pEP != NULL) ?
        (pEP->currentProfile == profile) ?
        ((pEP->connectionStatus == EPCS_CONNECTED) &&
         (swl_str_matches(pEP->pSSID->SSID, profile->SSID))) : false
            : false;

    SAH_TRACEZ_INFO(ME, "enable %d; profile->status %d; connected %d",
                    profile->enable, profile->status, connected);

    if(profile->enable) {
        if(connected) {
            status = EPPS_ACTIVE;
        } else if(!wld_epProfile_hasValidConf(profile)) {
            status = EPPS_ERROR;
        } else {
            status = EPPS_AVAILABLE;
        }
    } else {
        status = EPPS_DISABLED;
    }

    if(status != profile->status) {
        profile->status = status;
        amxd_trans_t trans;
        ASSERT_TRANSACTION_INIT(object, &trans, , ME, "%s : trans init failure", profile->alias);
        amxd_trans_set_cstring_t(&trans, "Status",
                                 cstr_EndPointProfile_status[profile->status]);
        ASSERT_TRANSACTION_LOCAL_DM_END(&trans, , ME, "%s : trans apply failure", profile->alias);
    }
}

/**
 * @brief s_setMultiApInfo
 *
 * Sets the MultiAPProfile and MultiAPVlanId fields of the endpoint in the datamodel.
 *
 * @param pEP The endpoint
 */
static void s_setMultiApInfo(T_EndPoint* pEP) {
    ASSERTS_NOT_NULL(pEP, , ME, "NULL");
    swl_typeUInt8_commitObjectParam(pEP->pBus, "MultiAPProfile", pEP->multiAPProfile);

    /* on disconnection, vlan is no more relevant */
    if(pEP->connectionStatus != EPCS_CONNECTED) {
        pEP->multiAPVlanId = 0;
    }
    swl_typeUInt16_commitObjectParam(pEP->pBus, "MultiAPVlanId", pEP->multiAPVlanId);
}

/**
 * Get BSSID of the accesspoint that the given endpoint is connected to.
 *
 * In case of error or if not connected, the null-mac is written to `tgtMac`.
 */
swl_rc_ne wld_endpoint_getBssidBin(T_EndPoint* pEP, swl_macBin_t* tgtMac) {
    ASSERT_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(tgtMac, SWL_RC_INVALID_PARAM, ME, "NULL");

    // Make sure in case of any errors, it's set to the null-mac.
    *tgtMac = g_swl_macBin_null;

    swl_macChar_t macChar = SWL_MAC_CHAR_NEW();
    swl_rc_ne ok = pEP->pFA->mfn_wendpoint_bssid(pEP, &macChar);
    ASSERT_TRUE(swl_rc_isOk(ok), SWL_RC_ERROR, ME, "%s not connected", pEP->Name);
    bool ret = swl_mac_charToBin(tgtMac, &macChar);
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "fail to convert mac address to binary");
    return SWL_RC_OK;
}

void wld_endpoint_performConnectCommit(T_EndPoint* pEP, bool alwaysCommit) {

    T_Radio* pR = pEP->pRadio;

    /* stop any ongoing scans (if any) */
    if(pR->pFA->mfn_wrad_stop_scan(pR) < 0) {
        SAH_TRACEZ_ERROR(ME, "Failed to stop ongoing scans");
    }

    if(pEP->pFA->mfn_wendpoint_connect_ap(pEP->currentProfile) < 0) {
        SAH_TRACEZ_ERROR(ME, "Failed to connect to AP");
        s_setEndpointStatus(pEP, APSTI_ENABLED, EPCS_ERROR, EPE_ERROR_MISCONFIGURED);
        s_setProfileStatus(pEP->currentProfile);
    }

    if(alwaysCommit) {
        wld_rad_doRadioCommit(pR);
    } else {
        wld_rad_doCommitIfUnblocked(pR);
    }

    SAH_TRACEZ_OUT(ME);

}

/**
 * @brief endpointReconfigure
 *
 * Reconfigures the endpoint based on the enable flags and the selected
 * profile.
 *
 * @param pEP The endpoint to reconfigure
 */
void wld_endpoint_reconfigure(T_EndPoint* pEP) {
    ASSERT_NOT_NULL(pEP, , ME, "null Endpoint");
    ASSERTI_NOT_NULL(pEP->pRadio, , ME, "null radio");
    ASSERTI_FALSE(pEP->index <= 0, , ME, "%s: endpoint has no iface", pEP->alias);

    T_Radio* pR = pEP->pRadio;

    SAH_TRACEZ_IN(ME);

    // Ignore this when we're doing a WPS Pairing!
    if(pEP->connectionStatus == EPCS_WPS_PAIRING) {
        SAH_TRACEZ_INFO(ME, "%s: WPS EP pairing session ongoing", pEP->alias);
        return;
    }

    /*
     * Disconnect any previous connecting/connected AP. We do this in any
     * case. Either the profile is disabled and we need to disconnect. Or
     * we want to connect and we want disconnect any previously connected
     * profile. If there was no connection this should not hurt.
     */
    SAH_TRACEZ_INFO(ME, "%s: Disconnecting previous Profile (if any)", pEP->alias);
    pEP->pFA->mfn_wendpoint_disconnect(pEP);

    /* Ignore also when we're not in STA mode.
       We don't want to sync the connection, as this force SCAN for AP */
    if(!pR->isSTA) {
        SAH_TRACEZ_INFO(ME, "%s: STA mode not active", pEP->alias);
        return;
    }

    bool enabled = true;
    int error = EPE_NONE;
    if(!pEP->currentProfile) {
        SAH_TRACEZ_INFO(ME, "%s: endpoint does not have a profile assigned.", pEP->alias);
        enabled = false;
        error = EPE_ERROR_MISCONFIGURED;
    } else if(!wld_endpoint_isReady(pEP)) {
        SAH_TRACEZ_INFO(ME, "%s: endpoint not ready", pEP->alias);
        if(pEP->currentProfile->status == EPPS_ERROR) {
            SAH_TRACEZ_INFO(ME, "%s: endpoint current profile has invalid conf.", pEP->alias);
            error = EPE_ERROR_MISCONFIGURED;
        }
        enabled = false;
    }

    if(!enabled) {
        /* Endpoint not enabled, nothing left to do. */
        SAH_TRACEZ_INFO(ME, "%s: endpoint disabled.", pEP->alias);
        wld_endpoint_sync_connection(pEP, false, error);
        return;
    }

    wld_endpoint_performConnectCommit(pEP, false);
}


/**
 * @brief endpoint_wps_pbc_delayed_time_handler
 *
 * The handler that get's called when the WPS PBC get's delayed because of an
 * ongoing commit.
 *
 * @param timer: The timer responsible for the callback.
 * @param userdata: Void pointer to the T_EndPoint struct.
 */
static void endpoint_wps_pbc_delayed_time_handler(amxp_timer_t* timer, void* userdata) {
    SAH_TRACEZ_IN(ME);
    swl_usp_cmdStatus_ne cmdStatus;
    T_EndPoint* pEP = userdata;
    T_Radio* pR = pEP->pRadio;
    if(pR->pFA->mfn_wrad_fsm_state(pR) != FSM_IDLE) {
        // We're still not ready... start timer again maybe we must count this?
        amxp_timer_start(timer, 10000);
        goto exit;
    }

    // In case we're enrollee (STA mode and no AP active) we accept Radio RST_DORMANT status.
    // As the Radio is UP but passively on a DFS channel.
    if(pEP->enable && pEP->WPS_Enable && (wld_rad_hasOnlyActiveEP(pR) || (pR->status == RST_UP))) {
        swl_rc_ne ret = endpoint_wps_start(pEP, pEP->WPS_PBC_Delay.call_id, pEP->WPS_PBC_Delay.args);
        if(swl_rc_isOk(ret)) {
            cmdStatus = SWL_USP_CMD_STATUS_SUCCESS;
        } else if(ret == SWL_RC_NOT_IMPLEMENTED) {
            cmdStatus = SWL_USP_CMD_STATUS_ERROR_NOT_IMPLEMENTED;
        } else if(ret == SWL_RC_INVALID_PARAM) {
            cmdStatus = SWL_USP_CMD_STATUS_ERROR_INVALID_INPUT;
        } else if(ret == SWL_RC_NOT_AVAILABLE) {
            cmdStatus = SWL_USP_CMD_STATUS_ERROR_NOT_READY;
        } else {
            cmdStatus = SWL_USP_CMD_STATUS_ERROR;
        }
    } else {
        SAH_TRACEZ_ERROR(ME, "Radio condition not ok, endpoint not enabled or WPS not enabled");
        if(pR->status != RST_UP) {
            cmdStatus = SWL_USP_CMD_STATUS_ERROR_INTERFACE_DOWN;
        } else {
            cmdStatus = SWL_USP_CMD_STATUS_ERROR_OTHER;
        }
    }

    amxp_timer_delete(&timer);
    wld_wps_pushButton_reply(pEP->WPS_PBC_Delay.call_id, cmdStatus);
    pEP->WPS_PBC_Delay.call_id = 0;
    pEP->WPS_PBC_Delay.args = NULL;
    pEP->WPS_PBC_Delay.timer = NULL;
    pEP->WPS_PBC_Delay.intf.pEP = NULL;

exit:
    SAH_TRACEZ_OUT(ME);
}

static swl_rc_ne endpoint_wps_start(T_EndPoint* pEP, uint64_t call_id _UNUSED, amxc_var_t* args) {
    const char* ssid = GET_CHAR(args, "ssid");
    const char* bssid = GET_CHAR(args, "bssid");
    const char* pin = GET_CHAR(args, "clientPIN");

    int method;
    if(pin) {
        method = WPS_CFG_MTHD_PIN;
    } else {
        method = WPS_CFG_MTHD_PBC;
    }
    swl_macChar_t cBssid = SWL_MAC_CHAR_NEW();
    if(!swl_str_isEmpty(bssid)) {
        swl_mac_charToStandard(&cBssid, bssid);
    }
    return pEP->pFA->mfn_wendpoint_wps_start(pEP, method, (char*) pin, (char*) ssid, &cBssid);
}

static void endpoint_reconnect_handler(amxp_timer_t* timer _UNUSED, void* userdata) {
    T_EndPoint* pEP = userdata;
    T_Radio* pRad = pEP->pRadio;
    swl_chanspec_t chanspec;

    SAH_TRACEZ_WARNING(ME, "%s: Retrying to connect", pEP->alias);

    if(pEP->connectionStatus == EPCS_WPS_PAIRING) {
        SAH_TRACEZ_INFO(ME, "Skip reconnect trigger due WPS pairing");
        return;
    }

    chanspec = wld_rad_getSwlChanspec(pRad);
    uint32_t curStateTime = (uint32_t) (swl_time_getMonoSec() - pRad->changeInfo.lastStatusChange);
    uint32_t clearTimeSec = wld_channel_get_band_clear_time(chanspec) / 1000;

    SAH_TRACEZ_INFO(ME, "%s: status %u timeInfo %u/%u chanInfo %u/%u", pRad->Name, pRad->status,
                    curStateTime, clearTimeSec, pRad->channel, pRad->runningChannelBandwidth);

    if(pRad->status == RST_DORMANT) {
        if(clearTimeSec == 0) {
            SAH_TRACEZ_ERROR(ME, "%s: dormant, no dfs chan %u/%u", pRad->Name, pRad->channel, pRad->runningChannelBandwidth);
            clearTimeSec = 60;
        }

        if(curStateTime < (clearTimeSec + 10)) {
            SAH_TRACEZ_INFO(ME, "%s: skip reconnect DFS CAC %u/%u : %u/%u ",
                            pRad->Name, pRad->channel, pRad->runningChannelBandwidth,
                            curStateTime, clearTimeSec);
            return;
        }
    }

    // Do reconnect
    wld_endpoint_performConnectCommit(pEP, true);


    pEP->assocStats.nrAssocAttempts++;
    pEP->assocStats.nrAssocAttemptsSinceDc++;
    s_writeAssocStats(pEP);

    SAH_TRACEZ_INFO(ME, "%s: rad reconnect try %u / %u : prevTries %u / %u",
                    pRad->Name, pEP->reconnect_count, pEP->reconnect_rad_trigger,
                    pEP->assocStats.nrAssocAttempts, pEP->assocStats.nrAssocAttemptsSinceDc);

    if(pEP->reconnect_rad_trigger == 0) {
        SAH_TRACEZ_INFO(ME, "no rad trigger");
        return;
    }

    pEP->reconnect_count++;
    if(pEP->reconnect_count >= pEP->reconnect_rad_trigger) {
        SAH_TRACEZ_WARNING(ME, "%s: perform radio toggle, try %u", pRad->Name, pEP->assocStats.nrAssocAttemptsSinceDc);
        pRad->pFA->mfn_wrad_enable(pRad, 1, SET);

        swl_chanspec_t chanspec = swl_chanspec_fromDm(swl_channel_defaults[pRad->operatingFrequencyBand],
                                                      pRad->operatingChannelBandwidth, pRad->operatingFrequencyBand);
        wld_chanmgt_setTargetChanspec(pRad, chanspec, false, CHAN_REASON_EP_MOVE, NULL);
        pEP->reconnect_count = 0;
    }
}

swl_rc_ne wld_endpoint_checkConnection(T_EndPoint* pEP) {
    ASSERT_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTI_TRUE(pEP->connectionStatus == EPCS_CONNECTED, SWL_RC_INVALID_STATE, ME, "%s not connected", pEP->Name);

    swl_macBin_t binMac;
    char* failMsg = NULL;

    swl_rc_ne ret = wld_endpoint_getBssidBin(pEP, &binMac);
    if(ret < SWL_RC_OK) {
        failMsg = "Failed to get BSSID";
    } else {
        T_SSID* pSSID = pEP->pSSID;
        if(!SWL_MAC_BIN_MATCHES(&binMac, pSSID->BSSID)) {
            failMsg = "HW BSSID does match stored BSSID";
        }
    }

    if(failMsg != NULL) {
        T_Radio* pRad = pEP->pRadio;
        wld_rad_incrementCounterStr(pRad, &pRad->genericCounters, WLD_RAD_EV_EP_FAIL,
                                    "%s: %s", pEP->Name, failMsg);
        wld_endpoint_sync_connection(pEP, false, false);
        return SWL_RC_ERROR;
    } else {
        SAH_TRACEZ_INFO(ME, "%s: check ok "MAC_PRINT_FMT, pEP->Name, MAC_PRINT_ARG(binMac.bMac));
        return SWL_RC_OK;
    }
}

T_EndPoint* wld_endpoint_fromIt(amxc_llist_it_t* it) {
    ASSERTS_NOT_NULL(it, NULL, ME, "NULL");

    return amxc_llist_it_get_data(it, T_EndPoint, it);
}


/**
 * Provide the target mac address. If false, no target is currently configured.
 */
bool wld_endpoint_getTargetBssid(T_EndPoint* pEP, swl_macBin_t* macBuffer) {
    ASSERT_NOT_NULL(pEP, false, ME, "NULL");
    T_EndPointProfile* pEpProf = pEP->currentProfile;
    ASSERTI_NOT_NULL(pEpProf, false, ME, "NULL");
    ASSERTI_NOT_NULL(macBuffer, false, ME, "NULL");

    if(!SWL_MAC_BIN_MATCHES(pEpProf->BSSID, &g_swl_macBin_null)) {
        memcpy(macBuffer->bMac, pEpProf->BSSID, ETHER_ADDR_LEN);
        return true;
    } else if(wld_tinyRoam_isRoaming(pEP)) {
        const swl_macBin_t* pTrTgtBssid = wld_tinyRoam_targetBssid(pEP);
        ASSERT_NOT_NULL(pTrTgtBssid, false, ME, "NULL");
        memcpy(macBuffer->bMac, pTrTgtBssid, ETHER_ADDR_LEN);
        return true;
    }
    return false;
}

void wld_endpoint_destroy(T_EndPoint* pEP) {
    ASSERT_NOT_NULL(pEP, , ME, "NULL");
    s_deinitEP(pEP);

    while(amxc_llist_size(&pEP->llProfiles) > 0) {
        amxc_llist_it_t* it = amxc_llist_take_first(&pEP->llProfiles);
        T_EndPointProfile* pEpProf = wld_epProfile_fromIt(it);
        wld_epProfile_delete(pEP, pEpProf);
    }

    if(pEP->pBus != NULL) {
        pEP->pBus->priv = NULL;
        pEP->pBus = NULL;
    }

    free(pEP);
}

T_EndPoint* wld_endpoint_create(T_Radio* pRad, const char* epName, amxd_object_t* object) {
    ASSERT_NOT_NULL(pRad, NULL, ME, "NULL");
    T_EndPoint* pEP = calloc(1, sizeof(T_EndPoint));
    ASSERT_NOT_NULL(pEP, NULL, ME, "NULL");

    if(!s_initEp(pEP, pRad, epName)) {
        free(pEP);
        return NULL;
    }

    // init ep obj
    pEP->pBus = object;
    pEP->wpsSessionInfo.intfObj = object;
    amxd_object_t* wpsinstance = amxd_object_get(object, "WPS");
    if(wpsinstance == NULL) {
        SAH_TRACEZ_WARNING(ME, "%s: WPS subObj is not available", pEP->Name);
    } else {
        wpsinstance->priv = &pEP->wpsSessionInfo;
    }
    object->priv = pEP;
    return pEP;
}

static amxd_status_t _linkEpSsid(amxd_object_t* object, amxd_object_t* pSsidObj) {
    ASSERT_NOT_NULL(object, amxd_status_unknown_error, ME, "NULL");
    const char* epName = amxd_object_get_name(object, AMXD_OBJECT_NAMED);
    ASSERT_NOT_NULL(epName, amxd_status_unknown_error, ME, "NULL");
    T_SSID* pSSID = NULL;
    if(pSsidObj) {
        pSSID = (T_SSID*) pSsidObj->priv;
    }
    T_EndPoint* pEP = (T_EndPoint*) object->priv;
    if(pEP) {
        ASSERTI_NOT_EQUALS(pSSID, pEP->pSSID, amxd_status_ok, ME, "same ssid reference");
        s_deinitEP(pEP);
    }
    ASSERTI_NOT_NULL(pSSID, amxd_status_ok, ME, "No SSID Ctx");
    T_Radio* pRad = pSSID->RADIO_PARENT;
    ASSERTI_NOT_NULL(pRad, amxd_status_ok, ME, "No Radio Ctx");
    SAH_TRACEZ_INFO(ME, "pSSID(%p) pRad(%p)", pSSID, pRad);

    if(pEP != NULL) {
        bool ret = s_initEp(pEP, pRad, epName);
        ASSERT_EQUALS(ret, true, amxd_status_unknown_error, ME, "%s: fail to re-create endpoint", epName);
    } else {
        pEP = wld_endpoint_create(pRad, epName, object);
        ASSERT_NOT_NULL(pEP, amxd_status_unknown_error, ME, "%s: fail to create endpoint", epName);
    }
    pSSID->ENDP_HOOK = pEP;
    pEP->pSSID = pSSID;

    SAH_TRACEZ_INFO(ME, "%s: add ep %s", pRad->Name, epName);

    return amxd_status_ok;
}

amxd_status_t _wld_ep_delInstance_odf(amxd_object_t* object,
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
    wld_endpoint_destroy(object->priv);

    SAH_TRACEZ_OUT(ME);
    return status;
}

amxd_status_t _wld_ep_addInstance_oaf(amxd_object_t* object,
                                      amxd_param_t* param,
                                      amxd_action_t reason,
                                      const amxc_var_t* const args,
                                      amxc_var_t* const retval,
                                      void* priv) {
    SAH_TRACEZ_IN(ME);
    amxd_status_t status = amxd_status_ok;
    status = amxd_action_object_add_inst(object, param, reason, args, retval, priv);
    ASSERT_EQUALS(status, amxd_status_ok, status, ME, "Fail to create instance");
    amxd_object_t* instance = amxd_object_get_instance(object, NULL, GET_UINT32(retval, "index"));
    ASSERT_NOT_NULL(instance, amxd_status_unknown_error, ME, "Fail to get instance");
    amxd_object_t* pSsidObj = amxd_object_findf(get_wld_object(), "SSID.%s", amxd_object_get_name(instance, AMXD_OBJECT_NAMED));
    _linkEpSsid(instance, pSsidObj);
    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

static void s_syncEpSSIDDm(T_EndPoint* pEP) {
    ASSERTS_NOT_NULL(pEP, , ME, "NULL");
    T_Radio* pRad = pEP->pRadio;
    ASSERTS_NOT_NULL(pRad, , ME, "No mapped Rad Ctx");
    T_SSID* pSSID = pEP->pSSID;
    ASSERTS_NOT_NULL(pSSID, , ME, "No mapped SSID Ctx");

    SAH_TRACEZ_IN(ME);

    bool enable = amxd_object_get_bool(pEP->pBus, "Enable", NULL);
    if(enable && !pEP->enable) {
        /* restore enable flag */
        pEP->enable = enable;
        pRad->pFA->mfn_wendpoint_enable(pEP, pEP->enable);
        wld_autoCommitMgr_notifyEpEdit(pEP);
    }
    wld_endpoint_reconfigure(pEP);

    pRad->pFA->mfn_sync_ep(pEP);
    pRad->pFA->mfn_sync_ssid(pSSID->pBus, pSSID, SET);

    SAH_TRACEZ_OUT(ME);
}

/*
 * check conditions to create endpoint interface
 */
bool wld_endpoint_needCreateIface(T_EndPoint* pEP) {
    ASSERTS_NOT_NULL(pEP, false, ME, "NULL");
    return ((pEP->index <= 0) && (pEP->pRadio && pEP->pRadio->isSTASup && pEP->pRadio->isSTA));
}

/*
 * check conditions to delete endpoint interface
 */
bool wld_endpoint_needDestroyIface(T_EndPoint* pEP) {
    ASSERTS_NOT_NULL(pEP, false, ME, "NULL");
    return ((pEP->index > 0) && !(pEP->pRadio && pEP->pRadio->isSTASup && pEP->pRadio->isSTA));
}

/*
 * finalize AP interface creation and MAC/BSSID reading
 */
static bool s_finalizeEpCreation(T_EndPoint* pEP) {
    ASSERTS_NOT_NULL(pEP, false, ME, "NULL");
    T_Radio* pRad = pEP->pRadio;
    ASSERTS_NOT_NULL(pRad, false, ME, "No mapped Rad Ctx");
    T_SSID* pSSID = pEP->pSSID;
    ASSERTS_NOT_NULL(pSSID, false, ME, "No mapped SSID Ctx");

    SAH_TRACEZ_IN(ME);

    char epIfName[IFNAMSIZ] = {0};
    int epIfIndex = -1;
    int ret = -1;
    if(wld_endpoint_needCreateIface(pEP)) {
        swl_str_copy(epIfName, sizeof(epIfName), pRad->Name);
        epIfIndex = pRad->index;
        int ret = pRad->pFA->mfn_wrad_addendpointif(pRad, epIfName, sizeof(epIfName));
        ASSERT_FALSE(ret < 0, false, ME, "%s: fail to add endpoint iface", pRad->Name);
        if(ret > 0) {
            epIfIndex = ret;
        }
        SAH_TRACEZ_WARNING(ME, "set ep iface(%s) (ifIndex:%d) on radio(%s)", epIfName, epIfIndex, pRad->Name);
        swl_str_copy(pEP->Name, sizeof(pEP->Name), epIfName);
        pEP->index = epIfIndex;
        pEP->toggleBssOnReconnect = (pEP->index == pRad->index);

        ret = pRad->pFA->mfn_wendpoint_create_hook(pEP);
        ASSERT_FALSE(ret < 0, false, ME, "%s: ep create hook failed %d", pEP->alias, ret);
    }

    //default EP iface mac when not defined by the implementation, is base rad mac
    if((epIfIndex != -1) && (swl_mac_binIsNull((swl_macBin_t*) pSSID->MACAddress))) {
        memcpy(pSSID->MACAddress, pRad->MACAddr, sizeof(pRad->MACAddr));
    }

    /* Get defined paramater values from the default instance */
    char* epMac = amxd_object_get_cstring_t(pSSID->pBus, "MACAddress", NULL);
    if(swl_mac_charIsValidStaMac((swl_macChar_t*) epMac)) {
        pRad->pFA->mfn_sync_ssid(pSSID->pBus, pSSID, GET);
    }
    free(epMac);

    wld_mld_registerLink(pSSID, pSSID->mldUnit);
    s_sendChangeEvent(pEP, WLD_EP_CHANGE_EVENT_CREATE_FINAL, NULL);

    //delay sync AP and SSID Dm after all conf has been loaded
    swla_delayExec_add((swla_delayExecFun_cbf) s_syncEpSSIDDm, pEP);

    SAH_TRACEZ_OUT(ME);
    return true;
}

/*
 * AP instance addition event handler
 * late handling only to finalize AP interface creation (using twin ssid instance)
 * when SSIDReference is not explicitely provided by user conf
 */
static void s_addEpInst_oaf(void* priv _UNUSED, amxd_object_t* object, const amxc_var_t* const initialParamValues _UNUSED) {
    SAH_TRACEZ_IN(ME);

    if(GET_ARG(initialParamValues, "SSIDReference") == NULL) {
        /*
         * As user conf does not include specific SSIDReference
         * then use the default twin ssid mapping already initiated in the early action handler
         */
        s_finalizeEpCreation(object->priv);
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
    T_EndPoint* pEP = object->priv;
    if((pEP != NULL) && (pEP->pSSID != NULL) && (pEP->pSSID->pBus == pSsidObj) && (pEP->index > 0)) {
        SAH_TRACEZ_NOTICE(ME, "%s: same reference ssid: no need for new finalization", oname);
        return;
    }

    ASSERT_EQUALS(_linkEpSsid(object, pSsidObj), amxd_status_ok, , ME, "%s: fail to link Ep to SSID (%s)", oname, ssidRef);

    s_finalizeEpCreation(object->priv);

    SAH_TRACEZ_OUT(ME);
}

/*
 * @brief finalize endpoint, by :
 * - mapping/unmapping endpoint instance with relative ssid instance
 * - creating/deleting the endpoint interface, when needed.
 * If interface and already set, and required for creation, then it will be re-created
 */
swl_rc_ne wld_endpoint_finalize(T_EndPoint* pEP) {
    SAH_TRACEZ_IN(ME);
    ASSERT_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pEP->pSSID, SWL_RC_INVALID_STATE, ME, "NULL");
    amxd_object_t* pSsidObj = pEP->pSSID->pBus;
    const char* ssidRefName = amxd_object_get_name(pSsidObj, AMXD_OBJECT_NAMED);
    s_deinitEP(pEP);
    ASSERT_EQUALS(_linkEpSsid(pEP->pBus, pSsidObj), amxd_status_ok, SWL_RC_ERROR, ME, "%s: fail to link Ep to SSID (%s)", pEP->alias, ssidRefName);
    ASSERT_TRUE(s_finalizeEpCreation(pEP), SWL_RC_ERROR, ME, "%s: fail to finalize ep creation", pEP->alias);
    SAH_TRACEZ_OUT(ME);
    return SWL_RC_OK;
}

amxd_status_t _EndPoint_debug(amxd_object_t* object,
                              amxd_function_t* func _UNUSED,
                              amxc_var_t* args,
                              amxc_var_t* retval) {

    T_EndPoint* pEP = object->priv;
    ASSERT_NOT_NULL(pEP, amxd_status_unknown_error, ME, "NULL");

    amxc_var_init(retval);
    amxc_var_set_type(retval, AMXC_VAR_ID_HTABLE);

    const char* feature = GET_CHAR(args, "op");
    char buffer[100];

    if(feature == NULL) {
        feature = "help";
    }

    if(strcmp(feature, "profile") == 0) {
        if(pEP->currentProfile == NULL) {
            amxc_var_add_key(cstring_t, retval, "Profile", "<NULL>");
        } else {
            amxc_var_add_key(cstring_t, retval, "Profile", pEP->currentProfile->alias);
            amxc_var_add_key(cstring_t, retval, "SSID", pEP->currentProfile->SSID);
            char macBuffer[ETHER_ADDR_STR_LEN];
            wldu_convMac2Str(pEP->currentProfile->BSSID, ETHER_ADDR_LEN, macBuffer, sizeof(macBuffer));
            amxc_var_add_key(cstring_t, retval, "BSSID", macBuffer);
            amxc_var_add_key(cstring_t, retval, "Security", swl_security_apModeToString(pEP->currentProfile->secModeEnabled, SWL_SECURITY_APMODEFMT_LEGACY));
            amxc_var_add_key(cstring_t, retval, "KeyPassPhrase", pEP->currentProfile->keyPassPhrase);
            amxc_var_add_key(cstring_t, retval, "SAEPassphrase", pEP->currentProfile->saePassphrase);
            amxc_var_add_key(cstring_t, retval, "PreSharedKey", pEP->currentProfile->preSharedKey);
            amxc_var_add_key(cstring_t, retval, "WEPKey", pEP->currentProfile->WEPKey);
        }
    } else if(strcmp(feature, "assocStats") == 0) {
        swl_time_mapAddMono(retval, "LastConnChange", pEP->lastConnStatusChange);
        swl_time_mapAddMono(retval, "LastAssoc", pEP->assocStats.lastAssocTime);
        amxc_var_add_key(uint32_t, retval, "NrAssoc", pEP->assocStats.nrAssociations);
        amxc_var_add_key(uint32_t, retval, "NrAssocTry", pEP->assocStats.nrAssocAttempts);
        amxc_var_add_key(uint32_t, retval, "NrAssocTrySinceDc", pEP->assocStats.nrAssocAttemptsSinceDc);
    } else if(strcmp(feature, "help") == 0) {
        amxc_var_t cmdList;
        amxc_var_init(&cmdList);
        amxc_var_set_type(&cmdList, AMXC_VAR_ID_LIST);
        amxc_var_add_key(cstring_t, &cmdList, "assocStats", "");
        amxc_var_add_key(cstring_t, &cmdList, "hasEnoughRoamAp", "");
        amxc_var_add_key(cstring_t, &cmdList, "profile", "");
        amxc_var_add_key(cstring_t, &cmdList, "checkConnection", "");
        amxc_var_add_key(cstring_t, &cmdList, "WpaSuppCfg", "");
        amxc_var_add_key(cstring_t, &cmdList, "help", "");
        amxc_var_add_key(amxc_llist_t, retval, "cmds", amxc_var_get_const_amxc_llist_t(&cmdList));
    } else if(strcmp(feature, "checkConnection") == 0) {
        swl_rc_ne retCode = wld_endpoint_checkConnection(pEP);
        amxc_var_add_key(cstring_t, retval, "result", swl_rc_toString(retCode));
    } else if(strcmp(feature, "WpaSuppCfg") == 0) {
        swl_rc_ne retCode;
        const char* instruction = GET_CHAR(args, "instruction");
        char tmpName[128];
        snprintf(tmpName, sizeof(tmpName), "%s-%s.tmp.txt", "/tmp/wpa_supplicant", pEP->Name);
        if(swl_str_matches(instruction, "createConfig")) {
            retCode = wld_wpaSupp_cfgFile_create(pEP, tmpName);
        } else if(swl_str_matches(instruction, "globalUpdate")) {
            const char* key = GET_CHAR(args, "key");
            const char* value = GET_CHAR(args, "value");
            retCode = wld_wpaSupp_cfgFile_globalConfigUpdate(tmpName, key, value);
        } else if(swl_str_matches(instruction, "networkUpdate")) {
            const char* key = GET_CHAR(args, "key");
            const char* value = GET_CHAR(args, "value");
            retCode = wld_wpaSupp_cfgFile_networkConfigUpdate(tmpName, key, value);
        } else {
            // dump the config file
            wld_wpaSupp_config_t* config;
            wld_wpaSupp_loadConfig(&config, tmpName);
            swl_mapIt_t it;
            char key[64];
            char value[64];
            swl_map_for_each(it, wld_wpaSupp_getGlobalConfig(config)) {
                swl_map_itKeyChar(key, sizeof(key), &it);
                swl_map_itValueChar(value, sizeof(value), &it);
                amxc_var_add_key(cstring_t, retval, key, value);
            }
            swl_map_for_each(it, wld_wpaSupp_getNetworkConfig(config)) {
                swl_map_itKeyChar(key, sizeof(key), &it);
                swl_map_itValueChar(value, sizeof(value), &it);
                amxc_var_add_key(cstring_t, retval, key, value);
            }
            wld_wpaSupp_deleteConfig(config);
            retCode = SWL_RC_OK;
        }
        amxc_var_add_key(cstring_t, retval, "result", swl_rc_toString(retCode));
    } else {
        snprintf(buffer, sizeof(buffer), "unknown command %s", feature);
        amxc_var_add_key(cstring_t, retval, "error", buffer);
    }

    return amxd_status_ok;
}

SWLA_DM_HDLRS(sEpDmHdlrs,
              ARR(SWLA_DM_PARAM_HDLR("SSIDReference", s_setSSIDRef_pwf),
                  SWLA_DM_PARAM_HDLR("ReconnectDelay", s_setReconnectDelay_pwf),
                  SWLA_DM_PARAM_HDLR("ReconnectInterval", s_setReconnectInterval_pwf),
                  SWLA_DM_PARAM_HDLR("MultiAPEnable", s_setMultiAPEnable_pwf),
                  SWLA_DM_PARAM_HDLR("MultiAPProfile", s_setMultiAPProfile_pwf),
                  SWLA_DM_PARAM_HDLR("ReconnectRadioToggleThreshold", s_setRadToggleThreshold_pwf),
                  SWLA_DM_PARAM_HDLR("ProfileReference", s_setProfileReference_pwf),
                  SWLA_DM_PARAM_HDLR("BridgeInterface", s_setBridgeInterface_pwf),
                  SWLA_DM_PARAM_HDLR("Enable", s_setEnable_pwf),
                  ),
              .instAddedCb = s_addEpInst_oaf, );

void _wld_ep_setConf_ocf(const char* const sig_name,
                         const amxc_var_t* const data,
                         void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sEpDmHdlrs, sig_name, data, priv);
}
