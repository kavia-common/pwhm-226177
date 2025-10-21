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
#include "wld.h"
#include "wld_daemon.h"
#include "swl/swl_common.h"
#include "swl/swl_string.h"

#define ME "wldDmn"

const char* g_str_wld_dmn_state[] = {"Init", "Up", "Down", "Error", "Error", "Destroyed", "Restarting", };

static wld_daemonMonitorConf_t s_dmnMoniConf = {
    .enableParam = false,
    .instantRestartLimit = 3,
    .minRestartInterval = 1800,
};

void wld_dmn_setMonitorConf(wld_daemonMonitorConf_t* pDmnMoniConf) {
    ASSERTS_NOT_NULL(pDmnMoniConf, , ME, "NULL");
    s_dmnMoniConf.enableParam = pDmnMoniConf->enableParam;
    s_dmnMoniConf.instantRestartLimit = pDmnMoniConf->instantRestartLimit;
    s_dmnMoniConf.minRestartInterval = pDmnMoniConf->minRestartInterval;
}

static void s_deamonStarter(amxp_timer_t* timer _UNUSED, void* userdata) {
    wld_process_t* process = (wld_process_t*) userdata;
    ASSERTS_NOT_NULL(process, , ME, "NULL");
    if(process->handlers.restartCb != NULL) {
        process->handlers.restartCb(process, process->userData);
        return;
    }
    //keep same stop handler
    wld_dmn_startDeamon(process);
}

static int s_cmdBuilder(amxc_array_t* cmd, amxc_var_t* settings) {
    ASSERT_NOT_NULL(settings, -1, ME, "No settings");
    wld_process_t* dmn_process = (wld_process_t*) settings->data.data;
    ASSERT_NOT_NULL(dmn_process, -1, ME, "NULL");
    uint32_t i = 0;
    while(dmn_process->argList && dmn_process->argList[i]) {
        amxc_array_append_data(cmd, strdup(dmn_process->argList[i]));
        i++;
    }
    return -(i == 0);
}

bool wld_dmn_initializeDeamon(wld_process_t* process, char* cmd) {
    ASSERT_NOT_NULL(process, false, ME, "NULL");
    ASSERT_NOT_NULL(cmd, false, ME, "NULL");
    ASSERT_NOT_EQUALS(process->status, WLD_DAEMON_STATE_DOWN, false, ME, "INVALID STATE");
    ASSERT_NOT_EQUALS(process->status, WLD_DAEMON_STATE_UP, false, ME, "INVALID STATE");
    ASSERT_NOT_EQUALS(process->status, WLD_DAEMON_STATE_ERROR, false, ME, "INVALID STATE");

    SAH_TRACEZ_INFO(ME, "initialize %s", cmd);

    memset(process, 0, sizeof(*process));
    process->argList = calloc(2, sizeof(char*));
    ASSERT_NOT_NULL(process->argList, false, ME, "fail to alloc argList");
    amxp_proc_ctrl_new(&process->process, s_cmdBuilder);
    if(process->process == NULL) {
        SAH_TRACEZ_ERROR(ME, "fail to create process");
        free(process->argList);
        process->argList = NULL;
        return false;
    }

    process->cmd = cmd;
    swl_str_copyMalloc(&process->argList[0], process->cmd);
    process->status = WLD_DAEMON_STATE_DOWN;
    process->enabled = false;

    amxp_timer_new(&process->restart_timer, s_deamonStarter, process);
    process->failDate = time(NULL);

    return true;
}

bool wld_dmn_setDeamonEvtHandlers(wld_process_t* process, wld_deamonEvtHandlers* pHandlers,
                                  void* userData) {
    ASSERT_NOT_NULL(process, false, ME, "NULL");
    process->userData = userData;
    if(pHandlers == NULL) {
        memset(&process->handlers, 0, sizeof(wld_deamonEvtHandlers));
    } else {
        memcpy(&process->handlers, pHandlers, sizeof(wld_deamonEvtHandlers));
    }
    return true;
}

void wld_dmn_cleanupDaemon(wld_process_t* process) {
    ASSERT_NOT_NULL(process, , ME, "NULL");
    ASSERT_NOT_EQUALS(process->status, WLD_DAEMON_STATE_INIT, , ME, "INVALID STATE");
    ASSERT_NOT_EQUALS(process->status, WLD_DAEMON_STATE_DESTROYED, , ME, "INVALID STATE");

    SAH_TRACEZ_INFO(ME, "Cleaning up full %s", process->cmd);

    if(process->status == WLD_DAEMON_STATE_UP) {
        wld_dmn_stopDeamon(process);
    }

    if(process->stdErrPeer != 0) {
        amxo_connection_remove(get_wld_plugin_parser(), process->stdErrPeer);
        process->stdErrPeer = 0;
    }
    if(process->stdInPeer != 0) {
        amxo_connection_remove(get_wld_plugin_parser(), process->stdInPeer);
        process->stdInPeer = 0;
    }

    if(process->argList != NULL) {
        for(uint32_t i = 0; i < process->nrArgs + 2; i++) { // +2 because argList[0] = process->cmd, and last is NULL
            W_SWL_FREE(process->argList[i]);
        }
        W_SWL_FREE(process->argList);
    }

    W_SWL_FREE(process->args);
    process->status = WLD_DAEMON_STATE_DESTROYED;

    amxp_proc_ctrl_delete(&process->process);
    process->process = NULL;

    amxp_timer_delete(&process->restart_timer);
    process->restart_timer = NULL;
}

swl_rc_ne wld_dmn_createDeamon(wld_process_t** pDmnProcess, char* cmd, char* startArgs, wld_deamonEvtHandlers* pEvtHdlrs, void* pEvtData) {
    ASSERT_STR(cmd, SWL_RC_INVALID_PARAM, ME, "invalid cmd");
    ASSERT_NOT_NULL(pDmnProcess, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NULL(*pDmnProcess, SWL_RC_OK, ME, "already initialized");
    wld_process_t* dmnProcess = calloc(1, sizeof(wld_process_t));
    ASSERT_NOT_NULL(dmnProcess, SWL_RC_ERROR, ME, "NULL");
    if((!wld_dmn_initializeDeamon(dmnProcess, cmd)) ||
       (!wld_dmn_setDeamonEvtHandlers(dmnProcess, pEvtHdlrs, pEvtData))) {
        SAH_TRACEZ_ERROR(ME, "fail to initialize daemon %s", cmd);
        wld_dmn_cleanupDaemon(dmnProcess);
        free(dmnProcess);
        return SWL_RC_ERROR;
    }
    wld_dmn_setArgList(dmnProcess, startArgs);
    *pDmnProcess = dmnProcess;
    return SWL_RC_OK;
}

void wld_dmn_destroyDeamon(wld_process_t** pDmnProcess) {
    ASSERTS_NOT_NULL(pDmnProcess, , ME, "NULL");
    ASSERTS_NOT_NULL(*pDmnProcess, , ME, "NULL");
    wld_dmn_cleanupDaemon(*pDmnProcess);
    free(*pDmnProcess);
    *pDmnProcess = NULL;
}

static void s_getCurArgs(wld_process_t* process, char* args, uint32_t maxLen) {
    ASSERT_NOT_NULL(process, , ME, "NULL");
    ASSERT_NOT_NULL(args, , ME, "NULL");
    uint32_t i = 0;
    memset(args, 0, maxLen);
    for(i = 0; i < process->nrArgs; i++) {
        swl_str_catFormat(args, maxLen, "%s%s", (args[0] ? " " : ""), process->argList[i + 1]);
    }
}

static void s_stopHandler(const char* const sig_name _UNUSED,
                          const amxc_var_t* const data _UNUSED,
                          void* const priv _UNUSED) {
    wld_process_t* dmn_process = (wld_process_t*) priv;
    wld_dmn_stopCb(dmn_process);
}


bool wld_dmn_reloadDeamon(wld_process_t* dmn_process) {
    ASSERT_NOT_NULL(dmn_process, false, ME, "NULL");
    ASSERT_NOT_NULL(dmn_process->process, false, ME, "NULL");
    ASSERT_EQUALS(dmn_process->status, WLD_DAEMON_STATE_UP, false, ME, "Not Running");
    const char* name = dmn_process->cmd;

    bool ret = false;
    if(dmn_process->handlers.reload) {
        SAH_TRACEZ_INFO(ME, "reload %s via user handler", name);
        ret = dmn_process->handlers.reload(dmn_process, dmn_process->userData);
    }
    if(!ret) {
        SAH_TRACEZ_INFO(ME, "reload %s with signals", name);
        ret = (amxp_subproc_kill(dmn_process->process->proc, SIGHUP) == 0);
    }
    return ret;
}

bool wld_dmn_startDeamon(wld_process_t* dmn_process) {
    ASSERT_NOT_NULL(dmn_process, false, ME, "NULL");
    ASSERT_NOT_NULL(dmn_process->process, false, ME, "NULL");
    ASSERT_TRUE(dmn_process->status != WLD_DAEMON_STATE_UP, false, ME, "Running");

    SAH_TRACEZ_INFO(ME, "Starting %s", dmn_process->cmd);

    if(dmn_process->handlers.getArgsCb != NULL) {
        char* newStartArgs = dmn_process->handlers.getArgsCb(dmn_process, dmn_process->userData);
        if(newStartArgs != NULL) {
            SAH_TRACEZ_INFO(ME, "Setting %s startArgs (%s)", dmn_process->cmd, newStartArgs);
            wld_dmn_setArgList(dmn_process, newStartArgs);
            free(newStartArgs);
        }
    }

    amxc_var_t settings;
    amxc_var_init(&settings);
    amxc_var_set_type(&settings, AMXC_VAR_ID_CUSTOM_BASE);
    settings.data.data = dmn_process;
    int ret = amxp_proc_ctrl_start(dmn_process->process, 0, &settings);
    amxc_var_clean(&settings);
    if(ret != 0) {
        SAH_TRACEZ_ERROR(ME, "Failed to start %s dmn_process", dmn_process->cmd);
        return false;
    }
    // Ensure previous stop handler registrations are removed before re-registering.
    amxp_slot_disconnect_with_priv(amxp_subproc_get_sigmngr(dmn_process->process->proc),
                                   s_stopHandler, dmn_process);
    // Register the stop handler for the stop signal.
    amxp_slot_connect(amxp_subproc_get_sigmngr(dmn_process->process->proc),
                      "stop", NULL, s_stopHandler, dmn_process);

    amxp_timer_state_t tmState = amxp_timer_get_state(dmn_process->restart_timer);
    if((tmState == amxp_timer_started) || (tmState == amxp_timer_running)) {
        if(dmn_process->status == WLD_DAEMON_STATE_ERROR) {
            SAH_TRACEZ_ERROR(ME, "Plugin called crashed process %s to start", dmn_process->cmd);
        } else if(dmn_process->status == WLD_DAEMON_STATE_RESTARTING) {
            SAH_TRACEZ_WARNING(ME, "Plugin force immediate restart of process %s", dmn_process->cmd);
        }
        amxp_timer_stop(dmn_process->restart_timer);
        dmn_process->fails = 0;
        dmn_process->restarts = 0;
    } else {
        if(dmn_process->status == WLD_DAEMON_STATE_ERROR) {
            SAH_TRACEZ_ERROR(ME, "Process %s restarts after a crash.", dmn_process->cmd);
        } else if(dmn_process->status == WLD_DAEMON_STATE_RESTARTING) {
            SAH_TRACEZ_ERROR(ME, "Process %s restarts by user request.", dmn_process->cmd);
        }
        dmn_process->restarts++;
        dmn_process->totalRestarts++;
    }

    dmn_process->status = WLD_DAEMON_STATE_UP;
    dmn_process->enabled = true;

    char curArgs[dmn_process->argLen + 1];
    s_getCurArgs(dmn_process, curArgs, sizeof(curArgs));
    SAH_TRACEZ_INFO(ME, "Successful start %s, args: %s", dmn_process->cmd, curArgs);
    SWL_CALL(dmn_process->handlers.startCb, dmn_process, dmn_process->userData);
    return true;
}

bool wld_dmn_stopDeamon(wld_process_t* dmn_process) {
    ASSERT_NOT_NULL(dmn_process, false, ME, "NULL");
    amxp_proc_ctrl_t* p = dmn_process->process;
    const char* name = dmn_process->cmd;

    ASSERT_NOT_NULL(p, false, ME, "NULL");
    ASSERT_NOT_NULL(name, false, ME, "NULL");
    if(amxp_subproc_is_running(p->proc)) {
        SAH_TRACEZ_INFO(ME, "stopping up %s", dmn_process->cmd);

        amxp_slot_disconnect_with_priv(amxp_subproc_get_sigmngr(dmn_process->process->proc),
                                       s_stopHandler, dmn_process);
        amxp_proc_ctrl_stop(p);
    } else {
        SAH_TRACEZ_INFO(ME, "stop crashed daemon %s", dmn_process->cmd);
    }

    amxp_timer_stop(dmn_process->restart_timer);



    dmn_process->status = WLD_DAEMON_STATE_DOWN;
    dmn_process->enabled = false;

    memset(&dmn_process->lastExitInfo, 0, sizeof(wld_deamonExitInfo_t));
    dmn_process->lastExitInfo.isStopped = true;
    SWL_CALL(dmn_process->handlers.stopCb, dmn_process, dmn_process->userData);

    SAH_TRACEZ_INFO(ME, "%s success terminated", name);
    return true;
}

bool wld_dmn_restartDeamon(wld_process_t* dmn_process) {
    ASSERT_NOT_NULL(dmn_process, false, ME, "NULL");
    amxp_proc_ctrl_t* p = dmn_process->process;
    const char* name = dmn_process->cmd;

    ASSERT_NOT_NULL(p, false, ME, "NULL");
    ASSERT_NOT_NULL(name, false, ME, "NULL");
    ASSERTI_TRUE(amxp_subproc_is_running(p->proc), true, ME, "Stopped %s", name);

    SAH_TRACEZ_INFO(ME, "restarting %s", dmn_process->cmd);

    amxp_timer_stop(dmn_process->restart_timer);

    bool ret = false;
    if(dmn_process->handlers.stop) {
        SAH_TRACEZ_INFO(ME, "terminate %s via user handler", name);
        ret = dmn_process->handlers.stop(dmn_process, dmn_process->userData);
    }
    if(!ret) {
        SAH_TRACEZ_INFO(ME, "stop %s with signals", name);
        amxp_subproc_kill(p->proc, SIGTERM);
    }

    memset(&dmn_process->lastExitInfo, 0, sizeof(wld_deamonExitInfo_t));

    //rely on the restart timer to start again the process and being fully stopped
    //(detection through child process status notification)
    dmn_process->status = WLD_DAEMON_STATE_RESTARTING;
    dmn_process->enabled = true;

    SAH_TRACEZ_INFO(ME, "%s restart initiated", name);
    return true;
}

bool wld_dmn_stopCb(wld_process_t* end_process) {
    if(end_process == NULL) {
        SAH_TRACEZ_ERROR(ME, "Unknown stopped unexpectedly");
        return true;
    }
    ASSERTS_NOT_EQUALS(end_process->status, WLD_DAEMON_STATE_ERROR, true,
                       ME, "Process %s crash already processed", end_process->cmd);

    wld_deamonExitInfo_t* pExitInfo = &end_process->lastExitInfo;
    memset(pExitInfo, 0, sizeof(wld_deamonExitInfo_t));
    pExitInfo->isExited = (amxp_subproc_ifexited(end_process->process->proc) != 0);
    if(pExitInfo->isExited) {
        pExitInfo->exitStatus = amxp_subproc_get_exitstatus(end_process->process->proc);
    }
    pExitInfo->isSignaled = (amxp_subproc_ifsignaled(end_process->process->proc) != 0);
    if(pExitInfo->isSignaled) {
        pExitInfo->termSignal = amxp_subproc_get_termsig(end_process->process->proc);
    }

    if(end_process->status == WLD_DAEMON_STATE_RESTARTING) {
        SWL_CALL(end_process->handlers.stopCb, end_process, end_process->userData);
        amxp_timer_start(end_process->restart_timer, 100 /* ms */);
        SAH_TRACEZ_WARNING(ME, "User requested Process %s restart now.", end_process->cmd);
        return true;
    }

    end_process->status = WLD_DAEMON_STATE_ERROR;
    SAH_TRACEZ_ERROR(ME, "Process %s stopped unexpectedly (e:%d/r:%d/k:%d/s:%d)",
                     end_process->cmd,
                     pExitInfo->isExited, pExitInfo->exitStatus,
                     pExitInfo->isSignaled, pExitInfo->termSignal);

    SAH_TRACEZ_INFO(ME, "%s->fails is %d And %s->totalFails is %d.", end_process->cmd,
                    end_process->fails, end_process->cmd, end_process->totalFails);
    end_process->fails++;
    end_process->totalFails++;
    SAH_TRACEZ_INFO(ME, "%s->fails updated to %d And %s->totalFails to %d.", end_process->cmd,
                    end_process->fails, end_process->cmd, end_process->totalFails);
    SWL_CALL(end_process->handlers.stopCb, end_process, end_process->userData);

    time_t lastFailDate = end_process->failDate;
    end_process->failDate = time(NULL);
    if(s_dmnMoniConf.enableParam && end_process->cmd) {
        double elapsedTime = difftime(end_process->failDate, lastFailDate);
        SAH_TRACEZ_ERROR(ME, "Last daemon crash occured %.2f seconds before.", elapsedTime);

        amxp_timer_stop(end_process->restart_timer);
        if((s_dmnMoniConf.instantRestartLimit >= 0) &&
           (end_process->totalFails > (uint32_t) s_dmnMoniConf.instantRestartLimit) &&
           (s_dmnMoniConf.minRestartInterval > (uint32_t) elapsedTime)) {
            uint32_t restartInterval = (end_process->totalFails == (uint32_t) s_dmnMoniConf.instantRestartLimit + 1) ?
                s_dmnMoniConf.minRestartInterval :
                (s_dmnMoniConf.minRestartInterval - elapsedTime);
            amxp_timer_start(end_process->restart_timer, restartInterval * 1000);
            SAH_TRACEZ_ERROR(ME, "Dead Process %s restart in %d s.", end_process->cmd, restartInterval);
        } else {
            amxp_timer_start(end_process->restart_timer, 100 /* ms */);
            SAH_TRACEZ_ERROR(ME, "Dead Process %s restart now.", end_process->cmd);
        }
    }

    return true;
}

bool wld_dmn_isRunning(wld_process_t* process) {
    if(process && (process->status == WLD_DAEMON_STATE_UP)) {
        return true;
    }
    return false;
}

bool wld_dmn_isEnabled(wld_process_t* process) {
    ASSERT_NOT_NULL(process, false, ME, "NULL");
    return process->enabled;
}

bool wld_dmn_isRestarting(wld_process_t* process) {
    ASSERT_NOT_NULL(process, false, ME, "NULL");
    amxp_timer_state_t tmState = amxp_timer_get_state(process->restart_timer);
    if(((tmState == amxp_timer_started) || (tmState == amxp_timer_running)) &&
       ((process->status == WLD_DAEMON_STATE_RESTARTING) ||
        (process->status == WLD_DAEMON_STATE_ERROR))) {
        return true;
    }
    return false;
}

void wld_dmn_setArgList(wld_process_t* process, char* args) {
    uint32_t nrArgs = 0;
    uint32_t i = 0;
    const char* newArgs = "";

    ASSERT_NOT_NULL(process, , ME, "NULL");
    ASSERT_NOT_NULL(process->argList, , ME, "NULL");
    char curArgs[process->argLen + 1];
    s_getCurArgs(process, curArgs, sizeof(curArgs));
    if(args != NULL) {
        newArgs = args;
    }
    ASSERTI_FALSE(swl_str_matches(curArgs, newArgs), , ME, "dmn %s: same args (%s)", process->cmd, newArgs);

    for(uint32_t i = 0; i < process->nrArgs + 2; i++) { // +2 because argList[0] = process->cmd, and last is NULL
        W_SWL_FREE(process->argList[i]);
    }
    W_SWL_FREE(process->argList);
    process->nrArgs = 0;
    W_SWL_FREE(process->args);
    process->argLen = 0;

    if(newArgs[0]) {
        nrArgs = swl_str_countChar(newArgs, ' ') + 1;
        swl_str_copyMalloc(&process->args, newArgs);
        ASSERT_NOT_NULL(process->args, , ME, "fail to duplicate args");
    }
    process->argLen = swl_str_len(newArgs);
    //argList = cmd + args + null termination
    process->argList = calloc((nrArgs + 2), sizeof(char*));
    ASSERT_NOT_NULL(process->argList, , ME, "fail to alloc argList");
    process->nrArgs = nrArgs;

    char bufferTok[swl_str_len(newArgs) + 1];
    swl_str_copy(bufferTok, sizeof(bufferTok), newArgs);
    swl_str_copyMalloc(&process->argList[0], process->cmd);
    for(i = 0; i < nrArgs; i++) {
        swl_str_copyMalloc(&process->argList[i + 1], strtok((i ? NULL : bufferTok), " "));
    }
    SAH_TRACEZ_INFO(ME, "init dmn %s args %s : %u", process->cmd, newArgs, nrArgs);
}

void wld_dmn_setForceKill(wld_process_t* process, bool forceKill) {
    ASSERT_NOT_NULL(process, , ME, "NULL");
    process->forceKill = forceKill;
}

