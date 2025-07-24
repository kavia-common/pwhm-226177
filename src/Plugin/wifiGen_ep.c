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
#include "wld/wld.h"
#include "wld/wld_rad_nl80211.h"
#include "wld/wld_ep_nl80211.h"
#include "wld/wld_linuxIfUtils.h"
#include "wld/wld_linuxIfStats.h"
#include "wld/wld_radio.h"
#include "wifiGen_wpaSupp.h"
#include "wld/wld_wpaSupp_ep_api.h"
#include "wld/wld_util.h"
#include "wld/wld_wps.h"
#include "wld/wld_endpoint.h"
#include "wifiGen_ep.h"
#include "wifiGen_events.h"
#include "wifiGen_fsm.h"

#define ME "genEp"

swl_rc_ne wifiGen_ep_createHook(T_EndPoint* pEP) {
    ASSERT_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    // The fsm is started before the endpoint instance creation
    wifiGen_wpaSupp_init(pEP);
    wifiGen_setEpEvtHandlers(pEP);
    return SWL_RC_OK;
}

swl_rc_ne wifiGen_ep_destroyHook(T_EndPoint* pEP) {
    ASSERT_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    wifiGen_wpaSupp_cleanup(pEP);
    return SWL_RC_OK;
}

/**
 * @brief wifiGen_ep_enable
 *
 * Enable/Disable the Endpoint
 *
 * @param pEP T_EndPoint struct endpoint instance
 * @param enable false(disable)/true(enable) value
 * @return - SWL_RC_INVALID_PARAM if pEP is NULL
 *         - SWL_RC_ERROR if radio instance associated to pEP is NULL
 *         - Otherwise, SWL_RC_OK
 */
swl_rc_ne wifiGen_ep_enable(T_EndPoint* pEP, bool enable) {
    ASSERT_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    T_Radio* pRad = pEP->pRadio;
    ASSERT_NOT_NULL(pRad, SWL_RC_ERROR, ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s : Endpoint enable changed : [%d] --> [%d]", pEP->Name, pEP->enable, enable);
    pEP->enable = enable;
    bool isRadSTA = (pRad->isSTASup && enable);
    swl_typeBool_commitObjectParam(pRad->pBus, "STA_Mode", isRadSTA);
    SAH_TRACEZ_INFO(ME, "ep %s/%d rad %s/%d isSta(%d/%d) isStaSupp(%d)",
                    pEP->Name, pEP->index,
                    pRad->Name, pRad->index, pRad->isSTA, isRadSTA, pRad->isSTASup);
    setBitLongArray(pEP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_ENABLE_EP);
    return SWL_RC_OK;
}

/**
 * @brief mfn_wendpoint_disconnect
 *
 * Disassociate from an AP
 *
 * @param pEP The current endpoint
 * @return - SWL_RC_INVALID_PARAM if pEP is NULL
           - Otherwise, the result of wld_wpaSupp_ep_disconnect function
 */
swl_rc_ne wifiGen_ep_disconnect(T_EndPoint* pEP) {
    ASSERT_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTI_TRUE(wld_secDmn_isRunning(pEP->wpaSupp), SWL_RC_OK, ME, "%s: wpaSupp not started", pEP->Name);
    swl_rc_ne rc = wld_wpaSupp_ep_disconnect(pEP);
    if(pEP->currentProfile == NULL) {
        wifiGen_ep_update(pEP, SET);
    }
    return rc;
}

/**
 * @brief mfn_wendpoint_bssid
 *
 * request current endpoint bssid (mac of remote AP) if ep has connected state
 *
 * @param pEP The current endpoint
 * @param bssid The buffer containing the bssid
 * @return - SWL_RC_INVALID_PARAM if either pEP or buf is NULL
           - SWL_RC_INVALID_STATE if pEP is not connected to an AP
           - Otherwise, the result of wld_wpaSupp_ep_getBssid function
 */
swl_rc_ne wifiGen_ep_bssid(T_EndPoint* pEP, swl_macChar_t* bssid) {
    ASSERTS_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(bssid, SWL_RC_INVALID_PARAM, ME, "NULL");
    if(pEP->connectionStatus != EPCS_CONNECTED) {
        SAH_TRACEZ_INFO(ME, "%s: unable to get bssid, not connected", pEP->Name);
        return SWL_RC_INVALID_STATE;
    }
    return wld_wpaSupp_ep_getBssid(pEP, bssid);
}

swl_rc_ne wifiGen_ep_setMacAddress(T_EndPoint* pEP) {
    ASSERTS_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    setBitLongArray(pEP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_START_WPASUPP);
    setBitLongArray(pEP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_MOD_EP_MACADDR);
    return SWL_RC_OK;
}

/**
 * @brief wifiGen_ep_connectAp
 *
 * Connect to an AP
 *
 * @param epProfile a profile containing informations to which the endpoint should connect
 * @return - SWL_RC_INVALID_PARAM if epProfile is NULL
           - SWL_RC_ERROR if pEP associated to the profile is NULL
           - Otherwise, SWL_RC_OK
 */
swl_rc_ne wifiGen_ep_connectAp(T_EndPointProfile* epProfile) {
    ASSERTS_NOT_NULL(epProfile, SWL_RC_INVALID_PARAM, ME, "NULL");
    T_EndPoint* pEP = epProfile->endpoint;
    ASSERTS_NOT_NULL(pEP, SWL_RC_ERROR, ME, "NULL");
    setBitLongArray(pEP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_ENABLE_EP);
    return SWL_RC_OK;
}

/**
 * @brief wifiGen_ep_status
 *
 * get the interface status
 *
 * @param pEP The current endpoint
 * @return - SWL_RC_INVALID_PARAM if pEP is NULL
           - SWL_RC_ERROR when wpactrl interface isn't ready or interface hasn't netdev index or the netdev ionterface state is down
           - Otherwise, SWL_RC_OK
 */
swl_rc_ne wifiGen_ep_status(T_EndPoint* pEP) {
    ASSERT_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_nl80211_ifaceInfo_t ifaceInfo;
    swl_rc_ne rc = wld_ep_nl80211_getInterfaceInfo(pEP, &ifaceInfo);
    ASSERTI_TRUE(swl_rc_isOk(rc), SWL_RC_ERROR, ME, "%s: Fail to get nl80211 ep iface info", pEP->alias);
    int ret = wld_linuxIfUtils_getState(wld_rad_getSocket(pEP->pRadio), ifaceInfo.name);
    ASSERTI_FALSE(ret <= 0, SWL_RC_ERROR, ME, "%s: ep iface down", pEP->alias);
    return SWL_RC_OK;
}

/**
 * @brief wifiGen_ep_update
 *
 * update wpa supplicant with new configuration
 *
 * @param pEP The current endpoint
 * @return -  SWL_RC_OK
 */
swl_rc_ne wifiGen_ep_update(T_EndPoint* pEP, int set) {
    ASSERT_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_TRUE(set & SET, SWL_RC_INVALID_PARAM, ME, "Set Only");
    SAH_TRACEZ_INFO(ME, "%s : set wendpoint_update", pEP->Name);
    setBitLongArray(pEP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_MOD_WPASUPP);
    setBitLongArray(pEP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_UPDATE_WPASUPP);
    wld_rad_doCommitIfUnblocked(pEP->pRadio);
    return SWL_RC_OK;
}

/**
 * @brief wifiGen_ep_connStatus
 *
 * get the endpoint connection status
 *
 * @param pEP The current endpoint
 * @param pConnState The current endpoint connection status
 * @return - SWL_RC_OK if successful
 *         - error code otherwise
 */
swl_rc_ne wifiGen_ep_connStatus(T_EndPoint* pEP, wld_epConnectionStatus_e* pConnState) {
    ASSERT_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_epConnectionStatus_e connState = pEP->connectionStatus;
    if(wifiGen_ep_status(pEP) < SWL_RC_OK) {
        connState = EPCS_DISABLED;
    } else if(wld_wpaSupp_ep_getConnState(pEP, &connState) < SWL_RC_OK) {
        connState = EPCS_IDLE;
    } else if((pEP->connectionStatus == EPCS_WPS_PAIRING) &&
              ((connState == EPCS_DISCOVERING) || (connState == EPCS_CONNECTING))) {
        connState = EPCS_WPS_PAIRING;
    }
    SAH_TRACEZ_INFO(ME, "%s: connSta %s => %s", pEP->Name,
                    cstr_EndPoint_connectionStatus[pEP->connectionStatus],
                    cstr_EndPoint_connectionStatus[connState]);
    W_SWL_SETPTR(pConnState, connState);
    return SWL_RC_OK;
}

/**
 * @brief wifiGen_ep_wpsStart
 *
 * start a WPS attempt for an endpoint.
 *
 * @param pEP: Pointer to the endpoint.
 * @param configmethod: The WPS method to use.
 * @param pin: The PIN to use (only for PIN based methods)
 * @param ssid: The SSID to connect to (mandatory for PIN methods, optional otherwise)
 * @param bssid: The BSSID to connect to (optional)
 * @return - SWL_RC_INVALID_PARAM if pEP is NULL
           - SWL_RC_ERROR if endpoint radio is NULL
           - SWL_RC_NOT_AVAILABLE if there is on going WPS session on another endpoint of the same radio
           - Otherwise, result of wld_wpaSupp_ep_startWps call
 */
swl_rc_ne wifiGen_ep_wpsStart(T_EndPoint* pEP, wld_wps_cfgMethod_e method, char* pin, char* ssid _UNUSED, swl_macChar_t* bssid) {
    ASSERT_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    T_Radio* pRad = pEP->pRadio;
    ASSERT_NOT_NULL(pRad, SWL_RC_ERROR, ME, "NULL");
    if(wld_rad_hasWpsActiveEndpoint(pRad)) {
        SAH_TRACEZ_ERROR(ME, "%s: WPS already started for other endpoint", pEP->Name);
        return SWL_RC_NOT_AVAILABLE;
    }
    char* reason = NULL;
    swl_rc_ne ret;

    if((method == WPS_CFG_MTHD_LABEL) || (method == WPS_CFG_MTHD_DISPLAY) || (method == WPS_CFG_MTHD_DISPLAY_V)
       || (method == WPS_CFG_MTHD_DISPLAY_P) || (method == WPS_CFG_MTHD_DISPLAY_V) || (method == WPS_CFG_MTHD_PIN)) {
        ret = wld_wpaSupp_ep_startWpsPin(pEP, pin, bssid);
        reason = WPS_CAUSE_START_WPS_PIN;
    } else if((method == WPS_CFG_MTHD_PBC) || (method == WPS_CFG_MTHD_PBC_P) || (method == WPS_CFG_MTHD_PBC_V)) {
        ret = wld_wpaSupp_ep_startWpsPbc(pEP, bssid);
        reason = WPS_CAUSE_START_WPS_PBC;
    } else {
        SAH_TRACEZ_ERROR(ME, "Not supported WPS configmethod %d", method);
        ret = SWL_RC_ERROR;
    }
    ASSERT_TRUE(swl_rc_isOk(ret), ret, ME, "error %d", ret);
    wld_endpoint_sendPairingNotification(pEP, NOTIFY_PAIRING_READY, reason, NULL);
    return SWL_RC_OK;
}

/**
 * @brief wifiGen_ep_wpsCancel
 *
 * cancel a WPS attempt for an endpoint
 *
 * @param pEP: Pointer to the endpoint
 * @return - SWL_RC_INVALID_PARAM if pEP is NULL
           - SWL_RC_ERROR if endpoint radio is NULL
           - SWL_RC_NOT_AVAILABLE if there is on going WPS session on another endpoint of the same radio
           - Otherwise, result of wld_wpaSupp_ep_cancelWps call
 */
swl_rc_ne wifiGen_ep_wpsCancel(T_EndPoint* pEP) {
    ASSERT_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    swl_rc_ne ret = wld_wpaSupp_ep_cancelWps(pEP);
    ASSERT_TRUE(swl_rc_isOk(ret), ret, ME, "error %d", ret);
    NOTIFY_NA_EXT(pEP->wpaCtrlInterface, fWpsCancelMsg);
    return SWL_RC_OK;
}

/**
 * @brief wifiGen_ep_stats
 *
 * get endpoint stats
 *
 * @param pEP: Pointer to the T_EndPoint
 * @param stats: Pointer to the T_EndPointStats
 * @return - SWL_RC_ERROR if pEP is NULL
           - SWL_RC_ERROR if stats is NULL
           - SWL_RC_OK, if success to get stats, SWL_RC_ERROR if not
 */
swl_rc_ne wifiGen_ep_stats(T_EndPoint* pEP, T_EndPointStats* stats) {
    ASSERT_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pEP->pRadio, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pEP->pSSID, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTI_EQUALS(pEP->connectionStatus, EPCS_CONNECTED, SWL_RC_INVALID_STATE, ME, "%s: not connected %u", pEP->Name, pEP->connectionStatus);
    ASSERT_NOT_NULL(stats, SWL_RC_INVALID_PARAM, ME, "NULL");

    // interface station dump
    swl_macBin_t bssid;
    memcpy(bssid.bMac, pEP->pSSID->BSSID, SWL_MAC_BIN_LEN);
    ASSERTI_FALSE(swl_mac_binIsNull(&bssid), SWL_RC_INVALID_STATE, ME, "%s: no remote bssid", pEP->Name);
    wld_nl80211_stationInfo_t stationInfo;
    if(wld_nl80211_getStationInfo(wld_nl80211_getSharedState(), pEP->index, &bssid, &stationInfo) < SWL_RC_OK) {
        SAH_TRACEZ_INFO(ME, "get stats for %s fail", pEP->Name);
        return SWL_RC_ERROR;
    }

    wld_rad_getCurrentNoise(pEP->pRadio, &pEP->pRadio->stats.noise);
    stats->SignalStrength = stationInfo.rssiDbm;
    stats->noise = pEP->pRadio->stats.noise;
    stats->SignalNoiseRatio = 0;
    if((stats != 0) && (stats->SignalStrength != 0) && (stats->SignalStrength > stats->noise)) {
        stats->SignalNoiseRatio = stats->SignalStrength - stats->noise;
    }
    stats->RSSI = convSignal2Quality(stats->SignalStrength);

    stats->txbyte = stationInfo.txBytes;
    stats->txPackets = stationInfo.txPackets;
    stats->txRetries = stationInfo.txRetries;
    stats->rxbyte = stationInfo.rxBytes;
    stats->rxPackets = stationInfo.rxPackets;
    stats->rxRetries = 0;
    stats->Retransmissions = SWL_MIN((stationInfo.txRetries * 100) / stationInfo.txPackets, (uint32_t) 100);
    stats->operatingStandard = SWL_MAX(
        swl_mcs_radStdFromMcsStd(stationInfo.txRate.mcsInfo.standard, pEP->pRadio->operatingFrequencyBand),
        swl_mcs_radStdFromMcsStd(stationInfo.rxRate.mcsInfo.standard, pEP->pRadio->operatingFrequencyBand));
    stats->LastDataUplinkRate = stationInfo.rxRate.bitrate;
    stats->maxRxStream = (uint16_t) stationInfo.rxRate.mcsInfo.numberOfSpatialStream;
    stats->LastDataDownlinkRate = stationInfo.txRate.bitrate;
    stats->maxTxStream = (uint16_t) stationInfo.txRate.mcsInfo.numberOfSpatialStream;

    stats->assocCaps.linkBandwidth = swl_chanspec_intToBw(
        SWL_MAX(swl_chanspec_bwToInt(stationInfo.txRate.mcsInfo.bandwidth),
                swl_chanspec_bwToInt(stationInfo.rxRate.mcsInfo.bandwidth)));
    if(pEP->currentProfile && (stationInfo.flags.authorized == SWL_TRL_TRUE)) {
        stats->assocCaps.currentSecurity = pEP->currentProfile->secModeEnabled;
    }
    stats->assocCaps.freqCapabilities = pEP->pRadio->supportedFrequencyBands;

    SAH_TRACEZ_INFO(ME, "get stats for %s OK", pEP->Name);

    return SWL_RC_OK;
}

swl_rc_ne wifiGen_ep_multiApEnable(T_EndPoint* pEP) {
    ASSERTS_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    setBitLongArray(pEP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_MOD_WPASUPP);
    setBitLongArray(pEP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_UPDATE_WPASUPP);
    return SWL_RC_OK;
}

swl_rc_ne wifiGen_ep_sendManagementFrame(T_EndPoint* pEP, swl_80211_mgmtFrameControl_t* fc, swl_macBin_t* tgtMac, swl_bit8_t* data, size_t dataLen, swl_chanspec_t* chanspec) {
    return wld_ep_nl80211_sendManagementFrameCmd(pEP, fc, tgtMac, data, dataLen, chanspec, 0);
}
