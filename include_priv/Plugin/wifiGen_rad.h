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

#ifndef INCLUDE_PRIV_PLUGIN_WIFIGEN_RAD_H_
#define INCLUDE_PRIV_PLUGIN_WIFIGEN_RAD_H_

#include "wld/wld.h"

/* FTA APIs */
int wifiGen_rad_createHook(T_Radio* pRad);
void wifiGen_rad_destroyHook(T_Radio* pRad);
int wifiGen_rad_poschans(T_Radio* pRad, uint8_t* buf, int bufsize);
int wifiGen_rad_supports(T_Radio* pRad, char* buf, int bufsize);
int wifiGen_rad_miscHasSupport(T_Radio* rad, T_AccessPoint* vap, char* buf, int bufsize);
int wifiGen_rad_addVapExt(T_Radio* pRad, T_AccessPoint* pAP);
int wifiGen_rad_delvapif(T_Radio* pRad, char* vapName);
int wifiGen_rad_addvapif(T_Radio* pRad, char* ifname, int ifnameSize);
int wifiGen_rad_addEndpointIf(T_Radio* pRad, char* buf, int bufsize);
swl_rc_ne wifiGen_rad_delendpointif(T_Radio* pRad, char* endpoint);
swl_rc_ne wifiGen_rad_generateVapIfName(T_Radio* pRad, uint32_t ifaceShift, char* ifName, size_t ifNameSize);
swl_rc_ne wifiGen_rad_generateEpIfName(T_Radio* pRad, uint32_t ifaceShift, char* ifName, size_t ifNameSize);
int wifiGen_rad_status(T_Radio* pRad);
int wifiGen_rad_enable(T_Radio* rad, int val, int set);
int wifiGen_rad_sync(T_Radio* pRad, int set);
int wifiGen_rad_restart(T_Radio* pRad, int set);
int wifiGen_rad_refresh(T_Radio* pRad, int set);
int wifiGen_rad_toggle(T_Radio* pRad, int set);
swl_rc_ne wifiGen_rad_regDomain(T_Radio* pRad, char* val, int bufsize, int set);
int wifiGen_rad_txpow(T_Radio* pRad, int val, int set);
swl_rc_ne wifiGen_rad_getTxPowerdBm(T_Radio* rad, int32_t* dbm);
swl_rc_ne wifiGen_rad_getMaxTxPowerdBm(T_Radio* pRad, uint16_t channel, int32_t* dbm);
swl_rc_ne wifiGen_rad_setChanspec(T_Radio* pRad, bool direct);
swl_rc_ne wifiGen_rad_getChanspec(T_Radio* pRad, swl_chanspec_t* pChSpec);
int wifiGen_rad_antennactrl(T_Radio* pRad, int val, int set);
int wifiGen_rad_staMode(T_Radio* pRad, int val, int set);
int wifiGen_rad_supstd(T_Radio* pRad, swl_radioStandard_m radioStandards);
swl_rc_ne wifiGen_rad_stats(T_Radio* pRad);
int wifiGen_rad_delayedCommitUpdate(T_Radio* pRad);
swl_rc_ne wifiGen_rad_getScanResults(T_Radio* rad, wld_scanResults_t* results);
swl_rc_ne wifiGen_rad_getAirStats(T_Radio* pRad, wld_airStats_t* pStats);
swl_rc_ne wifiGen_rad_getSpectrumInfo(T_Radio* rad, bool update _UNUSED, amxc_llist_t* llSpectrumChannelInfo);
swl_rc_ne wifiGen_rad_bgDfsStartExt(T_Radio* pRad, wld_startBgdfsArgs_t* args);
swl_rc_ne wifiGen_rad_bgDfsStop(T_Radio* pRad);
swl_rc_ne wifiGen_rad_fsmReset(T_Radio* pRad);

/* Internal APIs */
void wifiGen_rad_initBands(T_Radio* pRad);
swl_rc_ne wifiGen_rad_addVap(T_Radio* pRad, T_AccessPoint* pAP);

#endif /* INCLUDE_PRIV_PLUGIN_WIFIGEN_RAD_H_ */
