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
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <setjmp.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cmocka.h>

#include <debug/sahtrace.h>

#include <net/if.h>
#include <fcntl.h>
#include "wld_nl80211_core.h"
#include "wld_nl80211_api.h"
#include "wld_nl80211_events.h"
#include "wld_nl80211_utils.h"
#include "wld_radio.h"
#include "swl/swl_common.h"
#include "swl/swl_80211.h"
#include "test-toolbox/ttb_amx.h"
#include "test-toolbox/ttb_mockTimer.h"

static int s_loIfIndex = 0;
static wld_nl80211_state_t* s_sharedState = NULL;

static int setup_suite(void** state) {
    ttb_amx_t* ttbAmx = ttb_amx_init();
    assert_non_null(ttbAmx);
    *state = ttbAmx;
    wld_plugin_init(&ttbAmx->dm, &ttbAmx->parser);
    s_loIfIndex = if_nametoindex("lo");
    return 0;
}

static int teardown_suite(void** state) {
    wld_nl80211_cleanupAll();
    ttb_amx_cleanup(*state);
    *state = NULL;
    return 0;
}

static void test_wld_nl80211_newState(void** mockaState _UNUSED) {
    //create nl80211 socket manager
    wld_nl80211_state_t* state = wld_nl80211_newState();
    assert_non_null(state);
    //delete nl80211 socket manager
    wld_nl80211_cleanup(state);
}

static void test_wld_nl80211_getSharedState(void** mockaState _UNUSED) {
    //get shared nl80211 socket manager
    s_sharedState = wld_nl80211_getSharedState();
    assert_non_null(s_sharedState);
}

static void test_wld_nl80211_setEvtListeners(void** mockaState _UNUSED) {
    //create nl80211 socket manager
    wld_nl80211_state_t* state = wld_nl80211_newState();
    assert_non_null(state);
    wld_nl80211_evtHandlers_cb handlers = {};
    //register global listener, with no handlers
    wld_nl80211_listener_t* globalListener = wld_nl80211_addGlobalEvtListener(state, NULL, NULL, &handlers);
    assert_non_null(globalListener);
    //register listener for wiphy 0, with no handlers
    wld_nl80211_listener_t* wiphy0Listener = wld_nl80211_addEvtListener(state, 0, WLD_NL80211_ID_ANY, NULL, NULL, &handlers);
    assert_non_null(wiphy0Listener);
    //register listener for "lo" interface (dummy), with no handlers
    wld_nl80211_listener_t* loListener = wld_nl80211_addEvtListener(state, WLD_NL80211_ID_UNDEF, s_loIfIndex, NULL, NULL, &handlers);
    assert_non_null(loListener);
    //remove default listener
    assert_int_equal(wld_nl80211_delEvtListener(&globalListener), SWL_RC_OK);
    //remove wiphy 0 events listener
    assert_int_equal(wld_nl80211_delEvtListener(&wiphy0Listener), SWL_RC_OK);
    //Keep "lo" events listener allocated, to be freed when state is cleaned up
    //try to remove invalid listener
    assert_int_equal(wld_nl80211_delEvtListener((wld_nl80211_listener_t**) &state), SWL_RC_INVALID_PARAM);
    //delete nl80211 socket manager
    wld_nl80211_cleanup(state);
    //try to remove listener already cleared by state cleanup
    assert_int_equal(wld_nl80211_delEvtListener(&loListener), SWL_RC_INVALID_PARAM);
}

//Tests of private (internal) apis
#include "wld_nl80211_api_priv.h"
#include "wld_nl80211_debug.h"

typedef struct {
    char message[128];
    int error;
    swl_rc_ne rc;
    swl_timeMono_t timestamp;
} testData_t;

typedef struct {
    wld_nl80211_state_t* state;
    uint32_t cmd;
    uint32_t flags;
    uint32_t ifIndex;
    wld_nl80211_nlAttrList_t attrList;
    testData_t data;
    swl_timeMono_t timestart;
    swl_rc_ne expectedRc;
    testData_t expectedData;
    uint32_t expectedTime;
} testDesc_t;

static void test_wld_nl80211_sendCmdSyncAck(void** mockaState _UNUSED) {
    swl_rc_ne rc = SWL_RC_ERROR;
    //create nl80211 socket manager
    wld_nl80211_state_t* state = wld_nl80211_newState();
    assert_non_null(state);
    //delete nl80211 socket manager
    wld_nl80211_cleanup(state);
    testDesc_t tests[] = {
        //error case: try to send cmd without creating nl80211State
        {.cmd = NL80211_CMD_UNSPEC, .ifIndex = s_loIfIndex, .expectedRc = SWL_RC_INVALID_PARAM, },
        //error case: try to send cmd with wrong nl80211State
        {.state = (wld_nl80211_state_t*) &rc, .cmd = NL80211_CMD_GET_INTERFACE, .ifIndex = s_loIfIndex, .expectedRc = SWL_RC_INVALID_PARAM},
        //error case: try to send cmd with deleted mgr
        {.state = state, .cmd = NL80211_CMD_GET_INTERFACE, .flags = NLM_F_DUMP, .expectedRc = SWL_RC_INVALID_PARAM},
        //success: get available nl80211 interfaces, using common nl80211State
        {.state = s_sharedState, .cmd = NL80211_CMD_GET_INTERFACE, .flags = NLM_F_DUMP, .expectedRc = SWL_RC_OK},
    };
    for(uint32_t i = 0; i < SWL_ARRAY_SIZE(tests); i++) {
        rc = wld_nl80211_sendCmdSyncWithAck(tests[i].state, tests[i].cmd, tests[i].flags, tests[i].ifIndex, NULL);
        assert_int_equal(rc, tests[i].expectedRc);
    }
}

static void test_wld_nl80211_sendCmdWithAttrs(void** mockaState _UNUSED) {
    swl_rc_ne rc = SWL_RC_ERROR;
    testDesc_t tests[] = {
        {.state = s_sharedState, .cmd = NL80211_CMD_SET_INTERFACE, .ifIndex = s_loIfIndex, .expectedRc = SWL_RC_ERROR},
        {.state = s_sharedState, .cmd = NL80211_CMD_TRIGGER_SCAN, .ifIndex = s_loIfIndex, .expectedRc = SWL_RC_ERROR},
    };

    //simple attributes
    uint32_t ifType = NL80211_IFTYPE_AP;
    uint8_t use4Mac = true;
    NL_ATTRS_SET(&tests[0].attrList,
                 ARR(NL_ATTR_VAL(NL80211_ATTR_IFTYPE, ifType),
                     NL_ATTR_VAL(NL80211_ATTR_4ADDR, use4Mac),
                     NL_ATTR_DATA(NL80211_ATTR_MAC, SWL_MAC_BIN_LEN, g_swl_macBin_null.bMac)));

    //simple and nested attributes
    uint32_t flags = NL80211_SCAN_FLAG_FLUSH;
    NL_ATTRS_SET(&tests[1].attrList,
                 ARR(NL_ATTR_VAL(NL80211_ATTR_SCAN_FLAGS, flags)));

    NL_ATTR_NESTED(ssidsAttr, NL80211_ATTR_SCAN_SSIDS);
    char* ssids[] = {"scanSSID1", "scanSSID2"};
    for(uint32_t i = 0; i < SWL_ARRAY_SIZE(ssids); i++) {
        NL_ATTRS_ADD(&ssidsAttr.data.attribs,
                     NL_ATTR_DATA(swl_unLiList_size(&ssidsAttr.data.attribs) + 1, strlen(ssids[i]), ssids[i]));
    }
    swl_unLiList_add(&tests[1].attrList, &ssidsAttr);

    NL_ATTR_NESTED(freqsAttr, NL80211_ATTR_SCAN_FREQUENCIES);
    uint32_t freqs[] = {2437, };
    for(uint32_t i = 0; i < SWL_ARRAY_SIZE(freqs); i++) {
        NL_ATTRS_ADD(&freqsAttr.data.attribs,
                     NL_ATTR_VAL(swl_unLiList_size(&freqsAttr.data.attribs) + 1, freqs[i]));
    }
    swl_unLiList_add(&tests[1].attrList, &freqsAttr);

    //send requests
    for(uint32_t i = 0; i < SWL_ARRAY_SIZE(tests); i++) {
        rc = wld_nl80211_sendCmdSyncWithAck(tests[i].state, tests[i].cmd, tests[i].flags, tests[i].ifIndex, &tests[i].attrList);
        assert_int_equal(rc, tests[i].expectedRc);
        NL_ATTRS_CLEAR(&tests[i].attrList);
    }
}

static swl_rc_ne s_cmdReplyCb (swl_rc_ne rc, struct nlmsghdr* nlh _UNUSED, void* priv) {
    testData_t* pData = (testData_t*) priv;
    if(pData == NULL) {
        return SWL_RC_INVALID_PARAM;
    }
    pData->timestamp = swl_time_getMonoSec();
    if(rc < SWL_RC_OK) {
        swl_str_cat(pData->message, sizeof(pData->message), "Error");
        if(nlh) {
            struct nlmsgerr* e = (struct nlmsgerr*) nlmsg_data(nlh);
            pData->error = e->error;
        }
        return rc;
    }
    swl_str_cat(pData->message, sizeof(pData->message), "Success");
    return rc;
}

static uint32_t gLastSentSeqId = 0;

static int s_sendNothing(struct nl_sock* sock _UNUSED, struct nl_msg* msg) {
    gLastSentSeqId = nlmsg_hdr(msg)->nlmsg_seq;
    return 0;
}

static int s_sendNothingAndJumpToSyncTimeout(struct nl_sock* sock _UNUSED, struct nl_msg* msg _UNUSED) {
    ttb_mockTimer_goToFutureSec(REQUEST_SYNC_TIMEOUT);
    return 0;
}

static void test_wld_nl80211_sendCmdSync(void** mockaState _UNUSED) {
    //create nl80211 socket manager
    wld_nl80211_state_t* state = wld_nl80211_newState();
    assert_non_null(state);
    //tweak: override nl_send API to systematically drop cmd, and make request expire
    state->fNlSendPriv = s_sendNothing;

    swl_rc_ne rc;
    testDesc_t tests[] = {
        //error case: unsupported cmd: error:-95:Operation not supported
        {.state = s_sharedState, .cmd = NL80211_CMD_UNSPEC, .ifIndex = s_loIfIndex, .expectedRc = SWL_RC_NOT_AVAILABLE, .expectedData = {.message = "Error", .error = -EOPNOTSUPP, }},
        //error case: not nl80211 interface: error:-19:No such device
        {.state = s_sharedState, .cmd = NL80211_CMD_GET_INTERFACE, .ifIndex = s_loIfIndex, .expectedRc = SWL_RC_ERROR, .expectedData = {.message = "Error", .error = -ENODEV, }},
        //error case: invalid ifIndex: error:-22:Invalid argument
        {.state = s_sharedState, .cmd = NL80211_CMD_GET_INTERFACE, .expectedRc = SWL_RC_ERROR, .expectedData = {.message = "Error", .error = -EINVAL, }},
        //error case: request timeouted
        {.state = state, .cmd = NL80211_CMD_GET_INTERFACE, .flags = NLM_F_DUMP, .expectedRc = SWL_RC_NOT_AVAILABLE, .expectedData = {.message = "Error", }, .expectedTime = REQUEST_SYNC_TIMEOUT, },
        //success: get available nl80211 interfaces
        {.state = s_sharedState, .cmd = NL80211_CMD_GET_INTERFACE, .flags = NLM_F_DUMP, .expectedRc = SWL_RC_OK, .expectedData = {.message = "Success", }},
    };
    for(uint32_t i = 0; i < SWL_ARRAY_SIZE(tests); i++) {
        tests[i].timestart = swl_time_getMonoSec();
        rc = wld_nl80211_sendCmdSync(tests[i].state, tests[i].cmd, tests[i].flags, tests[i].ifIndex, NULL, s_cmdReplyCb, &tests[i].data);
        uint32_t diff = tests[i].data.timestamp - tests[i].timestart;
        assert_int_equal(rc, tests[i].expectedRc);
        assert_string_equal(tests[i].data.message, tests[i].expectedData.message);
        assert_int_equal(tests[i].data.error, tests[i].expectedData.error);
        assert_in_range(diff, tests[i].expectedTime, tests[i].expectedTime + 1);
    }
    //restore native nl_send
    state->fNlSendPriv = NULL;
    //delete nl80211 socket manager
    wld_nl80211_cleanup(state);
}

static void s_runEventLoopIter() {
    fd_set rfds;
    amxc_llist_t* connList = amxo_parser_get_connections(get_wld_plugin_parser());
    if(amxc_llist_is_empty(connList)) {
        return;
    }
    int res = -1;
    //temporize reading
    struct timeval timeout = {0, 200000};
    do {
        int fd = -1;
        FD_ZERO(&rfds);
        amxc_llist_for_each(it, connList) {
            amxo_connection_t* conn = amxc_container_of(it, amxo_connection_t, it);
            FD_SET((uint32_t) conn->fd, &rfds);
            fd = SWL_MAX(fd, conn->fd);
        }
        if(fd <= 0) {
            break;
        }
        res = select(fd + 1, &rfds, NULL, NULL, &timeout);
        if(res > 0) {
            //process all available events (on any connection)
            amxc_llist_for_each(it, connList) {
                amxo_connection_t* conn = amxc_container_of(it, amxo_connection_t, it);
                if(FD_ISSET(conn->fd, &rfds)) {
                    conn->reader(conn->fd, conn->priv);
                }
            }
        } else if((res < 0) && (errno == EINTR)) {
            continue;
        }
    } while(res > 0);
    //if time-outing, check and clear expired requests
    wld_nl80211_clearAllExpiredRequests();
}

static void test_wld_nl80211_sendCmdAsync(void** mockaState _UNUSED) {
    //create nl80211 socket manager
    wld_nl80211_state_t* state = wld_nl80211_newState();
    assert_non_null(state);
    //tweak: override nl_send API to systematically drop cmd, and make request expire
    state->fNlSendPriv = s_sendNothing;

    swl_rc_ne rc;
    testDesc_t tests[] = {
        //error case: unsupported cmd: error:-95:Operation not supported
        {.state = s_sharedState, .cmd = NL80211_CMD_UNSPEC, .ifIndex = s_loIfIndex, .expectedRc = SWL_RC_NOT_AVAILABLE, .expectedData = {.message = "Error", .error = -EOPNOTSUPP, }},
        //error case: not nl80211 interface: error:-19:No such device
        {.state = s_sharedState, .cmd = NL80211_CMD_GET_INTERFACE, .ifIndex = s_loIfIndex, .expectedRc = SWL_RC_ERROR, .expectedData = {.message = "Error", .error = -ENODEV, }},
        //error case: invalid ifIndex: error:-22:Invalid argument
        {.state = s_sharedState, .cmd = NL80211_CMD_GET_INTERFACE, .expectedRc = SWL_RC_ERROR, .expectedData = {.message = "Error", .error = -EINVAL, }},
        //error case: request timeouted
        {.state = state, .cmd = NL80211_CMD_GET_INTERFACE, .flags = NLM_F_DUMP, .expectedRc = SWL_RC_NOT_AVAILABLE, .expectedData = {.message = "Error", }, .expectedTime = REQUEST_ASYNC_TIMEOUT, },
        //success: get available nl80211 interfaces
        {.state = s_sharedState, .cmd = NL80211_CMD_GET_INTERFACE, .flags = NLM_F_DUMP, .expectedRc = SWL_RC_DONE, .expectedData = {.message = "Success", }},
    };

    //send async requests
    for(uint32_t i = 0; i < SWL_ARRAY_SIZE(tests); i++) {
        tests[i].timestart = swl_time_getMonoSec();
        rc = wld_nl80211_sendCmd(false, tests[i].state, tests[i].cmd, tests[i].flags, tests[i].ifIndex, NULL, s_cmdReplyCb, &tests[i].data, &tests[i].data.rc);
        assert_string_equal(tests[i].data.message, "");
        assert_int_equal(rc, SWL_RC_OK);
    }

    //run local event loop, until all requests are processed
    wld_nl80211_stateCounters_t counters = {};
    do {
        s_runEventLoopIter();
        wld_nl80211_getAllCounters(&counters);
    } while(counters.reqPending > 0);

    //check test results
    for(uint32_t i = 0; i < SWL_ARRAY_SIZE(tests); i++) {
        assert_int_equal(tests[i].data.rc, tests[i].expectedRc);
        assert_string_equal(tests[i].data.message, tests[i].expectedData.message);
        assert_int_equal(tests[i].data.error, tests[i].expectedData.error);
        uint32_t diff = tests[i].data.timestamp - tests[i].timestart;
        assert_in_range(diff, tests[i].expectedTime, tests[i].expectedTime + 1);
    }

    //restore native nl_send
    state->fNlSendPriv = NULL;
    //delete nl80211 socket manager
    wld_nl80211_cleanup(state);
}

typedef struct {
    wld_nl80211_state_t* state;
    int pipeFds[2]; //pipe used to simulate (mirroring) events
} stateMock_t;

typedef struct {
    uint32_t nCalls;        //count of invoked listeners
    uint32_t calledIds[10]; //array of invoked listeners
} listenerData_t;

typedef struct {
    uint32_t id;
    uint32_t stateId;
    bool isGlobal;
    uint32_t wiphy;
    uint32_t ifIndex;
    wld_nl80211_evtHandlers_cb* handlers;
    wld_nl80211_listener_t* listener;
} listenerDesc_t;

typedef struct {
    uint32_t wiphy;
    uint32_t ifIndex;
    listenerData_t expected;
} listenerTest_t;

void s_unspecEvtCb(void* pRef, void* pData, uint32_t wiphy _UNUSED, uint32_t ifIndex _UNUSED) {
    listenerDesc_t* pListenerDesc = (listenerDesc_t*) pRef;
    listenerData_t* pListenerData = (listenerData_t*) pData;
    if(!pListenerDesc || !pListenerData) {
        return;
    }
    pListenerData->calledIds[pListenerData->nCalls] = pListenerDesc->id;
    pListenerData->nCalls++;
}

static void s_sendEvt(stateMock_t* state, int cmd, uint32_t wiphy, uint32_t ifIndex) {
    int fd = state->pipeFds[1];
    int id = g_nl80211DriverIDs.family_id;
    struct nl_msg* msg = nlmsg_alloc();
    genlmsg_put(msg, 0, 0, id, 0, 0, cmd, 0);
    if(0 < (int) ifIndex) {
        nla_put_u32(msg, NL80211_ATTR_IFINDEX, ifIndex);
    }
    if(0 <= (int) wiphy) {
        nla_put_u32(msg, NL80211_ATTR_WIPHY, wiphy);
    }
    write(fd, (void*) nlmsg_hdr(msg), nlmsg_hdr(msg)->nlmsg_len);
    nlmsg_free(msg);
}

static void s_mcastEvt(uint32_t nMocks, stateMock_t* mocks, int cmd, uint32_t wiphy, uint32_t ifIndex) {
    for(uint32_t i = 0; i < nMocks; i++) {
        s_sendEvt(&mocks[i], cmd, wiphy, ifIndex);
    }
}

ssize_t s_recv(int socket, void* buffer, size_t length, int flags _UNUSED) {
    return read(socket, buffer, length);
}

static bool s_stateMockDeInit(stateMock_t* pStateMock) {
    if((pStateMock == NULL) || (pStateMock->state == NULL)) {
        return false;
    }
    wld_nl80211_cleanup(pStateMock->state);
    pStateMock->state = NULL;
    close(pStateMock->pipeFds[0]);
    close(pStateMock->pipeFds[1]);
    return true;
}

static bool s_stateMockInit(stateMock_t* pStateMock) {
    if((pStateMock == NULL) ||
       ((pStateMock->state = wld_nl80211_newState()) == NULL) ||
       (pipe(pStateMock->pipeFds) == -1)) {
        return false;
    }
    amxo_connection_t* conn = amxo_connection_get(get_wld_plugin_parser(), pStateMock->state->nl_event);
    if(conn == NULL) {
        s_stateMockDeInit(pStateMock);
        return false;
    }
    //REDIRECTION to nl80211 mocker
    //register pipe readFd as input
    pStateMock->state->nl_event = pStateMock->pipeFds[0];
    conn->fd = pStateMock->pipeFds[0];
    fcntl(pStateMock->pipeFds[0], F_SETFL, O_NONBLOCK);
    //overwrite recv api to avoid default nl msg checking/parsing
    pStateMock->state->fRecvPriv = s_recv;
    return true;
}

static void test_wld_nl80211_callEvtListeners(void** mockaState _UNUSED) {
    //to simulate mcast, create pool of state mockers
    stateMock_t stateMocks[2] = {};
    uint32_t nStates = SWL_ARRAY_SIZE(stateMocks);
    for(uint32_t i = 0; i < nStates; i++) {
        assert_true(s_stateMockInit(&stateMocks[i]));
    }

    //only register unspec handler
    wld_nl80211_evtHandlers_cb handlers = {
        .fUnspecEvtCb = s_unspecEvtCb,
    };

    //empty handlers
    wld_nl80211_evtHandlers_cb noHandlers = {};

    listenerDesc_t listeners[] = {
        //listeners over state 1 socket
        //listener for wiphy0 events
        {.id = 0, .stateId = 1, .wiphy = 0, .ifIndex = WLD_NL80211_ID_ANY, .handlers = &handlers, },
        //fake listener for ifIndex 16: no handlers
        {.id = 1, .stateId = 1, .wiphy = WLD_NL80211_ID_UNDEF, .ifIndex = 16, .handlers = &noHandlers, },
        //listener for ifIndex 17
        {.id = 2, .stateId = 1, .wiphy = 0, .ifIndex = 17, .handlers = &handlers, },
        //global listener (any wiphy, any ifIndex)
        {.id = 3, .stateId = 1, .isGlobal = true, .handlers = &handlers, },

        //listeners over state 0 socket
        //listener for ifIndex 16 on state 0
        {.id = 4, .stateId = 0, .wiphy = WLD_NL80211_ID_UNDEF, .ifIndex = 16, .handlers = &handlers, },
        //listener for wiphy1 events on state 0
        {.id = 5, .stateId = 0, .wiphy = 1, .ifIndex = WLD_NL80211_ID_ANY, .handlers = &handlers, },
    };

    listenerTest_t tests[] = {
        //mcast w:1 event => (s:0,w:1),(s:1:global) invocation order
        {.wiphy = 1, .ifIndex = WLD_NL80211_ID_UNDEF, .expected = {.nCalls = 2, .calledIds = {5, 3, }, }, },
        //mcast w:0,i:16 event => (s:0,i:16),(s:1,w:0),(s:1:global) invocation order
        {.wiphy = 0, .ifIndex = 16, .expected = {.nCalls = 3, .calledIds = {4, 0, 3, }, }},
        //mcast w:0,i:17 event => (s:1,i:17),(s:1,w:0),(s:1:global) invocation order
        {.wiphy = 0, .ifIndex = 17, .expected = {.nCalls = 3, .calledIds = {2, 0, 3, }, }},
        //mcast i:17 event => (s:1,i:17),(s:1:global) invocation order
        {.wiphy = WLD_NL80211_ID_UNDEF, .ifIndex = 17, .expected = {.nCalls = 2, .calledIds = {2, 3, }, }},
        //mcast i:10 event (unknown wiphy,ifIndex) => (s:1:global) invocation order
        {.wiphy = WLD_NL80211_ID_UNDEF, .ifIndex = 10, .expected = {.nCalls = 1, .calledIds = {3, }, }},
        //mcast w:1,i:18 event (unknown ifIndex) => (s:0,w:1),(s:1:global) invocation order
        {.wiphy = 1, .ifIndex = 18, .expected = {.nCalls = 2, .calledIds = {5, 3, }, }},
    };

    listenerData_t data = {};
    for(uint32_t i = 0; i < SWL_ARRAY_SIZE(listeners); i++) {
        if(listeners[i].isGlobal) {
            listeners[i].listener = wld_nl80211_addGlobalEvtListener(stateMocks[listeners[i].stateId].state,
                                                                     &listeners[i], &data, listeners[i].handlers);
            assert_non_null(listeners[i].listener);
        } else {
            listeners[i].listener = wld_nl80211_addEvtListener(stateMocks[listeners[i].stateId].state,
                                                               listeners[i].wiphy, listeners[i].ifIndex,
                                                               &listeners[i], &data, listeners[i].handlers);
            assert_non_null(listeners[i].listener);
        }
    }

    for(uint32_t i = 0; i < SWL_ARRAY_SIZE(tests); i++) {
        memset(&data, 0, sizeof(data));
        s_mcastEvt(nStates, stateMocks, NL80211_CMD_UNSPEC, tests[i].wiphy, tests[i].ifIndex);
        s_runEventLoopIter();
        assert_int_equal(data.nCalls, tests[i].expected.nCalls);
        assert_memory_equal(data.calledIds, tests[i].expected.calledIds, data.nCalls * sizeof(data.calledIds[0]));
    }

    //cleanup nl state mocker
    for(uint32_t i = 0; i < nStates; i++) {
        s_stateMockDeInit(&stateMocks[i]);
    }
}

static struct nl_msg* s_mirrorNlMsgExt(struct nl_msg* msg, int msgType, uint32_t cmd, uint32_t flags) {
    struct nlmsghdr* nlh = nlmsg_hdr(msg);
    uint32_t seqId = nlh->nlmsg_seq;
    uint32_t portId = nlh->nlmsg_pid;
    struct nl_msg* msgReply = nlmsg_alloc();
    genlmsg_put(msgReply, portId, seqId, msgType, 0, flags, cmd, 0);
    return msgReply;
}

static struct nl_msg* s_mirrorNlMsg(struct nl_msg* msg, uint32_t cmd, uint32_t flags) {
    struct nlmsghdr* nlh = nlmsg_hdr(msg);
    return s_mirrorNlMsgExt(msg, nlh->nlmsg_type, cmd, flags);
}

typedef struct {
    stateMock_t stateMock;
    void* expectedData;
} cmdMockTest_t;

typedef struct {
    stateMock_t stateMock;
    void* expectedData;
    size_t nExpectedElts;
} cmdMockTestMultiElt_t;

cmdMockTest_t mockGetIfaceInfo;
static int s_nlSend_ifaceInfo(struct nl_sock* sock _UNUSED, struct nl_msg* msg _UNUSED) {
    int fd = mockGetIfaceInfo.stateMock.pipeFds[1];
    wld_nl80211_ifaceInfo_t* pIfaceInfo = (wld_nl80211_ifaceInfo_t*) mockGetIfaceInfo.expectedData;
    uint64_t wdevId = ((uint64_t) pIfaceInfo->wiphy << 32);
    if(pIfaceInfo->isMain) {
        wdevId |= 0x1;
    }
    uint32_t ifType = NL80211_IFTYPE_UNSPECIFIED;
    if(pIfaceInfo->isAp) {
        ifType = NL80211_IFTYPE_AP;
    } else if(pIfaceInfo->isSta) {
        ifType = NL80211_IFTYPE_STATION;
    }
    uint8_t use4Mac = pIfaceInfo->use4Mac;

    struct nl_msg* msgReply = s_mirrorNlMsg(msg, NL80211_CMD_NEW_INTERFACE, 0);
    NL_ATTRS(attribs,
             ARR(NL_ATTR_VAL(NL80211_ATTR_IFINDEX, pIfaceInfo->ifIndex),
                 NL_ATTR_VAL(NL80211_ATTR_WIPHY, pIfaceInfo->wiphy),
                 NL_ATTR_VAL(NL80211_ATTR_WDEV, wdevId),
                 NL_ATTR_DATA(NL80211_ATTR_IFNAME, strlen(pIfaceInfo->name) + 1, pIfaceInfo->name),
                 NL_ATTR_VAL(NL80211_ATTR_IFTYPE, ifType),
                 NL_ATTR_VAL(NL80211_ATTR_4ADDR, use4Mac),
                 NL_ATTR_DATA(NL80211_ATTR_MAC, SWL_MAC_BIN_LEN, pIfaceInfo->mac.bMac),
                 ));
    if(pIfaceInfo->txPower > 0) {
        uint32_t txPowMbm = pIfaceInfo->txPower * 100;
        NL_ATTRS_ADD(&attribs, NL_ATTR_VAL(NL80211_ATTR_WIPHY_TX_POWER_LEVEL, txPowMbm));
    }
    if(pIfaceInfo->ssid[0]) {
        NL_ATTRS_ADD(&attribs, NL_ATTR_DATA(NL80211_ATTR_SSID, strlen(pIfaceInfo->ssid), pIfaceInfo->ssid));
    }
    if(pIfaceInfo->chanSpec.ctrlFreq > 0) {
        NL_ATTRS_ADD(&attribs, NL_ATTR_VAL(NL80211_ATTR_WIPHY_FREQ, pIfaceInfo->chanSpec.ctrlFreq));
        int32_t nlBw = wld_nl80211_bwValToNl(pIfaceInfo->chanSpec.chanWidth);
        if(nlBw >= 0) {
            NL_ATTRS_ADD(&attribs, NL_ATTR_VAL(NL80211_ATTR_CHANNEL_WIDTH, nlBw));
        }
        if(pIfaceInfo->chanSpec.centerFreq1 > 0) {
            NL_ATTRS_ADD(&attribs, NL_ATTR_VAL(NL80211_ATTR_CENTER_FREQ1, pIfaceInfo->chanSpec.centerFreq1));
        }
    }
    wld_nl80211_addNlAttrs(msgReply, &attribs);
    write(fd, (void*) nlmsg_hdr(msgReply), nlmsg_hdr(msgReply)->nlmsg_len);
    nlmsg_free(msgReply);
    NL_ATTRS_CLEAR(&attribs);
    return 0;
}

static void test_wld_nl80211_getIfaceInfo(void** mockaState _UNUSED) {
    assert_true(s_stateMockInit(&mockGetIfaceInfo.stateMock));
    //tweak: override nl_send API to catch request and build reply locally
    mockGetIfaceInfo.stateMock.state->fNlSendPriv = s_nlSend_ifaceInfo;
    wld_nl80211_ifaceInfo_t expectedIfaceInfo = {
        .ifIndex = 14,
        .wiphy = 1,
        .name = "wl1",
        .mac = {.bMac = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6}},
        .isMain = true,
        .isAp = true,
        .isSta = false,
        .txPower = 20, // dbm
        .ssid = "ssid_wl1",
        .chanSpec = {
            .ctrlFreq = 5500,
            .chanWidth = 80,
            .centerFreq1 = 5530,
        },
    };
    mockGetIfaceInfo.expectedData = &expectedIfaceInfo;
    wld_nl80211_ifaceInfo_t retIfaceInfo = {};
    swl_rc_ne rc = wld_nl80211_getInterfaceInfo(mockGetIfaceInfo.stateMock.state, expectedIfaceInfo.ifIndex, &retIfaceInfo);
    assert_true(rc >= SWL_RC_OK);
    wld_nl80211_dumpIfaceInfo(&retIfaceInfo, NULL);
    assert_int_equal(retIfaceInfo.ifIndex, expectedIfaceInfo.ifIndex);
    assert_int_equal(retIfaceInfo.wiphy, expectedIfaceInfo.wiphy);
    assert_string_equal(retIfaceInfo.name, expectedIfaceInfo.name);
    assert_int_equal(retIfaceInfo.isMain, expectedIfaceInfo.isMain);
    assert_int_equal(retIfaceInfo.isAp, expectedIfaceInfo.isAp);
    assert_int_equal(retIfaceInfo.isSta, expectedIfaceInfo.isSta);
    assert_memory_equal(retIfaceInfo.mac.bMac, expectedIfaceInfo.mac.bMac, SWL_MAC_BIN_LEN);
    assert_string_equal(retIfaceInfo.ssid, expectedIfaceInfo.ssid);
    assert_int_equal(retIfaceInfo.txPower, expectedIfaceInfo.txPower);
    assert_int_equal(retIfaceInfo.chanSpec.ctrlFreq, expectedIfaceInfo.chanSpec.ctrlFreq);
    assert_int_equal(retIfaceInfo.chanSpec.chanWidth, expectedIfaceInfo.chanSpec.chanWidth);
    assert_int_equal(retIfaceInfo.chanSpec.centerFreq1, expectedIfaceInfo.chanSpec.centerFreq1);
    s_stateMockDeInit(&mockGetIfaceInfo.stateMock);
}

uint32_t s_getMask(uint32_t val) {
    return (1 << val) - 1;
}
cmdMockTestMultiElt_t mockGetWiphyInfo;
extern swl_table_t sChanDfsStatusMap;
static int s_nlSend_wiphyInfoChunk(int fd, wld_nl80211_wiphyInfo_t* pWiphyInfo, struct nl_msg* msg, bool hasMulti) {
    uint8_t maxScanSsids = pWiphyInfo->maxScanSsids;
    uint32_t antMask[COM_DIR_MAX] = ARR(s_getMask(pWiphyInfo->nrAntenna[COM_DIR_TRANSMIT]), s_getMask(pWiphyInfo->nrAntenna[COM_DIR_RECEIVE]));
    uint32_t actAntMask[COM_DIR_MAX] = ARR(s_getMask(pWiphyInfo->nrActiveAntenna[COM_DIR_TRANSMIT]), s_getMask(pWiphyInfo->nrActiveAntenna[COM_DIR_RECEIVE]));
    uint32_t genId = 2;
    struct nl_msg* msgReply = s_mirrorNlMsg(msg, NL80211_CMD_NEW_WIPHY, hasMulti ? NLM_F_MULTI : 0);
    NL_ATTRS(attribs,
             ARR(NL_ATTR_VAL(NL80211_ATTR_GENERATION, genId),
                 NL_ATTR_VAL(NL80211_ATTR_WIPHY, pWiphyInfo->wiphy),
                 NL_ATTR_DATA(NL80211_ATTR_WIPHY_NAME, strlen(pWiphyInfo->name) + 1, pWiphyInfo->name),
                 NL_ATTR_VAL(NL80211_ATTR_MAX_NUM_SCAN_SSIDS, maxScanSsids),
                 NL_ATTR_VAL(NL80211_ATTR_MAX_SCAN_IE_LEN, pWiphyInfo->maxScanIeLen),
                 NL_ATTR_DATA(NL80211_ATTR_CIPHER_SUITES, (pWiphyInfo->nCipherSuites * sizeof(pWiphyInfo->cipherSuites[0])), pWiphyInfo->cipherSuites),
                 NL_ATTR_VAL(NL80211_ATTR_WIPHY_ANTENNA_AVAIL_TX, antMask[COM_DIR_TRANSMIT]),
                 NL_ATTR_VAL(NL80211_ATTR_WIPHY_ANTENNA_AVAIL_RX, antMask[COM_DIR_RECEIVE]),
                 NL_ATTR_VAL(NL80211_ATTR_WIPHY_ANTENNA_TX, actAntMask[COM_DIR_TRANSMIT]),
                 NL_ATTR_VAL(NL80211_ATTR_WIPHY_ANTENNA_RX, actAntMask[COM_DIR_RECEIVE])));

    NL_ATTR_NESTED(ifTypeAttr, NL80211_ATTR_SUPPORTED_IFTYPES);
    if(pWiphyInfo->suppEp) {
        NL_ATTRS_ADD(&ifTypeAttr.data.attribs, NL_ATTR(NL80211_IFTYPE_STATION));
    }
    if(pWiphyInfo->suppAp) {
        NL_ATTRS_ADD(&ifTypeAttr.data.attribs, NL_ATTR(NL80211_IFTYPE_AP));
    }
    swl_unLiList_add(&attribs, &ifTypeAttr);

    NL_ATTR_NESTED(bandsAttr, NL80211_ATTR_WIPHY_BANDS);
    uint32_t nlBand[SWL_FREQ_BAND_MAX] = {NL80211_BAND_2GHZ, NL80211_BAND_5GHZ, NL80211_BAND_6GHZ};
    for(uint32_t b = 0; b < SWL_FREQ_BAND_MAX; b++) {
        wld_nl80211_bandDef_t* pBand = &pWiphyInfo->bands[b];
        if(!pBand->radStdsMask) {
            continue;
        }
        NL_ATTR_NESTED(bandAttr, nlBand[pBand->freqBand]);
        if(SWL_BIT_IS_SET(pBand->radStdsMask, SWL_RADSTD_N)) {
            uint16_t htCapa = 0x19ef;
            NL_ATTRS_ADD(&bandAttr.data.attribs, NL_ATTR_VAL(NL80211_BAND_ATTR_HT_CAPA, htCapa));
            uint8_t htMcs[] = {0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, 0};
            NL_ATTRS_ADD(&bandAttr.data.attribs, NL_ATTR_DATA(NL80211_BAND_ATTR_HT_MCS_SET, sizeof(htMcs), htMcs));
        }
        if(SWL_BIT_IS_SET(pBand->radStdsMask, SWL_RADSTD_AC)) {
            uint32_t vhtCapa = 0x039071f6;
            NL_ATTRS_ADD(&bandAttr.data.attribs, NL_ATTR_VAL(NL80211_BAND_ATTR_VHT_CAPA, vhtCapa));
            uint8_t vhtMcs[] = {0xfa, 0xff, 0, 0, 0xfa, 0xff, 0, 0};
            NL_ATTRS_ADD(&bandAttr.data.attribs, NL_ATTR_DATA(NL80211_BAND_ATTR_VHT_MCS_SET, sizeof(vhtMcs), vhtMcs));
        }
        if(SWL_BIT_IS_SET(pBand->radStdsMask, SWL_RADSTD_AX)) {
            NL_ATTR_NESTED(bandIfTypeDataAttr, NL80211_BAND_ATTR_IFTYPE_DATA);
            NL_ATTR_NESTED(bandIfTypeDataApAttr, 1);
            uint8_t heCapMac[] = {0x0d, 0x00, 0x08, 0x12, 0x00, 0x10, };
            NL_ATTRS_ADD(&bandIfTypeDataApAttr.data.attribs, NL_ATTR_DATA(NL80211_BAND_IFTYPE_ATTR_HE_CAP_MAC, sizeof(heCapMac), heCapMac));
            uint8_t heCapPhy[] = {0x0e, 0x3f, 0x02, 0x00, 0xfd, 0x09, 0x80, 0x0e, 0xcf, 0xf2, 0x00, };
            NL_ATTRS_ADD(&bandIfTypeDataApAttr.data.attribs, NL_ATTR_DATA(NL80211_BAND_IFTYPE_ATTR_HE_CAP_PHY, sizeof(heCapPhy), heCapPhy));
            uint8_t heMcsMapLe80[] = {0xfa, 0xff, 0xfa, 0xff};
            NL_ATTRS_ADD(&bandIfTypeDataApAttr.data.attribs, NL_ATTR_DATA(NL80211_BAND_IFTYPE_ATTR_HE_CAP_MCS_SET, sizeof(heMcsMapLe80), heMcsMapLe80));
            swl_unLiList_add(&bandIfTypeDataAttr.data.attribs, &bandIfTypeDataApAttr);
            swl_unLiList_add(&bandAttr.data.attribs, &bandIfTypeDataAttr);
        }
        NL_ATTR_NESTED(bandFreqAttr, NL80211_BAND_ATTR_FREQS);
        uint32_t maxTxPower = pBand->chans[0].maxTxPwr * 100;
        for(uint32_t i = 0; i < pBand->nChans; i++) {
            NL_ATTR_NESTED(bandChanAttr, i + 1);
            NL_ATTRS_ADD(&bandChanAttr.data.attribs, NL_ATTR_VAL(NL80211_FREQUENCY_ATTR_FREQ, pBand->chans[i].ctrlFreq));
            NL_ATTRS_ADD(&bandChanAttr.data.attribs, NL_ATTR_VAL(NL80211_FREQUENCY_ATTR_MAX_TX_POWER, maxTxPower));
            if(pBand->chans[i].isDfs) {
                NL_ATTRS_ADD(&bandChanAttr.data.attribs, NL_ATTR(NL80211_FREQUENCY_ATTR_RADAR));
                NL_ATTRS_ADD(&bandChanAttr.data.attribs, NL_ATTR_VAL(NL80211_FREQUENCY_ATTR_DFS_TIME, pBand->chans[i].dfsTime));
                NL_ATTRS_ADD(&bandChanAttr.data.attribs, NL_ATTR_VAL(NL80211_FREQUENCY_ATTR_DFS_CAC_TIME, pBand->chans[i].dfsCacTime));
                uint32_t* pStateVal = (uint32_t*) swl_table_getMatchingValue(&sChanDfsStatusMap, 0, 1, &pBand->chans[i].status);
                if(pStateVal) {
                    NL_ATTRS_ADD(&bandChanAttr.data.attribs, NL_ATTR_DATA(NL80211_FREQUENCY_ATTR_DFS_STATE, sizeof(*pStateVal), pStateVal));
                }
            }
            swl_unLiList_add(&bandFreqAttr.data.attribs, &bandChanAttr);
        }
        swl_unLiList_add(&bandAttr.data.attribs, &bandFreqAttr);
        swl_unLiList_add(&bandsAttr.data.attribs, &bandAttr);
    }
    swl_unLiList_add(&attribs, &bandsAttr);

    wld_nl80211_addNlAttrs(msgReply, &attribs);
    write(fd, (void*) nlmsg_hdr(msgReply), nlmsg_hdr(msgReply)->nlmsg_len);
    nlmsg_free(msgReply);
    NL_ATTRS_CLEAR(&attribs);
    return 0;
}

static int s_nlSend_wiphyInfo(struct nl_sock* sock _UNUSED, struct nl_msg* msg _UNUSED) {
    int fd = mockGetWiphyInfo.stateMock.pipeFds[1];
    wld_nl80211_wiphyInfo_t* pAllWiphyInfo = (wld_nl80211_wiphyInfo_t*) mockGetWiphyInfo.expectedData;
    bool hasMulti = (mockGetWiphyInfo.nExpectedElts > 1);
    for(int32_t i = mockGetWiphyInfo.nExpectedElts - 1; i >= 0; i--) {
        wld_nl80211_wiphyInfo_t* pWiphyInfo = &pAllWiphyInfo[i];
        s_nlSend_wiphyInfoChunk(fd, pWiphyInfo, msg, hasMulti);
    }

    if(hasMulti) {
        struct nl_msg* msgReply = s_mirrorNlMsgExt(msg, NLMSG_DONE, 0, NLM_F_MULTI);
        write(fd, (void*) nlmsg_hdr(msgReply), nlmsg_hdr(msgReply)->nlmsg_len);
        nlmsg_free(msgReply);
    }
    return 0;
}

static void s_checkWiphyInfo(wld_nl80211_wiphyInfo_t* pRetWiphyInfo, const wld_nl80211_wiphyInfo_t* pExpectedWiphyInfo) {
    wld_nl80211_dumpWiphyInfo(pRetWiphyInfo, NULL);
    assert_int_equal(pRetWiphyInfo->wiphy, pExpectedWiphyInfo->wiphy);
    assert_string_equal(pRetWiphyInfo->name, pExpectedWiphyInfo->name);
    assert_int_equal(pRetWiphyInfo->maxScanSsids, pExpectedWiphyInfo->maxScanSsids);
    assert_int_equal(pRetWiphyInfo->maxScanIeLen, pExpectedWiphyInfo->maxScanIeLen);
    assert_int_equal(pRetWiphyInfo->nrAntenna[COM_DIR_TRANSMIT], pExpectedWiphyInfo->nrAntenna[COM_DIR_TRANSMIT]);
    assert_int_equal(pRetWiphyInfo->nrAntenna[COM_DIR_RECEIVE], pExpectedWiphyInfo->nrAntenna[COM_DIR_RECEIVE]);
    assert_int_equal(pRetWiphyInfo->nrActiveAntenna[COM_DIR_TRANSMIT], pExpectedWiphyInfo->nrActiveAntenna[COM_DIR_TRANSMIT]);
    assert_int_equal(pRetWiphyInfo->nrActiveAntenna[COM_DIR_RECEIVE], pExpectedWiphyInfo->nrActiveAntenna[COM_DIR_RECEIVE]);
    assert_int_equal(pRetWiphyInfo->suppAp, pExpectedWiphyInfo->suppAp);
    assert_int_equal(pRetWiphyInfo->suppEp, pExpectedWiphyInfo->suppEp);
    assert_int_equal(pRetWiphyInfo->nCipherSuites, pExpectedWiphyInfo->nCipherSuites);
    assert_memory_equal(pRetWiphyInfo->cipherSuites, pExpectedWiphyInfo->cipherSuites, pExpectedWiphyInfo->nCipherSuites * sizeof(pExpectedWiphyInfo->cipherSuites[0]));
    assert_int_equal(pRetWiphyInfo->nSSMax, pExpectedWiphyInfo->nSSMax);
    assert_int_equal(pRetWiphyInfo->freqBandsMask, pExpectedWiphyInfo->freqBandsMask);
    for(uint32_t i = 0; i < SWL_FREQ_BAND_MAX; i++) {
        assert_int_equal(pRetWiphyInfo->bands[i].freqBand, pExpectedWiphyInfo->bands[i].freqBand);
        assert_int_equal(pRetWiphyInfo->bands[i].radStdsMask, pExpectedWiphyInfo->bands[i].radStdsMask);
        assert_int_equal(pRetWiphyInfo->bands[i].chanWidthMask, pExpectedWiphyInfo->bands[i].chanWidthMask);
        assert_int_equal(pRetWiphyInfo->bands[i].nSSMax, pExpectedWiphyInfo->bands[i].nSSMax);
        assert_int_equal(pRetWiphyInfo->bands[i].nChans, pExpectedWiphyInfo->bands[i].nChans);
        assert_memory_equal(pRetWiphyInfo->bands[i].chans, pExpectedWiphyInfo->bands[i].chans, pExpectedWiphyInfo->bands[i].nChans * sizeof(pExpectedWiphyInfo->bands[i].chans[0]));
        assert_int_equal(pRetWiphyInfo->bands[i].bfCapsSupported[COM_DIR_TRANSMIT], pExpectedWiphyInfo->bands[i].bfCapsSupported[COM_DIR_TRANSMIT]);
        assert_int_equal(pRetWiphyInfo->bands[i].bfCapsSupported[COM_DIR_RECEIVE], pExpectedWiphyInfo->bands[i].bfCapsSupported[COM_DIR_RECEIVE]);
        for(uint32_t j = 0; j < SWL_MCS_STANDARD_MAX; j++) {
            assert_int_equal(pRetWiphyInfo->bands[i].mcsStds[j].standard, pExpectedWiphyInfo->bands[i].mcsStds[j].standard);
            assert_int_equal(pRetWiphyInfo->bands[i].mcsStds[j].mcsIndex, pExpectedWiphyInfo->bands[i].mcsStds[j].mcsIndex);
            assert_int_equal(pRetWiphyInfo->bands[i].mcsStds[j].bandwidth, pExpectedWiphyInfo->bands[i].mcsStds[j].bandwidth);
            assert_int_equal(pRetWiphyInfo->bands[i].mcsStds[j].numberOfSpatialStream, pExpectedWiphyInfo->bands[i].mcsStds[j].numberOfSpatialStream);
            assert_int_equal(pRetWiphyInfo->bands[i].mcsStds[j].guardInterval, pExpectedWiphyInfo->bands[i].mcsStds[j].guardInterval);
        }
    }
}

static wld_nl80211_wiphyInfo_t sTestWiphyInfo[] = {
    {
        .wiphy = 0,
        .name = "phy0",
        .maxScanSsids = 16,
        .maxScanIeLen = 2048,
        .nrAntenna = {2, 2},
        .nrActiveAntenna = {2, 2},
        .nSSMax = 2,
        .suppUAPSD = true,
        .suppAp = true,
        .suppEp = true,
        .nCipherSuites = 4,
        .cipherSuites = ARR(0x000fac01, 0x000fac05, 0x000fac02, 0x000fac04),
        .freqBandsMask = M_SWL_FREQ_BAND_2_4GHZ,
        .bands = ARR({.freqBand = SWL_FREQ_BAND_2_4GHZ,
                         .radStdsMask = M_SWL_RADSTD_B | M_SWL_RADSTD_G | M_SWL_RADSTD_N,
                         .chanWidthMask = M_SWL_BW_20MHZ | M_SWL_BW_40MHZ,
                         .nSSMax = 2,
                         .nChans = 11,
                         .chans = ARR({.ctrlFreq = 2412, .status = WLD_NL80211_CHAN_AVAILABLE, .maxTxPwr = 20},
                                      {.ctrlFreq = 2417, .status = WLD_NL80211_CHAN_AVAILABLE, .maxTxPwr = 20},
                                      {.ctrlFreq = 2422, .status = WLD_NL80211_CHAN_AVAILABLE, .maxTxPwr = 20},
                                      {.ctrlFreq = 2427, .status = WLD_NL80211_CHAN_AVAILABLE, .maxTxPwr = 20},
                                      {.ctrlFreq = 2432, .status = WLD_NL80211_CHAN_AVAILABLE, .maxTxPwr = 20},
                                      {.ctrlFreq = 2437, .status = WLD_NL80211_CHAN_AVAILABLE, .maxTxPwr = 20},
                                      {.ctrlFreq = 2442, .status = WLD_NL80211_CHAN_AVAILABLE, .maxTxPwr = 20},
                                      {.ctrlFreq = 2447, .status = WLD_NL80211_CHAN_AVAILABLE, .maxTxPwr = 20},
                                      {.ctrlFreq = 2452, .status = WLD_NL80211_CHAN_AVAILABLE, .maxTxPwr = 20},
                                      {.ctrlFreq = 2457, .status = WLD_NL80211_CHAN_AVAILABLE, .maxTxPwr = 20},
                                      {.ctrlFreq = 2462, .status = WLD_NL80211_CHAN_AVAILABLE, .maxTxPwr = 20}, ),
                         .bfCapsSupported = ARR(0, 0),
                         .mcsStds = ARR({}, {},
                                        {.standard = SWL_MCS_STANDARD_HT,
                                            .mcsIndex = 15,
                                            .bandwidth = SWL_BW_40MHZ,
                                            .numberOfSpatialStream = 2,
                                            .guardInterval = SWL_SGI_400, }), }, ),
    },
    {
        .wiphy = 1,
        .name = "phy1",
        .maxScanSsids = 10,
        .maxScanIeLen = 2048,
        .nrAntenna = {2, 2},
        .nrActiveAntenna = {2, 2},
        .nSSMax = 2,
        .suppUAPSD = true,
        .suppAp = true,
        .suppEp = true,
        .nCipherSuites = 4,
        .cipherSuites = ARR(0x000fac01, 0x000fac05, 0x000fac02, 0x000fac04),
        .freqBandsMask = M_SWL_FREQ_BAND_5GHZ,
        .bands = ARR({},
                     {.freqBand = SWL_FREQ_BAND_5GHZ,
                         .radStdsMask = M_SWL_RADSTD_A | M_SWL_RADSTD_N | M_SWL_RADSTD_AC | M_SWL_RADSTD_AX,
                         .chanWidthMask = M_SWL_BW_20MHZ | M_SWL_BW_40MHZ | M_SWL_BW_80MHZ | M_SWL_BW_160MHZ,
                         .nSSMax = 2,
                         .nChans = 3,
                         .chans = ARR({.ctrlFreq = 5180, .isDfs = false, .status = WLD_NL80211_CHAN_AVAILABLE, .maxTxPwr = 22},
                                      {.ctrlFreq = 5500, .isDfs = true, .status = WLD_NL80211_CHAN_UNAVAILABLE, .dfsCacTime = 60000, .dfsTime = 3600000, .maxTxPwr = 22},
                                      {.ctrlFreq = 5660, .isDfs = true, .status = WLD_NL80211_CHAN_USABLE, .dfsCacTime = 600000, .dfsTime = 62510, .maxTxPwr = 22}, ),
                         .bfCapsSupported = ARR(0, M_RAD_BF_CAP_VHT_SU | M_RAD_BF_CAP_VHT_MU | M_RAD_BF_CAP_HE_SU | M_RAD_BF_CAP_HE_MU),
                         .mcsStds = ARR({}, {},
                                        {.standard = SWL_MCS_STANDARD_HT,
                                            .mcsIndex = 15,
                                            .bandwidth = SWL_BW_40MHZ,
                                            .numberOfSpatialStream = 2,
                                            .guardInterval = SWL_SGI_400, },
                                        {.standard = SWL_MCS_STANDARD_VHT,
                                            .mcsIndex = 9,
                                            .bandwidth = SWL_BW_160MHZ,
                                            .numberOfSpatialStream = 2,
                                            .guardInterval = SWL_SGI_400, },
                                        {.standard = SWL_MCS_STANDARD_HE,
                                            .mcsIndex = 11,
                                            .bandwidth = SWL_BW_160MHZ,
                                            .numberOfSpatialStream = 2,
                                            .guardInterval = SWL_SGI_800, }), }),
    },
};
static void test_wld_nl80211_getWiphyInfo(void** mockaState _UNUSED) {
    assert_true(s_stateMockInit(&mockGetWiphyInfo.stateMock));
    //tweak: override nl_send API to catch request and build reply locally
    mockGetWiphyInfo.stateMock.state->fNlSendPriv = s_nlSend_wiphyInfo;
    mockGetWiphyInfo.expectedData = &sTestWiphyInfo[1];
    mockGetWiphyInfo.nExpectedElts = 1;

    wld_nl80211_wiphyInfo_t retWiphyInfo = {};
    swl_rc_ne rc = wld_nl80211_getWiphyInfo(mockGetWiphyInfo.stateMock.state, 1, &retWiphyInfo);
    assert_true(rc >= SWL_RC_OK);
    s_checkWiphyInfo(&retWiphyInfo, mockGetWiphyInfo.expectedData);
    s_stateMockDeInit(&mockGetWiphyInfo.stateMock);
}

static void test_wld_nl80211_getAllWiphyInfo(void** mockaState _UNUSED) {
    assert_true(s_stateMockInit(&mockGetWiphyInfo.stateMock));
    //tweak: override nl_send API to catch request and build reply locally
    mockGetWiphyInfo.stateMock.state->fNlSendPriv = s_nlSend_wiphyInfo;
    mockGetWiphyInfo.expectedData = sTestWiphyInfo;
    mockGetWiphyInfo.nExpectedElts = SWL_ARRAY_SIZE(sTestWiphyInfo);

    wld_nl80211_wiphyInfo_t retWiphyInfo[mockGetWiphyInfo.nExpectedElts];
    uint32_t nrWiphyInfo = 0;
    swl_rc_ne rc = wld_nl80211_getAllWiphyInfo(mockGetWiphyInfo.stateMock.state, mockGetWiphyInfo.nExpectedElts, retWiphyInfo, &nrWiphyInfo);
    assert_true(rc >= SWL_RC_OK);
    assert_int_equal(nrWiphyInfo, mockGetWiphyInfo.nExpectedElts);
    for(uint32_t i = 0; i < nrWiphyInfo; i++) {
        s_checkWiphyInfo(&retWiphyInfo[i], &sTestWiphyInfo[i]);
    }
    s_stateMockDeInit(&mockGetWiphyInfo.stateMock);
}

typedef struct {
    stateMock_t stateMock;
    void* expectedData;
    void* resultingData;
} cmdMockAsyncTest_t;

cmdMockAsyncTest_t mockGetScanResults;

static void s_scanResultsCb(void* priv, swl_rc_ne rc, wld_scanResults_t* results) {
    assert_false(rc < SWL_RC_OK);
    assert_non_null(results);
    cmdMockAsyncTest_t* mockCtx = (cmdMockAsyncTest_t*) priv;
    assert_non_null(mockCtx);
    wld_scanResults_t* pCopy = mockCtx->resultingData;
    wld_scan_cleanupScanResults(pCopy);
    pCopy = (wld_scanResults_t*) calloc(1, sizeof(wld_scanResults_t));
    amxc_llist_for_each(it, &results->ssids) {
        wld_scanResultSSID_t* pItem = amxc_container_of(it, wld_scanResultSSID_t, it);
        amxc_llist_it_take(&pItem->it);
        amxc_llist_append(&pCopy->ssids, &pItem->it);
    }
    mockCtx->resultingData = pCopy;
}

static int s_nlSend_getScanResults(struct nl_sock* sock _UNUSED, struct nl_msg* msg _UNUSED) {
    int fd = mockGetScanResults.stateMock.pipeFds[1];
    wld_scanResults_t* pResults = (wld_scanResults_t*) mockGetScanResults.expectedData;
    uint32_t genId = 2;
    uint32_t wiphy = 1;
    uint32_t ifIndex = 14;
    uint64_t wdevId = ((uint64_t) wiphy << 32) | 0x1;
    bool hasMulti = (amxc_llist_size(&pResults->ssids) > 1);

    amxc_llist_for_each(it, &pResults->ssids) {
        wld_scanResultSSID_t* bss = amxc_container_of(it, wld_scanResultSSID_t, it);
        struct nl_msg* msgReply = s_mirrorNlMsg(msg, NL80211_CMD_NEW_SCAN_RESULTS, hasMulti ? NLM_F_MULTI : 0);
        NL_ATTRS(attribs,
                 ARR(NL_ATTR_VAL(NL80211_ATTR_GENERATION, genId),
                     NL_ATTR_VAL(NL80211_ATTR_IFINDEX, ifIndex),
                     NL_ATTR_VAL(NL80211_ATTR_WIPHY, wiphy),
                     NL_ATTR_VAL(NL80211_ATTR_WDEV, wdevId)));
        NL_ATTR_NESTED(bssAttr, NL80211_ATTR_BSS);
        NL_ATTRS_ADD(&bssAttr.data.attribs, NL_ATTR_DATA(NL80211_BSS_BSSID, SWL_MAC_BIN_LEN, bss->bssid.bMac));
        uint32_t rssi = bss->rssi * 100;
        NL_ATTRS_ADD(&bssAttr.data.attribs, NL_ATTR_VAL(NL80211_BSS_SIGNAL_MBM, rssi));
        swl_chanspec_t chspec = SWL_CHANSPEC_EMPTY;
        chspec.channel = bss->channel;
        chspec.band = swl_chanspec_freqBandExtFromBaseChannel(bss->channel);
        chspec.bandwidth = swl_chanspec_intToBw(bss->bandwidth);
        uint32_t freq = 0;
        swl_chanspec_channelToMHz(&chspec, &freq);
        NL_ATTRS_ADD(&bssAttr.data.attribs, NL_ATTR_VAL(NL80211_BSS_FREQUENCY, freq));
        size_t len = 0;
        swl_bit8_t iesBuf[2048];
        /* fill IE SSID */
        iesBuf[len++] = SWL_80211_EL_ID_SSID;
        iesBuf[len++] = bss->ssidLen;
        memcpy(&iesBuf[len], bss->ssid, bss->ssidLen);
        len += bss->ssidLen;
        /* fill IE Supported Operating class */
        swl_freqBandExt_e freqBandExt = swl_chanspec_freqBandExtFromBaseChannel(bss->channel);
        swl_bandwidth_e bandwidth = swl_chanspec_intToBw(bss->bandwidth);
        iesBuf[len++] = SWL_80211_EL_ID_SUP_OP_CLASS;
        iesBuf[len++] = 2;
        uint8_t defOperClass[SWL_BW_MAX][SWL_FREQ_BAND_EXT_MAX] = {
            {0, 0, 0},
            {81, 115, 131},
            {83, 122, 132},
            {0, 128, 133},
            {0, 129, 134}
        };
        iesBuf[len++] = defOperClass[bandwidth][freqBandExt];
        iesBuf[len++] = 0; //no alternatives

        NL_ATTRS_ADD(&bssAttr.data.attribs, NL_ATTR_DATA(NL80211_BSS_INFORMATION_ELEMENTS, len, iesBuf));
        swl_unLiList_add(&attribs, &bssAttr);
        wld_nl80211_addNlAttrs(msgReply, &attribs);
        write(fd, (void*) nlmsg_hdr(msgReply), nlmsg_hdr(msgReply)->nlmsg_len);
        nlmsg_free(msgReply);
        NL_ATTRS_CLEAR(&attribs);
    }

    if(hasMulti) {
        struct nl_msg* msgReply = s_mirrorNlMsgExt(msg, NLMSG_DONE, 0, NLM_F_MULTI);
        write(fd, (void*) nlmsg_hdr(msgReply), nlmsg_hdr(msgReply)->nlmsg_len);
        nlmsg_free(msgReply);
    }
    return 0;
}

static int s_test_getScanResults_setup(void** state) {
    *state = &mockGetScanResults;
    return 0;
}
static int s_test_getScanResults_teardown(void** state) {
    cmdMockAsyncTest_t* pMockGetScanResults = *state;
    s_stateMockDeInit(&pMockGetScanResults->stateMock);
    wld_scanResults_t* results = (wld_scanResults_t*) pMockGetScanResults->resultingData;
    wld_scan_cleanupScanResults(results);
    free(results);
    return 0;
}

static void test_wld_nl80211_getScanResults(void** mockaState _UNUSED) {
    assert_true(s_stateMockInit(&mockGetScanResults.stateMock));
    //tweak: override nl_send API to catch request and build reply locally
    mockGetScanResults.stateMock.state->fNlSendPriv = s_nlSend_getScanResults;

    wld_scanResults_t expectedList;
    amxc_llist_init(&expectedList.ssids);
    wld_scanResultSSID_t aItems[] = {
        {
            .ssid = "Neigh_1_2G_20M",
            .ssidLen = swl_str_len("Neigh_1_2G_20M"),
            .bssid = {.bMac = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06}},
            .rssi = -60,
            .channel = 6,
            .bandwidth = 20,
        },
        {
            .ssid = "Neigh_2_2G_40M",
            .ssidLen = swl_str_len("Neigh_2_2G_40M"),
            .bssid = {.bMac = {0x11, 0x12, 0x13, 0x14, 0x15, 0x16}},
            .rssi = -47,
            .channel = 3,
            .bandwidth = 40,
        },
    };
    for(uint32_t i = 0; i < SWL_ARRAY_SIZE(aItems); i++) {
        wld_scanResultSSID_t* pResultItem = &aItems[i];
        amxc_llist_it_init(&pResultItem->it);
        amxc_llist_append(&expectedList.ssids, &pResultItem->it);
    }
    mockGetScanResults.expectedData = &expectedList;

    swl_rc_ne rc = wld_nl80211_getScanResults(mockGetScanResults.stateMock.state, 14, &mockGetScanResults, s_scanResultsCb);
    assert_true(rc >= SWL_RC_OK);

    //run local event loop, until all requests are processed
    wld_nl80211_stateCounters_t counters = {};
    do {
        s_runEventLoopIter();
        wld_nl80211_getAllCounters(&counters);
    } while(counters.reqPending > 0);

    wld_scanResults_t* resultingList = (wld_scanResults_t*) mockGetScanResults.resultingData;
    assert_non_null(resultingList);

    assert_int_equal(amxc_llist_size(&expectedList.ssids), amxc_llist_size(&resultingList->ssids));
    amxc_llist_for_each(it, &expectedList.ssids) {
        wld_scanResultSSID_t* pExpectedItem = amxc_container_of(it, wld_scanResultSSID_t, it);
        bool found = false;
        amxc_llist_for_each(it2, &resultingList->ssids) {
            wld_scanResultSSID_t* pResultItem = amxc_container_of(it2, wld_scanResultSSID_t, it);
            if(swl_mac_binMatches(&pResultItem->bssid, &pExpectedItem->bssid)) {
                found = true;
                assert_int_equal(pResultItem->channel, pExpectedItem->channel);
                assert_int_equal(pResultItem->bandwidth, pExpectedItem->bandwidth);
                assert_int_equal(pResultItem->ssidLen, pExpectedItem->ssidLen);
                assert_string_equal(pResultItem->ssid, pExpectedItem->ssid);
                assert_int_equal(pResultItem->rssi, pExpectedItem->rssi);
                wld_scan_cleanupScanResultSSID(pResultItem);
                break;
            }
        }
        assert_true(found);
    }
    assert_true(amxc_llist_is_empty(&resultingList->ssids));
}

cmdMockTestMultiElt_t mockGetChanSurvey;
static int s_nlSend_chanSurveyInfo(struct nl_sock* sock _UNUSED, struct nl_msg* msg _UNUSED) {
    int fd = mockGetChanSurvey.stateMock.pipeFds[1];
    wld_nl80211_channelSurveyInfo_t* pExpectedChansInfo = (wld_nl80211_channelSurveyInfo_t*) mockGetChanSurvey.expectedData;
    uint32_t wiphy = 1;
    uint32_t ifIndex = 14;
    bool hasMulti = (mockGetChanSurvey.nExpectedElts > 1);
    for(size_t i = 0; i < mockGetChanSurvey.nExpectedElts; i++) {
        wld_nl80211_channelSurveyInfo_t* pElt = &pExpectedChansInfo[i];
        struct nl_msg* msgReply = s_mirrorNlMsg(msg, NL80211_CMD_NEW_SURVEY_RESULTS, hasMulti ? NLM_F_MULTI : 0);
        NL_ATTRS(attribs,
                 ARR(NL_ATTR_VAL(NL80211_ATTR_IFINDEX, ifIndex),
                     NL_ATTR_VAL(NL80211_ATTR_WIPHY, wiphy)));

        NL_ATTR_NESTED(infoaAttr, NL80211_ATTR_SURVEY_INFO);
        NL_ATTRS_ADD(&infoaAttr.data.attribs, NL_ATTR_VAL(NL80211_SURVEY_INFO_FREQUENCY, pElt->frequencyMHz));
        uint8_t noise = pElt->noiseDbm;
        NL_ATTRS_ADD(&infoaAttr.data.attribs, NL_ATTR_VAL(NL80211_SURVEY_INFO_NOISE, noise));
        if(pElt->inUse) {
            NL_ATTRS_ADD(&infoaAttr.data.attribs, NL_ATTR(NL80211_SURVEY_INFO_IN_USE));
        }
        NL_ATTRS_ADD(&infoaAttr.data.attribs, NL_ATTR_VAL(NL80211_SURVEY_INFO_TIME, pElt->timeOn));
        NL_ATTRS_ADD(&infoaAttr.data.attribs, NL_ATTR_VAL(NL80211_SURVEY_INFO_TIME_BUSY, pElt->timeBusy));
        NL_ATTRS_ADD(&infoaAttr.data.attribs, NL_ATTR_VAL(NL80211_SURVEY_INFO_TIME_RX, pElt->timeRx));
        NL_ATTRS_ADD(&infoaAttr.data.attribs, NL_ATTR_VAL(NL80211_SURVEY_INFO_TIME_TX, pElt->timeTx));
        swl_unLiList_add(&attribs, &infoaAttr);
        wld_nl80211_addNlAttrs(msgReply, &attribs);
        write(fd, (void*) nlmsg_hdr(msgReply), nlmsg_hdr(msgReply)->nlmsg_len);
        nlmsg_free(msgReply);
        NL_ATTRS_CLEAR(&attribs);
    }

    if(hasMulti) {
        struct nl_msg* msgReply = s_mirrorNlMsgExt(msg, NLMSG_DONE, 0, NLM_F_MULTI);
        write(fd, (void*) nlmsg_hdr(msgReply), nlmsg_hdr(msgReply)->nlmsg_len);
        nlmsg_free(msgReply);
    }
    return 0;
}

static void test_wld_nl80211_getChanSurveyInfo(void** mockaState _UNUSED) {
    assert_true(s_stateMockInit(&mockGetChanSurvey.stateMock));
    //tweak: override nl_send API to catch request and build reply locally
    mockGetChanSurvey.stateMock.state->fNlSendPriv = s_nlSend_chanSurveyInfo;
    wld_nl80211_channelSurveyInfo_t expectedList[] = {
        {
            .frequencyMHz = 5180,
        },
        {
            .frequencyMHz = 5200,
        },
        {
            .frequencyMHz = 5220,
            .noiseDbm = -104,
            .timeOn = 56546072,
            .timeBusy = 28262392,
            .timeRx = 27749,
            .timeTx = 27694016,
            .inUse = true,
        },
        {
            .frequencyMHz = 5240,
            .noiseDbm = -101,
            .timeOn = 136,
            .timeBusy = 5,
        },
        {
            .frequencyMHz = 5260,
        },
    };
    mockGetChanSurvey.expectedData = expectedList;
    mockGetChanSurvey.nExpectedElts = SWL_ARRAY_SIZE(expectedList);
    wld_nl80211_channelSurveyInfo_t* pResChanSurveyInfo = NULL;
    uint32_t nResChanSurveyInfo = 0;
    swl_rc_ne rc = wld_nl80211_getSurveyInfo(mockGetChanSurvey.stateMock.state, 14, &pResChanSurveyInfo, &nResChanSurveyInfo);
    assert_true(rc >= SWL_RC_OK);
    assert_int_equal(nResChanSurveyInfo, mockGetChanSurvey.nExpectedElts);
    for(uint32_t i = 0; i < nResChanSurveyInfo; i++) {
        assert_int_equal(pResChanSurveyInfo[i].frequencyMHz, expectedList[i].frequencyMHz);
        assert_int_equal(pResChanSurveyInfo[i].noiseDbm, expectedList[i].noiseDbm);
        assert_int_equal(pResChanSurveyInfo[i].inUse, expectedList[i].inUse);
        assert_int_equal(pResChanSurveyInfo[i].timeOn, expectedList[i].timeOn);
        assert_int_equal(pResChanSurveyInfo[i].timeBusy, expectedList[i].timeBusy);
        assert_int_equal(pResChanSurveyInfo[i].timeRx, expectedList[i].timeRx);
        assert_int_equal(pResChanSurveyInfo[i].timeTx, expectedList[i].timeTx);
    }
    free(pResChanSurveyInfo);
    s_stateMockDeInit(&mockGetChanSurvey.stateMock);
}

static swl_rc_ne s_getItfCb(swl_rc_ne rc, struct nlmsghdr* nlh _UNUSED, void* priv _UNUSED) {
    return rc;
}

static swl_rc_ne s_getScanCb(swl_rc_ne rc, struct nlmsghdr* nlh _UNUSED, void* priv) {
    if(rc == SWL_RC_OK) {
        // send sync cmd which will expire while we are still in this callback
        stateMock_t* mock = (stateMock_t*) priv;
        mock->state->fNlSendPriv = s_sendNothingAndJumpToSyncTimeout;
        swl_rc_ne rc = wld_nl80211_sendCmdSync(mock->state, NL80211_CMD_GET_INTERFACE, 0, 456, NULL, s_getItfCb, NULL);
        assert_int_equal(SWL_RC_NOT_AVAILABLE, rc);
        return SWL_RC_DONE;
    }
    return rc;
}

static void test_wld_nl80211_request_expires_while_in_callback(void** mockaState _UNUSED) {
    stateMock_t mock;
    assert_true(s_stateMockInit(&mock));

    // send GET_SCAN request
    mock.state->fNlSendPriv = s_sendNothing;
    swl_rc_ne rc = wld_nl80211_sendCmd(false, mock.state, NL80211_CMD_GET_SCAN, NLM_F_DUMP, 123, NULL, s_getScanCb, &mock, NULL);
    assert_int_equal(SWL_RC_OK, rc);

    // reply to GET_SCAN with NLMSG_DONE
    struct nl_msg* msg = nlmsg_alloc();
    nlmsg_put(msg, 0, gLastSentSeqId, NLMSG_DONE, 0, 0);
    int n = write(mock.pipeFds[1], (void*) nlmsg_hdr(msg), nlmsg_hdr(msg)->nlmsg_len);
    assert_int_equal(n, nlmsg_hdr(msg)->nlmsg_len);
    nlmsg_free(msg);

    //run local event loop, until all requests are processed
    wld_nl80211_stateCounters_t counters = {};
    do {
        s_runEventLoopIter();
        wld_nl80211_getAllCounters(&counters);
    } while(counters.reqPending > 0);

    assert_true(s_stateMockDeInit(&mock));
}

int main(int argc _UNUSED, char* argv[] _UNUSED) {
    sahTraceOpen(__FILE__, TRACE_TYPE_STDERR);
    if(!sahTraceIsOpen()) {
        fprintf(stderr, "FAILED to open SAH TRACE\n");
    }
    sahTraceSetLevel(TRACE_LEVEL_WARNING);
    sahTraceAddZone(sahTraceLevel(), "nl80211");
    sahTraceAddZone(sahTraceLevel(), "nlCore");
    sahTraceSetTimeFormat(TRACE_TIME_APP_SECONDS);
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_wld_nl80211_newState),
        cmocka_unit_test(test_wld_nl80211_getSharedState),
        cmocka_unit_test(test_wld_nl80211_setEvtListeners),
        cmocka_unit_test(test_wld_nl80211_sendCmdSyncAck),
        cmocka_unit_test(test_wld_nl80211_sendCmdWithAttrs),
        cmocka_unit_test(test_wld_nl80211_sendCmdSync),
        cmocka_unit_test(test_wld_nl80211_sendCmdAsync),
        cmocka_unit_test(test_wld_nl80211_callEvtListeners),
        cmocka_unit_test(test_wld_nl80211_getIfaceInfo),
        cmocka_unit_test(test_wld_nl80211_getWiphyInfo),
        cmocka_unit_test(test_wld_nl80211_getAllWiphyInfo),
        cmocka_unit_test_setup_teardown(test_wld_nl80211_getScanResults, s_test_getScanResults_setup, s_test_getScanResults_teardown),
        cmocka_unit_test(test_wld_nl80211_getChanSurveyInfo),
        cmocka_unit_test(test_wld_nl80211_request_expires_while_in_callback),
    };
    int rc = cmocka_run_group_tests(tests, setup_suite, teardown_suite);
    sahTraceClose();
    return rc;
}
