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

#ifndef INCLUDE_WLD_WLD_SECDMNGRP_H_
#define INCLUDE_WLD_WLD_SECDMNGRP_H_

#include "wld_secDmn_types.h"
#include "wld_secDmn.h"
#include "wld_wpaCtrl_types.h"

/*
 * @brief handler to build dynamically arguments before starting daemon
 * @param pSecDmnGrp pointer to group context
 * @param userData user data provided at initialization
 * @param pProc process to be started
 * @return newly allocate string filled with new start args (freed by the API)
 * If returned NULL, then previous start args will be using
 */
typedef char* (* wld_secDmnGrp_getArgsHandler)(wld_secDmnGrp_t* pSecDmnGrp, void* userData, const wld_process_t* pProc);

/*
 * @brief handler to check whether a member is startable (ie has starting conditions)
 * @param pSecDmnGrp pointer to group context
 * @param userData user data provided at initialization
 * @param pMember group member (pointer to sec deamon context)
 * @return bool member starting conditions status (true when startable)
 */
typedef bool (* wld_secDmnGrp_isMemberStartable)(wld_secDmnGrp_t* pSecDmnGrp, void* userData, wld_secDmn_t* pMember);

/*
 * @brief generic handler to check whether a member is pending for runtime action
 * @param pSecDmnGrp pointer to group context
 * @param userData user data provided at initialization
 * @param pMember group member (pointer to sec deamon context)
 * @return bool member reload readiness (true when ready to be reloaded)
 */
typedef bool (* wld_secDmnGrp_hasMemberRtmAction)(wld_secDmnGrp_t* pSecDmnGrp, void* userData, wld_secDmn_t* pMember);

typedef struct {
    wld_secDmnGrp_getArgsHandler getArgsCb;               /* handler to build dynamically arguments before starting daemon */
    wld_secDmnGrp_isMemberStartable isMemberStartableCb;  /* handler to pre-check starting conditions of group member */
    wld_secDmnGrp_hasMemberRtmAction hasSchedRestartCb;   /* handler to check whether member is pending for restart. */
} wld_secDmnGrp_EvtHandlers_t;

swl_rc_ne wld_secDmnGrp_init(wld_secDmnGrp_t** ppSecDmnGrp, char* cmd, char* startArgs, const char* groupName);
swl_rc_ne wld_secDmnGrp_cleanup(wld_secDmnGrp_t** ppSecDmn);
swl_rc_ne wld_secDmnGrp_setEvtHandlers(wld_secDmnGrp_t* pSecDmnGrp, wld_secDmnGrp_EvtHandlers_t* pHandlers, void* userData);
bool wld_secDmnGrp_isEnabled(wld_secDmnGrp_t* pSecDmnGrp);
bool wld_secDmnGrp_isRunning(wld_secDmnGrp_t* pSecDmnGrp);
wld_process_t* wld_secDmnGrp_getProc(wld_secDmnGrp_t* pSecDmnGrp);
wld_wpaCtrlGSock_t* wld_secDmnGrp_getGlSk(wld_secDmnGrp_t* pSecDmnGrp);

/*
 * In order to add/del members to group, secDmn APIs must be used
 */

uint32_t wld_secDmnGrp_getMembersCount(wld_secDmnGrp_t* pSecDmnGrp);
uint32_t wld_secDmnGrp_getActiveMembersCount(wld_secDmnGrp_t* pSecDmnGrp);
const wld_secDmn_t* wld_secDmnGrp_getMemberByPos(wld_secDmnGrp_t* pSecDmnGrp, int32_t pos);
const wld_secDmn_t* wld_secDmnGrp_getMemberByName(wld_secDmnGrp_t* pSecDmnGrp, const char* name);
bool wld_secDmnGrp_hasMember(wld_secDmnGrp_t* pSecDmnGrp, wld_secDmn_t* pSecDmn);
swl_rc_ne wld_secDmnGrp_setMemberStartable(wld_secDmnGrp_t* pSecDmnGrp, wld_secDmn_t* pSecDmn, bool isStartable);
swl_rc_ne wld_secDmnGrp_dropMembers(wld_secDmnGrp_t* pSecDmnGrp);

#endif /* INCLUDE_WLD_WLD_SECDMNGRP_H_ */
