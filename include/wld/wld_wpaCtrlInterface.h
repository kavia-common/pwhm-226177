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

#ifndef INCLUDE_WLD_WLD_WPACTRLINTERFACE_H_
#define INCLUDE_WLD_WLD_WPACTRLINTERFACE_H_

#include "wld_wpaCtrl_types.h"
#include "wld_wpaCtrl_events.h"

bool wld_wpaCtrlInterface_init(wld_wpaCtrlInterface_t** ppIface, char* interfaceName, char* serverPath);
bool wld_wpaCtrlInterface_initWithSockName(wld_wpaCtrlInterface_t** ppIface, char* interfaceName, const char* serverPath, const char* sockName);
bool wld_wpaCtrlInterface_setConnectionInfo(wld_wpaCtrlInterface_t* pIface, const char* serverPath, const char* sockName);
bool wld_wpaCtrlInterface_testConnection(const char* serverPath, const char* sockName);
bool wld_wpaCtrlInterface_setEvtHandlers(wld_wpaCtrlInterface_t* pIface, void* userdata, wld_wpaCtrl_evtHandlers_cb* pHandlers);
bool wld_wpaCtrlInterface_getEvtHandlers(wld_wpaCtrlInterface_t* pIface, void** userdata, wld_wpaCtrl_evtHandlers_cb* pHandlers);
bool wld_wpaCtrlInterface_open(wld_wpaCtrlInterface_t* pIface);
void wld_wpaCtrlInterface_close(wld_wpaCtrlInterface_t* pIface);
bool wld_wpaCtrlInterface_reset(wld_wpaCtrlInterface_t* pIface);
void wld_wpaCtrlInterface_cleanup(wld_wpaCtrlInterface_t** ppIface);
bool wld_wpaCtrlInterface_ping(const wld_wpaCtrlInterface_t* pIface);
bool wld_wpaCtrlInterface_isReady(const wld_wpaCtrlInterface_t* pIface);
const char* wld_wpaCtrlInterface_getPath(const wld_wpaCtrlInterface_t* pIface);
const char* wld_wpaCtrlInterface_getName(const wld_wpaCtrlInterface_t* pIface);
wld_wpaCtrlMngr_t* wld_wpaCtrlInterface_getMgr(const wld_wpaCtrlInterface_t* pIface);
void wld_wpaCtrlInterface_setEnable(wld_wpaCtrlInterface_t* pIface, bool enable);
bool wld_wpaCtrlInterface_isEnabled(const wld_wpaCtrlInterface_t* pIface);
const char* wld_wpaCtrlInterface_getConnectionDirPath(const wld_wpaCtrlInterface_t* pIface);
bool wld_wpaCtrlInterface_checkConnectionPath(const wld_wpaCtrlInterface_t* pIface);
const char* wld_wpaCtrlInterface_getConnectionSockName(const wld_wpaCtrlInterface_t* pIface);

#define CALL_INTF_EXT(pIntf, fName, ...) \
    { \
        void* userdata = NULL; \
        wld_wpaCtrl_evtHandlers_cb handlers = {0}; \
        if(wld_wpaCtrlInterface_getEvtHandlers(pIntf, &userdata, &handlers)) { \
            char* ifName = (char*) wld_wpaCtrlInterface_getName(pIntf); \
            SWL_CALL(handlers.fName, userdata, ifName, __VA_ARGS__); \
        } \
    }

#define CALL_INTF_NA_EXT(pIntf, fName) \
    { \
        void* userdata = NULL; \
        wld_wpaCtrl_evtHandlers_cb handlers = {0}; \
        if(wld_wpaCtrlInterface_getEvtHandlers(pIntf, &userdata, &handlers)) { \
            char* ifName = (char*) wld_wpaCtrlInterface_getName(pIntf); \
            SWL_CALL(handlers.fName, userdata, ifName); \
        } \
    }

#endif /* INCLUDE_WLD_WLD_WPACTRLINTERFACE_H_ */
