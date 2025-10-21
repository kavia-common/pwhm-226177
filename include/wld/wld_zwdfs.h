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

#ifndef _WLD_ZWDFS_H_
#define _WLD_ZWDFS_H_


#include <swl/swl_returnCode.h>

#include "wld.h"
#include "wld_radio.h"

/* timeout in msec for the setChanspec, in addition of clear time */
#define WLD_ZWDFS_REQ_CHANGE_CS_TIMEOUT 10000

typedef enum {
    ZWDFS_FSM_STATE_INIT,
    ZWDFS_FSM_STATE_FG_CLEARING,
    ZWDFS_FSM_STATE_BG_CLEARING,
    ZWDFS_FSM_STATE_SWITCHING,
    ZWDFS_FSM_STATE_MAX,
} wld_zwdfs_fsmState_e;

typedef enum {
    ZWDFS_FSM_EVENT_START,
    ZWDFS_FSM_EVENT_CLEAR_DONE,
    ZWDFS_FSM_EVENT_RADAR,
    ZWDFS_FSM_EVENT_END,
    ZWDFS_FSM_EVENT_STOP,
    ZWDFS_FSM_EVENT_MAX,
} wld_zwsdf_fsmEvent_e;

typedef struct {
    T_Radio* pRad;
    bool direct;
    amxp_timer_t* timer;
    swl_chanspec_t tgtChSpec;
    uint32_t estimatedCacTime;
} wld_zwdfs_fsmCtx_t;

typedef struct {
    wld_zwdfs_fsmState_e state;
    wld_zwsdf_fsmEvent_e event;
    wld_zwdfs_fsmState_e (* fsm_actionCb)(const wld_zwdfs_fsmCtx_t* pCtx);
} wld_zwdfs_fsmAction_t;

typedef struct {
    wld_zwdfs_fsmState_e curState;
    wld_zwdfs_fsmState_e previousState;
    wld_zwdfs_fsmCtx_t fsmCtx;
} wld_zwdfs_fsm_t;

swl_rc_ne wld_zwdfs_init(T_Radio* pRad);
swl_rc_ne wld_zwdfs_deinit(T_Radio* pRad);
swl_rc_ne wld_zwdfs_start(T_Radio* pRad, bool direct);
swl_rc_ne wld_zwdfs_stop(T_Radio* pRad);

#endif /* _WLD_ZWDFS_H_ */
