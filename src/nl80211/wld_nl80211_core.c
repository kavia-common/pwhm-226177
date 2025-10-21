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
#include "wld_nl80211_core_priv.h"
#include "wld_nl80211_events_priv.h"
#include "wld_nl80211_api_priv.h"
#include "wld_nl80211_attr.h"
#include <linux/netlink.h>
#include "swla/swla_time.h"

#define ME "nlCore"

#define NL_ALLOC_SIZE 32768 //default nl sock rcv buf size: 32k

wld_nl80211_driverIds_t g_nl80211DriverIDs = {
    .family_id = -1,
    .scan_mcgrp_id = -1,
    .config_mcgrp_id = -1,
    .mlme_mcgrp_id = -1,
    .vendor_grp_id = -1,
};

swl_rc_ne s_learnDriverIDs(struct nl_sock* pNlSock) {
    ASSERTS_NOT_NULL(pNlSock, SWL_RC_ERROR, ME, "Invalid fd");
    if(g_nl80211DriverIDs.family_id < 0) {
        g_nl80211DriverIDs.family_id = genl_ctrl_resolve(pNlSock, NL80211_GENL_NAME);
        ASSERT_FALSE(g_nl80211DriverIDs.family_id < 0, SWL_RC_ERROR, ME, "nl80211 not found");
    }
    if(g_nl80211DriverIDs.scan_mcgrp_id < 0) {
        g_nl80211DriverIDs.scan_mcgrp_id = genl_ctrl_resolve_grp(pNlSock, NL80211_GENL_NAME, NL80211_MULTICAST_GROUP_SCAN);
    }
    if(g_nl80211DriverIDs.mlme_mcgrp_id < 0) {
        g_nl80211DriverIDs.mlme_mcgrp_id = genl_ctrl_resolve_grp(pNlSock, NL80211_GENL_NAME, NL80211_MULTICAST_GROUP_MLME);
    }
    if(g_nl80211DriverIDs.config_mcgrp_id < 0) {
        g_nl80211DriverIDs.config_mcgrp_id = genl_ctrl_resolve_grp(pNlSock, NL80211_GENL_NAME, NL80211_MULTICAST_GROUP_CONFIG);
    }
    if(g_nl80211DriverIDs.vendor_grp_id < 0) {
        g_nl80211DriverIDs.vendor_grp_id = genl_ctrl_resolve_grp(pNlSock, NL80211_GENL_NAME, NL80211_MULTICAST_GROUP_VENDOR);
    }
    return SWL_RC_OK;
}

/*
 * @brief nl80211 request context
 */
typedef struct {
    amxc_llist_it_t it;            //iterator for list
    wld_nl80211_state_t* state;    //manager ctx, receiving nl reply
    uint32_t seqId;                //request's sequence number (unique identifier)
    swl_timeMono_t timestamp;      //request sending time
    uint32_t timeout;              //max time (in seconds) to finalize request
    void* priv;                    //user data
    wld_nl80211_handler_f handler; //handler to process replies
    bool handlerRunning;           //flag to note whether the handler is currently running
    swl_rc_ne* pStatus;            //pointer to status, where to save request result before clearing context.
    uint32_t cmd;
} nlRequest_t;

/*
 * @brief list of nl80211 socket managers
 */
amxc_llist_t gStates = {NULL, NULL};

static wld_nl80211_state_t* sSharedState = NULL;

/*
 * @brief private api to check validity of state pointer:
 * must be found in global states list
 *
 * @param state pointer to nl80211 socket manager
 *
 * @return true is state is valid
 *         false otherwise
 */
bool s_isValidState(const wld_nl80211_state_t* state) {
    ASSERTS_NOT_NULL(state, false, ME, "NULL");
    amxc_llist_for_each(it, &gStates) {
        if(state == amxc_llist_it_get_data(it, wld_nl80211_state_t, it)) {
            return true;
        }
    }
    return false;
}

wld_nl80211_state_t* wld_nl80211_getSharedState() {
    if(s_isValidState(sSharedState)) {
        return sSharedState;
    }
    sSharedState = wld_nl80211_newState();
    return sSharedState;
}

static nlRequest_t* s_allocRequest(wld_nl80211_state_t* state, uint32_t seqId) {
    nlRequest_t* pReq = calloc(1, sizeof(nlRequest_t));
    ASSERT_NOT_NULL(pReq, NULL, ME, "NULL");
    pReq->state = state;
    pReq->seqId = seqId;
    amxc_llist_append(&state->requests, &pReq->it);
    return pReq;
}

static void s_freeRequest(nlRequest_t* pReq) {
    ASSERTS_NOT_NULL(pReq, , ME, "NULL");
    amxc_llist_it_take(&pReq->it);
    free(pReq);
}

static nlRequest_t* s_findRequest(const wld_nl80211_state_t* state, uint32_t seqId) {
    ASSERTS_NOT_NULL(state, NULL, ME, NULL);
    nlRequest_t* pReq = NULL;
    amxc_llist_for_each(it, &state->requests) {
        pReq = amxc_llist_it_get_data(it, nlRequest_t, it);
        if(pReq->seqId == seqId) {
            return pReq;
        }
    }
    return NULL;
}

static wld_nl80211_listener_t* s_allocListener(wld_nl80211_state_t* state, uint32_t wiphy, uint32_t ifIndex) {
    ASSERT_NOT_NULL(state, NULL, ME, "NULL");
    if((ifIndex == 0) || ((wiphy == WLD_NL80211_ID_UNDEF) && (ifIndex == WLD_NL80211_ID_UNDEF))) {
        SAH_TRACEZ_ERROR(ME, "Invalid listener filter (wiphy:%d, iface:%d)", wiphy, ifIndex);
        return NULL;
    }
    wld_nl80211_listener_t* pListener = calloc(1, sizeof(wld_nl80211_listener_t));
    ASSERT_NOT_NULL(pListener, NULL, ME, "fail to alloc listener");
    pListener->state = state;
    pListener->wiphy = wiphy;
    pListener->ifIndex = ifIndex;
    SAH_TRACEZ_INFO(ME, "create event listener for wiphy:%d iface:%d over fd:%d", wiphy, ifIndex, state->nl_event);
    amxc_llist_append(&state->listeners, &pListener->it);
    return pListener;
}

static void s_freeListener(wld_nl80211_listener_t* pListener) {
    ASSERTS_NOT_NULL(pListener, , ME, "NULL");
    amxc_llist_it_take(&pListener->it);
    free(pListener);
}

static void s_freeListenerList(amxc_llist_t* pListenerList) {
    ASSERTS_NOT_NULL(pListenerList, , ME, "NULL");
    wld_nl80211_listener_t* pListener;
    amxc_llist_it_t* it = amxc_llist_get_first(pListenerList);
    while(it != NULL) {
        pListener = amxc_llist_it_get_data(it, wld_nl80211_listener_t, it);
        it = amxc_llist_it_get_next(it);
        s_freeListener(pListener);
    }
}

/*
 * @brief find registered listener in the state's listeners list
 *
 * @param state nl80211 sock manager context
 * @param pListener pointer to fetch
 *
 * @return pointer to listener context when found, or null otherwise
 */
static wld_nl80211_listener_t* s_findListener(wld_nl80211_state_t* state, wld_nl80211_listener_t* pListener) {
    ASSERTS_NOT_NULL(state, NULL, ME, NULL);
    ASSERTS_NOT_NULL(pListener, NULL, ME, NULL);
    amxc_llist_for_each(it, &state->listeners) {
        if(pListener == amxc_llist_it_get_data(it, wld_nl80211_listener_t, it)) {
            return pListener;
        }
    }
    return NULL;
}

/*
 * @brief fetch listener in all managers
 *
 * @param pListener pointer to fetch
 *
 * @return pointer to listener context when found, or null otherwise
 */
static wld_nl80211_listener_t* s_findListenerExt(wld_nl80211_listener_t* pListener) {
    ASSERTS_NOT_NULL(pListener, NULL, ME, NULL);
    wld_nl80211_state_t* state;
    amxc_llist_for_each(it, &gStates) {
        state = amxc_llist_it_get_data(it, wld_nl80211_state_t, it);
        if(s_findListener(state, pListener)) {
            return pListener;
        }
    }
    return NULL;
}

/*
 * @brief find registered listeners for specific event over wiphy/iface
 * The listener selected is sorted with some priority:
 * 1) on head, the iface listeners
 * 2) then, the default wiphy listeners (any iface)
 * 3) on tail, the global listeners (any wiphy)
 *
 * @param state nl80211 sock manager context
 * @param wiphy identifier of wiphy
 * @param ifIndex interface network id
 * @param cmd nl80211 event id
 * @param pList (output) list filled with clones of listeners matching the event/ifIndex/Wiphy
 *
 * @return pointer to listener context, or null otherwise
 */
int s_listenerCmp(const void* e1, const void* e2) {
    if(!e1 || !e2) {
        return 0;
    }
    wld_nl80211_listener_t* pL1 = *((wld_nl80211_listener_t**) e1);
    wld_nl80211_listener_t* pL2 = *((wld_nl80211_listener_t**) e2);
    //ifaceId1 < ifaceId2 < .... < ifaceIdAny < ifaceIdUndef
    if(((int) pL1->ifIndex > 0) || ((int) pL2->ifIndex > 0)) {
        return (pL2->ifIndex - pL1->ifIndex);
    }
    //wiphyId1 < wiphyId2 < .... < wiphyIdAny < wiphyIdUndef
    return (pL2->wiphy - pL1->wiphy);
}
static swl_rc_ne s_findListenersOfEvent(wld_nl80211_state_t* state, uint32_t wiphy, uint32_t ifIndex, uint32_t ctrlFreq, const char* ifName, uint32_t cmd, swl_unLiList_t* pList) {
    ASSERTS_NOT_NULL(state, SWL_RC_INVALID_PARAM, ME, NULL);
    ASSERTS_NOT_NULL(pList, SWL_RC_INVALID_PARAM, ME, NULL);
    swl_unLiList_init(pList, sizeof(wld_nl80211_listener_t*));
    ASSERTS_FALSE(amxc_llist_is_empty(&state->listeners), SWL_RC_OK, ME, "No listeners");
    uint32_t nSelectedListeners = 0;
    wld_nl80211_listener_t* selectedListeners[amxc_llist_size(&state->listeners)];
    amxc_llist_for_each(it, &state->listeners) {
        wld_nl80211_listener_t* pL = amxc_llist_it_get_data(it, wld_nl80211_listener_t, it);
        if(!wld_nl80211_hasEventHandler(pL, cmd)) {
            continue;
        }
        if((ifIndex == pL->ifIndex) ||
           ((wiphy == pL->wiphy) && (pL->ifIndex == WLD_NL80211_ID_ANY) &&
            (!pL->handlers.fCheckTgtCb || pL->handlers.fCheckTgtCb(pL->pRef, pL->pData, wiphy, ifIndex, ctrlFreq, ifName))) ||
           (pL->wiphy == WLD_NL80211_ID_ANY)) {
            SAH_TRACEZ_INFO(ME, "find listener(w:%d,i:%d) for evt(%d) over w:%d,i:%d",
                            pL->wiphy, pL->ifIndex, cmd, wiphy, ifIndex);
            selectedListeners[nSelectedListeners] = pL;
            nSelectedListeners++;
        }
    }
    qsort(selectedListeners, nSelectedListeners, sizeof(selectedListeners[0]), s_listenerCmp);
    for(uint32_t i = 0; i < nSelectedListeners; i++) {
        swl_unLiList_add(pList, &selectedListeners[i]);
    }
    return SWL_RC_OK;
}

/*
 * @brief handler of received nl msg event part
 * The event is checked for parsers, then for listener handler.
 * Then, it can be processed.
 *
 * @param state pointer to nl80211 socket manager
 * @param nlh netlink msg header
 *
 * @return retcode, possible values are:
 *         SWL_RC_OK: msg handled and wait for next one
 *         SWL_RC_CONTINUE: msg is ignored as can not be parsed as event
 *         SWL_RC_ERROR: msg handled but with error
 */
static swl_rc_ne s_handleEvent(wld_nl80211_state_t* state, struct nlmsghdr* nlh) {
    ASSERTS_NOT_NULL(nlh, SWL_RC_ERROR, ME, "NULL");
    void* msgData = nlmsg_data(nlh);
    ASSERTS_NOT_NULL(msgData, SWL_RC_ERROR, ME, "NULL");

    if(nlh->nlmsg_type == NLMSG_ERROR) {
        struct nlmsgerr* e = (struct nlmsgerr*) msgData;
        // error == 0 means ack/success
        ASSERT_EQUALS(e->error, 0, SWL_RC_ERROR, ME, "nl error msg, error:%d", e->error);
        return SWL_RC_OK;
    }

    // only process events including data
    ASSERTS_TRUE(nlh->nlmsg_type >= NLMSG_DONE, SWL_RC_OK, ME, "no event in msgtype %d", nlh->nlmsg_type);

    //fetch event parser
    struct genlmsghdr* gnlh = (struct genlmsghdr*) msgData;
    wld_nl80211_evtParser_f fEvtParser = wld_nl80211_getEventParser(gnlh->cmd);
    ASSERTS_NOT_NULL(fEvtParser, SWL_RC_CONTINUE, ME, "No parser for evt(%d) type(%d)", gnlh->cmd, nlh->nlmsg_type);

    //initial parsing of received msg
    struct nlattr* tb[NL80211_ATTR_MAX + 1] = {};
    if(nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0), genlmsg_attrlen(gnlh, 0), NULL)) {
        SAH_TRACEZ_ERROR(ME, "Failed to parse nl msg evt(%d)", gnlh->cmd);
        return SWL_RC_ERROR;
    }

    //fetch event listener
    uint32_t wiphy = wld_nl80211_getWiphy(tb);
    uint32_t ifIndex = wld_nl80211_getIfIndex(tb);
    char ifName[IFNAMSIZ] = {0};
    wld_nl80211_getIfName(tb, ifName, sizeof(ifName));
    uint32_t ctrlFreq = 0;
    if(tb[NL80211_ATTR_WIPHY_FREQ] != NULL) {
        ctrlFreq = nla_get_u32(tb[NL80211_ATTR_WIPHY_FREQ]);
    }
    swl_unLiList_t listeners;
    s_findListenersOfEvent(state, wiphy, ifIndex, ctrlFreq, ifName, gnlh->cmd, &listeners);
    ASSERTI_NOT_EQUALS(swl_unLiList_size(&listeners), 0, SWL_RC_CONTINUE, ME, "unhandled evt(%d:%s) (w:%d,i:%d)",
                       gnlh->cmd, wld_nl80211_msgName(gnlh->cmd), wiphy, ifIndex);
    //parse event and call listener handler
    SAH_TRACEZ_INFO(ME, "Processing evt(%d:%s) (w:%d,i:%d) and forward to %d listeners",
                    gnlh->cmd, wld_nl80211_msgName(gnlh->cmd), wiphy, ifIndex, swl_unLiList_size(&listeners));
    swl_rc_ne rc = fEvtParser(&listeners, nlh, tb);
    swl_unLiList_destroy(&listeners);
    return rc;
}

/*
 * @brief update and finalize a running request:
 * - call registered handler
 * - terminate processing if :
 *   + initial checks (based on msgtype) failed
 *   + or handler returns final retcode (DONE or ERROR)
 * - clear request when :
 *   + getting final retcode (DONE or ERROR)
 *   + request is handled but msg need to be forwarded as event
 *
 * @param state pointer to nl80211 socket manager
 * @param pReq pointer to request
 * @param handler user private handler
 * @param priv user private data
 * @param nlh netlink msg header
 * @param rc retcode from initial checks
 *
 * @return retcode, possible values are:
 * - SWL_RC_OK: msg handled and wait for next one
 * - SWL_RC_DONE: msg handled and request is terminated successfully
 * - <= SWL_RC_ERROR: msg handled and request is terminated with error
 *
 * When request is terminated (DONE, ERROR), pReq is no more valid after this function
 */
static swl_rc_ne s_processReply(wld_nl80211_state_t* state, nlRequest_t* pReq, swl_rc_ne* pStatus, wld_nl80211_handler_f handler, void* priv, struct nlmsghdr* nlh, swl_rc_ne rc) {
    if(handler) {
        // call request handler for all msgs
        if(pReq) {
            pReq->handlerRunning = true;
        }
        int hRet = handler(rc, nlh, priv);
        if(pReq) {
            pReq->handlerRunning = false;
        }
        // but save the returned result, only when not yet in error case
        if(rc == SWL_RC_OK) {
            rc = hRet;
        }
    }
    // If request handler has not confirmed end of processing of a last message
    // then assume that reply processing is terminated
    if((rc == SWL_RC_OK) && nlh && (!(nlh->nlmsg_flags & NLM_F_MULTI) || (nlh->nlmsg_type == NLMSG_DONE))) {
        rc = SWL_RC_DONE;
    }
    if(pStatus) {
        *pStatus = rc;
    }
    /*
     * clear request if :
     * 1) getting error
     * 2) receiving all multipart msg frags
     * 3) handler finish processing
     */
    if((rc <= SWL_RC_ERROR) || (rc == SWL_RC_DONE)) {
        if(pReq) {
            SAH_TRACEZ_INFO(ME, "no more processing: clear request seqId:%d rc:%d", pReq->seqId, rc);
            s_freeRequest(pReq);
        }
        ASSERTS_TRUE(s_isValidState(state), rc, ME, "Invalid state");
        if(rc == SWL_RC_DONE) {
            (state->counters.reqSuccess)++;
        } else {
            (state->counters.reqFailed)++;
        }
    }
    return rc;
}

/*
 * @brief process received msg and update request status
 * This a wrapper on top of processReply function, dereferencing the request context
 *
 * @param pReq pointer to request
 * @param nlh netlink msg header
 * @param rc retcode from initial checks
 *
 * @return retcode, possible values are:
 * - SWL_RC_OK: msg handled and wait for next one
 * - SWL_RC_DONE: msg handled and request is terminated successfully
 * - <= SWL_RC_ERROR: msg handled and request is terminated with error
 *
 * When request is terminated (DONE, ERROR), pReq is no more valid after this function
 */
static swl_rc_ne s_updateRequest(nlRequest_t* pReq, struct nlmsghdr* nlh, swl_rc_ne rc) {
    ASSERT_NOT_NULL(pReq, rc, ME, "NULL");
    return s_processReply(pReq->state, pReq, pReq->pStatus, pReq->handler, pReq->priv, nlh, rc);
}

/*
 * @brief handler of received nl msg reply part
 * The relative request is fetched using the included sequence number
 * Any identified request, having processed the reply, is then:
 * - pending (waiting for next msgs)
 * - terminated (done, error, or expired)
 *
 * @param state pointer to nl80211 socket manager
 * @param nlh netlink msg header
 *
 * @return retcode, possible values are:
 *         SWL_RC_OK: msg handled and wait for next one
 *         SWL_RC_CONTINUE: msg is ignored as not matching any pending request
 *         SWL_RC_DONE: msg handled and request is terminated successfully
 *         SWL_RC_ERROR: msg handled and request is terminated with error
 */
static swl_rc_ne s_handleReply(wld_nl80211_state_t* state, struct nlmsghdr* nlh) {
    ASSERTS_NOT_NULL(nlh, SWL_RC_ERROR, ME, "NULL");
    ASSERTS_NOT_EQUALS(nlh->nlmsg_type, NLMSG_NOOP, SWL_RC_OK, ME, "Message to be ignored");
    // match sequence number with request ctx
    nlRequest_t* pReq = s_findRequest(state, nlh->nlmsg_seq);
    ASSERTS_NOT_NULL(pReq, SWL_RC_CONTINUE, ME, "NULL");

    swl_rc_ne rc = SWL_RC_OK;
    void* msgData = nlmsg_data(nlh);
    struct nlmsgerr* e = (struct nlmsgerr*) msgData;
    if(nlh->nlmsg_type == NLMSG_DONE) {
        SAH_TRACEZ_INFO(ME, "The end of multipart message");
    } else if(nlh->nlmsg_type == NLMSG_OVERRUN) {
        //data lost: cancel request
        SAH_TRACEZ_WARNING(ME, "nl msg data lost");
        rc = SWL_RC_ERROR;
    } else if(msgData == NULL) {
        SAH_TRACEZ_ERROR(ME, "NULL msg data");
        rc = SWL_RC_ERROR;
    } else if((nlh->nlmsg_type == NLMSG_ERROR) && (e->error != 0)) {
        // error == 0 means ack/success
        int errId = (e->error > 0) ? e->error : -(e->error);
        char errMsg[256] = {0};
        strerror_r(errId, errMsg, sizeof(errMsg));
        int cmdId = pReq->cmd;
        const char* cmdStr = wld_nl80211_msgName(cmdId) ? : "unknown";
        SAH_TRACEZ_ERROR(ME, "nl error:%d (errno:%d:%s) msg:(cmd:%d:%s)",
                         e->error, errId, errMsg,
                         cmdId, cmdStr);
        rc = SWL_RC_ERROR;
    }
    return s_updateRequest(pReq, nlh, rc);
}

static void s_clearPendingRequests(wld_nl80211_state_t* state) {
    ASSERTS_NOT_NULL(state, , ME, "NULL");
    nlRequest_t* pReq;
    amxc_llist_it_t* it = amxc_llist_get_first(&state->requests);
    while(it != NULL) {
        pReq = amxc_llist_it_get_data(it, nlRequest_t, it);
        it = amxc_llist_it_get_next(it);
        s_freeRequest(pReq);
    }
}

static void s_clearExpiredRequests(wld_nl80211_state_t* state) {
    ASSERTS_NOT_NULL(state, , ME, "NULL");
    nlRequest_t* pReq = NULL;
    amxc_llist_it_t* it = amxc_llist_get_first(&state->requests);
    amxc_llist_it_t* itNext = NULL;
    while(it) {
        itNext = amxc_llist_it_get_next(it);
        pReq = amxc_llist_it_get_data(it, nlRequest_t, it);
        uint32_t diff = swl_time_getMonoSec() - pReq->timestamp;
        if((diff >= pReq->timeout) && !pReq->handlerRunning) {
            SAH_TRACEZ_WARNING(ME, "request %p (seqId:%d) expired (%d >= %d): terminate it",
                               pReq, pReq->seqId, diff, pReq->timeout);
            (state->counters.reqExpired)++;
            s_updateRequest(pReq, NULL, SWL_RC_NOT_AVAILABLE);
        }
        it = itNext;
    }
}

void wld_nl80211_clearAllExpiredRequests() {
    wld_nl80211_state_t* state;
    amxc_llist_for_each(it, &gStates) {
        state = amxc_llist_it_get_data(it, wld_nl80211_state_t, it);
        s_clearExpiredRequests(state);
    }
}

static int s_nlSend(const wld_nl80211_state_t* state, struct nl_sock* sk, struct nl_msg* msg) {
    if(state && state->fNlSendPriv) {
        return state->fNlSendPriv(sk, msg);
    }
    return nl_send(sk, msg);
}

static ssize_t s_recv(const wld_nl80211_state_t* state, int socket, void* buffer, size_t length, int flags) {
    if(state && state->fRecvPriv) {
        return state->fRecvPriv(socket, buffer, length, flags);
    }
    return recv(socket, buffer, length, flags);
}

static size_t s_getSockRcvBufSize(int fd) {
    int rcvBufSize = NL_ALLOC_SIZE;
    socklen_t sockOptSize = sizeof(rcvBufSize);
    // get nl sock recv buf dynamic size: it has been auto sized by nl_connect
    if(getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvBufSize, &sockOptSize) < 0) {
        return NL_ALLOC_SIZE;
    }
    //consider max size between driver value and the default one
    return SWL_MAX(rcvBufSize, NL_ALLOC_SIZE);
}

/*
 * @brief main netlink message read handler
 * It calls appropriate request or event handler
 * based on on the received msg parts sequence numbers
 */
static void s_readHandler(int fd, void* priv _UNUSED) {
    SAH_TRACEZ_IN(ME);
    ASSERTI_TRUE(fd > 0, , ME, "No peer");
    amxo_connection_t* con = amxo_connection_get(get_wld_plugin_parser(), fd);
    ASSERT_NOT_NULL(con, , ME, "con NULL");
    wld_nl80211_state_t* state = (wld_nl80211_state_t*) con->priv;
    ASSERT_TRUE(s_isValidState(state), , ME, "Invalid state");

    size_t bufSize = s_getSockRcvBufSize(fd);
    char* buf = calloc(1, bufSize);
    ASSERT_NOT_NULL(buf, , ME, "allocation of %zu failed", bufSize);

    int len = s_recv(state, fd, buf, bufSize, 0);
    struct nlmsghdr* nlh = NULL;
    for(nlh = (struct nlmsghdr*) buf; len > 0 && NLMSG_OK(nlh, (uint32_t) len); nlh = NLMSG_NEXT(nlh, len)) {
        if(!nlh) {
            break;
        }
        if(nlh->nlmsg_type == NLMSG_NOOP) {
            // Message to be ignored
            continue;
        }
        SAH_TRACEZ_INFO(ME, "nlmsg sock(%d) type(%d) seqId(%d) flags(0x%x)",
                        fd, nlh->nlmsg_type, nlh->nlmsg_seq, nlh->nlmsg_flags);

        swl_rc_ne rc = s_handleReply(state, nlh);
        if((rc == SWL_RC_CONTINUE) || (nlh->nlmsg_seq == 0)) {
            //event handler
            rc = s_handleEvent(state, nlh);
            if(nlh->nlmsg_type >= NLMSG_DONE) {
                //valid nl event
                (state->counters.evtTotal)++;
            }
            if(rc == SWL_RC_DONE) {
                //valid nl event parsed and handled by listener
                (state->counters.evtHandled)++;
            } else if(rc == SWL_RC_CONTINUE) {
                //valid nl event with no parser or no listener
                (state->counters.evtUnhandled)++;
            }
        }
        if(rc < SWL_RC_OK) {
            SAH_TRACEZ_INFO(ME, "stop reading nl msgs");
            break;
        }
    }

    free(buf);
    s_clearExpiredRequests(state);

    SAH_TRACEZ_OUT(ME);
}

wld_nl80211_listener_t* wld_nl80211_addEvtListener(wld_nl80211_state_t* state,
                                                   uint32_t wiphy, uint32_t ifIndex,
                                                   void* pRef, void* pData, const wld_nl80211_evtHandlers_cb* const handlers) {

    ASSERT_TRUE(s_isValidState(state), NULL, ME, "Invalid state");
    ASSERT_NOT_NULL(handlers, NULL, ME, "Missing handlers");
    wld_nl80211_listener_t* pListener = s_allocListener(state, wiphy, ifIndex);
    ASSERT_NOT_NULL(pListener, NULL, ME, "NULL");
    pListener->pRef = pRef;
    pListener->pData = pData;
    wld_nl80211_updateEventHandlers(pListener, handlers);
    if(wiphy != WLD_NL80211_ID_UNDEF) {
        //here, add membership for nl80211 multicast groups related to wiphy (SCAN, CONFIG)
        nl_socket_add_membership(state->nl_sock, g_nl80211DriverIDs.scan_mcgrp_id);
        nl_socket_add_membership(state->nl_sock, g_nl80211DriverIDs.config_mcgrp_id);
    }
    if(ifIndex != WLD_NL80211_ID_UNDEF) {
        //here, add membership for nl80211 multicast groups related to iface (MLME, CONFIG)
        nl_socket_add_membership(pListener->state->nl_sock, g_nl80211DriverIDs.mlme_mcgrp_id);
    }
    return pListener;
}

wld_nl80211_listener_t* wld_nl80211_addGlobalEvtListener(wld_nl80211_state_t* state,
                                                         void* pRef, void* pData, const wld_nl80211_evtHandlers_cb* const handlers) {
    return wld_nl80211_addEvtListener(state, WLD_NL80211_ID_ANY, WLD_NL80211_ID_ANY, pRef, pData, handlers);
}

wld_nl80211_listener_t* wld_nl80211_addVendorEvtListener(wld_nl80211_state_t* state,
                                                         wld_nl80211_listener_t* pListener,
                                                         wld_nl80211_vendorEvtCb_f handler) {
    ASSERT_NOT_NULL(pListener, NULL, ME, "NULL");
    SAH_TRACEZ_INFO(ME, "Add vendor's events listener %p", pListener);
    pListener->handlers.fVendorEvtCb = handler;
    if(nl_socket_add_membership(state->nl_sock, g_nl80211DriverIDs.vendor_grp_id) != 0) {
        SAH_TRACEZ_ERROR(ME, "failed to add vendor's events listener %p", pListener);
    }

    return pListener;
}

swl_rc_ne wld_nl80211_delEvtListener(wld_nl80211_listener_t** ppListener) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERTS_NOT_NULL(ppListener, rc, ME, "NULL");
    wld_nl80211_listener_t* pListener = s_findListenerExt(*ppListener);
    ASSERTS_NOT_NULL(pListener, rc, ME, "invalid listener");
    SAH_TRACEZ_INFO(ME, "remove listener %p wiphy(%d) ifIndex(%d)", pListener, pListener->wiphy, pListener->ifIndex);
    s_freeListener(pListener);
    *ppListener = NULL;
    return SWL_RC_OK;
}

static void s_clearEvtHandlers(wld_nl80211_state_t* state) {
    ASSERTS_NOT_NULL(state, , ME, "NULL");
    s_freeListenerList(&state->listeners);
    if(state->nl_sock) {
        //here, drop memberships of all nl80211 multicast groups
        nl_socket_drop_membership(state->nl_sock, g_nl80211DriverIDs.scan_mcgrp_id);
        nl_socket_drop_membership(state->nl_sock, g_nl80211DriverIDs.config_mcgrp_id);
        nl_socket_drop_membership(state->nl_sock, g_nl80211DriverIDs.mlme_mcgrp_id);
        if(g_nl80211DriverIDs.vendor_grp_id >= 0) {
            nl_socket_drop_membership(state->nl_sock, g_nl80211DriverIDs.vendor_grp_id);
        }
    }
}

void wld_nl80211_cleanup(wld_nl80211_state_t* state) {
    ASSERTS_TRUE(s_isValidState(state), , ME, "Invalid state");
    if(state->nl_event != 0) {
        amxo_connection_remove(get_wld_plugin_parser(), state->nl_event);
        state->nl_event = 0;
    }
    s_clearPendingRequests(state);
    s_clearEvtHandlers(state);
    if(state->nl_sock) {
        SAH_TRACEZ_INFO(ME, "free state->nl_sock");
        nl_socket_free(state->nl_sock);
        state->nl_sock = NULL;
    }
    amxc_llist_it_take(&state->it);
    free(state);
}

void wld_nl80211_cleanupAll() {
    wld_nl80211_state_t* state;
    amxc_llist_it_t* it = amxc_llist_get_first(&gStates);
    while(it != NULL) {
        state = amxc_llist_it_get_data(it, wld_nl80211_state_t, it);
        it = amxc_llist_it_get_next(it);
        wld_nl80211_cleanup(state);
    }
}

wld_nl80211_state_t* wld_nl80211_newState() {
    wld_nl80211_state_t* state = calloc(1, sizeof(wld_nl80211_state_t));
    ASSERT_NOT_NULL(state, NULL, ME, "NULL");

    amxc_llist_init(&state->requests);

    int opt = FALSE;

    state->nl_sock = nl_socket_alloc();
    if(!state->nl_sock) {
        SAH_TRACEZ_ERROR(ME, "Failed to allocate netlink socket");
        goto out_handle_destroy;
    }

    if(genl_connect(state->nl_sock)) {
        SAH_TRACEZ_ERROR(ME, "Failed to connect to generic netlink");
        goto out_handle_destroy;
    }

    //check nl80211 support
    if(s_learnDriverIDs(state->nl_sock) != SWL_RC_OK) {
        goto out_handle_destroy;
    }

    // set nl socket as non-blocking
    if(nl_socket_set_nonblocking(state->nl_sock) < 0) {
        SAH_TRACEZ_ERROR(ME, "set nl sock O_NONBLOCK failed: error:%d:%s", errno, strerror(errno));
        /* Not fatal, continue on.*/
    }

    int fd = nl_socket_get_fd(state->nl_sock);
    /* try to set NETLINK_EXT_ACK to 1, ignoring errors */
    opt = TRUE;
    setsockopt(fd, SOL_NETLINK,
               NETLINK_EXT_ACK, &opt, sizeof(opt));

    /* try to set NETLINK_CAP_ACK to 1, ignoring errors */
    opt = TRUE;
    setsockopt(fd, SOL_NETLINK,
               NETLINK_CAP_ACK, &opt, sizeof(opt));

    int ret = amxo_connection_add(get_wld_plugin_parser(), fd, s_readHandler, "readHandler", AMXO_CUSTOM, state);
    if(ret != 0) {
        SAH_TRACEZ_ERROR(ME, "amxo_connection_add() failed");
        goto out_handle_destroy;
    }
    state->nl_event = fd;

    amxc_llist_append(&gStates, &state->it);

    SAH_TRACEZ_INFO(ME, "Create nl80211 client ctx: famId:%d sock:%d", g_nl80211DriverIDs.family_id, fd);
    return state;

out_handle_destroy:
    wld_nl80211_cleanup(state);
    return NULL;
}

swl_rc_ne wld_nl80211_getStateCounters(wld_nl80211_state_t* state, wld_nl80211_stateCounters_t* pCounters) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_TRUE(s_isValidState(state), rc, ME, "Invalid state");
    ASSERT_NOT_NULL(pCounters, rc, ME, "NULL");
    memcpy(pCounters, &state->counters, sizeof(*pCounters));
    pCounters->reqPending = amxc_llist_size(&state->requests);
    pCounters->nStates = 1;
    return SWL_RC_OK;
}

swl_rc_ne wld_nl80211_getAllCounters(wld_nl80211_stateCounters_t* pCounters) {
    ASSERT_NOT_NULL(pCounters, SWL_RC_ERROR, ME, "NULL");
    memset(pCounters, 0, sizeof(*pCounters));
    wld_nl80211_stateCounters_t counters;
    wld_nl80211_state_t* state;
    amxc_llist_for_each(it, &gStates) {
        state = amxc_llist_it_get_data(it, wld_nl80211_state_t, it);
        if(wld_nl80211_getStateCounters(state, &counters) == SWL_RC_OK) {
            pCounters->reqTotal += counters.reqTotal;
            pCounters->reqSuccess += counters.reqSuccess;
            pCounters->reqFailed += counters.reqFailed;
            pCounters->reqExpired += counters.reqExpired;
            pCounters->reqPending += counters.reqPending;
            pCounters->evtTotal += counters.evtTotal;
            pCounters->evtHandled += counters.evtHandled;
            pCounters->evtUnhandled += counters.evtUnhandled;
            pCounters->nStates += counters.nStates;
        }
    }
    return SWL_RC_OK;
}

/*
 * @brief create new nl80211 msg, fill header with command id and initialize flags
 * (using learned nl80211 family id)
 *
 * @param cmd nl80211 Command id NL80211_CMD_xxx
 * @param flags initial flags of the msg (typically 0 or NL_F_DUMP for multipart reply)
 *
 * @return pointer to the built netlink message, null otherwise
 */
static struct nl_msg* s_newMsg(uint32_t cmd, size_t msgSize, uint32_t flags) {
    ASSERT_TRUE((g_nl80211DriverIDs.family_id > 0), NULL, ME, "Invalid nl80211 familty id");
    SAH_TRACEZ_INFO(ME, "cmd = %d", cmd);
    struct nl_msg* msg = (msgSize > 0) ? nlmsg_alloc_size(msgSize) : nlmsg_alloc();
    ASSERT_NOT_NULL(msg, NULL, ME, "NULL");
    genlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, g_nl80211DriverIDs.family_id, 0, flags, cmd, 0);
    return msg;
}

/*
 * @brief free netlink message
 *
 * @param msg pointer to message
 *
 * @return void
 */
static void s_freeMsg(struct nl_msg* msg) {
    ASSERTS_NOT_NULL(msg, , ME, "NULL");
    nlmsg_free(msg);
}

/*
 * @brief send asynchronous nl80211 message
 * When terminated (or expired), the provided handler is called
 *
 * @param state pointer to nl80211 socket manager used to send the commande
 * @param msg pointer to nl80211 request message
 * @param handler Callback function to be called for any received reply part
 * @param priv user data to provide when handler is called
 * @param timeout Max time (in seconds) to finalize request
 * @param pStatus (output) pointer to variable where to save current async request result
 * @param pSeqId (output) pointer to variable when to save request's sequence number
 *
 * @return SWL_RC_OK when request has been sent successfully
 *         SWL_RC_ERROR otherwise
 */
static swl_rc_ne s_sendMsg(wld_nl80211_state_t* state, struct nl_msg* msg,
                           wld_nl80211_handler_f handler, void* priv, uint32_t timeout, swl_rc_ne* pStatus, uint32_t* pSeqId) {
    SAH_TRACEZ_IN(ME);
    nlRequest_t* pReq = NULL;
    int rc = SWL_RC_ERROR;
    if((!s_isValidState(state)) || (state->nl_sock == NULL) || (msg == NULL)) {
        rc = SWL_RC_INVALID_PARAM;
        goto sending_error;
    }
    if(state->nl_event <= 0) {
        SAH_TRACEZ_ERROR(ME, "Not ready to receive replies");
        rc = SWL_RC_INVALID_STATE;
        goto sending_error;
    }
    nl_complete_msg(state->nl_sock, msg);
    struct nlmsghdr* hdr = nlmsg_hdr(msg);
    uint32_t seqId = hdr->nlmsg_seq;
    if(((pReq = s_findRequest(state, seqId)) == NULL) &&
       ((pReq = s_allocRequest(state, seqId)) == NULL)) {
        SAH_TRACEZ_ERROR(ME, "Fail to alloc request");
        goto sending_error;
    }
    pReq->handler = handler;
    pReq->priv = priv;
    pReq->pStatus = pStatus;
    pReq->timestamp = swl_time_getMonoSec();
    pReq->timeout = timeout;
    struct genlmsghdr* gnlh = (struct genlmsghdr*) nlmsg_data(hdr);
    if(gnlh) {
        pReq->cmd = gnlh->cmd;
    }
    SAH_TRACEZ_INFO(ME, "sending nl request with seqId:%d flags(0x%x)", seqId, hdr->nlmsg_flags);
    int nlRet = s_nlSend(state, state->nl_sock, msg);
    if(nlRet < 0) {
        SAH_TRACEZ_ERROR(ME, "fail to send nl request seqId:%d nlRet:%d:%s", seqId, nlRet, nl_geterror(nlRet));
        goto sending_error;
    }
    if(pSeqId) {
        *pSeqId = seqId;
    }
    (state->counters.reqTotal)++;
    SAH_TRACEZ_OUT(ME);
    return SWL_RC_OK;
sending_error:
    //ensure that the sending error is notified via the user handler
    rc = s_processReply(state, pReq, pStatus, handler, priv, NULL, rc);
    SAH_TRACEZ_INFO(ME, "Fail to send msg (rc:%d)", rc);
    return rc;
}

static swl_rc_ne s_sendMsgSync(wld_nl80211_state_t* state, struct nl_msg* msg, wld_nl80211_handler_f handler, void* priv) {
    SAH_TRACEZ_IN(ME);
    uint32_t seqId = 0;
    swl_rc_ne rc = s_sendMsg(state, msg, handler, priv, REQUEST_SYNC_TIMEOUT, &rc, &seqId);
    ASSERT_EQUALS(rc, SWL_RC_OK, rc, ME, "Fail to send msg");

    //request remains until it is terminated
    fd_set rfds;
    while(s_findRequest(state, seqId) != NULL) {
        int fd = state->nl_event;
        if(fd <= 0) {
            SAH_TRACEZ_ERROR(ME, "invalid peer fd");
            rc = SWL_RC_ERROR;
            break;
        }
        FD_ZERO(&rfds);
        FD_SET((uint32_t) fd, &rfds);
        struct timeval timeout = {0, 200000};
        //use select to temporize reading from non-blocking socket
        select(fd + 1, &rfds, NULL, NULL, &timeout);
        s_readHandler(state->nl_event, priv);
    }
    SAH_TRACEZ_INFO(ME, "retcode = %d", rc);
    //simplify result: all success retcodes mean successful operation
    if(rc >= SWL_RC_OK) {
        rc = SWL_RC_OK;
    }
    SAH_TRACEZ_OUT(ME);
    return rc;
}

swl_rc_ne wld_nl80211_addNlAttrs(struct nl_msg* msg, wld_nl80211_nlAttrList_t* const pAttrList) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERTS_NOT_NULL(msg, rc, ME, "NULL");
    ASSERTS_NOT_NULL(pAttrList, SWL_RC_OK, ME, "NULL");
    ASSERTS_TRUE((swl_unLiList_size(pAttrList) > 0), SWL_RC_OK, ME, "Empty list");
    rc = SWL_RC_ERROR;
    swl_unLiListIt_t it;
    swl_unLiList_for_each(it, pAttrList) {
        wld_nl80211_nlAttr_t* pAttr = swl_unLiList_data(&it, wld_nl80211_nlAttr_t*);
        //skip empty/unspecified attributes
        if(pAttr == NULL) {
            continue;
        }
        //manage nested attributes
        if(pAttr->nested) {
            //skip nested when no attributes inside
            if(swl_unLiList_size(&pAttr->data.attribs) == 0) {
                continue;
            }
            struct nlattr* nestedAttr = nla_nest_start(msg, pAttr->type);
            ASSERT_NOT_NULL(nestedAttr, rc, ME, "fail to start nested attr(%d)", pAttr->type);
            rc = wld_nl80211_addNlAttrs(msg, &pAttr->data.attribs);
            ASSERTS_EQUALS(rc, SWL_RC_OK, rc, ME, "fail to add nested attr(%d)", pAttr->type)
            nla_nest_end(msg, nestedAttr);
            continue;
        }
        //add simple attributes (may have no data)
        uint32_t len = pAttr->data.raw.len;
        const void* data = pAttr->data.raw.ptr;
        if(len == 0) {
            data = NULL;
        } else if(data == NULL) {
            len = 0;
        }
        ASSERT_FALSE((nla_put(msg, pAttr->type, len, data) < 0),
                     SWL_RC_ERROR, ME, "Fail to add attrib(%d) (len:%d,data:%p)",
                     pAttr->type, len, data);
    }
    return SWL_RC_OK;
}

static struct nl_msg* s_createNlMsg(uint32_t cmd, size_t attrSize, uint32_t flags, uint32_t ifIndex) {
    SAH_TRACEZ_INFO(ME, "create nlmsg cmd(%d)(%s) ifIndex(%d)", cmd, wld_nl80211_msgName(cmd), ifIndex);
    /* Netlink Header + Generic Header + attrSize */
    size_t msgSize = NLMSG_LENGTH(GENL_HDRLEN + NLMSG_ALIGN(attrSize));
    msgSize += (ifIndex > 0) ? (NLA_HDRLEN + NLA_ALIGN(sizeof(uint32_t))) : 0;
    struct nl_msg* msg = s_newMsg(cmd, msgSize, flags);
    ASSERT_NOT_NULL(msg, NULL, ME, "Fail to create nl msg");
    if(ifIndex > 0) {
        nla_put_u32(msg, NL80211_ATTR_IFINDEX, ifIndex);
    }
    return msg;
}

static size_t s_getNlMsgAttrSize(wld_nl80211_nlAttrList_t* const pAttrList) {
    ASSERTS_NOT_NULL(pAttrList, 0, ME, "NULL");
    size_t size = 0;
    swl_unLiListIt_t it;
    swl_unLiList_for_each(it, pAttrList) {
        wld_nl80211_nlAttr_t* pAttr = swl_unLiList_data(&it, wld_nl80211_nlAttr_t*);
        if(pAttr == NULL) {
            continue;
        }
        size += NLA_HDRLEN;
        size += pAttr->nested ? s_getNlMsgAttrSize(&pAttr->data.attribs) : NLA_ALIGN(pAttr->data.raw.len);
    }
    return NLA_ALIGN(size);
}

swl_rc_ne wld_nl80211_sendCmd(bool isSync, wld_nl80211_state_t* state, uint32_t cmd, uint32_t flags,
                              uint32_t ifIndex, wld_nl80211_nlAttrList_t* const pAttrList,
                              wld_nl80211_handler_f handler, void* priv, swl_rc_ne* pResult) {
    swl_rc_ne rc = SWL_RC_ERROR;
    size_t attrSize = s_getNlMsgAttrSize(pAttrList);
    struct nl_msg* msg = s_createNlMsg(cmd, attrSize, flags, ifIndex);
    ASSERT_NOT_NULL(msg, SWL_RC_ERROR, ME, "create nlmsg failed");
    rc = wld_nl80211_addNlAttrs(msg, pAttrList);
    if(rc < SWL_RC_OK) {
        SAH_TRACEZ_ERROR(ME, "error addind nlmsg attributes");
        s_freeMsg(msg);
        return rc;
    }
    if(isSync) {
        rc = s_sendMsgSync(state, msg, handler, priv);
        if(pResult) {
            *pResult = rc;
        }
    } else {
        //no need to get the request sequence number
        rc = s_sendMsg(state, msg, handler, priv, REQUEST_ASYNC_TIMEOUT, pResult, NULL);
    }
    s_freeMsg(msg);
    return rc;
}

swl_rc_ne wld_nl80211_sendCmdSync(wld_nl80211_state_t* state, uint32_t cmd, uint32_t flags,
                                  uint32_t ifIndex, wld_nl80211_nlAttrList_t* const pAttrList,
                                  wld_nl80211_handler_f handler, void* priv) {
    return wld_nl80211_sendCmd(true, state, cmd, flags, ifIndex, pAttrList, handler, priv, NULL);
}

/*
 * @brief default handler for request acknowledgment
 *
 * @param state nl80211 socket manager context
 * @param rc result of reply initial checks
 * @param nlh netlink msg header
 * @param priv user private data
 *
 * @return SWL_RC_OK msg valid but ignored
 *         SWL_RC_DONE msg is a valid: request is terminated with success
 *         <= SWL_RC_ERROR : request is terminated with error
 */
static swl_rc_ne s_defaultAckCb(swl_rc_ne rc _UNUSED, struct nlmsghdr* nlh, void* priv _UNUSED) {
    ASSERTS_FALSE((rc <= SWL_RC_ERROR), rc, ME, "Request error");
    ASSERT_NOT_NULL(nlh, SWL_RC_ERROR, ME, "NULL");
    ASSERTS_EQUALS(nlh->nlmsg_type, NLMSG_ERROR, SWL_RC_OK, ME, "not error msg");
    struct nlmsgerr* e = (struct nlmsgerr*) nlmsg_data(nlh);
    ASSERTI_NOT_EQUALS(e->error, 0, SWL_RC_DONE, ME, "request seqId %d acknowledged", nlh->nlmsg_seq);
    return SWL_RC_ERROR;
}

swl_rc_ne wld_nl80211_sendCmdSyncWithAck(wld_nl80211_state_t* state, uint32_t cmd, uint32_t flags,
                                         uint32_t ifIndex, wld_nl80211_nlAttrList_t* const pAttrList) {
    return wld_nl80211_sendCmdSync(state, cmd, flags, ifIndex, pAttrList, s_defaultAckCb, NULL);
}

