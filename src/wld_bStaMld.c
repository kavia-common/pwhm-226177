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
#include "wld_bStaMld.h"
#include "wld_mld.h"
#include "wld_ssid.h"
#include "wld_endpoint.h"
#include "wld_radio.h"
#include "wld_eventing.h"
#include <debug/sahtrace.h>

#define ME "bStaMld"

static amxd_object_t* s_mldIdExists(int32_t mldId) {
    amxd_object_t* bSTAMLDTemplate = amxd_object_get(get_wld_object(), "bSTAMLD");
    ASSERTS_NOT_NULL(bSTAMLDTemplate, NULL, ME, "No template");
    amxd_object_for_each(instance, it, bSTAMLDTemplate) {
        amxd_object_t* object = amxc_llist_it_get_data(it, amxd_object_t, it);
        int32_t tmpMldId = amxd_object_get_int32_t(object, "MLDID", NULL);
        if(tmpMldId == mldId) {
            return object;
        }
    }
    return NULL;
}

static swl_rc_ne s_createDmInstance(wld_mld_t* pMld) {
    ASSERTS_NOT_NULL(pMld, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTI_NULL(pMld->object, SWL_RC_ERROR, ME, "Instance exists");
    amxd_object_t* bSTAMLDTemplate = amxd_object_get(get_wld_object(), "bSTAMLD");
    ASSERTS_NOT_NULL(bSTAMLDTemplate, SWL_RC_ERROR, ME, "No template");
    amxd_object_t* mldObject = s_mldIdExists(pMld->unit);

    //Reuse old instance
    if(mldObject != NULL) {
        pMld->object = mldObject;
        pMld->object->priv = pMld;
        return SWL_RC_OK;
    }

    uint32_t newIndex = swla_object_getFirstAvailableIndex(bSTAMLDTemplate);

    amxd_trans_t trans;
    ASSERT_TRANSACTION_INIT(bSTAMLDTemplate, &trans, SWL_RC_ERROR, ME, "trans init failure");
    amxd_trans_add_inst(&trans, 0, NULL);
    amxd_trans_set_value(int32_t, &trans, "MLDID", pMld->unit);
    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, SWL_RC_ERROR, ME, "Failed to create instance");

    pMld->object = amxd_object_get_instance(bSTAMLDTemplate, NULL, newIndex);
    ASSERT_NOT_NULL(pMld->object, SWL_RC_ERROR, ME, "Failed to get instance");
    pMld->object->priv = pMld;

    SAH_TRACEZ_INFO(ME, "Created bSTAMLD for mldUnit %d", pMld->unit);

    return SWL_RC_OK;
}

static swl_rc_ne s_updateDmParams(wld_mld_t* pMld) {
    ASSERTS_NOT_NULL(pMld, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pMld->object, SWL_RC_ERROR, ME, "No instance");

    swl_macChar_t mldMacAddress;
    memset(&mldMacAddress, 0, sizeof(mldMacAddress));
    swl_macChar_t bssid;
    memset(&bssid, 0, sizeof(bssid));
    char affiliatedbSTAList[1024] = {'\0'};

    if(pMld->pPrimLink) {
        T_SSID* pPrimSSID = wld_mld_getLinkSsid(pMld->pPrimLink);
        if(pPrimSSID && !swl_mac_binIsNull((swl_macBin_t*) pPrimSSID->MACAddress) && wld_ssid_hasValidMLDLinkID(pPrimSSID)) {
            SWL_MAC_BIN_TO_CHAR(&mldMacAddress, (swl_macBin_t*) pPrimSSID->MACAddress);
            SWL_MAC_BIN_TO_CHAR(&bssid, (swl_macBin_t*) pPrimSSID->BSSID);
        }
    }

    wld_mldLink_t* pLink = NULL;
    amxc_llist_for_each(it, &pMld->links) {
        pLink = amxc_container_of(it, wld_mldLink_t, it);
        T_SSID* pLinkSSID = wld_mld_getLinkSsid(pLink);
        if((pLinkSSID == NULL)
           || !wld_ssid_hasValidMLDLinkID(pLinkSSID)
           || swl_mac_binIsNull((swl_macBin_t*) pLinkSSID->MACAddress)) {
            continue;
        }

        swl_macChar_t affiliatedSta;
        memset(&affiliatedSta, 0, sizeof(affiliatedSta));
        SWL_MAC_BIN_TO_CHAR(&affiliatedSta, (swl_macBin_t*) pLinkSSID->MACAddress);

        swl_strlst_catFormat(affiliatedbSTAList, sizeof(affiliatedbSTAList), ",", "%s", affiliatedSta.cMac);
    }

    amxd_trans_t trans;
    ASSERT_TRANSACTION_INIT(pMld->object, &trans, SWL_RC_ERROR, ME, "trans init failure");
    amxd_trans_set_value(cstring_t, &trans, "MLDMACAddress", mldMacAddress.cMac);
    amxd_trans_set_value(cstring_t, &trans, "BSSID", bssid.cMac);
    amxd_trans_set_value(cstring_t, &trans, "AffiliatedbSTAList", affiliatedbSTAList);
    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, SWL_RC_ERROR, ME, "Failed to update params");

    SAH_TRACEZ_INFO(ME, "Updated unit %d: MLDMACAddress = %s, BSSID = %s, AffiliatedbSTAList = %s",
                    pMld->unit, mldMacAddress.cMac, bssid.cMac, affiliatedbSTAList);

    return SWL_RC_OK;
}

static void s_clearDmParams(wld_mld_t* pMld) {
    ASSERTS_NOT_NULL(pMld, , ME, "NULL");
    ASSERTS_NOT_NULL(pMld->object, , ME, "No instance");

    amxd_trans_t trans;
    ASSERT_TRANSACTION_INIT(pMld->object, &trans, , ME, "trans init failure");
    amxd_trans_set_value(cstring_t, &trans, "MLDMACAddress", "");
    amxd_trans_set_value(cstring_t, &trans, "BSSID", "");
    amxd_trans_set_value(cstring_t, &trans, "AffiliatedbSTAList", "");
    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, , ME, "Failed to update params");

    pMld->object->priv = NULL;
    pMld->object = NULL;
}

static void s_mldChange(wld_mldChange_t* event) {
    ASSERT_NOT_NULL(event, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "detect mld event %d mldtype %d unit %d", event->event, event->mldType, event->mldUnit);
    ASSERTI_EQUALS(event->mldType, WLD_SSID_TYPE_EP, , ME, "wrong mld type");
    T_SSID* pSSID = event->pEvtLinkSsid;
    ASSERT_NOT_NULL(pSSID, , ME, "No link ssid");
    T_Radio* pRad = pSSID->RADIO_PARENT;
    ASSERT_NOT_NULL(pRad, , ME, "No link radio");
    wld_mld_t* pMld = wld_mld_getMldByUnit(WLD_SSID_TYPE_EP, event->mldUnit);

    if(event->event == WLD_MLD_EVT_ADD) {
        s_createDmInstance(pMld);
    } else if(event->event == WLD_MLD_EVT_DEL) {
        s_clearDmParams(pMld);
    }
}

/**
 * Update all bSTAMLD instance
 */
void wld_bStaMld_update() {
    amxd_object_t* bSTAMLDTemplate = amxd_object_get(get_wld_object(), "bSTAMLD");
    ASSERTS_NOT_NULL(bSTAMLDTemplate, , ME, "No template");
    amxd_object_for_each(instance, it, bSTAMLDTemplate) {
        amxd_object_t* object = amxc_llist_it_get_data(it, amxd_object_t, it);
        wld_mld_t* pMld = (wld_mld_t*) object->priv;
        s_updateDmParams(pMld);
    }
}

static wld_event_callback_t s_onMldChange = {
    .callback = (wld_event_callback_fun) s_mldChange,
};

void wld_bStaMld_init() {
    wld_event_add_callback(gWld_queue_mld_onChangeEvent, &s_onMldChange);
}

static void s_setEMLMREnabled_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    amxd_object_t* mldObj = amxd_object_get_parent(object);
    ASSERT_NOT_NULL(mldObj, , ME, "NULL");
    wld_mld_t* pMld = (wld_mld_t*) mldObj->priv;
    ASSERT_NOT_NULL(pMld, , ME, "NULL");
    swl_trl_e newEnable = swl_trl_fromInt(amxc_var_get_const_int8_t(newValue));
    pMld->Cfg.emlmrEnable = newEnable;
}

static void s_setEMLSREnabled_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    amxd_object_t* mldObj = amxd_object_get_parent(object);
    ASSERT_NOT_NULL(mldObj, , ME, "NULL");
    wld_mld_t* pMld = (wld_mld_t*) mldObj->priv;
    ASSERT_NOT_NULL(pMld, , ME, "NULL");
    swl_trl_e newEnable = swl_trl_fromInt(amxc_var_get_const_int8_t(newValue));
    pMld->Cfg.emlsrEnable = newEnable;
}

static void s_setSTREnabled_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    amxd_object_t* mldObj = amxd_object_get_parent(object);
    ASSERT_NOT_NULL(mldObj, , ME, "NULL");
    wld_mld_t* pMld = (wld_mld_t*) mldObj->priv;
    ASSERT_NOT_NULL(pMld, , ME, "NULL");
    swl_trl_e newEnable = swl_trl_fromInt(amxc_var_get_const_int8_t(newValue));
    pMld->Cfg.strEnable = newEnable;
}

static void s_setNSTREnabled_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    amxd_object_t* mldObj = amxd_object_get_parent(object);
    ASSERT_NOT_NULL(mldObj, , ME, "NULL");
    wld_mld_t* pMld = (wld_mld_t*) mldObj->priv;
    ASSERT_NOT_NULL(pMld, , ME, "NULL");
    swl_trl_e newEnable = swl_trl_fromInt(amxc_var_get_const_int8_t(newValue));
    pMld->Cfg.nstrEnable = newEnable;
}

SWLA_DM_HDLRS(sbSTAMLDCfgHdlrs,
              ARR(
                  SWLA_DM_PARAM_HDLR("EMLMREnabled", s_setEMLMREnabled_pwf),
                  SWLA_DM_PARAM_HDLR("EMLSREnabled", s_setEMLSREnabled_pwf),
                  SWLA_DM_PARAM_HDLR("STREnabled", s_setSTREnabled_pwf),
                  SWLA_DM_PARAM_HDLR("NSTREnabled", s_setNSTREnabled_pwf),
                  ));

void _wld_bSTAMLD_setConf_ocf(const char* const sig_name,
                              const amxc_var_t* const data,
                              void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sbSTAMLDCfgHdlrs, sig_name, data, priv);
}
