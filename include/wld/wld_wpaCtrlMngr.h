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

#ifndef __WLD_WPA_CTRL_MNGR_H__
#define __WLD_WPA_CTRL_MNGR_H__

#include "wld_wpaCtrl_types.h"
#include "wld_wpaCtrlInterface.h"
#include "wld_secDmn_types.h"

bool wld_wpaCtrlMngr_init(wld_wpaCtrlMngr_t** ppMgr, struct wld_secDmn* pSecDmn);
bool wld_wpaCtrlMngr_setEvtHandlers(wld_wpaCtrlMngr_t* pMgr, void* userdata, wld_wpaCtrl_radioEvtHandlers_cb* pHandlers);
bool wld_wpaCtrlMngr_getEvtHandlers(wld_wpaCtrlMngr_t* pMgr, void** userdata, wld_wpaCtrl_radioEvtHandlers_cb* pHandlers);
bool wld_wpaCtrlMngr_setSecDmnGrp(wld_wpaCtrlMngr_t* pMgr, wld_secDmnGrp_t* pSecDmnGrp);
bool wld_wpaCtrlMngr_connect(wld_wpaCtrlMngr_t* pMgr);
bool wld_wpaCtrlMngr_disconnect(wld_wpaCtrlMngr_t* pMgr);
bool wld_wpaCtrlMngr_stopConnecting(wld_wpaCtrlMngr_t* pMgr);
bool wld_wpaCtrlMngr_isConnecting(wld_wpaCtrlMngr_t* pMgr);
bool wld_wpaCtrlMngr_isConnected(wld_wpaCtrlMngr_t* pMgr);
bool wld_wpaCtrlMngr_isRunning(const wld_wpaCtrlMngr_t* pMgr);
bool wld_wpaCtrlMngr_isReady(wld_wpaCtrlMngr_t* pMgr);
void wld_wpaCtrlMngr_cleanup(wld_wpaCtrlMngr_t** ppMgr);

bool wld_wpaCtrlMngr_registerInterface(wld_wpaCtrlMngr_t* pMgr, wld_wpaCtrlInterface_t* pIface);
bool wld_wpaCtrlMngr_unregisterInterface(wld_wpaCtrlMngr_t* pMgr, wld_wpaCtrlInterface_t* pIface);
wld_wpaCtrlInterface_t* wld_wpaCtrlMngr_getInterface(const wld_wpaCtrlMngr_t* pMgr, int32_t pos);
wld_wpaCtrlInterface_t* wld_wpaCtrlMngr_getInterfaceByName(const wld_wpaCtrlMngr_t* pMgr, const char* name);
wld_wpaCtrlInterface_t* wld_wpaCtrlMngr_getFirstInterface(const wld_wpaCtrlMngr_t* pMgr);
wld_wpaCtrlInterface_t* wld_wpaCtrlMngr_getFirstReadyInterface(const wld_wpaCtrlMngr_t* pMgr);
wld_wpaCtrlInterface_t* wld_wpaCtrlMngr_getFirstNotReadyInterface(const wld_wpaCtrlMngr_t* pMgr);
wld_wpaCtrlInterface_t* wld_wpaCtrlMngr_getFirstAvailableInterface(const wld_wpaCtrlMngr_t* pMgr);
wld_wpaCtrlInterface_t* wld_wpaCtrlMngr_getDefaultInterface(const wld_wpaCtrlMngr_t* pMgr);
const char* wld_wpaCtrlMngr_getCtrlIfaceDirPath(wld_wpaCtrlMngr_t* pMgr);
swl_rc_ne wld_wpaCtrlMngr_checkAllIfaces(wld_wpaCtrlMngr_t* pMgr);
uint32_t wld_wpaCtrlMngr_countInterfaces(const wld_wpaCtrlMngr_t* pMgr);
struct wld_secDmn* wld_wpaCtrlMngr_getSecDmn(const wld_wpaCtrlMngr_t* pMgr);
wld_secDmnGrp_t* wld_wpaCtrlMngr_getSecDmnGrp(const wld_wpaCtrlMngr_t* pMgr);
bool wld_wpaCtrlMngr_ping(const wld_wpaCtrlMngr_t* pMgr);
uint32_t wld_wpaCtrlMngr_countEnabledInterfaces(const wld_wpaCtrlMngr_t* pMgr);
uint32_t wld_wpaCtrlMngr_countReadyInterfaces(const wld_wpaCtrlMngr_t* pMgr);

#define CALL_MGR_EXT(pMgr, fName, ifName, ...) \
    { \
        void* userdata = NULL; \
        wld_wpaCtrl_radioEvtHandlers_cb handlers = {0}; \
        if(wld_wpaCtrlMngr_getEvtHandlers(pMgr, &userdata, &handlers)) { \
            SWL_CALL(handlers.fName, userdata, ifName, __VA_ARGS__); \
        } \
    }

#define CALL_MGR_I_EXT(pIntf, fName, ...) \
    if(pIntf != NULL) { \
        CALL_MGR_EXT(wld_wpaCtrlInterface_getMgr(pIntf), fName, (char*) wld_wpaCtrlInterface_getName(pIntf), __VA_ARGS__); \
    }

#define CALL_MGR_NA_EXT(pMgr, fName, ifName) \
    { \
        void* userdata = NULL; \
        wld_wpaCtrl_radioEvtHandlers_cb handlers = {0}; \
        if(wld_wpaCtrlMngr_getEvtHandlers(pMgr, &userdata, &handlers)) { \
            SWL_CALL(handlers.fName, userdata, ifName); \
        } \
    }

#define CALL_MGR_I_NA_EXT(pIntf, fName) \
    if(pIntf != NULL) { \
        char* ifName = (char*) wld_wpaCtrlInterface_getName(pIntf); \
        CALL_MGR_NA_EXT(wld_wpaCtrlInterface_getMgr(pIntf), fName, ifName); \
    }

#define NOTIFY_EXT(PIFACE, FNAME, ...) \
    if((PIFACE != NULL)) { \
        /* for VAP/EP events */ \
        CALL_INTF_EXT(PIFACE, FNAME, __VA_ARGS__); \
        /* for RAD/Glob events */ \
        CALL_MGR_I_EXT(PIFACE, FNAME, __VA_ARGS__); \
    }

#define NOTIFY_NA_EXT(PIFACE, FNAME, ...) \
    if((PIFACE != NULL)) { \
        /* for VAP/EP events */ \
        CALL_INTF_NA_EXT(PIFACE, FNAME); \
        /* for RAD/Glob events */ \
        CALL_MGR_I_NA_EXT(PIFACE, FNAME); \
    }

#endif /* __WLD_WPA_CTRL_MNGR_H__ */
