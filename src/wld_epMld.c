/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2025 SoftAtHome
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
#include "wld_ssid.h"
#include "wld_endpoint.h"
#include "wld_epMld.h"
#include "wld_radio.h"

#define ME "mld"

/**
 * Report if only one EndPoint profile exists and is MLO compliant.
 */
static bool s_singleValidMloProfileExists() {
    uint32_t nrProfile = 0;
    T_Radio* tmpRad = NULL;
    wld_for_eachRad(tmpRad) {
        T_EndPoint* tmpEp = NULL;
        wld_rad_forEachEp(tmpEp, tmpRad) {
            T_SSID* tmpSSID = tmpEp->pSSID;
            if(tmpSSID == NULL) {
                continue;
            }
            if(wld_mld_checkUsableLinkBasicConditions(tmpSSID->pMldLink)
               && (tmpEp->currentProfile != NULL)
               && wld_endpoint_isSecurityModeMloCompliant(tmpEp->currentProfile->secModeEnabled)) {
                nrProfile++;
            }
        }
    }
    return (nrProfile == 1);
}

/*
 * @brief check whether one bSTAMLD link have applicable and shared
 * the same Profile values with the other links
 */
static bool s_hasSharedConnectionConf(T_EndPoint* pEP) {
    ASSERTS_NOT_NULL(pEP, false, ME, "NULL");
    T_SSID* pSSID = pEP->pSSID;
    ASSERTS_NOT_NULL(pSSID, false, ME, "NULL");

    wld_mldLink_t* pLink = pSSID->pMldLink;
    if(!wld_mld_checkUsableLinkBasicConditions(pLink)) {
        return false;
    }

    uint32_t countSharedConf = 0;
    wld_mldLink_t* pNgLink = NULL;
    wld_for_eachNeighMldLink(pNgLink, pLink) {
        if(!wld_mld_checkUsableLinkBasicConditions(pNgLink)) {
            continue;
        }
        T_SSID* pNgLinkSSID = wld_mld_getLinkSsid(pNgLink);
        if(pNgLinkSSID == NULL) {
            continue;
        }
        T_EndPoint* pLinkEP = pNgLinkSSID->ENDP_HOOK;
        if((pLinkEP == NULL) || (pLinkEP == pEP)) {
            continue;
        }
        if(wld_endpoint_isProfileMloCompliant(pLinkEP->currentProfile, pEP->currentProfile)) {
            countSharedConf++;
        }
    }
    SAH_TRACEZ_INFO(ME, "%s: countSharedConfLinks:%d",
                    pEP->alias, countSharedConf);

    /*
     * EPMLD links must have same Profile in order to be usable.
     * otherwise, the link configs are misaligned, so the MLD is split into individual links.
     */
    return (countSharedConf > 0);
}

bool wld_epMld_isMloReady(T_EndPoint* pEP) {
    ASSERTS_NOT_NULL(pEP, false, ME, "NULL");
    if(s_singleValidMloProfileExists() || s_hasSharedConnectionConf(pEP)) {
        return true;
    }
    return false;
}