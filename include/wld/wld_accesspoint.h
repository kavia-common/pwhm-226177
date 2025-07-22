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

#ifndef __WLD_ACCESSPOINT_H__
#define __WLD_ACCESSPOINT_H__



#include "wld.h"
#include "wld_wps.h"
#include "wld_extMod.h"

void t_destroy_handler_AP (amxd_object_t* object);
T_AccessPoint* wld_ap_create(T_Radio* pRad, const char* vapName, uint32_t idx);
T_AccessPoint* wld_ap_fromObj(amxd_object_t* apObj);
int wld_ap_initializeVendor(T_Radio* pR, T_AccessPoint* pAP, T_SSID* pSSID);
int wld_ap_init(T_AccessPoint* pAP);
void wld_ap_destroy(T_AccessPoint* pAP);

typedef struct {
    const char* mac;
    int32_t reason;
    int32_t result;
} wld_ap_kickAction_t;

typedef struct {
    const char* mac;
    const char* bssid;
    wld_transferStaArgs_t* args;
    swl_rc_ne result;
} wld_ap_steerAction_t;

typedef struct {
    amxc_var_t* updates;
    uint32_t nrUpdates;
} wld_ap_rssiSampleAction_t;

void wld_vap_sendActionEvent(T_AccessPoint* pAP, wld_vap_actionEvent_e actionEvent, void* data);
void SyncData_AP2OBJ(amxd_object_t* object, T_AccessPoint* pAP, int set);
void SetAPDefaults(T_AccessPoint* pAP, int idx);

swl_rc_ne wld_vap_saveAssocReq(T_AccessPoint* pAP, swl_bit8_t* frame, size_t frameLen);
swl_rc_ne wld_vap_notifyActionFrame(T_AccessPoint* pAP, const char* frame);
swl_rc_ne wld_vap_notifyDeauthDisassocFrame(T_AccessPoint* pAP, const char* eventName, swl_macBin_t* staMacAddress, swl_IEEE80211deauthReason_ne reason, bool isTxFrame);
swl_rc_ne wld_vap_sync_device(T_AccessPoint* pAP, T_AssociatedDevice* pAD);
void wld_vap_syncNrDev(T_AccessPoint* pAP);
bool wld_vap_sync_assoclist(T_AccessPoint* pAP);
void wld_ap_sec_doSync(T_AccessPoint* pAP);
bool wld_ap_sec_checkSharedSecConfigs(T_AccessPoint* pAP1, T_AccessPoint* pAP2);
bool wld_ap_sec_checkSecConfigParams(const char* oname, amxc_var_t* pParams, swl_security_apMode_m defModesSupported, T_Radio* pRad);

amxd_status_t kickStation(amxd_object_t* obj_AP,
                          amxd_function_t* func,
                          amxc_var_t* args,
                          amxc_var_t* ret);

amxd_status_t _AccessPoint_VerifyAccessPoint(amxd_object_t* object, amxd_function_t* func _UNUSED, amxc_var_t* args _UNUSED, amxc_var_t* ret _UNUSED);
amxd_status_t _AccessPoint_CommitAccessPoint(amxd_object_t* object, amxd_function_t* func _UNUSED, amxc_var_t* args _UNUSED, amxc_var_t* ret _UNUSED);


void wld_destroy_associatedDevice(T_AccessPoint* pAP, int index);
T_AssociatedDevice* wld_create_associatedDevice(T_AccessPoint* pAP, swl_macBin_t* MacAddress);
T_AssociatedDevice* wld_vap_get_existing_station(T_AccessPoint* pAP, swl_macBin_t* macAddress);
bool wld_vap_cleanup_stationlist(T_AccessPoint* pAP);
bool wld_vap_assoc_update_cuid(T_AccessPoint* pAP, swl_macBin_t* mac, char* cuid, int len);

void wld_ap_bss_done(T_AccessPoint* ap, const swl_macChar_t* mac, int reply_code, const swl_macChar_t* targetBssid);
void wld_ap_rrm_item(T_AccessPoint* ap, const swl_macChar_t* mac, amxc_var_t* result);

void wld_ap_notifyToggle(T_AccessPoint* pAP);

void wld_ap_performErrorToggle(T_AccessPoint* pAP, const char* reason);

void wld_ap_sendPairingNotification(T_AccessPoint* pAP, uint32_t type, const char* reason, const char* macAddress);

void wld_vap_updateState(T_AccessPoint* pAP);
T_AccessPoint* wld_vap_get_vap(const char* ifname);
T_AccessPoint* wld_ap_getVapByName(const char* name);
T_AccessPoint* wld_ap_fromIt(amxc_llist_it_t* it);
T_AccessPoint* wld_ap_getVapByBssid(swl_macBin_t* bssid);
uint32_t wld_ap_getMaxNbrSta(T_AccessPoint* pAP);
void wld_station_stats_done(T_AccessPoint* pAP, bool success);
bool wld_ap_getDesiredState(T_AccessPoint* pAp);
void wld_ap_updateDiscoveryMethod(T_AccessPoint* pAP);
wld_ap_dm_m wld_ap_getDiscoveryMethod(T_AccessPoint* pAP);
T_ApNeighbour* wld_ap_findNeigh(T_AccessPoint* vap, const char* bssid_str);

void wld_ap_doSync(T_AccessPoint* pAP);
void wld_ap_doWpsSync(T_AccessPoint* pAP);

#ifndef WLD_MAC_MAXBAN
#define WLD_MAC_MAXBAN 64
#endif

typedef struct {
    swl_macBin_t banList[WLD_MAC_MAXBAN];
    uint32_t staToBan;
    swl_macBin_t kickList[WLD_MAC_MAXBAN];
    uint32_t staToKick;
} wld_banlist_t;
void wld_apMacFilter_getBanList(T_AccessPoint* pAP, wld_banlist_t* banlist, bool includePf);
void wld_apMacFilter_setAddressList_pwf(void* priv, amxd_object_t* object, amxd_param_t* param, const amxc_var_t* const newValue);
swl_rc_ne wld_ap_getLastAssocReq(T_AccessPoint* pAP, const char* macStation, wld_vap_assocTableStruct_t** data);

swl_rc_ne wld_vap_registerExtModData(T_AccessPoint* pAP, uint32_t extModId, void* extModData, wld_extMod_deleteData_dcf deleteHandler);
void* wld_vap_getExtModData(T_AccessPoint* pAP, uint32_t extModId);
swl_rc_ne wld_vap_unregisterExtModData(T_AccessPoint* pAP, uint32_t extModId);
bool wld_vap_isDummyVap(T_AccessPoint* pAP);
void wld_vap_setNetdevIndex(T_AccessPoint* pAP, int32_t netDevIndex);

#endif /* __WLD_ACCESSPOINT_H__ */
