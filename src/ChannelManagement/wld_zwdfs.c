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

#include <swl/swl_assert.h>
#include <swl/swl_returnCode.h>

#include "wld.h"
#include "wld_util.h"
#include "wld_nl80211_api.h"
#include "wld_eventing.h"
#include "wld_radio.h"
#include "wld_chanmgt.h"
#include "wld_zwdfs.h"

#define ME "wldZwd"

static const char* s_fsmStateName[] = {"INIT", "FG_CLEARING", "BG_CLEARING", "SWITCHING", "INVALID"};

static void s_setZwdfsTgtChspec(wld_zwdfs_fsmCtx_t* pCtx, swl_chanspec_t srcChSpec) {
    ASSERT_NOT_NULL(pCtx, , ME, "NULL");
    memcpy(&pCtx->tgtChSpec, &srcChSpec, sizeof(swl_chanspec_t));
}
static wld_zwdfs_fsmState_e s_handleFsmStartEvent(const wld_zwdfs_fsmCtx_t* pCtx) {
    T_Radio* pRad = pCtx->pRad;
    ASSERT_NOT_NULL(pRad, ZWDFS_FSM_STATE_INIT, ME, "NULL");
    swl_rc_ne rc = SWL_RC_OK;
    bool isBgDfsEnabled = (pRad->bgdfs_config.status != BGDFS_STATUS_OFF);
    s_setZwdfsTgtChspec((wld_zwdfs_fsmCtx_t*) pCtx, pRad->targetChanspec.chanspec);
    if(pRad->detailedState == CM_RAD_FG_CAC) {
        /* next state */
        return ZWDFS_FSM_STATE_FG_CLEARING;
    }
    if(!wld_rad_is_5ghz(pRad) ||
       !wld_channel_is_band_passive(pRad->targetChanspec.chanspec) ||
       !isBgDfsEnabled) {
        SAH_TRACEZ_INFO(ME, "%s: ZW_DFS direct switch to %u", pRad->Name, pRad->targetChanspec.chanspec.channel);
        SWL_CALL(pRad->pFA->mfn_wrad_setChanspec, pRad, pCtx->direct);
        /* next state */
        return ZWDFS_FSM_STATE_SWITCHING;
    }
    /* start clearing target channels */
    SWL_CALL(pRad->pFA->mfn_wrad_bgdfs_stop, pRad);
    wld_startBgdfsArgs_t args = {
        .channel = pRad->targetChanspec.chanspec.channel,
        .bandwidth = pRad->targetChanspec.chanspec.bandwidth,
    };
    if((wld_chanmgt_getCurChannel(pRad) == wld_chanmgt_getTgtChannel(pRad)) &&
       (wld_chanmgt_getCurBw(pRad) < wld_chanmgt_getTgtBw(pRad))) {
        args.channel = swl_channel_getComplementaryBaseChannel(&pRad->targetChanspec.chanspec);
        args.bandwidth = wld_chanmgt_getCurBw(pRad);
    }
    SAH_TRACEZ_INFO(ME, "%s: ZW_DFS OFFLOAD CAC start", pRad->Name);
    rc = pRad->pFA->mfn_wrad_bgdfs_start_ext(pRad, &args);
    if(rc < SWL_RC_OK) {
        SAH_TRACEZ_INFO(ME, "%s: ZW_DFS error starting OFFLOAD CAC", pRad->Name);
        SWL_CALL(pRad->pFA->mfn_wrad_setChanspec, pRad, pCtx->direct);
        /* next state */
        return ZWDFS_FSM_STATE_SWITCHING;
    }
    /* next state */
    return ZWDFS_FSM_STATE_BG_CLEARING;
}

static wld_zwdfs_fsmState_e s_handleFsmClearDoneEvent(const wld_zwdfs_fsmCtx_t* pCtx) {
    T_Radio* pRad = pCtx->pRad;
    ASSERT_NOT_NULL(pRad, ZWDFS_FSM_STATE_INIT, ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: ZW_DFS switch to %u", pRad->Name, pRad->targetChanspec.chanspec.channel);
    SWL_CALL(pRad->pFA->mfn_wrad_setChanspec, pRad, true);
    /* next state */
    return ZWDFS_FSM_STATE_SWITCHING;
}

static wld_zwdfs_fsmState_e s_handleFsmSwitchedEvent(const wld_zwdfs_fsmCtx_t* pCtx) {
    T_Radio* pRad = pCtx->pRad;
    ASSERT_NOT_NULL(pRad, ZWDFS_FSM_STATE_INIT, ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: ZW_DFS switch end", pRad->Name);
    s_setZwdfsTgtChspec((wld_zwdfs_fsmCtx_t*) pCtx, ((swl_chanspec_t) SWL_CHANSPEC_EMPTY));
    if(pCtx->timer) {
        amxp_timer_stop(pCtx->timer);
    }
    /* next state */
    return ZWDFS_FSM_STATE_INIT;
}

static wld_zwdfs_fsmState_e s_handleFsmStopEvent(const wld_zwdfs_fsmCtx_t* pCtx) {
    T_Radio* pRad = pCtx->pRad;
    ASSERT_NOT_NULL(pRad, ZWDFS_FSM_STATE_INIT, ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: ZW_DFS stopped", pRad->Name);
    s_setZwdfsTgtChspec((wld_zwdfs_fsmCtx_t*) pCtx, ((swl_chanspec_t) SWL_CHANSPEC_EMPTY));
    SWL_CALL(pRad->pFA->mfn_wrad_bgdfs_stop, pRad);
    if(pCtx->timer) {
        amxp_timer_stop(pCtx->timer);
    }
    /* next state */
    return ZWDFS_FSM_STATE_INIT;
}

static wld_zwdfs_fsmAction_t s_zwdfs_fsmActions[] = {
    {ZWDFS_FSM_STATE_INIT, ZWDFS_FSM_EVENT_START, s_handleFsmStartEvent},
    /* ZWDFS_FSM_STATE_FG_CLEARING */
    {ZWDFS_FSM_STATE_FG_CLEARING, ZWDFS_FSM_EVENT_CLEAR_DONE, s_handleFsmStartEvent},
    {ZWDFS_FSM_STATE_FG_CLEARING, ZWDFS_FSM_EVENT_RADAR, s_handleFsmStartEvent},
    {ZWDFS_FSM_STATE_FG_CLEARING, ZWDFS_FSM_EVENT_STOP, s_handleFsmStopEvent},
    /* ZWDFS_FSM_STATE_BG_CLEARING */
    {ZWDFS_FSM_STATE_BG_CLEARING, ZWDFS_FSM_EVENT_CLEAR_DONE, s_handleFsmClearDoneEvent},
    {ZWDFS_FSM_STATE_BG_CLEARING, ZWDFS_FSM_EVENT_RADAR, s_handleFsmStopEvent},
    {ZWDFS_FSM_STATE_BG_CLEARING, ZWDFS_FSM_EVENT_START, s_handleFsmStartEvent},
    {ZWDFS_FSM_STATE_BG_CLEARING, ZWDFS_FSM_EVENT_STOP, s_handleFsmStopEvent},
    {ZWDFS_FSM_STATE_BG_CLEARING, ZWDFS_FSM_EVENT_END, s_handleFsmSwitchedEvent},
    /* ZWDFS_FSM_STATE_SWITCHING */
    {ZWDFS_FSM_STATE_SWITCHING, ZWDFS_FSM_EVENT_END, s_handleFsmSwitchedEvent},
    {ZWDFS_FSM_STATE_SWITCHING, ZWDFS_FSM_EVENT_START, s_handleFsmStartEvent},
    {ZWDFS_FSM_STATE_SWITCHING, ZWDFS_FSM_EVENT_STOP, s_handleFsmStopEvent},
};

swl_rc_ne s_execFsm(wld_zwdfs_fsm_t* fsm, wld_zwsdf_fsmEvent_e event, const wld_zwdfs_fsmCtx_t* pCtx) {
    for(uint32_t i = 0; i < SWL_ARRAY_SIZE(s_zwdfs_fsmActions); i++) {
        if((s_zwdfs_fsmActions[i].event == event) &&
           (fsm->curState == s_zwdfs_fsmActions[i].state) &&
           (s_zwdfs_fsmActions[i].fsm_actionCb != NULL)) {
            fsm->previousState = fsm->curState;
            fsm->curState = s_zwdfs_fsmActions[i].fsm_actionCb(pCtx);
            SAH_TRACEZ_INFO(ME, "%s: ZW_DFS FSM [%s]->[%s]",
                            pCtx->pRad->Name,
                            s_fsmStateName[fsm->previousState],
                            s_fsmStateName[fsm->curState]);
            break;
        }
    }
    return SWL_RC_OK;
}

static void s_radioStatusChange(wld_radio_status_change_event_t* event) {
    ASSERT_NOT_NULL(event, , ME, "NULL");
    T_Radio* pRad = event->radio;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    wld_zwdfs_fsm_t* fsm = (wld_zwdfs_fsm_t*) pRad->zwdfsData;
    ASSERTS_NOT_NULL(fsm, , ME, "NULL");
    chanmgt_rad_state oldState = event->oldDetailedState;
    bool isCac = (oldState == CM_RAD_FG_CAC) ||
        (oldState == CM_RAD_BG_CAC) ||
        (oldState == CM_RAD_BG_CAC_EXT) ||
        (oldState == CM_RAD_BG_CAC_NS) ||
        (oldState == CM_RAD_BG_CAC_EXT_NS);
    SAH_TRACEZ_INFO(ME, "%s: handle state event (%d -> %d)", pRad->Name, oldState, pRad->detailedState);
    bool isSameTgtChspec = swl_typeChanspecExt_equals(pRad->targetChanspec.chanspec, fsm->fsmCtx.tgtChSpec);
    SAH_TRACEZ_INFO(ME, "%s: isCac:%d, isSameTgtChspec:%d (%s - %s)",
                    pRad->Name, isCac, isSameTgtChspec,
                    swl_typeChanspecExt_toBuf32(pRad->targetChanspec.chanspec).buf,
                    swl_typeChanspecExt_toBuf32(fsm->fsmCtx.tgtChSpec).buf);
    switch(pRad->detailedState) {
    case CM_RAD_DOWN:
        s_execFsm(fsm, isCac ? ZWDFS_FSM_EVENT_RADAR : ZWDFS_FSM_EVENT_END, &fsm->fsmCtx);
        break;
    case CM_RAD_UP:
        s_execFsm(fsm, (isCac && isSameTgtChspec) ? ZWDFS_FSM_EVENT_CLEAR_DONE : ZWDFS_FSM_EVENT_END, &fsm->fsmCtx);
        break;
    case CM_RAD_FG_CAC:
    case CM_RAD_BG_CAC:
    case CM_RAD_BG_CAC_EXT:
    case CM_RAD_BG_CAC_NS:
    case CM_RAD_BG_CAC_EXT_NS:
        break;
    case CM_RAD_CONFIGURING:
    case CM_RAD_DEEP_POWER_DOWN:
    case CM_RAD_DELAY_AP_UP:
    case CM_RAD_ERROR:
        s_execFsm(fsm, ZWDFS_FSM_EVENT_END, &fsm->fsmCtx);
        break;
    case CM_RAD_UNKNOWN:
    default:
        SAH_TRACEZ_ERROR(ME, "%s: unknown radio status %d", pRad->Name, pRad->detailedState);
        break;
    }
}

static void s_radioChange(wld_rad_changeEvent_t* event) {
    ASSERT_NOT_NULL(event, , ME, "NULL");
    T_Radio* pRad = event->pRad;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    wld_zwdfs_fsm_t* fsm = (wld_zwdfs_fsm_t*) pRad->zwdfsData;
    ASSERTS_NOT_NULL(fsm, , ME, "NULL");
    if(event->changeType == WLD_RAD_CHANGE_CHANSPEC) {
        s_execFsm(fsm, ZWDFS_FSM_EVENT_END, &fsm->fsmCtx);
    }
}

static void s_zwdfsTimeout(amxp_timer_t* timer _UNUSED, void* userdata _UNUSED) {
    T_Radio* pRad = (T_Radio*) userdata;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    wld_zwdfs_fsm_t* fsm = (wld_zwdfs_fsm_t*) pRad->zwdfsData;
    ASSERTS_NOT_NULL(fsm, , ME, "%s: invalid fsm", pRad->Name);
    SAH_TRACEZ_INFO(ME, "%s: ZeroWait DFS timeout, stopping", pRad->Name);
    s_execFsm(fsm, ZWDFS_FSM_EVENT_STOP, &fsm->fsmCtx);
}

swl_rc_ne wld_zwdfs_start(T_Radio* pRad, bool direct) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_zwdfs_fsm_t* fsm = (wld_zwdfs_fsm_t*) pRad->zwdfsData;
    ASSERTI_NOT_NULL(fsm, SWL_RC_ERROR, ME, "%s: invalid fsm", pRad->Name);
    amxp_timer_state_t timerState = amxp_timer_get_state(fsm->fsmCtx.timer);
    if((timerState == amxp_timer_running) || (timerState == amxp_timer_started)) {
        return SWL_RC_ERROR;
    }
    fsm->fsmCtx.pRad = pRad;
    fsm->fsmCtx.direct = direct;
    amxp_timer_delete(&fsm->fsmCtx.timer);
    int ret = amxp_timer_new(&fsm->fsmCtx.timer, s_zwdfsTimeout, pRad);
    ASSERT_FALSE(ret != 0, SWL_RC_ERROR, ME, "%s: error timer", pRad->Name);
    int timeout = WLD_ZWDFS_REQ_CHANGE_CS_TIMEOUT;
    timeout += wld_channel_get_band_clear_time(pRad->targetChanspec.chanspec);
    amxp_timer_start(fsm->fsmCtx.timer, timeout);
    SAH_TRACEZ_INFO(ME, "%s: start ZWDFS on %s timeout=%d", pRad->Name,
                    swl_typeChanspec_toBuf32(pRad->targetChanspec.chanspec).buf, timeout);
    return s_execFsm(fsm, ZWDFS_FSM_EVENT_START, &fsm->fsmCtx);
}

swl_rc_ne wld_zwdfs_stop(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_zwdfs_fsm_t* fsm = (wld_zwdfs_fsm_t*) pRad->zwdfsData;
    ASSERTS_NOT_NULL(fsm, SWL_RC_ERROR, ME, "%s: invalid fsm", pRad->Name);
    return s_execFsm(fsm, ZWDFS_FSM_EVENT_STOP, &fsm->fsmCtx);
}

static wld_event_callback_t s_onRadioStatusChange = {
    .callback = (wld_event_callback_fun) s_radioStatusChange,
};

static wld_event_callback_t s_onRadioChange = {
    .callback = (wld_event_callback_fun) s_radioChange,
};


swl_rc_ne wld_zwdfs_init(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_zwdfs_fsm_t* fsm = calloc(1, sizeof(wld_zwdfs_fsm_t));
    ASSERTI_NOT_NULL(fsm, SWL_RC_ERROR, ME, "%s: invalid fsm", pRad->Name);
    fsm->curState = ZWDFS_FSM_STATE_INIT;
    pRad->zwdfsData = fsm;
    wld_event_add_callback(gWld_queue_rad_onStatusChange, &s_onRadioStatusChange);
    wld_event_add_callback(gWld_queue_rad_onChangeEvent, &s_onRadioChange);
    return SWL_RC_OK;
}

swl_rc_ne wld_zwdfs_deinit(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_zwdfs_fsm_t* fsm = (wld_zwdfs_fsm_t*) pRad->zwdfsData;
    ASSERTS_NOT_NULL(fsm, SWL_RC_ERROR, ME, "%s: invalid fsm", pRad->Name);
    wld_event_remove_callback(gWld_queue_rad_onChangeEvent, &s_onRadioChange);
    wld_event_remove_callback(gWld_queue_rad_onStatusChange, &s_onRadioStatusChange);
    amxp_timer_delete(&fsm->fsmCtx.timer);
    W_SWL_FREE(fsm);
    return SWL_RC_OK;
}
