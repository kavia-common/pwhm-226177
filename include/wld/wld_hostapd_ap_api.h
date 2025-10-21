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

#ifndef __WLD_AP_API_H__
#define __WLD_AP_API_H__

#include <stdint.h>

#include "swl/swl_ieee802_1x_defs.h"

typedef enum {
    SECDMN_ACTION_ERROR = -1,
    SECDMN_ACTION_OK_DONE = 0,
    SECDMN_ACTION_OK_NEED_UPDATE_BEACON,
    SECDMN_ACTION_OK_NEED_RELOAD_SECKEY, // Only WPA-PSK Security key params on specific AP
    SECDMN_ACTION_OK_NEED_SIGHUP,
    SECDMN_ACTION_OK_NEED_TOGGLE,        // Like a restart, but keeps available wpactrl sockets
    SECDMN_ACTION_OK_NEED_RESTART
} wld_secDmn_action_rc_ne;

bool wld_ap_hostapd_setParamValue(T_AccessPoint* pAP, const char* field, const char* value, const char* reason);
bool wld_ap_hostapd_sendCommand(T_AccessPoint* pAP, char* cmd, const char* reason);
swl_rc_ne wld_ap_hostapd_getParamAction(wld_secDmn_action_rc_ne* pOutMappedAction, const char* paramName);
swl_rc_ne wld_ap_hostapd_setParamAction(const char* paramName, wld_secDmn_action_rc_ne inMappedAction);
bool wld_ap_hostapd_updateBeacon(T_AccessPoint* pAP, const char* reason);
bool wld_ap_hostapd_reloadSecKey(T_AccessPoint* pAP, const char* reason);
swl_rc_ne wld_ap_hostapd_setNeighbor(T_AccessPoint* pAP, T_ApNeighbour* pApNeighbor);
swl_rc_ne wld_ap_hostapd_removeNeighbor(T_AccessPoint* pAP, T_ApNeighbour* pApNeighbor);
swl_rc_ne wld_ap_hostapd_dumpNeighborList(T_AccessPoint* pAP, swl_macBin_t* list, uint32_t maxLen, uint32_t* pLen);

wld_secDmn_action_rc_ne wld_ap_hostapd_setSsid(T_AccessPoint* pAP, const char* ssid);
wld_secDmn_action_rc_ne wld_ap_hostapd_setSecretKey(T_AccessPoint* pAP);
wld_secDmn_action_rc_ne wld_ap_hostapd_setSecurityMode(T_AccessPoint* pAP);
wld_secDmn_action_rc_ne wld_ap_hostapd_setSecParams(T_AccessPoint* pAP);
wld_secDmn_action_rc_ne wld_ap_hostapd_setClientIsolation(T_AccessPoint* pAP);
wld_secDmn_action_rc_ne wld_ap_hostapd_setSSIDAdvertisement(T_AccessPoint* pAP, bool enable);
wld_secDmn_action_rc_ne wld_ap_hostapd_setMaxNbrSta(T_AccessPoint* pAP, uint32_t num);
wld_secDmn_action_rc_ne wld_ap_hostapd_updateMaxNbrSta(T_AccessPoint* pAP);
wld_secDmn_action_rc_ne wld_ap_hostapd_setNoSecParams(T_AccessPoint* pAP);
wld_secDmn_action_rc_ne wld_ap_hostapd_setMldParams(T_AccessPoint* pAP);
wld_secDmn_action_rc_ne wld_ap_hostapd_setEnableVap(T_AccessPoint* pAP, bool enable);
wld_secDmn_action_rc_ne wld_ap_hostapd_enableVap(T_AccessPoint* pAP, bool enable);

swl_rc_ne wld_ap_hostapd_kickStation(T_AccessPoint* pAP, swl_macBin_t* mac, swl_IEEE80211deauthReason_ne reason);
swl_rc_ne wld_ap_hostapd_deauthAllStations(T_AccessPoint* pAP);
swl_rc_ne wld_ap_hostapd_disassocStation(T_AccessPoint* pAP, swl_macBin_t* mac, swl_IEEE80211deauthReason_ne reason);
swl_rc_ne wld_ap_hostapd_deauthKnownStations(T_AccessPoint* pAP);
swl_rc_ne wld_ap_hostapd_transferStation(T_AccessPoint* pAP, wld_transferStaArgs_t* params);
swl_rc_ne wld_ap_hostapd_startWps(T_AccessPoint* pAP);

/*
 * @brief start a WPS client PIN session, to pair against a remote device.
 *
 * @param pAP accesspoint
 * @param pin numerical string of 4 or 8 digits (Cf. WPS 2.x)
 *            that can even start with sequence of zero digit.
 * @param timeout Time (in seconds) when the PIN will be invalidated; 0 = no timeout
 *
 * @return SWL_RC_OK in case of success
 *         SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_ap_hostapd_startWpsPin(T_AccessPoint* pAP, const char* pin, uint32_t timeout);

/*
 * @brief start a WPS AP Pin session, allowing a remote device to pair.
 *
 * @param pAP accesspoint
 * @param pin numerical string of 4 or 8 digits (Cf. WPS 2.x)
 *            that can even start with sequence of zero digit
 * @param timeout Time (in seconds) when the AP PIN will be disabled; 0 = no timeout
 *
 * @return SWL_RC_OK in case of success
 *         SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_ap_hostapd_setWpsApPin(T_AccessPoint* pAP, const char* pin, uint32_t timeout);

swl_rc_ne wld_ap_hostapd_stopWps(T_AccessPoint* pAP);
swl_rc_ne wld_ap_hostapd_delAllMacFilteringEntries(T_AccessPoint* pAP);
swl_rc_ne wld_ap_hostapd_setMacFilteringList(T_AccessPoint* pAP);
swl_rc_ne wld_ap_hostapd_addMacFilteringEntry(T_AccessPoint* pAP, char* macStr);
swl_rc_ne wld_ap_hostapd_delMacFilteringEntry(T_AccessPoint* pAP, char* macStr);
swl_rc_ne wld_ap_hostapd_getStaInfo(T_AccessPoint* pAP, T_AssociatedDevice* pAD);
swl_rc_ne wld_ap_hostapd_getAllStaInfo(T_AccessPoint* pAP);
swl_rc_ne wld_ap_hostapd_getNumMldLinks(T_AccessPoint* pAP, uint32_t* pNLinks);
swl_rc_ne wld_ap_hostapd_getMldLinkId(T_AccessPoint* pAP, int32_t* pLinkId);
swl_rc_ne wld_ap_hostapd_requestRRMReport(T_AccessPoint* pAP, const swl_macChar_t* sta, uint8_t reqMode, uint8_t operClass, swl_channel_t channel, bool addNeighbor,
                                          uint16_t randomInterval, uint16_t measurementDuration, uint8_t measurementMode, const swl_macChar_t* bssid, const char* ssid);
swl_rc_ne wld_ap_hostapd_requestRRMReport_ext(T_AccessPoint* pAP, const swl_macChar_t* sta, wld_rrmReq_t* req);

swl_trl_e wld_hostapd_ap_getCfgParamSupp(T_AccessPoint* pAP, const char* param);
swl_rc_ne wld_hostapd_ap_sendCfgParam(T_AccessPoint* pAP, const char* param, const char* value);
bool wld_hostapd_ap_needWpaCtrlIface(T_AccessPoint* pAP);
swl_rc_ne wld_ap_hostapd_getCfgInterface(T_AccessPoint* pAP, char* valStr, size_t valStrSize);

const char* wld_hostapd_ap_selectApLinkIface(T_AccessPoint* pAP);
bool wld_ap_hostapd_isMainStaMldLink(T_AccessPoint* pAP, swl_macBin_t* pMacBin);

#endif /* __WLD_AP_API_H__ */
