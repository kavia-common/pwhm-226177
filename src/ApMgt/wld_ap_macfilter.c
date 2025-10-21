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
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <debug/sahtrace.h>
#include <assert.h>

#include "wld.h"
#include "wld_util.h"
#include "wld_accesspoint.h"
#include "wld_ssid.h"
#include "wld_radio.h"
#include "wld_assocdev.h"
#include "Utils/wld_autoCommitMgr.h"
#include "swl/swl_assert.h"
#include "swl/swl_string.h"

#define ME "apMf"

/* Delay for deferred MF Entries synchronization (in ms) */
#define MF_SYNC_DELAY_MS 100

static void s_syncAddressList(T_AccessPoint* pAP);
static void s_updateMACFilterAddressList(T_AccessPoint* pAP);

/* Unified timer callback for MF Entries synchronization */
static void s_mfSyncTimer_cb(amxp_timer_t* timer _UNUSED, void* userdata) {
    T_AccessPoint* pAP = (T_AccessPoint*) userdata;
    ASSERTS_NOT_NULL(pAP, , ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: MF sync timer expired, executing delayed sync (syncMfAddrListToObjs:%d)",
                    pAP->alias, pAP->syncMfAddrListToObjs);

    if(pAP->syncMfAddrListToObjs) {
        s_syncAddressList(pAP);
    } else {
        s_updateMACFilterAddressList(pAP);
    }
}

/* Function to start or restart the unified MF synchronization timer */
static void s_startMfSyncTimer(T_AccessPoint* pAP, bool syncMfAddrListToObjs) {
    ASSERTS_NOT_NULL(pAP, , ME, "NULL");
    int ret;
    if(pAP->MF_SyncTimer == NULL) {
        ret = amxp_timer_new(&pAP->MF_SyncTimer, s_mfSyncTimer_cb, pAP);
        ASSERT_EQUALS(ret, 0, , ME, "%s: Failed to create MF sync timer", pAP->alias);
        SAH_TRACEZ_INFO(ME, "%s: Created MF sync timer", pAP->alias);
    }
    /* Restart the timer with the configured delay 100ms */
    ret = amxp_timer_start(pAP->MF_SyncTimer, MF_SYNC_DELAY_MS);
    ASSERT_EQUALS(ret, 0, , ME, "%s: Failed to start MF sync timer", pAP->alias);
    SAH_TRACEZ_INFO(ME, "%s: MF sync timer started with %d ms delay, syncMfAddrListToObjs(%d=>%d)",
                    pAP->alias, amxp_timer_remaining_time(pAP->MF_SyncTimer),
                    pAP->syncMfAddrListToObjs, syncMfAddrListToObjs);
    pAP->syncMfAddrListToObjs = syncMfAddrListToObjs;
}

/* Function to stop and clean up the MF timer */
static void s_stopMfSyncTimer(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, , ME, "NULL");
    amxp_timer_state_t timerState = amxp_timer_get_state(pAP->MF_SyncTimer);
    if((timerState == amxp_timer_started ) || (timerState == amxp_timer_running)) {
        SAH_TRACEZ_INFO(ME, "%s: MF sync timer stopped and deleted", pAP->alias);
    }
    amxp_timer_delete(&pAP->MF_SyncTimer);
}

static void delay_WPS_disable(void* userdata) {
    T_AccessPoint* pAP = (T_AccessPoint*) userdata;
    ASSERTS_NOT_NULL(pAP, , ME, "NULL");
    amxd_object_t* apWpsObj = amxd_object_get(pAP->pBus, "WPS");
    swl_typeUInt8_commitObjectParam(apWpsObj, "Enable", pAP->WPS_Enable);
    wld_wps_updateState(pAP);
}

static bool sync_changes(amxd_object_t* mf, const char* objectName, unsigned char maclist[][ETHER_ADDR_LEN], int* count, int maxentries) {
    int idx = 0;
    int changes = 0;
    amxd_object_t* templateObj = amxd_object_get(mf, objectName);

    amxd_object_for_each(instance, it, templateObj) {
        if(idx >= maxentries) {
            break;
        }
        amxd_object_t* mfentry = amxc_container_of(it, amxd_object_t, it);
        if(mfentry->priv == NULL) {
            continue;
        }
        char* macstr = amxd_object_get_cstring_t(mfentry, "MACAddress", NULL);
        swl_macBin_t macBin;
        if(swl_typeMacBin_fromChar(&macBin, macstr)) {
            if(!SWL_MAC_BIN_MATCHES(&macBin, maclist[idx])) {
                changes++;
                memcpy(maclist[idx], macBin.bMac, ETHER_ADDR_LEN);
            }
            idx++;
        }
        free(macstr);
    }

    if(idx != *count) {
        *count = idx;
        changes++;
    }
    for(; idx < maxentries; idx++) {
        memset(maclist[idx], 0, ETHER_ADDR_LEN);
    }

    SAH_TRACEZ_INFO(ME, "%s.%s internal Count %d", amxd_object_get_name(mf, AMXD_OBJECT_NAMED), objectName, *count);
    for(idx = 0; idx < *count; idx++) {
        SAH_TRACEZ_INFO(ME, "internal %s[%d].Mac = %s", objectName, idx, swl_typeMacBin_toBuf32Ref((swl_macBin_t* ) maclist[idx]).buf);
    }

    return changes != 0;
}

static void s_updateMACFilterAddressList(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, , ME, "NULL");
    ASSERTI_FALSE(pAP->MF_AddressListBlockSync, , ME, "%s: MF Sync blocked (bulk operation in progress)", pAP->alias);
    if(pAP->MF_AddressList != NULL) {
        free(pAP->MF_AddressList);
        pAP->MF_AddressList = NULL;
    }
    const char* newList = "";
    if(pAP->MF_EntryCount > 0) {
        size_t size = (ETHER_ADDR_STR_LEN * pAP->MF_EntryCount) + 1;
        pAP->MF_AddressList = calloc(1, size);
        ASSERT_NOT_NULL(pAP->MF_AddressList, , ME, "NULL");
        for(int i = 0; i < pAP->MF_EntryCount; i++) {
            swl_strlst_cat(pAP->MF_AddressList, size, ",", swl_typeMacBin_toBuf32Ref((swl_macBin_t* ) pAP->MF_Entry[i]).buf);
        }
        swl_str_toUpper(pAP->MF_AddressList, strlen(pAP->MF_AddressList));
        newList = pAP->MF_AddressList;
    }
    swl_typeCharPtr_commitObjectParam(pAP->pBus, "MACFilterAddressList", (char*) newList);
}

/**
 * This function syncs the MAC filtering list for an access point object
 * E.g. WiFiABC.AccessPoint.wl0
 * Note that the mac list changes are only set in the driver, but not executed. One must perform a commit on the radio
 * to sync the plugin mac list with driver mac list.
 */
static void s_syncMACFiltering(amxd_object_t* object) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERTS_NOT_NULL(pAP, , ME, "NULL");

    amxd_object_t* mf = amxd_object_get(object, "MACFiltering");

    bool needSyncToHw = false;
    char* mfModeStr = amxd_object_get_cstring_t(mf, "Mode", NULL);
    wld_mfMode_e mfMode = swl_conv_charToEnum(mfModeStr, cstr_AP_MFMode, APMFM_COUNT, APMFM_OFF);
    free(mfModeStr);
    if(mfMode != pAP->MF_Mode) {
        SAH_TRACEZ_INFO(ME, "%s: Update Mac Filtering Mode from %u to %u", pAP->alias, pAP->MF_Mode, mfMode);
        pAP->MF_Mode = mfMode;
        swl_typeBool_commitObjectParam(pAP->pBus, "MACAddressControlEnabled", (pAP->MF_Mode == APMFM_WHITELIST));
        needSyncToHw = true;
    }

    bool tempBlacklistEnable = amxd_object_get_bool(mf, "TempBlacklistEnable", NULL);
    if(tempBlacklistEnable != pAP->MF_TempBlacklistEnable) {
        SAH_TRACEZ_INFO(ME, "%s: Update MF TempBlacklistEnable to %u", pAP->alias, tempBlacklistEnable);
        pAP->MF_TempBlacklistEnable = tempBlacklistEnable;
        needSyncToHw = true;
    }

    swl_mask_m flag = SET;
    if((pAP->MF_Mode != APMFM_OFF) || pAP->MF_TempBlacklistEnable) {
        int prev_WPS_Enable = pAP->WPS_Enable;
        if(!pAP->WPS_CertMode && (pAP->MF_Mode == APMFM_WHITELIST)) {
            /* We can't have WPS enabled if MAC filtering is set on white list. Disable it. */
            /* For Certification we keep WPS active */
            SAH_TRACEZ_WARNING(ME, "%s: Force disabling WPS because of enabled MF mode WhiteList", pAP->alias);
            pAP->WPS_Enable = 0;
            wld_ap_doWpsSync(pAP);
        }

        if(prev_WPS_Enable != pAP->WPS_Enable) {
            /* Update the data model *after* this commit finishes */
            swla_delayExec_add(delay_WPS_disable, pAP);
        }

        if(sync_changes(mf, "Entry", pAP->MF_Entry, &pAP->MF_EntryCount, MAXNROF_MFENTRY)) {
            s_startMfSyncTimer(pAP, false);
            needSyncToHw = true;
        }
        if(sync_changes(mf, "TempEntry", pAP->MF_Temp_Entry, &pAP->MF_TempEntryCount, MAXNROF_MFENTRY)) {
            //mf temp entries require dynamic acl handling, without need to commit fsm on each change
            flag |= DIRECT;
            needSyncToHw = true;
        }
    }

    ASSERTI_TRUE(needSyncToHw, , ME, "%s: no need for mf hw sync", pAP->alias);
    SAH_TRACEZ_WARNING(ME, "%s: Syncing mac entries to HW", pAP->alias);
    pAP->pFA->mfn_wvap_mf_sync(pAP, flag);
    wld_autoCommitMgr_notifyVapEdit(pAP);

    SAH_TRACEZ_OUT(ME);
}

/**
 * This function syncs the Probe filtering list for an access point object
 * E.g. WiFiABC.AccessPoint.wl0
 * Note that the mac list changes are only set in the driver, but not executed. One must perform a commit on the radio
 * to sync the plugin mac list with driver mac list.
 */
static void s_syncProbeFiltering(amxd_object_t* object) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERTS_NOT_NULL(pAP, , ME, "NULL");

    amxd_object_t* pf = amxd_object_get(object, "ProbeFiltering");
    if(sync_changes(pf, "TempEntry", pAP->PF_Temp_Entry, &pAP->PF_TempEntryCount, MAXNROF_PFENTRY)) {
        SAH_TRACEZ_INFO(ME, "%s: Syncing mac entries to HW", pAP->alias);
        pAP->pFA->mfn_wvap_pf_sync(pAP, SET | DIRECT);
    }

    SAH_TRACEZ_OUT(ME);
}

static void s_setMfConf_ocf(void* priv _UNUSED, amxd_object_t* object, const amxc_var_t* const newParamValues _UNUSED) {
    SAH_TRACEZ_IN(ME);

    amxd_object_t* apObj = amxd_object_get_parent(object);
    s_syncMACFiltering(apObj);

    SAH_TRACEZ_OUT(ME);
}

SWLA_DM_HDLRS(sApMfDmHdlrs, ARR(), .objChangedCb = s_setMfConf_ocf, );

void _wld_apMacFilter_setConf_ocf(const char* const sig_name,
                                  const amxc_var_t* const data,
                                  void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sApMfDmHdlrs, sig_name, data, priv);
}

amxd_status_t _wld_ap_deleteMACFilteringEntry_odf(amxd_object_t* object,
                                                  amxd_param_t* param,
                                                  amxd_action_t reason,
                                                  const amxc_var_t* const args,
                                                  amxc_var_t* const retval,
                                                  void* priv) {
    SAH_TRACEZ_IN(ME);

    amxd_status_t status = amxd_action_object_destroy(object, param, reason, args, retval, priv);
    ASSERT_EQUALS(status, amxd_status_ok, status, ME, "Fail to destroy object st:%d", status);
    ASSERTS_EQUALS(amxd_object_get_type(object), amxd_object_instance, status, ME, "obj is not instance");
    char* path = amxd_object_get_path(object, AMXD_OBJECT_NAMED);
    amxd_object_t* categFObj = amxd_object_get_parent(amxd_object_get_parent(object));
    const char* categF = amxd_object_get_name(categFObj, AMXD_OBJECT_NAMED);
    amxd_object_t* apObj = amxd_object_get_parent(categFObj);
    const char* apName = amxd_object_get_name(apObj, AMXD_OBJECT_NAMED);
    SAH_TRACEZ_INFO(ME, "%s: destroy instance %s", apName, path);
    free(path);
    object->priv = NULL;
    if(swl_str_matches(categF, "MACFiltering")) {
        s_syncMACFiltering(apObj);
    } else if(swl_str_matches(categF, "ProbeFiltering")) {
        s_syncProbeFiltering(apObj);
    }

    SAH_TRACEZ_OUT(ME);
    return status;
}

static void s_setMACFilteringEntry_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);
    char* path = amxd_object_get_path(object, AMXD_OBJECT_NAMED);
    amxd_object_t* categFObj = amxd_object_get_parent(amxd_object_get_parent(object));
    const char* categF = amxd_object_get_name(categFObj, AMXD_OBJECT_NAMED);
    amxd_object_t* apObj = amxd_object_get_parent(categFObj);
    const char* apName = amxd_object_get_name(apObj, AMXD_OBJECT_NAMED);
    const char* pname = amxd_param_get_name(param);
    char* pvalue = swl_typeCharPtr_fromVariantDef((amxc_var_t*) newValue, NULL);
    SAH_TRACEZ_INFO(ME, "%s: %s.%s = %s", apName, path, pname, pvalue);
    free(pvalue);
    free(path);
    object->priv = object;
    if(swl_str_matches(categF, "MACFiltering")) {
        s_syncMACFiltering(apObj);
    } else if(swl_str_matches(categF, "ProbeFiltering")) {
        s_syncProbeFiltering(apObj);
    }

    SAH_TRACEZ_OUT(ME);
}

SWLA_DM_HDLRS(sApMfEntryDmHdlrs, ARR(SWLA_DM_PARAM_HDLR("MACAddress", s_setMACFilteringEntry_pwf)));

void _wld_apMacFilterEntry_setConf_ocf(const char* const sig_name,
                                       const amxc_var_t* const data,
                                       void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sApMfEntryDmHdlrs, sig_name, data, priv);
}

static void s_deleteAllEntries(T_AccessPoint* pAP, char* listname) {
    ASSERTS_NOT_NULL(pAP, , ME, "NULL");
    ASSERTS_NOT_NULL(listname, , ME, "NULL");
    amxd_object_t* mfObject = amxd_object_get(pAP->pBus, "MACFiltering");
    ASSERTS_NOT_NULL(mfObject, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: Deleting all %s entries", pAP->alias, listname);
    pAP->MF_AddressListBlockSync = true;
    amxd_object_t* templateObj = amxd_object_get(mfObject, listname);
    if(amxd_object_get_instance_count(templateObj) > 0) {
        amxd_trans_t trans;
        swl_object_prepareTransaction(&trans, templateObj);
        amxd_object_for_each(instance, it, templateObj) {
            amxd_object_t* mfentry = amxc_container_of(it, amxd_object_t, it);
            amxd_trans_del_inst(&trans, amxd_object_get_index(mfentry), NULL);
        }
        swl_object_finalizeTransactionOnLocalDm(&trans);
    }
    pAP->MF_AddressListBlockSync = false;
}

static bool s_addEntryObject(T_AccessPoint* pAP,
                             const char* objName,
                             const char* listName,
                             const char* macStr) {
    amxd_object_t* mfObject = amxd_object_get(pAP->pBus, objName);
    ASSERTS_NOT_NULL(mfObject, false, ME, "NULL");
    amxd_object_t* entry = amxd_object_get(mfObject, listName);
    ASSERTS_NOT_NULL(entry, false, ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: adding sta %s to %s.%s", pAP->alias, macStr, objName, listName);
    amxd_trans_t trans;
    ASSERT_TRANSACTION_INIT(entry, &trans, false, ME, "%s: trans init failure", pAP->alias);
    amxd_trans_add_inst(&trans, 0, NULL);
    amxd_trans_set_value(cstring_t, &trans, "MACAddress", macStr);
    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, false, ME, "%s : trans apply failure", pAP->alias);
    SAH_TRACEZ_INFO(ME, "%s: Added %s entry %s", pAP->alias, listName, macStr);
    trans.current->priv = trans.current;
    return true;
}

static amxd_object_t* s_fetchEntryExt(amxd_object_t* templateObj, const char* macStr, amxd_object_t* optExclObj, size_t* pLastPos) {
    amxd_object_for_each(instance, it, templateObj) {
        amxd_object_t* mfEntry = amxc_container_of(it, amxd_object_t, it);
        char* entryStr = amxd_object_get_cstring_t(mfEntry, "MACAddress", NULL);
        if(pLastPos) {
            *pLastPos += (!swl_str_isEmpty(entryStr));
        }
        if(mfEntry == optExclObj) {
            free(entryStr);
            continue;
        }
        bool match = (!swl_str_isEmpty(entryStr) && SWL_MAC_CHAR_MATCHES(entryStr, macStr));
        free(entryStr);
        if(match) {
            return mfEntry;
        }
    }
    return NULL;
}

static amxd_object_t* s_fetchEntry(T_AccessPoint* pAP, const char* categF, const char* listname, const char* macStr) {
    return s_fetchEntryExt(amxd_object_findf(pAP->pBus, "%s.%s", categF, listname), macStr, NULL, NULL);
}

static bool s_doesEntryExist(T_AccessPoint* pAP, const char* categF, const char* listname, const char* macStr) {
    return (s_fetchEntry(pAP, categF, listname, macStr) != NULL);
}

static void s_deleteUnusedEntries(T_AccessPoint* pAP, char* listname) {
    amxd_object_t* mfObject = amxd_object_get(pAP->pBus, "MACFiltering");
    ASSERTS_NOT_NULL(mfObject, , ME, "NULL");
    uint32_t nMatch = 0;
    amxd_trans_t trans;
    amxd_object_t* templateObj = amxd_object_get(mfObject, listname);
    amxd_object_for_each(instance, it, templateObj) {
        amxd_object_t* mfEntry = amxc_container_of(it, amxd_object_t, it);
        char* entryMac = amxd_object_get_cstring_t(mfEntry, "MACAddress", NULL);
        if((swl_str_isEmpty(entryMac)) ||
           ((pAP->MF_AddressList != NULL) && (strcasestr(pAP->MF_AddressList, entryMac) == NULL))) {
            SAH_TRACEZ_INFO(ME, "%s: Deleting MF %s entry (%s)", pAP->alias, listname, entryMac);
            nMatch++;
            if(nMatch == 1) {
                swl_object_prepareTransaction(&trans, templateObj);
            }
            amxd_trans_del_inst(&trans, amxd_object_get_index(mfEntry), NULL);
        }
        free(entryMac);
    }
    if(nMatch > 0) {
        swl_object_finalizeTransactionOnLocalDm(&trans);
    }
}

static void s_syncAddressList(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP->MF_AddressList, , ME, "NULL");
    pAP->MF_AddressListBlockSync = true;
    char* allowed_str_list = strdup(pAP->MF_AddressList);
    char* allowed_str = allowed_str_list;
    if(allowed_str == NULL) {
        pAP->MF_AddressListBlockSync = false;
        SAH_TRACEZ_ERROR(ME, "allowed_str - M`emory allocation is failure");
        return; // Handle memory allocation failure gracefully
    }
    while(allowed_str != NULL) {
        char* macStr = strsep(&allowed_str, ",");
        //skip empty fields
        if(swl_str_isEmpty(macStr)) {
            continue;
        }
        if(!s_doesEntryExist(pAP, "MACFiltering", "Entry", macStr)) {
            s_addEntryObject(pAP, "MACFiltering", "Entry", macStr);
        }
    }
    free(allowed_str_list);
    s_deleteUnusedEntries(pAP, "Entry");
    pAP->MF_AddressListBlockSync = false;
}

static void s_cleanupMACFilterAddressList(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, , ME, "NULL");
    s_stopMfSyncTimer(pAP);
    W_SWL_FREE(pAP->MF_AddressList);
    s_deleteAllEntries(pAP, "Entry");
}

amxd_status_t _wld_ap_validateMfMac_pvf(amxd_object_t* object,
                                        amxd_param_t* param,
                                        amxd_action_t reason _UNUSED,
                                        const amxc_var_t* const args,
                                        amxc_var_t* const retval _UNUSED,
                                        void* priv _UNUSED) {
    ASSERTS_FALSE(amxc_var_is_null(args), amxd_status_invalid_value, ME, "invalid");
    ASSERTS_EQUALS(amxd_object_get_type(object), amxd_object_instance, amxd_status_ok, ME, "obj is not instance");
    SAH_TRACEZ_IN(ME);

    amxd_status_t status = amxd_status_invalid_value;
    amxd_object_t* tmplObj = amxd_object_get_parent(object);
    const char* tmplName = amxd_object_get_name(tmplObj, AMXD_OBJECT_NAMED);

    const char* currMacStr = amxc_var_constcast(cstring_t, &param->value);
    const char* newMacStr = amxc_var_constcast(cstring_t, args);
    if(swl_str_isEmpty(newMacStr) || SWL_MAC_CHAR_MATCHES(newMacStr, currMacStr)) {
        return amxd_status_ok;
    }
    size_t count = 0;
    amxd_object_t* duplMfEntry = s_fetchEntryExt(tmplObj, newMacStr, object, &count);
    if(duplMfEntry != NULL) {
        SAH_TRACEZ_ERROR(ME, "%s entry pos(%zu) mac (%s) already exist", tmplName, count, newMacStr);
    } else if((count >= MAXNROF_MFENTRY) && (!swl_str_isEmpty(newMacStr)) && (swl_str_isEmpty(currMacStr))) {
        //New mac value (not empty), set to current empty entry (ie new filter data):
        //=> need to check internal storing max count
        SAH_TRACEZ_ERROR(ME, "%s max entries reached %zu", tmplName, count);
    } else {
        status = amxd_status_ok;
    }

    SAH_TRACEZ_OUT(ME);
    return status;
}

amxd_status_t _wld_ap_validateAddressList_pvf(amxd_object_t* object,
                                              amxd_param_t* param,
                                              amxd_action_t reason _UNUSED,
                                              const amxc_var_t* const args,
                                              amxc_var_t* const retval _UNUSED,
                                              void* priv _UNUSED) {
    ASSERTS_FALSE(amxc_var_is_null(args), amxd_status_invalid_value, ME, "invalid");
    ASSERTS_EQUALS(amxd_object_get_type(object), amxd_object_instance, amxd_status_ok, ME, "obj is not instance");
    SAH_TRACEZ_IN(ME);

    amxd_status_t status = amxd_status_invalid_value;
    const char* oname = amxd_object_get_name(object, AMXD_OBJECT_NAMED);
    const char* currentValue = amxc_var_constcast(cstring_t, &param->value);
    ASSERT_NOT_NULL(currentValue, status, ME, "NULL");
    bool validF = false;
    const char* newValue = amxc_var_constcast(cstring_t, args);
    ASSERT_NOT_NULL(newValue, status, ME, "NULL");
    if(swl_str_matchesIgnoreCase(currentValue, newValue) || (swl_str_isEmpty(newValue))) {
        validF = true;
    } else {
        swl_macChar_t mac = SWL_MAC_CHAR_NEW();
        char strBuf[swl_str_len(newValue) + 1];
        swl_str_copy(strBuf, sizeof(strBuf), newValue);
        uint32_t index = 0;
        for(char* strPtr = strBuf; strPtr != NULL; index++) {
            char* field = strsep(&strPtr, ",");
            validF = (swl_typeMacChar_fromChar(&mac, field) && swl_mac_charIsValidStaMac(&mac));
            if(!validF) {
                SAH_TRACEZ_ERROR(ME, "%s: invalid address field[%d](%s)", oname, index, field);
                break;
            }
        }
    }

    if(validF) {
        status = amxd_status_ok;
    } else {
        SAH_TRACEZ_ERROR(ME, "%s: invalid address list (%s)", oname, newValue);
    }

    SAH_TRACEZ_OUT(ME);
    return status;
}

void wld_apMacFilter_setAddressList_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(object);
    ASSERT_NOT_NULL(pAP, , ME, "Invalid AP Ctx");

    char* newMACFilterAddressList = amxc_var_dyncast(cstring_t, newValue);
    ASSERT_NOT_NULL(newMACFilterAddressList, , ME, "NULL");
    if(swl_str_matches(pAP->MF_AddressList, newMACFilterAddressList)) {
        SAH_TRACEZ_INFO(ME, "%s: Same MACFilterAddressList", pAP->alias);
    } else if(swl_str_isEmpty(newMACFilterAddressList)) {
        s_cleanupMACFilterAddressList(pAP);
    } else {
        swl_str_copyMalloc(&pAP->MF_AddressList, newMACFilterAddressList);
        SAH_TRACEZ_INFO(ME, "%s: Set new MF_AddressList %s", pAP->alias, pAP->MF_AddressList);
        /* delay syncing mf addressList string with mf obj entries list
         * to give time for nemo to push its entries to plugin side.
         * This avoid duplicating MF entries.
         * Start the unified timer
         */
        s_startMfSyncTimer(pAP, true);
    }
    free(newMACFilterAddressList);

    SAH_TRACEZ_OUT(ME);
}

static bool addEntry(
    T_AccessPoint* pAP,
    const char* objName,
    amxc_var_t* args,
    const char* listname) {

    ASSERT_NOT_NULL(pAP, false, ME, "NULL");
    ASSERT_NOT_NULL(objName, false, ME, "NULL");

    const char* macStr = GET_CHAR(args, "mac");
    swl_macBin_t macBin;
    if(!swl_typeMacBin_fromChar(&macBin, macStr)) {
        SAH_TRACEZ_ERROR(ME, "wrong mac %s", macStr);
        return false;
    }

    if(s_doesEntryExist(pAP, objName, listname, macStr)) {
        SAH_TRACEZ_INFO(ME, "%s: %s.%s Mac exists %s", pAP->alias, objName, listname, macStr);
        return true;
    }

    /* Create the data model object */
    return s_addEntryObject(pAP, objName, listname, macStr);
}

static bool delEntry(
    T_AccessPoint* pAP,
    amxd_object_t* obj _UNUSED,
    amxc_var_t* args,
    const char* listname) {

    ASSERT_NOT_NULL(pAP, false, ME, "NULL");

    const char* macStr = GET_CHAR(args, "mac");
    swl_macBin_t macBin;
    if(!swl_typeMacBin_fromChar(&macBin, macStr)) {
        SAH_TRACEZ_ERROR(ME, "wrong mac %s", macStr);
        return false;
    }

    const char* categF = amxd_object_get_name(obj, 0);
    amxd_object_t* mfEntry = s_fetchEntry(pAP, categF, listname, macStr);
    ASSERTI_NOT_NULL(mfEntry, true, ME, "MAC address %s not found in the %s entries.", macStr, listname);

    /* Delete the object entry */
    return (swl_object_delInstWithTransOnLocalDm(mfEntry) == amxd_status_ok);
}

amxd_status_t _MACFiltering_addEntry(amxd_object_t* obj,
                                     amxd_function_t* func _UNUSED,
                                     amxc_var_t* args,
                                     amxc_var_t* ret _UNUSED) {
    SAH_TRACEZ_IN(ME);
    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(obj));
    ASSERT_NOT_NULL(pAP, amxd_status_unknown_error, ME, "NULL");

    if(!addEntry(pAP, amxd_object_get_name(obj, 0), args, "Entry")) {
        SAH_TRACEZ_ERROR(ME, "Failed to add Entry");
        SAH_TRACEZ_OUT(ME);
        return amxd_status_unknown_error;
    }
    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _MACFiltering_addTempEntry(amxd_object_t* obj,
                                         amxd_function_t* func _UNUSED,
                                         amxc_var_t* args,
                                         amxc_var_t* ret _UNUSED) {

    SAH_TRACEZ_IN(ME);
    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(obj));
    ASSERT_NOT_NULL(pAP, amxd_status_unknown_error, ME, "NULL");

    if(!addEntry(pAP, amxd_object_get_name(obj, 0), args, "TempEntry")) {
        SAH_TRACEZ_ERROR(ME, "Failed to add TempEntry");
        SAH_TRACEZ_OUT(ME);
        return amxd_status_unknown_error;
    }
    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _MACFiltering_delEntry(amxd_object_t* obj,
                                     amxd_function_t* func _UNUSED,
                                     amxc_var_t* args,
                                     amxc_var_t* ret _UNUSED) {

    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(obj));
    ASSERT_NOT_NULL(pAP, amxd_status_unknown_error, ME, "NULL");

    if(!delEntry(pAP, obj, args, "Entry")) {
        SAH_TRACEZ_ERROR(ME, "Failed to delete Entry");
        SAH_TRACEZ_OUT(ME);
        return amxd_status_unknown_error;
    }
    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _MACFiltering_delTempEntry(amxd_object_t* obj,
                                         amxd_function_t* func _UNUSED,
                                         amxc_var_t* args,
                                         amxc_var_t* ret _UNUSED) {

    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(obj));
    ASSERT_NOT_NULL(pAP, amxd_status_unknown_error, ME, "NULL");

    if(!delEntry(pAP, obj, args, "TempEntry")) {
        SAH_TRACEZ_ERROR(ME, "Failed to delete TempEntry");
        SAH_TRACEZ_OUT(ME);
        return amxd_status_unknown_error;
    }
    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _ProbeFiltering_addTempEntry(amxd_object_t* obj,
                                           amxd_function_t* func _UNUSED,
                                           amxc_var_t* args,
                                           amxc_var_t* ret _UNUSED) {

    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(obj));
    ASSERT_NOT_NULL(pAP, amxd_status_unknown_error, ME, "NULL");

    if(!addEntry(pAP, amxd_object_get_name(obj, 0), args, "TempEntry")) {
        SAH_TRACEZ_ERROR(ME, "Failed to add TempEntry");
        SAH_TRACEZ_OUT(ME);
        return amxd_status_unknown_error;
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

amxd_status_t _ProbeFiltering_delTempEntry(amxd_object_t* obj,
                                           amxd_function_t* func _UNUSED,
                                           amxc_var_t* args,
                                           amxc_var_t* ret _UNUSED) {

    SAH_TRACEZ_IN(ME);

    T_AccessPoint* pAP = wld_ap_fromObj(amxd_object_get_parent(obj));

    if(!delEntry(pAP, obj, args, "TempEntry")) {
        SAH_TRACEZ_ERROR(ME, "Failed to delete TempEntry");
        SAH_TRACEZ_OUT(ME);
        return amxd_status_unknown_error;
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

static void s_addMfEntry(wld_banlist_t* banlist, unsigned char* mac) {
    if(banlist->staToBan >= WLD_MAC_MAXBAN) {
        return;
    }
    memcpy(&banlist->banList[banlist->staToBan], mac, SWL_MAC_BIN_LEN);
    banlist->staToBan++;
}

static void s_addKickEntry(wld_banlist_t* banlist, unsigned char* mac) {
    if(banlist->staToKick >= WLD_MAC_MAXBAN) {
        return;
    }
    memcpy(&banlist->kickList[banlist->staToKick], mac, SWL_MAC_BIN_LEN);
    banlist->staToKick++;
}

void wld_apMacFilter_getBanList(T_AccessPoint* vap, wld_banlist_t* banlist, bool includePf) {
    ASSERT_NOT_NULL(vap, , ME, "NULL");
    ASSERT_NOT_NULL(banlist, , ME, "NULL");
    banlist->staToBan = 0;
    banlist->staToKick = 0;
    SAH_TRACEZ_INFO(ME, "%s get data %u mode %u/%u tmp %u/%u pf %u", vap->alias, includePf,
                    vap->MF_Mode, vap->MF_EntryCount,
                    vap->MF_TempBlacklistEnable, vap->MF_TempEntryCount,
                    vap->PF_TempEntryCount);

    if(vap->MF_Mode == APMFM_WHITELIST) {
        for(int i = 0; i < vap->MF_EntryCount; i++) {
            if((!swl_typeMacBin_arrayContainsRef((swl_macBin_t*) vap->PF_Temp_Entry, vap->PF_TempEntryCount, (swl_macBin_t*) vap->MF_Entry[i])) &&
               ((!vap->MF_TempBlacklistEnable) ||
                (!swl_typeMacBin_arrayContainsRef((swl_macBin_t*) vap->MF_Temp_Entry, vap->MF_TempEntryCount, (swl_macBin_t*) vap->MF_Entry[i])))) {
                // add entry either if temporary blacklisting is disabled, or entry not in temp blaclist
                s_addMfEntry(banlist, vap->MF_Entry[i]);
            }
        }


        //Before setting whitelist, kick all stations not whitelisted
        for(int i = 0; i < vap->AssociatedDeviceNumberOfEntries; i++) {
            if(vap->AssociatedDevice[i]->Active
               && !swl_typeMacBin_arrayContainsRef(banlist->banList, banlist->staToBan, (swl_macBin_t*) vap->AssociatedDevice[i]->MACAddress)
               && !swl_typeMacBin_arrayContainsRef((swl_macBin_t*) vap->PF_Temp_Entry, vap->PF_TempEntryCount, (swl_macBin_t*) vap->AssociatedDevice[i]->MACAddress)) {
                s_addKickEntry(banlist, vap->AssociatedDevice[i]->MACAddress);
            }
        }
    } else if((vap->MF_Mode == APMFM_BLACKLIST)
              || (vap->MF_TempBlacklistEnable && (vap->MF_TempEntryCount > 0))
              || (includePf && (vap->PF_TempEntryCount > 0))) {
        if(vap->MF_Mode == APMFM_BLACKLIST) {
            for(int i = 0; i < vap->MF_EntryCount; i++) {
                s_addMfEntry(banlist, vap->MF_Entry[i]);
                T_AssociatedDevice* assocDev = wld_vap_get_existing_station(vap, (swl_macBin_t*) vap->MF_Entry[i]);
                if(( assocDev != NULL) && assocDev->Active) {
                    s_addKickEntry(banlist, assocDev->MACAddress);
                }
            }
        }
        if(vap->MF_TempBlacklistEnable) {
            for(int i = 0; i < vap->MF_TempEntryCount; i++) {
                s_addMfEntry(banlist, vap->MF_Temp_Entry[i]);

                T_AssociatedDevice* assocDev = wld_vap_get_existing_station(vap, (swl_macBin_t*) vap->MF_Temp_Entry[i]);
                if(( assocDev != NULL) && assocDev->Active) {
                    s_addKickEntry(banlist, assocDev->MACAddress);
                }
            }
        }
        if(includePf) {
            for(int i = 0; i < vap->PF_TempEntryCount; i++) {
                s_addMfEntry(banlist, vap->PF_Temp_Entry[i]);
                //don't kick probe filter entries.
            }
        }
    } else {
        SAH_TRACEZ_INFO(ME, "%s nothing to add", vap->alias);
    }
    SAH_TRACEZ_INFO(ME, "%s done add %u %u", vap->alias, banlist->staToBan, banlist->staToKick);
}

