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
 * This file defines wrapper functions to use nl80211 generic apis with T_Radio context
 */

#ifndef INCLUDE_WLD_WLD_RAD_NL80211_H_
#define INCLUDE_WLD_WLD_RAD_NL80211_H_

#include "wld.h"
#include "wld_nl80211_api.h"
#include "wld_nl80211_events.h"
#include "wld_nl80211_debug.h"

/*
 * @brief set a listener for all received nl80211 events related to a radio device.
 *
 * @param pRadio pointer to radio context
 * @param pData user private data to provide when invoking handlers
 * @param handlers Pointer to structure of callback functions, handling the received events
 *                 Callbacks of unmanaged events shall remain null.
 *                 Handlers will be called with pRef equal to pRadio.
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_setEvtListener(T_Radio* pRadio, void* pData, const wld_nl80211_evtHandlers_cb* const handlers);

/*
 * @brief delete listener of radio device's nl80211 events
 *
 * @param pRadio pointer to radio context
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_delEvtListener(T_Radio* pRadio);

/*
 * @brief get info of main radio interface (AP/EP)
 * This includes some :
 * - physical info like current channel and tx power
 * - logical info: interface name, mac, type, ssid
 *
 * @param pRadio pointer to radio context
 * @param pIfaceInfo pointer to interface info struct to be filled
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_getInterfaceInfo(T_Radio* pRadio, wld_nl80211_ifaceInfo_t* pIfaceInfo);

/*
 * @brief configure radio's main interface type as AP
 *
 * @param pRadio pointer to radio context
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_setAp(T_Radio* pRadio);

/*
 * @brief configure radio's main interface type as Station (endpoint)
 *
 * @param pRadio pointer to radio context
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_setSta(T_Radio* pRadio);

/*
 * @brief configure radio's main interface 4Mac option
 * (With NL80211, it is only available in STATION and AP_VLAN types)
 *
 * @param pRadio pointer to radio context
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_set4Mac(T_Radio* pRadio, bool use4Mac);

/*
 * @brief create interface on the radio device
 *
 * @param pRadio pointer to radio context
 * @param ifname interface name
 * @param mac mac address of the new interface
 * @param isSta flag for managed/AP interface type
 * @param pOutIfInfo output interface info
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_addInterface(T_Radio* pRadio, const char* ifname, swl_macBin_t* mac, bool isSta, wld_nl80211_ifaceInfo_t* pOutIfInfo);

/*
 * @brief create/set AP interface on top of radio device
 * AP.alias (expected net dev name) must be provided as input.
 *
 * @param pAP pointer to accesspoint context
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_addVapInterface(T_Radio* pRadio, T_AccessPoint* pAP);

/*
 * @brief create/set EP interface on top of radio device
 * EP.alias (expected net dev name) must be provided as input.
 *
 * @param pRadio pointer to radio context
 * @param pEP pointer to endpoint context
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_addEpInterface(T_Radio* pRadio, T_EndPoint* pEP);

/*
 * @brief register all unassigned radio devices (via main nl2011 wl ifaces) to provided vendor context
 * (Synchronous api)
 *
 * @param vendor vendor context
 * @param maxWiphys max wiphy value to fetch (i.e max wiphy devices)
 * @param maxWlIfaces max number of interfaces per wiphy (i.e max AP/EP per radio)
 * @param wlIfacesInfo (input) 2D array of wl interfaces, including all existing wiphy/wlIfIndex
 *
 * @return count of detected and registered radio devices
 */
uint8_t wld_rad_nl80211_addRadios(vendor_t* vendor,
                                  const uint32_t maxWiphys, const uint32_t maxWlIfaces,
                                  wld_nl80211_ifaceInfo_t wlIfacesInfo[maxWiphys][maxWlIfaces]);

/*
 * @brief get info of radio device
 * This includes:
 * - supported radio standards, freq Bands (+chanList), channel widths
 *
 * @param pRadio pointer to radio context
 * @param pWiphyInfo pointer to radio info struct to be filled
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_getWiphyInfo(T_Radio* pRadio, wld_nl80211_wiphyInfo_t* pWiphyInfo);

/*
 * @brief get survey info of all radio channels
 *
 * @param pRadio pointer to radio context
 * @param ppChanSurveyInfo (output) array of channel survey info, dynamically allocated
 * (need to be freed by user)
 * @param pnChanSurveyInfo (output) number of available survey info
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_getSurveyInfo(T_Radio* pRadio, wld_nl80211_channelSurveyInfo_t** ppChanSurveyInfo, uint32_t* pnrChanSurveyInfo);

/*
 * @brief convert channel survey info of the central channel being used into radio air statistics
 *
 * @param pRadio pointer to radio context
 * @param pStats pointer to result
 * @param pChanSurveyInfo  pointer to channel survey info to be processed
 *
 * @return SWL_RC_OK in case of success
 *         SWL_RC_CONTINUE if survey info is not of the central channel being used
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_getAirStatsFromSurveyInfo(T_Radio* pRadio, wld_airStats_t* pStats, wld_nl80211_channelSurveyInfo_t* pChanSurveyInfo);


/*
 * @brief Retrieves the channel survey report, calculates the interference factor
 * for each channel, and updates the lastSurveyReport structure.
 *
 * @param pRad Pointer to the radio context.
 * @param pChanSurveyInfoList array of channel survey info
 * @param nChanSurveyInfo count of channel survey array entries
 *
 * @return SWL_RC_OK on success, SWL_RC_ERROR on failure.
 */
swl_rc_ne wld_rad_nl80211_updateChanSurveyReportFromSurveyInfo(T_Radio* rad, wld_nl80211_channelSurveyInfo_t* pChanSurveyInfoList, uint32_t nChanSurveyInfo);

/*
 * @brief update radio usage statistics (spectrum info list)
 * based on retrieved channel survey results
 *
 * @param pRadio pointer to radio context
 * @param pOutSpectrumResults spectrum results filled with process survey info
 * @param pChanSurveyInfoList array of channel survey info
 * @param nChanSurveyInfo count of channel survey array entries
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_updateUsageStatsFromSurveyInfo(T_Radio* pRadio, amxc_llist_t* pOutSpectrumResults,
                                                         wld_nl80211_channelSurveyInfo_t* pChanSurveyInfoList, uint32_t nChanSurveyInfo);

/*
 * @brief get radio air statistics of the central channel being used
 * The channel load is defined as the percentage of time that the AP sensed the medium was busy.
 *
 * @param pRadio pointer to radio context
 * @param stats pointer to result
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_getAirstats(T_Radio* pRad, wld_airStats_t* stats);

/*
 * @brief configure radio's tx/rx antennas
 *
 * @param pRadio pointer to radio context
 * @param txMapAnt Bitmap of allowed antennas for transmitting
 * @param rxMapAnt Bitmap of allowed antennas for receiving
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_setAntennas(T_Radio* pRadio, uint32_t txMapAnt, uint32_t rxMapAnt);

/*
 * @brief configure auto transmit power level
 *
 * @param pRadio pointer to radio context
 * @param type TX power adjustment
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_setTxPowerAuto(T_Radio* pRadio);

/*
 * @brief configure fixed transmit power level
 *
 * @param pRadio pointer to radio context
 * @param mbm transmit power level in dbm
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_setTxPowerFixed(T_Radio* pRadio, int32_t dbm);

/*
 * @brief configure limited transmit power level
 *
 * @param pRadio pointer to radio context
 * @param mbm transmit power level in dbm
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_setTxPowerLimited(T_Radio* pRadio, int32_t dbm);

/*
 * @brief get transmit power level
 *
 * @param pRadio pointer to radio context
 * @param dbm current tx power in dbm
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_getTxPower(T_Radio* pRadio, int32_t* dbm);

/*
 * @brief get maximum transmit power level for a spesific channel
 *
 * @param pRadio pointer to radio context
 * @param channel for which the max transmit power is being retrieved
 * @param dbm max tx power in dbm
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_getMaxTxPowerdBm(T_Radio* pRadio, uint16_t channel, int32_t* dbm);

/*
 * @brief get channel specification from nl80211 interface info
 *
 * @param pChanSpec pointer to chanspec info struct to be filled
 * @param pIfaceInfo source pointer to interface info
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_getChanSpecFromIfaceInfo(swl_chanspec_t* pChanSpec, wld_nl80211_ifaceInfo_t* pIfaceInfo);

/*
 * @brief get current radio channel info
 *
 * @param pRadio pointer to radio context
 * @param pChanSpec pointer to chanspec info struct to be filled
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_getChannel(T_Radio* pRadio, swl_chanspec_t* pChanSpec);

/*
 * @brief initiate a neighbor scan
 * It is possible to indicate scan options:
 * - ssids to fetch
 * - frequencies to use
 * Every call flushes the previous scan cache
 *
 * @param pRadio pointer to radio context
 *
 * @return SWL_RC_OK in case of success (scan trigger acknowledged)
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_startScan(T_Radio* pRadio);

/*
 * @brief initiate a neighbor scan with selected scan flags
 * It is possible to indicate scan options:
 * - ssids to fetch
 * - frequencies to use
 *
 * @param pRadio pointer to radio context
 * @param pFlags pointer to scan flags struct
 *
 * @return SWL_RC_OK in case of success (scan trigger acknowledged)
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_startScanExt(T_Radio* pRadio, wld_nl80211_scanFlags_t* pFlags);

/*
 * @brief abort a running neighbor scan
 *
 * @param pRadio pointer to radio context
 *
 * @return SWL_RC_CONTINUE in case the nl80211 scan abort command was successfully issued and waiting for scanAborted event
 *         SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_abortScan(T_Radio* pRadio);

/*
 * @brief subscribe to get asynchronous scan results
 *
 * @param pRadio pointer to radio context
 * @param priv user data that will returned in the result handler
 * @param fScanResultsCb handler that will be called when results are ready
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_getScanResults(T_Radio* pRadio, void* priv, scanResultsCb_f fScanResultsCb);

/*
 * @brief schedule the next scan
 *
 * @param pRad pointer to radio context
 *
 * @return swl_rc_ne in case of success
 */
swl_rc_ne wld_rad_nl80211_scheduleNextScan(T_Radio* pRad);

/*
 * @brief set regulatory ISO/IEC 3166-1 alpha2 country code
 * (applicable as global or on radio device)
 *
 * @param pRadio pointer to radio context
 * @param alpha2 ISO/IEC 3166-1 alpha2 country code
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_setRegDomain(T_Radio* pRadio, const char* alpha2);

/*
 * @brief start background DFS
 *
 * @param pRadio pointer to radio context
 * @param args pointer to background DFS args context
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_bgDfsStart(T_Radio* pRadio, wld_startBgdfsArgs_t* args);

/*
 * @brief stop background DFS
 *
 * @param pRadio pointer to radio context
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_bgDfsStop(T_Radio* pRadio);

/*
 * @brief common function to send vendor sub command
 *
 * @param pRadio pointer to radio context
 * @param ifIndex interface net dev index (ignored if ifIndex is null)
 * @param oui vendor driver identifier
 * @param subcmd vendor sub command to be sent
 * @param data optional depending on sent sub command
 * @param dataLen length of sent data
 * @param isSync flag to send sync/async request
 * @param withAck flag to wait for acknowledgment request
 * @param flags optional nl80211 msg flags
 * @param handler callback invoked when a reply is available
 * @param priv private data to pass in to the handler
 *
 * @return SWL_RC_OK on success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_sendVendorSubCmd(T_Radio* pRadio, uint32_t oui, int subcmd, void* data, int dataLen, bool isSync,
                                           bool withAck, uint32_t flags, wld_nl80211_handler_f handler, void* priv);

/*
 * @brief common function to send vendor sub command with attributes inside vendor data
 *
 * @param pRadio pointer to radio context
 * @param ifIndex interface net dev index (ignored if ifIndex is null)
 * @param oui vendor driver identifier
 * @param subcmd vendor sub command to be sent
 * @param vendorAttr list of attributes to be added inside the vendor data attributes.
 * @param isSync flag to send sync/async request
 * @param withAck flag to wait for acknowledgment request
 * @param flags optional nl80211 msg flags
 * @param handler callback invoked when a reply is available
 * @param priv private data to pass in to the handler
 *
 * @return SWL_RC_OK on success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_sendVendorSubCmdAttr(T_Radio* pRadio, uint32_t oui, int subcmd, wld_nl80211_nlAttr_t* vendorAttr,
                                               bool isSync, bool withAck, uint32_t flags, wld_nl80211_handler_f handler, void* priv);

/*
 * @brief common function to register to frame
 *
 * @param pRadio pointer to radio context
 * @param type packet type to be received
 * @param pattern first bytes of the packet (optional)
 * @param patternLen pattern's length (optional)
 *
 * @return SWL_RC_OK on success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_registerFrame(T_Radio* pRadio, uint16_t type, const char* pattern, size_t patternLen);

/*
 * @brief get first enabled radio's vap mld link info (mld iface index, and mld link id)
 *
 * @param pRadio pointer to radio context
 * @param pIfIndex pointer to mld iface index to be filled
 * @param pLinkId pointer to mld link id to be filled
 *
 * @return SWL_RC_OK in case of success
 *         <= SWL_RC_ERROR otherwise
 */
swl_rc_ne wld_rad_nl80211_getFirstEnabledVapLinkInfo(T_Radio* pRadio, int32_t* pIfIndex, int8_t* pLinkId);

#endif /* INCLUDE_WLD_WLD_RAD_NL80211_H_ */
