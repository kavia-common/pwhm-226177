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

#ifndef __WLD_TESTHELPER_VAP_H__
#define __WLD_TESTHELPER_VAP_H__
#include "wld_types.h"
#include "wld_th_types.h"

#include <amxc/amxc_macros.h>
#include <amxc/amxc.h>
#include <amxp/amxp.h>
#include <amxd/amxd_dm.h>
#include <amxd/amxd_object.h>
#include <amxd/amxd_path.h>
#include <amxd/amxd_object_event.h>
#include <amxo/amxo.h>
#include <amxo/amxo_save.h>
#include <amxb/amxb.h>

#include "wld/wld.h"
#include "wld/wld_accesspoint.h"

#include "wld_th_fsm.h"

typedef void (* wld_th_vap_doVapFsmAction)(T_AccessPoint* pAP, T_Radio* pRad, wld_th_fsmStates_e state);

typedef struct {
    bool errorOnStaStats;
    uint32_t nrStaInFile;
    char* staStatsFileName;
    uint32_t fsmCommits[WLD_TH_FSM_MAX];
    wld_th_vap_doVapFsmAction fsmCallback[WLD_TH_FSM_MAX];
} wld_th_vap_vendorData_t;

T_AccessPoint* wld_th_vap_createVap(amxb_bus_ctx_t* const bus_ctx, wld_th_mockVendor_t* mockVendor, T_Radio* radio, const char* name);

swl_rc_ne wld_th_vap_createHook(T_AccessPoint* pAP);
void wld_th_vap_destroyHook(T_AccessPoint* pAP);



int wld_th_vap_vendorCb_addVapIf(T_Radio* rad, char* vap, int bufsize);
int wld_th_vap_vendorCb_bssid(T_Radio* pRad, T_AccessPoint* pAP, unsigned char* buf, int bufsize, int set);
int wld_th_vap_status(T_AccessPoint* pAP);
swl_rc_ne wld_th_vap_getStationStats(T_AccessPoint* pAP);
int wld_th_vap_enable(T_AccessPoint* pAP, int enable, int set);
int wld_th_vap_ssid(T_AccessPoint* pAP, char* buf, int bufsize, int set);

//Helper functions
void wld_th_vap_doFsmClean(T_AccessPoint* pAP);
wld_th_vap_vendorData_t* wld_th_vap_getVendorData(T_AccessPoint* pAP);
void wld_th_vap_setSSIDEnable(T_AccessPoint* pAP, bool enable, bool commit);
void wld_th_vap_setApEnable(T_AccessPoint* pAP, bool enable, bool commit);

void wld_th_vap_clearCommits(T_AccessPoint* pAP);
void wld_th_vap_checkCommitAll(T_AccessPoint* pAP);
void wld_th_vap_checkCommitted(T_AccessPoint* pAP, swl_mask_m commitMask);
#endif
