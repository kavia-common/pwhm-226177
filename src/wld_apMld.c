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

#include "wld.h"
#include "wld_util.h"
#include "wld_accesspoint.h"
#include "wld_ssid.h"
#include "wld_radio.h"
#include "wld_assocdev.h"
#include "wld_apMld.h"

#define ME "mld"

/**
 * Retrieve an affiliatedStaInfo for a given MAC Address associated to a given MLD unit.
 *
 * Because inactive affiliated sta is remembered, and affiliatedSta mac addresses may be reused on other band
 * it is possible there are multiple afSta with the same mac address, although there should never be more than one
 * of them active at the same time.
 *
 * @param info: the struct in which the affiliated sta info will be written
 * @param mldUnit: the mld unit ID of the ap MLD.
 * @param mac: the mac address for which to do a lookup for a matching affiliated sta.
 *
 * @return
 *  - true if an affiliated sta with the given mac address has been found. The info struct
 * shall be properly filled in with the affiliated sta, the associated device to which the affiliated sta belongs,
 * and the access point to which the associated device is associated.
 * - false if no matching affiliated sta can be found, or some error took place. In this case, the info
 * parameter will not be touched.
 */
bool wld_apMld_fetchAffiliatedStaInfo(wld_apMld_afStaInfo_t* info, int32_t mldUnit, swl_macBin_t* mac) {
    ASSERT_NOT_NULL(info, false, ME, "NULL");
    ASSERT_NOT_NULL(mac, false, ME, "NULL");

    T_Radio* pRad = NULL;
    wld_for_eachRad(pRad) {
        T_AccessPoint* tmpAp = NULL;
        wld_rad_forEachAp(tmpAp, pRad) {
            if(tmpAp == NULL) {
                continue;
            }

            if(tmpAp->pSSID == NULL) {
                continue;
            }

            if(tmpAp->pSSID->mldUnit != mldUnit) {
                continue;
            }

            for(int i = 0; i < tmpAp->AssociatedDeviceNumberOfEntries; i++) {
                T_AssociatedDevice* pAD = tmpAp->AssociatedDevice[i];
                amxc_llist_for_each(it, &pAD->affiliatedStaList) {
                    wld_affiliatedSta_t* afSta = amxc_llist_it_get_data(it, wld_affiliatedSta_t, it);
                    if(swl_mac_binMatches(&afSta->mac, mac)) {
                        info->afSta = afSta;
                        info->pAD = pAD;
                        info->mainAp = tmpAp;
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

/**
 * Fetch the active AffiliatedStaInfo for a given MAC afSta connected to a given AP.
 */
bool wld_apMld_getActiveApAffiliatedStaInfo(wld_apMld_afStaInfo_t* info, T_AccessPoint* pAP, swl_macBin_t* mac) {
    ASSERT_NOT_NULL(info, false, ME, "NULL");
    ASSERT_NOT_NULL(mac, false, ME, "NULL");

    T_Radio* pRad = NULL;
    wld_for_eachRad(pRad) {
        T_AccessPoint* tmpAp = NULL;
        wld_rad_forEachAp(tmpAp, pRad) {
            if(tmpAp == NULL) {
                continue;
            }

            if(tmpAp->pSSID == NULL) {
                continue;
            }

            if((tmpAp != pAP) &&
               ((tmpAp->pSSID->mldUnit < 0) ||
                (tmpAp->pSSID->mldUnit != pAP->pSSID->mldUnit))) {
                continue;
            }

            for(int i = 0; i < tmpAp->AssociatedDeviceNumberOfEntries; i++) {
                T_AssociatedDevice* pAD = tmpAp->AssociatedDevice[i];
                if(!pAD->Active) {
                    continue;
                }

                amxc_llist_for_each(it, &pAD->affiliatedStaList) {
                    wld_affiliatedSta_t* afSta = amxc_llist_it_get_data(it, wld_affiliatedSta_t, it);
                    if(!afSta->active) {
                        continue;
                    }

                    if(swl_mac_binMatches(&afSta->mac, mac) && (afSta->pAP == pAP)) {
                        info->afSta = afSta;
                        info->pAD = pAD;
                        info->mainAp = tmpAp;
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

/*
 * @brief check whether one APMLD link have applicable and shared
 * ssid and security configurations (secMode, keypass) values
 * with the other links
 */
bool wld_apMld_hasSharedConnectionConf(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, false, ME, "NULL");
    T_SSID* pSSID = pAP->pSSID;
    ASSERTS_NOT_NULL(pSSID, false, ME, "NULL");
    wld_mldLink_t* pLink = pSSID->pMldLink;
    if(!wld_mld_checkUsableLinkBasicConditions(pLink)) {
        return false;
    }

    uint32_t countUsable = 0;
    uint32_t countSharedConf = 0;
    wld_mldLink_t* pNgLink = NULL;
    wld_for_eachNeighMldLink(pNgLink, pLink) {
        if(!wld_mld_checkUsableLinkBasicConditions(pNgLink)) {
            continue;
        }
        countUsable++;
        T_SSID* pNgLinkSSID = wld_mld_getLinkSsid(pNgLink);
        if(swl_str_matches(pNgLinkSSID->SSID, pSSID->SSID) &&
           wld_ap_sec_checkSharedSecConfigs(pNgLinkSSID->AP_HOOK, pAP)) {
            countSharedConf++;
        }
    }
    SAH_TRACEZ_INFO(ME, "%s: countUsableLinks:%d countSharedConfLinks:%d",
                    pAP->alias, countUsable, countSharedConf);

    /*
     * APMLD links must have same SSID and valid shared security config (mode,psk,saePassPhrase)
     * in order to be usable.
     * otherwise, the link configs are misaligned, so the MLD is split into individual links.
     */
    if((countUsable > 0) && (countSharedConf > 0) &&
       (countUsable == countSharedConf)) {
        return true;
    }
    return false;
}

