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

#ifndef INCLUDE_WLD_WLD_DAEMON_H_
#define INCLUDE_WLD_WLD_DAEMON_H_

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <signal.h>

/* Daemon running state. */
typedef enum {
    WLD_DAEMON_STATE_INIT,
    WLD_DAEMON_STATE_UP,
    WLD_DAEMON_STATE_DOWN,
    WLD_DAEMON_STATE_ERROR,
    WLD_DAEMON_STATE_DESTROYED,
    WLD_DAEMON_STATE_RESTARTING,
    WLD_DAEMON_STATE_MAX,
} wld_deamonState_e;

extern const char* g_str_wld_dmn_state[];
typedef struct wld_process wld_process_t;
typedef void (* wld_dmn_restartHandler)(wld_process_t* pProc, void* userdata);

typedef struct {
    bool isStopped;     // process stopped by user
    bool isExited;      // process exited unexpectedly
    int32_t exitStatus; // exit code
    bool isSignaled;    // process ended with signal
    int32_t termSignal; // received signa
} wld_deamonExitInfo_t;

/*
 * @brief handler notifying child process's end of execution
 *
 * @param pProc process context
 * @param userdata private data context pointer registered when starting the process
 */
typedef void (* wld_dmn_onStopHandler)(wld_process_t* pProc, void* userdata);

/*
 * @brief handler notifying child process's started
 *
 * @param pProc process context
 * @param userdata private data context pointer registered when starting the process
 */
typedef void (* wld_dmn_onStartHandler)(wld_process_t* pProc, void* userdata);

/*
 * @brief handler called to build dynamically start arguments, just before starting process
 *
 * @param pProc process context
 * @param userdata private data context pointer registered at initialization call
 * @param newStartArgs string to be allocated and filled with new start args
 *                     If returned NULL, then previous start args will be using
 * @return newly allocate string filled with new start args (freed by the API)
 * If returned NULL, then previous start args will be using
 */
typedef char* (* wld_dmn_getArgsHandler)(wld_process_t* pProc, void* userdata);

/*
 * @brief handler to reload child process in custom way (default SIGHUP)
 *
 * @param pProc process context
 * @param userdata private data context pointer registered when starting the process
 */
typedef bool (* wld_dmn_reloadHandler)(wld_process_t* pProc, void* userdata);

/*
 * @brief handler to terminate child process in custom way (default SIGTERM/SIGKILL)
 *
 * @param pProc process context
 * @param userdata private data context pointer registered when starting the process
 */
typedef bool (* wld_dmn_stopHandler)(wld_process_t* pProc, void* userdata);

typedef struct {
    wld_dmn_restartHandler restartCb; // optional handler to manage child process restarting
    wld_dmn_onStopHandler stopCb;     // optional handler called after child process end
    wld_dmn_onStartHandler startCb;   // optional handler called after child process start
    wld_dmn_getArgsHandler getArgsCb; // optional handler to build process args dynamically just before starting it
    wld_dmn_reloadHandler reload;     // optional handler to reload process configuration
    wld_dmn_stopHandler stop;         // optional handler to terminate process
} wld_deamonEvtHandlers;

/* wld daemon context. */
struct wld_process {
    char* cmd;                         /* Command to run */
    char* args;                        /* arguments as provided by user */
    uint32_t argLen;                   /* length of args */
    char** argList;                    /* argument list to be provided to start API */
    uint32_t nrArgs;                   /* number of arguments */
    bool enabled;                      /* Indicates if daemon is enabled(true) or disabled(false) */
    wld_deamonState_e status;          /* Indicates daemon's current state */
    uint32_t fails;                    /* Amount of fails since last time daemon is set to enabled */
    uint32_t totalFails;               /* Total amount of fails since first time daemon is set to enabled */
    uint32_t restarts;                 /* Amount of restarts since last time daemon is set to enabled */
    uint32_t totalRestarts;            /* Total amount of restarts since first time daemon is set to enabled */
    time_t failDate;                   /* Indicates the last time daemon has been started */
    bool forceKill;                    /* Force -9 option when terminating, as done by some ref software */

    amxp_proc_ctrl_t* process;         /* Process information struct */
    amxp_timer_t* restart_timer;       /* used when a daemon need to be restarted */
    int stdInPeer;                     /* peer for stdIn reading */
    int stdErrPeer;                    /* peer for stdErr reading */

    wld_deamonExitInfo_t lastExitInfo; /* Last process termination details. */
    wld_deamonEvtHandlers handlers;    /* Optional handlers of daemon process events. */
    void* userData;                    /* Optional userdata for process */
    amxc_var_t* settings;              /* internal amxp proc settings. */
};

typedef struct {
    bool enableParam;
    int32_t instantRestartLimit;
    uint32_t minRestartInterval;
} wld_daemonMonitorConf_t;

void wld_dmn_setMonitorConf(wld_daemonMonitorConf_t* pDmnMoniConf);

/**
 * Create and initialize a daemonMonitor object.
 * @param process: IN process context
 * @return : true is successful, false otherwise
 */
bool wld_dmn_initializeDeamon(wld_process_t* process, char* cmd);
bool wld_dmn_setDeamonEvtHandlers(wld_process_t* process, wld_deamonEvtHandlers* pHandlers, void* userData);
void wld_dmn_cleanupDaemon(wld_process_t* process);

/*
 * @brief allocate and initialize a daemon process context
 */
swl_rc_ne wld_dmn_createDeamon(wld_process_t** pDmnProcess, char* cmd, char* startArgs, wld_deamonEvtHandlers* pEvtHdlrs, void* pEvtData);

/*
 * @brief cleanup and free a daemon process context
 */
void wld_dmn_destroyDeamon(wld_process_t** pDmnProcess);

/* Start daemon. */
bool wld_dmn_startDeamon(wld_process_t* dmn_process);
bool wld_dmn_restartDeamon(wld_process_t* dmn_process);
/* Stop daemon. */
bool wld_dmn_stopDeamon(wld_process_t* dmn_process);
bool wld_dmn_stopCb(wld_process_t* p);
/* Reload daemon */
bool wld_dmn_reloadDeamon(wld_process_t* dmn_process);

bool wld_dmn_isRunning(wld_process_t* process);
bool wld_dmn_isEnabled(wld_process_t* process);
bool wld_dmn_isRestarting(wld_process_t* process);

/* init or reinit the daemon arg list , before starting/restarting it. */
void wld_dmn_setArgList(wld_process_t* process, char* args);

/* Always force kill the deamon. Broadcom sometimes requires this to prevent deamon from doing too much side actions */
void wld_dmn_setForceKill(wld_process_t* process, bool forceKill);


#endif /* INCLUDE_WLD_WLD_DAEMON_H_ */
