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

#ifndef __WLD_ENDPOINT_H__
#define __WLD_ENDPOINT_H__
#include "wld.h"
#include "wld_wps.h"

void syncData_EndPoint2OBJ(T_EndPoint* endpoint);
bool syncData_OBJ2EndPoint(amxd_object_t* object);

T_EndPoint* wld_ep_fromObj(amxd_object_t* epObj);
void wld_endpoint_setProfile_ocf(void* priv, amxd_object_t* object, const amxc_var_t* const newParamValues);
void wld_endpoint_setCurrentProfile(amxd_object_t* endpointObject, T_EndPointProfile* Profile);
void wld_endpoint_setProfileSecurity_ocf(void* priv, amxd_object_t* object, const amxc_var_t* const newParamValues);

bool wld_endpoint_isEnabledWithRef(T_EndPoint* pEP);
bool wld_endpoint_hasStackEnabled(T_EndPoint* pEP);
swl_rc_ne wld_endpoint_applyEnable(T_EndPoint* pEP, bool combEnable, bool enable);
bool wld_endpoint_isReady(T_EndPoint* pEP);
bool wld_endpoint_updateStats(amxd_object_t* obj, T_EndPointStats* stats);
void wld_endpoint_resetStats(T_EndPoint* pEP);
bool wld_endpoint_validate_profile(const T_EndPointProfile* Profile);
void wld_endpoint_setConnectionStatus(T_EndPoint* pEP, wld_epConnectionStatus_e connectionStatus, wld_epError_e error);
void wld_endpoint_sync_connection(T_EndPoint* pEP, bool connected, wld_epError_e error);
wld_epConnectionStatus_e wld_endpoint_connStatusFromEpError(wld_epError_e error);
void wld_endpoint_sendPairingNotification(T_EndPoint* pEP, uint32_t type, const char* reason, T_WPSCredentials* credentials);

amxd_status_t _EndPoint_WPS_pushButton(amxd_object_t* obj,
                                       amxd_function_t* func,
                                       amxc_var_t* args,
                                       amxc_var_t* ret);
amxd_status_t _EndPoint_WPS_cancelPairing(amxd_object_t* obj,
                                          amxd_function_t* func,
                                          amxc_var_t* args,
                                          amxc_var_t* ret);
amxd_status_t _wld_endpoint_getStats_orf(amxd_object_t* const object,
                                         amxd_param_t* const param,
                                         amxd_action_t reason,
                                         const amxc_var_t* const args,
                                         amxc_var_t* const action_retval,
                                         void* priv);
void wld_endpoint_create_reconnect_timer(T_EndPoint* pEP);
void wld_endpoint_reconfigure(T_EndPoint* pEP);
void wld_endpoint_performConnectCommit(T_EndPoint* pEP, bool alwaysCommit);
swl_rc_ne wld_endpoint_getBssidBin(T_EndPoint* pEP, swl_macBin_t* tgtMac);

swl_rc_ne wld_endpoint_checkConnection(T_EndPoint* pEP);
void wld_endpoint_writeStats(T_EndPoint* pEP, T_EndPointStats* stats, bool success);
bool wld_endpoint_getTargetBssid(T_EndPoint* pEP, swl_macBin_t* macBuffer);
bool wld_endpoint_isMloRequired(T_EndPoint* pEP);
void wld_endpoint_setNetdevIndex(T_EndPoint* pEP, int32_t netDevIndex);

T_EndPoint* wld_endpoint_fromIt(amxc_llist_it_t* it);

T_EndPoint* wld_endpoint_create(T_Radio* pRad, const char* epName, amxd_object_t* object);
void wld_endpoint_destroy(T_EndPoint* pEP);

/*
 * @brief finalize endpoint, by :
 * - mapping/unmapping endpoint instance with relative ssid instance
 * - creating/deleting the endpoint interface, when needed.
 * If interface and already set, and required for creation, then it will be re-created
 */
swl_rc_ne wld_endpoint_finalize(T_EndPoint* pEP);

/*
 * check conditions to create endpoint interface
 */
bool wld_endpoint_needCreateIface(T_EndPoint* pEP);

/*
 * check conditions to delete endpoint interface
 */
bool wld_endpoint_needDestroyIface(T_EndPoint* pEP);

#endif /* __WLD_ENDPOINT_H__ */
