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

#ifndef INCLUDE_PRIV_PLUGIN_WIFIGEN_EP_H_
#define INCLUDE_PRIV_PLUGIN_WIFIGEN_EP_H_

#include "wld/wld.h"

swl_rc_ne wifiGen_ep_createHook(T_EndPoint* pEP);
swl_rc_ne wifiGen_ep_destroyHook(T_EndPoint* pEP);
swl_rc_ne wifiGen_ep_enable(T_EndPoint* endpoint, bool enable);
swl_rc_ne wifiGen_ep_connectAp(T_EndPointProfile* epProfile);
swl_rc_ne wifiGen_ep_disconnect(T_EndPoint* pEP);
swl_rc_ne wifiGen_ep_bssid(T_EndPoint* pEP, swl_macChar_t* bssid);
swl_rc_ne wifiGen_ep_status(T_EndPoint* pEP);
swl_rc_ne wifiGen_ep_wpsStart(T_EndPoint* pEP, wld_wps_cfgMethod_e method, char* pin, char* ssid, swl_macChar_t* bssid);
swl_rc_ne wifiGen_ep_wpsCancel(T_EndPoint* pEP);
swl_rc_ne wifiGen_ep_stats(T_EndPoint* pEP, T_EndPointStats* stats);
swl_rc_ne wifiGen_ep_multiApEnable(T_EndPoint* pEP);
swl_rc_ne wifiGen_ep_sendManagementFrame(T_EndPoint* pEP, swl_80211_mgmtFrameControl_t* fc, swl_macBin_t* tgtMac, swl_bit8_t* data, size_t dataLen, swl_chanspec_t* chanspec);
swl_rc_ne wifiGen_ep_setMacAddress(T_EndPoint* pEP);
swl_rc_ne wifiGen_ep_update(T_EndPoint* pEP, int set);
swl_rc_ne wifiGen_ep_connStatus(T_EndPoint* pEP, wld_epConnectionStatus_e* pConnState);
swl_rc_ne wifiGen_ep_getConnChspec(T_EndPoint* pEP, swl_chanspec_t* pChanSpec);
/**
 * @brief Set MLD unit configuration for endpoint
 *
 * Configures the MLD unit for the specified endpoint by
 * triggering the appropriate FSM state changes. Validates that the endpoint's
 * radio supports MLO capabilities before applying the configuration.
 *
 * @param pEP pointer to endpoint to configure MLD unit for
 *
 * @return SWL_RC_OK on success, SWL_RC_INVALID_PARAM for invalid parameters
 */
swl_rc_ne wifiGen_ep_setMldUnit(T_EndPoint* pEP);

#endif /* INCLUDE_PRIV_PLUGIN_WIFIGEN_EP_H_ */
