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
/*
 * This file implement nl80211 events handling
 */

#include "wld.h"
#include "wld_nl80211_core_priv.h"
#include "wld_nl80211_events_priv.h"
#include "wld_nl80211_scan_priv.h"
#include "wld_nl80211_parser.h"
#include "swl/swl_common.h"
#include "swla/swla_table.h"

#define ME "nlEvt"

#define FOR_EACH_LISTENER(pEntry, pList, ...) \
    { \
        wld_nl80211_listener_t* pEntry = NULL; \
        swl_unLiListIt_t pEntry ## _it; \
        swl_unLiList_for_each(pEntry ## _it, pList) { \
            pEntry = *(swl_unLiList_data(&pEntry ## _it, wld_nl80211_listener_t * *)); \
            {__VA_ARGS__} \
        } \
    }

static swl_rc_ne s_commonEvtCb(swl_unLiList_t* pListenerList, struct nlmsghdr* nlh, struct nlattr* tb[]) {
    ASSERT_NOT_NULL(nlh, SWL_RC_ERROR, ME, "NULL");
    void* msgData = nlmsg_data(nlh);
    ASSERT_NOT_NULL(msgData, SWL_RC_ERROR, ME, "No msg data");
    ASSERT_NOT_NULL(tb, SWL_RC_ERROR, ME, "NULL");
    ASSERTI_NOT_EQUALS(swl_unLiList_size(pListenerList), 0, SWL_RC_CONTINUE, ME, "No listener");
    return SWL_RC_OK;
}

static swl_rc_ne s_mgmtFrameEvtCb(swl_unLiList_t* pListenerList, struct nlmsghdr* nlh, struct nlattr* tb[]) {
    swl_rc_ne rc = s_commonEvtCb(pListenerList, nlh, tb);
    ASSERTS_EQUALS(rc, SWL_RC_OK, rc, ME, "abort evt parsing");
    if(nlh->nlmsg_type != g_nl80211DriverIDs.family_id) {
        SAH_TRACEZ_INFO(ME, "skip msgtype %d", nlh->nlmsg_type);
        return SWL_RC_OK;
    }
    uint32_t wiphy = wld_nl80211_getWiphy(tb);
    uint32_t ifIndex = wld_nl80211_getIfIndex(tb);

    SAH_TRACEZ_INFO(ME, "frame received on w:%d,i:%d", wiphy, ifIndex);

    wld_nl80211_mgmtFrame_t mgmtFrame;
    memset(&mgmtFrame, 0, sizeof(wld_nl80211_mgmtFrame_t));

    rc = wld_nl80211_parseMgmtFrame(tb, &mgmtFrame);
    ASSERTS_EQUALS(rc, SWL_RC_OK, rc, ME, "Invalid frame");

    FOR_EACH_LISTENER(pListener, pListenerList, {
        pListener->handlers.fMgtFrameEvtCb(pListener->pRef, pListener->pData, mgmtFrame.frameLen, mgmtFrame.frame, mgmtFrame.rssi);
    });

    return SWL_RC_DONE;
}

static swl_rc_ne s_mgmtFrameTxStatusEvtCb(swl_unLiList_t* pListenerList, struct nlmsghdr* nlh, struct nlattr* tb[]) {
    swl_rc_ne rc = s_commonEvtCb(pListenerList, nlh, tb);
    ASSERT_EQUALS(rc, SWL_RC_OK, rc, ME, "abort evt parsing");

    if((nlh->nlmsg_type != g_nl80211DriverIDs.family_id) &&
       (nlh->nlmsg_type != g_nl80211DriverIDs.mlme_mcgrp_id)) {
        SAH_TRACEZ_INFO(ME, "skip msgtype %d", nlh->nlmsg_type);
        return SWL_RC_OK;
    }

    uint32_t wiphy = wld_nl80211_getWiphy(tb);
    uint32_t ifIndex = wld_nl80211_getIfIndex(tb);
    SAH_TRACEZ_INFO(ME, "Status of transmitted Mgmt frame reported on (w:%u, i:%u)", wiphy, ifIndex);

    wld_nl80211_mgmtFrameTxStatus_t mgmtFrameTxStatus;
    memset(&mgmtFrameTxStatus, 0, sizeof(wld_nl80211_mgmtFrameTxStatus_t));

    rc = wld_nl80211_parseMgmtFrameTxStatus(tb, &mgmtFrameTxStatus);
    ASSERT_EQUALS(rc, SWL_RC_OK, rc, ME, "Invalid frame");

    FOR_EACH_LISTENER(pListener, pListenerList, {
        pListener->handlers.fMgtFrameTxStatusEvtCb(pListener->pRef, pListener->pData, mgmtFrameTxStatus.frameLen, mgmtFrameTxStatus.frame, mgmtFrameTxStatus.ack);
    });

    return SWL_RC_DONE;
}

static swl_rc_ne s_unspecEvtCb(swl_unLiList_t* pListenerList, struct nlmsghdr* nlh, struct nlattr* tb[]) {
    swl_rc_ne rc = s_commonEvtCb(pListenerList, nlh, tb);
    ASSERTS_EQUALS(rc, SWL_RC_OK, rc, ME, "abort evt parsing");
    if(nlh->nlmsg_type != g_nl80211DriverIDs.family_id) {
        SAH_TRACEZ_INFO(ME, "skip msgtype %d", nlh->nlmsg_type);
        return SWL_RC_OK;
    }
    uint32_t wiphy = wld_nl80211_getWiphy(tb);
    uint32_t ifIndex = wld_nl80211_getIfIndex(tb);
    SAH_TRACEZ_INFO(ME, "unspec event notified on w:%d,i:%d", wiphy, ifIndex);
    FOR_EACH_LISTENER(pListener, pListenerList, {
        pListener->handlers.fUnspecEvtCb(pListener->pRef, pListener->pData, wiphy, ifIndex);
    });
    return SWL_RC_DONE;
}

static swl_rc_ne s_newWiphyEvtCb(swl_unLiList_t* pListenerList, struct nlmsghdr* nlh, struct nlattr* tb[]) {
    swl_rc_ne rc = s_commonEvtCb(pListenerList, nlh, tb);
    ASSERTS_EQUALS(rc, SWL_RC_OK, rc, ME, "abort evt parsing");
    if((nlh->nlmsg_type != g_nl80211DriverIDs.family_id) &&
       (nlh->nlmsg_type != g_nl80211DriverIDs.config_mcgrp_id)) {
        SAH_TRACEZ_INFO(ME, "skip msgtype %d", nlh->nlmsg_type);
        return SWL_RC_OK;
    }
    wld_nl80211_wiphyInfo_t wiphyInfo;
    memset(&wiphyInfo, 0, sizeof(wiphyInfo));
    rc = wld_nl80211_parseWiphyInfo(tb, &wiphyInfo);
    ASSERTS_FALSE(rc < SWL_RC_OK, rc, ME, "parsing failed");
    SAH_TRACEZ_INFO(ME, "new wiphy notified : w:%d,name:%s", wiphyInfo.wiphy, wiphyInfo.name);
    FOR_EACH_LISTENER(pListener, pListenerList, {
        pListener->handlers.fNewWiphyCb(pListener->pRef, pListener->pData, &wiphyInfo);
    });
    return SWL_RC_DONE;
}

static swl_rc_ne s_delWiphyEvtCb(swl_unLiList_t* pListenerList, struct nlmsghdr* nlh, struct nlattr* tb[]) {
    swl_rc_ne rc = s_commonEvtCb(pListenerList, nlh, tb);
    ASSERTS_EQUALS(rc, SWL_RC_OK, rc, ME, "abort evt parsing");
    if((nlh->nlmsg_type != g_nl80211DriverIDs.family_id) &&
       (nlh->nlmsg_type != g_nl80211DriverIDs.config_mcgrp_id)) {
        SAH_TRACEZ_INFO(ME, "skip msgtype %d", nlh->nlmsg_type);
        return SWL_RC_OK;
    }
    wld_nl80211_wiphyInfo_t wiphyInfo;
    memset(&wiphyInfo, 0, sizeof(wiphyInfo));
    rc = wld_nl80211_parseWiphyInfo(tb, &wiphyInfo);
    ASSERTS_FALSE(rc < SWL_RC_OK, rc, ME, "parsing failed");
    SAH_TRACEZ_INFO(ME, "deleted wiphy notified : w:%d,name:%s", wiphyInfo.wiphy, wiphyInfo.name);
    FOR_EACH_LISTENER(pListener, pListenerList, {
        pListener->handlers.fDelWiphyCb(pListener->pRef, pListener->pData, &wiphyInfo);
    });
    return SWL_RC_DONE;
}

static swl_rc_ne s_newInterfaceEvtCb(swl_unLiList_t* pListenerList, struct nlmsghdr* nlh, struct nlattr* tb[]) {
    swl_rc_ne rc = s_commonEvtCb(pListenerList, nlh, tb);
    ASSERTS_EQUALS(rc, SWL_RC_OK, rc, ME, "abort evt parsing");
    if((nlh->nlmsg_type != g_nl80211DriverIDs.family_id) &&
       (nlh->nlmsg_type != g_nl80211DriverIDs.config_mcgrp_id)) {
        SAH_TRACEZ_INFO(ME, "skip msgtype %d", nlh->nlmsg_type);
        return SWL_RC_OK;
    }
    wld_nl80211_ifaceInfo_t ifaceInfo;
    rc = wld_nl80211_parseInterfaceInfo(tb, &ifaceInfo);
    ASSERTS_FALSE(rc < SWL_RC_OK, rc, ME, "parsing failed");
    SAH_TRACEZ_INFO(ME, "new interface notified on w:%d,i:%d", ifaceInfo.wiphy, ifaceInfo.ifIndex);
    FOR_EACH_LISTENER(pListener, pListenerList, {
        pListener->handlers.fNewInterfaceCb(pListener->pRef, pListener->pData, &ifaceInfo);
    });
    return SWL_RC_DONE;
}

static swl_rc_ne s_vendorEvtCb(swl_unLiList_t* pListenerList, struct nlmsghdr* nlh, struct nlattr* tb[]) {
    swl_rc_ne rc = s_commonEvtCb(pListenerList, nlh, tb);
    ASSERTS_EQUALS(rc, SWL_RC_OK, rc, ME, "abort evt parsing");
    if((nlh->nlmsg_type != g_nl80211DriverIDs.family_id) &&
       (nlh->nlmsg_type != g_nl80211DriverIDs.vendor_grp_id)) {
        SAH_TRACEZ_INFO(ME, "skip msgtype %d", nlh->nlmsg_type);
        return SWL_RC_OK;
    }
    uint32_t wiphy = wld_nl80211_getWiphy(tb);
    uint32_t ifIndex = wld_nl80211_getIfIndex(tb);
    SAH_TRACEZ_INFO(ME, "vendor event received on w:%d,i:%d", wiphy, ifIndex);
    FOR_EACH_LISTENER(pListener, pListenerList, {
        pListener->handlers.fVendorEvtCb(pListener->pRef, pListener->pData, nlh, tb);
    });
    return SWL_RC_DONE;
}

static swl_rc_ne s_delInterfaceEvtCb(swl_unLiList_t* pListenerList, struct nlmsghdr* nlh, struct nlattr* tb[]) {
    swl_rc_ne rc = s_commonEvtCb(pListenerList, nlh, tb);
    ASSERTS_EQUALS(rc, SWL_RC_OK, rc, ME, "abort evt parsing");
    if((nlh->nlmsg_type != g_nl80211DriverIDs.family_id) &&
       (nlh->nlmsg_type != g_nl80211DriverIDs.config_mcgrp_id)) {
        SAH_TRACEZ_INFO(ME, "skip msgtype %d", nlh->nlmsg_type);
        return SWL_RC_OK;
    }
    wld_nl80211_ifaceInfo_t ifaceInfo;
    rc = wld_nl80211_parseInterfaceInfo(tb, &ifaceInfo);
    ASSERTS_FALSE(rc < SWL_RC_OK, rc, ME, "parsing failed");
    SAH_TRACEZ_INFO(ME, "del interface notified on w:%d,i:%d", ifaceInfo.wiphy, ifaceInfo.ifIndex);
    FOR_EACH_LISTENER(pListener, pListenerList, {
        pListener->handlers.fDelInterfaceCb(pListener->pRef, pListener->pData, &ifaceInfo);
    });
    return SWL_RC_DONE;
}

static swl_rc_ne s_scanStartedCb(swl_unLiList_t* pListenerList, struct nlmsghdr* nlh, struct nlattr* tb[]) {
    swl_rc_ne rc = s_commonEvtCb(pListenerList, nlh, tb);
    ASSERTS_EQUALS(rc, SWL_RC_OK, rc, ME, "abort evt parsing");
    if((nlh->nlmsg_type != g_nl80211DriverIDs.family_id) &&
       (nlh->nlmsg_type != g_nl80211DriverIDs.scan_mcgrp_id)) {
        SAH_TRACEZ_INFO(ME, "skip msgtype %d", nlh->nlmsg_type);
        return SWL_RC_OK;
    }
    uint32_t wiphy = wld_nl80211_getWiphy(tb);
    uint32_t ifIndex = wld_nl80211_getIfIndex(tb);
    SAH_TRACEZ_INFO(ME, "scan started on w:%d,i:%d", wiphy, ifIndex);
    FOR_EACH_LISTENER(pListener, pListenerList, {
        if(!wld_nl80211_hasStartedScan(pListener->pRef, wiphy, ifIndex)) {
            continue;
        }
        pListener->handlers.fScanStartedCb(pListener->pRef, pListener->pData, wiphy, ifIndex);
    });
    return SWL_RC_DONE;
}

static swl_rc_ne s_scanAbortedCb(swl_unLiList_t* pListenerList, struct nlmsghdr* nlh, struct nlattr* tb[]) {
    swl_rc_ne rc = s_commonEvtCb(pListenerList, nlh, tb);
    ASSERTS_EQUALS(rc, SWL_RC_OK, rc, ME, "abort evt parsing");
    if((nlh->nlmsg_type != g_nl80211DriverIDs.family_id) &&
       (nlh->nlmsg_type != g_nl80211DriverIDs.scan_mcgrp_id)) {
        SAH_TRACEZ_INFO(ME, "skip msgtype %d", nlh->nlmsg_type);
        return SWL_RC_OK;
    }
    uint32_t wiphy = wld_nl80211_getWiphy(tb);
    uint32_t ifIndex = wld_nl80211_getIfIndex(tb);
    SAH_TRACEZ_INFO(ME, "scan aborted on w:%d,i:%d", wiphy, ifIndex);
    FOR_EACH_LISTENER(pListener, pListenerList, {
        if(!wld_nl80211_hasStartedScan(pListener->pRef, wiphy, ifIndex)) {
            continue;
        }
        pListener->handlers.fScanAbortedCb(pListener->pRef, pListener->pData, wiphy, ifIndex);
    });
    return SWL_RC_DONE;
}

static swl_rc_ne s_scanResultsCb(swl_unLiList_t* pListenerList, struct nlmsghdr* nlh, struct nlattr* tb[]) {
    swl_rc_ne rc = s_commonEvtCb(pListenerList, nlh, tb);
    ASSERTS_EQUALS(rc, SWL_RC_OK, rc, ME, "abort evt parsing");
    if((nlh->nlmsg_type != g_nl80211DriverIDs.family_id) &&
       (nlh->nlmsg_type != g_nl80211DriverIDs.scan_mcgrp_id)) {
        SAH_TRACEZ_INFO(ME, "skip msgtype %d", nlh->nlmsg_type);
        return SWL_RC_OK;
    }
    uint32_t wiphy = wld_nl80211_getWiphy(tb);
    uint32_t ifIndex = wld_nl80211_getIfIndex(tb);
    ASSERTS_EQUALS(nlh->nlmsg_seq, 0, SWL_RC_DONE, ME, "not notif");
    SAH_TRACEZ_INFO(ME, "scan done on w:%d,i:%d", wiphy, ifIndex);
    FOR_EACH_LISTENER(pListener, pListenerList, {
        if(!wld_nl80211_hasStartedScan(pListener->pRef, wiphy, ifIndex)) {
            continue;
        }
        pListener->handlers.fScanDoneCb(pListener->pRef, pListener->pData, wiphy, ifIndex);
    });
    return SWL_RC_DONE;
}

static swl_rc_ne s_radarEvtCb(swl_unLiList_t* pListenerList, struct nlmsghdr* nlh, struct nlattr* tb[]) {
    swl_rc_ne rc = s_commonEvtCb(pListenerList, nlh, tb);
    ASSERTS_EQUALS(rc, SWL_RC_OK, rc, ME, "abort evt parsing");
    if((nlh->nlmsg_type != g_nl80211DriverIDs.family_id) &&
       (nlh->nlmsg_type != g_nl80211DriverIDs.mlme_mcgrp_id)) {
        SAH_TRACEZ_INFO(ME, "skip msgtype %d", nlh->nlmsg_type);
        return SWL_RC_OK;
    }
    wld_nl80211_radarEvtInfo_t radarEvtInfo;
    rc = wld_nl80211_parseRadarInfo(tb, &radarEvtInfo);
    ASSERTS_FALSE(rc < SWL_RC_OK, rc, ME, "parsing failed");
    SAH_TRACEZ_INFO(ME, "receive radar event %d on w:%d,i:%d", radarEvtInfo.event, radarEvtInfo.wiphy, radarEvtInfo.ifIndex);
    FOR_EACH_LISTENER(pListener, pListenerList, {
        pListener->handlers.fRadarEventCb(pListener->pRef, pListener->pData, &radarEvtInfo);
    });

    return SWL_RC_DONE;
}

#define OFFSET_UNDEF (-1)
#define MSG_ID_NAME(x) x, #x

/*
 * @brief table of supported nl80211 cmd IDs
 * These IDs may be used for requests, received as events, or both.
 * This table has to be updated with each newly supported event
 * by setting its specific parser function and the offset of the listener handler.
 * Eg:
 * {MSG_ID_NAME(NL80211_CMD_XXX), s_XXXEvtParser, offsetof(wld_nl80211_evtHandlers_cb, fXXXEvtHandler)},
 *
 * Received nl msgs are ignored when:
 * - CMD id is not in this table
 * - no specific msgParser is set (i.e msgParser is null or default (s_commonEvtCb))
 * - no listener callback is defined for it (OFFSET_UNDEF)
 * - no listener has registered a relative callback
 */
SWL_TABLE(sNl80211Msgs,
          ARR(uint32_t msgId; char* msgName; void* msgParser; int32_t msgHdlrOffset; ),
          ARR(swl_type_uint32, swl_type_charPtr, swl_type_voidPtr, swl_type_int32, ),
          ARR(/* only for test purpose. */
              {MSG_ID_NAME(NL80211_CMD_UNSPEC), s_unspecEvtCb, offsetof(wld_nl80211_evtHandlers_cb, fUnspecEvtCb)},
              /* Pure Commands (i.e CMD IDs only sent, never received*/
              {MSG_ID_NAME(NL80211_CMD_GET_WIPHY), NULL, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_SET_WIPHY), NULL, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_GET_INTERFACE), NULL, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_SET_INTERFACE), NULL, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_GET_STATION), NULL, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_REQ_SET_REG), NULL, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_GET_SCAN), NULL, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_ABORT_SCAN), NULL, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_GET_SURVEY), NULL, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_REGISTER_ACTION), NULL, OFFSET_UNDEF},
              /* Events */
              /* NL80211_MCGRP_SCAN */
              {MSG_ID_NAME(NL80211_CMD_TRIGGER_SCAN), s_scanStartedCb, offsetof(wld_nl80211_evtHandlers_cb, fScanStartedCb)},
              {MSG_ID_NAME(NL80211_CMD_SCAN_ABORTED), s_scanAbortedCb, offsetof(wld_nl80211_evtHandlers_cb, fScanAbortedCb)},
              {MSG_ID_NAME(NL80211_CMD_NEW_SCAN_RESULTS), s_scanResultsCb, offsetof(wld_nl80211_evtHandlers_cb, fScanDoneCb)},
              {MSG_ID_NAME(NL80211_CMD_START_SCHED_SCAN), s_commonEvtCb, OFFSET_UNDEF},
              /* NL80211_MCGRP_CONFIG */
              {MSG_ID_NAME(NL80211_CMD_NEW_WIPHY), s_newWiphyEvtCb, offsetof(wld_nl80211_evtHandlers_cb, fNewWiphyCb)},
              {MSG_ID_NAME(NL80211_CMD_DEL_WIPHY), s_delWiphyEvtCb, offsetof(wld_nl80211_evtHandlers_cb, fDelWiphyCb)},
              {MSG_ID_NAME(NL80211_CMD_NEW_INTERFACE), s_newInterfaceEvtCb, offsetof(wld_nl80211_evtHandlers_cb, fNewInterfaceCb)},
              {MSG_ID_NAME(NL80211_CMD_DEL_INTERFACE), s_delInterfaceEvtCb, offsetof(wld_nl80211_evtHandlers_cb, fDelInterfaceCb)},
              /* NL80211_MCGRP_MLME */
              {MSG_ID_NAME(NL80211_CMD_REMAIN_ON_CHANNEL), s_commonEvtCb, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_CANCEL_REMAIN_ON_CHANNEL), s_commonEvtCb, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_RADAR_DETECT), s_radarEvtCb, offsetof(wld_nl80211_evtHandlers_cb, fRadarEventCb)},
              {MSG_ID_NAME(NL80211_CMD_CH_SWITCH_STARTED_NOTIFY), s_commonEvtCb, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_CH_SWITCH_NOTIFY), s_commonEvtCb, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_NOTIFY_CQM), s_commonEvtCb, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_NEW_STATION), s_commonEvtCb, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_AUTHENTICATE), s_commonEvtCb, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_ASSOCIATE), s_commonEvtCb, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_DEAUTHENTICATE), s_commonEvtCb, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_DISASSOCIATE), s_commonEvtCb, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_UNPROT_DISASSOCIATE), s_commonEvtCb, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_UNPROT_DEAUTHENTICATE), s_commonEvtCb, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_CONNECT), s_commonEvtCb, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_CONN_FAILED), s_commonEvtCb, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_ROAM), s_commonEvtCb, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_DISCONNECT), s_commonEvtCb, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_NEW_PEER_CANDIDATE), s_commonEvtCb, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_MICHAEL_MIC_FAILURE), s_commonEvtCb, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_FRAME_TX_STATUS), s_mgmtFrameTxStatusEvtCb, offsetof(wld_nl80211_evtHandlers_cb, fMgtFrameTxStatusEvtCb)},
              {MSG_ID_NAME(NL80211_CMD_SET_REKEY_OFFLOAD), s_commonEvtCb, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_PMKSA_CANDIDATE), s_commonEvtCb, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_PROBE_CLIENT), s_commonEvtCb, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_FT_EVENT), s_commonEvtCb, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_STOP_AP), s_commonEvtCb, OFFSET_UNDEF},
              /* unicast */
              {MSG_ID_NAME(NL80211_CMD_UNEXPECTED_FRAME), s_commonEvtCb, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_UNEXPECTED_4ADDR_FRAME), s_commonEvtCb, OFFSET_UNDEF},
              {MSG_ID_NAME(NL80211_CMD_ACTION), s_mgmtFrameEvtCb, offsetof(wld_nl80211_evtHandlers_cb, fMgtFrameEvtCb)},
              /* vendor command */
              {MSG_ID_NAME(NL80211_CMD_VENDOR), s_vendorEvtCb, offsetof(wld_nl80211_evtHandlers_cb, fVendorEvtCb)},
              )
          );

const char* wld_nl80211_msgName(uint32_t cmd) {
    return swl_table_getMatchingValue(&sNl80211Msgs, 1, 0, &cmd);
}

wld_nl80211_evtParser_f wld_nl80211_getEventParser(uint32_t eventId) {
    wld_nl80211_evtParser_f* pfEvtHdlr = (wld_nl80211_evtParser_f*) swl_table_getMatchingValue(&sNl80211Msgs, 2, 0, &eventId);
    ASSERTS_NOT_NULL(pfEvtHdlr, NULL, ME, "no internal hdlr defined for evt(%d)", eventId);
    return *pfEvtHdlr;
}

bool wld_nl80211_hasEventHandler(wld_nl80211_listener_t* pListener, uint32_t eventId) {
    ASSERTS_NOT_NULL(pListener, false, ME, "NULL");
    int32_t* hdlrOffset = (int32_t*) swl_table_getMatchingValue(&sNl80211Msgs, 3, 0, &eventId);
    ASSERTI_NOT_NULL(hdlrOffset, false, ME, "Not found evt(%d)", eventId);
    ASSERTS_NOT_EQUALS(*hdlrOffset, OFFSET_UNDEF, false, ME, "no hdlr defined for evt(%d)", eventId);
    void* hdlr = *(void**) (((void*) &pListener->handlers) + *hdlrOffset);
    ASSERTS_NOT_NULL(hdlr, false, ME, "listener(w:%d,i:%d) has no hdlr evt(%d)", pListener->wiphy, pListener->ifIndex, eventId);
    return true;
}

swl_rc_ne wld_nl80211_updateEventHandlers(wld_nl80211_listener_t* pListener, const wld_nl80211_evtHandlers_cb* const handlers) {
    ASSERT_NOT_NULL(pListener, SWL_RC_ERROR, ME, "NULL");
    ASSERT_NOT_NULL(handlers, SWL_RC_ERROR, ME, "NULL");
    if(pListener->wiphy != WLD_NL80211_ID_UNDEF) {
        //Wiphy events handlers to be set here
        pListener->handlers.fNewInterfaceCb = handlers->fNewInterfaceCb;
        pListener->handlers.fDelInterfaceCb = handlers->fDelInterfaceCb;
        pListener->handlers.fScanStartedCb = handlers->fScanStartedCb;
        pListener->handlers.fScanAbortedCb = handlers->fScanAbortedCb;
        pListener->handlers.fScanDoneCb = handlers->fScanDoneCb;
        pListener->handlers.fMgtFrameEvtCb = handlers->fMgtFrameEvtCb;
        pListener->handlers.fRadarEventCb = handlers->fRadarEventCb;
    }
    if(pListener->ifIndex != WLD_NL80211_ID_UNDEF) {
        //Iface events handlers to be set here
        pListener->handlers.fMgtFrameTxStatusEvtCb = handlers->fMgtFrameTxStatusEvtCb;
    }
    //common events handlers to be set here
    pListener->handlers.fUnspecEvtCb = handlers->fUnspecEvtCb;
    pListener->handlers.fCheckTgtCb = handlers->fCheckTgtCb;
    pListener->handlers.fNewWiphyCb = handlers->fNewWiphyCb;
    pListener->handlers.fDelWiphyCb = handlers->fDelWiphyCb;
    return SWL_RC_OK;
}

bool wld_nl80211_getEvtListenerHandlers(wld_nl80211_listener_t* pListener, void** pData, wld_nl80211_evtHandlers_cb* pHandlers) {
    ASSERT_NOT_NULL(pListener, false, ME, "NULL");
    if(pData != NULL) {
        *pData = pListener->pData;
    }
    if(pHandlers != NULL) {
        *pHandlers = pListener->handlers;
    }
    return true;
}
