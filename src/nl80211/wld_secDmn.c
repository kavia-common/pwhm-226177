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
#include <stdlib.h>

#include "swl/swl_common.h"
#include "swl/swl_string.h"
#include "wld_secDmn.h"
#include "wld_secDmnGrp_priv.h"
#include "wld_util.h"
#include "wld_wpaCtrl_api.h"

#define ME "secDmn"

void wld_secDmn_restartCb(wld_secDmn_t* pSecDmn) {
    ASSERT_NOT_NULL(pSecDmn, , ME, "NULL");
    wld_wpaCtrlMngr_disconnect(pSecDmn->wpaCtrlMngr);
    wld_secDmn_start(pSecDmn);
}

static void s_restartProcCb(wld_process_t* pProc _UNUSED, void* userdata) {
    wld_secDmn_t* pSecDmn = (wld_secDmn_t*) userdata;
    ASSERT_NOT_NULL(pSecDmn, , ME, "NULL");
    wld_secDmn_setRestartNeeded(pSecDmn, false);
    if(pSecDmn->handlers.restartCb != NULL) {
        pSecDmn->handlers.restartCb(pSecDmn, pSecDmn->userData);
        return;
    }
    wld_secDmn_restartCb(pSecDmn);
}

static void s_onStopProcCb(wld_process_t* pProc _UNUSED, void* userdata) {
    wld_secDmn_t* pSecDmn = (wld_secDmn_t*) userdata;
    ASSERT_NOT_NULL(pSecDmn, , ME, "NULL");
    wld_wpaCtrlMngr_disconnect(pSecDmn->wpaCtrlMngr);
    if(pSecDmn->handlers.stopCb) {
        pSecDmn->handlers.stopCb(pSecDmn, pSecDmn->userData);
        return;
    }
    //finalize secDmn cleanup
    wld_secDmn_stop(pSecDmn);
}

static void s_onStartProcCb(wld_process_t* pProc _UNUSED, void* userdata) {
    wld_secDmn_t* pSecDmn = (wld_secDmn_t*) userdata;
    ASSERT_NOT_NULL(pSecDmn, , ME, "NULL");
    wld_wpaCtrlMngr_connect(pSecDmn->wpaCtrlMngr);
    if(pSecDmn->handlers.startCb) {
        pSecDmn->handlers.startCb(pSecDmn, pSecDmn->userData);
    }
}

static char* s_getArgsProcCb(wld_process_t* pProc _UNUSED, void* userdata) {
    wld_secDmn_t* pSecDmn = (wld_secDmn_t*) userdata;
    ASSERT_NOT_NULL(pSecDmn, NULL, ME, "NULL");
    if(pSecDmn->handlers.getArgs) {
        return pSecDmn->handlers.getArgs(pSecDmn, pSecDmn->userData);
    }
    return NULL;
}

static bool s_stopProcCb(wld_process_t* pProc _UNUSED, void* userdata) {
    wld_secDmn_t* pSecDmn = (wld_secDmn_t*) userdata;
    ASSERT_NOT_NULL(pSecDmn, false, ME, "NULL");
    if(pSecDmn->handlers.stop) {
        return pSecDmn->handlers.stop(pSecDmn, pSecDmn->userData);
    }
    return false;
}

static wld_deamonEvtHandlers fProcCbs = {
    .restartCb = s_restartProcCb,
    .stopCb = s_onStopProcCb,
    .startCb = s_onStartProcCb,
    .getArgsCb = s_getArgsProcCb,
    .stop = s_stopProcCb,
};

swl_rc_ne wld_secDmn_init(wld_secDmn_t** ppSecDmn, char* cmd, char* startArgs, char* cfgFile, char* ctrlIfaceDir) {
    ASSERT_STR(cmd, SWL_RC_INVALID_PARAM, ME, "invalid cmd");
    ASSERT_NOT_NULL(ppSecDmn, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_secDmn_t* pSecDmn = *ppSecDmn;
    if(pSecDmn == NULL) {
        pSecDmn = calloc(1, sizeof(wld_secDmn_t));
        ASSERT_NOT_NULL(pSecDmn, SWL_RC_ERROR, ME, "NULL");
        *ppSecDmn = pSecDmn;
    }
    ASSERT_NULL(pSecDmn->dmnProcess, SWL_RC_OK, ME, "already initialized");
    if((wld_dmn_createDeamon(&pSecDmn->selfDmnProcess, cmd, startArgs, &fProcCbs, pSecDmn) < SWL_RC_OK) ||
       (!wld_wpaCtrlMngr_init(&pSecDmn->wpaCtrlMngr, pSecDmn))) {
        SAH_TRACEZ_ERROR(ME, "fail to initialize self daemon %s", cmd);
        wld_dmn_destroyDeamon(&pSecDmn->selfDmnProcess);
        return SWL_RC_ERROR;
    }
    pSecDmn->dmnProcess = pSecDmn->selfDmnProcess;
    swl_str_copyMalloc(&pSecDmn->cfgFile, cfgFile);
    swl_str_copyMalloc(&pSecDmn->ctrlIfaceDir, ctrlIfaceDir);
    swl_mapCharInt32_init(&pSecDmn->cfgParamSup);
    swl_mapCharInt32_init(&pSecDmn->cmdSup);
    return SWL_RC_OK;
}

swl_rc_ne wld_secDmn_setEvtHandlers(wld_secDmn_t* pSecDmn, wld_secDmnEvtHandlers* pHandlers, void* userData) {
    ASSERT_NOT_NULL(pSecDmn, SWL_RC_INVALID_PARAM, ME, "NULL");
    pSecDmn->userData = userData;
    if(pHandlers == NULL) {
        memset(&pSecDmn->handlers, 0, sizeof(wld_secDmnEvtHandlers));
    } else {
        memcpy(&pSecDmn->handlers, pHandlers, sizeof(wld_secDmnEvtHandlers));
    }
    return SWL_RC_OK;
}

swl_rc_ne wld_secDmn_cleanup(wld_secDmn_t** ppSecDmn) {
    ASSERTS_NOT_NULL(ppSecDmn, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_secDmn_t* pSecDmn = *ppSecDmn;
    ASSERTS_NOT_NULL(pSecDmn, SWL_RC_OK, ME, "cleaned up");
    wld_wpaCtrlMngr_cleanup(&pSecDmn->wpaCtrlMngr);
    wld_dmn_destroyDeamon(&pSecDmn->selfDmnProcess);
    if(wld_secDmn_isGrpMember(pSecDmn)) {
        wld_secDmnGrp_delMember(pSecDmn->secDmnGroup, pSecDmn);
    }
    pSecDmn->dmnProcess = NULL;
    swl_mapCharInt32_cleanup(&pSecDmn->cfgParamSup);
    swl_mapCharInt32_cleanup(&pSecDmn->cmdSup);
    W_SWL_FREE(pSecDmn->cfgFile);
    W_SWL_FREE(pSecDmn->ctrlIfaceDir);
    free(pSecDmn);
    *ppSecDmn = NULL;
    return SWL_RC_OK;
}

/*
 * @brief start a secDmn process
 * If secDmn has a self daemon then it is started immediately
 * If secDmn is a group member, then the process is finally started when all startable members are started
 *
 * @param pSecDmnGrp security daemon group ctx
 * @param pSecDmn security daemon member ctx
 *
 * @return SWL_RC_OK when daemon process is being started
 *         SWL_RC_CONTINUE when process waits for other startable members to start
 *         SWL_RC_DONE when daemon process is already started (running)
 *         error code otherwise
 */
swl_rc_ne wld_secDmn_start(wld_secDmn_t* pSecDmn) {
    ASSERT_NOT_NULL(pSecDmn, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pSecDmn->dmnProcess, SWL_RC_ERROR, ME, "NULL");
    wld_secDmn_setRestartNeeded(pSecDmn, false);
    if(wld_secDmn_isGrpMember(pSecDmn)) {
        return wld_secDmnGrp_startMember(pSecDmn->secDmnGroup, pSecDmn);
    }
    ASSERTI_FALSE(wld_dmn_isRunning(pSecDmn->dmnProcess), SWL_RC_DONE, ME, "already running");
    return wld_dmn_startDeamon(pSecDmn->dmnProcess) ? SWL_RC_OK : SWL_RC_ERROR;
}

/*
 * @brief stop a secDmn process
 * If secDmn has a self daemon then it is stopped immediately
 * If secDmn is a group member, then the process is finally stopped when all started members are stopped
 *
 * @param pSecDmn security daemon member ctx
 *
 * @return SWL_RC_OK when daemon process is being stopped
 *         SWL_RC_CONTINUE when shared process waits for other members to stop
 *         SWL_RC_DONE when daemon process is already stopped (not running)
 *         error code otherwise
 */
swl_rc_ne wld_secDmn_stop(wld_secDmn_t* pSecDmn) {
    ASSERT_NOT_NULL(pSecDmn, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pSecDmn->dmnProcess, SWL_RC_ERROR, ME, "NULL");
    ASSERTI_TRUE(wld_dmn_isEnabled(pSecDmn->dmnProcess), SWL_RC_INVALID_STATE, ME, "not enabled");
    wld_secDmn_setRestartNeeded(pSecDmn, false);
    if(wld_secDmn_isGrpMember(pSecDmn)) {
        return wld_secDmnGrp_stopMember(pSecDmn->secDmnGroup, pSecDmn);
    }
    ASSERTI_TRUE(wld_dmn_isEnabled(pSecDmn->dmnProcess), SWL_RC_DONE, ME, "already stopped");
    wld_wpaCtrlMngr_disconnect(pSecDmn->wpaCtrlMngr);
    return wld_dmn_stopDeamon(pSecDmn->dmnProcess) ? SWL_RC_OK : SWL_RC_ERROR;
}

swl_rc_ne wld_secDmn_reload(wld_secDmn_t* pSecDmn) {
    ASSERT_NOT_NULL(pSecDmn, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pSecDmn->dmnProcess, SWL_RC_ERROR, ME, "NULL");
    ASSERT_TRUE(wld_dmn_isRunning(pSecDmn->dmnProcess), SWL_RC_INVALID_STATE, ME, "not running");
    wld_dmn_reloadDeamon(pSecDmn->dmnProcess);
    return SWL_RC_OK;
}

swl_rc_ne wld_secDmn_restart(wld_secDmn_t* pSecDmn) {
    ASSERT_NOT_NULL(pSecDmn, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pSecDmn->dmnProcess, SWL_RC_ERROR, ME, "NULL");
    ASSERT_TRUE(wld_dmn_isRunning(pSecDmn->dmnProcess), SWL_RC_INVALID_STATE, ME, "not running");
    wld_secDmn_setRestartNeeded(pSecDmn, false);
    if(wld_secDmn_isGrpMember(pSecDmn)) {
        return wld_secDmnGrp_restartMember(pSecDmn->secDmnGroup, pSecDmn);
    }
    wld_dmn_restartDeamon(pSecDmn->dmnProcess);
    return SWL_RC_OK;
}

swl_rc_ne wld_secDmn_setRestartNeeded(wld_secDmn_t* pSecDmn, bool flag) {
    ASSERT_NOT_NULL(pSecDmn, SWL_RC_INVALID_PARAM, ME, "NULL");
    pSecDmn->needRestart = flag;
    return SWL_RC_OK;
}

bool wld_secDmn_checkRestartNeeded(wld_secDmn_t* pSecDmn) {
    ASSERT_NOT_NULL(pSecDmn, false, ME, "NULL");
    return pSecDmn->needRestart;
}

bool wld_secDmn_isRestarting(wld_secDmn_t* pSecDmn) {
    ASSERT_NOT_NULL(pSecDmn, false, ME, "NULL");
    ASSERT_NOT_NULL(pSecDmn->dmnProcess, false, ME, "NULL");
    if(wld_secDmn_isGrpMember(pSecDmn)) {
        return wld_secDmnGrp_isMemberRestarting(pSecDmn->secDmnGroup, pSecDmn);
    }
    return wld_dmn_isRestarting(pSecDmn->dmnProcess);
}

bool wld_secDmn_isGrpRestarting(wld_secDmn_t* pSecDmn) {
    ASSERT_NOT_NULL(pSecDmn, false, ME, "NULL");
    ASSERT_NOT_NULL(pSecDmn->dmnProcess, false, ME, "NULL");
    return (wld_dmn_isRestarting(pSecDmn->dmnProcess) ||
            (wld_secDmn_isGrpMember(pSecDmn) &&
             wld_secDmnGrp_hasMemberRestarting(pSecDmn->secDmnGroup)));
}

swl_rc_ne wld_secDmn_setArgs(wld_secDmn_t* pSecDmn, char* startArgs) {
    ASSERT_NOT_NULL(pSecDmn, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pSecDmn->dmnProcess, SWL_RC_ERROR, ME, "NULL");

    ASSERT_FALSE(wld_secDmn_isGrpMember(pSecDmn), SWL_RC_ERROR, ME, "grp member must not set args of grp process");
    wld_dmn_setArgList(pSecDmn->dmnProcess, startArgs);

    return SWL_RC_OK;
}

bool wld_secDmn_isRunning(wld_secDmn_t* pSecDmn) {
    ASSERTS_NOT_NULL(pSecDmn, false, ME, "NULL");
    return wld_dmn_isRunning(pSecDmn->dmnProcess);
}

bool wld_secDmn_isEnabled(wld_secDmn_t* pSecDmn) {
    ASSERTS_NOT_NULL(pSecDmn, false, ME, "NULL");
    if(wld_secDmn_isGrpMember(pSecDmn)) {
        return wld_secDmnGrp_isMemberStarted(pSecDmn->secDmnGroup, pSecDmn);
    }
    return wld_dmn_isEnabled(pSecDmn->dmnProcess);
}

bool wld_secDmn_isAlive(wld_secDmn_t* pSecDmn) {
    ASSERTS_NOT_NULL(pSecDmn, false, ME, "NULL");
    return (wld_secDmn_isRunning(pSecDmn) && wld_wpaCtrlMngr_isConnected(pSecDmn->wpaCtrlMngr) &&
            ((!wld_secDmn_isGrpMember(pSecDmn)) ||
             (wld_secDmnGrp_isMemberStarted(pSecDmn->secDmnGroup, pSecDmn))));
}

bool wld_secDmn_hasAvailableCtrlIface(wld_secDmn_t* pSecDmn) {
    ASSERTS_NOT_NULL(pSecDmn, false, ME, "NULL");
    return (wld_wpaCtrlMngr_getFirstAvailableInterface(pSecDmn->wpaCtrlMngr) != NULL);
}

wld_wpaCtrlMngr_t* wld_secDmn_getWpaCtrlMgr(wld_secDmn_t* pSecDmn) {
    ASSERTS_NOT_NULL(pSecDmn, NULL, ME, "NULL");
    return pSecDmn->wpaCtrlMngr;
}

bool wld_secDmn_setCfgParamSupp(wld_secDmn_t* pSecDmn, const char* param, swl_trl_e supp) {
    ASSERTS_NOT_NULL(pSecDmn, false, ME, "NULL");
    return swl_mapCharInt32_addOrSet(&pSecDmn->cfgParamSup, (char*) param, supp);
}

/*
 * @brief checks wether a config param is supported or not (with dynamic detection)
 *
 * @param pSecDmn pointer to security daemon context
 * @param param parameter name
 *
 * @return SWL_TRL_TRUE when the param is supported
 *         SWL_TRL_FALSE when the param is not supported
 *         SWL_TRL_UNKNOWN when the param support is not yet learned
 */
swl_trl_e wld_secDmn_getCfgParamSupp(wld_secDmn_t* pSecDmn, const char* param) {
    ASSERTS_NOT_NULL(pSecDmn, SWL_TRL_UNKNOWN, ME, "NULL");
    int32_t* pVal = swl_map_get(&pSecDmn->cfgParamSup, (char*) param);
    ASSERTS_NOT_NULL(pVal, SWL_TRL_UNKNOWN, ME, "not found");
    uint32_t nMbr = wld_secDmnGrp_getMembersCount(pSecDmn->secDmnGroup);
    if(nMbr <= 1) {
        return *pVal;
    }
    swl_trl_e supp = SWL_TRL_UNKNOWN;
    for(uint32_t i = 0; i < nMbr; i++) {
        wld_secDmn_t* pTmpSecDmn = (wld_secDmn_t*) wld_secDmnGrp_getMemberByPos(pSecDmn->secDmnGroup, i);
        pVal = swl_map_get(&pTmpSecDmn->cfgParamSup, (char*) param);
        if(pVal) {
            if(*pVal == SWL_TRL_FALSE) {
                return SWL_TRL_FALSE;
            }
            if(*pVal == SWL_TRL_TRUE) {
                supp = SWL_TRL_TRUE;
            }
        }
    }
    return supp;
}

uint32_t wld_secDmn_countCfgParamSuppAll(wld_secDmn_t* pSecDmn) {
    ASSERTS_NOT_NULL(pSecDmn, 0, ME, "NULL");
    return swl_map_size(&pSecDmn->cfgParamSup);
}

uint32_t wld_secDmn_countCfgParamSuppChecked(wld_secDmn_t* pSecDmn) {
    uint32_t count = 0;
    ASSERTS_NOT_NULL(pSecDmn, count, ME, "NULL");
    swl_mapIt_t mapIt;
    swl_map_for_each(mapIt, &pSecDmn->cfgParamSup) {
        int32_t* pSupp = (int32_t*) swl_map_itValue(&mapIt);
        if((pSupp != NULL) && (*pSupp != SWL_TRL_UNKNOWN)) {
            count++;
        }
    }
    return count;
}

uint32_t wld_secDmn_countCfgParamSuppByVal(wld_secDmn_t* pSecDmn, swl_trl_e supp) {
    uint32_t count = 0;
    ASSERTS_NOT_NULL(pSecDmn, count, ME, "NULL");
    swl_mapIt_t mapIt;
    swl_map_for_each(mapIt, &pSecDmn->cfgParamSup) {
        int32_t* pSupp = (int32_t*) swl_map_itValue(&mapIt);
        if((pSupp != NULL) && (*pSupp == (int32_t) supp)) {
            count++;
        }
    }
    return count;
}

bool wld_secDmn_setCmdSupp(wld_secDmn_t* pSecDmn, const char* cmd, swl_trl_e supp) {
    ASSERTS_NOT_NULL(pSecDmn, false, ME, "NULL");
    return swl_mapCharInt32_addOrSet(&pSecDmn->cmdSup, (char*) cmd, supp);
}

swl_trl_e wld_secDmn_getCmdSupp(wld_secDmn_t* pSecDmn, const char* cmd) {
    ASSERTS_NOT_NULL(pSecDmn, SWL_TRL_UNKNOWN, ME, "NULL");
    int32_t* pVal = swl_map_get(&pSecDmn->cmdSup, (char*) cmd);
    ASSERTS_NOT_NULL(pVal, SWL_TRL_UNKNOWN, ME, "not found");
    uint32_t nMbr = wld_secDmnGrp_getMembersCount(pSecDmn->secDmnGroup);
    if(nMbr <= 1) {
        return *pVal;
    }
    swl_trl_e supp = SWL_TRL_UNKNOWN;
    for(uint32_t i = 0; i < nMbr; i++) {
        wld_secDmn_t* pTmpSecDmn = (wld_secDmn_t*) wld_secDmnGrp_getMemberByPos(pSecDmn->secDmnGroup, i);
        pVal = swl_map_get(&pTmpSecDmn->cmdSup, (char*) cmd);
        if(pVal) {
            if(*pVal == SWL_TRL_FALSE) {
                return SWL_TRL_FALSE;
            }
            if(*pVal == SWL_TRL_TRUE) {
                supp = SWL_TRL_TRUE;
            }
        }
    }
    return supp;
}

const char* wld_secDmn_getCtrlIfaceDirPath(wld_secDmn_t* pSecDmn) {
    ASSERTS_NOT_NULL(pSecDmn, "", ME, "NULL");
    return pSecDmn->ctrlIfaceDir;
}

/*
 * @brief add secDmn member to a group
 * Therefore, the secDmn is sharing same process with other members:
 * The group process is started only when all startable members are started
 * and it is stopped when all started members have asked to stop
 *
 * @param pSecDmn pointer to secDmn context
 * @param pSecDmnGrp pointer to secDmn group context
 * @param memberName optional member name used as string id
 *
 * @return SWL_RC_OK if successful, error code otherwise
 */
swl_rc_ne wld_secDmn_addToGrp(wld_secDmn_t* pSecDmn, wld_secDmnGrp_t* pSecDmnGrp, const char* memberName) {
    ASSERTS_NOT_NULL(pSecDmn, SWL_RC_INVALID_PARAM, ME, "NULL");
    if(pSecDmnGrp == NULL) {
        ASSERTS_NOT_NULL(pSecDmn->secDmnGroup, SWL_RC_INVALID_PARAM, ME, "No target grp provided");
        SAH_TRACEZ_WARNING(ME, "removing secDmn %p from current group %p, and restore using self proc", pSecDmn, pSecDmnGrp);
        wld_secDmnGrp_delMember(pSecDmnGrp, pSecDmn);
        pSecDmn->dmnProcess = pSecDmn->selfDmnProcess;
        return SWL_RC_OK;
    }
    if(pSecDmn->secDmnGroup != NULL) {
        ASSERT_EQUALS(pSecDmn->secDmnGroup, pSecDmnGrp, SWL_RC_ERROR,
                      ME, "secDmn %p is already in another group %p (than new %p)",
                      pSecDmn, pSecDmn->secDmnGroup, pSecDmnGrp);
        SAH_TRACEZ_INFO(ME, "secDmn %p already added to group %p", pSecDmn, pSecDmnGrp);
        return SWL_RC_OK;
    }
    swl_rc_ne rc = wld_secDmnGrp_addMember(pSecDmnGrp, pSecDmn, memberName);
    ASSERT_TRUE(swl_rc_isOk(rc), rc, ME, "fail to add secDmn %p (%s) to group %p", pSecDmn, memberName, pSecDmnGrp);
    wld_secDmnGrp_setMemberEvtHdlrs(pSecDmnGrp, pSecDmn, &fProcCbs, pSecDmn);
    if(wld_dmn_isRunning(pSecDmn->selfDmnProcess)) {
        SAH_TRACEZ_INFO(ME, "stop running self proc %p", pSecDmn->selfDmnProcess);
        wld_dmn_stopDeamon(pSecDmn->selfDmnProcess);
    }
    pSecDmn->dmnProcess = wld_secDmnGrp_getProc(pSecDmnGrp);
    pSecDmn->secDmnGroup = pSecDmnGrp;
    wld_secDmn_setRestartNeeded(pSecDmn, false);
    return SWL_RC_OK;
}

/*
 * @brief delete secDmn member from a group
 * Therefore, the secDmn is implicitly stopped before, and it is was the last
 * running member, then the group process is terminated.
 *
 * @param pSecDmn pointer to secDmn context
 * @param pSecDmnGrp pointer to secDmn group context
 *
 * @return SWL_RC_OK if successful, error code otherwise
 */
swl_rc_ne wld_secDmn_delFromGrp(wld_secDmn_t* pSecDmn) {
    ASSERTS_NOT_NULL(pSecDmn, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pSecDmn->secDmnGroup, SWL_RC_INVALID_STATE, ME, "NULL");
    swl_rc_ne rc = wld_secDmnGrp_delMember(pSecDmn->secDmnGroup, pSecDmn);
    pSecDmn->secDmnGroup = NULL;
    pSecDmn->dmnProcess = pSecDmn->selfDmnProcess;
    wld_secDmn_setRestartNeeded(pSecDmn, false);
    return rc;
}

/*
 * @brief returns whethersecDmn is a group member
 */
bool wld_secDmn_isGrpMember(wld_secDmn_t* pSecDmn) {
    ASSERTS_NOT_NULL(pSecDmn, false, ME, "NULL");
    ASSERTS_NOT_NULL(pSecDmn->dmnProcess, false, ME, "NULL");
    return (pSecDmn->dmnProcess == wld_secDmnGrp_getProc(pSecDmn->secDmnGroup));
}

/*
 * @brief returns the own group context, if secDmn is a member
 */
wld_secDmnGrp_t* wld_secDmn_getGrp(wld_secDmn_t* pSecDmn) {
    ASSERTS_NOT_NULL(pSecDmn, NULL, ME, "NULL");
    return pSecDmn->secDmnGroup;
}

/*
 * @brief returns the count of members of the secDmn group
 */
uint32_t wld_secDmn_countGrpMembers(wld_secDmn_t* pSecDmn) {
    return wld_secDmnGrp_getMembersCount(wld_secDmn_getGrp(pSecDmn));
}

/*
 * @brief returns the count of active (ie connected) members of the secDmn group
 */
uint32_t wld_secDmn_countActiveGrpMembers(wld_secDmn_t* pSecDmn) {
    return wld_secDmnGrp_getActiveMembersCount(wld_secDmn_getGrp(pSecDmn));
}

/*
 * @brief returns whether active (ie connected) alone or the only one in secDmn group
 */
bool wld_secDmn_isActiveAlone(wld_secDmn_t* pSecDmn) {
    return ((wld_secDmn_isAlive(pSecDmn)) &&
            ((!wld_secDmn_isGrpMember(pSecDmn)) ||
             (wld_secDmn_countActiveGrpMembers(pSecDmn) == 1)));
}

/*
 * @brief detect support of token string fetched in security daemon binary
 * (binary full path fetched through PATH env)
 *
 * @param pSecDmn secDmn ctx
 * @param cfgParams array of cfg param strings
 * @param nCfgParams number of elements in cfgParams array
 * @param optCfgParamPrefix optional common prefix string to speed up scanning cfg param strings in binary file
 *
 * @return number of detected params, -1 in case of error
 */
int32_t wld_secDmn_detectCfgParamsSupp(wld_secDmn_t* pSecDmn, const char* cfgParams[], uint32_t nCfgParams, const char* optCfgParamPrefix) {
    ASSERTS_NOT_NULL(pSecDmn, -1, ME, "NULL");
    ASSERT_NOT_NULL(pSecDmn->dmnProcess, -1, ME, "Null dmn proc ctx");
    ASSERT_TRUE(cfgParams && nCfgParams, -1, ME, "No cfg params");
    char binPath[PATH_MAX] = {0};
    const char* cmd = pSecDmn->dmnProcess->cmd;
    swl_rc_ne rc = wld_util_getExecutablePath(cmd, binPath, sizeof(binPath));
    ASSERT_STR(binPath, -1, ME, "Fail to locate cmd(%s) bin path (rc:%d:%s)", cmd, rc, swl_rc_toString(rc))
    return wld_wpaCtrl_detectKeywordsSupp(&pSecDmn->cfgParamSup, binPath, cfgParams, nCfgParams, optCfgParamPrefix);
}

