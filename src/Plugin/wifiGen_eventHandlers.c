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
#include "wifiGen_events.h"
#include "wifiGen_hapd.h"
#include "wifiGen_rad.h"
#include "wifiGen_fsm.h"
#include "wifiGen_ep.h"
#include "wifiGen_mld.h"
#include "wld/wld_wpaCtrlMngr.h"
#include "wld/wld_wpaCtrl_api.h"
#include "wld/wld_rad_nl80211.h"
#include "wld/wld_ap_nl80211.h"
#include "wld/wld_rad_hostapd_api.h"
#include "wld/wld_wpaSupp_ep_api.h"
#include "wld/wld_secDmn.h"
#include "wld/wld_linuxIfUtils.h"
#include "wld/wld_accesspoint.h"
#include "wld/wld_wps.h"
#include "wld/wld_assocdev.h"
#include "wld/wld_util.h"
#include "wld/wld_radio.h"
#include "wld/wld_endpoint.h"
#include "wld/wld_ssid.h"
#include "wld/wld_chanmgt.h"
#include "wld/wld_sensing.h"
#include "wld/wld_eventing.h"
#include "wld/Utils/wld_autoNeighAdd.h"
#include "swl/swl_hex.h"
#include "swl/swl_ieee802_1x_defs.h"
#include "swl/swl_genericFrameParser.h"
#include "swla/swla_chanspec.h"
#include <amxc/amxc.h>
#include <errno.h>

#define ME "genEvt"

/* Table of standard wpactrl iface events to be forwarded to other applications */
static const char* s_fwdIfaceStdWpaCtrlEventsList[] = {
    "AP-STA-POSSIBLE-PSK-MISMATCH",
    "CTRL-EVENT-SAE-UNKNOWN-PASSWORD-IDENTIFIER",
    "CTRL-EVENT-EAP-FAILURE",
    "CTRL-EVENT-EAP-TIMEOUT-FAILURE",
    // WPA EAP failure2/timeout2 events while using an external RADIUS server
    "CTRL-EVENT-EAP-FAILURE2",
    "CTRL-EVENT-EAP-TIMEOUT-FAILURE2",
    "BEACON-REQ-TX-STATUS",
    "BEACON-RESP-RX"
};

/* Table of standard wpactrl radio events to be forwarded to other applications */
static const char* s_fwdRadioStdWpaCtrlEventsList[] = {
    "CTRL-EVENT-CHANNEL-SWITCH",
    "AP-CSA-FINISHED",
    "DFS-RADAR-DETECTED",
    "DFS-CAC-START",
    "DFS-CAC-COMPLETED",
    "DFS-NOP-FINISHED"
};

static void s_notifyWpaCtrlEvent(amxd_object_t* object, char* ifName, char* eventName,
                                 const char** eventList, size_t eventListSize, char* msgData) {
    ASSERTS_NOT_NULL(object, , ME, "NULL");
    for(size_t i = 0; i < eventListSize; i++) {
        if(!swl_str_matches(eventName, eventList[i])) {
            continue;
        }
        amxc_var_t eventData;
        amxc_var_init(&eventData);
        amxc_var_set_type(&eventData, AMXC_VAR_ID_HTABLE);
        amxc_var_add_key(cstring_t, &eventData, "ifName", ifName);
        amxc_var_add_key(cstring_t, &eventData, "eventData", msgData);

        SAH_TRACEZ_INFO(ME, "%s: notify wpaCtrlEvents eventData=%s", ifName, msgData);

        amxd_object_trigger_signal(object, "wpaCtrlEvents", &eventData);
        amxc_var_clean(&eventData);
    }
}

static void s_wpaCtrlIfaceStdEvt(void* userData, char* ifName, char* eventName, char* msgData) {
    ASSERT_NOT_NULL(ifName, , ME, "NULL");
    ASSERT_NOT_NULL(eventName, , ME, "NULL");
    ASSERT_NOT_NULL(msgData, , ME, "NULL");

    amxd_object_t* object = NULL;
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    T_EndPoint* pEP = (T_EndPoint*) userData;
    if(debugIsVapPointer(pAP)) {
        object = pAP->pBus;
    } else if(debugIsEpPointer(pEP)) {
        object = pEP->pBus;
    }

    ASSERTS_NOT_NULL(object, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: received wpaCtrl iface standard event %s", ifName, eventName);

    s_notifyWpaCtrlEvent(object, ifName, eventName, s_fwdIfaceStdWpaCtrlEventsList,
                         SWL_ARRAY_SIZE(s_fwdIfaceStdWpaCtrlEventsList), msgData);
}

static void s_wpaCtrlRadioStdEvt(void* userData, char* ifName, char* eventName, char* msgData) {
    ASSERT_NOT_NULL(ifName, , ME, "NULL");
    ASSERT_NOT_NULL(eventName, , ME, "NULL");
    ASSERT_NOT_NULL(msgData, , ME, "NULL");

    T_Radio* pRad = (T_Radio*) userData;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    ASSERTS_NOT_NULL(pRad->pBus, , ME, "NULL");

    ASSERTS_TRUE(swl_str_matches(pRad->Name, ifName), , ME, "invalid ifName %s", ifName);

    SAH_TRACEZ_INFO(ME, "%s: received wpaCtrl radio standard event %s", ifName, eventName);

    s_notifyWpaCtrlEvent(pRad->pBus, ifName, eventName, s_fwdRadioStdWpaCtrlEventsList,
                         SWL_ARRAY_SIZE(s_fwdRadioStdWpaCtrlEventsList), msgData);
}

static void s_saveChanChanged(T_Radio* pRad, swl_chanspec_t* pChanSpec, wld_channelChangeReason_e reason) {
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    ASSERTS_NOT_NULL(pChanSpec, , ME, "NULL");
    ASSERTS_TRUE(pChanSpec->channel > 0, , ME, "invalid chan");
    if(pRad->targetChanspec.reason == reason) {
        swl_chanspec_t chSpec = *pChanSpec;
        if(pRad->targetChanspec.chanspec.extensionHigh == SWL_CHANSPEC_EXT_AUTO) {
            chSpec.extensionHigh = SWL_CHANSPEC_EXT_AUTO;
        }
        if(!swl_typeChanspec_equals(pRad->targetChanspec.chanspec, chSpec)) {
            SAH_TRACEZ_WARNING(ME, "%s: target chspec %s is not that requested %s (reason %s): reset curr chan to force notif",
                               pRad->Name, swl_typeChanspecExt_toBuf32Ref(pChanSpec).buf,
                               swl_typeChanspecExt_toBuf32(pRad->targetChanspec.chanspec).buf,
                               g_wld_channelChangeReason_str[reason]);
            if(swl_typeChanspec_equals(pRad->currentChanspec.chanspec, *pChanSpec)) {
                // reset saved CurrChannel to force considering chanspec read from driver
                pRad->currentChanspec.chanspec.channel = 0;
            }

            if(wld_channel_is_radar_detected(pRad->currentChanspec.chanspec)) {
                reason = CHAN_REASON_DFS;
            } else {
                // reset saved tgtChannel to force sync with chanspec read from driver
                pRad->targetChanspec.chanspec.channel = 0;
                reason = CHAN_REASON_INITIAL;
            }
        }
    }
    swl_rc_ne rc = wld_chanmgt_reportCurrentChanspec(pRad, *pChanSpec, reason);
    ASSERTS_FALSE(rc < SWL_RC_OK, , ME, "no changes to be reported");
    wld_rad_updateOperatingClass(pRad);
    swl_chanspec_t savedChSpec = wld_rad_hostapd_getCfgChanspec(pRad);
    if(!swl_typeChanspecExt_equals(pRad->targetChanspec.chanspec, savedChSpec)) {
        SAH_TRACEZ_WARNING(ME, "%s: saved chanspec %s different than tgt %s", pRad->Name,
                           swl_typeChanspecExt_toBuf32(savedChSpec).buf,
                           swl_typeChanspecExt_toBuf32(pRad->targetChanspec.chanspec).buf);
    }
}

static void s_syncCurrentChannel(T_Radio* pRad, wld_channelChangeReason_e reason) {
    swl_chanspec_t chanSpec;
    ASSERTS_FALSE(pRad->pFA->mfn_wrad_getChanspec(pRad, &chanSpec) < SWL_RC_OK, ,
                  ME, "%s: fail to get channel", pRad->Name);
    s_saveChanChanged(pRad, &chanSpec, reason);
}

static void s_chanSwitchCb(void* userData, char* ifName _UNUSED, swl_chanspec_t* chanSpec) {
    T_Radio* pRad = (T_Radio*) userData;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    SAH_TRACEZ_WARNING(ME, "%s: channel switch event central_chan=%d bandwidth=%s band=%s ext=%d", pRad->Name,
                       chanSpec->channel, swl_bandwidth_str[chanSpec->bandwidth], swl_freqBandExt_str[chanSpec->band], chanSpec->extensionHigh);

    if(wld_rad_is_24ghz(pRad) && (wld_chanmgt_getTgtBw(pRad) != chanSpec->bandwidth) &&
       (wld_chanmgt_getCurBw(pRad) == SWL_BW_40MHZ) && (chanSpec->bandwidth == SWL_BW_20MHZ) && (pRad->currentChanspec.chanspec.channel == chanSpec->channel)) {
        pRad->targetChanspec.reason = CHAN_REASON_OBSS_COEX;
        pRad->obssCoexistenceActive = true;
    }

    s_saveChanChanged(pRad, chanSpec, pRad->targetChanspec.reason);
}

static void s_csaFinishedCb(void* userData, char* ifName _UNUSED, swl_chanspec_t* chanSpec) {
    T_Radio* pRad = (T_Radio*) userData;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: csa finished to new_chan=%d", pRad->Name, chanSpec->channel);
    wld_autoNeighAdd_radioSetDelNeighbourAP(pRad, true);
}

/*
 * update rad detailed state from hapd, when not yet set with endpoint (wpa_supp)
 * or with bg_dfs
 */
static bool s_saveHapdRadDetState(T_Radio* pRad, chanmgt_rad_state radDetState) {
    ASSERTS_NOT_EQUALS(radDetState, CM_RAD_UNKNOWN, false, ME, "unknown");
    ASSERTS_NOT_EQUALS(radDetState, pRad->detailedState, false, ME, "same");
    if((!wld_rad_isUpExt(pRad)) &&
       ((radDetState == CM_RAD_UP) ||
        (!wld_rad_isDoingDfsScan(pRad)) ||
        (pRad->detailedState == CM_RAD_FG_CAC))) {
        SAH_TRACEZ_INFO(ME, "%s: update detailedState %s with hapd %s", pRad->Name,
                        cstr_chanmgt_rad_state[pRad->detailedState],
                        cstr_chanmgt_rad_state[radDetState]);
        pRad->detailedState = radDetState;
    } else {
        SAH_TRACEZ_WARNING(ME, "%s: keep detailedState %s against hapd %s", pRad->Name,
                           cstr_chanmgt_rad_state[pRad->detailedState],
                           cstr_chanmgt_rad_state[radDetState]);
    }
    return (pRad->detailedState == radDetState);
}
static void s_mngrReadyCb(void* userData, char* ifName, bool isReady) {
    SAH_TRACEZ_WARNING(ME, "%s: wpactrl mngr is %s ready", ifName, (isReady ? "" : "not"));
    T_Radio* pRad = (T_Radio*) userData;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    T_AccessPoint* pAP = wld_rad_vap_from_name(pRad, ifName);
    T_EndPoint* pEP = wld_rad_ep_from_name(pRad, ifName);
    if(pAP != NULL) {
        if(!isReady) {
            wld_wpaCtrlMngr_t* pMgr = wld_secDmn_getWpaCtrlMgr(pRad->hostapd);
            if((pMgr != NULL) && (!wld_wpaCtrlMngr_isReady(pMgr)) && (wld_secDmn_isEnabled(pRad->hostapd))) {
                SAH_TRACEZ_WARNING(ME, "%s: hostapd still enabled: restarting wpactrl mgr connection", pRad->Name);
                wld_wpaCtrlMngr_connect(pMgr);
            }
            return;
        }
        //once we restart hostapd, previous dfs clearing must be reinitialized
        wifiGen_rad_initBands(pRad);
        pRad->pFA->mfn_wrad_poschans(pRad, NULL, 0);
        //when manager is started, radio chanspec is read from secDmn conf file (saved conf)
        s_syncCurrentChannel(pRad, pRad->targetChanspec.reason);
        //update rad status from hapd main iface state
        //when radio is not yet UP via EP connection, then update detailed state from hapd
        chanmgt_rad_state hapdRadDetState = CM_RAD_UNKNOWN;
        wifiGen_hapd_getRadState(pRad, &hapdRadDetState);
        s_saveHapdRadDetState(pRad, hapdRadDetState);
        bool isRadReady = (hapdRadDetState == CM_RAD_UP);
        if(isRadReady) {
            //we may missed the CAC Done event waiting to finalize wpactrl connection
            //so mark current chanspec as active
            wld_channel_clear_passive_band(wld_rad_getSwlChanspec(pRad));
        }
        CALL_SECDMN_MGR_EXT(pRad->hostapd, fSyncOnRadioUp, ifName, isRadReady);
        return;
    }
    ASSERTS_TRUE(isReady, , ME, "Not ready");
    if(pEP != NULL) {
        wld_epConnectionStatus_e connState = pEP->connectionStatus;
        wifiGen_ep_connStatus(pEP, &connState);
        if(connState == EPCS_CONNECTED) {
            CALL_INTF_EXT(pEP->wpaCtrlInterface, fStationConnectedCb, NULL, 0);
        } else {
            wld_endpoint_setConnectionStatus(pEP, connState, EPE_NONE);
        }
        CALL_SECDMN_MGR_EXT(pEP->wpaSupp, fSyncOnRadioUp, ifName, true);
        return;
    }
    wld_rad_updateState(pRad, false);
}

static void s_mainApSetupCompletedCb(void* userData, char* ifName) {
    SAH_TRACEZ_WARNING(ME, "%s: main AP has setup completed", ifName);
    T_Radio* pRad = (T_Radio*) userData;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    // when main iface setup is completed, current chanspec is the last applied target chanspec
    pRad->pFA->mfn_wrad_poschans(pRad, NULL, 0);
    s_syncCurrentChannel(pRad, pRad->targetChanspec.reason);
    wld_channel_clear_passive_band(wld_rad_getSwlChanspec(pRad));
    if(s_saveHapdRadDetState(pRad, CM_RAD_UP)) {
        CALL_SECDMN_MGR_EXT(pRad->hostapd, fSyncOnRadioUp, ifName, true);
    }
    wld_rad_updateState(pRad, true);
}

static void s_mainApDisabledCb(void* userData, char* ifName) {
    SAH_TRACEZ_WARNING(ME, "%s: main AP has been disabled", ifName);
    T_Radio* pRad = (T_Radio*) userData;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    //once we toggle hostapd, previous dfs clearing must be reinitialized
    wld_rad_updateState(pRad, true);
    if(pRad->detailedState == CM_RAD_DOWN) {
        wifiGen_rad_initBands(pRad);
    }
}

static void s_dfsCacStartedCb(void* userData, char* ifName, swl_chanspec_t* chanSpec, uint32_t cac_time) {
    SAH_TRACEZ_WARNING(ME, "%s: CAC started on channel %d for %d sec", ifName, chanSpec->channel, cac_time);
    T_Radio* pRad = (T_Radio*) userData;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    wld_channelChangeReason_e reason = pRad->targetChanspec.reason;
    if(wld_channel_is_radar_detected(pRad->currentChanspec.chanspec)) {
        reason = CHAN_REASON_DFS;
    }
    if(!wld_bgdfs_isRunning(pRad)) {
        s_syncCurrentChannel(pRad, reason);
    }
    wld_channel_mark_passive_band(*chanSpec);

    swl_chanspec_t curChanspec = wld_chanmgt_getCurChspec(pRad);
    swl_chanspec_t tgtChanspec = wld_chanmgt_getTgtChspec(pRad);

    if(swl_channel_isInChanspec(&curChanspec, chanSpec->channel)) {
        pRad->detailedState = CM_RAD_FG_CAC;
    } else if(swl_channel_isInChanspec(&tgtChanspec, chanSpec->channel)) {
        pRad->detailedState = CM_RAD_BG_CAC;
    } else {
        pRad->detailedState = CM_RAD_BG_CAC_NS;
    }
    wld_rad_updateState(pRad, false);
}

static void s_dfsCacDoneCb(void* userData, char* ifName, swl_chanspec_t* chanSpec, bool success) {
    SAH_TRACEZ_WARNING(ME, "%s: CAC finished on channel %d : %s", ifName, chanSpec->channel,
                       (success ? "completed" : "aborted"));
    //CAC completed
    T_Radio* pRad = (T_Radio*) userData;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    swl_chanspec_t currChanSpec = *chanSpec;
    currChanSpec.bandwidth = swl_radBw_toBw[pRad->runningChannelBandwidth];
    if(success) {
        wld_channel_clear_passive_band(currChanSpec);
        wld_chanmgt_writeDfsChannels(pRad);
    } else if(wld_rad_isDoingDfsScan(pRad) && swl_channel_isInChanspec(&currChanSpec, pRad->channel)) {
        // CAC aborted: so mark temporary radio as down, until fallback to alternative channel (eg: non-dfs)
        // Radio detailed status will then be rectified
        pRad->detailedState = CM_RAD_DOWN;
    }
    if(wld_bgdfs_isRunning(pRad)) {
        wld_bgdfs_notifyClearEnded(pRad, (success ? DFS_RESULT_OK : DFS_RESULT_OTHER));
    }
    wld_rad_updateState(pRad, false);
}

static void s_dfsCacExpiredCb(void* userData, char* ifName, swl_chanspec_t* chanSpec) {
    SAH_TRACEZ_WARNING(ME, "%s: Pre-CAC Expired on channel %d", ifName, chanSpec->channel);
    T_Radio* pRad = (T_Radio*) userData;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    wld_channel_mark_passive_channel(*chanSpec);
    wld_rad_updateState(pRad, false);
    wld_chanmgt_writeDfsChannels(pRad);
}

static void s_dfsRadarDetectedCb(void* userData, char* ifName, swl_chanspec_t* chanSpec) {
    SAH_TRACEZ_WARNING(ME, "%s: DFS Radar detected on channel %d bw %d", ifName, chanSpec->channel, chanSpec->bandwidth);
    T_Radio* pRad = (T_Radio*) userData;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    wld_channel_mark_radar_detected_band(*chanSpec);
    if(wld_bgdfs_isRunning(pRad)) {
        wld_bgdfs_notifyClearEnded(pRad, DFS_RESULT_RADAR);
    }
    wld_rad_updateState(pRad, false);
    wld_chanmgt_writeDfsChannels(pRad);
}

static void s_dfsNopFinishedCb(void* userData _UNUSED, char* ifName, swl_chanspec_t* chanSpec) {
    SAH_TRACEZ_WARNING(ME, "%s: DFS Non-Occupancy Period is over on channel %d", ifName, chanSpec->channel);
    wld_channel_clear_radar_detected_channel(*chanSpec);
    wld_channel_mark_passive_channel(*chanSpec);
    T_Radio* pRad = (T_Radio*) userData;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    wld_chanmgt_writeDfsChannels(pRad);
}

static void s_dfsNewChannelCb(void* userData _UNUSED, char* ifName, swl_chanspec_t* chanSpec) {
    SAH_TRACEZ_WARNING(ME, "%s: DFS-NEW-CHANNEL: %s", ifName, swl_typeChanspecExt_toBuf32Ref(chanSpec).buf);
}

static void s_fetchSockLinkIface(void* userData _UNUSED, wld_wpaCtrlMngr_t* pMgr, const char* sockName, char** ppLinkIfName) {
    ASSERT_NOT_NULL(ppLinkIfName, , ME, "NULL");
    swl_str_copyMalloc(ppLinkIfName, NULL);
    const char* pLinkIfName = wld_ssid_getIfName(wifiGen_mld_fetchSockLinkSSID(pMgr, sockName));
    ASSERTI_STR(sockName, , ME, "no link iface mapped to wpa sock (%s)", sockName);
    swl_str_copyMalloc(ppLinkIfName, pLinkIfName);
}

#define SET_HDLR(tgt, src) {tgt = (tgt ? : src); }

static swl_rc_ne s_setWpaCtrlRadEvtHandlers(wld_wpaCtrlMngr_t* wpaCtrlMngr, T_Radio* pRad) {
    ASSERT_NOT_NULL(wpaCtrlMngr, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_wpaCtrl_radioEvtHandlers_cb wpaCtrlRadEvthandlers;
    memset(&wpaCtrlRadEvthandlers, 0, sizeof(wpaCtrlRadEvthandlers));
    wld_wpaCtrlMngr_getEvtHandlers(wpaCtrlMngr, NULL, &wpaCtrlRadEvthandlers);
    //Set here the wpa_ctrl RAD event handlers
    SET_HDLR(wpaCtrlRadEvthandlers.fProcStdEvtMsg, s_wpaCtrlRadioStdEvt);
    SET_HDLR(wpaCtrlRadEvthandlers.fChanSwitchStartedCb, s_chanSwitchCb);
    SET_HDLR(wpaCtrlRadEvthandlers.fChanSwitchCb, s_chanSwitchCb);
    SET_HDLR(wpaCtrlRadEvthandlers.fApCsaFinishedCb, s_csaFinishedCb);
    SET_HDLR(wpaCtrlRadEvthandlers.fMngrReadyCb, s_mngrReadyCb);
    SET_HDLR(wpaCtrlRadEvthandlers.fMainApSetupCompletedCb, s_mainApSetupCompletedCb);
    SET_HDLR(wpaCtrlRadEvthandlers.fMainApDisabledCb, s_mainApDisabledCb);
    SET_HDLR(wpaCtrlRadEvthandlers.fDfsCacStartedCb, s_dfsCacStartedCb);
    SET_HDLR(wpaCtrlRadEvthandlers.fDfsCacDoneCb, s_dfsCacDoneCb);
    SET_HDLR(wpaCtrlRadEvthandlers.fDfsCacExpiredCb, s_dfsCacExpiredCb);
    SET_HDLR(wpaCtrlRadEvthandlers.fDfsRadarDetectedCb, s_dfsRadarDetectedCb);
    SET_HDLR(wpaCtrlRadEvthandlers.fDfsNopFinishedCb, s_dfsNopFinishedCb);
    SET_HDLR(wpaCtrlRadEvthandlers.fDfsNewChannelCb, s_dfsNewChannelCb);
    SET_HDLR(wpaCtrlRadEvthandlers.fFetchSockLinkIface, s_fetchSockLinkIface);
    wld_wpaCtrlMngr_setEvtHandlers(wpaCtrlMngr, pRad, &wpaCtrlRadEvthandlers);
    return SWL_RC_OK;
}

static uint32_t s_checkEnabledIfacesCreatedReady(wld_wpaCtrlMngr_t* pMgr, T_Radio* pRad) {
    uint32_t countEnabled = wld_wpaCtrlMngr_countEnabledInterfaces(pMgr);
    uint32_t countReady = 0;
    for(uint32_t i = 0; i < wld_wpaCtrlMngr_countInterfaces(pMgr); i++) {
        wld_wpaCtrlInterface_t* pWpaIface = wld_wpaCtrlMngr_getInterface(pMgr, i);
        if(!wld_wpaCtrlInterface_isReady(pWpaIface)) {
            continue;
        }
        const char* ifname = wld_wpaCtrlInterface_getName(pWpaIface);
        T_AccessPoint* pAP = NULL;
        T_EndPoint* pEP = NULL;

        if((((pAP = wld_rad_vap_from_name(pRad, ifname)) != NULL) && wld_ssid_isLinkCreated(pAP->pSSID)) ||
           (((pEP = wld_rad_ep_from_name(pRad, ifname)) != NULL) && wld_ssid_isLinkCreated(pEP->pSSID))) {
            countReady++;
        }
    }
    SAH_TRACEZ_INFO(ME, "%s: wpaIfaces enabled:%d createdReady:%d", pRad->Name, countEnabled, countReady);
    return (countReady >= countEnabled);
}

static void s_newInterfaceCb(void* pRef, void* pData _UNUSED, wld_nl80211_ifaceInfo_t* pIfaceInfo) {
    T_Radio* pRad = (T_Radio*) pRef;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    ASSERT_NOT_NULL(pIfaceInfo, , ME, "NULL");
    SAH_TRACEZ_NOTICE(ME, "%s: interface created (devIdx:%d)", pIfaceInfo->name, pIfaceInfo->ifIndex);
    if(swl_str_matches(pRad->Name, pIfaceInfo->name)) {
        pRad->index = pIfaceInfo->ifIndex;
        pRad->wDevId = pIfaceInfo->wDevId;
    }
    T_AccessPoint* pAP = wld_rad_vap_from_name(pRad, pIfaceInfo->name);
    if(pAP != NULL) {
        wld_vap_setNetdevIndex(pAP, pIfaceInfo->ifIndex);
        pAP->wDevId = pIfaceInfo->wDevId;
        pAP->pFA->mfn_wvap_setEvtHandlers(pAP);
        wld_wpaCtrlMngr_t* pMgr = wld_wpaCtrlInterface_getMgr(pAP->wpaCtrlInterface);
        if(pMgr != NULL) {
            wld_wpaCtrlMngr_checkAllIfaces(pMgr);
            if(s_checkEnabledIfacesCreatedReady(pMgr, pRad)) {
                chanmgt_rad_state radDetState = CM_RAD_UNKNOWN;
                wifiGen_hapd_getRadState(pRad, &radDetState);
                if(!s_saveHapdRadDetState(pRad, radDetState)) {
                    return;
                }
                bool isRadReady = (radDetState == CM_RAD_UP);
                bool isRadStarting = (radDetState == CM_RAD_FG_CAC) || (radDetState == CM_RAD_CONFIGURING);
                SAH_TRACEZ_INFO(ME, "%s: rad %s isRadReady:%d isRadStarting:%d",
                                pAP->alias, pRad->Name, isRadReady, isRadStarting);
                if(isRadReady || isRadStarting) {
                    const char* ifName = wld_wpaCtrlInterface_getName(pAP->wpaCtrlInterface);
                    CALL_SECDMN_MGR_EXT(pRad->hostapd, fSyncOnRadioUp, (char*) ifName, isRadReady);
                }
            }
        }
    }
}

static void s_delInterfaceCb(void* pRef, void* pData _UNUSED, wld_nl80211_ifaceInfo_t* pIfaceInfo _UNUSED) {
    T_Radio* pRad = (T_Radio*) pRef;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    ASSERT_NOT_NULL(pIfaceInfo, , ME, "NULL");
    SAH_TRACEZ_NOTICE(ME, "%s: interface deleted (devIdx:%d)", pIfaceInfo->name, pIfaceInfo->ifIndex);
    T_AccessPoint* pAP = wld_rad_vap_from_name(pRad, pIfaceInfo->name);
    if(pAP != NULL) {
        wld_autoNeighAdd_vapSetDelNeighbourAP(pAP, false);
        wld_ap_nl80211_delEvtListener(pAP);
        wld_vap_setNetdevIndex(pAP, 0);
        pAP->wDevId = 0;
    }
    if((uint32_t) pRad->index == pIfaceInfo->ifIndex) {
        pRad->index = pRad->wDevId = 0;
    }
}

void s_radarEvtCb(void* pRef, void* pData _UNUSED, wld_nl80211_radarEvtInfo_t* radarEvtInfo) {
    ASSERT_NOT_NULL(radarEvtInfo, , ME, "NULL");
    ASSERT_NOT_EQUALS(radarEvtInfo->event, WLD_NL80211_RADAR_EVT_UNKNOWN, , ME, "unknown");
    T_Radio* pRad = (T_Radio*) pRef;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    swl_chanspec_t chanspec = SWL_CHANSPEC_EMPTY;
    swl_rc_ne rc = wld_nl80211_chanSpecNlToSwl(&chanspec, &radarEvtInfo->chanSpec);
    ASSERT_TRUE(swl_rc_isOk(rc), , ME, "%s: fail to convert dfs evt chanspec", pRad->Name);
    ASSERTS_EQUALS(pRad->operatingFrequencyBand, chanspec.band, , ME, "%s: unmatching freqBand", pRad->Name);
    SAH_TRACEZ_INFO(ME, "%s: (w:%d,i:%d) dfs cac event %d on chspec %s bg:%d",
                    pRad->Name, radarEvtInfo->wiphy, radarEvtInfo->ifIndex, radarEvtInfo->event,
                    swl_typeChanspecExt_toBuf32Ref(&chanspec).buf, radarEvtInfo->isBackground);
    ASSERTS_TRUE(radarEvtInfo->isBackground, , ME, "only process bgDfs events");
    ASSERTS_NOT_EQUALS(pRad->bgdfs_config.status, BGDFS_STATUS_OFF, , ME, "%s: bgDfs not supported");
    switch(radarEvtInfo->event) {
    case WLD_NL80211_RADAR_CAC_STARTED: {
        if(!wld_bgdfs_isRunning(pRad)) {
            wld_bgdfs_notifyClearStarted(pRad, chanspec.channel, chanspec.bandwidth, BGDFS_TYPE_CLEAR);
            s_dfsCacStartedCb(pRad, pRad->Name, &chanspec, wld_channel_get_band_clear_time(chanspec));
        }
        break;
    }
    case WLD_NL80211_RADAR_CAC_ABORTED: {
        if(wld_bgdfs_isRunning(pRad)) {
            /* stop ZeroWait DFS if any */
            pRad->pFA->mfn_wrad_zwdfs_stop(pRad);
            s_dfsCacDoneCb(pRad, pRad->Name, &chanspec, false);
        }
        break;
    }
    case WLD_NL80211_RADAR_DETECTED: {
        if(wld_bgdfs_isRunning(pRad)) {
            /* stop ZeroWait DFS if any */
            pRad->pFA->mfn_wrad_zwdfs_stop(pRad);
            s_dfsRadarDetectedCb(pRad, pRad->Name, &chanspec);
        }
    }
    default: break;
    }
}

/*
 * @brief: force refreshing vaps ifindex as sometimes we miss the nl80211 iface added/removed events
 */
void wifiGen_refreshVapsIfIdx(T_Radio* pRad) {
    T_AccessPoint* pAP = NULL;
    wld_rad_forEachAp(pAP, pRad) {
        wld_nl80211_ifaceInfo_t ifaceInfo;
        swl_str_copy(ifaceInfo.name, sizeof(ifaceInfo.name), pAP->alias);
        ifaceInfo.ifIndex = pAP->index;
        ifaceInfo.wDevId = pAP->wDevId;
        int32_t curIfIdx = 0;
        if(((wld_linuxIfUtils_getIfIndex(wld_rad_getSocket(pRad), pAP->alias, &curIfIdx) < 0) ||
            (curIfIdx <= 0) || (pAP->index != curIfIdx)) && (pAP->index > 0)) {
            s_delInterfaceCb(pRad, NULL, &ifaceInfo);
        }
        if(curIfIdx > 0) {
            if((pAP->index != curIfIdx) &&
               (wld_nl80211_getInterfaceInfo(wld_nl80211_getSharedState(), curIfIdx, &ifaceInfo) >= SWL_RC_OK)) {
                s_newInterfaceCb(pRad, NULL, &ifaceInfo);
            }
            if((wifiGen_hapd_isRunning(pRad)) &&
               (wld_hostapd_ap_needWpaCtrlIface(pAP)) &&
               (!wld_wpaCtrlInterface_isReady(pAP->wpaCtrlInterface))) {
                wld_wpaCtrlInterface_open(pAP->wpaCtrlInterface);
            }
        }
    }
}

static void s_scanResultsCb(void* priv, swl_rc_ne rc, wld_scanResults_t* results) {
    T_Radio* pRad = (T_Radio*) priv;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    ASSERT_NOT_NULL(results, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: rc:%d nResults:%zu", pRad->Name, rc, amxc_llist_size(&results->ssids));
    if(rc < SWL_RC_OK) {
        wld_scan_done(pRad, false);
        return;
    }
    wld_scan_cleanupScanResults(&pRad->scanState.lastScanResults);
    amxc_llist_for_each(it, &results->ssids) {
        wld_scanResultSSID_t* pResult = amxc_container_of(it, wld_scanResultSSID_t, it);
        SAH_TRACEZ_INFO(ME, "scan result entry: bssid("SWL_MAC_FMT ") ssid(%s) signal(%d dbm)",
                        SWL_MAC_ARG(pResult->bssid.bMac), pResult->ssid, pResult->rssi);
        amxc_llist_it_take(&pResult->it);
        amxc_llist_append(&pRad->scanState.lastScanResults.ssids, &pResult->it);
    }
    wld_scan_done(pRad, true);
}

static void s_scanDoneCb(void* pRef, void* pData _UNUSED, uint32_t wiphy _UNUSED, uint32_t ifIndex _UNUSED) {
    T_Radio* pRad = (T_Radio*) pRef;
    const char* radName = pRad ? pRad->Name : "";
    SAH_TRACEZ_INFO(ME, "%s: scan done on wiphy:%d iface:%d", radName, wiphy, ifIndex);
    if((pRad == NULL) || (pRad->wiphy != wiphy) || (!wld_rad_hasLinkIfIndex(pRad, ifIndex))) {
        return;
    }
    SAH_TRACEZ_INFO(ME, "%s: getting scan async results", radName);
    wld_rad_nl80211_getScanResults(pRad, pRad, s_scanResultsCb);
}

static void s_scanAbortedCb(void* pRef, void* pData _UNUSED, uint32_t wiphy _UNUSED, uint32_t ifIndex _UNUSED) {
    T_Radio* pRad = (T_Radio*) pRef;
    const char* radName = pRad ? pRad->Name : "";
    SAH_TRACEZ_INFO(ME, "%s: scan aborted on wiphy:%d iface:%d", radName, wiphy, ifIndex);
    if((pRad == NULL) || (pRad->wiphy != wiphy) || (!wld_rad_hasLinkIfIndex(pRad, ifIndex))) {
        return;
    }
    wld_scan_done(pRad, false);
}

static void s_scanStartedCb(void* pRef, void* pData _UNUSED, uint32_t wiphy _UNUSED, uint32_t ifIndex _UNUSED) {
    T_Radio* pRad = (T_Radio*) pRef;
    const char* radName = pRad ? pRad->Name : "";
    SAH_TRACEZ_INFO(ME, "%s: scan started on wiphy:%d iface:%d", radName, wiphy, ifIndex);
    if((pRad == NULL) || (pRad->wiphy != wiphy) || (!wld_rad_hasLinkIfIndex(pRad, ifIndex))) {
        return;
    }
}

static void s_frameReceivedCb(void* pRef, void* pData _UNUSED, size_t frameLen, swl_80211_mgmtFrame_t* frame, int32_t rssi) {
    T_Radio* pRad = (T_Radio*) pRef;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    ASSERT_NOT_NULL(frame, , ME, "NULL");
    wld_rad_triggerFrameEvent(pRad, (swl_bit8_t*) frame, frameLen, rssi);
}


static bool s_chechTgtRadListener(void* pRef, void* pData _UNUSED, int32_t wiphy, int32_t ifIndex, uint32_t freqMHz, const char* ifName) {
    T_Radio* pRad = (T_Radio*) pRef;
    ASSERTS_TRUE(debugIsRadPointer(pRad), false, ME, "NULL");
    //1) try to match radios of known ifIndex or ifName
    if(ifIndex > 0) {
        if(wld_rad_hasLinkIfIndex(pRad, ifIndex) || wld_rad_hasLinkIfName(pRad, ifName)) {
            return true;
        }

        swl_chanspec_t chanSpec = SWL_CHANSPEC_EMPTY;
        if(swl_rc_isOk(swl_chanspec_channelFromMHz(&chanSpec, freqMHz))) {
            T_SSID* pSSID = wld_ssid_getSsidByIfIndex(ifIndex);
            return ((pRad->operatingFrequencyBand == chanSpec.band) &&
                    (pSSID != NULL) &&
                    ((pSSID->RADIO_PARENT == pRad) ||
                     (wld_mld_getNeighLinkByRad(pSSID->pMldLink, pRad)) != NULL));
        }
    }
    //2) otherwise, only match wiphy id
    if(wiphy >= 0) {
        return (wiphy == (int32_t) pRad->wiphy);
    }
    return false;
}

swl_rc_ne wifiGen_setRadEvtHandlers(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pRad->hostapd, SWL_RC_ERROR, ME, "NULL");

    s_setWpaCtrlRadEvtHandlers(pRad->hostapd->wpaCtrlMngr, pRad);

    wld_nl80211_evtHandlers_cb nl80211RadEvtHandlers;
    memset(&nl80211RadEvtHandlers, 0, sizeof(nl80211RadEvtHandlers));
    //Set here the nl80211 RAD event handlers
    nl80211RadEvtHandlers.fNewInterfaceCb = s_newInterfaceCb;
    nl80211RadEvtHandlers.fDelInterfaceCb = s_delInterfaceCb;
    nl80211RadEvtHandlers.fScanStartedCb = s_scanStartedCb;
    nl80211RadEvtHandlers.fScanAbortedCb = s_scanAbortedCb;
    nl80211RadEvtHandlers.fScanDoneCb = s_scanDoneCb;
    nl80211RadEvtHandlers.fMgtFrameEvtCb = s_frameReceivedCb;
    nl80211RadEvtHandlers.fRadarEventCb = s_radarEvtCb;
    nl80211RadEvtHandlers.fCheckTgtCb = s_chechTgtRadListener;
    wld_rad_nl80211_setEvtListener(pRad, NULL, &nl80211RadEvtHandlers);

    return SWL_RC_OK;
}

static void s_wpsCancel(void* userData, char* ifName _UNUSED) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    wld_ap_sendPairingNotification(pAP, NOTIFY_PAIRING_DONE, WPS_CAUSE_CANCELLED, NULL);
}

static void s_wpsTimeout(void* userData, char* ifName _UNUSED) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    wld_ap_sendPairingNotification(pAP, NOTIFY_PAIRING_DONE, WPS_CAUSE_TIMEOUT, NULL);
}

static void s_wpsSuccess(void* userData, char* ifName _UNUSED, swl_macChar_t* mac) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    wld_ap_sendPairingNotification(pAP, NOTIFY_PAIRING_DONE, WPS_CAUSE_SUCCESS, mac->cMac);
}

static void s_wpsOverlap(void* userData, char* ifName _UNUSED) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    wld_ap_sendPairingNotification(pAP, NOTIFY_PAIRING_DONE, WPS_CAUSE_OVERLAP, NULL);
}

static void s_wpsFail(void* userData, char* ifName _UNUSED) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    wld_ap_sendPairingNotification(pAP, NOTIFY_PAIRING_DONE, WPS_CAUSE_FAILURE, NULL);
}

static void s_wpsSetupLocked(void* userData, char* ifName _UNUSED, bool setupLocked) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    SAH_TRACEZ_INFO(ME, "%s: setup %s event received", pAP->alias, setupLocked ? "locked" : "unlocked");
    pAP->WPS_ApSetupLocked = setupLocked;
    wld_wps_updateState(pAP);
}

static void s_apEnabledCb(void* userData, char* ifName) {
    ASSERTS_STR(ifName, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: AP iface enabled", ifName);
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    ASSERT_TRUE(debugIsVapPointer(pAP), , ME, "INVALID");
    wld_vap_updateState(pAP);
}

static void s_apDisabledCb(void* userData, char* ifName) {
    ASSERTS_STR(ifName, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: AP iface disabled", ifName);
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    ASSERT_TRUE(debugIsVapPointer(pAP), , ME, "INVALID");
    wld_vap_updateState(pAP);
}

static void s_selectPrimLinkIface(void* userData, const char* ifName, char** pPrimLinkIfName) {
    ASSERT_NOT_NULL(pPrimLinkIfName, , ME, "NULL");
    swl_str_copyMalloc(pPrimLinkIfName, NULL);
    ASSERT_STR(ifName, , ME, "empty ifname");
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    ASSERT_EQUALS(wld_vap_from_name(ifName), pAP, , ME, "vap (%p) ifname (%s) mismatch", pAP, ifName);
    const char* pLinkIfName = wld_ssid_getIfName(wifiGen_mld_selectPrimLinkSSID(pAP->pSSID));
    ASSERTI_STR(pLinkIfName, , ME, "no prim link iface for iface (%s)", ifName);
    SAH_TRACEZ_INFO(ME, "select primary link iface (%s) for link (%s)", pLinkIfName, ifName);
    swl_str_copyMalloc(pPrimLinkIfName, pLinkIfName);
}

static void s_apStationConnectedEvt(void* pRef, char* ifName, swl_macBin_t* macAddress) {
    T_AccessPoint* pAP = (T_AccessPoint*) pRef;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(ifName, , ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: connecting station "MAC_PRINT_FMT, pAP->alias, MAC_PRINT_ARG(macAddress->bMac));

    T_AssociatedDevice* pAD = wld_vap_findOrCreateAssociatedDevice(pAP, macAddress);
    ASSERT_NOT_NULL(pAD, , ME, "%s: Failure to create associated device "MAC_PRINT_FMT, pAP->alias, MAC_PRINT_ARG(macAddress->bMac));

    pAP->pFA->mfn_wvap_get_single_station_stats(pAD);

    wld_ad_add_connection_success(pAP, pAD);
}

static void s_stationEapCompletedEvt(void* pRef, char* ifName, swl_macBin_t* macAddress) {
    T_AccessPoint* pAP = (T_AccessPoint*) pRef;
    SAH_TRACEZ_INFO(ME, "%s: EAP completed "MAC_PRINT_FMT, pAP->alias, MAC_PRINT_ARG(macAddress->bMac));
    s_apStationConnectedEvt(pRef, ifName, macAddress);
}

static void s_apStationDisconnectedEvt(void* pRef, char* ifName, swl_macBin_t* macAddress) {
    T_AccessPoint* pAP = (T_AccessPoint*) pRef;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(ifName, , ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: disconnecting station " MAC_PRINT_FMT, pAP->alias,
                    MAC_PRINT_ARG(macAddress->bMac));

    T_AssociatedDevice* pAD = wld_vap_find_asociatedDevice(pAP, macAddress);
    ASSERT_NOT_NULL(pAD, , ME, "NULL");

    // The AP-STA-DISCONNECTED event may be received before the AP-MGMT-FRAME-RECEIVED event.
    // Start timer to Delay sending associated device disconnection notification:
    // This delay makes it possible to set the lastDeauthReason from the received deauth mgmt frame.
    if(pAD->lastDeauthAssocTime == pAD->associationTime) {
        wld_ad_deauthWithReason(pAP, pAD, pAD->lastDeauthReason);
    } else {
        wld_ad_startDelayDisassocNotifTimer(pAD);
        wld_ad_add_disconnection(pAP, pAD);
    }
    wld_sensing_delCsiClientEntry(pAP->pRadio, macAddress);
}

static void s_addWdsEntry(T_AccessPoint* pAP, T_AssociatedDevice* pAD, char* wdsIntfName) {
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(pAD, , ME, "NULL");
    ASSERT_STR(wdsIntfName, , ME, "NULL/empty");

    int32_t index = -1;
    swl_rc_ne rc = wld_linuxIfUtils_getIfIndexExt(wdsIntfName, &index);
    ASSERT_FALSE((rc < SWL_RC_OK), , ME, "WDS '%s' intf index not found", wdsIntfName);

    pAD->wdsIntf = calloc(1, sizeof(wld_wds_intf_t));
    ASSERT_NOT_NULL(pAD->wdsIntf, , ME, "memory allocation fails");

    pAD->wdsIntf->active = true;
    pAD->wdsIntf->name = strdup(wdsIntfName);
    pAD->wdsIntf->index = index;
    pAD->wdsIntf->ap = pAP;
    memcpy(&pAD->wdsIntf->bStaMac, pAD->MACAddress, sizeof(swl_macBin_t));
    amxc_llist_append(&pAP->llIntfWds, &pAD->wdsIntf->entry);

    wld_event_trigger_callback(gWld_queue_wdsInterface, pAD->wdsIntf);
}

static void s_delWdsEntry(T_AssociatedDevice* pAD) {
    ASSERT_NOT_NULL(pAD, , ME, "NULL");
    ASSERT_NOT_NULL(pAD->wdsIntf, , ME, "NULL");
    pAD->wdsIntf->active = false;
    wld_event_trigger_callback(gWld_queue_wdsInterface, pAD->wdsIntf);
    pAD->wdsIntf->index = -1;
    amxc_llist_it_take(&pAD->wdsIntf->entry);
    free(pAD->wdsIntf->name);
    free(pAD->wdsIntf);
    pAD->wdsIntf = NULL;
}

static void s_wdsCreatedEvt(void* pRef, char* ifName, char* wdsIntfName, swl_macBin_t* macAddress) {
    T_AccessPoint* pAP = (T_AccessPoint*) pRef;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(ifName, , ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: connecting WDS station " MAC_PRINT_FMT " to intf %s", pAP->alias,
                    MAC_PRINT_ARG(macAddress->bMac), wdsIntfName);

    T_AssociatedDevice* pAD = wld_vap_findOrCreateAssociatedDevice(pAP, macAddress);
    ASSERT_NOT_NULL(pAD, , ME, "%s: Failure to create associated device "MAC_PRINT_FMT, pAP->alias, MAC_PRINT_ARG(macAddress->bMac));

    pAD->Active = true;
    /* AssociatedDevice should have only one WDS interface.
     * delete old one, if any is still there */
    s_delWdsEntry(pAD);
    /* and create new one */
    s_addWdsEntry(pAP, pAD, wdsIntfName);

    pAP->pFA->mfn_wvap_get_single_station_stats(pAD);
    wld_ad_add_connection_success(pAP, pAD);
}

static void s_wdsRemovedEvt(void* pRef, char* ifName, char* wdsIntfName, swl_macBin_t* macAddress) {
    T_AccessPoint* pAP = (T_AccessPoint*) pRef;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(ifName, , ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: disconnecting WDS station " MAC_PRINT_FMT " from %s", pAP->alias,
                    MAC_PRINT_ARG(macAddress->bMac), wdsIntfName);

    T_AssociatedDevice* pAD = wld_vap_find_asociatedDevice(pAP, macAddress);
    ASSERT_NOT_NULL(pAD, , ME, "NULL");

    s_delWdsEntry(pAD);

    wld_ad_add_disconnection(pAP, pAD);
    wld_sensing_delCsiClientEntry(pAP->pRadio, macAddress);
}

static void s_apStationAssocFailureEvt(void* pRef, char* ifName, swl_macBin_t* macAddress) {
    T_AccessPoint* pAP = (T_AccessPoint*) pRef;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(ifName, , ME, "NULL");

    T_AssociatedDevice* pAD = wld_vap_find_asociatedDevice(pAP, macAddress);
    ASSERT_NOT_NULL(pAD, , ME, "NULL");
    wld_ad_add_sec_failNoDc(pAP, pAD);
}

static void s_btmReplyEvt(void* userData, char* ifName _UNUSED, swl_macChar_t* mac, uint8_t replyCode, swl_macChar_t* targetBssid) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    wld_ap_bss_done(pAP, mac, (int) replyCode, targetBssid);
}

static void s_handleAssocMsgAffiliatedSta(T_AccessPoint* pAP, T_AssociatedDevice* pAD, swl_macBin_t* mac,
                                          swl_wirelessDevice_infoElements_t* pWirelessDevIE) {
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(pAD, , ME, "NULL");
    ASSERT_NOT_NULL(mac, , ME, "NULL");
    ASSERT_NOT_NULL(pWirelessDevIE, , ME, "NULL");

    /* handle only affiliatedSta */
    ASSERTS_FALSE(swl_mac_binIsNull(&pWirelessDevIE->ehtMldMacAddress), , ME, NULL);

    /* default to EMLSR if supported */
    pAD->mloMode = SWL_BIT_IS_SET(pWirelessDevIE->ehtEmlCapabilities, SWL_STACAP_EHT_EML_EMLSR) ?
        SWL_MLO_MODE_EMLSR :
        SWL_BIT_IS_SET(pWirelessDevIE->ehtEmlCapabilities, SWL_STACAP_EHT_EML_EMLMR) ?
        SWL_MLO_MODE_EMLMR :
        SWL_MLO_MODE_NSTR;

    /* force 802.11BE */
    pAD->operatingStandard = SWL_RADSTD_BE;

    wld_affiliatedSta_t* afSta = wld_ad_getOrAddAffiliatedSta(pAD, pAP);
    ASSERT_NOT_NULL(afSta, , ME, "%s: create affiliatedSta (linkId:%u,mac:%s) failed for sta(%s)!", pAP->alias,
                    wld_mld_getLinkId(pAP->pSSID->pMldLink), swl_typeMacBin_toBuf32Ref(mac).buf,
                    pAD->Name);
    afSta->active = true;
    afSta->mac = *mac;
    afSta->linkId = wld_mld_getLinkId(pAP->pSSID->pMldLink);
    afSta->lastDataDownlinkRate = pWirelessDevIE->maxDownlinkRateSupported;
    afSta->lastDataUplinkRate = pWirelessDevIE->maxUplinkRateSupported;

    /* add detected STA Profile in the Multi-Link */
    for(uint8_t i = 0; i < SWL_ARRAY_SIZE(pWirelessDevIE->ehtLinksMacAddress); i++) {
        if(SWL_BIT_IS_SET(pWirelessDevIE->ehtLinksMask, i)) {
            T_SSID* pSSIDLink = wld_mld_getLinkSsidByLinkId(pAP->pSSID->pMldLink, i);
            if(pSSIDLink == NULL) {
                continue;
            }
            wld_affiliatedSta_t* afSta = wld_ad_getOrAddAffiliatedSta(pAD, pSSIDLink->AP_HOOK);
            ASSERT_NOT_NULL(afSta, , ME, "%s: create affiliatedSta (linkId:%u,mac:%s) failed for sta(%s)!",
                            pSSIDLink->AP_HOOK->alias,
                            i, swl_typeMacBin_toBuf32Ref(&pWirelessDevIE->ehtLinksMacAddress[i]).buf,
                            pAD->Name);
            afSta->mac = pWirelessDevIE->ehtLinksMacAddress[i];
            afSta->active = true;
            afSta->linkId = i;
        }
    }
}

static void s_commonAssocReqCb(T_AccessPoint* pAP, swl_80211_mgmtFrame_t* frame, size_t frameLen, swl_bit8_t* iesData, size_t iesLen) {
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(pAP->pRadio, , ME, "NULL");
    ASSERT_NOT_NULL(frame, , ME, "NULL");
    ASSERT_NOT_NULL(pAP->pSSID, , ME, "%s: AP has No SSID", pAP->alias);
    if(!SWL_MAC_BIN_MATCHES(pAP->pSSID->BSSID, &frame->bssid)) {
        T_SSID* pTargetSSID = wld_ssid_getSsidByBssid(&frame->bssid);
        if((pTargetSSID == NULL) || (wld_mld_getPrimaryLink(pAP->pSSID->pMldLink) != wld_mld_getPrimaryLink(pTargetSSID->pMldLink))) {
            SAH_TRACEZ_ERROR(ME, "%s: bssid does not match ap("MAC_PRINT_FMT ") frame("MAC_PRINT_FMT ")",
                             pAP->alias, MAC_PRINT_ARG(pAP->pSSID->BSSID), MAC_PRINT_ARG(frame->bssid.bMac));
            return;
        }
    }
    swl_wirelessDevice_infoElements_t wirelessDevIE;
    swl_parsingArgs_t parsingArgs = {
        .seenOnChanspec = swl_chanspec_fromDm(pAP->pRadio->channel, pAP->pRadio->runningChannelBandwidth, pAP->pRadio->operatingFrequencyBand),
    };
    ssize_t parsedLen = swl_80211_parseInfoElementsBuffer(&wirelessDevIE, &parsingArgs, iesLen, iesData);
    ASSERTW_FALSE(parsedLen < (ssize_t) iesLen, , ME, "Partial IEs parsing (%zi/%zu)", parsedLen, iesLen);
    swl_macBin_t* mac = swl_mac_binIsNull(&wirelessDevIE.ehtMldMacAddress) ? &frame->transmitter : &wirelessDevIE.ehtMldMacAddress;
    SAH_TRACEZ_INFO(ME, "%s: create AssociatedDevice(%s) for Association Request from (%s)", pAP->alias,
                    swl_typeMacBin_toBuf32Ref(mac).buf,
                    swl_typeMacBin_toBuf32Ref(&frame->transmitter).buf);
    T_AssociatedDevice* pAD = wld_vap_findOrCreateAssociatedDevice(pAP, mac);
    ASSERT_NOT_NULL(pAD, , ME, "%s: Failure to retrieve associated device %s", pAP->alias,
                    swl_typeMacBin_toBuf32Ref(mac).buf);
    wld_vap_saveAssocReq(pAP, (swl_bit8_t*) frame, frameLen);
    wld_ad_handleAssocMsg(pAP, pAD, iesData, iesLen);
    s_handleAssocMsgAffiliatedSta(pAP, pAD, &frame->transmitter, &wirelessDevIE);
    W_SWL_BIT_SET(pAD->assocCaps.freqCapabilities, pAP->pRadio->operatingFrequencyBand);
    wld_ad_add_connection_try(pAP, pAD);
}

static void s_assocReqCb(void* userData, swl_80211_mgmtFrame_t* frame, size_t frameLen, swl_80211_assocReqFrameBody_t* assocReq, size_t assocReqDataLen) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(assocReq, , ME, "NULL");
    s_commonAssocReqCb(pAP, frame, frameLen, assocReq->data, assocReqDataLen);
}

static void s_reassocReqCb(void* userData, swl_80211_mgmtFrame_t* frame, size_t frameLen, swl_80211_reassocReqFrameBody_t* reassocReq, size_t reassocReqDataLen) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(reassocReq, , ME, "NULL");
    s_commonAssocReqCb(pAP, frame, frameLen, reassocReq->data, reassocReqDataLen);
}

static void s_btmQueryCb(void* userData, swl_80211_mgmtFrame_t* frame, size_t frameLen _UNUSED, swl_80211_wnmActionBTMQueryFrameBody_t* btmQuery, size_t btmQueryDataLen _UNUSED) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(frame, , ME, "NULL");
    ASSERT_NOT_NULL(btmQuery, , ME, "NULL");
    swl_macChar_t mac;
    SWL_MAC_BIN_TO_CHAR(&mac, &frame->transmitter);
    SAH_TRACEZ_INFO(ME, "%s BSS-TM-QUERY %s reason %d", pAP->alias, mac.cMac, btmQuery->reason);
}

static void s_btmRespCb(void* userData, swl_80211_mgmtFrame_t* frame, size_t frameLen _UNUSED, swl_80211_wnmActionBTMResponseFrameBody_t* btmResp, size_t btmRespDataLen _UNUSED) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(frame, , ME, "NULL");
    ASSERT_NOT_NULL(btmResp, , ME, "NULL");
    uint8_t replyCode = btmResp->statusCode;
    swl_macChar_t mac;
    SWL_MAC_BIN_TO_CHAR(&mac, &frame->transmitter);
    swl_macChar_t targetBssid = g_swl_macChar_null;
    /*The Target BSSID field is the BSSID of the BSS that the non-AP STA transitions to. This field is present if
       the Status code subfield contains 0, and not present otherwise.*/
    if((replyCode == 0) && (btmRespDataLen >= SWL_MAC_BIN_LEN)) {
        SWL_MAC_BIN_TO_CHAR(&targetBssid, &btmResp->data);
        SAH_TRACEZ_INFO(ME, "%s BSS-TM-RESP %s status_code=%d targetBssid= %s", pAP->alias, mac.cMac, replyCode, targetBssid.cMac);
    } else {
        SAH_TRACEZ_INFO(ME, "%s BSS-TM-RESP %s status_code=%d", pAP->alias, mac.cMac, replyCode);
    }

    s_btmReplyEvt(pAP, pAP->alias, &mac, replyCode, &targetBssid);
}

static void s_actionFrameCb(void* userData, swl_80211_mgmtFrame_t* frame, size_t frameLen, swl_80211_actionFrame_t* actionFrame _UNUSED, size_t actionFrameDataLen _UNUSED) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(frame, , ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s action frame received (len:%zu)", pAP->alias, frameLen);

    char frameStr[(frameLen * 2) + 1];
    bool ret = swl_hex_fromBytesSep(frameStr, sizeof(frameStr), (swl_bit8_t*) frame, frameLen, false, 0, NULL);
    ASSERT_TRUE(ret, , ME, "%s: fail to hex dump action frame (len:%zu)", pAP->alias, frameLen);

    // trigger AMX signal to notify the receive of the action frame
    wld_vap_notifyActionFrame(pAP, frameStr);
}

static void s_disassocCb(void* userData, swl_80211_mgmtFrame_t* frame, swl_80211_disassocFrameBody_t* disassocFrame) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(frame, , ME, "NULL");
    ASSERT_NOT_NULL(disassocFrame, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s Disassoc Frame received from "MAC_PRINT_FMT " reasonCode [%u]", pAP->alias, MAC_PRINT_ARG(frame->transmitter.bMac), disassocFrame->reason);
    wld_vap_notifyDeauthDisassocFrame(pAP, "MgmtDisassocFrame", &frame->transmitter, disassocFrame->reason, false);
    T_AssociatedDevice* pAD = wld_vap_find_asociatedDevice(pAP, &frame->transmitter);
    ASSERTS_NOT_NULL(pAD, , ME, "NULL");
    pAD->lastDeauthReason = disassocFrame->reason;
    pAD->lastDeauthAssocTime = pAD->associationTime;
}

static void s_deauthCb(void* userData, swl_80211_mgmtFrame_t* frame, swl_80211_deauthFrameBody_t* deauthFrame) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(frame, , ME, "NULL");
    ASSERT_NOT_NULL(deauthFrame, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s Deauth Frame received from "MAC_PRINT_FMT " reasonCode [%u]", pAP->alias, MAC_PRINT_ARG(frame->transmitter.bMac), deauthFrame->reason);
    wld_vap_notifyDeauthDisassocFrame(pAP, "MgmtDeauthFrame", &frame->transmitter, deauthFrame->reason, false);
    T_AssociatedDevice* pAD = wld_vap_find_asociatedDevice(pAP, &frame->transmitter);
    ASSERTS_NOT_NULL(pAD, , ME, "NULL");
    pAD->lastDeauthReason = deauthFrame->reason;
    pAD->lastDeauthAssocTime = pAD->associationTime;
}

static swl_80211_mgmtFrameHandlers_cb s_mgmtFrameHandlers = {
    .fProcActionFrame = s_actionFrameCb,
    .fProcAssocReq = s_assocReqCb,
    .fProcReAssocReq = s_reassocReqCb,
    .fProcBtmQuery = s_btmQueryCb,
    .fProcBtmResp = s_btmRespCb,
    .fProcDisassoc = s_disassocCb,
    .fProcDeauth = s_deauthCb,
};

static void s_mgtFrameReceivedEvt(void* userData, char* ifName _UNUSED, swl_80211_mgmtFrame_t* mgmtFrame, size_t frameLen, char* frameStr _UNUSED) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(mgmtFrame, , ME, "NULL");
    swl_80211_handleMgmtFrame(userData, (swl_bit8_t*) mgmtFrame, frameLen, &s_mgmtFrameHandlers);
}

static void s_beaconResponseEvt(void* userData, char* ifName _UNUSED, swl_macBin_t* station, wld_wpaCtrl_rrmBeaconRsp_t* rrmBeaconResponse) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");

    swl_macChar_t cStation;
    swl_macChar_t cBssid;
    SWL_MAC_BIN_TO_CHAR(&cStation, station);
    SWL_MAC_BIN_TO_CHAR(&cBssid, &(rrmBeaconResponse->bssid));


    SAH_TRACEZ_INFO(ME, "%s: RRM reply received station:%s bssid:%s", pAP->alias, cStation.cMac, cBssid.cMac);

    SAH_TRACEZ_INFO(ME, "%s: opclass: %d channel: %d, duration: %d, frame info: %d, rcpi: %d, rsni: %d, antenna id: %d, parent tsf: %u\n",
                    pAP->alias, rrmBeaconResponse->opclass, rrmBeaconResponse->channel, rrmBeaconResponse->duration, rrmBeaconResponse->frameInfo,
                    rrmBeaconResponse->rcpi, rrmBeaconResponse->rsni, rrmBeaconResponse->antennaId, rrmBeaconResponse->parentTsf);

    amxc_var_t retval;
    amxc_var_init(&retval);
    amxc_var_set_type(&retval, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, &retval, "BSSID", (char*) cBssid.cMac);
    amxc_var_add_key(uint8_t, &retval, "Channel", rrmBeaconResponse->channel);
    amxc_var_add_key(uint8_t, &retval, "RCPI", rrmBeaconResponse->rcpi);
    amxc_var_add_key(uint8_t, &retval, "RSNI", rrmBeaconResponse->rsni);
    wld_ap_rrm_item(pAP, &cStation, &retval);
    amxc_var_clean(&retval);
}

static void s_disassocTxFrameCb(void* userData, swl_80211_mgmtFrame_t* frame, swl_80211_disassocFrameBody_t* disassocFrame) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(frame, , ME, "NULL");
    ASSERT_NOT_NULL(disassocFrame, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s Disassoc Frame transmitted to "MAC_PRINT_FMT " reasonCode [%u]", pAP->alias, MAC_PRINT_ARG(frame->destination.bMac), disassocFrame->reason);
    wld_vap_notifyDeauthDisassocFrame(pAP, "MgmtDisassocFrame", &frame->destination, disassocFrame->reason, true);
    T_AssociatedDevice* pAD = wld_vap_find_asociatedDevice(pAP, &frame->destination);
    ASSERTS_NOT_NULL(pAD, , ME, "NULL");
    pAD->lastDeauthReason = disassocFrame->reason;
    pAD->lastDeauthAssocTime = pAD->associationTime;
}

static void s_deauthTxFrameCb(void* userData, swl_80211_mgmtFrame_t* frame, swl_80211_deauthFrameBody_t* deauthFrame) {
    T_AccessPoint* pAP = (T_AccessPoint*) userData;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(frame, , ME, "NULL");
    ASSERT_NOT_NULL(deauthFrame, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s Deauth Frame Transmitted to "MAC_PRINT_FMT " reasonCode [%u]", pAP->alias, MAC_PRINT_ARG(frame->destination.bMac), deauthFrame->reason);
    wld_vap_notifyDeauthDisassocFrame(pAP, "MgmtDeauthFrame", &frame->destination, deauthFrame->reason, true);
    T_AssociatedDevice* pAD = wld_vap_find_asociatedDevice(pAP, &frame->destination);
    ASSERTS_NOT_NULL(pAD, , ME, "NULL");
    pAD->lastDeauthReason = deauthFrame->reason;
    pAD->lastDeauthAssocTime = pAD->associationTime;
}

static swl_80211_mgmtFrameHandlers_cb s_mgmtFrameTxStatusHandlers = {
    .fProcDisassoc = s_disassocTxFrameCb,
    .fProcDeauth = s_deauthTxFrameCb,
};

static void s_mgmtFrameTxStatusCb(void* pRef, void* pData _UNUSED, size_t frameLen, swl_80211_mgmtFrame_t* mgmtFrame, bool isAck _UNUSED) {
    T_AccessPoint* pAP = (T_AccessPoint*) pRef;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    ASSERT_NOT_NULL(mgmtFrame, , ME, "NULL");
    swl_80211_handleMgmtFrame(pRef, (swl_bit8_t*) mgmtFrame, frameLen, &s_mgmtFrameTxStatusHandlers);
}

swl_rc_ne wifiGen_setVapEvtHandlers(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");

    wld_wpaCtrl_evtHandlers_cb wpaCtrlVapEvtHandlers;
    memset(&wpaCtrlVapEvtHandlers, 0, sizeof(wpaCtrlVapEvtHandlers));

    //Set here the wpa_ctrl VAP event handlers
    wpaCtrlVapEvtHandlers.fProcStdEvtMsg = s_wpaCtrlIfaceStdEvt;
    wpaCtrlVapEvtHandlers.fWpsCancelMsg = s_wpsCancel;
    wpaCtrlVapEvtHandlers.fWpsTimeoutMsg = s_wpsTimeout;
    wpaCtrlVapEvtHandlers.fWpsSuccessMsg = s_wpsSuccess;
    wpaCtrlVapEvtHandlers.fWpsOverlapMsg = s_wpsOverlap;
    wpaCtrlVapEvtHandlers.fWpsFailMsg = s_wpsFail;
    wpaCtrlVapEvtHandlers.fWpsSetupLockedMsg = s_wpsSetupLocked;
    wpaCtrlVapEvtHandlers.fApStationEapCompletedCb = s_stationEapCompletedEvt;
    wpaCtrlVapEvtHandlers.fApStationConnectedCb = s_apStationConnectedEvt;
    wpaCtrlVapEvtHandlers.fApStationDisconnectedCb = s_apStationDisconnectedEvt;
    wpaCtrlVapEvtHandlers.fWdsIfaceAddedCb = s_wdsCreatedEvt;
    wpaCtrlVapEvtHandlers.fWdsIfaceRemovedCb = s_wdsRemovedEvt;
    wpaCtrlVapEvtHandlers.fApStationAssocFailureCb = s_apStationAssocFailureEvt;
    wpaCtrlVapEvtHandlers.fBtmReplyCb = s_btmReplyEvt;
    wpaCtrlVapEvtHandlers.fMgtFrameReceivedCb = s_mgtFrameReceivedEvt;
    wpaCtrlVapEvtHandlers.fBeaconResponseCb = s_beaconResponseEvt;
    wpaCtrlVapEvtHandlers.fApEnabledCb = s_apEnabledCb;
    wpaCtrlVapEvtHandlers.fApDisabledCb = s_apDisabledCb;
    wpaCtrlVapEvtHandlers.fSelectPrimLinkIface = s_selectPrimLinkIface;

    if(pAP->wpaCtrlInterface != NULL) {
        ASSERT_TRUE(wld_wpaCtrlInterface_setEvtHandlers(pAP->wpaCtrlInterface, pAP, &wpaCtrlVapEvtHandlers),
                    SWL_RC_ERROR, ME, "%s: fail to set interface wpa evt handlers", pAP->alias);
    }

    wld_nl80211_evtHandlers_cb nl80211VapEvtHandlers;
    memset(&nl80211VapEvtHandlers, 0, sizeof(nl80211VapEvtHandlers));

    //Set here the nl80211 VAP event handlers
    nl80211VapEvtHandlers.fMgtFrameTxStatusEvtCb = s_mgmtFrameTxStatusCb;
    wld_ap_nl80211_setEvtListener(pAP, NULL, &nl80211VapEvtHandlers);

    return SWL_RC_OK;
}

static void s_stationDisconnectedEvt(void* pRef, char* ifName, swl_macBin_t* bBssidMac, swl_IEEE80211deauthReason_ne reason) {
    T_EndPoint* pEP = (T_EndPoint*) pRef;
    ASSERT_NOT_NULL(pEP, , ME, "NULL");
    ASSERT_NOT_NULL(ifName, , ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: station disconnected from "MAC_PRINT_FMT " reason %d", pEP->Name, MAC_PRINT_ARG(bBssidMac->bMac), reason);

    if((pEP->currentProfile != NULL) && (pEP->connectionStatus == EPCS_CONNECTED)) {
        wld_endpoint_sync_connection(pEP, false, 0);
    }
}

static void s_stationAssociatedEvt(void* pRef, char* ifName, swl_macBin_t* bBssidMac, swl_IEEE80211deauthReason_ne reason _UNUSED) {
    T_EndPoint* pEP = (T_EndPoint*) pRef;
    ASSERT_NOT_NULL(pEP, , ME, "NULL");
    ASSERT_NOT_NULL(ifName, , ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: station associated to "MAC_PRINT_FMT, pEP->Name, MAC_PRINT_ARG(bBssidMac->bMac));
    //save remote bssid to allow fully filling wps_done notif (with mac address)
    if(pEP->wpsSessionInfo.WPS_PairingInProgress) {
        memcpy(&pEP->wpsSessionInfo.peerMac, bBssidMac, sizeof(swl_macBin_t));
    }
}

static void s_stationConnectedEvt(void* pRef, char* ifName, swl_macBin_t* bBssidMac, swl_IEEE80211deauthReason_ne reason _UNUSED) {
    T_EndPoint* pEP = (T_EndPoint*) pRef;
    ASSERT_NOT_NULL(pEP, , ME, "NULL");
    ASSERT_NOT_NULL(ifName, , ME, "NULL");
    T_Radio* pRad = pEP->pRadio;
    ASSERT_NOT_NULL(pRad, , ME, "%s: no radio mapped", pEP->Name);

    swl_macChar_t tmpBssidStr = SWL_MAC_CHAR_NEW();
    if((bBssidMac != NULL) && (!swl_mac_binIsNull(bBssidMac))) {
        memcpy(pEP->pSSID->BSSID, bBssidMac->bMac, SWL_MAC_BIN_LEN);
        swl_mac_binToChar(&tmpBssidStr, bBssidMac);
    } else if(wld_wpaSupp_ep_getBssid(pEP, &tmpBssidStr) >= SWL_RC_OK) {
        swl_mac_charToBin((swl_macBin_t*) pEP->pSSID->BSSID, &tmpBssidStr);
    }
    char tmpSsid[128] = {0};
    if(wld_wpaSupp_ep_getSsid(pEP, tmpSsid, sizeof(tmpSsid)) >= SWL_RC_OK) {
        swl_str_copy(pEP->pSSID->SSID, sizeof(pEP->pSSID->SSID), tmpSsid);
    }

    SAH_TRACEZ_INFO(ME, "%s: station connected to ssid(%s) bssid(%s)", pEP->Name, tmpSsid, tmpBssidStr.cMac);
    wld_endpoint_sync_connection(pEP, true, 0);

    // update radio datamodel
    swl_chanspec_t chanSpec = SWL_CHANSPEC_EMPTY;
    pRad->pFA->mfn_wrad_getChanspec(pRad, &chanSpec);
    s_saveChanChanged(pRad, &chanSpec, CHAN_REASON_EP_MOVE);
    wld_channel_clear_passive_band(chanSpec);
    wld_rad_updateState(pRad, true);

    CALL_SECDMN_MGR_EXT(pEP->wpaSupp, fSyncOnEpConnected, pEP->Name, true);
}

static void s_stationScanFailedEvt(void* pRef, char* ifName, int error) {
    T_EndPoint* pEP = (T_EndPoint*) pRef;
    ASSERT_NOT_NULL(pEP, , ME, "NULL");
    ASSERT_NOT_NULL(ifName, , ME, "NULL");
    T_Radio* pRad = pEP->pRadio;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: scan Failed with error(%d)", pEP->Name, error);
    if((error == -EBUSY) && wifiGen_hapd_isAlive(pRad)) {
        ASSERT_TRUE(pEP->toggleBssOnReconnect, , ME, "%s: do not disable hostapd", pEP->Name);
        // disable hostapd to allow wpa_supplicant to do scan
        wld_rad_hostapd_disable(pRad);
    }
}

static void s_stationWpsCredReceivedEvt(void* pRef, char* ifName, void* creds, swl_rc_ne status) {
    T_EndPoint* pEP = (T_EndPoint*) pRef;
    ASSERT_NOT_NULL(pEP, , ME, "NULL");
    ASSERT_NOT_NULL(ifName, , ME, "NULL");
    T_Radio* pRad = pEP->pRadio;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    ASSERT_NOT_NULL(creds, , ME, "NULL");
    T_WPSCredentials* pCreds = (T_WPSCredentials*) creds;

    SAH_TRACEZ_INFO(ME, "%s: wps credentials received with status (%d)", pEP->Name, status);
    if(swl_rc_isOk(status)) {
        uint32_t pskLen = swl_str_len(pCreds->key);
        // If key is 64char long, it must be a hex key, and this can not be used to configure WPA3 security.
        if(pskLen == PSK_KEY_SIZE_LEN - 1) {
            if(!swl_hex_isHexChar(pCreds->key, pskLen)) {
                SAH_TRACEZ_ERROR(ME, "%s: Received 64len WPS key, that is not valid hex", pEP->Name);
                wld_endpoint_sendPairingNotification(pEP, NOTIFY_PAIRING_ERROR, WPS_FAILURE_CREDENTIALS, NULL);
                return;
            }
        } else {
            wld_wpaSupp_ep_increaseSecurityModeInCreds(pEP, (T_WPSCredentials*) pCreds);
        }

        if(!pEP->wpsSessionInfo.WPS_PairingInProgress) {
            wld_endpoint_sendPairingNotification(pEP, NOTIFY_BACKHAUL_CREDS, "Backhaul", pCreds);
        } else {
            pEP->WPS_Configured = 1;
            pEP->connectionStatus = EPCS_WPS_PAIRINGDONE;
            wld_endpoint_sendPairingNotification(pEP, NOTIFY_PAIRING_DONE, WPS_CAUSE_SUCCESS, pCreds);
        }
    } else {
        wld_endpoint_sendPairingNotification(pEP, NOTIFY_PAIRING_ERROR, WPS_FAILURE_CREDENTIALS, NULL);
    }
}

static void s_stationWpsInProgress(void* userData, char* ifName _UNUSED) {
    T_EndPoint* pEP = (T_EndPoint*) userData;
    ASSERTS_NOT_NULL(pEP, , ME, "NULL");
    ASSERTS_TRUE(pEP->wpsSessionInfo.WPS_PairingInProgress, , ME, "%s: no wps session ongoing", pEP->Name);
    wld_endpoint_setConnectionStatus(pEP, EPCS_WPS_PAIRING, EPE_NONE);
}

static void s_stationWpsCancel(void* userData, char* ifName _UNUSED) {
    T_EndPoint* pEP = (T_EndPoint*) userData;
    ASSERTS_NOT_NULL(pEP, , ME, "NULL");
    ASSERTS_TRUE(pEP->wpsSessionInfo.WPS_PairingInProgress, , ME, "%s: no wps session ongoing", pEP->Name);
    wld_endpoint_sync_connection(pEP, false, EPE_WPS_CANCELED);
    wld_endpoint_sendPairingNotification(pEP, NOTIFY_PAIRING_DONE, WPS_CAUSE_CANCELLED, NULL);
}

static void s_stationWpsTimeout(void* userData, char* ifName _UNUSED) {
    T_EndPoint* pEP = (T_EndPoint*) userData;
    ASSERTS_NOT_NULL(pEP, , ME, "NULL");
    ASSERTS_TRUE(pEP->wpsSessionInfo.WPS_PairingInProgress, , ME, "%s: no wps session ongoing", pEP->Name);
    wld_endpoint_sync_connection(pEP, false, EPE_WPS_TIMEOUT);
    wld_endpoint_sendPairingNotification(pEP, NOTIFY_PAIRING_DONE, WPS_CAUSE_TIMEOUT, NULL);
}

static void s_stationWpsSuccess(void* userData, char* ifName _UNUSED, swl_macChar_t* mac _UNUSED) {
    T_EndPoint* pEP = (T_EndPoint*) userData;
    ASSERTS_NOT_NULL(pEP, , ME, "NULL");
    ASSERTS_TRUE(pEP->wpsSessionInfo.WPS_PairingInProgress, , ME, "%s: no wps session ongoing", pEP->Name);
    wld_endpoint_sendPairingNotification(pEP, NOTIFY_PAIRING_DONE, WPS_CAUSE_SUCCESS, NULL);
}

static void s_stationWpsOverlap(void* userData, char* ifName _UNUSED) {
    T_EndPoint* pEP = (T_EndPoint*) userData;
    ASSERTS_NOT_NULL(pEP, , ME, "NULL");
    ASSERTS_TRUE(pEP->wpsSessionInfo.WPS_PairingInProgress, , ME, "%s: no wps session ongoing", pEP->Name);
    wld_endpoint_sync_connection(pEP, false, EPE_NONE);
    wld_endpoint_sendPairingNotification(pEP, NOTIFY_PAIRING_DONE, WPS_CAUSE_OVERLAP, NULL);
}

static void s_stationWpsFail(void* userData, char* ifName _UNUSED) {
    T_EndPoint* pEP = (T_EndPoint*) userData;
    ASSERTS_NOT_NULL(pEP, , ME, "NULL");
    ASSERTS_TRUE(pEP->wpsSessionInfo.WPS_PairingInProgress, , ME, "%s: no wps session ongoing", pEP->Name);
    wld_endpoint_sync_connection(pEP, false, EPE_NONE);
    wld_endpoint_sendPairingNotification(pEP, NOTIFY_PAIRING_DONE, WPS_CAUSE_FAILURE, NULL);
}

static void s_stationMultiApInfo(void* userData, char* ifName _UNUSED, uint8_t profileId, uint16_t vlanId) {
    T_EndPoint* pEP = (T_EndPoint*) userData;
    ASSERTS_NOT_NULL(pEP, , ME, "NULL");
    if((profileId <= MULTIAP_PROFILE_MAX) && (profileId != pEP->multiAPProfile)) {
        pEP->multiAPProfile = profileId;
    }
    if(vlanId != pEP->multiAPVlanId) {
        pEP->multiAPVlanId = vlanId;
    }
}

swl_rc_ne wifiGen_setEpEvtHandlers(T_EndPoint* pEP) {
    ASSERT_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pEP->wpaSupp, SWL_RC_ERROR, ME, "NULL");

    s_setWpaCtrlRadEvtHandlers(pEP->wpaSupp->wpaCtrlMngr, pEP->pRadio);

    wld_wpaCtrl_evtHandlers_cb wpaCtrlEpEvtHandlers;
    memset(&wpaCtrlEpEvtHandlers, 0, sizeof(wpaCtrlEpEvtHandlers));
    //Set here the wpa_ctrl EP event handlers
    wpaCtrlEpEvtHandlers.fProcStdEvtMsg = s_wpaCtrlIfaceStdEvt;
    wpaCtrlEpEvtHandlers.fStationAssociatedCb = s_stationAssociatedEvt;
    wpaCtrlEpEvtHandlers.fStationDisconnectedCb = s_stationDisconnectedEvt;
    wpaCtrlEpEvtHandlers.fStationConnectedCb = s_stationConnectedEvt;
    wpaCtrlEpEvtHandlers.fStationScanFailedCb = s_stationScanFailedEvt;
    wpaCtrlEpEvtHandlers.fWpsCredReceivedCb = s_stationWpsCredReceivedEvt;
    wpaCtrlEpEvtHandlers.fWpsInProgressMsg = s_stationWpsInProgress;
    wpaCtrlEpEvtHandlers.fWpsCancelMsg = s_stationWpsCancel;
    wpaCtrlEpEvtHandlers.fWpsTimeoutMsg = s_stationWpsTimeout;
    wpaCtrlEpEvtHandlers.fWpsSuccessMsg = s_stationWpsSuccess;
    wpaCtrlEpEvtHandlers.fWpsOverlapMsg = s_stationWpsOverlap;
    wpaCtrlEpEvtHandlers.fWpsFailMsg = s_stationWpsFail;
    wpaCtrlEpEvtHandlers.fStationMultiApInfoCb = s_stationMultiApInfo;

    wld_wpaCtrlInterface_setEvtHandlers(pEP->wpaCtrlInterface, pEP, &wpaCtrlEpEvtHandlers);

    return SWL_RC_OK;
}

