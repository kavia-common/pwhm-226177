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

#include <errno.h>
#include <net/if.h>
#include "wld/wld.h"
#include "wld/wld_radio.h"
#include "wld/wld_util.h"
#include "wld/wld_ssid.h"
#include "wld/wld_linuxIfUtils.h"
#include "wld/wld_rad_nl80211.h"
#include "swl/swl_common.h"
#include "wifiGen_rad.h"
#include "wifiGen_hapd.h"
#include "wifiGen_events.h"
#include "wifiGen_fsm.h"

#define ME "genRadI"

static int s_applyMac(T_SSID* pSSID, int ifIndex, char* ifName) {
    ASSERT_NOT_NULL(pSSID, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTI_TRUE(ifIndex > 0, SWL_RC_INVALID_STATE, ME, "%s: iface not yet created", ifName);
    SAH_TRACEZ_INFO(ME, "%s: Set mac address "MAC_PRINT_FMT, ifName, MAC_PRINT_ARG(pSSID->MACAddress));
    int ret = wld_linuxIfUtils_updateMac(wld_rad_getSocket(pSSID->RADIO_PARENT), ifName, (swl_macBin_t*) pSSID->MACAddress);
    ASSERT_EQUALS(ret, SWL_RC_OK, ret, ME, "%s: fail to apply mac ["SWL_MAC_FMT "]", ifName, SWL_MAC_ARG(pSSID->MACAddress));
    return ret;
}

int wifiGen_vap_setBssid(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    return s_applyMac(pAP->pSSID, pAP->index, pAP->alias);
}

swl_rc_ne wifiGen_rad_generateVapIfName(T_Radio* pRad, uint32_t ifaceShift, char* ifName, size_t ifNameSize) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    swl_str_copy(ifName, ifNameSize, NULL);
    bool ret = false;
    if(!ifaceShift) {
        ret = swl_str_copy(ifName, ifNameSize, pRad->Name);
    } else {
        ret = swl_str_catFormat(ifName, ifNameSize, "%s.%d", pRad->Name, ifaceShift);
    }
    return (ret ? SWL_RC_OK : SWL_RC_ERROR);
}

static bool s_getNewVapIfaceName(T_Radio* pRad, T_SSID* pSSID, int if_count, char* ifName, size_t ifNameSize) {
    ASSERT_NOT_NULL(pRad, false, ME, "NULL");
    ASSERT_NOT_NULL(pSSID, false, ME, "NULL");
    swl_str_copy(ifName, ifNameSize, NULL);
    char tgtIfname[IFNAMSIZ] = {0};
    if((pSSID != NULL) && (!swl_str_isEmpty(pSSID->customNetDevName))) {
        // use custom vap iface name when provided
        swl_str_copy(tgtIfname, sizeof(tgtIfname), pSSID->customNetDevName);
    } else {
        // generate new vap iface name when supported, or fallback to local implementation
        swl_rc_ne rc = pRad->pFA->mfn_wrad_generateVapIfName(pRad, if_count, tgtIfname, sizeof(tgtIfname));
        if(rc == SWL_RC_NOT_IMPLEMENTED) {
            rc = wifiGen_rad_generateVapIfName(pRad, if_count, tgtIfname, sizeof(tgtIfname));
        }
        ASSERT_TRUE(swl_rc_isOk(rc), false, ME, "%s: fail to generate new vap iface name", pRad->Name);
    }
    T_AccessPoint* pAP;
    T_EndPoint* pEP;
    if((((pAP = wld_vap_from_name(tgtIfname)) != NULL) && (pAP->pSSID != pSSID)) ||
       (((pEP = wld_vep_from_name(tgtIfname)) != NULL) && (pEP->pSSID != pSSID))) {
        SAH_TRACEZ_ERROR(ME, "%s: already added iface %s", pRad->Name, tgtIfname);
        return false;
    }
    return swl_str_copy(ifName, ifNameSize, tgtIfname);
}

static bool s_updateVapIfaceName(T_Radio* pRad, T_AccessPoint* pAP, const char* vapIfName) {
    ASSERT_NOT_NULL(pRad, false, ME, "NULL");
    ASSERT_NOT_NULL(pAP, false, ME, "NULL");
    return swl_str_copy(pAP->alias, sizeof(pAP->alias), vapIfName);
}

int wifiGen_rad_addvapif(T_Radio* pRad, char* ifname, int ifnameSize) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_NOT_NULL(pRad, rc, ME, "NULL");
    ASSERT_STR(ifname, rc, ME, "%s: missing new vapIfName", pRad->Name);
    swl_macBin_t* pMac = NULL;
    T_AccessPoint* pAP = wld_rad_vap_from_name(pRad, ifname);
    if((pAP != NULL) && (pAP->pSSID != NULL) && (!swl_mac_binIsNull((swl_macBin_t*) pAP->pSSID->MACAddress))) {
        pMac = (swl_macBin_t*) pAP->pSSID->MACAddress;
    }
    wld_nl80211_ifaceInfo_t ifaceInfo;
    rc = wld_rad_nl80211_addInterface(pRad, ifname, pMac, false, &ifaceInfo);
    ASSERT_FALSE(rc < SWL_RC_ERROR, rc, ME, "fail to add VAP(%s) on radio(%s)", ifname, pRad->Name);
    swl_str_copy(ifname, ifnameSize, ifaceInfo.name);
    return ifaceInfo.ifIndex;
}

swl_rc_ne wifiGen_rad_addVap(T_Radio* pRad, T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    char* ifname = pAP->alias;
    int ret = pRad->pFA->mfn_wrad_addvapif(pRad, ifname, 0);
    if(ret == SWL_RC_NOT_IMPLEMENTED) {
        SAH_TRACEZ_WARNING(ME, "fallback to default addVapIf impl AP(%s)/Rad(%s)", ifname, pRad->Name);
        ret = wifiGen_rad_addvapif(pRad, ifname, 0);
    }
    ASSERT_FALSE(ret <= 0, SWL_RC_ERROR, ME, "fail to create ap(%s) on rad(%s) ret:%d", ifname, pRad->Name, ret);
    int ifIndex = -1;
    ret = wld_linuxIfUtils_getIfIndex(wld_rad_getSocket(pRad), ifname, &ifIndex);
    ASSERT_FALSE((ret < 0) || (ifIndex <= 0), SWL_RC_ERROR, ME, "fail to get ap(%s) info on rad(%s)", pAP->alias, pRad->Name);
    pAP->index = ifIndex;
    wld_nl80211_ifaceInfo_t ifaceInfo;
    if(wld_nl80211_getInterfaceInfo(wld_nl80211_getSharedState(), ifIndex, &ifaceInfo) >= 0) {
        pAP->wDevId = ifaceInfo.wDevId;
    }
    return SWL_RC_OK;
}

static swl_rc_ne s_createAp(T_Radio* pRad, T_AccessPoint* pAP) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_NOT_NULL(pRad, rc, ME, "NULL");
    ASSERT_NOT_NULL(pAP, rc, ME, "NULL");
    rc = wifiGen_rad_addVap(pRad, pAP);
    ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "fail to create ap(%s) on rad(%s)", pAP->alias, pRad->Name);
    rc = wifiGen_hapd_initVAP(pAP);
    ASSERTS_FALSE(rc < SWL_RC_OK, rc, ME, "%s: fail to init hapd interface", pAP->alias);
    rc = pAP->pFA->mfn_wvap_setEvtHandlers(pAP);
    return rc;
}

static int s_apIfIndex(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, 0, ME, "NULL");
    ASSERTS_NOT_NULL(pAP->pRadio, pAP->ref_index, ME, "NULL");
    return (pAP->pRadio->isSTASup + pAP->ref_index);
}

static uint32_t s_apMacIndex(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, 0, ME, "NULL");
    ASSERT_NOT_NULL(pAP->pSSID, 0, ME, "No mapped SSID");
    ASSERT_NOT_NULL(pAP->pRadio, pAP->pSSID->autoMacRefIndex, ME, "No mapped radio");
    return (pAP->pRadio->isSTASup + pAP->pSSID->autoMacRefIndex);
}

static void s_updateAllMacs(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    // update eps with new shifted mac
    T_EndPoint* tmpEp = NULL;
    wld_rad_forEachEp(tmpEp, pRad) {
        if(!wld_ssid_hasAutoMacBssIndex(tmpEp->pSSID, NULL)) {
            continue;
        }
        swl_macBin_t macBin = SWL_MAC_BIN_NEW();
        wld_ssid_generateMac(pRad, tmpEp->pSSID, tmpEp->pSSID->bssIndex, &macBin);
        wld_ssid_setMac(tmpEp->pSSID, &macBin);
        tmpEp->pFA->mfn_sync_ssid(tmpEp->pSSID->pBus, tmpEp->pSSID, SET);
        //sched to reapply via fsm
        tmpEp->pFA->mfn_wendpoint_set_mac_address(tmpEp);
    }
    // update vaps with new shifted mac
    T_AccessPoint* tmpAp = NULL;
    wld_rad_forEachAp(tmpAp, pRad) {
        if(!wld_ssid_hasAutoMacBssIndex(tmpAp->pSSID, NULL)) {
            continue;
        }
        swl_macBin_t macBin = SWL_MAC_BIN_NEW();
        wld_ssid_generateMac(pRad, tmpAp->pSSID, tmpAp->pSSID->bssIndex, &macBin);
        wld_ssid_setBssid(tmpAp->pSSID, &macBin);
        tmpAp->pFA->mfn_sync_ssid(tmpAp->pSSID->pBus, tmpAp->pSSID, SET);
        //sched to reapply via fsm
        tmpAp->pFA->mfn_wvap_bssid(pRad, tmpAp, (uint8_t*) swl_typeMacBin_toBuf32Ref(&macBin).buf, SWL_MAC_CHAR_LEN, SET);
    }
}

static void s_applyAllMacs(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    //Brcm requires to re-apply the primary config's cur_etheraddr,
    //in order to clear all previously set secondary config ethernet addresses
    //and allow setting them again with shifted MACs
    //1) if the first interface is the primary vap, then the mac restore is implicitly done, when re-writing bssid.
    //2) otherwise (i.e endpoint is first interface), re-apply mac on radio (as endpoint object may be not yet created on startup)
    if(pRad->isSTASup) {
        wld_linuxIfUtils_updateMac(wld_rad_getSocket(pRad), pRad->Name, (swl_macBin_t*) pRad->MACAddr);
    }
    // apply new shifted mac to eps
    T_EndPoint* tmpEp = NULL;
    wld_rad_forEachEp(tmpEp, pRad) {
        s_applyMac(tmpEp->pSSID, tmpEp->index, tmpEp->Name);
    }
    // apply new shifted mac to vaps
    // actually: only secondary interfaces are impacted (i.e: the radio base mac addr does not change)
    T_AccessPoint* tmpAp = NULL;
    wld_rad_forEachAp(tmpAp, pRad) {
        s_applyMac(tmpAp->pSSID, tmpAp->index, tmpAp->alias);
    }
}

static void s_checkAndProcessMbssBMacChange(T_Radio* pRad, uint32_t apMacIndex) {
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    ASSERTS_TRUE(apMacIndex > 0, , ME, "no mbss baseMac shift for primary bss");
    swl_macBin_t prevMbssMacAddr = pRad->mbssBaseMACAddr;
    bool changed = wld_rad_macCfg_updateRadBaseMac(pRad);
    changed |= wld_rad_macCfg_shiftMbssIfNotEnoughVaps(pRad, apMacIndex);
    changed |= (!swl_mac_binMatches(&prevMbssMacAddr, &pRad->mbssBaseMACAddr));
    if(changed) {
        SAH_TRACEZ_WARNING(ME, "%s: Shifting neigh vap/ep interfaces after mbss mac shift ", pRad->Name);
        s_updateAllMacs(pRad);
    }
    if((!pRad->macCfg.useBaseMacOffset) && (!pRad->macCfg.useLocalBitForGuest) && (pRad != wld_lastRadFromObjs())) {
        T_Radio* pLastRad = pRad;
        T_Radio* pNextRad = NULL;
        for(pNextRad = wld_rad_nextRadFromObj(pRad->pBus); pNextRad; pNextRad = wld_rad_nextRadFromObj(pNextRad->pBus)) {
            if(pNextRad->macCfg.useBaseMacOffset) {
                continue;
            }
            swl_macBin_t prevBMac = pNextRad->mbssBaseMACAddr;
            wld_rad_macCfg_updateRadBaseMac(pNextRad);
            if(swl_mac_binMatches(&pNextRad->mbssBaseMACAddr, &prevBMac)) {
                break;
            }
            SAH_TRACEZ_WARNING(ME, "%s: updating next rad child vap/ep interfaces after mbss mac shift ", pNextRad->Name);
            s_updateAllMacs(pNextRad);
            pLastRad = pNextRad;
        }
        for(pNextRad = pLastRad; pNextRad && (pNextRad != pRad); pNextRad = wld_rad_prevRadFromObj(pNextRad->pBus)) {
            if(pNextRad->macCfg.useBaseMacOffset) {
                continue;
            }
            SAH_TRACEZ_WARNING(ME, "%s: applying next rad new macs after mbss mac shift ", pNextRad->Name);
            s_applyAllMacs(pNextRad);
            pNextRad->fsmRad.FSM_SyncAll = TRUE;
            wld_rad_doRadioCommit(pNextRad);
        }
    }
    if(changed) {
        SAH_TRACEZ_WARNING(ME, "%s: applying neigh vap/ep interfaces after mbss mac shift ", pRad->Name);
        s_applyAllMacs(pRad);
    }
}

int wifiGen_rad_addVapExt(T_Radio* pRad, T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pAP->pSSID, SWL_RC_INVALID_PARAM, ME, "NULL");
    uint32_t apMacRefIndex = pAP->pSSID->autoMacRefIndex;

    // Ap index is number of AP already initialized plus 1 if sta is supported, 0 otherwise.
    int32_t apIfIndex = s_apIfIndex(pAP);
    uint32_t apMacIndex = s_apMacIndex(pAP);
    SAH_TRACEZ_INFO(ME, "%s: add vap - [%s] ap ifIdx %u macIdx %u", pRad->Name, pAP->name, apIfIndex, apMacIndex);

    uint32_t maxNrAp = pRad->maxNrHwBss;
    ASSERT_FALSE(apMacRefIndex >= maxNrAp, SWL_RC_INVALID_STATE, ME, "%s: Max Number of BSS reached : %d", pRad->Name, maxNrAp);
    ASSERT_FALSE(wld_rad_getSocket(pRad) < 0, SWL_RC_INVALID_STATE, ME, "%s: Unable to create socket", pRad->Name);

    char vapIfname[IFNAMSIZ] = {0};
    //build new vap interface name and check that it is unique
    ASSERT_TRUE(s_getNewVapIfaceName(pRad, pAP->pSSID, apIfIndex, vapIfname, sizeof(vapIfname)), SWL_RC_ERROR,
                ME, "%s: fail to get new vap %s iface name", pRad->Name, pAP->name);

    pRad->pFA->mfn_wrad_enable(pRad, 0, SET | DIRECT);

    //check need for updating base mac when adding next interface
    s_checkAndProcessMbssBMacChange(pRad, apMacIndex);

    // retrieve interface name and update pAP->alias
    s_updateVapIfaceName(pRad, pAP, vapIfname);
    // generate BSSID and save it
    swl_macBin_t macBin = SWL_MAC_BIN_NEW();
    wld_ssid_generateBssid(pRad, pAP, apMacIndex, &macBin);
    wld_ssid_setBssid(pAP->pSSID, &macBin);
    // actually create AP, and update netdev index.
    s_createAp(pRad, pAP);

    /* Set network address! */
    wifiGen_vap_setBssid(pAP);

    pRad->pFA->mfn_wrad_enable(pRad, pRad->enable, SET);

    SAH_TRACEZ_INFO(ME, "%s: created interface %s (%s) netdevIdx %d - ifIdx %u macIdx %u",
                    pRad->Name, pAP->name, pAP->alias, pAP->index, apIfIndex, apMacIndex);

    return SWL_RC_OK;
}

int wifiGen_rad_delvapif(T_Radio* pRad, char* vapName) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_STR(vapName, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTW_FALSE(swl_str_matches(pRad->Name, vapName), SWL_RC_OK, ME, "%s: avoid deleting main rad iface", vapName);
    int ifIndex = if_nametoindex(vapName);
    ASSERT_TRUE(ifIndex > 0, SWL_RC_INVALID_PARAM, ME, "unknown iface (%s)", vapName);
    swl_rc_ne rc = wld_nl80211_delInterface(wld_nl80211_getSharedState(), ifIndex);
    ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "fail to del vap (%s)", vapName);
    return SWL_RC_OK;
}

swl_rc_ne wifiGen_rad_generateEpIfName(T_Radio* pRad, uint32_t ifaceShift _UNUSED, char* ifName, size_t ifNameSize) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    // assumption: endpoint interface is the MAIN radio interface
    bool ret = swl_str_copy(ifName, ifNameSize, pRad->Name);
    return (ret ? SWL_RC_OK : SWL_RC_ERROR);
}

static bool s_getNewEpIfaceName(T_Radio* pRad, T_SSID* pSSID, int if_count, char* ifName, size_t ifNameSize) {
    ASSERT_NOT_NULL(pRad, false, ME, "NULL");
    ASSERT_NOT_NULL(pSSID, false, ME, "NULL");
    swl_str_copy(ifName, ifNameSize, NULL);
    char tgtIfname[IFNAMSIZ] = {0};
    if((pSSID != NULL) && (!swl_str_isEmpty(pSSID->customNetDevName))) {
        // use custom endpoint iface name when provided
        swl_str_copy(tgtIfname, sizeof(tgtIfname), pSSID->customNetDevName);
    } else {
        // generate new vap iface name when supported, or fallback to local implementation
        swl_rc_ne rc = pRad->pFA->mfn_wrad_generateEpIfName(pRad, if_count, tgtIfname, sizeof(tgtIfname));
        if(rc == SWL_RC_NOT_IMPLEMENTED) {
            rc = wifiGen_rad_generateEpIfName(pRad, if_count, tgtIfname, sizeof(tgtIfname));
        }
        ASSERT_TRUE(swl_rc_isOk(rc), false, ME, "%s: fail to generate new endpoint iface name", pRad->Name);
    }
    if((wld_vap_from_name(tgtIfname) != NULL) || (wld_vep_from_name(tgtIfname) != NULL)) {
        SAH_TRACEZ_ERROR(ME, "%s: already added iface %s", pRad->Name, tgtIfname);
        return false;
    }
    return swl_str_copy(ifName, ifNameSize, tgtIfname);
}

int wifiGen_rad_addEndpointIf(T_Radio* pRad, char* buf, int bufsize) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(buf, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_TRUE(bufsize > 0, SWL_RC_INVALID_PARAM, ME, "null size");

    // assumption: endpoint interface is the MAIN radio interface
    char epIfname[IFNAMSIZ] = {0};
    T_SSID* pSSID = NULL;
    T_EndPoint* pEP;
    wld_rad_forEachEp(pEP, pRad) {
        ASSERT_FALSE(pEP->index > 0, SWL_RC_ERROR, ME, "%s: rad has already EP %s created: can not add new one", pRad->Name, pEP->Name);
        if(swl_str_isEmpty(pEP->Name)) {
            pSSID = pEP->pSSID;
            break;
        }
    }
    // get new endpoint interface name and check that it is unique
    ASSERT_TRUE(s_getNewEpIfaceName(pRad, pSSID, wld_rad_countIfaces(pRad), epIfname, sizeof(epIfname)), SWL_RC_ERROR,
                ME, "%s: fail to get new endpoint iface name", pRad->Name);
    swl_str_copy(buf, bufsize, epIfname);
    wld_nl80211_ifaceInfo_t ifaceInfo;
    memset(&ifaceInfo, 0, sizeof(ifaceInfo));
    swl_macBin_t epMacAddr = SWL_MAC_BIN_NEW();
    wld_ssid_generateMac(pRad, pSSID, 0, &epMacAddr);
    wld_linuxIfUtils_setState(wld_rad_getSocket(pRad), pRad->Name, false);
    if((pRad->isSTA) && (swl_str_matches(pRad->Name, epIfname))) {
        wld_rad_nl80211_setSta(pRad);
        wld_nl80211_getInterfaceInfo(wld_nl80211_getSharedState(), pRad->index, &ifaceInfo);
    } else {
        swl_rc_ne rc = wld_nl80211_newInterface(wld_nl80211_getSharedState(), pRad->index, epIfname, NULL, false, true, &ifaceInfo);
        ASSERT_TRUE(swl_rc_isOk(rc), rc, ME, "%s: fail to create new ep iface %s", pRad->Name, epIfname);
        wld_linuxIfUtils_setMac(wld_rad_getSocket(pRad), epIfname, &epMacAddr);
    }
    wld_nl80211_setInterfaceUse4Mac(wld_nl80211_getSharedState(), ifaceInfo.ifIndex, (pEP != NULL) ? pEP->multiAPEnable : false);
    if(pEP != NULL) {
        pEP->index = ifaceInfo.ifIndex;
        pEP->wDevId = ifaceInfo.wDevId;
        wld_ssid_setMac(pEP->pSSID, &epMacAddr);
    }

    return ifaceInfo.ifIndex;
}

swl_rc_ne wifiGen_rad_delendpointif(T_Radio* pRad _UNUSED, char* endpoint) {
    ASSERT_STR(endpoint, SWL_RC_INVALID_PARAM, ME, "NULL");
    int ifIndex = -1;
    swl_rc_ne rc = wld_linuxIfUtils_getIfIndexExt(endpoint, &ifIndex);
    ASSERT_FALSE((rc < SWL_RC_OK) || (ifIndex <= 0), rc, ME, "no ifIndex for endpoint %s", endpoint);
    ASSERT_NOT_EQUALS(pRad->index, ifIndex, SWL_RC_ERROR, ME, "%s: can not delete main rad iface", endpoint);
    SAH_TRACEZ_WARNING(ME, "%s: deleting endpoint %s ifIndex %d", pRad->Name, endpoint, ifIndex);
    rc = wld_nl80211_delInterface(wld_nl80211_getSharedState(), ifIndex);
    return rc;
}

int wifiGen_vap_bssid(T_Radio* pRad, T_AccessPoint* pAP, unsigned char* buf, int bufsize, int set) {
    ASSERT_FALSE((pRad == NULL) && (pAP == NULL), SWL_RC_INVALID_PARAM, ME, "NULL");
    T_SSID* pSSID = NULL;
    SAH_TRACEZ_INFO(ME, "%p - %p - %p - %d - %d", pRad, pAP, buf, bufsize, set);
    if(set & SET) {
        if(pAP) {
            pSSID = (T_SSID*) pAP->pSSID;
            ASSERT_NOT_NULL(pSSID, SWL_RC_ERROR, ME, "NULL");
            ASSERT_STR(buf, SWL_RC_INVALID_PARAM, ME, "Empty bssid");
            ASSERT_TRUE(SWL_MAC_CHAR_TO_BIN(pSSID->BSSID, buf),
                        SWL_RC_ERROR, ME, "fail to convert mac(%s) len(%d)", buf, bufsize);
            setBitLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_MOD_BSSID);
            if(pSSID->bssIndex == 0) {
                // if changing first VAP, all secondary vaps also must have their BSSID set.
                setBitLongArray(pAP->pRadio->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_MOD_BSSID);
                return SWL_RC_OK;
            }
        } else {
            wld_rad_macCfg_updateRadBaseMac(pRad);
            wld_linuxIfUtils_updateMac(wld_rad_getSocket(pRad), pRad->Name, (swl_macBin_t*) pRad->MACAddr);
        }
    } else {
        ASSERTS_NOT_NULL(buf, SWL_RC_INVALID_PARAM, ME, "NULL");
        ASSERTS_TRUE((bufsize >= ETHER_ADDR_STR_LEN), SWL_RC_INVALID_PARAM, ME, "Not enough space");
        swl_macBin_t* srcBMac = NULL;
        if(pAP) {
            pSSID = (T_SSID*) pAP->pSSID;
            ASSERT_NOT_NULL(pSSID, SWL_RC_ERROR, ME, "NULL");
            srcBMac = (swl_macBin_t*) pSSID->BSSID;
        } else {
            srcBMac = (swl_macBin_t*) pRad->MACAddr;
        }
        // String formatted or octet based ?
        swl_mac_binToCharSep((swl_macChar_t*) buf, srcBMac, false, ':');
        SAH_TRACEZ_INFO(ME, "%s", buf);
    }
    return SWL_RC_OK;
}

