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

#define ME "radIfM"

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
#include "wld_radio.h"
#include "wld_accesspoint.h"
#include "wld_ssid.h"
#include "wld_endpoint.h"
#include "wld_wps.h"
#include "wld_channel.h"
#include "swl/swl_assert.h"
#include "wld_statsmon.h"
#include "wld_channel_types.h"
#include "wld_ap_rssiMonitor.h"
#include "wld_rad_stamon.h"
#include "wld_assocdev.h"
#include "wld_tinyRoam.h"

amxd_status_t _addVAPIntf(amxd_object_t* obj _UNUSED,
                          amxd_function_t* func _UNUSED,
                          amxc_var_t* args,
                          amxc_var_t* retval) {
    SAH_TRACEZ_IN(ME);

    amxd_status_t status = amxd_status_unknown_error;
    const char* radioname = GET_CHAR(args, "radio");
    /* Get our T_Radio pointer of the selected radio */
    T_Radio* pR = wld_getRadioDataHandler(wifi, radioname);
    ASSERT_NOT_NULL(pR, status, ME, "Radio structure for radio [%s] is missing", radioname);
    uint32_t nrCfgVaps = amxc_llist_size(&pR->llAP);
    ASSERT_FALSE(nrCfgVaps >= pR->maxNrHwBss, amxd_status_not_supported, ME, "Vap Addition Failed");
    const char* apname = GET_CHAR(args, "vap");
    ASSERT_STR(apname, status, ME, "missing accesspoint instance alias");
    const char* bridgename = GET_CHAR(args, "bridge");
    const char* custNetDevName = GET_CHAR(args, "ifname");
    amxd_object_t* apObjTmpl = amxd_object_get(wifi, "AccessPoint");
    amxd_object_t* ssidObjTmpl = amxd_object_get(wifi, "SSID");
    amxd_object_t* apObj = amxd_object_get_instance(apObjTmpl, apname, 0);

    char ssidRef[128] = {0};
    amxd_object_t* ssidObj = amxd_object_get_instance(ssidObjTmpl, apname, 0);
    amxd_trans_t trans;
    amxd_trans_init(&trans);
    if(ssidObj == NULL) {
        uint32_t newSsidIdx = swla_object_getFirstAvailableIndex(ssidObjTmpl);
        amxd_trans_select_object(&trans, ssidObjTmpl);
        amxd_trans_add_inst(&trans, newSsidIdx, apname);
        char* ssidTmplPath = amxd_object_get_path(ssidObjTmpl, AMXD_OBJECT_INDEXED);
        swl_str_catFormat(ssidRef, sizeof(ssidRef), "%s%s.%d.", ROOT_OBJ_PREFIX_STR, ssidTmplPath, newSsidIdx);
        free(ssidTmplPath);
        SAH_TRACEZ_INFO(ME, "%s: set trans to add new ssid instance (%s) at index (%d)", pR->Name, apname, newSsidIdx);
    } else {
        amxd_trans_select_object(&trans, ssidObj);
        wld_util_getRealReferencePath(ssidRef, sizeof(ssidRef), "", ssidObj);
        SAH_TRACEZ_INFO(ME, "%s: select trans existing ssid instance (%s)", pR->Name, apname);
    }
    char lowerLayers[128] = {0};
    wld_util_getRealReferencePath(lowerLayers, sizeof(lowerLayers), "", pR->pBus);
    SAH_TRACEZ_INFO(ME, "%s: set trans ssidInst(%s) lowerLayers(%s)", pR->Name, apname, lowerLayers);
    amxd_trans_set_value(cstring_t, &trans, "LowerLayers", lowerLayers);

    if(apObj == NULL) {
        amxd_trans_select_object(&trans, apObjTmpl);
        amxd_trans_add_inst(&trans, 0, apname);
        SAH_TRACEZ_INFO(ME, "%s: set trans to add new ap instance (%s)", pR->Name, apname);
    } else {
        amxd_trans_select_object(&trans, apObj);
        SAH_TRACEZ_INFO(ME, "%s: select trans existing ap instance (%s)", pR->Name, apname);
    }
    SAH_TRACEZ_INFO(ME, "%s: set trans apInst(%s) SSIDReference(%s)", pR->Name, apname, ssidRef);
    amxd_trans_set_value(cstring_t, &trans, "SSIDReference", ssidRef);
    if(!swl_str_isEmpty(bridgename)) {
        SAH_TRACEZ_INFO(ME, "%s: set trans apInst(%s) BridgeInterface(%s)", pR->Name, apname, bridgename);
        amxd_trans_set_value(cstring_t, &trans, "BridgeInterface", bridgename);
    }
    if(!swl_str_isEmpty(custNetDevName)) {
        SAH_TRACEZ_INFO(ME, "%s: set trans apInst(%s) CustomNetDevName(%s)", pR->Name, apname, custNetDevName);
        amxd_trans_set_value(cstring_t, &trans, "CustomNetDevName", custNetDevName);
    }
    SAH_TRACEZ_INFO(ME, "%s: apply trans to add accesspoint (%s)", pR->Name, apname);
    status = amxd_trans_apply(&trans, get_wld_plugin_dm());
    amxd_trans_clean(&trans);

    ASSERT_EQUALS(status, amxd_status_ok, status, ME, "%s: fail to apply trans to add ap (%s)", pR->Name, apname);
    amxc_var_set(bool, retval, true);

    SAH_TRACEZ_OUT(ME);
    return status;
}

amxd_status_t _delVAPIntf(amxd_object_t* wifi,
                          amxd_function_t* func _UNUSED,
                          amxc_var_t* args,
                          amxc_var_t* retval _UNUSED) {
    SAH_TRACEZ_IN(ME);

    amxd_status_t status = amxd_status_unknown_error;
    const char* vapname = GET_CHAR(args, "vap");
    amxd_object_t* apObj = amxd_object_findf(wifi, "AccessPoint.%s", vapname);
    ASSERT_NOT_NULL(apObj, status, ME, "AccessPoint instance (%s) not found", vapname);
    amxd_object_t* ssidObj = amxd_object_findf(wifi, "SSID.%s", vapname);
    ASSERT_NOT_NULL(ssidObj, status, ME, "Endpoint instance (%s) not found", vapname);

    amxd_trans_t trans;
    amxd_trans_init(&trans);
    amxd_trans_select_object(&trans, amxd_object_get_parent(apObj));
    amxd_trans_del_inst(&trans, amxd_object_get_index(apObj), NULL);
    amxd_trans_select_object(&trans, amxd_object_get_parent(ssidObj));
    amxd_trans_del_inst(&trans, amxd_object_get_index(ssidObj), NULL);
    status = amxd_trans_apply(&trans, get_wld_plugin_dm());
    amxd_trans_clean(&trans);

    ASSERT_EQUALS(status, amxd_status_ok, status, ME, "fail to apply trans to del ap (%s)", vapname);
    amxc_var_set(bool, retval, true);

    SAH_TRACEZ_OUT(ME);
    return status;
}

/**
 * @brief _addEndPointIntf
 *
 * Creates an EndPoint interface on the RADIO and updates the EndPoint and SSID object fields
 *
 * @param args argument list
 *     - radio : Name of the RADIO interface that derrives the endpoint interface. (wifi0, wifi1,...)
 *     - endpoint : Name of the EndPoint interface that will be created. (wln0, wln1, ...)
 * @param retval variant that must contain the return value
 * @return
 *     - function execution state
 */
amxd_status_t _addEndPointIntf(amxd_object_t* wifi,
                               amxd_function_t* func _UNUSED,
                               amxc_var_t* args,
                               amxc_var_t* retval) {
    SAH_TRACEZ_IN(ME);

    amxd_status_t status = amxd_status_unknown_error;
    const char* radioname = GET_CHAR(args, "radio");
    /* Get our T_Radio pointer of the selected radio */
    T_Radio* pR = wld_getRadioDataHandler(wifi, radioname);
    ASSERT_NOT_NULL(pR, status, ME, "Radio structure for radio [%s] is missing", radioname);
    const char* endpointname = GET_CHAR(args, "endpoint");
    ASSERT_STR(endpointname, status, ME, "missing endpoint instance alias");
    const char* custNetDevName = GET_CHAR(args, "ifname");
    amxd_object_t* epObjTmpl = amxd_object_get(wifi, "EndPoint");
    amxd_object_t* ssidObjTmpl = amxd_object_get(wifi, "SSID");
    amxd_object_t* endpointObj = amxd_object_get_instance(epObjTmpl, endpointname, 0);

    char ssidRef[128] = {0};
    amxd_object_t* ssidObj = amxd_object_get_instance(ssidObjTmpl, endpointname, 0);
    amxd_trans_t trans;
    amxd_trans_init(&trans);
    if(ssidObj == NULL) {
        uint32_t newSsidIdx = swla_object_getFirstAvailableIndex(ssidObjTmpl);
        amxd_trans_select_object(&trans, ssidObjTmpl);
        amxd_trans_add_inst(&trans, newSsidIdx, endpointname);
        char* ssidTmplPath = amxd_object_get_path(ssidObjTmpl, AMXD_OBJECT_INDEXED);
        swl_str_catFormat(ssidRef, sizeof(ssidRef), "%s%s.%d.", ROOT_OBJ_PREFIX_STR, ssidTmplPath, newSsidIdx);
        free(ssidTmplPath);
        SAH_TRACEZ_INFO(ME, "%s: set trans to add new ssid instance (%s) at index (%d)", pR->Name, endpointname, newSsidIdx);
    } else {
        amxd_trans_select_object(&trans, ssidObj);
        wld_util_getRealReferencePath(ssidRef, sizeof(ssidRef), "", ssidObj);
        SAH_TRACEZ_INFO(ME, "%s: select trans existing ssid instance (%s)", pR->Name, endpointname);
    }
    char lowerLayers[128] = {0};
    wld_util_getRealReferencePath(lowerLayers, sizeof(lowerLayers), "", pR->pBus);
    SAH_TRACEZ_INFO(ME, "%s: set trans ssidInst(%s) lowerLayers(%s)", pR->Name, endpointname, lowerLayers);
    amxd_trans_set_value(cstring_t, &trans, "LowerLayers", lowerLayers);

    if(endpointObj == NULL) {
        amxd_trans_select_object(&trans, epObjTmpl);
        amxd_trans_add_inst(&trans, 0, endpointname);
        SAH_TRACEZ_INFO(ME, "%s: set trans to add new ep instance (%s)", pR->Name, endpointname);
    } else {
        amxd_trans_select_object(&trans, endpointObj);
        SAH_TRACEZ_INFO(ME, "%s: select trans existing ep instance (%s)", pR->Name, endpointname);
    }
    SAH_TRACEZ_INFO(ME, "%s: set trans epInst(%s) SSIDReference(%s)", pR->Name, endpointname, ssidRef);
    amxd_trans_set_value(cstring_t, &trans, "SSIDReference", ssidRef);
    if(!swl_str_isEmpty(custNetDevName)) {
        SAH_TRACEZ_INFO(ME, "%s: set trans epInst(%s) CustomNetDevName(%s)", pR->Name, endpointname, custNetDevName);
        amxd_trans_set_value(cstring_t, &trans, "CustomNetDevName", custNetDevName);
    }
    SAH_TRACEZ_INFO(ME, "%s: apply trans to add endpoint (%s)", pR->Name, endpointname);
    status = amxd_trans_apply(&trans, get_wld_plugin_dm());
    amxd_trans_clean(&trans);

    ASSERT_EQUALS(status, amxd_status_ok, status, ME, "%s: fail to apply trans to add ep (%s)", pR->Name, endpointname);
    amxc_var_set(bool, retval, true);

    SAH_TRACEZ_OUT(ME);
    return status;
}

/**
 * @brief _delEndPointIntf
 *
 * Deletes an EndPoint interface on the RADIO and its associated structures
 *
 * @param fcall function call context
 * @param args argument list
 *     - endpoint : Name of the EndPoint interface that will be created (wln0, wln1, ...)
 * @param retval : variant that must contain the return value
 * @return
 *     - function execution state
 */
amxd_status_t _delEndPointIntf(amxd_object_t* wifi,
                               amxd_function_t* func _UNUSED,
                               amxc_var_t* args,
                               amxc_var_t* retval) {
    SAH_TRACEZ_IN(ME);

    amxd_status_t status = amxd_status_unknown_error;
    const char* endpointname = GET_CHAR(args, "endpoint");
    amxd_object_t* epObj = amxd_object_findf(wifi, "EndPoint.%s", endpointname);
    ASSERT_NOT_NULL(epObj, status, ME, "Endpoint instance (%s) not found", endpointname);
    amxd_object_t* ssidObj = amxd_object_findf(wifi, "SSID.%s", endpointname);
    ASSERT_NOT_NULL(ssidObj, status, ME, "Endpoint instance (%s) not found", endpointname);

    amxd_trans_t trans;
    amxd_trans_init(&trans);
    amxd_trans_select_object(&trans, amxd_object_get_parent(epObj));
    amxd_trans_del_inst(&trans, amxd_object_get_index(epObj), NULL);
    amxd_trans_select_object(&trans, amxd_object_get_parent(ssidObj));
    amxd_trans_del_inst(&trans, amxd_object_get_index(ssidObj), NULL);
    status = amxd_trans_apply(&trans, get_wld_plugin_dm());
    amxd_trans_clean(&trans);

    ASSERT_EQUALS(status, amxd_status_ok, status, ME, "fail to apply trans to del ep (%s)", endpointname);
    amxc_var_set(bool, retval, true);

    SAH_TRACEZ_OUT(ME);
    return status;
}

