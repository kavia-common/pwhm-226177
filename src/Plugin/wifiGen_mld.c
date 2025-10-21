/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2024 SoftAtHome
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
#include "wld/wld_util.h"
#include "wld/wld_radio.h"
#include "wld/wld_ssid.h"
#include "wld/wld_accesspoint.h"
#include "wld/wld_linuxIfUtils.h"
#include "wld/wld_wpaCtrl_api.h"
#include "wld/wld_wpaCtrlMngr.h"
#include "wld/wld_nl80211_api.h"
#include "wld/wld_hostapd_ap_api.h"
#include "wld/Utils/wld_autoCommitMgr.h"
#include "wifiGen_hapd.h"

#define ME "genMld"

static T_SSID* s_fetchLinkSSIDWithNl80211(const char* mldIface, int32_t linkId) {
    int ifIndex = -1;
    int ret = wld_linuxIfUtils_getIfIndexExt((char*) mldIface, &ifIndex);
    ASSERTS_TRUE(ret >= 0, NULL, ME, "fail to get iface %s index", mldIface);
    wld_nl80211_ifaceInfo_t ifaceInfo;
    memset(&ifaceInfo, 0, sizeof(ifaceInfo));
    ret = wld_nl80211_getInterfaceInfo(wld_nl80211_getSharedState(), ifIndex, &ifaceInfo);
    ASSERTS_TRUE(swl_rc_isOk(ret), NULL, "fail to get nl iface %s info", mldIface);
    swl_macBin_t* pMac = NULL;
    if((ifaceInfo.nMloLinks > 0) && (linkId >= 0)) {
        wld_nl80211_ifaceMloLinkInfo_t* pLinkInfo =
            (wld_nl80211_ifaceMloLinkInfo_t* ) wld_nl80211_fetchIfaceMloLinkById(&ifaceInfo, linkId);
        ASSERTI_NOT_NULL(pLinkInfo, NULL, ME, "mld iface(%s) linkId(%d) is not found", mldIface, linkId);
        pMac = &pLinkInfo->link.linkMac;
    } else {
        pMac = &ifaceInfo.mac;
    }
    ASSERTS_FALSE(swl_mac_binIsNull(pMac), NULL, ME, "iface(%s) linkId(%d) has null mac", mldIface, linkId);
    T_SSID* pSSID = wld_ssid_getSsidByMacAddress(pMac);
    ASSERTS_NOT_NULL(pSSID, NULL, ME, "iface(%s) linkId(%d) has no ssid ctx", mldIface, linkId);
    SAH_TRACEZ_INFO(ME, "fetch with NL80211: iface(%s) linkId(%d) => linkMac(%s) => pSSID(%s)",
                    mldIface, linkId,
                    swl_typeMacBin_toBuf32Ref(pMac).buf, pSSID->Name);
    return pSSID;
}

static T_SSID* s_fetchLinkSSIDWithWpaCtrl(const char* mldIface, int32_t linkId, const char* sockName) {
    T_SSID* pLinkSSID = NULL;
    T_SSID* pMldSSID = wld_ssid_getSsidByIfName(mldIface);
    ASSERTS_NOT_NULL(pMldSSID, NULL, ME, "NULL");
    if(pMldSSID->AP_HOOK != NULL) {
        T_AccessPoint* pAPLink = wifiGen_hapd_fetchSockApLink(pMldSSID->AP_HOOK, sockName);
        ASSERTI_NOT_NULL(pAPLink, NULL, ME, "no APLink for mld %s link %d (sock %s)", mldIface, linkId, sockName);
        SAH_TRACEZ_INFO(ME, "Found APLink %s for mld %s link %d (sock %s)", pAPLink->alias, mldIface, linkId, sockName);
        pLinkSSID = pAPLink->pSSID;
    } else if(pMldSSID->ENDP_HOOK != NULL) {
        /*
         * TODO: manage STA link fetch with wpa_supplicant cmds
         */
        SAH_TRACEZ_ERROR(ME, "no STALink can be fetched for mld %s link %d (sock %s)", mldIface, linkId, sockName);
        return NULL;
    }
    ASSERTI_NOT_NULL(pLinkSSID, NULL, ME, "iface(%s) linkId(%d) (sock %s) has no ssid", mldIface, linkId, sockName);
    SAH_TRACEZ_INFO(ME, "fetch with CUSTOM wpa: iface(%s) linkId(%d) sock(%s) => pSSID(%s)",
                    mldIface, linkId, sockName, pLinkSSID->Name);
    return pLinkSSID;
}

static T_SSID* s_fetchLinkSSID(const char* mldIface, int32_t linkId, const char* sockName) {
    T_SSID* pSSID = wld_ssid_getSsidByIfName(mldIface);
    ASSERT_NOT_NULL(pSSID, NULL, ME, "no ssid is matching iface(%s) linkId(%d) of sock(%s)",
                    mldIface, linkId, sockName);
    if(linkId < 0) {
        ASSERTI_FALSE(wld_mld_isLinkEnabled(pSSID->pMldLink), NULL,
                      ME, "skip sock(%s): wait for mld link socket for ssid %s", sockName, pSSID->Name);
        SAH_TRACEZ_INFO(ME, "fetch DIRECT: iface(%s) linkId(%d) sock(%s) => pSSID(%s)",
                        mldIface, linkId, sockName, pSSID->Name);
        return pSSID;
    }
    ASSERT_TRUE(pSSID->mldUnit >= 0, NULL, ME, "iface(%s) linkId(%d) sock(%s) => ssid(%s) is not mld member",
                mldIface, linkId, sockName, pSSID->Name);
    if(((pSSID = s_fetchLinkSSIDWithNl80211(mldIface, linkId)) != NULL) ||
       ((pSSID = s_fetchLinkSSIDWithWpaCtrl(mldIface, linkId, sockName)) != NULL)) {
        return pSSID;
    }
    ASSERTW_NOT_NULL(pSSID, pSSID, ME, "sock(%s): Fail to match any SSID", sockName);
    return NULL;
}


T_SSID* wifiGen_mld_fetchSockLinkSSID(wld_wpaCtrlMngr_t* pMgr, const char* sockName) {
    ASSERT_STR(sockName, NULL, ME, "empty socket name");
    const char* refIface = wld_wpaCtrlInterface_getName(wld_wpaCtrlMngr_getFirstInterface(pMgr));
    ASSERT_STR(refIface, NULL, ME, "Fail to get reference wpactrl iface (pMgr:%p)", pMgr);

    char mldIface[swl_str_len(sockName) + 1];
    memset(mldIface, 0, sizeof(mldIface));
    int32_t linkId = -1;
    if(wld_vap_from_name(refIface) != NULL) {
        wifiGen_hapd_parseSockName(sockName, mldIface, sizeof(mldIface), &linkId);
    } else {
        SAH_TRACEZ_WARNING(ME, "not found parser for socket name (%s)", sockName);
        return NULL;
    }
    ASSERT_STR(mldIface, NULL, ME, "fail to parse sockName (%s)", sockName);

    T_SSID* pLinkSSID = s_fetchLinkSSID(mldIface, linkId, sockName);
    ASSERTS_NOT_NULL(pLinkSSID, NULL, ME, "sock(%s): Fail to match any SSID", sockName);

    if(wld_mld_getLinkId(pLinkSSID->pMldLink) != linkId) {
        wld_mld_setLinkId(pLinkSSID->pMldLink, linkId);
        const char* pLinkIfName = wld_ssid_getIfName(pLinkSSID);
        if(swl_str_matches(pLinkIfName, mldIface)) {
            wld_mld_setPrimaryLink(pLinkSSID->pMldLink);
        }
    }
    return pLinkSSID;
}

static wld_mldLink_t* s_selectPrimaryLink(T_SSID* pSSID) {
    ASSERTS_NOT_NULL(pSSID, NULL, ME, "NULL");
    wld_mldLink_t* pLink = pSSID->pMldLink;
    ASSERTI_TRUE(wld_mld_isLinkUsable(pLink), NULL, ME, "no primary link can be selected for link %s", pSSID->Name);

    wld_mldLink_t* pPrimLink = NULL;
    wld_mldLink_t* pNgLink = NULL;
    wld_for_eachNeighMldLink(pNgLink, pLink) {
        SAH_TRACEZ_INFO(ME, "link %s, ngLink %s usable:%d configured:%d",
                        wld_mld_getLinkName(pLink), wld_mld_getLinkName(pNgLink),
                        wld_mld_isLinkUsable(pNgLink), wld_mld_isLinkConfigured(pNgLink));
        if(wld_mld_isLinkUsable(pNgLink) && wld_mld_isLinkConfigured(pNgLink)) {
            pPrimLink = pNgLink;
            break;
        }
    }

    ASSERTI_NOT_NULL(pPrimLink, NULL, ME, "%s: no primary link can be selected for mld unit(%d)", pSSID->Name, pSSID->mldUnit);
    SAH_TRACEZ_INFO(ME, "%s: select primary link (%s) for mld unit(%d)",
                    pSSID->Name, wld_mld_getLinkName(pPrimLink), pSSID->mldUnit);

    return pPrimLink;
}

T_SSID* wifiGen_mld_selectPrimLinkSSID(T_SSID* pSSID) {
    ASSERTS_NOT_NULL(pSSID, NULL, ME, "NULL");
    if(!wld_mld_isLinkUsable(pSSID->pMldLink)) {
        return pSSID;
    }
    wld_mldLink_t* pCurPrimLink = wld_mld_getPrimaryLink(pSSID->pMldLink);
    if(wld_mld_isLinkConfigured(pCurPrimLink)) {
        SAH_TRACEZ_INFO(ME, "SSID %s has already a primLink %s",
                        pSSID->Name, wld_mld_getLinkName(pCurPrimLink));
        return wld_mld_getLinkSsid(pCurPrimLink);
    }
    wld_mldLink_t* pPrimLink = s_selectPrimaryLink(pSSID);
    if(pPrimLink != NULL) {
        SAH_TRACEZ_INFO(ME, "SSID %s select primLink %s",
                        pSSID->Name, wld_mld_getLinkName(pPrimLink));
        return wld_mld_getLinkSsid(pPrimLink);
    }
    return pSSID;
}

static wld_mldLink_t* s_getHapdCfgPrimAPMldLink(T_AccessPoint* pAP) {
    const char* primIface = wld_hostapd_ap_selectApLinkIface(pAP);
    T_SSID* pPrimSSID = wld_ssid_getSsidByIfName(primIface);
    return (pPrimSSID ? pPrimSSID->pMldLink : NULL);
}

swl_rc_ne wifiGen_mld_reconfigureNeighLinkSSIDs(T_SSID* pSSID) {
    ASSERTS_NOT_NULL(pSSID, SWL_RC_INVALID_PARAM, ME, "NULL");
    if(!wld_rad_firstCommitFinished(pSSID->RADIO_PARENT)) {
        SAH_TRACEZ_INFO(ME, "%s: still in first commit, no need to reconfigure", pSSID->Name);
        return SWL_RC_OK;
    }

    wld_mldLink_t* pLink = pSSID->pMldLink;
    T_AccessPoint* aNgAPs[wld_mld_countNeighLinks(pLink) + 1];
    memset(aNgAPs, 0, sizeof(aNgAPs));
    size_t nNgAPs = 0;
    wld_for_eachNeighMldLink_safe(pNgLink, pLink) {
        T_SSID* pNgSSID = NULL;
        if((pNgLink != pLink) && ((pNgSSID = wld_mld_getLinkSsid(pNgLink)) != NULL) && (pNgSSID->AP_HOOK != NULL)) {
            aNgAPs[nNgAPs++] = pNgSSID->AP_HOOK;
        }
    }
    for(size_t i = 0; i < nNgAPs; i++) {
        T_AccessPoint* pNgAP = aNgAPs[i];
        if(pNgAP && pNgAP->pSSID && pNgAP->pSSID->pMldLink) {
            wld_mldLink_t* pNgLink = pNgAP->pSSID->pMldLink;
            bool isConfigured = wld_mld_isLinkConfigured(pNgLink);
            bool isUsable = wld_mld_isLinkUsable(pNgLink);
            wld_mldLink_t* pPrimLink = s_getHapdCfgPrimAPMldLink(pNgAP);
            bool needUpdateConf = false;
            if(isConfigured != isUsable) {
                needUpdateConf = isConfigured;
            } else if(isConfigured && (pPrimLink == pLink)) {
                needUpdateConf = true;
            } else {
                continue;
            }
            if(needUpdateConf) {
                wld_mld_saveLinkConfigured(pNgLink, false);
            }
            SAH_TRACEZ_INFO(ME, "reconfigure Neigh AP mld %s (triggered by change in %s)",
                            wld_mld_getLinkName(pNgLink), pSSID->Name);
            pNgAP->pFA->mfn_wvap_setMldUnit(pNgAP);
            wld_autoCommitMgr_notifyVapEdit(pNgAP);
        }
    }
    if(pSSID->AP_HOOK != NULL) {
        T_AccessPoint* pAP = pSSID->AP_HOOK;
        SAH_TRACEZ_INFO(ME, "mark AP %s for mld conf refresh", pSSID->Name);
        pAP->pFA->mfn_wvap_setMldUnit(pAP);
    }
    return SWL_RC_OK;
}

