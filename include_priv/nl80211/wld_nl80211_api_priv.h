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

#ifndef INCLUDE_PRIV_NL80211_WLD_NL80211_API_PRIV_H_
#define INCLUDE_PRIV_NL80211_WLD_NL80211_API_PRIV_H_

#include "wld_nl80211_attr.h"
#include "wld_nl80211_core_priv.h"
#include "swl/swl_common.h"
#include "swl/swl_unLiList.h"

// maximum request processing time , in seconds.
// At this limit, all reply msg's fragments shall be received and processed
// 10 sec for async requests
// 5 sec for sync requests
#define REQUEST_SYNC_TIMEOUT  5  //shorter timeout as sync requests are blocking
#define REQUEST_ASYNC_TIMEOUT 10 //longer timeout: shall be enough

/*
 * @brief recursive function to add simple and nested nl attributes
 * to nl message
 *
 * @param msg pointer to netlink message
 * @param pAttrList pointer to netlink attribute list
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_nl80211_addNlAttrs(struct nl_msg* msg, wld_nl80211_nlAttrList_t* const pAttrList);

/*
 * @brief common function to send request
 *
 * @param isSync flag to send sync/async request
 * @param state nl80211 socket manager context
 * @param cmd nl80211 command to send
 * @param flags optional nl80211 msg flags
 * @param ifIndex interface net dev index (ignored if ifIndex is null)
 * @param pAttrList netlink attribute list to add to message
 * @param handler callback invoked when a reply is available
 * @param priv private data to pass in to the handler
 * @param pResult (optional) variable where to save current request status
 *
 * @return SWL_RC_OK on success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_nl80211_sendCmd(bool isSync, wld_nl80211_state_t* state, uint32_t cmd, uint32_t flags,
                              uint32_t ifIndex, wld_nl80211_nlAttrList_t* const pAttrList,
                              wld_nl80211_handler_f handler, void* priv, swl_rc_ne* pResult);

/*
 * @brief common function to send synchronous request
 *
 * @param state nl80211 socket manager context
 * @param cmd nl80211 command to send
 * @param flags optional nl80211 msg flags
 * @param ifIndex interface net dev index (ignored if ifIndex is null)
 * @param pAttrList netlink attribute list to add to message
 * @param handler callback invoked when a reply is available
 * @param priv private data to pass in to the handler
 *
 * @return SWL_RC_OK on success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_nl80211_sendCmdSync(wld_nl80211_state_t* state, uint32_t cmd, uint32_t flags,
                                  uint32_t ifIndex, wld_nl80211_nlAttrList_t* const pAttrList,
                                  wld_nl80211_handler_f handler, void* priv);

/*
 * @brief common function to send synchronous request
 * and wait for acknowledgment
 *
 * @param state nl80211 socket manager context
 * @param cmd nl80211 command to send
 * @param flags optional nl80211 msg flags
 * @param ifIndex interface net dev index (ignored if ifIndex is null)
 * @param pAttrList netlink attribute list to add to message
 *
 * @return SWL_RC_OK on success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_nl80211_sendCmdSyncWithAck(wld_nl80211_state_t* state, uint32_t cmd, uint32_t flags,
                                         uint32_t ifIndex, wld_nl80211_nlAttrList_t* const pAttrList);

/*
 * @brief common function to send asynchronous request
 * and handle differed acknowledgment
 *
 * @param state nl80211 socket manager context
 * @param cmd nl80211 command to send
 * @param flags optional nl80211 msg flags
 * @param ifIndex interface net dev index (ignored if ifIndex is null)
 * @param pAttrList netlink attribute list to add to message
 *
 * @return SWL_RC_OK on success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_nl80211_sendCmdAsyncWithAck(wld_nl80211_state_t* state, uint32_t cmd, uint32_t flags,
                                          uint32_t ifIndex, wld_nl80211_nlAttrList_t* const pAttrList);

/*
 * @brief cleans up expired requests of all socket managers
 *
 * @return void
 */
void wld_nl80211_clearAllExpiredRequests();

#endif /* INCLUDE_PRIV_NL80211_WLD_NL80211_API_PRIV_H_ */
