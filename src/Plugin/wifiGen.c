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

#include "wld/wld.h"
#include "wld/wld_radio.h"
#include "wld/wld_nl80211_api.h"
#include "wld/wld_rad_nl80211.h"
#include "wifiGen.h"
#include "wifiGen_rad.h"
#include "wifiGen_vap.h"
#include "wifiGen_ep.h"
#include "wifiGen_fsm.h"
#include "wifiGen_events.h"
#include "wifiGen_hapd.h"
#include "wifiGen_zwdfs.h"

#define ME "gen"

static bool s_init = false;
static vendor_t* s_vendor = NULL;

swl_rc_ne wifiGen_setDmnExecSettings(vendor_t* pVdr, const char* dmnName, wld_dmnMgt_dmnExecSettings_t* pCfg) {
    ASSERT_NOT_NULL(pVdr, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_STR(dmnName, SWL_RC_INVALID_PARAM, ME, "Empty");
    SAH_TRACEZ_INFO(ME, "%s: set daemon %s exec settings", pVdr->name, dmnName);
    if(swl_str_matches(dmnName, HOSTAPD_CMD)) {
        wifiGen_hapd_setGlobDmnSettings(pVdr, pCfg);
    }
    return SWL_RC_OK;
}

bool wifiGen_init() {
    ASSERT_FALSE(s_init, false, ME, "already initialized");
    SAH_TRACEZ_WARNING(ME, "Generic init");
    wld_daemonMonitorConf_t dmnMoniConf = {
        .enableParam = true,
        .instantRestartLimit = 3,
        .minRestartInterval = 5,
    };
    wld_dmn_setMonitorConf(&dmnMoniConf);

    T_CWLD_FUNC_TABLE fta;
    memset(&fta, 0, sizeof(T_CWLD_FUNC_TABLE));

    //Misc functions
    fta.mfn_misc_has_support = wifiGen_rad_miscHasSupport;
    fta.mfn_wvdr_setDmnExecSettings = wifiGen_setDmnExecSettings;

    //Wrad functions
    fta.mfn_wrad_create_hook = wifiGen_rad_createHook;
    fta.mfn_wrad_destroy_hook = wifiGen_rad_destroyHook;
    fta.mfn_wrad_addVapExt = wifiGen_rad_addVapExt;
    fta.mfn_wrad_delvapif = wifiGen_rad_delvapif;
    fta.mfn_wrad_addvapif = wifiGen_rad_addvapif;
    fta.mfn_wrad_addendpointif = wifiGen_rad_addEndpointIf;
    fta.mfn_wrad_delendpointif = wifiGen_rad_delendpointif;
    fta.mfn_wrad_generateVapIfName = wifiGen_rad_generateVapIfName;
    fta.mfn_wrad_generateEpIfName = wifiGen_rad_generateEpIfName;
    fta.mfn_wrad_poschans = wifiGen_rad_poschans;
    fta.mfn_wrad_supports = wifiGen_rad_supports;
    fta.mfn_wrad_radio_status = wifiGen_rad_status;
    fta.mfn_wrad_enable = wifiGen_rad_enable;
    fta.mfn_wrad_sync = wifiGen_rad_sync;
    fta.mfn_wrad_secDmn_restart = wifiGen_rad_restart;
    fta.mfn_wrad_secDmn_refresh = wifiGen_rad_refresh;
    fta.mfn_wrad_toggle = wifiGen_rad_toggle;
    fta.mfn_wrad_regdomain = wifiGen_rad_regDomain;
    fta.mfn_wrad_txpow = wifiGen_rad_txpow;
    fta.mfn_wrad_setChanspec = wifiGen_rad_setChanspec;
    fta.mfn_wrad_getChanspec = wifiGen_rad_getChanspec;
    fta.mfn_wrad_antennactrl = wifiGen_rad_antennactrl;
    fta.mfn_wrad_stamode = wifiGen_rad_staMode;
    fta.mfn_wrad_supstd = wifiGen_rad_supstd;
    fta.mfn_wrad_stats = wifiGen_rad_stats;
    fta.mfn_wrad_fsm_delay_commit = wifiGen_rad_delayedCommitUpdate;
    fta.mfn_wrad_start_scan = wld_rad_nl80211_startScan;
    fta.mfn_wrad_stop_scan = wld_rad_nl80211_abortScan;
    fta.mfn_wrad_scan_results = wifiGen_rad_getScanResults;
    fta.mfn_wrad_airstats = wifiGen_rad_getAirStats;
    fta.mfn_wrad_getspectruminfo = wifiGen_rad_getSpectrumInfo;
    fta.mfn_wrad_getCurrentTxPow_dBm = wifiGen_rad_getTxPowerdBm;
    fta.mfn_wrad_getMaxTxPow_dBm = wifiGen_rad_getMaxTxPowerdBm;

    /* Wrad BG DFS fta control */
    fta.mfn_wrad_bgdfs_start_ext = wifiGen_rad_bgDfsStartExt;
    fta.mfn_wrad_bgdfs_stop = wifiGen_rad_bgDfsStop;

    /* Wrad ZW DFS fta control */
    fta.mfn_wrad_zwdfs_start = wifiGen_zwdfs_start;
    fta.mfn_wrad_zwdfs_stop = wifiGen_zwdfs_stop;

    //vap functions
    fta.mfn_wvap_create_hook = wifiGen_vap_createHook;
    fta.mfn_wvap_destroy_hook = wifiGen_vap_destroyHook;
    fta.mfn_wvap_bssid = wifiGen_vap_bssid;
    fta.mfn_wvap_ssid = wifiGen_vap_ssid;
    fta.mfn_wvap_status = wifiGen_vap_status;
    fta.mfn_wvap_enable = wifiGen_vap_enable;
    fta.mfn_wvap_sync = wifiGen_vap_sync;
    fta.mfn_wvap_get_station_stats = wifiGen_vap_getStationStats;
    fta.mfn_wvap_get_single_station_stats = wifiGen_vap_getSingleStationStats;
    fta.mfn_wvap_update_rssi_stats = wifiGen_vap_updateRssiStats;
    fta.mfn_wvap_sec_sync = wifiGen_vap_sec_sync;
    fta.mfn_wvap_transfer_sta = wifiGen_vap_sta_transfer;
    fta.mfn_wvap_sendManagementFrame = wifiGen_vap_sendManagementFrame;
    fta.mfn_wvap_mf_sync = wifiGen_vap_mf_sync;
    fta.mfn_wvap_pf_sync = wifiGen_vap_mf_sync;
    fta.mfn_wvap_wps_sync = wifiGen_vap_wps_sync;
    fta.mfn_wvap_wps_enable = wifiGen_vap_wps_enable;
    fta.mfn_wvap_wps_label_pin = wifiGen_vap_wps_labelPin;
    fta.mfn_wvap_kick_sta = wifiGen_vap_kick_sta;
    fta.mfn_wvap_kick_sta_reason = wifiGen_vap_kick_sta_reason;
    fta.mfn_wvap_disassoc_sta_reason = wifiGen_vap_disassoc_sta_reason;
    fta.mfn_wvap_multiap_update_type = wifiGen_vap_multiap_update_type;
    fta.mfn_wvap_multiap_update_profile = wifiGen_vap_multiap_update_profile;
    fta.mfn_wvap_multiap_update_vlanid = wifiGen_vap_multiap_update_vlanid;
    fta.mfn_wvap_setMboDenyReason = wifiGen_vap_setMboDisallowReason;
    fta.mfn_wvap_update_ap_stats = wifiGen_vap_updateApStats;
    fta.mfn_wvap_request_rrm_report = wifiGen_vap_requestRrmReport;
    fta.mfn_wvap_setEvtHandlers = wifiGen_setVapEvtHandlers;
    fta.mfn_wvap_updated_neighbour = wifiGen_vap_updated_neighbor;
    fta.mfn_wvap_deleted_neighbour = wifiGen_vap_deleted_neighbor;
    fta.mfn_wvap_set_discovery_method = wifiGen_vap_setDiscoveryMethod;
    fta.mfn_wvap_setMldUnit = wifiGen_vap_setMldUnit;

    //endpoint functions
    fta.mfn_wendpoint_create_hook = wifiGen_ep_createHook;
    fta.mfn_wendpoint_destroy_hook = wifiGen_ep_destroyHook;
    fta.mfn_wendpoint_enable = wifiGen_ep_enable;
    fta.mfn_wendpoint_connect_ap = wifiGen_ep_connectAp;
    fta.mfn_wendpoint_disconnect = wifiGen_ep_disconnect;
    fta.mfn_wendpoint_bssid = wifiGen_ep_bssid;
    fta.mfn_wendpoint_stats = wifiGen_ep_stats;
    fta.mfn_wendpoint_status = wifiGen_ep_status;
    fta.mfn_wendpoint_wps_start = wifiGen_ep_wpsStart;
    fta.mfn_wendpoint_wps_cancel = wifiGen_ep_wpsCancel;
    fta.mfn_wendpoint_multiap_enable = wifiGen_ep_multiApEnable;
    fta.mfn_wendpoint_sendManagementFrame = wifiGen_ep_sendManagementFrame;
    fta.mfn_wendpoint_set_mac_address = wifiGen_ep_setMacAddress;
    fta.mfn_wendpoint_update = wifiGen_ep_update;

    s_vendor = wld_nl80211_registerVendor(&fta);
    ASSERT_NOT_NULL(s_vendor, false, ME, "NULL vendor");
    wld_nl80211_getSharedState();
    wifiGen_fsm_doInit(s_vendor);

    return true;
}

int wifiGen_addRadios() {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_NOT_NULL(s_vendor, SWL_RC_INVALID_PARAM, ME, "NULL");
    size_t maxWiphys = MAXNROF_RADIO;
    size_t maxWlIfaces = MAXNROF_ACCESSPOINT;
    wld_nl80211_ifaceInfo_t wlIfacesInfo[maxWiphys][maxWlIfaces];
    memset(wlIfacesInfo, 0, sizeof(wlIfacesInfo));

    // get current wl ifaces created by the driver
    rc = wld_nl80211_getInterfaces(maxWiphys, maxWlIfaces, wlIfacesInfo);
    ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "fail to get nl80211 interfaces");

    // add missing default wl ifaces (to cover all wiphy bands)
    rc = wld_nl80211_addDefaultWiphyInterfacesExt("wlan", maxWiphys, maxWlIfaces, wlIfacesInfo);
    ASSERT_EQUALS(rc, SWL_RC_OK, rc, ME, "Fail to check default wl ifaces");

    // register unassigned radio devices to genPlugin vendor
    uint8_t index = wld_rad_nl80211_addRadios(s_vendor, maxWiphys, maxWlIfaces, wlIfacesInfo);
    ASSERTW_NOT_EQUALS(index, 0, SWL_RC_ERROR, ME, "NO Wireless interface found");

    return SWL_RC_OK;
}

#define NEW_WIPHY_HANDLE_DELAY 3000

static struct globWiphyMonCtx_s {
    wld_nl80211_listener_t* listener;
    wifiGen_wiphyDevReadyCb_f fcb;
    void* userdata;
    amxp_timer_t* pTimer;
} gGlobWiphyMoniCtx = {NULL, NULL, NULL, NULL};

static void s_handleNewWiphy(amxp_timer_t* timer _UNUSED, void* userdata) {
    struct globWiphyMonCtx_s* ctx = (struct globWiphyMonCtx_s*) userdata;
    ASSERT_NOT_NULL(ctx, , ME, "NULL");
    if(!wld_countRadios()) {
        SAH_TRACEZ_WARNING(ME, "first wiphy detection: start adding gen radios");
        SWL_CALL(ctx->fcb, ctx->userdata);
    } else {
        SAH_TRACEZ_INFO(ME, "other radios are registered");
    }
}

static void s_newWiphyInfoEvtCb(void* pRef, void* pData _UNUSED, wld_nl80211_wiphyInfo_t* pWiphyInfo) {
    struct globWiphyMonCtx_s* ctx = (struct globWiphyMonCtx_s*) pRef;
    ASSERT_NOT_NULL(ctx, , ME, "NULL");
    ASSERT_NOT_NULL(pWiphyInfo, , ME, "NULL");
    SAH_TRACEZ_WARNING(ME, "wiphy id:%d name:%s is added", pWiphyInfo->wiphy, pWiphyInfo->name);
    //give time to driver to finalize default wl interface creation
    SAH_TRACEZ_INFO(ME, "delay processing new wl ifaces");
    amxp_timer_start(ctx->pTimer, NEW_WIPHY_HANDLE_DELAY);
}

swl_rc_ne wifiGen_waitForGenRadios(void* userData, wifiGen_wiphyDevReadyCb_f fcb) {
    if((wld_nl80211_countWiphyFromFS() > 0) || (wld_countRadios() > 0)) {
        SWL_CALL(fcb, userData);
        return SWL_RC_DONE;
    }
    ASSERT_NOT_NULL(fcb, SWL_RC_INVALID_PARAM, ME, "missing callback");
    gGlobWiphyMoniCtx.fcb = fcb;
    if(gGlobWiphyMoniCtx.listener == NULL) {
        SAH_TRACEZ_WARNING(ME, "no vdr radio neither wiphy dev: WAIT FOR DETECTION");
        wld_nl80211_evtHandlers_cb handlers;
        memset(&handlers, 0, sizeof(handlers));
        handlers.fNewWiphyCb = s_newWiphyInfoEvtCb;
        gGlobWiphyMoniCtx.listener = wld_nl80211_addGlobalEvtListener(wld_nl80211_getSharedState(), &gGlobWiphyMoniCtx, NULL, &handlers);
        amxp_timer_new(&gGlobWiphyMoniCtx.pTimer, s_handleNewWiphy, &gGlobWiphyMoniCtx);
        ASSERT_NOT_NULL(gGlobWiphyMoniCtx.listener, SWL_RC_ERROR, ME, "fail to add global listener");
    }
    return SWL_RC_CONTINUE;
}

bool wifiGen_deinit() {
    ASSERT_TRUE(s_init, false, ME, "already de-initialized");
    wld_nl80211_delEvtListener(&gGlobWiphyMoniCtx.listener);
    amxp_timer_delete(&gGlobWiphyMoniCtx.pTimer);
    s_init = false;
    return true;
}

