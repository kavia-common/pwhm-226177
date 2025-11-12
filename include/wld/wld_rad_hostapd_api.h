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

#ifndef __WLD_RAD_API_H__
#define __WLD_RAD_API_H__

#include "wld.h"
#include "wld_hostapd_ap_api.h"

wld_secDmn_action_rc_ne wld_rad_hostapd_setOperatingStandard(T_Radio* pRadio);
wld_secDmn_action_rc_ne wld_rad_hostapd_setChannel(T_Radio* pR);
wld_secDmn_action_rc_ne wld_rad_hostapd_setMiscParams(T_Radio* pR);
swl_rc_ne wld_rad_hostapd_switchChannel(T_Radio* pR);
swl_rc_ne wld_rad_hostapd_reload(T_Radio* pR);
swl_rc_ne wld_rad_hostapd_reconfigure(T_Radio* pR);
swl_rc_ne wld_rad_hostapd_enable(T_Radio* pR);
swl_rc_ne wld_rad_hostapd_disable(T_Radio* pR);
swl_trl_e wld_rad_hostapd_getCfgParamSupp(T_Radio* pRad, const char* param);
T_AccessPoint* wld_rad_hostapd_getFirstConnectedVap(T_Radio* pRad);
T_AccessPoint* wld_rad_hostapd_getCfgMainVap(T_Radio* pRad);
T_AccessPoint* wld_rad_hostapd_getRunMainVap(T_Radio* pRad);
swl_rc_ne wld_rad_hostapd_getCmdReplyParamStr(T_Radio* pRad, const char* cmd, const char* key, char* valStr, size_t valStrSize);
int32_t wld_rad_hostapd_getCmdReplyParam32Def(T_Radio* pRad, const char* cmd, const char* key, int32_t defVal);
swl_rc_ne wld_rad_hostapd_getCfgParamStr(T_Radio* pRad, const char* key, char* valStr, size_t valStrSize);
int32_t wld_rad_hostapd_getCfgParamInt32Def(T_Radio* pRad, const char* key, int32_t defVal);
swl_channel_t wld_rad_hostapd_getCfgChannel(T_Radio* pRad);
swl_chanspec_t wld_rad_hostapd_getCfgChanspec(T_Radio* pRad);
swl_rc_ne wld_rad_hostapd_updateAllVapsConfigId(T_Radio* pRad);
swl_rc_ne wld_rad_hostapd_updateMaxNumStations(T_Radio* pRad);
bool wld_rad_hostapd_hasActiveApMld(T_Radio* pRad, uint32_t minNLinks);

#endif /* __WLD_RAD_API_H__ */
