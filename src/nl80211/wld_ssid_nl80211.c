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
/*
 * This file implements wrapper functions to use nl80211 generic apis with T_SSID context
 */

#include "wld_ssid_nl80211_priv.h"
#include "swl/swl_common.h"
#include "wld_mld.h"
#include "wld_radio.h"

#define ME "nlSSID"

swl_rc_ne wld_ssid_nl80211_getMldIfaceInfo(T_SSID* pSSID, wld_nl80211_ifaceInfo_t* pMldIfaceInfo, int32_t* pLinkId) {
    wld_nl80211_ifaceInfo_t mldIfaceInfo;
    memset(&mldIfaceInfo, 0, sizeof(mldIfaceInfo));
    int32_t ifIndex = -1;
    int32_t linkId = -1;
    W_SWL_SETPTR(pMldIfaceInfo, mldIfaceInfo);
    W_SWL_SETPTR(pLinkId, linkId);
    ASSERT_NOT_NULL(pSSID, SWL_RC_INVALID_PARAM, ME, "No ssid");
    wld_mldLink_t* pLink = pSSID->pMldLink;
    ASSERTI_NOT_NULL(pLink, SWL_RC_INVALID_STATE, ME, "No link");
    swl_macBin_t* pLinkMac = (swl_macBin_t*) pSSID->MACAddress;
    if(((wld_mld_isLinkActive(pLink)) &&
        ((ifIndex = wld_mld_getPrimaryLinkIfIndex(pLink)) > 0) &&
        ((linkId = wld_mld_getLinkId(pLink)) >= 0) &&
        (wld_nl80211_getInterfaceInfo(wld_nl80211_getSharedState(), ifIndex, &mldIfaceInfo) >= SWL_RC_OK)) ||
       (wld_nl80211_findMldIfaceByLinkMac(wld_nl80211_getSharedState(), pLinkMac, &mldIfaceInfo, &linkId) >= SWL_RC_OK)) {
        ifIndex = mldIfaceInfo.ifIndex;
        wld_nl80211_ifaceMloLinkInfo_t* pIfaceMloLinkInfo = (wld_nl80211_ifaceMloLinkInfo_t*) wld_nl80211_fetchIfaceMloLinkById(&mldIfaceInfo, linkId);
        ASSERT_NOT_NULL(pIfaceMloLinkInfo, SWL_RC_ERROR, ME, "unmatch linkId(%d) in mld iface(ifName:%s)(ifId:%d) nLinks(%d)",
                        linkId, mldIfaceInfo.name, mldIfaceInfo.ifIndex, mldIfaceInfo.nMloLinks);
        ASSERT_TRUE(swl_typeMacBin_equalsRef(pLinkMac, &pIfaceMloLinkInfo->link.linkMac), SWL_RC_ERROR,
                    ME, "unmatch linkMac(%s) in mld iface(ifName:%s)(ifId:%d) linkId(%d/%d) linkMac(%s)",
                    swl_typeMacBin_toBuf32Ref(pLinkMac).buf,
                    mldIfaceInfo.name, mldIfaceInfo.ifIndex, linkId, mldIfaceInfo.nMloLinks,
                    swl_typeMacBin_toBuf32Ref(&pIfaceMloLinkInfo->link.linkMac).buf);
        pLinkMac = &pIfaceMloLinkInfo->link.linkMac;
        mldIfaceInfo.chanSpec = pIfaceMloLinkInfo->chanSpec;
        mldIfaceInfo.txPower = pIfaceMloLinkInfo->txPower;
        W_SWL_SETPTR(pMldIfaceInfo, mldIfaceInfo);
        W_SWL_SETPTR(pLinkId, linkId);
        swl_chanspec_t swlChanSpec = SWL_CHANSPEC_EMPTY;
        wld_nl80211_chanSpecNlToSwl(&swlChanSpec, &pIfaceMloLinkInfo->chanSpec);
        SAH_TRACEZ_INFO(ME, "SSID[%s] is mapped to mld:(ifName(%s),ifIndex(%d)) link[%d/%d]:(mac(%s),chspec(%s))",
                        pSSID->Name,
                        mldIfaceInfo.name, ifIndex,
                        linkId, mldIfaceInfo.nMloLinks,
                        swl_typeMacBin_toBuf32Ref(pLinkMac).buf, swl_typeChanspecExt_toBuf32Ref(&swlChanSpec).buf);
        return SWL_RC_OK;
    }
    SAH_TRACEZ_INFO(ME, "SSID[%s] is not mapped to any mld (id:%d) link (id:%d)",
                    pSSID->Name, ifIndex, linkId);
    return SWL_RC_NOT_AVAILABLE;
}

swl_rc_ne wld_ssid_nl80211_getInterfaceInfo(T_SSID* pSSID, wld_nl80211_ifaceInfo_t* pIfaceInfo) {
    ASSERT_NOT_NULL(pSSID, SWL_RC_INVALID_PARAM, ME, "No ssid");
    swl_rc_ne rc = SWL_RC_NOT_AVAILABLE;
    if(wld_mld_isLinkEnabled(pSSID->pMldLink) || wld_mld_isLinkActive(pSSID->pMldLink)) {
        rc = wld_ssid_nl80211_getMldIfaceInfo(pSSID, pIfaceInfo, NULL);
    }
    if(rc == SWL_RC_NOT_AVAILABLE) {
        wld_nl80211_ifaceInfo_t ifaceInfo;
        memset(&ifaceInfo, 0, sizeof(ifaceInfo));
        int32_t tgtIfIndex = wld_ssid_nl80211_getPrimaryLinkIfIndex(pSSID);
        ASSERTS_TRUE(tgtIfIndex > 0, rc, ME, "%s: no tgt ifIndex", pSSID->Name);
        rc = wld_nl80211_getInterfaceInfo(wld_nl80211_getSharedState(), tgtIfIndex, &ifaceInfo);
        ASSERTS_TRUE(swl_rc_isOk(rc), rc, ME, "fail to get iface info");
        W_SWL_SETPTR(pIfaceInfo, ifaceInfo);
    }
    return rc;
}

uint32_t wld_ssid_nl80211_getPrimaryLinkIfIndex(T_SSID* pSSID) {
    int32_t ifIndex = 0;
    ASSERTS_NOT_NULL(pSSID, ifIndex, ME, "NULL");
    if(wld_mld_isLinkActive(pSSID->pMldLink)) {
        ifIndex = wld_mld_getPrimaryLinkIfIndex(pSSID->pMldLink);
    } else {
        ifIndex = wld_ssid_getIfIndex(pSSID);
    }
    return SWL_MAX(0, ifIndex);
}

int8_t wld_ssid_nl80211_getMldLinkId(T_SSID* pSSID) {
    ASSERTS_NOT_NULL(pSSID, MLO_LINK_ID_UNKNOWN, ME, "NULL");
    return wld_mld_getLinkId(pSSID->pMldLink);
}

bool wld_ssid_nl80211_matchIfSta(T_SSID* pSSID, wld_nl80211_stationInfo_t* pStationInfo) {
    ASSERTS_NOT_NULL(pSSID, false, ME, "NULL");
    ASSERTS_NOT_NULL(pStationInfo, false, ME, "NULL");
    ASSERTW_FALSE(swl_mac_binIsNull(&pStationInfo->macAddr), false, ME, "null mac addr");
    int16_t linkId = wld_ssid_nl80211_getMldLinkId(pSSID);
    if((pStationInfo->nrLinks > 0) && (linkId >= 0)) {
        //match stations connected to MLD link
        for(uint32_t i = 0; i < pStationInfo->nrLinks; i++) {
            if(pStationInfo->linksInfo[i].linkId == linkId) {
                return true;
            }
        }
    } else if((pStationInfo->nrLinks == 0) && (linkId < 0)) {
        //match legacy stations seen on legacy AP/EP (non MLD)
        return true;
    }
    return false;
}

/*
 * @brief return the station main staLinkId when relevant (MLO station):
 * - first use, when possible, the main linkId provided by nl80211
 * - otherwise, fetch the preferred linkId based on matching linkIds with local APMLD links freqBand
 * (preference order 6/5/2.4): this is practically used on all the mlo stations
 *
 * @param pSSID pointer to mld link ssid ctx
 * @param pStationInfo pointer to nl80211 station info ctx (already including parsed nl80211 MLO_LINKS)
 *
 * @return the main station link id, or -1 when irrelevant
 */
int16_t wld_ssid_nl80211_getPrefStaLinkId(T_SSID* pSSID, wld_nl80211_stationInfo_t* pStationInfo) {
    ASSERT_NOT_NULL(pStationInfo, -1, ME, "NULL");
    if(pStationInfo->nrLinks < 1) {
        return -1;
    }
    if(pStationInfo->nrLinks == 1) {
        return pStationInfo->linksInfo[0].linkId;
    }
    if(pStationInfo->linkId >= 0) {
        return pStationInfo->linkId;
    }
    int16_t refLinkId = wld_ssid_nl80211_getMldLinkId(pSSID);
    int16_t prefStaLinkId = -1;
    swl_freqBand_e prefStaLinkBand = SWL_FREQ_BAND_2_4GHZ;
    for(uint32_t i = 0; i < pStationInfo->nrLinks; i++) {
        int16_t staLinkId = pStationInfo->linksInfo[i].linkId;
        if(staLinkId < 0) {
            continue;
        }
        T_SSID* pNgSSID = NULL;
        if(staLinkId == refLinkId) {
            pNgSSID = pSSID;
        } else if(pSSID != NULL) {
            pNgSSID = wld_mld_getLinkSsidByLinkId(pSSID->pMldLink, staLinkId);
        }
        if(pNgSSID == NULL) {
            continue;
        }
        swl_freqBand_e staLinkBand = wld_rad_getFreqBand(pNgSSID->RADIO_PARENT);
        /*
         * if sta main MLD linkId is not filled by the driver,
         * then use preferred matching linkId based on freqBand (6/5/2.4)
         */
        if(staLinkBand >= prefStaLinkBand) {
            prefStaLinkBand = staLinkBand;
            prefStaLinkId = staLinkId;
        }
    }
    return prefStaLinkId;
}

