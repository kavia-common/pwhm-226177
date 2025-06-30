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

#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <debug/sahtrace.h>
#include <assert.h>

#include "wld.h"
#include "wld_util.h"
#include "wld_ssid.h"
#include "wld_accesspoint.h"
#include "wld_endpoint.h"
#include "wld_statsmon.h"
#include "wld_radio.h"
#include "wld_dm_trans.h"
#include "swl/swl_string.h"
#include "swl/swl_common.h"
#include "swl/swl_assert.h"
#include "Utils/wld_autoCommitMgr.h"
#include "wld/Utils/wld_autoNeighAdd.h"
#include "wld_linuxIfUtils.h"
#include "wld/Utils/wld_config.h"

#define ME "ssid"

static char* SSID_SupStatus[] = {"Error", "LowerLayerDown", "NotPresent", "Dormant", "Unknown", "Down", "Up", 0};

static amxc_llist_t sSsidList = {NULL, NULL};

static const char* wld_autoMacSrc_str[WLD_AUTOMACSRC_MAX] = {"Dummy", "Radio"};

static void s_setEnable_internal(T_SSID* pSSID, bool enable) {
    ASSERTS_NOT_EQUALS(pSSID->enable, enable, , ME, "same value");
    SAH_TRACEZ_INFO(ME, "%s: SSID Enable %u -> %u", pSSID->Name, pSSID->enable, enable);
    pSSID->enable = enable;
    if(enable) {
        pSSID->changeInfo.nrEnables++;
        pSSID->changeInfo.lastEnableTime = swl_time_getMonoSec();
    } else {
        pSSID->changeInfo.lastDisableTime = swl_time_getMonoSec();
    }
}



bool wld_ssid_getIntfEnable(T_SSID* pSSID) {
    ASSERTS_NOT_NULL(pSSID, false, ME, "NULL");
    if(pSSID->AP_HOOK != NULL) {
        T_AccessPoint* pAP = pSSID->AP_HOOK;
        return pAP->enable;
    }
    if(pSSID->ENDP_HOOK != NULL) {
        T_EndPoint* pEP = pSSID->ENDP_HOOK;
        return pEP->enable;
    }
    SAH_TRACEZ_ERROR(ME, "%s: getEnable without hook", pSSID->Name);
    return false;
}



amxd_object_t* wld_ssid_getIntfObject(T_SSID* pSSID) {
    ASSERTS_NOT_NULL(pSSID, NULL, ME, "NULL");
    if(pSSID->AP_HOOK != NULL) {
        T_AccessPoint* pAP = pSSID->AP_HOOK;
        return pAP->pBus;
    }
    if(pSSID->ENDP_HOOK != NULL) {
        T_EndPoint* pEP = pSSID->ENDP_HOOK;
        return pEP->pBus;
    }
    SAH_TRACEZ_ERROR(ME, "%s: getIntfObj without hook", pSSID->Name);
    return false;
}

static void s_syncEnable (amxp_timer_t* timer _UNUSED, void* priv) {
    SAH_TRACEZ_IN(ME);
    T_SSID* pSSID = (T_SSID*) priv;
    ASSERT_NOT_NULL(pSSID, , ME, "NULL");

    bool tgtEnable = wld_ssid_getIntfEnable(pSSID);

    SAH_TRACEZ_INFO(ME, "%s: sync ssidEnable %d, objEnab %d, syncMode %s", pSSID->Name, pSSID->enable, tgtEnable,
                    wld_config_getEnableSyncModeStr());

    if(tgtEnable == pSSID->enable) {
        SAH_TRACEZ_OUT(ME);
        return;
    }

    if(!wld_config_isEnableSyncNeeded(pSSID->syncEnableToIntf)) {
        SAH_TRACEZ_OUT(ME);
        return;
    }

    if(pSSID->syncEnableToIntf) {
        amxd_object_t* pTgtObj = wld_ssid_getIntfObject(pSSID);
        swl_typeUInt8_commitObjectParam(pTgtObj, "Enable", pSSID->enable);
    } else {
        swl_typeUInt8_commitObjectParam(pSSID->pBus, "Enable", tgtEnable);
    }

    SAH_TRACEZ_OUT(ME);
}

/*
 * @brief checks whether SSID and referencing AP/EP will be both enabled
 * @param pSSID SSID context
 * @param enable New SSID enabling value
 * @return bool true when SSID and AP/EP will be both enabled
 */
static bool s_checkEnableWithRef(T_SSID* pSSID, bool enable) {
    return (enable && wld_ssid_getIntfEnable(pSSID));
}

/*
 * @brief returns whether SSID and referencing AP/EP are currently both enabled
 * @param pSSID SSID context
 * @return bool true when SSID and AP/EP are both enabled
 */
bool wld_ssid_isEnabledWithRef(T_SSID* pSSID) {
    ASSERTS_NOT_NULL(pSSID, false, ME, "NULL");
    return (pSSID->enable && wld_ssid_getIntfEnable(pSSID));
}

/*
 * @brief returns whether whole SSID stack ((AP/EP)+SSID+Radio) is enabled
 * @param pSSID SSID context
 * @return bool true when SSID stack is enabled
 */
bool wld_ssid_hasStackEnabled(T_SSID* pSSID) {
    ASSERTS_NOT_NULL(pSSID, false, ME, "NULL");
    T_Radio* pRad = (T_Radio*) pSSID->RADIO_PARENT;
    ASSERTI_NOT_NULL(pRad, false, ME, "%s: No referenced radio", pSSID->Name);
    return (pRad->enable && wld_ssid_isEnabledWithRef(pSSID));
}

/*
 * @brief apply ssid enable to referencing AP/EP
 * @param bool combEnable Combined enabling value
 * @param bool enable SSID's own enabling value
 * @return - SWL_RC_OK on success, error code otherwise
 */
swl_rc_ne wld_ssid_applyEnable(T_SSID* pSSID, bool combEnable, bool enable) {
    ASSERTS_NOT_NULL(pSSID, SWL_RC_INVALID_PARAM, ME, "NULL");
    T_AccessPoint* pAP = pSSID->AP_HOOK;
    T_EndPoint* pEP = pSSID->ENDP_HOOK;
    swl_rc_ne rc = SWL_RC_ERROR;
    if(pAP != NULL) {
        rc = wld_ap_applyEnable(pAP, combEnable, pAP->enable);
    } else if(pEP != NULL) {
        rc = wld_endpoint_applyEnable(pEP, combEnable, pEP->enable);
    } else {
        wld_mld_setLinkConfigured(pSSID->pMldLink, false);
    }

    pSSID->enable = enable;

    return rc;
}

// Manually trigger the enable sync. This allows for easier direct testing so loops or accidental ping pong does not occur.
void wld_ssid_dbgTriggerSync(T_SSID* pSSID) {
    amxp_timer_stop(pSSID->enableSyncTimer);
    s_syncEnable(NULL, pSSID);
}

void s_generateDefaultSSID(char* buffer, uint32_t size, uint32_t id) {
    snprintf(buffer, size, "PWHM_SSID%d", id);

}

T_SSID* s_createSsid(const char* name, uint32_t id) {
    ASSERT_STR(name, NULL, ME, "Empty name");
    SAH_TRACEZ_IN(ME);
    T_SSID* pSSID = calloc(1, sizeof(T_SSID));
    ASSERT_NOT_NULL(pSSID, NULL, ME, "NULL");
    pSSID->debug = SSID_POINTER;
    swl_str_copy(pSSID->Name, sizeof(pSSID->Name), name);
    s_generateDefaultSSID(pSSID->SSID, SSID_NAME_LEN, id);
    amxc_llist_append(&sSsidList, &pSSID->it);
    amxp_timer_new(&pSSID->enableSyncTimer, s_syncEnable, pSSID);
    pSSID->enable = 0;
    swl_timeMono_t now = swl_time_getMonoSec();
    pSSID->changeInfo.lastDisableTime = now;
    pSSID->changeInfo.lastStatusChange = now;
    pSSID->bssIndex = -1;
    pSSID->mldUnit = -1;
    pSSID->mldLinkId = -1;
    pSSID->mldRole = SWL_MLO_ROLE_NONE;

    SAH_TRACEZ_INFO(ME, "created ssid(%s) ctx(%p) id(%d)", name, pSSID, id);
    SAH_TRACEZ_OUT(ME);
    return pSSID;
}

static T_SSID* s_findSsid(amxd_object_t* object) {
    amxc_llist_for_each(it, &sSsidList) {
        T_SSID* pSSID = amxc_container_of(it, T_SSID, it);
        if(pSSID->pBus == object) {
            return pSSID;
        }
    }
    return NULL;
}

T_SSID* s_createSsidFromObj(amxd_object_t* obj) {
    ASSERT_NOT_NULL(obj, NULL, ME, "NULL");
    SAH_TRACEZ_IN(ME);
    T_SSID* pSSID = s_findSsid(obj);
    ASSERTI_NULL(pSSID, pSSID, ME, "obj %p has already internal ctx %p", obj, pSSID);
    pSSID = s_createSsid(amxd_object_get_name(obj, AMXD_OBJECT_NAMED), amxd_object_get_index(obj));
    ASSERT_NOT_NULL(pSSID, NULL, ME, "NULL");
    pSSID->pBus = obj;
    obj->priv = pSSID;
    SAH_TRACEZ_OUT(ME);
    return pSSID;
}

T_SSID* wld_ssid_createApSsid(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, NULL, ME, "NULL");
    T_SSID* pSSID = s_createSsid(pAP->name, pAP->ref_index);
    ASSERT_NOT_NULL(pSSID, NULL, ME, "NULL");
    pSSID->RADIO_PARENT = pAP->pRadio;
    pSSID->AP_HOOK = pAP;
    pAP->pSSID = pSSID;
    return pSSID;
}

T_SSID* wld_ssid_fromObj(amxd_object_t* ssidObj) {
    ASSERTS_EQUALS(amxd_object_get_type(ssidObj), amxd_object_instance, NULL, ME, "Not instance");
    amxd_object_t* parentObj = amxd_object_get_parent(ssidObj);
    ASSERT_EQUALS(get_wld_object(), amxd_object_get_parent(parentObj), NULL, ME, "wrong location");
    const char* parentName = amxd_object_get_name(parentObj, AMXD_OBJECT_NAMED);
    ASSERT_TRUE(swl_str_matches(parentName, "SSID"), NULL, ME, "invalid parent obj(%s)", parentName);
    T_SSID* pSSID = (T_SSID*) ssidObj->priv;
    ASSERTS_TRUE(pSSID, NULL, ME, "NULL");
    ASSERT_TRUE(debugIsSsidPointer(pSSID), NULL, ME, "INVALID");
    return pSSID;
}

static void s_clearApSSIDRef(void* param) {
    ASSERTS_NOT_NULL(param, , ME, "NULL");
    uint32_t vapInstIdx = (uint32_t) ((intptr_t) param);
    ASSERTS_TRUE(vapInstIdx > 0, , ME, "invalid");
    SAH_TRACEZ_IN(ME);
    amxd_object_t* pApTmplObj = amxd_object_get(get_wld_object(), "AccessPoint");
    amxd_object_t* pApObj = amxd_object_get_instance(pApTmplObj, NULL, vapInstIdx);
    ASSERTW_NOT_NULL(pApObj, , ME, "vap instance idx(%d) not found", vapInstIdx);
    swl_typeCharPtr_commitObjectParam(pApObj, "SSIDReference", "");
    SAH_TRACEZ_OUT(ME);
}

static void s_clearEpSSIDRef(void* param) {
    ASSERTS_NOT_NULL(param, , ME, "NULL");
    uint32_t epInstIdx = (uint32_t) ((intptr_t) param);
    ASSERTS_TRUE(epInstIdx > 0, , ME, "invalid");
    SAH_TRACEZ_IN(ME);
    amxd_object_t* pEpTmplObj = amxd_object_get(get_wld_object(), "EndPoint");
    amxd_object_t* pEpObj = amxd_object_get_instance(pEpTmplObj, NULL, epInstIdx);
    ASSERTW_NOT_NULL(pEpObj, , ME, "ep instance idx(%d) not found", epInstIdx);
    swl_typeCharPtr_commitObjectParam(pEpObj, "SSIDReference", "");
    SAH_TRACEZ_OUT(ME);
}

static void s_cleanSSID(T_SSID* pSSID, bool direct) {
    ASSERTS_NOT_NULL(pSSID, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: destroy SSID", pSSID->Name);
    T_AccessPoint* pAP = (T_AccessPoint*) pSSID->AP_HOOK;
    T_EndPoint* pEP = (T_EndPoint*) pSSID->ENDP_HOOK;
    if((pAP != NULL) && debugIsVapPointer(pAP) && (pAP->pSSID == pSSID)) {
        uint32_t vapInstIdx = amxd_object_get_index(pAP->pBus);
        //clear SSID Reference of AccessPoint
        pAP->pSSID = NULL;
        if(!direct && vapInstIdx) {
            void* param = (void*) ((intptr_t) vapInstIdx);
            swla_delayExec_add(s_clearApSSIDRef, param);
        }
    } else if((pEP != NULL) && debugIsEpPointer(pEP) && (pEP->pSSID == pSSID)) {
        uint32_t epInstIdx = amxd_object_get_index(pEP->pBus);
        //clear SSID Reference of EndPoint
        pEP->pSSID = NULL;
        if(!direct && epInstIdx) {
            void* param = (void*) ((intptr_t) epInstIdx);
            swla_delayExec_add(s_clearEpSSIDRef, param);
        }
    }
    wld_mld_unregisterLink(pSSID);
    amxc_llist_it_take(&pSSID->it);
    if(pSSID->pBus != NULL) {
        pSSID->pBus->priv = NULL;
    }
    free(pSSID);
}

/* Be sure that our attached memory structure is cleared */
static void s_destroySsid(amxd_object_t* object) {
    T_SSID* pSSID = wld_ssid_fromObj(object);
    s_cleanSSID(pSSID, false);
}

void wld_ssid_cleanAll() {
    amxc_llist_it_t* it = amxc_llist_get_first(&sSsidList);
    while(it != NULL) {
        T_SSID* pSSID = amxc_llist_it_get_data(it, T_SSID, it);
        s_cleanSSID(pSSID, true);
        it = amxc_llist_get_first(&sSsidList);
    }
}

amxd_status_t _wld_ssid_addInstance_oaf(amxd_object_t* object,
                                        amxd_param_t* param,
                                        amxd_action_t reason,
                                        const amxc_var_t* const args,
                                        amxc_var_t* const retval,
                                        void* priv) {
    SAH_TRACEZ_IN(ME);

    char* path = amxd_object_get_path(object, AMXD_OBJECT_NAMED);
    const char* name = amxd_object_get_name(object, AMXD_OBJECT_NAMED);
    SAH_TRACEZ_INFO(ME, "add instance object(%p:%s:%s)",
                    object, name, path);
    free(path);
    amxd_status_t status = amxd_action_object_add_inst(object, param, reason, args, retval, priv);
    ASSERTI_NOT_EQUALS(status, amxd_status_duplicate, status, ME, "override instance (%p:%s)", object, name);
    ASSERT_EQUALS(status, amxd_status_ok, status, ME, "Fail to create instance %s (status %d)", name, status);
    amxd_object_t* instance = amxd_object_get_instance(object, NULL, GET_UINT32(retval, "index"));
    ASSERT_NOT_NULL(instance, amxd_status_unknown_error, ME, "Fail to get instance");
    s_createSsidFromObj(instance);

    SAH_TRACEZ_OUT(ME);
    return status;
}

amxd_status_t _wld_ssid_delInstance_odf(amxd_object_t* object,
                                        amxd_param_t* param,
                                        amxd_action_t reason,
                                        const amxc_var_t* const args,
                                        amxc_var_t* const retval,
                                        void* priv) {
    SAH_TRACEZ_IN(ME);

    amxd_status_t status = amxd_action_object_destroy(object, param, reason, args, retval, priv);
    ASSERT_EQUALS(status, amxd_status_ok, status, ME, "Fail to destroy obj instance st:%d", status);
    ASSERTS_EQUALS(amxd_object_get_type(object), amxd_object_instance, status, ME, "obj is not instance");
    const char* name = amxd_object_get_name(object, AMXD_OBJECT_NAMED);
    SAH_TRACEZ_INFO(ME, "%s: destroy instance object(%p)", name, object);
    s_destroySsid(object);

    SAH_TRACEZ_OUT(ME);
    return status;
}

amxd_status_t _wld_ssid_setLowerLayers_pwf(amxd_object_t* object,
                                           amxd_param_t* parameter,
                                           amxd_action_t reason,
                                           const amxc_var_t* const args,
                                           amxc_var_t* const retval,
                                           void* priv) {
    SAH_TRACEZ_IN(ME);

    amxd_status_t rv = amxd_action_param_write(object, parameter, reason, args, retval, priv);
    ASSERT_EQUALS(rv, amxd_status_ok, rv, ME, "fail to set value rv:%d", rv);
    const char* lowerLayer = amxc_var_constcast(cstring_t, args);
    ASSERT_NOT_NULL(lowerLayer, amxd_status_ok, ME, "NULL");
    T_SSID* pSSID = wld_ssid_fromObj(object);
    if((pSSID == NULL) && (!swl_str_isEmpty(lowerLayer))) {
        pSSID = s_createSsidFromObj(object);
    }
    ASSERTI_NOT_NULL(pSSID, amxd_status_ok, ME, "No SSID Ctx");

    T_Radio* pRad = (T_Radio*) swla_object_getReferenceObjectPriv(object, lowerLayer);
    if(pRad != NULL) {
        SAH_TRACEZ_INFO(ME, "SSID (%s) has radio LowerLayer (%s) and refers to pRad(%p:%s)",
                        pSSID->Name, lowerLayer, pRad, pRad->Name);
    } else {
        SAH_TRACEZ_INFO(ME, "SSID (%s) has no identified radio LowerLayer (%s)",
                        pSSID->Name, lowerLayer);
    }
    pSSID->RADIO_PARENT = pRad;
    pSSID->autoMacSrc = WLD_AUTOMACSRC_RADIO_BASE;

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

static const char* s_getAutoMacSrcName(uint32_t srcId) {
    ASSERT_TRUE(srcId < SWL_ARRAY_SIZE(wld_autoMacSrc_str), "Invalid", ME, "invalid id %d", srcId);
    return wld_autoMacSrc_str[srcId];
}

bool wld_ssid_hasAutoMacBssIndex(T_SSID* pSSID, int32_t* pBssIndex) {
    if(pSSID && (pSSID->autoMacSrc == WLD_AUTOMACSRC_RADIO_BASE) && (pSSID->bssIndex > -1)) {
        W_SWL_SETPTR(pBssIndex, pSSID->bssIndex);
        return true;
    }
    return false;
}

int32_t wld_rad_getHighestVapAutoMacBssIndex(T_Radio* pR) {
    int32_t maxBssIndex = -1;
    ASSERTS_NOT_NULL(pR, maxBssIndex, ME, "NULL");
    T_AccessPoint* pAP;
    wld_rad_forEachAp(pAP, pR) {
        int32_t bssIndex = -1;
        if(wld_ssid_hasAutoMacBssIndex(pAP->pSSID, &bssIndex) && (bssIndex > maxBssIndex)) {
            maxBssIndex = bssIndex;
        }
    }
    return maxBssIndex;
}

void s_setBssIndex(T_Radio* pRad, T_SSID* pSSID, int32_t index) {
    ASSERT_NOT_NULL(pSSID, , ME, "NULL");
    ASSERT_NOT_NULL(pRad, , ME, "%s: No mapped radio", pSSID->Name);
    ASSERTS_NOT_EQUALS(pSSID->bssIndex, index, , ME, "same value");
    bool prev = wld_rad_macCfg_hasShiftedMbssBaseMac(pRad);
    pSSID->bssIndex = index;
    if(prev != wld_rad_macCfg_hasShiftedMbssBaseMac(pRad)) {
        wld_rad_macCfg_updateRadBaseMac(pRad);
    }
}

void wld_ssid_generateMac(T_Radio* pRad, T_SSID* pSSID, uint32_t index, swl_macBin_t* macBin) {
    ASSERT_NOT_NULL(pSSID, , ME, "NULL");
    ASSERT_NOT_NULL(pRad, , ME, "%s: No mapped radio", pSSID->Name);
    T_AccessPoint* pAP = pSSID->AP_HOOK;
    T_EndPoint* pEP = pSSID->ENDP_HOOK;
    ASSERT_FALSE((pAP == NULL) && (pEP == NULL), , ME, "%s: No mapped AP/EP", pSSID->Name);
    swl_rc_ne rc = SWL_RC_ERROR;
    const char* ifname = (pAP != NULL) ? pAP->alias : pEP->Name;
    const char* macType = (pAP != NULL) ? "AP BSSID" : "EP MAC";
    const char* macSrc = s_getAutoMacSrcName(pSSID->autoMacSrc);

    s_setBssIndex(pRad, pSSID, index);
    switch(pSSID->autoMacSrc) {
    case WLD_AUTOMACSRC_RADIO_BASE: {
        if(pAP != NULL) {
            rc = wld_rad_macCfg_generateBssid(pRad, ifname, index, macBin);
        } else {
            rc = wld_rad_macCfg_generateEpMac(pRad, ifname, index, macBin);
        }
        break;
    }
    case WLD_AUTOMACSRC_DUMMY: {
        ASSERT_NULL(pEP, , ME, "MUST Not generate dummy MAC address for Endpoint %s interface", ifname);
        rc = wld_rad_macCfg_generateDummyBssid(pRad, ifname, index, macBin);
        break;
    }
    default:
        break;
    }
    if(!swl_rc_isOk(rc)) {
        SAH_TRACEZ_ERROR(ME, "%s: fail to generate %s %s src %s (apIdx:%d)",
                         pSSID->Name, ifname, macType, macSrc, index);
        s_setBssIndex(pRad, pSSID, -1);
        return;
    }
    SAH_TRACEZ_INFO(ME, "%s: gen %s %s src %s rank(%d): "SWL_MAC_FMT,
                    pRad->Name, ifname, macType, macSrc, index, SWL_MAC_ARG(macBin->bMac));
    pSSID->bssIndex = index;
}

void wld_ssid_generateBssid(T_Radio* pRad, T_AccessPoint* pAP, uint32_t apIndex, swl_macBin_t* macBin) {
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    wld_ssid_generateMac(pRad, pAP->pSSID, apIndex, macBin);
}

void wld_ssid_setMac(T_SSID* pSSID, swl_macBin_t* macBin) {
    ASSERT_NOT_NULL(pSSID, , ME, "NULL");
    ASSERT_NOT_NULL(macBin, , ME, "NULL");
    memcpy(pSSID->MACAddress, macBin->bMac, sizeof(pSSID->MACAddress));
}

void wld_ssid_setBssid(T_SSID* pSSID, swl_macBin_t* macBin) {
    ASSERT_NOT_NULL(pSSID, , ME, "NULL");
    ASSERT_NOT_NULL(macBin, , ME, "NULL");
    memcpy(pSSID->BSSID, macBin->bMac, ETHER_ADDR_LEN);
    wld_ssid_setMac(pSSID, macBin);
}

T_SSID* wld_ssid_getSsidByBssid(swl_macBin_t* macBin) {
    amxc_llist_for_each(it, &sSsidList) {
        T_SSID* pSSID = amxc_container_of(it, T_SSID, it);
        if(SWL_MAC_BIN_MATCHES(pSSID->BSSID, macBin)) {
            return pSSID;
        }
    }
    return NULL;
}

T_SSID* wld_ssid_getSsidByIfIndex(int32_t ifIndex) {
    ASSERTS_FALSE(ifIndex <= 0, NULL, ME, "Invalid ifIndex %d", ifIndex);
    amxc_llist_for_each(it, &sSsidList) {
        T_SSID* pSSID = amxc_container_of(it, T_SSID, it);
        T_AccessPoint* pAP = pSSID->AP_HOOK;
        T_EndPoint* pEP = pSSID->ENDP_HOOK;
        if(((pAP != NULL) && ((int32_t) pAP->index == ifIndex))
           || ((pEP != NULL) && ((int32_t) pEP->index == ifIndex))) {
            return pSSID;
        }
    }
    return NULL;
}

T_SSID* wld_ssid_getSsidByMacAddress(swl_macBin_t* macBin) {
    amxc_llist_for_each(it, &sSsidList) {
        T_SSID* pSSID = amxc_container_of(it, T_SSID, it);
        if(SWL_MAC_BIN_MATCHES(pSSID->MACAddress, macBin)) {
            return pSSID;
        }
    }
    return NULL;
}

T_SSID* wld_ssid_getSsidByIfName(const char* ifName) {
    ASSERTS_STR(ifName, NULL, ME, "NULL");
    T_AccessPoint* pAP = wld_vap_from_name(ifName);
    if(pAP != NULL) {
        return pAP->pSSID;
    }
    T_EndPoint* pEP = wld_vep_from_name(ifName);
    if(pEP != NULL) {
        return pEP->pSSID;
    }
    return NULL;
}
wld_wpaCtrlInterface_t* wld_ssid_getWpaCtrlIface(T_SSID* pSSID) {
    ASSERTS_NOT_NULL(pSSID, NULL, ME, "NULL");
    if(pSSID->AP_HOOK != NULL) {
        return pSSID->AP_HOOK->wpaCtrlInterface;
    }
    if(pSSID->ENDP_HOOK != NULL) {
        return pSSID->ENDP_HOOK->wpaCtrlInterface;
    }
    return NULL;
}

wld_ssidType_e wld_ssid_getType(T_SSID* pSSID) {
    ASSERTS_NOT_NULL(pSSID, WLD_SSID_TYPE_UNKNOWN, ME, "NULL");
    if(pSSID->AP_HOOK != NULL) {
        return WLD_SSID_TYPE_AP;
    }
    if(pSSID->ENDP_HOOK != NULL) {
        return WLD_SSID_TYPE_EP;
    }
    return WLD_SSID_TYPE_UNKNOWN;
}

const char* wld_ssid_getIfName(T_SSID* pSSID) {
    ASSERTS_NOT_NULL(pSSID, "", ME, "NULL");
    if(pSSID->AP_HOOK != NULL) {
        return pSSID->AP_HOOK->alias;
    }
    if(pSSID->ENDP_HOOK != NULL) {
        return pSSID->ENDP_HOOK->Name;
    }
    return "";
}

int32_t wld_ssid_getIfIndex(T_SSID* pSSID) {
    ASSERTS_NOT_NULL(pSSID, -1, ME, "NULL");
    if(pSSID->AP_HOOK != NULL) {
        return pSSID->AP_HOOK->index;
    }
    if(pSSID->ENDP_HOOK != NULL) {
        return pSSID->ENDP_HOOK->index;
    }
    return -1;
}

bool wld_ssid_isLinkCreated(T_SSID* pSSID) {
    ASSERTS_NOT_NULL(pSSID, false, ME, "NULL");
    ASSERTS_FALSE(swl_mac_binIsNull((swl_macBin_t*) pSSID->MACAddress), false, ME, "no mac");
    return (wld_mld_isLinkActive(pSSID->pMldLink) ||
            (!wld_mld_isLinkEnabled(pSSID->pMldLink) && (wld_ssid_getIfIndex(pSSID) > 0)));
}

bool wld_ssid_isLinkEnabled(T_SSID* pSSID) {
    ASSERTS_NOT_NULL(pSSID, false, ME, "NULL");
    ASSERTS_FALSE(swl_mac_binIsNull((swl_macBin_t*) pSSID->MACAddress), false, ME, "no mac");
    const char* ifname = wld_ssid_getIfName(pSSID);
    ASSERTS_FALSE(swl_str_isEmpty(ifname), false, ME, "no ifname");
    int ret = SWL_RC_NOT_IMPLEMENTED;
    if(pSSID->AP_HOOK != NULL) {
        ret = pSSID->AP_HOOK->pFA->mfn_wvap_enable(pSSID->AP_HOOK, 0, GET | DIRECT);
    }
    if(ret == SWL_RC_NOT_IMPLEMENTED) {
        ret = wld_linuxIfUtils_getStateExt((char*) ifname);
    }
    return (ret > 0);
}

bool wld_ssid_isLinkActive(T_SSID* pSSID) {
    ASSERTS_NOT_NULL(pSSID, false, ME, "NULL");
    ASSERTS_FALSE(swl_mac_binIsNull((swl_macBin_t*) pSSID->MACAddress), false, ME, "no mac");
    const char* ifname = wld_ssid_getIfName(pSSID);
    ASSERTS_FALSE(swl_str_isEmpty(ifname), false, ME, "no ifname");
    if(pSSID->AP_HOOK != NULL) {
        return (pSSID->AP_HOOK->pFA->mfn_wvap_status(pSSID->AP_HOOK) > 0);
    }
    if(pSSID->ENDP_HOOK != NULL) {
        return (pSSID->ENDP_HOOK->pFA->mfn_wendpoint_status(pSSID->ENDP_HOOK) >= SWL_RC_OK);
    }
    return false;
}

const char* wld_ssid_getLinkIfName(T_SSID* pSSID) {
    ASSERTS_NOT_NULL(pSSID, "", ME, "NULL");
    const char* ifname = wld_mld_getPrimaryLinkIfName(pSSID->pMldLink);
    if(swl_str_isEmpty(ifname)) {
        ifname = wld_ssid_getIfName(pSSID);
    }
    return ifname;
}

int32_t wld_ssid_getLinkIfIndex(T_SSID* pSSID) {
    ASSERTS_NOT_NULL(pSSID, -1, ME, "NULL");
    const char* ifname = wld_ssid_getLinkIfName(pSSID);
    ASSERTS_FALSE(swl_str_isEmpty(ifname), -1, ME, "no ifname");
    int ifIndex = 0;
    wld_linuxIfUtils_getIfIndexExt((char*) ifname, &ifIndex);
    return ifIndex;
}

bool wld_ssid_hasMloSupport(T_SSID* pSSID) {
    ASSERTS_NOT_NULL(pSSID, false, ME, "NULL");
    if(wld_rad_hasMloSupport(pSSID->RADIO_PARENT)) {
        T_AccessPoint* pAP = pSSID->AP_HOOK;
        if(pAP != NULL) {
            return (pAP->WMMCapability && pAP->WMMEnable);
        }
        return true;
    }
    return false;
}

bool wld_ssid_isSyncEnablePending(T_SSID* pSSID) {
    ASSERTS_NOT_NULL(pSSID, false, ME, "NULL");
    if((amxp_timer_get_state(pSSID->enableSyncTimer) == amxp_timer_started) ||
       (amxp_timer_get_state(pSSID->enableSyncTimer) == amxp_timer_running)) {
        return true;
    }
    return false;
}

swl_rc_ne wld_ssid_syncEnable(T_SSID* pSSID, bool toIntf) {
    ASSERT_NOT_NULL(pSSID, SWL_RC_INVALID_PARAM, ME, "NULL");
    bool otherEnable = wld_ssid_getIntfEnable(pSSID);

    SAH_TRACEZ_INFO(ME, "%s: check do sync to SSID %u %u - %s",
                    pSSID->Name, pSSID->enable, otherEnable, wld_config_getEnableSyncModeStr());

    if(otherEnable == pSSID->enable) {
        amxp_timer_stop(pSSID->enableSyncTimer);
        return SWL_RC_DONE;
    }

    if(!wld_config_isEnableSyncNeeded(toIntf)) {
        return SWL_RC_NOT_AVAILABLE;
    }

    pSSID->syncEnableToIntf = toIntf;

    SAH_TRACEZ_INFO(ME, "%s: do sync to SSID %u", pSSID->Name, otherEnable);
    if(wld_ssid_isSyncEnablePending(pSSID)) {
        return SWL_RC_CONTINUE;
    }

    amxp_timer_start(pSSID->enableSyncTimer, 1);
    return SWL_RC_OK;
}


/**
 * Return whether the given SSID is configured with a non default SSID
 */
bool wld_ssid_isSSIDConfigured(T_SSID* pSSID) {
    ASSERTS_NOT_NULL(pSSID, false, ME, "NULL");
    ASSERTS_NOT_NULL(pSSID->pBus, false, ME, "NULL");
    uint32_t id = amxd_object_get_index(pSSID->pBus);
    char buffer[SWL_80211_SSID_STR_LEN];
    s_generateDefaultSSID(buffer, sizeof(buffer), id);
    return !swl_str_matches(buffer, pSSID->SSID);
}

static void s_setEnable_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param, const amxc_var_t* const newValue _UNUSED) {
    SAH_TRACEZ_IN(ME);
    T_SSID* pSSID = wld_ssid_fromObj(object);
    ASSERT_NOT_NULL(pSSID, , ME, "INVALID");

    amxc_var_t myVar;
    amxc_var_init(&myVar);
    amxd_status_t status = amxd_param_get_value(param, &myVar);
    ASSERT_EQUALS(status, amxd_status_ok, , ME, "%s: fail to receive latest enable value", pSSID->Name);
    bool newEnable = amxc_var_dyncast(bool, &myVar);
    amxc_var_clean(&myVar);

    SAH_TRACEZ_INFO(ME, "%s: set enable %d -> %d", pSSID->Name, pSSID->enable, newEnable);

    ASSERTI_NOT_EQUALS(newEnable, pSSID->enable, , ME, "%s: set to same enable %d", pSSID->Name, newEnable);


    bool newCombEna = s_checkEnableWithRef(pSSID, newEnable);
    SAH_TRACEZ_INFO(ME, "%s set SSID Enable %u (comb %u)", pSSID->Name, newEnable, newCombEna);
    s_setEnable_internal(pSSID, newEnable);
    swl_rc_ne rc = wld_ssid_syncEnable(pSSID, true);

    /*
     * if sync is not supported, or already happened,
     * then apply combined enable immediately
     * otherwise, wait for sync to be done
     */
    if((rc == SWL_RC_NOT_AVAILABLE) || (rc == SWL_RC_DONE)) {
        wld_ssid_applyEnable(pSSID, newCombEna, newEnable);
    }

    SAH_TRACEZ_OUT(ME);
}


static void s_checkMLDUnit(T_SSID* pSSID, int32_t newMldUnit) {
    SAH_TRACEZ_IN(ME);
    ASSERT_NOT_NULL(pSSID, , ME, "INVALID");
    if(pSSID->mldUnit == newMldUnit) {
        return;
    }
    SAH_TRACEZ_INFO(ME, "%s: SET MLD_UNIT %d %d", pSSID->Name, pSSID->mldUnit, newMldUnit);

    pSSID->mldUnit = newMldUnit;
    wld_mld_setLinkConfigured(pSSID->pMldLink, false);
    if(pSSID->AP_HOOK != NULL) {
        T_AccessPoint* pAP = pSSID->AP_HOOK;
        pAP->pFA->mfn_wvap_setMldUnit(pAP);
        wld_autoCommitMgr_notifyVapEdit(pAP);
    } else if(pSSID->ENDP_HOOK != NULL) {
        T_EndPoint* pEP = pSSID->ENDP_HOOK;
        pEP->pFA->mfn_wendpoint_setMldUnit(pEP);
        wld_autoCommitMgr_notifyEpEdit(pEP);
    }
    wld_mld_registerLink(pSSID, pSSID->mldUnit);

    SAH_TRACEZ_OUT(ME);
}

static void s_setSSIDConf_ocf(void* priv _UNUSED, amxd_object_t* object, const amxc_var_t* const newParamValues _UNUSED) {
    SAH_TRACEZ_IN(ME);
    T_SSID* pSSID = wld_ssid_fromObj(object);
    ASSERT_NOT_NULL(pSSID, , ME, "INVALID");
    s_checkMLDUnit(pSSID, amxd_object_get_int32_t(object, "MLDUnit", NULL));

    SAH_TRACEZ_OUT(ME);
}

int16_t wld_ssid_getMLDLinkID(T_SSID* pSSID) {
    ASSERTS_NOT_NULL(pSSID, -1, ME, "NULL");
    return wld_mld_getLinkId(pSSID->pMldLink);
}

void wld_ssid_resetMloStats(T_SSID* pSSID) {
    ASSERTS_NOT_NULL(pSSID, , ME, "NULL");
    memset(&pSSID->accuMloStats, 0, sizeof(wld_stats_t));
}

swl_rc_ne wld_ssid_accuMloStats(T_SSID* pSSID, wld_stats_t* pDiffStats) {
    ASSERTS_NOT_NULL(pSSID, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pDiffStats, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_util_accumulateStats(&pSSID->accuMloStats, pDiffStats);
    return SWL_RC_OK;
}

swl_rc_ne wld_ssid_getMloStats(T_SSID* pSSID, wld_stats_t* pOutStats) {
    ASSERTS_NOT_NULL(pSSID, SWL_RC_INVALID_PARAM, ME, "NULL");
    if(wld_ssid_getMLDLinkID(pSSID) < 0) {
        return SWL_RC_NOT_AVAILABLE;
    }
    W_SWL_SETPTR(pOutStats, pSSID->accuMloStats);
    return SWL_RC_OK;
}

swl_rc_ne wld_ssid_accuNetStats(T_SSID* pSSID, wld_stats_t* pDiffStats) {
    ASSERTS_NOT_NULL(pSSID, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pDiffStats, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_util_accumulateStats(&pSSID->stats, pDiffStats);
    return SWL_RC_OK;
}

swl_rc_ne wld_ssid_getNetStats(T_SSID* pSSID, wld_stats_t* pOutStats) {
    ASSERTS_NOT_NULL(pSSID, SWL_RC_INVALID_PARAM, ME, "NULL");
    W_SWL_SETPTR(pOutStats, pSSID->stats);
    return SWL_RC_OK;
}

/**
 * Set the MLD Role of this endpoint.
 * Shall only be relevant if endpoint is enabled
 */
void wld_ssid_setMLDRole(T_SSID* pSSID, swl_mlo_role_e mldRole) {
    ASSERTS_NOT_NULL(pSSID, , ME, "NULL");
    if(mldRole == SWL_MLO_ROLE_PRIMARY) {
        wld_mld_setPrimaryLink(pSSID->pMldLink);
    }
    ASSERTI_NOT_EQUALS(pSSID->mldRole, mldRole, , ME, "%s: same role %s", pSSID->Name, swl_mlo_role_str[mldRole]);
    pSSID->mldRole = mldRole;
    if(pSSID->ENDP_HOOK != NULL) {
        wld_endpoint_setConnectionStatus(pSSID->ENDP_HOOK, pSSID->ENDP_HOOK->connectionStatus, EPE_NONE);
    }

    amxd_trans_t trans;
    ASSERT_TRANSACTION_INIT(pSSID->pBus, &trans, , ME, "%s : trans init failure", pSSID->Name);
    amxd_trans_set_value(cstring_t, &trans, "MLDRole", swl_mlo_role_str[mldRole]);
    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, , ME, "trans apply failure");
}

void wld_ssid_setMLDLinkID(T_SSID* pSSID, int16_t mldLinkId) {
    ASSERTS_NOT_NULL(pSSID, , ME, "NULL");
    wld_mld_setLinkId(pSSID->pMldLink, mldLinkId);
    ASSERTI_NOT_EQUALS(pSSID->mldLinkId, mldLinkId, , ME, "%s: same id %d", pSSID->Name, mldLinkId);
    pSSID->mldLinkId = mldLinkId;

    amxd_trans_t trans;
    ASSERT_TRANSACTION_INIT(pSSID->pBus, &trans, , ME, "%s : trans init failure", pSSID->Name);
    amxd_trans_set_value(int16_t, &trans, "MLDLinkID", mldLinkId);
    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, , ME, "trans apply failure");
}

void wld_ssid_setMLDStatus(T_SSID* pSSID, swl_mlo_intfMldStatus_e mldStatus) {
    ASSERTI_NOT_EQUALS(pSSID->mldStatus, mldStatus, , ME, "%s: same mldStatus %d", pSSID->Name, mldStatus);
    pSSID->mldStatus = mldStatus;

    amxd_trans_t trans;
    ASSERT_TRANSACTION_INIT(pSSID->pBus, &trans, , ME, "%s : trans init failure", pSSID->Name);
    amxd_trans_set_value(cstring_t, &trans, "MLDStatus", swl_mlo_intfMldStatus_str[mldStatus]);
    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, , ME, "trans apply failure");
}



static void s_copyEpStats(T_Stats* pStats, T_EndPointStats* pEpStats) {
    ASSERTS_NOT_NULL(pStats, , ME, "NULL");
    ASSERTS_NOT_NULL(pEpStats, , ME, "NULL");
    pStats->BytesSent = pEpStats->txbyte;
    pStats->BytesReceived = pEpStats->rxbyte;
    pStats->PacketsSent = pEpStats->txPackets;
    pStats->PacketsReceived = pEpStats->rxPackets;
    pStats->RetransCount = pEpStats->Retransmissions;
    pStats->RetryCount = pEpStats->txRetries + pEpStats->rxRetries;
    pStats->noise = pEpStats->noise;
}

static amxd_status_t s_updateSsidStatsValues(T_SSID* pSSID, amxd_object_t* stats) {
    ASSERTS_NOT_NULL(pSSID, amxd_status_invalid_value, ME, "NULL");
    T_AccessPoint* pAP = (T_AccessPoint*) pSSID->AP_HOOK;
    T_EndPoint* pEP = (T_EndPoint*) pSSID->ENDP_HOOK;

    if(debugIsEpPointer(pEP)) {

        T_EndPointStats epStats;
        memset(&epStats, 0, sizeof(epStats));
        if((!wld_rad_hasRunningEndpoint(pEP->pRadio)) || (pEP->pFA->mfn_wendpoint_stats(pEP, &epStats) < SWL_RC_OK)) {
            /* Update the stats with Linux counters if we don't handle them in the vendor plugin. */
            wld_updateEPStats(pEP, NULL);
        } else {
            s_copyEpStats(&pSSID->stats, &epStats);
        }

    } else if(debugIsVapPointer(pAP)) {

        if(pAP->status != APSTI_ENABLED) {
            memset(&pSSID->stats, 0, sizeof(T_Stats));
        } else if(pAP->pFA->mfn_wvap_update_ap_stats(pAP) < SWL_RC_OK) {
            /* Update the stats with Linux counters if we don't handle them in the vendor plugin. */
            wld_updateVAPStats(pAP, NULL);
        }

    } else {
        SAH_TRACEZ_INFO(ME, "%s: no mapped AP/EP", pSSID->Name);
        return amxd_status_unknown_error;
    }
    ASSERTS_NOT_NULL(stats, amxd_status_ok, ME, "Nothing to copy, only refresh");
    return wld_util_stats2Obj(stats, &pSSID->stats);
}

amxd_status_t _SSID_getSSIDStats(amxd_object_t* object,
                                 amxd_function_t* func _UNUSED,
                                 amxc_var_t* args _UNUSED,
                                 amxc_var_t* retval) {
    SAH_TRACEZ_INFO(ME, "getSSIDStats");

    amxd_status_t status = amxd_status_ok;
    T_SSID* pSSID = (T_SSID*) object->priv;

    if(!pSSID || !debugIsSsidPointer(pSSID)) {
        SAH_TRACEZ_ERROR(ME, "Invalid pointer !");
        return amxd_status_ok;
    }

    amxd_object_t* stats = amxd_object_get(object, "Stats");
    status = s_updateSsidStatsValues(pSSID, stats);
    ASSERT_EQUALS(status, amxd_status_ok, status, ME, "fail to update stats");
    amxc_var_init(retval);
    amxc_var_set_type(retval, AMXC_VAR_ID_HTABLE);
    wld_util_statsObj2Var(retval, stats);

    return amxd_status_ok;
}

amxd_status_t _wld_ssid_getStats_orf(amxd_object_t* const object,
                                     amxd_param_t* const param,
                                     amxd_action_t reason,
                                     const amxc_var_t* const args,
                                     amxc_var_t* const action_retval,
                                     void* priv) {
    SAH_TRACEZ_IN(ME);

    amxd_status_t status = amxd_status_ok;
    if(reason != action_object_read) {
        status = amxd_status_function_not_implemented;
        return status;
    }

    T_SSID* pSSID = (T_SSID*) amxd_object_get_parent(object)->priv;

    if(!pSSID || !debugIsSsidPointer(pSSID)) {
        SAH_TRACEZ_INFO(ME, "SSID not present !");
        return amxd_status_unknown_error;
    }

    s_updateSsidStatsValues(pSSID, object);

    status = amxd_action_object_read(object, param, reason, args, action_retval, priv);
    return status;
}


/**
 * check the SSID object block on wrong input.
 * Currently only SSID is checked. (All other are accept by default).
 */
amxd_status_t _wld_ssid_validateSSID_pvf(amxd_object_t* object _UNUSED,
                                         amxd_param_t* param,
                                         amxd_action_t reason _UNUSED,
                                         const amxc_var_t* const args,
                                         amxc_var_t* const retval _UNUSED,
                                         void* priv _UNUSED) {
    ASSERTS_FALSE(amxc_var_is_null(args), amxd_status_invalid_value, ME, "invalid");
    ASSERTS_EQUALS(amxd_object_get_type(object), amxd_object_instance, amxd_status_ok, ME, "obj is not instance");
    SAH_TRACEZ_IN(ME);
    amxd_status_t status = amxd_status_invalid_value;
    const char* currentValue = amxc_var_constcast(cstring_t, &param->value);
    ASSERT_NOT_NULL(currentValue, status, ME, "NULL");
    char* newValue = amxc_var_dyncast(cstring_t, args);
    ASSERT_NOT_NULL(newValue, status, ME, "NULL");
    if(swl_str_matches(currentValue, newValue) || isValidSSID(newValue)) {
        status = amxd_status_ok;
    } else {
        SAH_TRACEZ_ERROR(ME, "invalid SSID(%s)", newValue);
    }
    free(newValue);
    SAH_TRACEZ_OUT(ME);
    return status;
}

static void s_setSSID_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);
    T_SSID* pSSID = wld_ssid_fromObj(object);
    ASSERT_NOT_NULL(pSSID, , ME, "INVALID");

    char* SSID = amxc_var_dyncast(cstring_t, newValue);
    ASSERT_NOT_NULL(SSID, , ME, "NULL");
    if(swl_str_matches(SSID, pSSID->SSID)) {
        SAH_TRACEZ_INFO(ME, "%s: same SSID %s", pSSID->Name, SSID);
        free(SSID);
        return;
    }
    SAH_TRACEZ_INFO(ME, "%s: set SSID %s", pSSID->Name, SSID);

    T_AccessPoint* pAP = (T_AccessPoint*) pSSID->AP_HOOK;
    if(pAP) {
        SAH_TRACEZ_INFO(ME, "%s: apply to AP %p", pSSID->Name, pAP);
        pAP->pFA->mfn_wvap_ssid(pAP, (char*) SSID, strlen(SSID), SET);
        wld_autoCommitMgr_notifyVapEdit(pAP);
        wld_autoNeighAdd_vapSetDelNeighbourAP(pAP, true);
        wld_mld_setLinkConfigured(pSSID->pMldLink, false);
    }
    free(SSID);
    //Setting endpoint ssid should only be done internally => no change necessary here.

    SAH_TRACEZ_OUT(ME);
}

/**
 * Write handler for MAC address settings.
 */
static void s_setMacAddress_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_SSID* pSSID = wld_ssid_fromObj(object);
    ASSERT_NOT_NULL(pSSID, , ME, "INVALID");
    const char* pMacStr = amxc_var_constcast(cstring_t, newValue);

    SAH_TRACEZ_INFO(ME, "pMacStr=%s pSSID->MAC=%.2X:%.2X:%.2X:%.2X:%.2X:%.2X", (pMacStr ? pMacStr : "NULL"),
                    pSSID->MACAddress[0], pSSID->MACAddress[1], pSSID->MACAddress[2],
                    pSSID->MACAddress[3], pSSID->MACAddress[4], pSSID->MACAddress[5]);

    swl_macBin_t mac = SWL_MAC_BIN_NEW();
    if(!SWL_MAC_CHAR_TO_BIN(&mac, pMacStr)) {
        SAH_TRACEZ_ERROR(ME, "MAC conversion failed %s", pMacStr);
        return;
    }
    if(swl_mac_binIsNull(&mac)) {
        SAH_TRACEZ_WARNING(ME, "[%s] SSID Mac Address is NULL: %s, skipping update", pSSID->Name, pMacStr);
        return;
    }
    if(!SWL_MAC_BIN_MATCHES(pSSID->MACAddress, &mac)) {
        memcpy(pSSID->MACAddress, mac.bMac, ETHER_ADDR_LEN);
        T_EndPoint* pEP = (T_EndPoint*) pSSID->ENDP_HOOK;
        T_AccessPoint* pAP = (T_AccessPoint*) pSSID->AP_HOOK;
        if((pAP != NULL) && debugIsVapPointer(pAP) && (pAP->pSSID == pSSID)) {
            SAH_TRACEZ_INFO(ME, "[%s] Accesspoint Mac Address : %s", pAP->alias, pMacStr);
            memcpy(pSSID->BSSID, mac.bMac, ETHER_ADDR_LEN);
            pAP->pFA->mfn_wvap_bssid(NULL, pAP, (unsigned char*) pMacStr, ETHER_ADDR_STR_LEN, SET);
            wld_autoCommitMgr_notifyVapEdit(pAP);
        } else if((pEP != NULL) && debugIsEpPointer(pEP) && (pEP->pSSID == pSSID)) {
            SAH_TRACEZ_INFO(ME, "[%s] Endpoint Mac Address : %s", pEP->Name, pMacStr);
            pEP->pFA->mfn_wendpoint_set_mac_address(pEP);
            wld_autoCommitMgr_notifyEpEdit(pEP);
        }
    }

    SAH_TRACEZ_OUT(ME);
}

static void s_setCustomNetDevName_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_SSID* pSSID = wld_ssid_fromObj(object);
    ASSERT_NOT_NULL(pSSID, , ME, "INVALID");
    const char* custNetDevName = amxc_var_constcast(cstring_t, newValue);
    ASSERTS_FALSE(swl_str_matches(pSSID->customNetDevName, custNetDevName), , ME, "same value");
    swl_str_copy(pSSID->customNetDevName, sizeof(pSSID->customNetDevName), custNetDevName);
    int32_t ifIndex = wld_ssid_getIfIndex(pSSID);
    SAH_TRACEZ_INFO(ME, "%s: SET CustomNetDevName %s (ndIdx %u)",
                    pSSID->Name, pSSID->customNetDevName, ifIndex);
    if(ifIndex > 0) {
        SAH_TRACEZ_WARNING(ME, "%s: interface %d already created: new custom ifname %s will be applied on next boot",
                           pSSID->Name, ifIndex, custNetDevName);
    }

    SAH_TRACEZ_OUT(ME);
}

SWLA_DM_HDLRS(sSsidDmHdlrs,
              ARR(SWLA_DM_PARAM_HDLR("SSID", s_setSSID_pwf),
                  SWLA_DM_PARAM_HDLR("CustomNetDevName", s_setCustomNetDevName_pwf),
                  SWLA_DM_PARAM_HDLR("MACAddress", s_setMacAddress_pwf),
                  SWLA_DM_PARAM_HDLR("Enable", s_setEnable_pwf)),
              .objChangedCb = s_setSSIDConf_ocf);

void _wld_ssid_setConf_ocf(const char* const sig_name,
                           const amxc_var_t* const data,
                           void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sSsidDmHdlrs, sig_name, data, priv);
}

void syncData_SSID2OBJ(amxd_object_t* object, T_SSID* pS, int set) {
    //int idx,bitchk,asdev;
    char ValBuf[32];
    char TBuf[128];
    char objPath[128];
    T_AccessPoint* pAP = NULL;
    T_EndPoint* pEP = NULL;

    SAH_TRACEZ_IN(ME);
    if(!(object && pS)) {
        /* Missing data pointers... */
        return;
    }

    pAP = (T_AccessPoint*) pS->AP_HOOK;
    pEP = (T_EndPoint*) pS->ENDP_HOOK;

    if(set & SET) {
        memset(ValBuf, 0, sizeof(ValBuf));
        memset(TBuf, 0, sizeof(TBuf));
        memset(objPath, 0, sizeof(objPath));

        amxd_trans_t trans;
        ASSERT_TRANSACTION_INIT(object, &trans, , ME, "%s : trans init failure", pS->Name);

        /* Set SSID data in mapped OBJ structure */

        /** 'Status' The current operational state of the SSID entry. */
        amxd_trans_set_cstring_t(&trans, "Status", SSID_SupStatus[pS->status]);
        swl_typeTimeMono_toTransParam(&trans, "LastStatusChangeTimeStamp", pS->changeInfo.lastStatusChange);

        /** 'SSID' The current service set identifier in use by the
         *  connection. The SSID is an identifier that is attached to
         *  packets sent over the wireless LAN that functions as an
         *  ID for joining a particular radio network (BSS). */
        if(pEP != NULL) {
            // Only need to set SSID if Endpoint. In case of accespoint, this is incoming config.
            amxd_trans_set_cstring_t(&trans, "SSID", pS->SSID);
        }


        /** 'MACAddress' The MAC address of this interface.  */
        sprintf(TBuf, "%.2X:%.2X:%.2X:%.2X:%.2X:%.2X",
                pS->MACAddress[0], pS->MACAddress[1], pS->MACAddress[2],
                pS->MACAddress[3], pS->MACAddress[4], pS->MACAddress[5]);
        amxd_trans_set_cstring_t(&trans, "MACAddress", TBuf);


        /** 'BSSID' The Basic Service Set ID. This is the MAC address
         *  of the access point, which can either be local (when this
         *  instance models an access point SSID) or remote (when
         *  this instance models an end point SSID).
         *  In multiple VAP setup it differs */
        int err = 0;
        if(pAP) {
            err = pAP->pFA->mfn_wvap_bssid(NULL, pAP, (unsigned char*) TBuf, sizeof(TBuf), GET);
        } else if(pEP) {
            err = pEP->pFA->mfn_wendpoint_bssid(pEP, (swl_macChar_t*) TBuf);
        } else {
            err = WLD_ERROR_NOT_IMPLEMENTED;
        }

        if(err < 0) {
            snprintf(TBuf, sizeof(TBuf), WLD_EMPTY_MAC_ADDRESS);
            memset(pS->BSSID, 0, ETHER_ADDR_LEN);
        } else {
            wldu_convStr2Mac(pS->BSSID, ETHER_ADDR_LEN, (char*) TBuf, ETHER_ADDR_STR_LEN);
        }
        amxd_trans_set_cstring_t(&trans, "BSSID", TBuf);

        TBuf[0] = 0;
        int32_t ifIndex = 0;
        if(pAP != NULL) {
            swl_str_copy(TBuf, sizeof(TBuf), pAP->alias);
            ifIndex = pAP->index;
        } else if(pEP != NULL) {
            swl_str_copy(TBuf, sizeof(TBuf), pEP->Name);
            ifIndex = pEP->index;
        }
        amxd_trans_set_cstring_t(&trans, "Name", TBuf);
        amxd_trans_set_uint32_t(&trans, "Index", ifIndex);

        wld_util_initCustomAlias(&trans, object);

        ASSERT_TRANSACTION_LOCAL_DM_END(&trans, , ME, "%s : trans apply failure", pS->Name);
    } else {
        /* Get SSID data from OBJ to pSSID */
        s_setSSID_pwf(NULL, object,
                      amxd_object_get_param_def(object, "SSID"),
                      amxd_object_get_param_value(object, "SSID"));

        const amxc_var_t* savedMacVar = amxd_object_get_param_value(object, "MACAddress");
        const char* savedMacStr = amxc_var_constcast(cstring_t, savedMacVar);
        if(swl_mac_charIsValidStaMac((const swl_macChar_t*) savedMacStr)) {
            s_setMacAddress_pwf(NULL, object,
                                amxd_object_get_param_def(object, "MACAddress"),
                                savedMacVar);
        }

        if(!wld_ssid_isSyncEnablePending(pS)) {
            s_setEnable_pwf(NULL, object,
                            amxd_object_get_param_def(object, "Enable"),
                            amxd_object_get_param_value(object, "Enable"));
        }
    }
    SAH_TRACEZ_OUT(ME);
}

void wld_ssid_setStatus(T_SSID* pSSID, wld_status_e status, bool commit) {
    ASSERT_NOT_NULL(pSSID, , ME, "NULL");
    wld_util_updateStatusChangeInfo(&pSSID->changeInfo, pSSID->status);
    ASSERTI_NOT_EQUALS(pSSID->status, status, , ME, "%s: same status %u", pSSID->Name, status);
    pSSID->changeInfo.lastStatusChange = swl_time_getMonoSec();
    pSSID->changeInfo.nrStatusChanges++;
    pSSID->status = status;

    if(commit) {
        ASSERTI_NOT_NULL(pSSID->pBus, , ME, "%s: not dm configurable", pSSID->Name);
        amxd_trans_t trans;
        ASSERT_TRANSACTION_INIT(pSSID->pBus, &trans, , ME, "%s : trans init failure", pSSID->Name);
        amxd_trans_set_value(cstring_t, &trans, "Status", Rad_SupStatus[pSSID->status]);
        swl_typeTimeMono_toTransParam(&trans, "LastStatusChangeTimeStamp", pSSID->changeInfo.lastStatusChange);
        ASSERT_TRANSACTION_LOCAL_DM_END(&trans, , ME, "%s : trans apply failure", pSSID->Name);
    }
}


amxd_status_t _SSID_getStatusHistogram(amxd_object_t* object,
                                       amxd_function_t* func _UNUSED,
                                       amxc_var_t* args _UNUSED,
                                       amxc_var_t* retval) {
    SAH_TRACEZ_IN(ME);

    T_SSID* pSSID = wld_ssid_fromObj(object);
    ASSERT_NOT_NULL(pSSID, amxd_status_unknown_error, ME, "invalid SSID");

    amxc_var_init(retval);
    amxc_var_set_type(retval, AMXC_VAR_ID_HTABLE);
    wld_util_updateStatusChangeInfo(&pSSID->changeInfo, pSSID->status);
    swl_type_toVariant((swl_type_t*) &gtWld_status_changeInfo, retval, &pSSID->changeInfo);
    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _wld_ssid_getLastChange_prf(amxd_object_t* object,
                                          amxd_param_t* param,
                                          amxd_action_t reason,
                                          const amxc_var_t* const args _UNUSED,
                                          amxc_var_t* const retval,
                                          void* priv _UNUSED) {
    ASSERTS_NOT_NULL(param, amxd_status_unknown_error, ME, "NULL");
    ASSERTS_EQUALS(reason, action_param_read, amxd_status_function_not_implemented, ME, "not impl");
    T_SSID* pSSID = wld_ssid_fromObj(object);
    ASSERTS_NOT_NULL(pSSID, amxd_status_ok, ME, "no SSID mapped");
    uint32_t lastChange = swl_time_getMonoSec() - pSSID->changeInfo.lastStatusChange;
    amxc_var_set(uint32_t, retval, lastChange);
    return amxd_status_ok;
}
