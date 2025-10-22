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
#include <swl/swl_common.h>
#include <swl/fileOps/swl_fileUtils.h>
#include <swla/swla_mac.h>

#include "wld/wld.h"
#include "wld/wld_util.h"
#include "wld/wld_radio.h"
#include "wld/wld_ssid.h"
#include "wld/wld_accesspoint.h"
#include "wld/wld_wps.h"
#include "wld/wld_linuxIfUtils.h"
#include "wld/wld_linuxIfStats.h"
#include "wld/wld_ap_nl80211.h"
#include "wld/wld_rad_nl80211.h"
#include "wld/wld_wpaCtrl_api.h"
#include "wld/wld_assocdev.h"
#include "wld/wld_hostapd_ap_api.h"
#include "wld/wld_hostapd_cfgFile.h"
#include "wifiGen_fsm.h"
#include "wifiGen_rad.h"
#include "wld/wld_statsmon.h"
#include "wld/Utils/wld_autoCommitMgr.h"
#include "wld/Utils/wld_autoNeighAdd.h"
#include "wld/wld_wpaSupp_parser.h"
#include "wifiGen_hapd.h"

#define ME "genVap"

#define RRM_BEACON_REPORT_MODE_ACTIVE 0x01
#define RRM_DEFAULT_REQ_MODE 0x00
#define RRM_DEFAULT_RANDOM_INTERVAL 0x0000
#define RRM_DEFAULT_MEASUREMENT_DURATION 0x0004
#define RRM_DEFAULT_MEASUREMENT_MODE RRM_BEACON_REPORT_MODE_ACTIVE

int wifiGen_vap_createHook(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    return SWL_RC_OK;
}

void wifiGen_vap_destroyHook(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    wld_ap_nl80211_delEvtListener(pAP);
    wld_wpaCtrlInterface_cleanup(&pAP->wpaCtrlInterface);
}

int wifiGen_vap_ssid(T_AccessPoint* pAP, char* buf, int bufsize, int set) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    T_SSID* pSSID = (T_SSID*) pAP->pSSID;
    ASSERTI_NOT_NULL(pSSID, SWL_RC_ERROR, ME, "NULL");
    if(set & SET) {
        swl_str_copy(pSSID->SSID, sizeof(pSSID->SSID), buf);
        setBitLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_MOD_SSID);
        SAH_TRACEZ_INFO(ME, "%s - %s", pAP->alias, pSSID->SSID);
    } else {
        strncpy(buf, pSSID->SSID, bufsize);
    }
    return SWL_RC_OK;
}

int wifiGen_vap_status(T_AccessPoint* pAP) {
    wld_nl80211_ifaceInfo_t ifaceInfo;
    swl_rc_ne rc = wld_ap_nl80211_getInterfaceInfo(pAP, &ifaceInfo);
    ASSERT_FALSE(rc < SWL_RC_OK, false, ME, "%s: Fail to get nl80211 ap iface info", pAP->alias);
    ASSERTS_STR(ifaceInfo.ssid, false, ME, "%s: ssid down", pAP->alias);
    ASSERTS_NOT_EQUALS(ifaceInfo.chanSpec.ctrlFreq, 0, false, ME, "%s: radio down", pAP->alias);
    ASSERTS_NOT_EQUALS(ifaceInfo.txPower, 0, false, ME, "%s: power down", pAP->alias);
    int ret = wld_linuxIfUtils_getLinkState(wld_rad_getSocket(pAP->pRadio), ifaceInfo.name);
    ASSERTI_FALSE(ret <= 0, false, ME, "%s: link down", pAP->alias);
    return true;
}

static int s_getVapEnable(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, 0, ME, "NULL");
    wld_nl80211_ifaceInfo_t ifaceInfo;
    memset(&ifaceInfo, 0, sizeof(ifaceInfo));
    swl_rc_ne rc = wld_ap_nl80211_getInterfaceInfo(pAP, &ifaceInfo);
    ASSERTS_TRUE(swl_rc_isOk(rc), 0, ME, "%s: fail to get iface info", pAP->name);
    if(ifaceInfo.nMloLinks > 0) {
        ASSERTI_NOT_EQUALS(ifaceInfo.chanSpec.ctrlFreq, 0, 0, ME, "%s: radio link disabled", ifaceInfo.name);
    }
    return (wld_linuxIfUtils_getStateExt(ifaceInfo.name) > 0);
}

int wifiGen_vap_enable(T_AccessPoint* pAP, int enable, int set) {
    ASSERT_NOT_NULL(pAP, 0, ME, "NULL");
    int ret;
    if(set & SET) {
        SAH_TRACEZ_INFO(ME, "VAP:%s State:%d-->%d - Set:%d", pAP->alias, pAP->enable, enable, set);
        if(set & DIRECT) {
            /*
             * we need to disable passive bss net ifaces (except the radio interface),
             * that were implicitly enabled by hostapd startup
             * (although not broadcasting)
             */
            if((enable) ||
               (pAP->index != pAP->pRadio->index) ||
               (pAP->pRadio->isSTA && pAP->pRadio->isSTASup) ||
               (!pAP->pRadio->enable) ||
               (wld_mld_countNeighEnabledLinks(pAP->pSSID->pMldLink) == 0)) {
                wld_linuxIfUtils_setState(wld_rad_getSocket(pAP->pRadio), pAP->alias, enable);
            }
            return s_getVapEnable(pAP);
        }
        ret = enable;
        setBitLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_ENABLE_AP);
    } else {
        if(set & DIRECT) {
            return s_getVapEnable(pAP);
        }
        ret = pAP->enable;
    }
    return ret;
}

void s_updateAssocDevWds(T_AccessPoint* pAP) {
    ASSERTI_FALSE(pAP->wdsEnable, , ME, "WDS enabled");
    int i;
    T_AssociatedDevice* pAD;
    int totalNrDev = pAP->AssociatedDeviceNumberOfEntries;
    for(i = 0; i < totalNrDev; i++) {
        pAD = pAP->AssociatedDevice[totalNrDev - 1 - i];
        if(pAD->wdsIntf != NULL) {
            SAH_TRACEZ_WARNING(ME, "%s: kicked for reason 'WDS mode disabled'", pAD->Name);
            pAP->pFA->mfn_wvap_kick_sta_reason(pAP,
                                               (char*) pAD->Name,
                                               SWL_MAC_CHAR_LEN,
                                               SWL_IEEE80211_DEAUTH_REASON_NONE);
        }
    }
}

int wifiGen_vap_sync(T_AccessPoint* pAP, int set) {
    int ret = 0;

    if(set & SET) {
        SAH_TRACEZ_INFO(ME, "%s : set vap_sync", pAP->alias);
        ret = setBitLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_MOD_AP);
        s_updateAssocDevWds(pAP);
    }
    return ret;
}

static void s_fillMldAssocDevInfo(T_AccessPoint* pAP, T_AssociatedDevice* pAD, wld_nl80211_stationInfo_t* pStationInfo) {
    ASSERT_NOT_NULL(pAP, , ME, "no accesspoint device entry!");
    ASSERT_NOT_NULL(pStationInfo, , ME, "no station info entry!");
    ASSERT_NOT_NULL(pAD, , ME, "no associated device entry!");
    ASSERTS_NOT_NULL(pAP->pSSID, , ME, "%s: no SSID", pAP->name);
    ASSERTS_NOT_NULL(pAP->pSSID->pMldLink, , ME, "%s: Accesspoint is not APMLD link", pAP->name);
    ASSERTS_TRUE(pAD->mloMode > SWL_MLO_MODE_NA, , ME, "%s: AssociatedDevice does not have active MLO", pAD->Name);
    wld_affiliatedSta_t* afSta = NULL;
    wld_ad_deactivateAllAfSta(pAD);
    for(int i = 0; i < pStationInfo->nrLinks; i++) {
        wld_nl80211_mloLinkInfo_t* pLinkInfo = &pStationInfo->linksInfo[i];
        T_SSID* pSSID = NULL;
        if((pSSID = wld_mld_getLinkSsidByLinkId(pAP->pSSID->pMldLink, pLinkInfo->linkId)) != NULL) {
            afSta = wld_ad_provideAffiliatedStaWithMac(pAD, pSSID->AP_HOOK, &pLinkInfo->linkMac);
            ASSERT_NOT_NULL(afSta, , ME, "%s: fetch affiliatedSta (linkId:%u,mac:%s) failed for sta(%s)!",
                            pSSID->AP_HOOK->alias,
                            pLinkInfo->linkId,
                            swl_typeMacBin_toBuf32Ref(&pLinkInfo->linkMac).buf,
                            pAD->Name);
            afSta->linkId = pLinkInfo->linkId;
            wld_ad_activateAfSta(pAD, afSta);
            afSta->bytesSent = pLinkInfo->stats.txBytes;
            afSta->bytesReceived = pLinkInfo->stats.rxBytes;
            afSta->packetsSent = pLinkInfo->stats.txPackets;
            afSta->packetsReceived = pLinkInfo->stats.rxPackets;
            afSta->errorsSent = pLinkInfo->stats.txErrors;
            afSta->signalStrength = pLinkInfo->stats.rssiDbm;
        }
    }
}

static void s_fillAssocDevInfo(T_AccessPoint* pAP, T_AssociatedDevice* pAD, wld_nl80211_stationInfo_t* pStationInfo) {
    ASSERTS_NOT_NULL(pAP, , ME, "NULL");
    ASSERTS_NOT_NULL(pAD, , ME, "NULL");
    wld_ap_nl80211_copyStationInfoToAssocDev(pAP, pAD, pStationInfo);
    pAD->AvgSignalStrengthByChain = wld_ad_getAvgSignalStrengthByChain(pAD);
    pAD->noise = pAP->pRadio->stats.noise;
    if((pAD->noise != 0) && (pAD->SignalStrength != 0) && (pAD->SignalStrength > pAD->noise)) {
        pAD->SignalNoiseRatio = pAD->SignalStrength - pAD->noise;
    } else {
        pAD->SignalNoiseRatio = 0;
    }
    s_fillMldAssocDevInfo(pAP, pAD, pStationInfo);
}

static void s_resetAssocDevSignalNoise(T_AssociatedDevice* pAD) {
    ASSERTS_NOT_NULL(pAD, , ME, "NULL");
    pAD->noise = 0;
    pAD->SignalStrength = 0;
    for(int j = 0; j < MAX_NR_ANTENNA; j++) {
        pAD->SignalStrengthByChain[j] = 0;
    }
    pAD->SignalNoiseRatio = 0;
}

static uint32_t s_getNetlinkAllStaInfo(T_AccessPoint* pAP) {

    uint32_t nrStations = 0;
    wld_nl80211_stationInfo_t* pAllStaInfo = NULL;
    swl_rc_ne retVal = wld_ap_nl80211_getAllStationsInfo(pAP, &pAllStaInfo, &nrStations);
    if(retVal < SWL_RC_OK) {
        SAH_TRACEZ_ERROR(ME, "%s: fail to get all stations info", pAP->alias);
        free(pAllStaInfo);
        return 0;
    }

    wld_rad_getCurrentNoise(pAP->pRadio, &pAP->pRadio->stats.noise);

    // Add new devices from driver maclist
    for(uint32_t id = 0; id < nrStations; id++) {
        T_AssociatedDevice* pAD = NULL;
        wld_nl80211_stationInfo_t* pStationInfo = &pAllStaInfo[id];
        if((pStationInfo->nrLinks <= 1) ||
           ((pStationInfo->linkId >= 0) && (wld_mld_getLinkId(pAP->pSSID->pMldLink) == pStationInfo->linkId)) ||
           ((pStationInfo->linkId < 0) && (wld_ap_hostapd_isMainStaMldLink(pAP, &pStationInfo->macAddr)))) {
            pAD = wld_vap_findOrCreateAssociatedDevice(pAP, &pStationInfo->macAddr);
        } else {
            SAH_TRACEZ_INFO(ME, "%s: skip reporting mlo sta %s (%d links) on auxiliary link",
                            pAP->alias,
                            swl_typeMacBin_toBuf32(pStationInfo->macAddr).buf,
                            pStationInfo->nrLinks);
            continue;
        }
        if(pAD == NULL) {
            SAH_TRACEZ_ERROR(ME, "%s: could not create new detected AD %s",
                             pAP->name, swl_typeMacBin_toBuf32Ref(&pStationInfo->macAddr).buf);
            continue;
        }
        pAD->seen = true;
        s_fillAssocDevInfo(pAP, pAD, pStationInfo);
        if(pStationInfo->flags.authenticated == SWL_TRL_TRUE) {
            wld_ad_add_connection_success(pAP, pAD);
        }
    }
    free(pAllStaInfo);
    return nrStations;
}

/*
 * lowest period since last refresh of assoc dev info in ms
 * to avoid too frequent low level requests
 */
#define MIN_AD_INFO_REFRESH_PERIOD_MS 100
static bool s_isStationInfoOld(T_AssociatedDevice* pAD) {
    ASSERTS_NOT_NULL(pAD, false, ME, "NULL");
    swl_timeSpecMono_t now = swl_timespec_getMonoVal();
    if(swl_timespec_isZero(&pAD->lastSampleTime)
       || (swl_timespec_diffToMillisec(&pAD->lastSampleTime, &now) > MIN_AD_INFO_REFRESH_PERIOD_MS)
       || (pAD->latestStateChangeTime >= pAD->lastSampleTime.tv_sec)) {
        return true;
    }
    return false;
}

static bool s_isAnyStationInfoOld(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, false, ME, "NULL");
    ASSERTS_NOT_NULL(pAP->AssociatedDevice, false, ME, "NULL");
    for(int i = 0; i < pAP->AssociatedDeviceNumberOfEntries; i++) {
        if(s_isStationInfoOld(pAP->AssociatedDevice[i])) {
            return true;
        }
    }
    return false;
}

swl_rc_ne wifiGen_vap_getStationStats(T_AccessPoint* pAP) {
    ASSERTI_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    T_Radio* pRad = (T_Radio*) pAP->pRadio;
    ASSERTI_NOT_EQUALS(pRad->status, RST_ERROR, SWL_RC_INVALID_STATE, ME, "NULL");
    ASSERTI_TRUE(s_isAnyStationInfoOld(pAP), SWL_RC_DONE, ME, "%s: station stats are too recent", pAP->alias);

    wld_vap_mark_all_stations_unseen(pAP);
    if(s_getNetlinkAllStaInfo(pAP) > 0) {
        wld_ap_hostapd_getAllStaInfo(pAP);
    }

    for(int i = 0; i < pAP->AssociatedDeviceNumberOfEntries; i++) {
        T_AssociatedDevice* pAD = pAP->AssociatedDevice[i];
        if(pAD == NULL) {
            SAH_TRACEZ_ERROR(ME, "%s: NULL AssociatedDev! wrong nrEntries (%d/%d)",
                             pAP->name, i, pAP->AssociatedDeviceNumberOfEntries);
            pAP->AssociatedDeviceNumberOfEntries = i;
            break;
        }
        if(!pAD->seen) {
            s_resetAssocDevSignalNoise(pAD);
        }
    }

    wld_vap_update_seen(pAP);
    return SWL_RC_OK;
}

swl_rc_ne wifiGen_vap_getSingleStationStats(T_AssociatedDevice* pAD) {
    T_AccessPoint* pAP = wld_ad_getAssociatedAp(pAD);
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_STATE, ME, "NULL");
    T_Radio* pRad = (T_Radio*) pAP->pRadio;
    ASSERTI_NOT_EQUALS(pRad->status, RST_ERROR, SWL_RC_INVALID_STATE, ME, "NULL");
    ASSERTI_TRUE(pAD->Active, SWL_RC_OK, ME, "%s: assocdev %s no more active", pAP->name, pAD->Name);
    ASSERTS_TRUE(s_isStationInfoOld(pAD), SWL_RC_DONE, ME, "station %s stats are too recent",
                 swl_typeMacBin_toBuf32Ref((swl_macBin_t*) pAD->MACAddress).buf);

    SAH_TRACEZ_INFO(ME, "AP %s (netdev %s)", pAP->name, pAP->alias);
    SAH_TRACEZ_INFO(ME, "AD %s", pAD->Name);

    wld_nl80211_stationInfo_t stationInfo;
    memset(&stationInfo, 0, sizeof(wld_nl80211_stationInfo_t));
    swl_rc_ne rc = wld_ap_nl80211_getStationInfo(pAP, (swl_macBin_t*) pAD->MACAddress, &stationInfo);
    if(rc >= SWL_RC_OK) {
        wld_rad_getCurrentNoise(pAP->pRadio, &pAP->pRadio->stats.noise);
        s_fillAssocDevInfo(pAP, pAD, &stationInfo);
        wld_ap_hostapd_getStaInfo(pAP, pAD);
    } else {
        s_resetAssocDevSignalNoise(pAD);
    }
    return SWL_RC_OK;
}

static bool s_updateLinkedStaInfoHdlr(void* userData _UNUSED, T_AccessPoint* pAP, T_AssociatedDevice* pAD) {
    ASSERTS_NOT_NULL(pAP, false, ME, "NULL");
    pAP->pFA->mfn_wvap_get_single_station_stats(pAD);
    return false;
}

static swl_rc_ne s_updateApStaStats(T_AccessPoint* pAP, swl_trl_e onlyAfSta) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_EQUALS(pAP->status, APSTI_ENABLED, SWL_RC_INVALID_STATE, ME, "invalid vap status");
    return wld_ap_doForEachLinkedStation(pAP, onlyAfSta, s_updateLinkedStaInfoHdlr, NULL);
}

swl_rc_ne wifiGen_vap_updateRssiStats(T_AccessPoint* pAP) {
    ASSERTI_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    return s_updateApStaStats(pAP, SWL_TRL_FALSE);
}

int wifiGen_vap_sec_sync(T_AccessPoint* pAP, int set) {
    int ret = 0;

    if(set & SET) {
        SAH_TRACEZ_INFO(ME, "%s : set vap_sec_sync", pAP->alias);
        ret = setBitLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_MOD_SEC);
    }
    return ret;
}

swl_rc_ne wifiGen_vap_setMboDisallowReason(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTI_TRUE(pAP->mboEnable, SWL_RC_OK, ME, "%s: mbo disabled", pAP->alias);
    char reasonIdStr[2] = {"0"};
    if(pAP->mboEnable) {
        reasonIdStr[0] = '0' + pAP->mboDenyReason;
    }
    if(!wld_ap_hostapd_setParamValue(pAP, "mbo_assoc_disallow", reasonIdStr, "mbo_assoc_disallow")) {
        SAH_TRACEZ_NOTICE(ME, "%s: can not apply mbo_assoc_disallow (%s) to hostapd: seems not supported",
                          pAP->alias, reasonIdStr);
        return SWL_RC_ERROR;
    }
    setBitLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_UPDATE_BEACON);
    wld_autoCommitMgr_notifyVapEdit(pAP);
    return SWL_RC_OK;
}

int wifiGen_vap_multiap_update_type(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    setBitLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_MOD_AP);
    return 0;
}

int wifiGen_vap_multiap_update_profile(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    setBitLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_MOD_AP);
    return 0;
}

int wifiGen_vap_multiap_update_vlanid(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    setBitLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_MOD_AP);
    return 0;
}

swl_rc_ne wifiGen_vap_sta_transfer(T_AccessPoint* pAP, wld_transferStaArgs_t* params) {
    return wld_ap_hostapd_transferStation(pAP, params);
}

swl_rc_ne wifiGen_vap_sendManagementFrame(T_AccessPoint* pAP, swl_80211_mgmtFrameControl_t* fc, swl_macBin_t* tgtMac, swl_bit8_t* data, size_t dataLen, swl_chanspec_t* chanspec) {
    return wld_ap_nl80211_sendManagementFrameCmd(pAP, fc, tgtMac, data, dataLen, chanspec, 0);
}

swl_rc_ne s_addDelNeighbor(T_AccessPoint* pAP, T_ApNeighbour* pApNeighbor, bool add) {
    ASSERTS_NOT_NULL(pApNeighbor, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    T_Radio* pRad = pAP->pRadio;
    ASSERTS_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");

    bool has11rFToDsEnabled = (pRad->IEEE80211rSupported && pAP->IEEE80211rEnable && pAP->IEEE80211rFTOverDSEnable);
    bool has11kNeighReportEnabled = (pRad->IEEE80211kSupported && pAP->IEEE80211kEnable);
    //dynamic setting of neighbors
    //1) clean old entry from hostapd DB
    wld_ap_hostapd_removeNeighbor(pAP, pApNeighbor);
    if((add) && (has11rFToDsEnabled || has11kNeighReportEnabled)) {
        //2) add new entry to hostapd DB
        wld_ap_hostapd_setNeighbor(pAP, pApNeighbor);
    }
    setBitLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_UPDATE_BEACON);

    if(has11rFToDsEnabled) {
        //only 11r FT neighbors requires cold conf apply
        setBitLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_UPDATE_HOSTAPD);
    }

    return SWL_RC_OK;
}

swl_rc_ne wifiGen_vap_deleted_neighbor(T_AccessPoint* pAP, T_ApNeighbour* pApNeighbor) {
    return s_addDelNeighbor(pAP, pApNeighbor, false);
}

swl_rc_ne wifiGen_vap_updated_neighbor(T_AccessPoint* pAP, T_ApNeighbour* pApNeighbor) {
    return s_addDelNeighbor(pAP, pApNeighbor, true);
}

swl_rc_ne wifiGen_vap_setDiscoveryMethod(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTI_TRUE(wifiGen_hapd_isAlive(pAP->pRadio), SWL_RC_INVALID_STATE, ME, "%s: secDmn not ready", pAP->alias);
    ASSERTI_TRUE(wld_wpaCtrlInterface_isReady(pAP->wpaCtrlInterface), SWL_RC_INVALID_STATE, ME, "%s: secDmn iface not ready", pAP->alias);
    swl_trl_e rnrSupp = wld_secDmn_getCfgParamSupp(pAP->pRadio->hostapd, "rnr");
    ASSERTI_NOT_EQUALS(rnrSupp, SWL_TRL_FALSE, SWL_RC_OK, ME, "%s: rnr not configurable", pAP->alias);
    bool enaRnr = (pAP->IEEE80211kEnable && (wld_ap_getDiscoveryMethod(pAP) == M_AP_DM_RNR));
    swl_rc_ne rc = wld_hostapd_ap_sendCfgParam(pAP, "rnr", (enaRnr ? "1" : "0"));
    ASSERTI_TRUE(swl_rc_isOk(rc), rc, ME, "%s: can not apply rnr ena(%d) to hostapd: seems not supported", pAP->alias, enaRnr);
    setBitLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_UPDATE_BEACON);
    wld_autoCommitMgr_notifyVapEdit(pAP);
    return SWL_RC_OK;
}

swl_rc_ne wifiGen_vap_setMldUnit(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    T_Radio* pRad = pAP->pRadio;
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTI_TRUE(wld_rad_isMloCapable(pRad), SWL_RC_OK, ME, "%s: not mlo capable", pRad->Name);
    SAH_TRACEZ_INFO(ME, "%s: applying MLDUnit %d", pAP->alias, pAP->pSSID->mldUnit);
    setBitLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_MOD_MLD);
    return SWL_RC_OK;
}

int wifiGen_vap_mf_sync(T_AccessPoint* vap, int set) {
    swl_rc_ne rc = SWL_RC_OK;
    ASSERTS_TRUE(set & SET, rc, ME, "Only do set");
    if(set & DIRECT) {
        rc = wld_ap_hostapd_setMacFilteringList(vap);
        wld_hostapd_cfgFile_createExt(vap->pRadio);
    } else {
        setBitLongArray(vap->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_MOD_MF_LIST);
    }
    return rc;
}

static void s_syncRelayCredentials(T_AccessPoint* pAP) {
    wld_ap_hostapd_setParamValue(pAP, "skip_cred_build", "0", "WPS");

    T_AccessPoint* relayAP = pAP->wpsSessionInfo.pReferenceApRelay;
    ASSERTI_NOT_NULL(relayAP, , ME, "NULL");
    ASSERTI_TRUE((relayAP != pAP) && pAP->wpsSessionInfo.addRelayApCredentials, , ME, "No relay enabled");

    char wpsRelaySettings[256] = {'\0'};
    size_t wpsRelaySettingsLen = sizeof(wpsRelaySettings);
    swl_rc_ne ret = wpaSupp_buildWpsCredentials(relayAP, wpsRelaySettings, &wpsRelaySettingsLen);
    ASSERTI_EQUALS(ret, SWL_RC_OK, , ME, "Error in wpaSupp_buildWpsCredentials");

    FILE* fptr = fopen("/tmp/wpsRelay.settings", "w");
    ASSERT_NOT_NULL(fptr, , ME, "NULL");

    fwrite(wpsRelaySettings, wpsRelaySettingsLen, 1, fptr);
    fclose(fptr);

    wld_ap_hostapd_setParamValue(pAP, "extra_cred", "/tmp/wpsRelay.settings", "WPS");
    wld_ap_hostapd_setParamValue(pAP, "skip_cred_build", "1", "WPS");
}

swl_rc_ne wifiGen_vap_wps_sync(T_AccessPoint* pAP, char* val, int bufsize, int set) {
    swl_rc_ne rc;
    if(!(set & SET)) {
        if((val != NULL) && (bufsize > 64)) {
            snprintf(val, bufsize, "wps_configured=%s; wps_configmethod=%x",
                     (pAP->WPS_Configured) ? "Configured" : "Un-Configured",
                     pAP->WPS_ConfigMethodsEnabled);
        }

        return SWL_RC_OK;
    }

    // When SSID is hidden... we don't start WPS but STOP as escape...
    if(!pAP->SSIDAdvertisementEnabled || swl_str_matches(val, "STOP")) {
        rc = wld_ap_hostapd_stopWps(pAP);
        ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "%s: fail to stop wps session", pAP->alias);
        wld_ap_sendPairingNotification(pAP, NOTIFY_PAIRING_DONE, WPS_CAUSE_CANCELLED, NULL);
        return SWL_RC_OK;
    }

    s_syncRelayCredentials(pAP);

    if(swl_str_matches(val, "UPDATE")) {
        return SWL_RC_OK;
    }

    /*
     * Empty val or asking for supported one of PushButton methods: start PBC
     */
    if(((swl_str_isEmpty(val)) ||
        (swl_str_matches(val, wld_wps_ConfigMethod_to_string(WPS_CFG_MTHD_PBC))) ||
        (swl_str_matches(val, wld_wps_ConfigMethod_to_string(WPS_CFG_MTHD_PBC_P))) ||
        (swl_str_matches(val, wld_wps_ConfigMethod_to_string(WPS_CFG_MTHD_PBC_V)))) &&
       (pAP->WPS_ConfigMethodsEnabled & (M_WPS_CFG_MTHD_PBC_ALL))) {
        rc = wld_ap_hostapd_startWps(pAP);
        ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "%s: fail to start wps pbc session", pAP->alias);
        //Note, the command to the driver is not sent yet (see set_wps_env below)
        wld_ap_sendPairingNotification(pAP, NOTIFY_PAIRING_READY, WPS_CAUSE_START_WPS_PBC, NULL);
        return SWL_RC_OK;
    }

    if(!swl_str_isEmpty(val)) {
        if(pAP->WPS_ConfigMethodsEnabled & (M_WPS_CFG_MTHD_PIN)) {
            char* clientPIN = NULL;
            if(swl_str_startsWith(val, wld_wps_ConfigMethod_to_string(WPS_CFG_MTHD_PIN))) {
                char* pCh = strchr(val, '=');
                ASSERT_NOT_NULL(pCh, SWL_RC_ERROR, ME, "%s: no wps pin in (%s)", pAP->alias, val);
                pCh++;
                ASSERT_TRUE(wldu_checkWpsPinStr(pCh), SWL_RC_ERROR, ME, "%s: invalid wps pin in (%s)", pAP->alias, val);
                clientPIN = pCh;
            } else if(wldu_checkWpsPinStr(val)) {
                // when command is a valid PIN (format & value), we assume a WPS client pin request (Keypad)
                clientPIN = val;
            }
            if(clientPIN != NULL) {
                rc = wld_ap_hostapd_startWpsPin(pAP, clientPIN, WPS_WALK_TIME_DEFAULT);
                ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "%s: fail to start wps client pin (%s) session", pAP->alias, clientPIN);
                wld_ap_sendPairingNotification(pAP, NOTIFY_PAIRING_READY, WPS_CAUSE_START_WPS_PIN, NULL);
                return SWL_RC_OK;
            }
        }
        if((pAP->WPS_ConfigMethodsEnabled & (M_WPS_CFG_MTHD_LABEL | M_WPS_CFG_MTHD_DISPLAY_ALL)) &&
           ((swl_str_matches(val, wld_wps_ConfigMethod_to_string(WPS_CFG_MTHD_LABEL))) ||
            (swl_str_matches(val, wld_wps_ConfigMethod_to_string(WPS_CFG_MTHD_DISPLAY))) ||
            (swl_str_matches(val, wld_wps_ConfigMethod_to_string(WPS_CFG_MTHD_DISPLAY_P))) ||
            (swl_str_matches(val, wld_wps_ConfigMethod_to_string(WPS_CFG_MTHD_DISPLAY_V))))) {
            rc = wld_ap_hostapd_setWpsApPin(pAP, pAP->pRadio->wpsConst->DefaultPin, WPS_WALK_TIME_DEFAULT);
            ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "%s: fail to start wps ap pin session", pAP->alias);
            //WPS AP Pin is not attached to a pairing session
            return SWL_RC_OK;
        }
        SAH_TRACEZ_ERROR(ME, "%s: unsupported wps sync val (%s)", pAP->alias, val);
        return SWL_RC_ERROR;
    }
    return SWL_RC_OK;
}

swl_rc_ne wifiGen_vap_wps_enable(T_AccessPoint* pAP, int enable, int set) {
    /* WPS enabling requires restarting hostapd */
    if(set & SET) {
        pAP->WPS_Enable = enable;
        setBitLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_MOD_AP);
    }

    return SWL_RC_OK;
}

int wifiGen_vap_wps_labelPin(T_AccessPoint* pAP, int set) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    swl_rc_ne rc = SWL_RC_OK;

    if(set & SET) {
        T_Radio* pRad = pAP->pRadio;
        ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
        if((set & DIRECT) && (wld_wpaCtrlInterface_isReady(pAP->wpaCtrlInterface))) {
            rc = wld_ap_hostapd_setWpsApPin(pAP, pRad->wpsConst->DefaultPin, 0);
        } else {
            setBitLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_MOD_AP);
        }
    }
    return rc;
}

int wifiGen_vap_kick_sta_reason(T_AccessPoint* pAP, char* buf, int bufsize _UNUSED, int reason) {
    ASSERT_TRUE(buf && strlen(buf), SWL_RC_INVALID_PARAM, ME, "Invalid");
    swl_macBin_t bMac;

    SWL_MAC_CHAR_TO_BIN(&bMac, buf);
    SAH_TRACEZ_INFO(ME, "kickmac %s - (%s) rsn %u", pAP->alias, buf, reason);
    return wld_ap_hostapd_kickStation(pAP, &bMac, (swl_IEEE80211deauthReason_ne) reason);
}

swl_rc_ne wifiGen_vap_disassoc_sta_reason(T_AccessPoint* pAP, swl_macBin_t* staMac, int reason) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(staMac, SWL_RC_INVALID_PARAM, ME, "NULL");

    return wld_ap_hostapd_disassocStation(pAP, staMac, reason);
}

int wifiGen_vap_kick_sta(T_AccessPoint* pAP, char* buf, int bufsize, int set _UNUSED) {
    return wifiGen_vap_kick_sta_reason(pAP, buf, bufsize, SWL_IEEE80211_DEAUTH_REASON_AUTH_NO_LONGER_VALID);
}

int wifiGen_vap_updateApStats(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_TRUE(pAP->index > 0, SWL_RC_OK, ME, "%s: no stats to update as iface is not found", pAP->alias);
    ASSERT_NOT_NULL(pAP->pSSID, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_TRUE(wld_linuxIfStats_getVapStats(pAP, &pAP->pSSID->stats), SWL_RC_ERROR,
                ME, "Fail to get stats for AP %s", pAP->alias);
    return SWL_RC_OK;
}

swl_rc_ne wifiGen_vap_requestRrmReport(T_AccessPoint* pAP, const swl_macChar_t* sta, wld_rrmReq_t* req) {
    SAH_TRACEZ_INFO(ME, "%s: send rrm to %s %u/%u %s %s", pAP->alias, sta->cMac, req->operClass, req->channel, req->bssid.cMac, req->ssid);
    return wld_ap_hostapd_requestRRMReport_ext(pAP, sta, req);
}

static swl_rc_ne s_reloadApNeighbors(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");

    uint32_t nSyncAct = 0;

    /*
     * Enable dynamically reduced neighbor reporting (RNR)
     * when it is not yet learned
     * Otherwise, let the rnr conf be applied from saved hostapd config file
     */
    if(wld_secDmn_getCfgParamSupp(pAP->pRadio->hostapd, "rnr") == SWL_TRL_UNKNOWN) {
        nSyncAct += (pAP->pFA->mfn_wvap_set_discovery_method(pAP) == SWL_RC_OK);
    }

    //now add dynamically the saved ap neigbours to hostapd db
    amxc_llist_for_each(it, &pAP->neighbours) {
        T_ApNeighbour* pApNeighbor = amxc_container_of(it, T_ApNeighbour, it);
        nSyncAct += (pAP->pFA->mfn_wvap_updated_neighbour(pAP, pApNeighbor) == SWL_RC_OK);
    }

    if(!nSyncAct) {
        return SWL_RC_DONE;
    }

    //apply list to beacon is RNR is enabled
    setBitLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_UPDATE_BEACON);
    wld_rad_doCommitIfUnblocked(pAP->pRadio);
    return SWL_RC_OK;
}

swl_rc_ne wifiGen_vap_postUpActions(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");

    if(!swl_rc_isOk(wld_autoNeighAdd_vapSetDelNeighbourAP(pAP, pAP->enable))) {
        SAH_TRACEZ_NOTICE(ME, "failed setting AP to neighbor list of other APs");
    }
    if(!swl_rc_isOk(s_reloadApNeighbors(pAP))) {
        SAH_TRACEZ_NOTICE(ME, "failed reloading AP Neighbors");
    }
    if(!swl_rc_isOk(wifiGen_vap_setMboDisallowReason(pAP))) {
        SAH_TRACEZ_NOTICE(ME, "failed setting mbo_assoc_disallow reason");
    }

    wld_ap_hostapd_updateMaxNbrSta(pAP);

    /* create/update stations connected before socket get established */
    if(pAP->pFA->mfn_wvap_get_station_stats(pAP) < SWL_RC_OK) {
        SAH_TRACEZ_ERROR(ME, "%s: get station stats failed", pAP->alias);
    }

    return SWL_RC_OK;
}

swl_rc_ne wifiGen_vap_postDownActions(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");

    if(!swl_rc_isOk(wld_autoNeighAdd_vapSetDelNeighbourAP(pAP, pAP->enable))) {
        SAH_TRACEZ_NOTICE(ME, "failed deleting AP from neighbor list of other APs");
    }
    return SWL_RC_OK;
}
