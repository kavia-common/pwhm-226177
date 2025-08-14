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

#ifndef __WLD_NL80211_CORE_H__
#define __WLD_NL80211_CORE_H__

#include "swl/swl_returnCode.h"

/*
 * @brief private structure defining nl80211 socket manager
 */
struct wld_nl80211_state;
typedef struct wld_nl80211_state wld_nl80211_state_t;

/*
 * @brief private structure defining nl80211 event listener
 */
struct wld_nl80211_listener;
typedef struct wld_nl80211_listener wld_nl80211_listener_t;

/*
 * @brief abstract types of common nl80211 structures used in public APIs
 * (detailed definitions in wld_nl80211_types.h)
 */
typedef struct wld_nl80211_channelSurveyInfo wld_nl80211_channelSurveyInfo_t;
typedef struct wld_nl80211_channelSurveyParam wld_nl80211_channelSurveyParam_t;

/*
 * @brief statistic counters of nl80211 socket manager
 */
typedef struct {
    uint32_t reqTotal;     //count of all sent requests
    uint32_t reqSuccess;   //count of requests that have been applied successfully
    uint32_t reqFailed;    //count of requests that failed (not sent, erroneous, expired)
    uint32_t reqExpired;   //count of requests that expired (timeouted with being totally answered)
    uint32_t reqPending;   //count of requests still waiting for answer
    uint32_t evtTotal;     //count of received events
    uint32_t evtHandled;   //count of events parsed and handled by a listener
    uint32_t evtUnhandled; //count of events unknown (i.e not parsed) or with no listener
    uint32_t nStates;      //count of running managers
} wld_nl80211_stateCounters_t;

/*
 * @brief struct including nl80211 driver IDs
 * (learned at runtime from kernel)
 */
typedef struct {
    int32_t family_id;       //nl80211 family id
    int32_t scan_mcgrp_id;   //nl80211 SCAN multicast group id
    int32_t config_mcgrp_id; //nl80211 CONFIG multicast group id
    int32_t mlme_mcgrp_id;   //nl80211 MLME multicast group id
    int32_t vendor_grp_id;   //nl80211 vendor evt group id
    int32_t reg_grp_id;      //nl80211 regulatory group id
} wld_nl80211_driverIds_t;

/*
 * @brief create nl80211 socket context
 * and add the socket fd to the event loop
 *
 * It is recommended to create one context per application
 * as it manages requests and events over it
 * but can also be created per radio and per VAP
 *
 * @return pointer to allocated nl80211 socket manager
 *         null in case of failure
 */
wld_nl80211_state_t* wld_nl80211_newState();

/*
 * @brief return shared nl80211 socket manager
 * If not available, it is created by the way.
 */
wld_nl80211_state_t* wld_nl80211_getSharedState();

/*
 * @brief clear and free nl80211 socket manager context
 * It also clears all requests and listeners registered on it.
 *
 * @param state pointer to nl80211 manager context
 *
 * @return void
 */
void wld_nl80211_cleanup(wld_nl80211_state_t* state);

/*
 * @brief clear and free all created nl80211 socket managers
 *
 * @return void
 */
void wld_nl80211_cleanupAll();

/*
 * @brief get socket manager statistics
 *
 * @param state pointer to nl80211 manager context
 * @param pCounters pointer to statistic counters
 *
 * @return SWL_RC_OK if successful
 *         SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_nl80211_getStateCounters(wld_nl80211_state_t* state, wld_nl80211_stateCounters_t* pCounters);

/*
 * @brief get global statistics (sum of all managers counters)
 *
 * @param pCounters pointer to statistic counters
 *
 * @return SWL_RC_OK if successful
 *         SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_nl80211_getAllCounters(wld_nl80211_stateCounters_t* pCounters);

/*
 * @brief provides the nl80211 driver IDs
 * learned at runtime from kernel
 *
 * @return const pointer to static struct including IDs
 */
const wld_nl80211_driverIds_t* wld_nl80211_getDriverIds();

#endif /* __WLD_NL80211_CORE_H__ */
