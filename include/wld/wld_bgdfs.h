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

#ifndef _WLD_BGDFS_H_
#define _WLD_BGDFS_H_

#include "wld_types.h"

typedef struct {
    swl_channel_t channel;
    swl_bandwidth_e bandwidth;
} wld_startBgdfsArgs_t;

typedef enum {
    BGDFS_STATUS_OFF,              // System is not working and turned off
    BGDFS_STATUS_IDLE,             // System is turned on but not clearing
    BGDFS_STATUS_CLEAR,            // System is clearing
    BGDFS_STATUS_CLEAR_EXT,        // System is clearing using external provider
    BGDFS_STATUS_CLEAR_CONTINUOUS, // System is clearing indefinitely
    BGDFS_STATUS_ERROR,            // System clearing in error
    BGDFS_STATUS_MAX
} wld_bgdfsStatus_e;

typedef enum {
    BGDFS_TYPE_CLEAR,             // Finite CAC which ends at the end of time.
    BGDFS_TYPE_CLEAR_EXT,         // same as @BGDFS_TYPE_CLEAR, except that it use an external provider.
    BGDFS_TYPE_CLEAR_CONTINUOUS,  // Indefinitely CAC.
    BGDFS_TYPE_MAX
} wld_bgdfsType_e;

typedef enum {
    DFS_RESULT_OK,    // performed successful dfs clear
    DFS_RESULT_RADAR, // dfs clear failed due to radar
    DFS_RESULT_OTHER, // dfs clear failed due to other reasons
    DFS_RESULT_MAX
} wld_dfsResult_e;

/**
 * Background DFS configuration
 */
typedef struct wld_rad_bgdfs_config {
    /**
     * Availability of the BgDfs feature
     */
    bool available;
    /**
     * Enable BgDfs feature
     */
    bool enable;
    /**
     * Status of the BgDFS (Idle or running)
     */
    wld_bgdfsStatus_e status;
    /**
     * Type of the background DFS running (applicable if status != idle)
     */
    wld_bgdfsType_e type;
    /**
     * Whether to use external provider if available
     */
    bool useProvider;
    /**
     * Channel currently being cleared
     */
    swl_channel_t channel;
    /**
     * The bandwidth currently being cleared
     */
    swl_bandwidth_e bandwidth;
    /**
     * timestamp when clear started
     */
    swl_timeSpecMono_t clearStartTime;
    /**
     * timestamp when clear last finished
     */
    swl_timeSpecMono_t clearEndTime;
    /**
     * The last result
     */
    wld_dfsResult_e lastResult;
    /**
     * Estimated time clear would take
     */
    uint32_t estimatedClearTime;
} wld_rad_bgdfs_config_t;

typedef struct {
    uint32_t nrClearStart[BGDFS_TYPE_MAX];      // Counter for started CAC
    uint32_t nrClearStopQuit[BGDFS_TYPE_MAX];   // Counter for stop CAC (Internal end or stop)
    uint32_t nrClearStopChange[BGDFS_TYPE_MAX]; // Counter for stop CAC (made in purpose, change channel in instance)
    uint32_t nrClearSuccess[BGDFS_TYPE_MAX];    // Counter for Success CAC (cleared channel)
    uint32_t nrClearFailRadar[BGDFS_TYPE_MAX];  // Counter for RADAR detection
    uint32_t nrClearFailOther[BGDFS_TYPE_MAX];  // Counter for other unknown CAC fails
} wld_rad_bgdfs_stats_t;


/**
 * Update the background dfs feature availability
 */
void wld_bgdfs_setAvailable(T_Radio* pRad, bool available);

/**
 * Start a bgdfs clear with given channel and bw, and whether it's external
 */
void wld_bgdfs_notifyClearStarted(T_Radio* pRad, swl_channel_t channel, swl_bandwidth_e bandwidth, wld_bgdfsType_e type);

/**
 * Stop a bgdfs clear
 */
void wld_bgdfs_notifyClearEnded(T_Radio* pRad, wld_dfsResult_e result);

/**
 * Function for other modules to call, to start a bgDfs
 */
swl_rc_ne wld_bgdfs_startExt(T_Radio* pRad, wld_startBgdfsArgs_t* args);

/**
 * Return elapsed clear time
 */
uint32_t wld_bgdfs_clearTimeEllapsed(T_Radio* pRad);

/**
 * Returns whether a background dfs clear is currently happening on this radio
 */
bool wld_bgdfs_isRunning(T_Radio* pRad);

/* Sync datamodel object with local values */
void wld_bgdfs_update(T_Radio* pRad, amxd_trans_t* trans);

#endif /* _WLD_BGDFS_H_ */
