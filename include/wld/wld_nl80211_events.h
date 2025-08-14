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
/*
 * This file includes nl80211 event callbacks definitions
 */

#ifndef INCLUDE_WLD_WLD_NL80211_EVENTS_H_
#define INCLUDE_WLD_WLD_NL80211_EVENTS_H_

#include "wld_nl80211_core.h"
#include "wld_nl80211_types.h"

/*
 * @brief dynamic checker of nl80211 event target listener
 *
 * @param pRef user private reference provided when registering handlers
 * @param pData user private data provided when registering handlers
 * @param wiphy: wiphy id read from received event (WLD_NL80211_ID_UNDEF if not found)
 * @param ifIndex: iface net dev index read from received event (WLD_NL80211_ID_UNDEF if not found)
 *
 * @return void
 *
 */
typedef bool (* wld_nl80211_checkTgtCb_f)(void* pRef, void* pData, int32_t wiphy, int32_t ifIndex, uint32_t ctrlFreq, const char* ifName);

/*
 * @brief generic callback for nl80211 event
 *
 * @param pRef user private reference provided when registering handlers
 * @param pData user private data provided when registering handlers
 * @param wiphy: wiphy id read from received event (WLD_NL80211_ID_UNDEF if not found)
 * @param ifIndex: iface net dev index read from received event (WLD_NL80211_ID_UNDEF if not found)
 *
 * @return void
 *
 */
typedef void (* wld_nl80211_genIfaceEvtCb_f)(void* pRef, void* pData, uint32_t wiphy, uint32_t ifIndex);

/*
 * @brief callback for created/deleted wiphy device
 *
 * @param pRef user private reference provided when registering handlers
 * @param pData user private data provided when registering handlers
 * @param pWiphyInfo: pointer to wiphy device info structure
 *
 * @return void
 *
 */
typedef void (* wld_nl80211_wiphyInfoEvtCb_f)(void* pRef, void* pData, wld_nl80211_wiphyInfo_t* pWiphyInfo);

/*
 * @brief callback for created/deleted virtual interface notif
 *
 * @param pRef user private reference provided when registering handlers
 * @param pData user private data provided when registering handlers
 * @param pIfaceInfo: pointer to interface info structure
 *
 * @return void
 *
 */
typedef void (* wld_nl80211_ifaceInfoEvtCb_f)(void* pRef, void* pData, wld_nl80211_ifaceInfo_t* pIfaceInfo);


/*
 * @brief generic callback for nl80211 event
 *
 * @param pRef user private reference provided when registering handlers
 * @param pData user private data provided when registering handlers
 * @param nlh pointer to netlink msg header
 * @param tb pointer to the entire data bloc
 *
 * @return void
 *
 */
typedef void (* wld_nl80211_vendorEvtCb_f)(void* pRef, void* pData, struct nlmsghdr* nlh, struct nlattr* tb[]);

/*
 * @brief generic mgmt frame callback
 *
 * @param pRef user private reference provided when registering handlers
 * @param pData user private data provided when registering handlers
 * @param frameLen the size of the mgmt frame
 * @param frame pointer to a mgmt frame
 * @param frameRssi the RSSI of the mgmt frame
 *
 * @return void
 *
 */
typedef void (* wld_nl80211_mgmtFrameEvtCb_f)(void* pRef, void* pData, size_t frameLen, swl_80211_mgmtFrame_t* frame, int32_t frameRssi);

/*
 * @brief generic transmitted mgmt frame status callback
 *
 * @param pRef user private reference provided when registering handlers
 * @param pData user private data provided when registering handlers
 * @param frameLen the size of the transmitted mgmt frame
 * @param frame pointer to the transmitted mgmt frame
 * @param ack indicating whether the transmitted frame was acknowledged by the recipient
 *
 * @return void
 *
 */
typedef void (* wld_nl80211_mgmtFrameTxStatusEvtCb_f)(void* pRef, void* pData, size_t frameLen, swl_80211_mgmtFrame_t* frame, bool isAck);

/*
 * @brief generic dfs/radar event callback
 *
 * @param pRef user private reference provided when registering handlers
 * @param pData user private data provided when registering handlers
 * @param dfsEvtInfo the pointer to the radar event info
 *
 * @return void
 *
 */
typedef void (* wld_nl80211_radarEvtCb_f)(void* pRef, void* pData, wld_nl80211_radarEvtInfo_t* dfsEvtInfo);

/*
 * @brief callback for changed reg domain info
 *
 * @param pRef user private reference provided when registering handlers
 * @param pData user private data provided when registering handlers
 * @param pRegChangeInfo: pointer to reg domain change info struct
 *
 * @return void
 *
 */
typedef void (* wld_nl80211_regChangedEvtCb_f)(void* pRef, void* pData, wld_nl80211_regChangeInfo_t* pRegChangeInfo);

/*
 * @brief structure of event handlers
 */
typedef struct {
    wld_nl80211_genIfaceEvtCb_f fUnspecEvtCb;                     // unspecified event is never sent by kernel: only defined for test purpose
    wld_nl80211_ifaceInfoEvtCb_f fNewInterfaceCb;                 // created virtual interface
    wld_nl80211_ifaceInfoEvtCb_f fDelInterfaceCb;                 // deleted virtual interface
    wld_nl80211_genIfaceEvtCb_f fScanStartedCb;                   // scan is started successfully and running
    wld_nl80211_genIfaceEvtCb_f fScanAbortedCb;                   // running scan was aborted
    wld_nl80211_genIfaceEvtCb_f fScanDoneCb;                      // scan is terminated and results can be retrieved
    wld_nl80211_vendorEvtCb_f fVendorEvtCb;                       // vendor event is reported
    wld_nl80211_mgmtFrameEvtCb_f fMgtFrameEvtCb;                  // a management frame is reported
    wld_nl80211_mgmtFrameTxStatusEvtCb_f fMgtFrameTxStatusEvtCb;  // status of transmitted management frame is reported
    wld_nl80211_radarEvtCb_f fRadarEventCb;                       // DFS/radar event is reported
    wld_nl80211_checkTgtCb_f fCheckTgtCb;                         // handler to check target listener
    wld_nl80211_wiphyInfoEvtCb_f fNewWiphyCb;                     // created wiphy device
    wld_nl80211_wiphyInfoEvtCb_f fDelWiphyCb;                     // deleted wiphy device
    wld_nl80211_regChangedEvtCb_f fRegChangedCb;                  // changed regulatory info
} wld_nl80211_evtHandlers_cb;

/*
 * @brief create a listener for a set of nl80211 events, received from a selection of wiphy/iface.
 * Received events are notified to the listener when:
 * - listener has a dedicated valid handler function (i.e not null)
 * - source wiphy/ifIndex match with the listener filter
 * Wildcards (WLD_NL80211_ID_ANY) can be used to create a default wiphy or a global event listener.
 * Exclusion (WLD_NL80211_ID_UNDEF) can be used to ignore events where expected source (wiphy or iface) is unknown.
 * When multiple listeners are matching a received event+wiphy/ifIndex, they are invoked in this defined order:
 * 1) first, the specific iface listeners
 * 2) then, the default wiphy listeners (any iface)
 * 3) last, the global listeners (any wiphy)
 *
 * @param state pointer to nl80211 socket manager, on which socket the event would be received
 * @param wiphy Index of wiphy to listen for it (>=0):
 *              WLD_NL80211_ID_ANY for any wiphy
 *              WLD_NL80211_ID_UNDEF to exclude events where wiphy is included
 * @param ifIndex Net dev index to listen for it (>0)
 *                WLD_NL80211_ID_ANY for any nl80211 interface
 *                WLD_NL80211_ID_UNDEF to exclude events where ifIndex is included
 * @param pRef user private reference to provide when invoking handlers
 * @param pData user private data to provide when invoking handlers
 * @param handlers Pointer to structure of callback functions, handling the received events
 *                 Callbacks of unmanaged events shall remain null.
 *
 * @return pointer to created listener
 *         It can be deleted by caller or implicitly when state is cleaned up.
 */
wld_nl80211_listener_t* wld_nl80211_addEvtListener(wld_nl80211_state_t* state,
                                                   uint32_t wiphy, uint32_t ifIndex,
                                                   void* pRef, void* pData,
                                                   const wld_nl80211_evtHandlers_cb* const handlers);

/*
 * @brief create a global listener for all received nl80211 events from any wiphy/iface.
 *
 * @param state pointer to nl80211 socket manager, on which socket the event would be received
 * @param pRef user private reference to provide when invoking handlers
 * @param pData user private data to provide when invoking handlers
 * @param handlers Pointer to structure of callback functions, handling the received events
 *                 Callbacks of unmanaged events shall remain null.
 *
 * @return pointer to created listener
 *         It can be deleted by caller or implicitly when state is cleaned up.
 */
wld_nl80211_listener_t* wld_nl80211_addGlobalEvtListener(wld_nl80211_state_t* state,
                                                         void* pRef, void* pData,
                                                         const wld_nl80211_evtHandlers_cb* const handlers);

/*
 * @brief delete an existing listener
 *
 * @param ppListener listener to be fetched and freed
 *
 * @return SWL_RC_OK on success (ppListener is reset to null)
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_nl80211_delEvtListener(wld_nl80211_listener_t** ppListener);

/*
 * @brief get internal data from listener
 *
 * @param pListener current listener use to get internal data
 * @param pData the user data saved inside pListener.
 * @param pHandlers the evt handler saved inside pListener.

 * @return true on success
 *         false otherwise
 */
bool wld_nl80211_getEvtListenerHandlers(wld_nl80211_listener_t* pListener, void** pData, wld_nl80211_evtHandlers_cb* pHandlers);

/*
 * @brief create a listener for vendor reported events, received from a selection of wiphy/iface.
 * Received events are notified to the listener when:
 * - listener has a dedicated valid handler function (i.e not null)
 * - source wiphy/ifIndex match with the listener filter
 *
 * @param state pointer to nl80211 socket manager, on which socket the event would be received
 * @param pListener pointer to listener context
 * @param handler Pointer to vendor events callback function, handling the received events
 *
 * @return pointer to updated listener
 */
wld_nl80211_listener_t* wld_nl80211_addVendorEvtListener(wld_nl80211_state_t* state,
                                                         wld_nl80211_listener_t* pListener,
                                                         wld_nl80211_vendorEvtCb_f handler);

#endif /* INCLUDE_WLD_WLD_NL80211_EVENTS_H_ */
