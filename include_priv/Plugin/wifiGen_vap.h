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

#ifndef INCLUDE_PRIV_PLUGIN_WIFIGEN_VAP_H_
#define INCLUDE_PRIV_PLUGIN_WIFIGEN_VAP_H_

#include "wld/wld.h"

int wifiGen_vap_createHook(T_AccessPoint* pAP);
void wifiGen_vap_destroyHook(T_AccessPoint* pAP);
int wifiGen_vap_ssid(T_AccessPoint* pAP, char* buf, int bufsize, int set);
int wifiGen_vap_status(T_AccessPoint* pAP);
int wifiGen_vap_enable(T_AccessPoint* pAP, int enable, int set);
int wifiGen_vap_sync(T_AccessPoint* pAP, int set);
swl_rc_ne wifiGen_vap_getStationStats(T_AccessPoint* pAP);
swl_rc_ne wifiGen_vap_getSingleStationStats(T_AssociatedDevice* pAD);
swl_rc_ne wifiGen_vap_updateRssiStats(T_AccessPoint* pAP);
int wifiGen_vap_bssid(T_Radio* pRad, T_AccessPoint* pAP, unsigned char* buf, int bufsize, int set);
int wifiGen_vap_setBssid(T_AccessPoint* pAP);
int wifiGen_vap_sec_sync(T_AccessPoint* pAP, int set);
int wifiGen_vap_multiap_update_type(T_AccessPoint* pAP);
int wifiGen_vap_multiap_update_profile(T_AccessPoint* pAP);
int wifiGen_vap_multiap_update_vlanid(T_AccessPoint* pAP);
swl_rc_ne wifiGen_vap_setMboDisallowReason(T_AccessPoint* pAP);
swl_rc_ne wifiGen_vap_sta_transfer(T_AccessPoint* vap, wld_transferStaArgs_t* params);
int wifiGen_vap_mf_sync(T_AccessPoint* vap, int set);
int wifiGen_vap_wps_sync(T_AccessPoint* pAP, char* val, int bufsize, int set);
int wifiGen_vap_wps_enable(T_AccessPoint* pAP, int enable, int set);
int wifiGen_vap_wps_labelPin(T_AccessPoint* pAP, int set);
int wifiGen_vap_kick_sta(T_AccessPoint* pAP, char* buf, int bufsize, int set);
swl_rc_ne wifiGen_vap_disassoc_sta_reason(T_AccessPoint* pAP, swl_macBin_t* staMac, int reason);
int wifiGen_vap_kick_sta_reason(T_AccessPoint* pAP, char* buf, int bufsize, int reason);
swl_rc_ne wifiGen_vap_clean_sta(T_AccessPoint* pAP, char* buf, int bufsize);
int wifiGen_vap_updateApStats(T_AccessPoint* vap);
swl_rc_ne wifiGen_vap_requestRrmReport(T_AccessPoint* pAP, const swl_macChar_t* sta, wld_rrmReq_t* req);
swl_rc_ne wifiGen_vap_deleted_neighbor(T_AccessPoint* pAP, T_ApNeighbour* pApNeighbor);
swl_rc_ne wifiGen_vap_updated_neighbor(T_AccessPoint* pAP, T_ApNeighbour* pApNeighbor);
swl_rc_ne wifiGen_vap_sendManagementFrame(T_AccessPoint* pAP, swl_80211_mgmtFrameControl_t* fc, swl_macBin_t* tgtMac, swl_bit8_t* data, size_t dataLen, swl_chanspec_t* chanspec);
swl_rc_ne wifiGen_vap_setDiscoveryMethod(T_AccessPoint* pAP);
swl_rc_ne wifiGen_vap_setMldUnit(T_AccessPoint* pAP);
swl_rc_ne wifiGen_vap_postUpActions(T_AccessPoint* pAP);
swl_rc_ne wifiGen_vap_postDownActions(T_AccessPoint* pAP);
swl_rc_ne wifiGen_vap_getMloStats(T_AccessPoint* pAP, wld_mloStats_t* pStats);
#endif /* INCLUDE_PRIV_PLUGIN_WIFIGEN_VAP_H_ */
