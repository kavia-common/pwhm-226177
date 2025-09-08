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

#ifndef __WLD_WPA_CTRL_EVENTS_H__
#define __WLD_WPA_CTRL_EVENTS_H__

#include "swla/swla_chanspec.h"
#include "swla/swla_mac.h"
#include "swl/swl_ieee802_1x_defs.h"
#include "swl/swl_80211.h"
#include "swl/swl_returnCode.h"
#include "wld_wpaCtrl_types.h"

#define WPA_MSG_LEVEL_INFO "<3>"

typedef struct {
    uint8_t opclass;
    uint8_t channel;
    uint64_t startTime;
    uint16_t duration;
    uint8_t frameInfo;
    uint8_t rcpi;
    uint8_t rsni;
    uint8_t bssid[6];
    uint8_t antennaId;
    uint32_t parentTsf;
} __attribute__((packed)) wld_wpaCtrl_rrmBeaconRsp_t;

/*
 * @brief Handler to prepare custom evt msg processing:
 * by fetching the real source interface and/or modify the received msg data to allow a generic processing.
 * When needed, newInterfaceName and newMsgData are allocated and filled
 * inside the handler, and they are freed on pwhm side
 * This handler allows to redirect custom wpactrl messages received on a global wpaCtrl socket
 * but targeted to a different interface
 *
 * @param userData private user data context registered with the wpactrl iface socket
 * @param ifName name of wpactrl iface receiving the event
 * @param msgData original text of the received msg
 * @param len original length of received msg
 * @param newIfName pointer to string allocated and filled with the interface name fetched in the msg
 * @param newMsgData pointer to string allocated and filled with the prepared msg for processing
 * @param newLen pointer to length variable set with the length of the prepared msg
 *
 * @return SWL_RC_DONE if the msg or the interface has been modified
 *         SWL_RC_OK if no custom changes has been done on target interface or received msg
 *         error code otherwise
 * @return void
 */
typedef swl_rc_ne (* wld_wpaCtrl_preProcMsgCb_f)(void* userData, char* ifName, char* msgData, size_t len, char** newIfName, char** newMsgData, size_t* newLen);

/*
 * @brief handler for custom processing of received wpa ctrl event
 *
 * @param userData private user data context registered with the wpactrl iface socket
 * @param ifName name of wpactrl iface receiving the event
 * @param msgData text of the received event
 *
 * @return SWL_RC_DONE if the msg has been completely processed as custom event (eg: vendor specific format)
 *                     and no more need to handle it as generic
 *         SWL_RC_OK or SWL_RC_CONTINUE if the msg has been partially handled
 *                     but can still be more processed
 *         error code otherwise
 */
typedef swl_rc_ne (* wld_wpaCtrl_custProcMsgCb_f)(void* userData, char* ifName, char* msgData);

/*
 * @brief Handler for post msg processing
 * It is called after a custom or generic msg parsing and handling has been tried
 */
typedef void (* wld_wpaCtrl_processMsgCb_f)(void* userData, char* ifName, char* msgData);

/*
 * @brief Handler for post standard event processing
 * It is called after a generic msg parsing and handling has been tried
 */
typedef void (* wld_wpaCtrl_processStdMsgCb_f)(void* userData, char* ifName, char* eventName, char* msgData);

/*
 * wpactrl event handlers called by a generic wpactrl msg parsing
 */
typedef void (* wld_wpaCtrl_wpsInProgressMsg_f)(void* userData, char* ifName);
typedef void (* wld_wpaCtrl_wpsSuccessMsg_f)(void* userData, char* ifName, swl_macChar_t* mac);
typedef void (* wld_wpaCtrl_wpsTimeoutMsg_f)(void* userData, char* ifName);
typedef void (* wld_wpaCtrl_wpsCancelMsg_f)(void* userData, char* ifName);
typedef void (* wld_wpaCtrl_wpsOverlapMsg_f)(void* userData, char* ifName);
typedef void (* wld_wpaCtrl_wpsFailMsg_f)(void* userData, char* ifName);
typedef void (* wld_wpaCtrl_wpsCredReceivedMsg_f)(void* userData, char* ifName, void* creds, swl_rc_ne status);
typedef void (* wld_wpaCtrl_wpsSetupLockedMsg_f)(void* userData, char* ifName, bool setupLocked);
typedef void (* wld_wpaCtrl_apStationEapCompletedCb_f)(void* userData, char* ifName, swl_macBin_t* bStationMac);
typedef void (* wld_wpaCtrl_apStationConnectivityCb_f)(void* userData, char* ifName, swl_macBin_t* bStationMac);
typedef void (* wld_wpaCtrl_apStationAssocFailureCb_f)(void* userData, char* ifName, swl_macBin_t* bStationMac);
typedef void (* wld_wpaCtrl_wdsIfaceAddedCb_f)(void* userData, char* ifName, char* wdsName, swl_macBin_t* bStationMac);
typedef void (* wld_wpaCtrl_wdsIfaceRemovedCb_f)(void* userData, char* ifName, char* wdsName, swl_macBin_t* bStationMac);
typedef void (* wld_wpaCtrl_btmReplyCb_f)(void* userData, char* ifName, swl_macChar_t* mac, uint8_t status, swl_macChar_t* targetBssid);
typedef void (* wld_wpaCtrl_mgtFrameReceivedCb_f)(void* userData, char* ifName, swl_80211_mgmtFrame_t* frame, size_t frameLen, char* frameStr);
typedef void (* wld_wpaCtrl_stationConnectivityCb_f)(void* userData, char* ifName, swl_macBin_t* bBssidMac, swl_IEEE80211deauthReason_ne reason);
typedef void (* wld_wpaCtrl_stationScanFailedCb_f)(void* userData, char* ifName, int error);
typedef void (* wld_wpaCtrl_beaconResponseCb_f)(void* userData, char* ifName, swl_macBin_t* station, wld_wpaCtrl_rrmBeaconRsp_t* rrmBeaconResponse);
typedef void (* wld_wpaCtrl_stationStartConnCb_f)(void* userData, char* ifName, const char* ssid, swl_macBin_t* bBssidMac, swl_chanspec_t* pChansSpec);
typedef void (* wld_wpaCtrl_stationStartConnFailedCb_f)(void* userData, char* ifName, int error);
typedef void (* wld_wpaCtrl_apIfaceEventCb_f)(void* userData, char* ifName);
typedef void (* wld_wpaCtrl_stationMultiApInfoCb_f)(void* userData, char* ifName, uint8_t profileId, uint16_t primVlanId);

/*
 * handler to select MLD iface (primary link) name of a provided SSID (AP/STA link) interface
 * pLinkIfName is allocated inside the handler and must be freed by the caller
 */
typedef void (* wld_wpaCtrl_selectPrimLinkIface_f)(void* userData, const char* ifName, char** pPrimLinkIfName);

/*
 * @brief structure of AP/EP event handlers
 */
typedef struct {
    wld_wpaCtrl_preProcMsgCb_f fPreProcEvtMsg;    // Handler to prepare custom msg processing (redirect, alter)
    wld_wpaCtrl_custProcMsgCb_f fCustProcEvtMsg;  // Handler for custom msg processing (filter)
    wld_wpaCtrl_processMsgCb_f fProcEvtMsg;       // Handler to post-process evt msg
    wld_wpaCtrl_processStdMsgCb_f fProcStdEvtMsg; // Basic handler of standard received wpa ctrl event
    wld_wpaCtrl_wpsInProgressMsg_f fWpsInProgressMsg;
    wld_wpaCtrl_wpsSuccessMsg_f fWpsSuccessMsg;
    wld_wpaCtrl_wpsTimeoutMsg_f fWpsTimeoutMsg;
    wld_wpaCtrl_wpsCancelMsg_f fWpsCancelMsg;
    wld_wpaCtrl_wpsOverlapMsg_f fWpsOverlapMsg;          /** WPS overlap detected in PBC mode */
    wld_wpaCtrl_wpsFailMsg_f fWpsFailMsg;                /** WPS registration failed after M2/M2D */
    wld_wpaCtrl_wpsCredReceivedMsg_f fWpsCredReceivedCb; /** WPS credentials received */
    wld_wpaCtrl_wpsSetupLockedMsg_f fWpsSetupLockedMsg;  /** WPS AP-SETUP-LOCKED/AP-SETUP-UNLOCKED received*/
    wld_wpaCtrl_apStationEapCompletedCb_f fApStationEapCompletedCb;
    wld_wpaCtrl_apStationConnectivityCb_f fApStationConnectedCb;
    wld_wpaCtrl_apStationConnectivityCb_f fApStationDisconnectedCb;
    wld_wpaCtrl_apStationAssocFailureCb_f fApStationAssocFailureCb;
    wld_wpaCtrl_wdsIfaceAddedCb_f fWdsIfaceAddedCb;
    wld_wpaCtrl_wdsIfaceRemovedCb_f fWdsIfaceRemovedCb;
    wld_wpaCtrl_btmReplyCb_f fBtmReplyCb;
    wld_wpaCtrl_mgtFrameReceivedCb_f fMgtFrameReceivedCb;
    wld_wpaCtrl_stationConnectivityCb_f fStationAssociatedCb;
    wld_wpaCtrl_stationConnectivityCb_f fStationDisconnectedCb;
    wld_wpaCtrl_stationConnectivityCb_f fStationConnectedCb;
    wld_wpaCtrl_stationScanFailedCb_f fStationScanFailedCb;
    wld_wpaCtrl_stationStartConnCb_f fStationStartConnCb;             // Endpoint starting connection
    wld_wpaCtrl_stationStartConnFailedCb_f fStationStartConnFailedCb; // Endpoint connection init failure
    wld_wpaCtrl_beaconResponseCb_f fBeaconResponseCb;
    wld_wpaCtrl_apIfaceEventCb_f fApEnabledCb;
    wld_wpaCtrl_apIfaceEventCb_f fApDisabledCb;
    wld_wpaCtrl_stationMultiApInfoCb_f fStationMultiApInfoCb;

    /*
     * AP/EP sync handlers:
     * may be used to prepare/schedule fsm actions, or post events processing
     * (from hostapd/wpa_supplicant)
     */
    wld_wpaCtrl_selectPrimLinkIface_f fSelectPrimLinkIface; // Handler to select primary link iface name for SSID iface

} wld_wpaCtrl_evtHandlers_cb;

typedef void (* wld_wpaCtrl_chanSwitchStartedCb_f)(void* userData, char* ifName, swl_chanspec_t* chanSpec);
typedef void (* wld_wpaCtrl_chanSwitchCb_f)(void* userData, char* ifName, swl_chanspec_t* chanSpec);
typedef void (* wld_wpaCtrl_apCsaFinishedCb_f)(void* userData, char* ifName, swl_chanspec_t* chanSpec);
typedef void (* wld_wpaCtrl_mngrReadyCb_f)(void* userData, char* ifName, bool isReady);
typedef void (* wld_wpaCtrl_dfsCacStartedCb_f)(void* userData, char* ifName, swl_chanspec_t* chanSpec, uint32_t cac_time);
typedef void (* wld_wpaCtrl_dfsCacDoneCb_f)(void* userData, char* ifName, swl_chanspec_t* chanSpec, bool success);
typedef void (* wld_wpaCtrl_dfsCacExpiredCb_f)(void* userData, char* ifName, swl_chanspec_t* chanSpec);
typedef void (* wld_wpaCtrl_dfsRadarDetectedCb_f)(void* userData, char* ifName, swl_chanspec_t* chanSpec);
typedef void (* wld_wpaCtrl_dfsNopFinishedCb_f)(void* userData, char* ifName, swl_chanspec_t* chanSpec);
typedef void (* wld_wpaCtrl_dfsNewChannelCb_f)(void* userData, char* ifName, swl_chanspec_t* chanSpec);
typedef void (* wld_wpaCtrl_mainApSetupCompletedCb_f)(void* userData, char* ifName);
typedef void (* wld_wpaCtrl_mainApDisabledCb_f)(void* userData, char* ifName);
typedef void (* wld_wpaCtrl_syncOnReadyCb_f)(void* userData, char* ifName, bool state);

/*
 * handler to fetch MLD Link iface name from wpa ctrl socket name
 * pLinkIfName is allocated inside the handler and shall be freed by the caller
 */
typedef void (* wld_wpaCtrl_fetchSocketLinkIface_f)(void* userData, wld_wpaCtrlMngr_t* pMgr, const char* sockName, char** pLinkIfName);

/*
 * @brief structure of Radio event handlers
 */
typedef struct {
    wld_wpaCtrl_custProcMsgCb_f fCustProcEvtMsg;  // Handler for custom msg processing (filter)
    wld_wpaCtrl_processMsgCb_f fProcEvtMsg;       // Handler to post process evt msg
    wld_wpaCtrl_processStdMsgCb_f fProcStdEvtMsg; // Basic handler of standard received wpa ctrl event
    wld_wpaCtrl_chanSwitchStartedCb_f fChanSwitchStartedCb;
    wld_wpaCtrl_chanSwitchCb_f fChanSwitchCb;
    wld_wpaCtrl_apCsaFinishedCb_f fApCsaFinishedCb;
    wld_wpaCtrl_mngrReadyCb_f fMngrReadyCb;
    wld_wpaCtrl_dfsCacStartedCb_f fDfsCacStartedCb;
    wld_wpaCtrl_dfsCacDoneCb_f fDfsCacDoneCb;
    wld_wpaCtrl_dfsCacExpiredCb_f fDfsCacExpiredCb;
    wld_wpaCtrl_dfsRadarDetectedCb_f fDfsRadarDetectedCb;
    wld_wpaCtrl_dfsNopFinishedCb_f fDfsNopFinishedCb;
    wld_wpaCtrl_dfsNewChannelCb_f fDfsNewChannelCb; //fallback after dfs radar detection
    wld_wpaCtrl_mainApSetupCompletedCb_f fMainApSetupCompletedCb;
    wld_wpaCtrl_mainApDisabledCb_f fMainApDisabledCb;

    /*
     * radio sync handlers:
     * may be used to schedule fsm actions, post events
     * (from hostapd/wpa_supplicant)
     */
    wld_wpaCtrl_syncOnReadyCb_f fSyncOnRadioUp;                       // Handler to sync ifaces when radio is going up (ie mgr ready and APMain is(/being) enabled)
    wld_wpaCtrl_syncOnReadyCb_f fSyncOnEpConnected;                   // Handler to sync radio conf when endpoint is connected
    wld_wpaCtrl_syncOnReadyCb_f fSyncOnEpDisconnected;                // Handler to sync radio conf when endpoint is disconnected
    wld_wpaCtrl_fetchSocketLinkIface_f fFetchSockLinkIface;           // Handler to fetch of link iface name from wpa socket file name
    wld_wpaCtrl_stationScanFailedCb_f fStationScanFailedCb;           // EndPoint failing to scan BSS
    wld_wpaCtrl_stationStartConnCb_f fStationStartConnCb;             // Endpoint starting connection
    wld_wpaCtrl_stationStartConnFailedCb_f fStationStartConnFailedCb; // Endpoint connection init failure
    wld_wpaCtrl_wpsTimeoutMsg_f fWpsTimeoutMsg;                       // WPS Timeouting
    wld_wpaCtrl_wpsCancelMsg_f fWpsCancelMsg;                         // WPS Canceled
    wld_wpaCtrl_wpsOverlapMsg_f fWpsOverlapMsg;                       // WPS Overlap
} wld_wpaCtrl_radioEvtHandlers_cb;

int wld_wpaCtrl_getValueStr(const char* pData, const char* pKey, char* pValue, int length);
int wld_wpaCtrl_getValueInt(const char* pData, const char* pKey);
bool wld_wpaCtrl_getValueIntExt(const char* pData, const char* pKey, int32_t* pVal);


/*
 * @brief parse a wpactrl msg and fetch the event name from a provided list
 * then copy the event name (delimited with separator), and the argument list (starting with separator)
 * The event name is potentially prefixed.
 * The expected message format is "[PREFIX]<EVENT_NAME><separator><EVENT_ARGS...>"
 *
 * @param pData wp ctrl message
 * @param prefix optional string prefixing the event name (typically the loglevel id , eg: <3>)
 * @param separator optional string (or char) separating event name and next event arguments
 * @param evtList optional array of event name strings to fetch in priority, before splitting msg with separator
 * @param evtListLen length of optional array of known event name strings
 * @param pEvtName pointer to output string allocated and filled with the event name (to be freed by the caller)
 * @param pEvtArgs pointer to output string allocated and filled with the event arguments (to be freed by the caller)
 *
 * @return -1,  when event name is unknown or not found
 *         index < evtListLen, when event name is found in the provided list
 */
int32_t wld_wpaCtrl_fetchEvent(const char* pData, const char* prefix, const char* sep, char* evtList[], size_t evtListLen, char** pEvtName, char** pEvtArgs);

/*
 * @brief parse a wpactrl msg and copy the event name, and the argument list
 * The event name is potentially prefixed.
 * The expected message format is "[PREFIX]<EVENT_NAME><separator><EVENT_ARGS...>"
 *
 * @param pData wp ctrl message
 * @param prefix optional string prefixing the event name (typically the loglevel id , eg: <3>)
 * @param separator optional string (or char) separating event name and next event arguments
 * @param pEvtName pointer to output string allocated and filled with the event name (to be freed by the caller)
 * @param pEvtArgs pointer to output string allocated and filled with the event arguments (to be freed by the caller)
 *
 * @return true when event name is found, false otherwise
 */
bool wld_wpaCtrl_parseMsg(const char* pData, const char* prefix, const char* sep, char** pEvtName, char** pEvtArgs);

/*
 * @brief public api to convert channel frequency from wpactrl msg (from hostapd/wpasupp) into swl chanspec struct
 * param key string may vary with events
 */
swl_rc_ne wld_wpaCtrl_freqParamToChanSpec(char* params, const char* key, swl_chanspec_t* pChanSpec);

/*
 * @brief public api to convert channel width description from wpactrl msg (from hostapd/wpasupp) into swl chanspec struct
 * param key string may vary with events
 */
swl_rc_ne wld_wpaCtrl_chWidthDescToChanSpec(char* params, const char* key, swl_chanspec_t* pChanSpec);

/*
 * @brief public api to convert channel width ID from wpactrl msg (from hostapd/wpasupp) into swl chanspec struct
 * param key string may vary with events
 */
swl_rc_ne wld_wpaCtrl_chWidthIdToChanSpec(char* params, const char* key, swl_chanspec_t* pChanSpec);

#endif /* __WLD_WPA_CTRL_EVENTS_H__ */
