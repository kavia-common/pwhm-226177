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

#ifndef __WLD_SSID_H__
#define __WLD_SSID_H__

#include "wld.h"

void wld_ssid_setStatus(T_SSID* pSSID, wld_status_e status, bool commit);
void syncData_SSID2OBJ(amxd_object_t* object, T_SSID* pR, int set);

void wld_ssid_cleanAll();
swl_rc_ne wld_ssid_syncEnable(T_SSID* pSSID, bool syncToIntf);
bool wld_ssid_isSyncEnablePending(T_SSID* pSSID);
bool wld_ssid_getIntfEnable(T_SSID* pSSID);
amxd_object_t* wld_ssid_getIntfObject(T_SSID* pSSID);
bool wld_ssid_isEnabledWithRef(T_SSID* pSSID);
bool wld_ssid_hasStackEnabled(T_SSID* pSSID);
bool wld_ssid_isSSIDConfigured(T_SSID* pSSID);
void wld_ssid_generateBssid(T_Radio* pRad, T_AccessPoint* pAP, uint32_t apIndex, swl_macBin_t* macBin);
void wld_ssid_setBssid(T_SSID* pSSID, swl_macBin_t* macBin);
void wld_ssid_generateMac(T_Radio* pRad, T_SSID* pSSID, uint32_t index, swl_macBin_t* macBin);
void wld_ssid_setMac(T_SSID* pSSID, swl_macBin_t* macBin);
bool wld_ssid_hasAutoMacBssIndex(T_SSID* pSSID, int32_t* pBssIndex);
int32_t wld_rad_getHighestVapAutoMacBssIndex(T_Radio* pRad);
int16_t wld_ssid_getMLDLinkID(T_SSID* pSSID);
void wld_ssid_resetMloStats(T_SSID* pSSID);
swl_rc_ne wld_ssid_accuMloStats(T_SSID* pSSID, wld_stats_t* pDiffStats);
swl_rc_ne wld_ssid_getMloStats(T_SSID* pSSID, wld_stats_t* pOutStats);
swl_rc_ne wld_ssid_accuNetStats(T_SSID* pSSID, wld_stats_t* pDiffStats);
swl_rc_ne wld_ssid_getNetStats(T_SSID* pSSID, wld_stats_t* pOutStats);
void wld_ssid_setMLDRole(T_SSID* pSSID, swl_mlo_role_e mldRole);
void wld_ssid_setMLDLinkID(T_SSID* pSSID, int16_t mldLinkId);
void wld_ssid_setMLDStatus(T_SSID* pSSID, swl_mlo_intfMldStatus_e mldStatus);

T_SSID* wld_ssid_createApSsid(T_AccessPoint* pAP);
T_SSID* wld_ssid_fromObj(amxd_object_t* ssidObj);
T_SSID* wld_ssid_getSsidByBssid(swl_macBin_t* macBin);
T_SSID* wld_ssid_getSsidByMacAddress(swl_macBin_t* macBin);
T_SSID* wld_ssid_getSsidByIfIndex(int32_t ifIndex);
wld_ssidType_e wld_ssid_getType(T_SSID* pSSID);
const char* wld_ssid_getIfName(T_SSID* pSSID);
int32_t wld_ssid_getIfIndex(T_SSID* pSSID);

T_SSID* wld_ssid_getSsidByIfName(const char* ifName);
wld_wpaCtrlInterface_t* wld_ssid_getWpaCtrlIface(T_SSID* pSSID);

bool wld_ssid_isLinkCreated(T_SSID* pSSID);
bool wld_ssid_isLinkEnabled(T_SSID* pSSID);
bool wld_ssid_isLinkActive(T_SSID* pSSID);
int32_t wld_ssid_getLinkIfIndex(T_SSID* pSSID);
const char* wld_ssid_getLinkIfName(T_SSID* pSSID);
bool wld_ssid_hasMloSupport(T_SSID* pSSID);


void wld_ssid_dbgTriggerSync(T_SSID* pSSID);
#endif /* __WLD_SSID_H__ */
