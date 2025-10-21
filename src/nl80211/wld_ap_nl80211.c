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
 * This file implements wrapper functions to use nl80211 generic apis with T_AccessPoint context
 */

#include "wld_ap_nl80211.h"
#include "wld_ssid_nl80211_priv.h"
#include "swl/swl_common.h"

#define ME "nlAP"

swl_rc_ne wld_ap_nl80211_setEvtListener(T_AccessPoint* pAP, void* pData, const wld_nl80211_evtHandlers_cb* const handlers) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    swl_rc_ne rc = wld_nl80211_delEvtListener(&pAP->nl80211Listener);
    if((rc == SWL_RC_OK) && (handlers == NULL)) {
        return rc;
    }
    uint32_t wiphy = WLD_NL80211_ID_UNDEF;
    uint32_t ifIndex = pAP->index;
    ASSERT_TRUE(ifIndex > 0, SWL_RC_INVALID_STATE, ME, "%s: no saved ifIndex", pAP->alias);
    SAH_TRACEZ_INFO(ME, "ap(%s): add evt listener wiphy(%d)/ifIndex(%d)", pAP->alias, wiphy, ifIndex);
    wld_nl80211_state_t* state = wld_nl80211_getSharedState();
    pAP->nl80211Listener = wld_nl80211_addEvtListener(state, wiphy, ifIndex, pAP, pData, handlers);
    ASSERT_NOT_NULL(pAP->nl80211Listener, SWL_RC_ERROR,
                    ME, "ap(%s): fail to add evt listener wiphy(%d)/ifIndex(%d)", pAP->alias, wiphy, ifIndex);
    return SWL_RC_OK;
}

swl_rc_ne wld_ap_nl80211_delEvtListener(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_nl80211_delEvtListener(&pAP->nl80211Listener);
}

swl_rc_ne wld_ap_nl80211_getInterfaceInfo(T_AccessPoint* pAP, wld_nl80211_ifaceInfo_t* pIfaceInfo) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_ssid_nl80211_getInterfaceInfo(pAP->pSSID, pIfaceInfo);
}

swl_rc_ne wld_ap_nl80211_delVapInterface(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    if(pAP->pRadio && (pAP->pRadio->index == pAP->index)) {
        return wld_nl80211_setInterfaceType(wld_nl80211_getSharedState(), pAP->index, false, true);
    } else {
        return wld_nl80211_delInterface(wld_nl80211_getSharedState(), pAP->index);
    }
}

static bool s_matchVapIfSta(T_AccessPoint* pAP, wld_nl80211_stationInfo_t* pStationInfo) {
    ASSERTS_NOT_NULL(pAP, false, ME, "NULL");
    ASSERTS_NOT_NULL(pStationInfo, false, ME, "NULL");
    ASSERTW_FALSE(swl_mac_binIsNull(&pStationInfo->macAddr), false, ME, "null mac addr");
    int16_t apLinkId = wld_mld_getLinkId(pAP->pSSID->pMldLink);
    if((pStationInfo->nrLinks > 0) && (apLinkId >= 0)) {
        //match stations connected to AP MLD link
        for(uint32_t i = 0; i < pStationInfo->nrLinks; i++) {
            if(pStationInfo->linksInfo[i].linkId == apLinkId) {
                return true;
            }
        }
    } else if((pStationInfo->nrLinks == 0) && (apLinkId < 0)) {
        //match legacy stations seen on legacy AP (non APMLD)
        return true;
    }
    return false;
}

swl_rc_ne wld_ap_nl80211_getStationInfo(T_AccessPoint* pAP, const swl_macBin_t* pMac, wld_nl80211_stationInfo_t* pStationInfo) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    swl_rc_ne rc;
    amxc_llist_for_each(it, &pAP->llIntfWds) {
        wld_wds_intf_t* wdsIntf = amxc_llist_it_get_data(it, wld_wds_intf_t, entry);
        if(SWL_MAC_BIN_MATCHES(&wdsIntf->bStaMac, pMac)) {
            rc = wld_nl80211_getStationInfo(wld_nl80211_getSharedState(), wdsIntf->index, pMac, pStationInfo);
            SAH_TRACEZ_INFO(ME, "%s: wds stats " SWL_MAC_FMT " %s found", pAP->name,
                            SWL_MAC_ARG(pMac->bMac),
                            (rc < SWL_RC_OK) ? "not" : "");
            if(rc < SWL_RC_OK) {
                break;
            }
            return rc;
        }
    }
    uint32_t index = wld_ssid_nl80211_getPrimaryLinkIfIndex(pAP->pSSID);
    wld_nl80211_stationInfo_t stationInfo;
    rc = wld_nl80211_getStationInfo(wld_nl80211_getSharedState(), index, pMac, &stationInfo);
    ASSERTS_TRUE(swl_rc_isOk(rc), rc, ME, "fail to get single sta info");
    if(s_matchVapIfSta(pAP, &stationInfo)) {
        W_SWL_SETPTR(pStationInfo, stationInfo);
        return SWL_RC_OK;
    }
    return SWL_RC_NOT_AVAILABLE;
}

swl_rc_ne wld_ap_nl80211_getAllStationsInfo(T_AccessPoint* pAP, wld_nl80211_stationInfo_t** ppStationInfo, uint32_t* pnrStation) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(ppStationInfo, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pnrStation, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_nl80211_stationInfo_t* staInfo = NULL;
    uint32_t nrStation = 0;
    uint32_t index = wld_ssid_nl80211_getPrimaryLinkIfIndex(pAP->pSSID);
    swl_rc_ne rc = wld_nl80211_getAllStationsInfo(wld_nl80211_getSharedState(), index, &staInfo, &nrStation);
    W_SWL_SETPTR(pnrStation, nrStation);
    W_SWL_SETPTR(ppStationInfo, staInfo);
    uint32_t nrApSta = 0;
    wld_nl80211_stationInfo_t* pApStaInfo = NULL;
    if((nrStation > 0) && staInfo) {
        pApStaInfo = calloc(nrStation, sizeof(wld_nl80211_stationInfo_t));
        ASSERT_NOT_NULL(pApStaInfo, rc, ME, "memory allocation failed");
        for(uint32_t i = 0; i < nrStation; i++) {
            if(s_matchVapIfSta(pAP, &staInfo[i])) {
                pApStaInfo[nrApSta++] = staInfo[i];
            }
        }
    }
    W_SWL_FREE(staInfo);
    W_SWL_SETPTR(pnrStation, nrApSta);
    W_SWL_SETPTR(ppStationInfo, pApStaInfo);
    nrStation = nrApSta;
    if(!amxc_llist_is_empty(&pAP->llIntfWds)) {
        nrStation = *pnrStation;
        staInfo = realloc(*ppStationInfo, (nrStation + amxc_llist_size(&pAP->llIntfWds)) * sizeof(wld_nl80211_stationInfo_t));
        ASSERTS_NOT_NULL(staInfo, rc, ME, "memory reallocation failed");
        amxc_llist_for_each(it, &pAP->llIntfWds) {
            wld_wds_intf_t* wdsIntf = amxc_llist_it_get_data(it, wld_wds_intf_t, entry);
            wld_nl80211_stationInfo_t wdsStaInfo;
            rc = wld_nl80211_getStationInfo(wld_nl80211_getSharedState(), wdsIntf->index,
                                            &wdsIntf->bStaMac, &wdsStaInfo);
            if(rc < SWL_RC_OK) {
                continue;
            }
            memcpy(&staInfo[nrStation], &wdsStaInfo, sizeof(wld_nl80211_stationInfo_t));
            nrStation++;
        }
        W_SWL_SETPTR(ppStationInfo, staInfo);
        W_SWL_SETPTR(pnrStation, nrStation);
    }
    return nrStation > 0 ? SWL_RC_OK : rc;
}

swl_rc_ne wld_ap_nl80211_copyStationInfoToAssocDev(T_AccessPoint* pAP, T_AssociatedDevice* pAD, wld_nl80211_stationInfo_t* pStationInfo) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pAD, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pStationInfo, SWL_RC_INVALID_PARAM, ME, "NULL");
    pAD->RxBytes = pStationInfo->rxBytes;
    pAD->TxBytes = pStationInfo->txBytes;

    int recent_packets_sent = (pStationInfo->txPackets - pAD->TxPacketCount);
    int recent_packets_retries = (pStationInfo->txRetries - pAD->Tx_Retransmissions);
    if(recent_packets_sent > 0) {
        pAD->Retransmissions = SWL_MIN((recent_packets_retries * 100) / recent_packets_sent, 100);
    } else {
        pAD->Retransmissions = 0;
    }

    pAD->RxPacketCount = pStationInfo->rxPackets;
    pAD->TxPacketCount = pStationInfo->txPackets;
    pAD->TxFailures = pStationInfo->txFailed;
    pAD->RxFailures = pStationInfo->rxFailed;
    pAD->Tx_Retransmissions = pStationInfo->txRetries;

    pAD->SignalStrength = pStationInfo->rssiDbm;
    for(uint32_t i = 0; i < SWL_MIN(pStationInfo->nrSignalChains, (uint32_t) MAX_NR_ANTENNA); i++) {
        pAD->SignalStrengthByChain[i] = pStationInfo->rssiDbmByChain[i];
    }
    pAD->connectionDuration = pStationInfo->connectedTime;
    pAD->Inactive = pStationInfo->inactiveTime / 1000;

    if(pStationInfo->flags.associated != SWL_TRL_UNKNOWN) {
        pAD->seen = (pStationInfo->flags.associated == SWL_TRL_TRUE);
    } else if(pStationInfo->rxRate.bitrate > 0) {
        pAD->seen = true;
    }
    if(pStationInfo->flags.authorized != SWL_TRL_UNKNOWN) {
        if(!swl_security_isApModeValid(pAD->assocCaps.currentSecurity)) {
            pAD->assocCaps.currentSecurity = pAP->secModeEnabled;
        }
    }

    SAH_TRACEZ_INFO(ME, "%s: Downlink MCS %s", pAD->Name, swl_typeMcs_toBuf32(pStationInfo->txRate.mcsInfo).buf);
    SAH_TRACEZ_INFO(ME, "%s: Uplink MCS %s", pAD->Name, swl_typeMcs_toBuf32(pStationInfo->rxRate.mcsInfo).buf);

    pAD->assocCaps.linkBandwidth = swl_chanspec_intToBw(
        SWL_MAX(swl_chanspec_bwToInt(pStationInfo->txRate.mcsInfo.bandwidth),
                swl_chanspec_bwToInt(pStationInfo->rxRate.mcsInfo.bandwidth)));

    pAD->LastDataUplinkRate = pStationInfo->rxRate.bitrate;
    pAD->MaxRxSpatialStreamsSupported = SWL_MAX(pAD->MaxRxSpatialStreamsSupported, (uint16_t) pStationInfo->rxRate.mcsInfo.numberOfSpatialStream);
    pAD->MaxUplinkRateSupported = SWL_MAX(pAD->MaxUplinkRateSupported, pStationInfo->rxRate.bitrate);
    pAD->UplinkMCS = pStationInfo->rxRate.mcsInfo.mcsIndex;
    pAD->UplinkBandwidth = swl_chanspec_bwToInt(pStationInfo->rxRate.mcsInfo.bandwidth);
    pAD->UplinkShortGuard = swl_mcs_guardIntervalToInt(pStationInfo->rxRate.mcsInfo.guardInterval);
    pAD->upLinkRateSpec = pStationInfo->rxRate.mcsInfo;

    pAD->LastDataDownlinkRate = pStationInfo->txRate.bitrate;
    pAD->MaxTxSpatialStreamsSupported = SWL_MAX(pAD->MaxTxSpatialStreamsSupported, (uint16_t) pStationInfo->txRate.mcsInfo.numberOfSpatialStream);
    pAD->DownlinkMCS = pStationInfo->txRate.mcsInfo.mcsIndex;
    pAD->DownlinkBandwidth = swl_chanspec_bwToInt(pStationInfo->txRate.mcsInfo.bandwidth);
    pAD->DownlinkShortGuard = swl_mcs_guardIntervalToInt(pStationInfo->txRate.mcsInfo.guardInterval);
    pAD->downLinkRateSpec = pStationInfo->txRate.mcsInfo;
    pAD->lastSampleTime = swl_timespec_getMonoVal();

    swl_mcsStandard_e mcsStd = SWL_MAX(pStationInfo->txRate.mcsInfo.standard, pStationInfo->rxRate.mcsInfo.standard);
    swl_radStd_e operStd = swl_mcs_radStdFromMcsStd(mcsStd, pAP->pRadio->operatingFrequencyBand);
    if(pAD->operatingStandard == SWL_RADSTD_AUTO) {
        pAD->operatingStandard = operStd;
    }

    if(mcsStd == SWL_MCS_STANDARD_EHT) {
        if(pStationInfo->nrLinks == 0) {
            pAD->mloMode = SWL_MLO_MODE_NA;
        } else if(pStationInfo->nrLinks == 1) {
            pAD->mloMode = SWL_MLO_MODE_SINGLE_LINK;
        }
    } else if(pStationInfo->nrLinks <= 1) {
        pAD->mloMode = SWL_MLO_MODE_NA;
    }
    if((pStationInfo->nrLinks > 1) && (pAD->mloMode <= SWL_MLO_MODE_SINGLE_LINK)) {
        pAD->mloMode = SWL_MLO_MODE_ACTIVE_UNKNOWN;
    }

    return SWL_RC_OK;
}

swl_rc_ne wld_ap_nl80211_sendVendorSubCmd(T_AccessPoint* pAP, uint32_t oui, int subcmd, void* data, int dataLen,
                                          bool isSync, bool withAck, uint32_t flags, wld_nl80211_handler_f handler, void* priv) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_NOT_NULL(pAP, rc, ME, "NULL");

    rc = wld_nl80211_sendVendorSubCmd(wld_nl80211_getSharedState(), oui, subcmd, data, dataLen, isSync, withAck,
                                      flags, pAP->index, pAP->wDevId, handler, priv);

    return rc;
}

swl_rc_ne wld_ap_nl80211_sendVendorSubCmdAttr(T_AccessPoint* pAP, uint32_t oui, int subcmd, wld_nl80211_nlAttr_t* vendorAttr,
                                              bool isSync, bool withAck, uint32_t flags, wld_nl80211_handler_f handler, void* priv) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_NOT_NULL(pAP, rc, ME, "NULL");

    rc = wld_nl80211_sendVendorSubCmdAttr(wld_nl80211_getSharedState(), oui, subcmd, vendorAttr, isSync, withAck,
                                          flags, pAP->index, pAP->wDevId, handler, priv);

    return rc;
}

swl_rc_ne wld_ap_nl80211_sendManagementFrameCmd(T_AccessPoint* pAP, swl_80211_mgmtFrameControl_t* fc, swl_macBin_t* tgtMac, swl_bit8_t* dataBytes, size_t dataBytesLen, swl_chanspec_t* chanspec,
                                                uint32_t flags) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_NOT_NULL(pAP, rc, ME, "NULL");
    T_SSID* pSSID = pAP->pSSID;
    ASSERT_NOT_NULL(pSSID, rc, ME, "NULL");

    uint32_t index = wld_ssid_nl80211_getPrimaryLinkIfIndex(pSSID);
    int8_t ifMloLinkId = wld_ssid_nl80211_getMldLinkId(pSSID);
    return wld_nl80211_sendManagementFrameCmd(wld_nl80211_getSharedState(), fc, dataBytes, dataBytesLen, chanspec,
                                              (swl_macBin_t*) &pSSID->MACAddress, tgtMac, (swl_macBin_t*) &pSSID->BSSID, flags, index, ifMloLinkId);
}

swl_rc_ne wld_ap_nl80211_registerFrame(T_AccessPoint* pAP, uint16_t type, const char* pattern, size_t patternLen) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_NOT_NULL(pAP, rc, ME, "NULL");
    return wld_nl80211_registerFrame(wld_nl80211_getSharedState(), pAP->index, type, pattern, patternLen);
}
