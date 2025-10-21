/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2024 SoftAtHome
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

#include "swl/swl_common.h"
#include "swl/swl_string.h"
#include <amxc/amxc.h>
#include <amxp/amxp.h>
#include "swl/swl_maps.h"
#include "wld_secDmnGrp_priv.h"

#define ME "secDmnG"

#define ACTION_DELAY_MS 500

typedef enum {
    WLD_SECDMN_STATE_IDLE,                          /* member is idle: not started or fully terminated (after process end) */
    WLD_SECDMN_STATE_START,                         /* member asked to start */
    WLD_SECDMN_STATE_STOP,                          /* member asked to stop */
    WLD_SECDMN_STATE_MAX,
} wld_secDmn_state_e;

struct wld_secDmnGrp {
    char* name;                                     /* group name */
    bool isRunning;                                 /* group running state */
    wld_process_t* dmnProcess;                      /* daemon context. */
    amxp_timer_t* actionTimer;                      /* delayed action timer: to give time to cumulate start/stop requests */
    void* userData;                                 /* private user data */
    wld_secDmnGrp_EvtHandlers_t handlers;           /* group event handlers: to allow customizing group process (cmdline, args,...) */
    amxc_llist_t members;                           /* list of secDmn group members, running into same daemon process */
};

/*
 * enum for secDmn group actions to execute at runtime
 */
typedef enum {
    WLD_SECDMN_RTM_ACTION_RESTART,
    WLD_SECDMN_RTM_ACTION_MAX,
} wld_secDmn_runTimeAction_e;

#define M_WLD_SECDMN_RTM_ACTION_RESTART SWL_BIT_SHIFT(WLD_SECDMN_RTM_ACTION_RESTART)

typedef swl_mask64_m wld_secDmn_runTimeAction_m;

typedef struct {
    amxc_llist_it_t it;
    char* name;                                     /* member name */
    wld_secDmn_t* pSecDmn;                          /* member secDmn context */
    wld_secDmn_state_e state;                       /* member state */
    bool isStartable;                               /* flag to indicate if daemon is started or ready to be */
    wld_deamonEvtHandlers dmnEvtHdlrs;              /* member evt handlers: to forward proc event from group to members */
    void* dmnEvtUserData;                           /* member evt user data */
    wld_secDmn_runTimeAction_m reqRtmActions;       /* bitmap of requested run time actions. */
} wld_secDmnGrp_member_t;

static void s_restartProcCb(wld_process_t* pProc, void* userdata) {
    wld_secDmnGrp_t* pSecDmnGrp = (wld_secDmnGrp_t*) userdata;
    ASSERT_NOT_NULL(pSecDmnGrp, , ME, "NULL");
    amxc_llist_for_each(it, &pSecDmnGrp->members) {
        wld_secDmnGrp_member_t* member = amxc_container_of(it, wld_secDmnGrp_member_t, it);
        SAH_TRACEZ_INFO(ME, "notify restart to member %s (st:%d)", member->name, member->state);
        SWL_CALL(member->dmnEvtHdlrs.restartCb, pProc, member->dmnEvtUserData);
    }
}

static void s_onStopProcCb(wld_process_t* pProc _UNUSED, void* userdata) {
    wld_secDmnGrp_t* pSecDmnGrp = (wld_secDmnGrp_t*) userdata;
    ASSERT_NOT_NULL(pSecDmnGrp, , ME, "NULL");
    pSecDmnGrp->isRunning = false;
    amxc_llist_for_each(it, &pSecDmnGrp->members) {
        wld_secDmnGrp_member_t* member = amxc_container_of(it, wld_secDmnGrp_member_t, it);
        SAH_TRACEZ_INFO(ME, "notify stop to member %s (st:%d)", member->name, member->state);
        if(member->state == WLD_SECDMN_STATE_STOP) {
            member->state = WLD_SECDMN_STATE_IDLE;
        }
        SWL_CALL(member->dmnEvtHdlrs.stopCb, pProc, member->dmnEvtUserData);
    }
}

static void s_onStartProcCb(wld_process_t* pProc, void* userdata) {
    wld_secDmnGrp_t* pSecDmnGrp = (wld_secDmnGrp_t*) userdata;
    ASSERT_NOT_NULL(pSecDmnGrp, , ME, "NULL");
    pSecDmnGrp->isRunning = true;
    amxc_llist_for_each(it, &pSecDmnGrp->members) {
        wld_secDmnGrp_member_t* member = amxc_container_of(it, wld_secDmnGrp_member_t, it);
        SAH_TRACEZ_INFO(ME, "notify start to member %s (st:%d)", member->name, member->state);
        SWL_CALL(member->dmnEvtHdlrs.startCb, pProc, member->dmnEvtUserData);
    }
}

static char* s_getArgsProcCb(wld_process_t* pProc, void* userdata) {
    wld_secDmnGrp_t* pSecDmnGrp = (wld_secDmnGrp_t*) userdata;
    ASSERT_NOT_NULL(pSecDmnGrp, NULL, ME, "NULL");
    if(pSecDmnGrp->handlers.getArgsCb != NULL) {
        return pSecDmnGrp->handlers.getArgsCb(pSecDmnGrp, pSecDmnGrp->userData, pProc);
    }
    return NULL;
}

static bool s_stopProcCb(wld_process_t* pProc, void* userdata) {
    wld_secDmnGrp_t* pSecDmnGrp = (wld_secDmnGrp_t*) userdata;
    ASSERT_NOT_NULL(pSecDmnGrp, false, ME, "NULL");
    bool ret = false;
    amxc_llist_for_each(it, &pSecDmnGrp->members) {
        wld_secDmnGrp_member_t* member = amxc_container_of(it, wld_secDmnGrp_member_t, it);
        if(member->dmnEvtHdlrs.stop) {
            SAH_TRACEZ_INFO(ME, "stop proc grp %s with member %s", pSecDmnGrp->name, member->name);
            ret |= member->dmnEvtHdlrs.stop(pProc, member->dmnEvtUserData);
        }
    }
    return ret;
}

static wld_deamonEvtHandlers fProcCbs = {
    .restartCb = s_restartProcCb,
    .stopCb = s_onStopProcCb,
    .startCb = s_onStartProcCb,
    .getArgsCb = s_getArgsProcCb,
    .stop = s_stopProcCb,
};

static void s_processGrpAction(amxp_timer_t* timer, void* userdata);

swl_rc_ne wld_secDmnGrp_init(wld_secDmnGrp_t** ppSecDmnGrp, char* cmd, char* startArgs, const char* groupName) {
    ASSERT_STR(cmd, SWL_RC_INVALID_PARAM, ME, "invalid cmd");
    ASSERT_NOT_NULL(ppSecDmnGrp, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_secDmnGrp_t* pSecDmnGrp = *ppSecDmnGrp;
    if(pSecDmnGrp == NULL) {
        pSecDmnGrp = calloc(1, sizeof(wld_secDmnGrp_t));
        ASSERT_NOT_NULL(pSecDmnGrp, SWL_RC_ERROR, ME, "NULL");
        amxc_llist_init(&pSecDmnGrp->members);
        amxp_timer_new(&pSecDmnGrp->actionTimer, s_processGrpAction, pSecDmnGrp);
        *ppSecDmnGrp = pSecDmnGrp;
        if(!swl_str_isEmpty(groupName)) {
            swl_str_copyMalloc(&pSecDmnGrp->name, groupName);
        } else if(swl_str_isEmpty(pSecDmnGrp->name)) {
            char dfltName[swl_str_len(cmd) + 32];
            //default group name: CMD-xxxx
            snprintf(dfltName, sizeof(dfltName), "%s-%p", cmd, pSecDmnGrp);
            swl_str_copyMalloc(&pSecDmnGrp->name, dfltName);
        }
        SAH_TRACEZ_INFO(ME, "new secDmn group %s allocated", pSecDmnGrp->name);
    }
    ASSERTI_NULL(pSecDmnGrp->dmnProcess, SWL_RC_OK, ME, "secDmn group %s already initialized", pSecDmnGrp->name);
    swl_rc_ne rc = wld_dmn_createDeamon(&pSecDmnGrp->dmnProcess, cmd, startArgs, &fProcCbs, pSecDmnGrp);
    ASSERT_TRUE(swl_rc_isOk(rc), rc, ME, "fail to init secDmn group %s", pSecDmnGrp->name);
    SAH_TRACEZ_INFO(ME, "secDmn group %s initialized", pSecDmnGrp->name);
    return rc;
}

static wld_secDmnGrp_member_t* s_getGrpMember(wld_secDmnGrp_t* pSecDmnGrp, wld_secDmn_t* pSecDmn) {
    ASSERTS_NOT_NULL(pSecDmnGrp, NULL, ME, "NULL");
    ASSERTS_NOT_NULL(pSecDmn, NULL, ME, "NULL");
    amxc_llist_for_each(it, &pSecDmnGrp->members) {
        wld_secDmnGrp_member_t* member = amxc_container_of(it, wld_secDmnGrp_member_t, it);
        if(member->pSecDmn == pSecDmn) {
            return member;
        }
    }
    return NULL;
}

bool wld_secDmnGrp_hasMember(wld_secDmnGrp_t* pSecDmnGrp, wld_secDmn_t* pSecDmn) {
    return (s_getGrpMember(pSecDmnGrp, pSecDmn) != NULL);
}

bool wld_secDmnGrp_hasProc(wld_secDmnGrp_t* pSecDmnGrp, wld_process_t* pDmnProcess) {
    ASSERTS_NOT_NULL(pSecDmnGrp, false, ME, "NULL");
    ASSERTS_NOT_NULL(pDmnProcess, false, ME, "NULL");
    return (pSecDmnGrp->dmnProcess == pDmnProcess);
}

wld_process_t* wld_secDmnGrp_getProc(wld_secDmnGrp_t* pSecDmnGrp) {
    ASSERTS_NOT_NULL(pSecDmnGrp, NULL, ME, "NULL");
    return pSecDmnGrp->dmnProcess;
}

/*
 * @brief internal api to add secDmn member to group
 */
swl_rc_ne wld_secDmnGrp_addMember(wld_secDmnGrp_t* pSecDmnGrp, wld_secDmn_t* pSecDmn, const char* memberName) {
    ASSERTS_NOT_NULL(pSecDmnGrp, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pSecDmn, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_secDmnGrp_member_t* member = s_getGrpMember(pSecDmnGrp, pSecDmn);
    ASSERTI_NULL(member, SWL_RC_OK, ME, "secDmn %p (%s) is already added to grp %p", pSecDmn, member->name, pSecDmnGrp);
    wld_secDmnGrp_t* pCurrMemberGrp = wld_secDmn_getGrp(pSecDmn);
    ASSERT_NULL(pCurrMemberGrp, SWL_RC_ERROR, ME, "secDmn %p is already in another group %p (vs %p)", pSecDmn, pCurrMemberGrp, pSecDmnGrp);

    member = calloc(1, sizeof(*member));
    ASSERT_NOT_NULL(member, SWL_RC_ERROR, ME, "NULL");
    member->pSecDmn = pSecDmn;
    member->state = WLD_SECDMN_STATE_IDLE;
    char name[swl_str_len(pSecDmnGrp->name) + SWL_MAX(swl_str_len(memberName), (size_t) 32) + 2];
    if(!swl_str_isEmpty(memberName)) {
        //custom member name: GROUP_NAME-MEMBER_NAME
        snprintf(name, sizeof(name), "%s-%s", pSecDmnGrp->name, memberName);
    } else {
        //default member name: GROUP_NAME-memberId
        snprintf(name, sizeof(name), "%s-%zu", pSecDmnGrp->name, amxc_llist_size(&pSecDmnGrp->members));
    }
    swl_str_copyMalloc(&member->name, name);
    amxc_llist_append(&pSecDmnGrp->members, &member->it);
    SAH_TRACEZ_INFO(ME, "added member (%s/%p) to group (%s/%p)", member->name, pSecDmn, pSecDmnGrp->name, pSecDmnGrp);
    return SWL_RC_OK;
}

/*
 * @brief internal api to save secDmn member proc event handlers (/usedata): needed to forward proc event
 */
swl_rc_ne wld_secDmnGrp_setMemberEvtHdlrs(wld_secDmnGrp_t* pSecDmnGrp, wld_secDmn_t* pSecDmn, wld_deamonEvtHandlers* pHdlrs, void* priv) {
    wld_secDmnGrp_member_t* member = s_getGrpMember(pSecDmnGrp, pSecDmn);
    ASSERTS_NOT_NULL(member, SWL_RC_INVALID_PARAM, ME, "Unknown member");
    if(pHdlrs != NULL) {
        memcpy(&member->dmnEvtHdlrs, pHdlrs, sizeof(*pHdlrs));
    } else {
        memset(&member->dmnEvtHdlrs, 0, sizeof(*pHdlrs));
    }
    member->dmnEvtUserData = priv;
    return SWL_RC_OK;
}

static uint32_t s_countGrpMembersInState(wld_secDmnGrp_t* pSecDmnGrp, wld_secDmn_state_e state) {
    uint32_t count = 0;
    amxc_llist_for_each(it, &pSecDmnGrp->members) {
        wld_secDmnGrp_member_t* member = amxc_container_of(it, wld_secDmnGrp_member_t, it);
        count += (member->state == state);
    }
    return count;
}

static uint32_t s_countGrpMembersStartable(wld_secDmnGrp_t* pSecDmnGrp) {
    uint32_t count = 0;
    amxc_llist_for_each(it, &pSecDmnGrp->members) {
        wld_secDmnGrp_member_t* member = amxc_container_of(it, wld_secDmnGrp_member_t, it);
        count += (member->isStartable == true);
    }
    return count;
}

static bool s_hasGrpOngoingAction(wld_secDmnGrp_t* pSecDmnGrp) {
    amxp_timer_state_t timerSt = amxp_timer_get_state(pSecDmnGrp->actionTimer);
    return ((timerSt == amxp_timer_running) || (timerSt == amxp_timer_started));
}

static swl_rc_ne s_stopGrp(wld_secDmnGrp_t* pSecDmnGrp, bool force) {
    ASSERTS_NOT_NULL(pSecDmnGrp, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_TRUE(wld_dmn_isRunning(pSecDmnGrp->dmnProcess), SWL_RC_OK, ME, "secDmn already stopped");
    uint32_t nbMStarted = s_countGrpMembersInState(pSecDmnGrp, WLD_SECDMN_STATE_START);
    ASSERTI_TRUE(force || (nbMStarted == 0), SWL_RC_CONTINUE,
                 ME, "secDmn group %s still has %d started member", pSecDmnGrp->name, nbMStarted);
    if(s_hasGrpOngoingAction(pSecDmnGrp)) {
        SAH_TRACEZ_INFO(ME, "%s: delayed actions are pending: cancelled", pSecDmnGrp->name);
    }
    amxp_timer_stop(pSecDmnGrp->actionTimer);
    SAH_TRACEZ_INFO(ME, "stop group %s (nbMS:%d,force:%d)", pSecDmnGrp->name, nbMStarted, force);
    s_onStopProcCb(pSecDmnGrp->dmnProcess, pSecDmnGrp);
    ASSERT_TRUE(wld_dmn_stopDeamon(pSecDmnGrp->dmnProcess), SWL_RC_ERROR, ME, "fail to stop secDmn group proc");
    return SWL_RC_OK;
}

static swl_rc_ne s_stopGrpMember(wld_secDmnGrp_t* pSecDmnGrp, wld_secDmnGrp_member_t* member) {
    ASSERTS_NOT_NULL(member, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pSecDmnGrp, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTI_EQUALS(member->state, WLD_SECDMN_STATE_START, SWL_RC_INVALID_STATE, ME, "member %s not running (st:%d)", member->name, member->state);
    SAH_TRACEZ_INFO(ME, "stop member %s (st:%d) of group %s", member->name, member->state, pSecDmnGrp->name);
    if(member->isStartable) {
        SAH_TRACEZ_INFO(ME, "assume member %s no more startable, as being stopped", member->name);
        member->isStartable = false;
    }
    if(!wld_dmn_isRunning(pSecDmnGrp->dmnProcess)) {
        SAH_TRACEZ_INFO(ME, "secDmn group %s proc is stopped", pSecDmnGrp->name);
        if(member->state != WLD_SECDMN_STATE_IDLE) {
            SAH_TRACEZ_INFO(ME, "late notify stop to member %s (st:%d)", member->name, member->state);
            member->state = WLD_SECDMN_STATE_IDLE;
            SWL_CALL(member->dmnEvtHdlrs.stopCb, pSecDmnGrp->dmnProcess, member->dmnEvtUserData);
        }
        return SWL_RC_DONE;
    }
    member->state = WLD_SECDMN_STATE_STOP;
    member->reqRtmActions = 0;
    return s_stopGrp(pSecDmnGrp, false);
}

/*
 * @brief stop a secDmn group member
 * The group daemon process is finally stopped when all started members are stopped
 *
 * @param pSecDmnGrp security daemon group ctx
 * @param pSecDmn security daemon member ctx
 *
 * @return SWL_RC_OK when group's daemon process is being stopped
 *         SWL_RC_CONTINUE when member is stopped but process waits for other members to stop
 *         SWL_RC_DONE when daemon process is already stopped (not running)
 *         error code otherwise
 */
swl_rc_ne wld_secDmnGrp_stopMember(wld_secDmnGrp_t* pSecDmnGrp, wld_secDmn_t* pSecDmn) {
    wld_secDmnGrp_member_t* member = s_getGrpMember(pSecDmnGrp, pSecDmn);
    ASSERTS_NOT_NULL(member, SWL_RC_INVALID_PARAM, ME, "NULL");
    return s_stopGrpMember(pSecDmnGrp, member);
}

swl_rc_ne wld_secDmnGrp_delMember(wld_secDmnGrp_t* pSecDmnGrp, wld_secDmn_t* pSecDmn) {
    wld_secDmnGrp_member_t* member = s_getGrpMember(pSecDmnGrp, pSecDmn);
    ASSERTS_NOT_NULL(member, SWL_RC_INVALID_PARAM, ME, "NULL");
    if(s_stopGrpMember(pSecDmnGrp, member) == SWL_RC_CONTINUE) {
        member->state = WLD_SECDMN_STATE_IDLE;
        SWL_CALL(member->dmnEvtHdlrs.stopCb, pSecDmnGrp->dmnProcess, member->dmnEvtUserData);
    }
    SAH_TRACEZ_INFO(ME, "delete member %s (st:%d) from group %s", member->name, member->state, pSecDmnGrp->name);
    amxc_llist_it_take(&member->it);
    W_SWL_FREE(member->name);
    free(member);
    return SWL_RC_OK;
}

swl_rc_ne wld_secDmnGrp_dropMembers(wld_secDmnGrp_t* pSecDmnGrp) {
    ASSERTS_NOT_NULL(pSecDmnGrp, SWL_RC_INVALID_PARAM, ME, "NULL");
    s_stopGrp(pSecDmnGrp, true);
    amxc_llist_for_each(it, &pSecDmnGrp->members) {
        wld_secDmnGrp_member_t* member = amxc_container_of(it, wld_secDmnGrp_member_t, it);
        wld_secDmn_delFromGrp(member->pSecDmn);
    }
    return SWL_RC_OK;
}

swl_rc_ne wld_secDmnGrp_cleanup(wld_secDmnGrp_t** ppSecDmnGrp) {
    ASSERTS_NOT_NULL(ppSecDmnGrp, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_secDmnGrp_t* pSecDmnGrp = *ppSecDmnGrp;
    ASSERTS_NOT_NULL(pSecDmnGrp, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_secDmnGrp_dropMembers(pSecDmnGrp);
    amxp_timer_delete(&pSecDmnGrp->actionTimer);
    W_SWL_FREE(pSecDmnGrp->name);
    W_SWL_FREE(*ppSecDmnGrp);
    return SWL_RC_OK;
}

swl_rc_ne wld_secDmnGrp_setEvtHandlers(wld_secDmnGrp_t* pSecDmnGrp, wld_secDmnGrp_EvtHandlers_t* pHandlers, void* userData) {
    ASSERT_NOT_NULL(pSecDmnGrp, SWL_RC_INVALID_PARAM, ME, "NULL");
    pSecDmnGrp->userData = userData;
    if(pHandlers == NULL) {
        memset(&pSecDmnGrp->handlers, 0, sizeof(wld_secDmnGrp_EvtHandlers_t));
    } else {
        memcpy(&pSecDmnGrp->handlers, pHandlers, sizeof(wld_secDmnGrp_EvtHandlers_t));
    }
    return SWL_RC_OK;
}

static void s_refreshStartableMembers(wld_secDmnGrp_t* pSecDmnGrp) {
    ASSERTS_NOT_NULL(pSecDmnGrp, , ME, "NULL");
    ASSERTS_NOT_NULL(pSecDmnGrp->handlers.isMemberStartableCb, , ME, "No handler");
    amxc_llist_for_each(it, &pSecDmnGrp->members) {
        wld_secDmnGrp_member_t* member = amxc_container_of(it, wld_secDmnGrp_member_t, it);
        member->isStartable = pSecDmnGrp->handlers.isMemberStartableCb(pSecDmnGrp, pSecDmnGrp->userData, member->pSecDmn);
    }
}

static uint32_t s_countGrpMembersReqRtmAction(wld_secDmnGrp_t* pSecDmnGrp, wld_secDmn_runTimeAction_e action) {
    ASSERT_NOT_NULL(pSecDmnGrp, 0, ME, "NULL");
    uint32_t count = 0;
    amxc_llist_for_each(it, &pSecDmnGrp->members) {
        wld_secDmnGrp_member_t* member = amxc_container_of(it, wld_secDmnGrp_member_t, it);
        ASSERT_TRUE(action < SWL_BIT_SIZE(member->reqRtmActions), 0, ME, "out of bound");
        count += SWL_BIT_IS_SET(member->reqRtmActions, action);
    }
    return count;
}

static uint32_t s_countGrpMembersReqAnyRtmAction(wld_secDmnGrp_t* pSecDmnGrp) {
    ASSERT_NOT_NULL(pSecDmnGrp, 0, ME, "NULL");
    uint32_t count = 0;
    for(uint32_t i = 0; i < WLD_SECDMN_RTM_ACTION_MAX; i++) {
        count += s_countGrpMembersReqRtmAction(pSecDmnGrp, i);
    }
    return count;
}

static swl_rc_ne s_startGrp(wld_secDmnGrp_t* pSecDmnGrp) {
    ASSERT_NOT_NULL(pSecDmnGrp, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pSecDmnGrp->dmnProcess, SWL_RC_INVALID_PARAM, ME, "NULL");
    SAH_TRACEZ_INFO(ME, "check group %s start conditions", pSecDmnGrp->name);
    s_refreshStartableMembers(pSecDmnGrp);
    uint32_t nbMStartable = s_countGrpMembersStartable(pSecDmnGrp);
    uint32_t nbMStarted = s_countGrpMembersInState(pSecDmnGrp, WLD_SECDMN_STATE_START);
    if(wld_dmn_isRunning(pSecDmnGrp->dmnProcess)) {
        if(!nbMStartable) {
            SAH_TRACEZ_WARNING(ME, "group %s proc running while it must not: members (started:%d/startable:%d) => force stop",
                               pSecDmnGrp->name, nbMStarted, nbMStartable);
            s_stopGrp(pSecDmnGrp, true);
            return SWL_RC_INVALID_STATE;
        }
        SAH_TRACEZ_INFO(ME, "group %s already started", pSecDmnGrp->name);
        if(!pSecDmnGrp->isRunning) {
            s_onStartProcCb(pSecDmnGrp->dmnProcess, pSecDmnGrp);
        }
        return SWL_RC_DONE;
    }
    ASSERTW_TRUE(nbMStartable > 0, SWL_RC_INVALID_STATE, ME, "group %s has no startable members", pSecDmnGrp->name);
    ASSERTI_TRUE(nbMStarted > 0, SWL_RC_ERROR, ME, "group %s has no started members", pSecDmnGrp->name);
    if(nbMStarted >= nbMStartable) {
        SAH_TRACEZ_INFO(ME, "all %d startable members of group %s are started, go ahead", nbMStarted, pSecDmnGrp->name);
        bool ret = wld_dmn_startDeamon(pSecDmnGrp->dmnProcess);
        ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "fail to start group %s process", pSecDmnGrp->name);
        if(s_countGrpMembersReqAnyRtmAction(pSecDmnGrp) == 0) {
            SAH_TRACEZ_INFO(ME, "no pending group %s actions", pSecDmnGrp->name);
            amxp_timer_stop(pSecDmnGrp->actionTimer);
        }
        return SWL_RC_OK;
    }

    SAH_TRACEZ_INFO(ME, "delay starting group %s, to wait for others to join (started:%d/startable:%d)",
                    pSecDmnGrp->name, nbMStarted, nbMStartable);
    amxp_timer_start(pSecDmnGrp->actionTimer, ACTION_DELAY_MS);
    return SWL_RC_CONTINUE;
}

static bool s_checkGrpMemberCurRtmAction(wld_secDmnGrp_t* pSecDmnGrp, wld_secDmnGrp_member_t* member, wld_secDmn_runTimeAction_e action) {
    ASSERTI_NOT_NULL(pSecDmnGrp, false, ME, "NULL");
    ASSERTI_NOT_NULL(member, false, ME, "NULL");
    ASSERTS_EQUALS(member->state, WLD_SECDMN_STATE_START, false, ME, "Member not started");
    switch(action) {
    case WLD_SECDMN_RTM_ACTION_RESTART: {
        if(pSecDmnGrp->handlers.hasSchedRestartCb != NULL) {
            return pSecDmnGrp->handlers.hasSchedRestartCb(pSecDmnGrp, pSecDmnGrp->userData, member->pSecDmn);
        }
        break;
    }
    default: break;
    }
    return false;
}

static uint32_t s_countGrpMembersCurRtmAction(wld_secDmnGrp_t* pSecDmnGrp, wld_secDmn_runTimeAction_e action) {
    ASSERT_NOT_NULL(pSecDmnGrp, 0, ME, "NULL");
    uint32_t count = 0;
    amxc_llist_for_each(it, &pSecDmnGrp->members) {
        wld_secDmnGrp_member_t* member = amxc_container_of(it, wld_secDmnGrp_member_t, it);
        if(s_checkGrpMemberCurRtmAction(pSecDmnGrp, member, action)) {
            SAH_TRACEZ_INFO(ME, "member %s has rtm action %d", member->name, action);
            count++;
        }
    }
    SAH_TRACEZ_INFO(ME, "%d members of %s have rtm action %d", count, pSecDmnGrp->name, action);
    return count;
}

static wld_secDmnGrp_member_t* s_getFirstGrpMemberReqRtmAction(wld_secDmnGrp_t* pSecDmnGrp, wld_secDmn_runTimeAction_e action) {
    ASSERT_NOT_NULL(pSecDmnGrp, NULL, ME, "NULL");
    amxc_llist_for_each(it, &pSecDmnGrp->members) {
        wld_secDmnGrp_member_t* member = amxc_container_of(it, wld_secDmnGrp_member_t, it);
        ASSERT_TRUE(action < SWL_BIT_SIZE(member->reqRtmActions), 0, ME, "out of bound");
        if((member->state == WLD_SECDMN_STATE_START) &&
           (SWL_BIT_IS_SET(member->reqRtmActions, action))) {
            return member;
        }
    }
    return NULL;
}

static void s_clearGrpMembersReqRtmAction(wld_secDmnGrp_t* pSecDmnGrp, wld_secDmn_runTimeAction_e action) {
    ASSERT_NOT_NULL(pSecDmnGrp, , ME, "NULL");
    amxc_llist_for_each(it, &pSecDmnGrp->members) {
        wld_secDmnGrp_member_t* member = amxc_container_of(it, wld_secDmnGrp_member_t, it);
        ASSERT_TRUE(action < SWL_BIT_SIZE(member->reqRtmActions), , ME, "out of bound");
        W_SWL_BIT_CLEAR(member->reqRtmActions, action);
    }
}

static void s_execGrpRtmAction(wld_secDmnGrp_t* pSecDmnGrp, wld_secDmn_runTimeAction_e action) {
    ASSERT_NOT_NULL(pSecDmnGrp, , ME, "NULL");
    wld_secDmnGrp_member_t* member = s_getFirstGrpMemberReqRtmAction(pSecDmnGrp, action);
    ASSERTI_NOT_NULL(member, , ME, "No member of %s requesting action %d", pSecDmnGrp->name, action);
    ASSERT_NOT_NULL(member->pSecDmn, , ME, "member %s has no secDmn", member->name);
    switch(action) {
    case WLD_SECDMN_RTM_ACTION_RESTART: {
        wld_dmn_restartDeamon(member->pSecDmn->dmnProcess);
        break;
    }
    default: break;
    }
}

static swl_rc_ne s_tryGrpRtmAction(wld_secDmnGrp_t* pSecDmnGrp, wld_secDmn_runTimeAction_e action) {
    ASSERT_NOT_NULL(pSecDmnGrp, SWL_RC_INVALID_PARAM, ME, "NULL");
    uint32_t nbMReq = s_countGrpMembersReqRtmAction(pSecDmnGrp, action);
    ASSERTS_TRUE(nbMReq > 0, SWL_RC_DONE, ME, "%s: No action %d requested", pSecDmnGrp->name);
    uint32_t nbMPend = s_countGrpMembersCurRtmAction(pSecDmnGrp, action);
    if((pSecDmnGrp->isRunning) && (nbMPend == 0)) {
        SAH_TRACEZ_INFO(ME, "%d members of group %s requested action %d, and none is pending: go ahead",
                        nbMReq, pSecDmnGrp->name, action);
        s_execGrpRtmAction(pSecDmnGrp, action);
        s_clearGrpMembersReqRtmAction(pSecDmnGrp, action);
        return SWL_RC_OK;
    }
    SAH_TRACEZ_INFO(ME, "delay rtm action %d for group %s, to wait for others to be ready (req:%d/pending:%d)",
                    action, pSecDmnGrp->name, nbMReq, nbMPend);
    amxp_timer_start(pSecDmnGrp->actionTimer, ACTION_DELAY_MS);
    return SWL_RC_CONTINUE;
}

static swl_rc_ne s_tryGrpMemberRtmAction(wld_secDmnGrp_t* pSecDmnGrp, wld_secDmnGrp_member_t* member, wld_secDmn_runTimeAction_e action) {
    ASSERTS_NOT_NULL(member, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pSecDmnGrp, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTI_EQUALS(member->state, WLD_SECDMN_STATE_START, SWL_RC_INVALID_STATE, ME, "member %s not running (st:%d)", member->name, member->state);
    SAH_TRACEZ_INFO(ME, "request action %d for member %s (st:%d) of group %s", action, member->name, member->state, pSecDmnGrp->name);
    W_SWL_BIT_SET(member->reqRtmActions, action);
    return s_tryGrpRtmAction(pSecDmnGrp, action);
}

swl_rc_ne wld_secDmnGrp_restartMember(wld_secDmnGrp_t* pSecDmnGrp, wld_secDmn_t* pSecDmn) {
    wld_secDmnGrp_member_t* member = s_getGrpMember(pSecDmnGrp, pSecDmn);
    ASSERTS_NOT_NULL(member, SWL_RC_INVALID_PARAM, ME, "NULL");
    return s_tryGrpMemberRtmAction(pSecDmnGrp, member, WLD_SECDMN_RTM_ACTION_RESTART);
}

bool wld_secDmnGrp_isMemberRestarting(wld_secDmnGrp_t* pSecDmnGrp, wld_secDmn_t* pSecDmn) {
    wld_secDmnGrp_member_t* member = s_getGrpMember(pSecDmnGrp, pSecDmn);
    ASSERTS_NOT_NULL(member, SWL_RC_INVALID_PARAM, ME, "NULL");
    return SWL_BIT_IS_SET(member->reqRtmActions, WLD_SECDMN_RTM_ACTION_RESTART);
}

static void s_processGrpAction(amxp_timer_t* timer _UNUSED, void* userdata) {
    wld_secDmnGrp_t* pSecDmnGrp = (wld_secDmnGrp_t*) userdata;
    ASSERT_NOT_NULL(pSecDmnGrp, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "process group %s actions", pSecDmnGrp->name);
    swl_rc_ne rc = s_startGrp(pSecDmnGrp);
    if(rc == SWL_RC_DONE) {
        s_tryGrpRtmAction(pSecDmnGrp, WLD_SECDMN_RTM_ACTION_RESTART);
    }
    if(((rc == SWL_RC_OK) || (rc == SWL_RC_DONE)) &&
       (s_countGrpMembersReqAnyRtmAction(pSecDmnGrp) == 0)) {
        SAH_TRACEZ_INFO(ME, "all group %s actions are applied", pSecDmnGrp->name);
        amxp_timer_stop(pSecDmnGrp->actionTimer);
    }
}

static swl_rc_ne s_startGrpMember(wld_secDmnGrp_t* pSecDmnGrp, wld_secDmnGrp_member_t* member) {
    ASSERTS_NOT_NULL(member, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pSecDmnGrp, SWL_RC_INVALID_PARAM, ME, "NULL");
    s_refreshStartableMembers(pSecDmnGrp);
    if(!member->isStartable) {
        SAH_TRACEZ_INFO(ME, "assume member %s startable, as being started", member->name);
        member->isStartable = true;
    }
    bool wasStarted = (member->state == WLD_SECDMN_STATE_START);
    member->state = WLD_SECDMN_STATE_START;
    if(wld_dmn_isRunning(pSecDmnGrp->dmnProcess)) {
        SAH_TRACEZ_INFO(ME, "member %s already running", member->name);
        if(!wasStarted) {
            SAH_TRACEZ_INFO(ME, "notify start %s done to member %s", pSecDmnGrp->name, member->name);
            SWL_CALL(member->dmnEvtHdlrs.startCb, pSecDmnGrp->dmnProcess, member->dmnEvtUserData);
        }
        return SWL_RC_DONE;
    }
    if(wasStarted) {
        if(s_hasGrpOngoingAction(pSecDmnGrp)) {
            SAH_TRACEZ_INFO(ME, "member %s already starting", member->name);
            return SWL_RC_CONTINUE;
        }
        if(wld_dmn_isEnabled(pSecDmnGrp->dmnProcess)) {
            SAH_TRACEZ_INFO(ME, "member %s already started", member->name);
            return SWL_RC_OK;
        }
    }
    SAH_TRACEZ_INFO(ME, "start member %s (st:%d)", member->name, member->state);
    return s_startGrp(pSecDmnGrp);
}

/*
 * @brief start a secDmn group member
 * group process is started when all startable members are started
 *
 * @param pSecDmnGrp security daemon group ctx
 * @param pSecDmn security daemon member ctx
 *
 * @return SWL_RC_OK when group's daemon process is being started (last startable member has started)
 *         SWL_RC_CONTINUE when member is started but process waits for other members to start
 *         SWL_RC_DONE when daemon process is already started (running)
 *         error code otherwise
 */
swl_rc_ne wld_secDmnGrp_startMember(wld_secDmnGrp_t* pSecDmnGrp, wld_secDmn_t* pSecDmn) {
    wld_secDmnGrp_member_t* member = s_getGrpMember(pSecDmnGrp, pSecDmn);
    ASSERT_NOT_NULL(member, SWL_RC_INVALID_PARAM, ME, "no member found");
    return s_startGrpMember(pSecDmnGrp, member);
}

swl_rc_ne wld_secDmnGrp_setMemberStartable(wld_secDmnGrp_t* pSecDmnGrp, wld_secDmn_t* pSecDmn, bool isStartable) {
    wld_secDmnGrp_member_t* member = s_getGrpMember(pSecDmnGrp, pSecDmn);
    ASSERTI_NOT_NULL(member, SWL_RC_INVALID_PARAM, ME, "no member found");
    SAH_TRACEZ_INFO(ME, "set member %s startable %d", member->name, isStartable);
    member->isStartable = isStartable;
    return SWL_RC_OK;
}

uint32_t wld_secDmnGrp_getMembersCount(wld_secDmnGrp_t* pSecDmnGrp) {
    ASSERTS_NOT_NULL(pSecDmnGrp, 0, ME, "NULL");
    return amxc_llist_size(&pSecDmnGrp->members);
}

uint32_t wld_secDmnGrp_getActiveMembersCount(wld_secDmnGrp_t* pSecDmnGrp) {
    ASSERTS_NOT_NULL(pSecDmnGrp, 0, ME, "NULL");
    uint32_t count = 0;
    amxc_llist_for_each(it, &pSecDmnGrp->members) {
        wld_secDmnGrp_member_t* member = amxc_container_of(it, wld_secDmnGrp_member_t, it);
        count += wld_secDmn_isAlive(member->pSecDmn);
    }
    return count;
}

const wld_secDmn_t* wld_secDmnGrp_getMemberByPos(wld_secDmnGrp_t* pSecDmnGrp, int32_t pos) {
    ASSERTS_NOT_NULL(pSecDmnGrp, NULL, ME, "NULL");
    amxc_llist_it_t* itRes = NULL;
    if(pos >= 0) {
        itRes = amxc_llist_get_at(&pSecDmnGrp->members, pos);
    } else {
        int32_t tmpId = pos;
        amxc_llist_iterate_reverse(it, &pSecDmnGrp->members) {
            if((++tmpId) == 0) {
                itRes = it;
                break;
            }
        }
    }
    ASSERT_NOT_NULL(itRes, NULL, ME, "pos %d not found", pos);
    wld_secDmnGrp_member_t* member = amxc_container_of(itRes, wld_secDmnGrp_member_t, it);
    return member->pSecDmn;
}

const wld_secDmn_t* wld_secDmnGrp_getMemberByName(wld_secDmnGrp_t* pSecDmnGrp, const char* name) {
    ASSERTS_NOT_NULL(pSecDmnGrp, NULL, ME, "NULL");
    ASSERT_STR(name, NULL, ME, "no name");
    amxc_llist_for_each(it, &pSecDmnGrp->members) {
        wld_secDmnGrp_member_t* member = amxc_container_of(it, wld_secDmnGrp_member_t, it);
        if(swl_str_matches(member->name, name)) {
            return member->pSecDmn;
        }
    }
    return NULL;
}

bool wld_secDmnGrp_isEnabled(wld_secDmnGrp_t* pSecDmnGrp) {
    ASSERTS_NOT_NULL(pSecDmnGrp, false, ME, "NULL");
    return ((wld_dmn_isEnabled(pSecDmnGrp->dmnProcess)) ||
            (s_countGrpMembersInState(pSecDmnGrp, WLD_SECDMN_STATE_START) > 0));
}

bool wld_secDmnGrp_isRunning(wld_secDmnGrp_t* pSecDmnGrp) {
    ASSERTS_NOT_NULL(pSecDmnGrp, false, ME, "NULL");
    return wld_dmn_isRunning(pSecDmnGrp->dmnProcess);
}

bool wld_secDmnGrp_isMemberStarted(wld_secDmnGrp_t* pSecDmnGrp, wld_secDmn_t* pSecDmn) {
    wld_secDmnGrp_member_t* member = s_getGrpMember(pSecDmnGrp, pSecDmn);
    ASSERTI_NOT_NULL(member, false, ME, "no member found");
    return (member->state == WLD_SECDMN_STATE_START);
}


