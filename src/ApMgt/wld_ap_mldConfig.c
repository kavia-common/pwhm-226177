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

#define ME "mldCfg"

#include <stdlib.h>
#include <debug/sahtrace.h>


#include <string.h>
#include <assert.h>
#include <ctype.h>

#include <swla/swla_commonLib.h>
#include <swla/swla_conversion.h>
#include <swl/swl_string.h>

#include "wld.h"
#include "wld_util.h"
#include "wld_accesspoint.h"

#include "amxc/amxc_llist.h"
#include "wld_mld.h"
#include "wld_apMld.h"

#include <swl/swl_base64.h>
#include "swla/swla_dm.h"


/**
 * @brief Apply current MLD configuration to all affiliated AP links.
 *
 * Iterates over all links of the MLD and updates their Access Point
 * configuration (emlmr, emlsr, str, nstr flags). Each change is logged,
 * and the updated configuration is pushed to the firmware adapter.
 *
 * @param pMld   Pointer to the MLD instance containing configuration.
 */
void set_ap_mldConfig(wld_mld_t* pMld) {
    SAH_TRACEZ_IN(ME);
    ASSERT_NOT_NULL(pMld, , ME, "pMld is NULL");

    amxc_llist_it_t* it = NULL;
    wld_mldLink_t* pLink = NULL;

    amxc_llist_for_each(it, &pMld->links) {
        pLink = amxc_llist_it_get_data(it, wld_mldLink_t, it);
        if((pLink == NULL) || (pLink->pSSID == NULL)) {
            continue;
        }

        T_SSID* pSSID = pLink->pSSID;
        T_AccessPoint* pAP = pSSID->AP_HOOK;

        if(pMld->Cfg.emlmrEnable != pAP->mldCfg.emlmrEnable) {
            pAP->mldCfg.emlmrEnable = pMld->Cfg.emlmrEnable;
            SAH_TRACEZ_INFO(ME, "Updated : %s emlmr_enable: new value %d", pSSID->Name, pAP->mldCfg.emlmrEnable);
        } else {
            SAH_TRACEZ_INFO(ME, "%s emlmr_enable value : %d same as parent mld", pSSID->Name, pAP->mldCfg.emlmrEnable);
        }
        if(pMld->Cfg.emlsrEnable != pAP->mldCfg.emlsrEnable) {
            pAP->mldCfg.emlsrEnable = pMld->Cfg.emlsrEnable;
            SAH_TRACEZ_INFO(ME, "Updated : %s emlsr_enable: new value %d", pSSID->Name, pAP->mldCfg.emlsrEnable);
        } else {
            SAH_TRACEZ_INFO(ME, "%s emlsr_enable value : %d same as parent mld", pSSID->Name, pAP->mldCfg.emlsrEnable);
        }
        if(pMld->Cfg.strEnable != pAP->mldCfg.strEnable) {
            pAP->mldCfg.strEnable = pMld->Cfg.strEnable;
            SAH_TRACEZ_INFO(ME, "Updated : %s str_enable: new value %d", pSSID->Name, pAP->mldCfg.strEnable);
        } else {
            SAH_TRACEZ_INFO(ME, "%s str_enable value : %d same as parent mld", pSSID->Name, pAP->mldCfg.strEnable);
        }
        if(pMld->Cfg.nstrEnable != pAP->mldCfg.nstrEnable) {
            pAP->mldCfg.nstrEnable = pMld->Cfg.nstrEnable;
            SAH_TRACEZ_INFO(ME, "Updated : %s nstr_enable: new value %d", pSSID->Name, pAP->mldCfg.nstrEnable);
        } else {
            SAH_TRACEZ_INFO(ME, "%s nstr_enable value : %d same as parent mld", pSSID->Name, pAP->mldCfg.nstrEnable);
        }

        pAP->pFA->mfn_wvap_setMldCfg(pAP);

        SAH_TRACEZ_INFO(ME, "LinkID: %d, SSID Name: %s Updated Config", pLink->linkId, pSSID->Name);
    }

    SAH_TRACEZ_OUT(ME);
}

/**
 * @brief Handle configuration parameter changes on the MLD object.
 *
 * Triggered when MLD config parameters are modified in the DM. Updates
 * the corresponding fields in the MLD structure, then applies the new
 * settings to all affiliated AP links.
 *
 * @param priv             Private pointer (unused).
 * @param object           MLD config DM object.
 * @param newParamValues   Updated parameter values.
 */
static void s_setApMldConfig_ocf(void* priv _UNUSED, amxd_object_t* object, const amxc_var_t* const newParamValues _UNUSED) {
    wld_mld_t* pMld = wld_apMld_getMldfromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pMld, , ME, "NULL");
    SAH_TRACEZ_IN(ME);
    amxc_var_for_each(newValue, newParamValues) {
        char* valStr = NULL;
        const char* pname = amxc_var_key(newValue);
        int8_t newEnable;
        if(swl_str_matches(pname, "EMLMREnabled")) {
            newEnable = swl_trl_fromInt(amxc_var_get_const_int8_t(newValue));
            SAH_TRACEZ_INFO(ME, "%d: EMLMREnable oldValue:%d newValue:%d", pMld->unit, pMld->Cfg.emlmrEnable, newEnable);
            pMld->Cfg.emlmrEnable = newEnable;
        } else if(swl_str_matches(pname, "EMLSREnabled")) {
            newEnable = swl_trl_fromInt(amxc_var_get_const_int8_t(newValue));
            SAH_TRACEZ_INFO(ME, "%d: EMLSREnable oldValue:%d newValue:%d", pMld->unit, pMld->Cfg.emlsrEnable, newEnable);
            pMld->Cfg.emlsrEnable = newEnable;
        } else if(swl_str_matches(pname, "STREnabled")) {
            newEnable = swl_trl_fromInt(amxc_var_get_const_int8_t(newValue));
            SAH_TRACEZ_INFO(ME, "%d: STREnable oldValue:%d newValue:%d", pMld->unit, pMld->Cfg.strEnable, newEnable);
            pMld->Cfg.strEnable = newEnable;
        } else if(swl_str_matches(pname, "NSTREnabled")) {
            newEnable = swl_trl_fromInt(amxc_var_get_const_int8_t(newValue));
            SAH_TRACEZ_INFO(ME, "%d: NSTREnable oldValue:%d newValue:%d", pMld->unit, pMld->Cfg.nstrEnable, newEnable);
            pMld->Cfg.nstrEnable = newEnable;
        }
    }
    uint32_t numLinks = amxc_llist_size(&pMld->links);
    if(numLinks > 0) {
        SAH_TRACEZ_INFO(ME, "%d: Calling set ap mld config", pMld->unit);
        set_ap_mldConfig(pMld);
    }

    SAH_TRACEZ_OUT(ME);
}

SWLA_DM_HDLRS(sApMldCfgHdlrs, ARR(), .objChangedCb = s_setApMldConfig_ocf);



void _wld_ap_setMLDConfig_ocf(const char* const sig_name,
                              const amxc_var_t* const data,
                              void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sApMldCfgHdlrs, sig_name, data, priv);
}
