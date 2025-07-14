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

#ifndef __WLD_RADIO_H__
#define __WLD_RADIO_H__


#include "swla/swla_trilean.h"
#include "wld.h"
#include "wld_channel.h"
#include "wld_radioOperatingStandards.h"
#include "wld_extMod.h"
#include "wld_rad_macCfg.h"

typedef enum {
    WLD_RAD_CHANGE_INIT,
    WLD_RAD_CHANGE_DESTROY,
    WLD_RAD_CHANGE_POSCHAN,
    WLD_RAD_CHANGE_CHANSPEC,
    WLD_RAD_CHANGE_MAX
} wld_rad_changeEvent_e;

typedef struct {
    wld_rad_changeEvent_e changeType;
    T_Radio* pRad;
    void* changeData;
} wld_rad_changeEvent_t;

typedef struct {
    T_Radio* pRad;
    swl_bit8_t* frameData;
    size_t frameLen;
    int32_t frameRssi;
} wld_rad_frameEvent_t;

T_Radio* wld_getRadioDataHandler(amxd_object_t* pobj, const char* rn);

void wld_radio_updateAntenna(T_Radio* pRad);
void wld_radio_updateAntennaExt(T_Radio* pRad, amxd_trans_t* trans);
wld_ap_dm_m wld_rad_getDiscoveryMethod(T_Radio* pR);
void wld_rad_updateDiscoveryMethod6GHz();

void syncData_Radio2OBJ(amxd_object_t* object, T_Radio* pR, int set);
void syncData_VendorWPS2OBJ(amxd_object_t* object, T_Radio* pR, int set);

amxd_status_t _addVAPIntf(amxd_object_t* obj,
                          amxd_function_t* func,
                          amxc_var_t* args,
                          amxc_var_t* ret);

amxd_status_t _delVAPIntf(amxd_object_t* obj,
                          amxd_function_t* func,
                          amxc_var_t* args,
                          amxc_var_t* ret);

amxd_status_t _wps_DefParam_wps_GenSelfPIN(amxd_object_t* obj,
                                           amxd_function_t* func _UNUSED,
                                           amxc_var_t* args,
                                           amxc_var_t* ret _UNUSED);

amxd_status_t _checkWPSPIN(amxd_object_t* obj,
                           amxd_function_t* func,
                           amxc_var_t* args,
                           amxc_var_t* ret);

extern amxc_llist_t g_radios;

/* DEBUG !!! FIX ME */
amxd_status_t _FSM_Start(amxd_object_t* wifi,
                         amxd_function_t* func,
                         amxc_var_t* args,
                         amxc_var_t* ret);

T_AccessPoint* wld_rad_getFirstVap(T_Radio* pR);
T_EndPoint* wld_rad_getFirstEp(T_Radio* pR);
T_AccessPoint* wld_rad_vap_from_name(T_Radio* pR, const char* ifname);
T_AccessPoint* wld_rad_vap_from_wds_name(T_Radio* pR, const char* ifname);
T_EndPoint* wld_rad_ep_from_name(T_Radio* pR, const char* ifname);
T_AccessPoint* wld_vap_from_name(const char* ifname);
T_EndPoint* wld_vep_from_name(const char* ifname);
T_Radio* wld_rad_from_name(const char* ifname);
T_Radio* wld_rad_fromObj(amxd_object_t* radObj);
bool wld_rad_isRadObj(amxd_object_t* radObj);
T_Radio* wld_rad_prevRadFromObj(amxd_object_t* radObj);
T_Radio* wld_rad_prevRadFromList(T_Radio* pRad);
T_Radio* wld_rad_nextRadFromObj(amxd_object_t* radObj);
T_Radio* wld_rad_nextRadFromList(T_Radio* pRad);
T_AccessPoint* wld_rad_getFirstEnabledVap(T_Radio* pR);
uint32_t wld_rad_countEnabledVaps(T_Radio* pR);
wld_mbssidAdvertisement_mode_e wld_rad_getMbssidAdsMode(T_Radio* pRad);
bool wld_rad_hasMbssidAds(T_Radio* pRad);

T_AccessPoint* wld_rad_getVapByIndex(T_Radio* pRad, int index);
T_EndPoint* wld_rad_getEpByIndex(T_Radio* pRad, int index);

amxd_object_t* ssid_create(amxd_object_t* pBus, T_Radio* pR, const char* name);
bool radio_scanresults_to_variant(T_Radio* radio, amxc_var_t* variant_out);
bool wld_radio_notify_scanresults(amxd_object_t* obj);
bool wld_radio_stats_to_variant(T_Radio* a, T_Stats* stats, amxc_var_t* map);
bool wld_radio_scanresults_find(T_Radio* pR, const char* ssid, wld_scanResultSSID_t* output);
bool wld_radio_scanresults_cleanup(wld_scanResults_t* results);

void wld_scan_init(T_Radio* pR);
void wld_scan_destroy(T_Radio* pR);
void wld_scan_done(T_Radio* pR, bool success);
bool wld_scan_isRunning(T_Radio* pR);
swl_rc_ne wld_scan_start(T_Radio* pRad, wld_scan_type_e type, const char* reason);
swl_rc_ne wld_scan_stop(T_Radio* pRad);
swl_rc_ne wld_scan_updateChanimInfo(T_Radio* pRad);
void wld_scan_cleanupScanResultSSID(wld_scanResultSSID_t* ssid);
void wld_scan_cleanupScanResults(wld_scanResults_t* res);
void wld_cleanupSurveyReport(wld_surveyReport_t* res);
void wld_spectrum_cleanupResults(T_Radio* pR);
T_Radio* wld_getRadioByFrequency(swl_freqBand_e freqBand);
uint32_t wld_getNrApMldLinksById(int32_t id);

int wld_rad_init_cap(T_Radio* pR);
void wld_rad_updateCapabilities(T_Radio* pR, amxd_trans_t* trans);
void wld_rad_writeCapStatus(T_Radio* pR, amxd_trans_t* trans);
bool wld_rad_is_cap_enabled(T_Radio* pR, int capability);
bool wld_rad_is_cap_active(T_Radio* pR, int capability);

void wld_rad_addSuppDrvCap(T_Radio* pRad, swl_freqBand_e suppBand, char* suppCap);
swl_trl_e wld_rad_findSuppDrvCap(T_Radio* pRad, swl_freqBand_e suppBand, char* suppCap);
const char* wld_rad_getSuppDrvCaps(T_Radio* pRad, swl_freqBand_e suppBand);
void wld_rad_clearSuppDrvCaps(T_Radio* pRad);

bool wld_rad_hasWeatherChannels(T_Radio* pRad);
bool wld_rad_hasChannel(T_Radio* pRad, int chan);
void do_updateOperatingChannelBandwidth5GHz(T_Radio* pRad);
void wld_rad_updateChannelsInUse(T_Radio* pRad);
bool wld_rad_hasChannelWidthCovered(T_Radio* pRad, swl_bandwidth_e chW);
wld_channel_extensionPos_e wld_rad_getExtensionChannel(T_Radio* pRad);

bool wld_rad_hasEnabledEp(T_Radio* pRad);
bool wld_rad_hasConnectedEp(T_Radio* pRad);
bool wld_rad_hasOnlyActiveEP(T_Radio* pRad);
bool wld_rad_hasMainEP(T_Radio* pRad);

bool wld_rad_areAllVapsDone(T_Radio* pRad);
bool wld_rad_hasActiveVap(T_Radio* pRad);
bool wld_rad_hasEnabledVap(T_Radio* pRad);
bool wld_rad_hasActiveEp(T_Radio* pRad);

bool wld_rad_hasEnabledIface(T_Radio* pRad);
bool wld_rad_hasActiveIface(T_Radio* pRad);
uint32_t wld_rad_countIfaces(T_Radio* pRad);
uint32_t wld_rad_countVapIfaces(T_Radio* pRad);
uint32_t wld_rad_countEpIfaces(T_Radio* pRad);
uint32_t wld_rad_countMappedAPs(T_Radio* pRad);
uint32_t wld_rad_countAPsByAutoMacSrc(T_Radio* pRad, wld_autoMacSrc_e autoMacSrc);
uint32_t wld_rad_countWiphyRads(uint32_t wiphy);
bool wld_rad_hasLinkIfIndex(T_Radio* pRad, int32_t ifIndex);
bool wld_rad_hasLinkIfName(T_Radio* pRad, const char* ifName);
bool wld_rad_isMloCapable(T_Radio* pRad);
bool wld_rad_hasMloSupport(T_Radio* pRad);
bool wld_rad_hasActiveApMldMultiLink(T_Radio* pRad);
bool wld_rad_hasActiveApMld(T_Radio* pRad, uint32_t minNLinks);
void wld_rad_setAllMldLinksUnconfigured(T_Radio* pRad);
bool wld_rad_hasUsableApMld(T_Radio* pRad, uint32_t minNLinks);

uint32_t wld_rad_getFirstEnabledIfaceIndex(T_Radio* pRad);
uint32_t wld_rad_getFirstActiveIfaceIndex(T_Radio* pRad);
T_AccessPoint* wld_rad_getFirstActiveAp(T_Radio* pRad);
T_AccessPoint* wld_rad_getFirstBroadcastingAp(T_Radio* pRad);
T_EndPoint* wld_rad_getFirstActiveEp(T_Radio* pRad);

bool wld_rad_isChannelSubset(T_Radio* pRad, uint8_t* chanlist, int size);

bool wld_rad_is_6ghz(const T_Radio* pR);
bool wld_rad_is_5ghz(const T_Radio* pR);
bool wld_rad_is_24ghz(const T_Radio* pRad);
bool wld_rad_is_on_dfs(T_Radio* pR);
bool wld_rad_isDoingDfsScan(T_Radio* pRad);
bool wld_rad_isUpAndReady(T_Radio* pRad);
bool wld_rad_isUpExt(T_Radio* pRad);

void wld_rad_write_possible_channels(T_Radio* pRad);
bool wld_rad_addDFSEvent(T_Radio* pR, T_DFSEvent* evt);
swl_chanspec_t wld_rad_getSwlChanspec(T_Radio* pRad);
uint32_t wld_rad_getCurrentFreq(T_Radio* pRad);
swl_rc_ne wld_rad_getCurrentNoise(T_Radio* pRad, int32_t* pNoise);

void wld_rad_triggerDelayCommit(T_Radio* pRad, uint32_t delay, bool restartIfActive);
int wld_rad_doRadioCommit(T_Radio* pR);
void wld_rad_doSync(T_Radio* pRad);
int wld_rad_doCommitIfUnblocked(T_Radio* pR);
bool wld_rad_has_endpoint_enabled(T_Radio* rad);
T_EndPoint* wld_rad_getEnabledEndpoint(T_Radio* rad);
bool wld_rad_hasRunningEndpoint(T_Radio* rad);
T_EndPoint* wld_rad_getRunningEndpoint(T_Radio* rad);
bool wld_rad_hasWpsActiveEndpoint(T_Radio* rad);
T_EndPoint* wld_rad_getWpsActiveEndpoint(T_Radio* rad);
void wld_rad_updateActiveDevices(T_Radio* pRad, amxd_trans_t* trans);

T_Radio* wld_rad_get_radio(const char* ifname);
void wld_rad_chan_update_model(T_Radio* pRad, amxd_trans_t* trans);
void wld_rad_updateOperatingClass(T_Radio* pRad);

void wld_rad_init_counters(T_Radio* pRad, T_EventCounterList* counters, const char** defaults);
void wld_rad_increment_counter(T_Radio* pRad, T_EventCounterList* counters, uint32_t counterIndex, const char* info);
void wld_rad_incrementCounterStr(T_Radio* pRad, T_EventCounterList* counters, uint32_t index, const char* template, ...);

void wld_radio_copySsidsToResult(wld_scanResults_t* results, amxc_llist_t* ssid_list);

T_AssociatedDevice* wld_rad_getAssociatedDevice(T_Radio* pRad, swl_macBin_t* macBin);
bool wld_rad_isAvailable(T_Radio* pRad);
bool wld_rad_isActive(T_Radio* pRad);
bool wld_rad_isUp(T_Radio* pRad);
int wld_rad_getSocket(T_Radio* rad);
void wld_rad_resetStatusHistogram(T_Radio* pRad);
void wld_rad_updateState(T_Radio* pRad, bool forceVapUpdate);

T_AccessPoint* wld_radio_getVapFromRole(T_Radio* pRad, wld_apRole_e role);
swl_freqBand_e wld_rad_getFreqBand(T_Radio* pRad);

T_AccessPoint* wld_rad_firstAp(T_Radio* pRad);
T_AccessPoint* wld_rad_nextAp(T_Radio* pRad, T_AccessPoint* pAP);
T_EndPoint* wld_rad_firstEp(T_Radio* pRad);
T_EndPoint* wld_rad_nextEp(T_Radio* pRad, T_EndPoint* pEP);
T_Radio* wld_rad_fromIt(amxc_llist_it_t* it);
void wld_rad_triggerChangeEvent(T_Radio* pRad, wld_rad_changeEvent_e event, void* data);
void wld_rad_triggerFrameEvent(T_Radio* pRad, swl_bit8_t* frame, size_t frameLen, int32_t rssi);

/**
 * Add ext module data registration for this radio.
 */
swl_rc_ne wld_rad_registerExtModData(T_Radio* pRad, uint32_t extModId, void* extModData, wld_extMod_deleteData_dcf deleteHandler);

/**
 * Retrieve ext module data from radio
 */
void* wld_rad_getExtModData(T_Radio* pRad, uint32_t extModId);

/**
 * remove a registration. Note this will NOT free the registration object, just remove its link with the radio.
 * The external module must still free the structure.
 */
swl_rc_ne wld_rad_unregisterExtModData(T_Radio* pRad, uint32_t extModId);

#define wld_rad_forEachAp(apPtr, radPtr) \
    for(apPtr = wld_rad_firstAp(radPtr); apPtr; apPtr = wld_rad_nextAp(radPtr, apPtr))
#define wld_rad_forEachEp(epPtr, radPtr) \
    for(epPtr = wld_rad_firstEp(radPtr); epPtr; epPtr = wld_rad_nextEp(radPtr, epPtr))

amxd_object_t* wld_rad_getObject(T_Radio* pRad);
bool wld_rad_firstCommitFinished(T_Radio* pRad);
#endif /* __WLD_RADIO_H__ */
