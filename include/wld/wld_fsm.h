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
#ifndef SRC_INCLUDE_WLD_WLD_FSM_H_
#define SRC_INCLUDE_WLD_WLD_FSM_H_

#define WLD_FSM_MAX_WAIT 15
#define WLD_FSM_WAIT_TIME 1000

#include "wld_types.h"

#define STR_NAME(action) #action
#define FSM_ACTION(action) .index = action, .name = STR_NAME(action)

typedef struct {
    uint32_t index;
    char* name;
    bool (* doRadFsmAction)(T_Radio* pRad);
    bool (* doVapFsmAction)(T_AccessPoint* pAP, T_Radio* pRad);
    bool (* doEpFsmAction)(T_EndPoint* pEP, T_Radio* pRad);
} wld_fsmMngr_action_t;

typedef struct {
    bool (* doLock)(T_Radio* pRad);     // call when trying to get lock. Shall only work if doUnlock is also not NULL
    void (* doUnlock)(T_Radio* pRad);   // call when freeing lock. Shall only work if doUnlock is also not NULL
    void (* ensureLock)(T_Radio* pRad); // check if current radio has lock.
    void (* doRestart)(T_Radio* pRad);  // Called on lock, allows update of FSM_ReqState
    void (* doRadFsmRun)(T_Radio* pRad);
    void (* doEpFsmRun)(T_EndPoint* pEP, T_Radio* pRad);
    void (* doVapFsmRun)(T_AccessPoint* pAP, T_Radio* pRad);
    void (* doCompendCheck)(T_Radio* pRad, bool last);
    void (* doFinish)(T_Radio* pRad);           // Called on lock, allows update of FSM_ReqState
    void (* checkPreDependency)(T_Radio* pRad); // Pre dependecy check for radio
    void (* checkVapDependency)(T_AccessPoint* pAP, T_Radio* pRad);
    void (* checkEpDependency)(T_EndPoint* pEP, T_Radio* pRad);
    void (* checkRadDependency)(T_Radio* pRad); // Post dependency check for radio
    bool (* waitGlobalSync)(T_Radio* pRad);     // Perform a global sync step across all radios
    wld_fsmMngr_action_t* actionList;           //list of actions
    uint32_t nrFsmBits;
} wld_fsmMngr_t;

FSM_STATE wld_rad_fsm(T_Radio* rad);
swl_rc_ne wld_rad_fsm_reset(T_Radio* rad);
void wld_rad_fsm_cleanFsmBits(T_Radio* rad);
void wld_rad_fsm_cleanAcFsmBits(T_Radio* rad);
int wld_rad_fsm_clearFsmBitForAll(T_Radio* rad, int bitNr);
bool wld_rad_fsm_doesExternalLocking(T_Radio* pRad);

void wld_rad_fsm_redoDependency(T_Radio* rad);
bool wld_rad_fsm_tryGetLock(T_Radio* rad);
void wld_rad_fsm_freeLock(T_Radio* rad);
void wld_rad_fsm_ensureLock(T_Radio* rad);

void wld_fsm_init(vendor_t* vendor, wld_fsmMngr_t* fsmMngr);
uint32_t wld_fsm_getNrNotIdle();

#endif /* SRC_INCLUDE_WLD_WLD_FSM_H_ */
