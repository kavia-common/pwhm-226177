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

#include "wld_nl80211.h"
#include "wld_ssid_nl80211_priv.h"
#include "wld_nl80211_types.h"

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

/**
 * @brief Notify when an MLD event occurs.
 *
 * Handles creation of DM objects or other actions based on MLD events
 * (e.g. when the first SSID is linked to an MLD).
 *
 * @param pMld   Pointer to the MLD object.
 * @param event  Event type (e.g. add/remove).
 * @param reason Reason string for the event.
 */
void wld_apMld_notifyChange(wld_mld_t* pMld, wld_mldChangeEvent_e event, const char* reason) {

    ASSERTS_NOT_NULL(pMld, , ME, "pMld is NULL in Notify Change");
    SAH_TRACEZ_INFO(ME, "MLD Event %d for unit %d - Reason: %s",
                    event, pMld->unit, reason);

    switch(event) {
    case WLD_MLD_EVT_ADD:
        amxd_object_t* Obj = wld_apMld_getOrCreateDmObject(pMld->unit, pMld->pGroup->type, pMld);
        if(Obj == NULL) {
            SAH_TRACEZ_ERROR(ME, "Failed to create/get DM object for MLD unit %d", pMld->unit);
        }
        break;

    default:
        SAH_TRACEZ_INFO(ME, "Unhandled MLD event: %d", event);
        break;
    }
}

/**
 * @brief Get or create the DM object for an APMLD instance.
 *
 * Looks up an APMLD.{i} object in the DM by unit ID. If not found,
 * creates a new instance.
 *
 * @param mld_unit     MLD unit ID.
 * @param mld_type     SSID type (must be AP).
 * @param pMld_internal Pointer to the internal MLD structure.
 *
 * @return Pointer to the DM object, or NULL on failure.
 */
amxd_object_t* wld_apMld_getOrCreateDmObject(uint32_t mld_unit, wld_ssidType_e mld_type, wld_mld_t* pMld_internal) {
    if(mld_type != WLD_SSID_TYPE_AP) {
        SAH_TRACEZ_ERROR(ME, "Expected AP SSID type for APMLD");
        return NULL;
    }

    amxd_object_t* object = NULL;
    amxd_object_t* rootObj = get_wld_object();
    ASSERT_NOT_NULL(rootObj, NULL, ME, "Failed to get root object");
    amxd_object_t* templateObject = amxd_object_get(rootObj, "APMLD");
    uint32_t instCount = amxd_object_get_instance_count(templateObject);
    SAH_TRACEZ_INFO(ME, "APMLD instance count is %d and mldunit is %d", instCount, mld_unit);

    // ---- Iterate all existing instances ----
    for(uint32_t i = 1; i <= instCount; i++) {
        amxd_object_t* inst = amxd_object_get_instance(templateObject, NULL, i);
        if(inst == NULL) {
            SAH_TRACEZ_INFO(ME, "APMLD instance is NULL");
            continue;
        }

        uint32_t mld_id = 0;
        mld_id = amxd_object_get_uint32_t(inst, "MLDID", NULL);
        if(mld_id == mld_unit) {
            object = inst;
            SAH_TRACEZ_INFO(ME, "Found existing APMLD instance for MLDID=%u", mld_unit);
            break;
        }
    }


    if(object == NULL) {
        SAH_TRACEZ_INFO(ME, "APMLD instance not found. Proceeding with creation");
        amxd_trans_t trans;
        ASSERT_TRANSACTION_INIT(templateObject, &trans, NULL, ME, "%s: Failed to init transaction for APMLD %u creation", ME, mld_unit);
        amxd_trans_add_inst(&trans, 0, NULL);
        ASSERT_TRANSACTION_LOCAL_DM_END(&trans, NULL, ME, "%s: Failed to apply transaction for APMLD %u creation", ME, mld_unit);
        instCount = amxd_object_get_instance_count(templateObject);
        object = amxd_object_get_instance(templateObject, NULL, instCount);
        ASSERT_NOT_NULL(object, NULL, ME, "%s: Failed to retrieve newly created APMLD instance %u after transaction.", ME, mld_unit);
        SAH_TRACEZ_INFO(ME, "New APMLD instance created for MLDUnit %u", mld_unit);
    }
    pMld_internal->object = object;
    pMld_internal->unit = (uint8_t) mld_unit;
    object->priv = pMld_internal;
    return object;
}

/**
 * @brief Create AffiliatedAP objects for a given MLD link.
 *
 * This function creates one or more AffiliatedAP.{i} objects in the
 * Data Model (DM) for the provided MLD link. If an existing AffiliatedAP
 * object already exists for the link, all existing instances from that
 * link onward are deleted before creating new instances.
 *
 * @param pLink      Pointer to the MLD link for which AffiliatedAP objects
 *                   need to be created. Must not be NULL.
 * @param dm_instance The DM instance ID to use as the starting point for
 *                   creating the AffiliatedAP objects.
 *
 * @return Pointer to the newly created AffiliatedAP DM object if successful,
 *         NULL if creation failed.
 *
 * @note The function assumes that the parent MLD object (`pLink->pMld->object`)
 *       is already initialized. Failure to have a valid parent object will
 *       result in returning NULL.
 *
 * @note Updates the `AffObj` pointer in the provided `pLink` to the newly
 *       created DM object.
 */
amxd_object_t* wld_apMld_createAffiliatedAPObject(wld_mldLink_t* pLink, uint32_t dm_instance) {

    ASSERTS_NOT_NULL(pLink, NULL, ME, "pLink is NULL can't create Affiliated AP");
    uint32_t dm_instance_id = 0;
    dm_instance_id = dm_instance + 1;
    wld_mldLink_t* pTempLink = pLink;
    amxd_object_t* CurrObj = pLink->AffObj;
    if(CurrObj) {
        SAH_TRACEZ_INFO(ME, "linkId (%d): current Affiliated object exists so deleting all the instances below", dm_instance);
        wld_apMld_deleteAffiliatedAPObjects(pTempLink);
    } else {
        SAH_TRACEZ_INFO(ME, "linkId (%d): current Affiliated object don't exists", dm_instance);
    }
    SAH_TRACEZ_INFO(ME, "linkId (%d): calling create affiliated ap %u", dm_instance, pLink->configured);

    amxd_object_t* parent_obj = pLink->pMld->object;
    if(parent_obj == NULL) {
        SAH_TRACEZ_ERROR(ME, "pMld->object is NULL, cannot create AffiliatedAP instances");
        return NULL;
    }

    amxd_object_t* templateObject = amxd_object_get(parent_obj, "AffiliatedAP");
    ASSERT_NOT_NULL(templateObject, NULL, ME, "%s: Could not get template", pLink->pSSID->Name);
    SAH_TRACEZ_INFO(ME, "%s: Creating template object for ssid", pLink->pSSID->Name);
    amxd_trans_t trans;
    ASSERT_TRANSACTION_INIT(templateObject, &trans, NULL, ME, "%s : trans init failure", pLink->pSSID->Name);

    amxd_trans_add_inst(&trans, dm_instance_id, NULL);

    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, NULL, ME, "%s : trans apply failure", pLink->pSSID->Name);

    pLink->AffObj = amxd_object_get_instance(templateObject, NULL, dm_instance_id);
    ASSERT_NOT_NULL(pLink->AffObj, NULL, ME, "%s: failure to create object", pLink->pSSID->Name);
    pLink->AffObj->priv = pLink;
    pTempLink = NULL;
    return pLink->AffObj;

}

/**
 * @brief Delete AffiliatedAP objects starting from a given MLD link.
 *
 * This function deletes all AffiliatedAP.{i} objects in the Data Model (DM)
 * starting from the specified link up to the end of the MLD's link list.
 * Each deleted object's private pointer (`priv`) is cleared before deletion.
 *
 * @param pStartLink Pointer to the first MLD link from which AffiliatedAP
 *                   objects should be deleted. Must not be NULL and must
 *                   have a valid parent MLD object (`pStartLink->pMld`).
 *
 * @return Status code of the operation:
 *         - amxd_status_ok if all deletions succeeded.
 *         - amxd_status_unknown_error if deletion failed due to invalid
 *           input or DM transaction failure.
 *
 * @note If the parent MLD object (`pStartLink->pMld->object`) is NULL,
 *       the function logs an error and returns amxd_status_unknown_error.
 * @note The function iterates from the provided link to the end of the list,
 *       deleting each AffiliatedAP DM instance and updating the `linkId` and
 *       `AffObj` fields of each link accordingly.
 */
amxd_status_t wld_apMld_deleteAffiliatedAPObjects(wld_mldLink_t* pStartLink) {
    if((pStartLink == NULL) || (pStartLink->pMld == NULL)) {
        SAH_TRACEZ_ERROR(ME, "Invalid start link or parent MLD");
        return amxd_status_unknown_error;
    }

    amxd_status_t status = amxd_status_ok;
    amxc_llist_it_t* it = &pStartLink->it;      // starting point in the list
    amxc_llist_it_t* next_it = NULL;

    // Get template once from the parent MLD
    amxd_object_t* parent_obj = pStartLink->pMld->object;
    if(parent_obj == NULL) {
        SAH_TRACEZ_ERROR(ME, "pMld->object is NULL, cannot retrieve AffiliatedAP instances");
        return amxd_status_unknown_error;
    }

    amxd_object_t* ObjTempl = amxd_object_get(parent_obj, "AffiliatedAP");
    if(ObjTempl == NULL) {
        SAH_TRACEZ_ERROR(ME, "Failed to get AffiliatedAP template from parent object");
        return amxd_status_unknown_error;
    }

    // Loop from current link till end of list
    while(it != NULL) {
        wld_mldLink_t* pLink = amxc_container_of(it, wld_mldLink_t, it);
        next_it = amxc_llist_it_get_next(it);          // store next iterator in advance

        if(pLink->AffObj != NULL) {
            uint32_t instance_id = amxd_object_get_index(pLink->AffObj);
            SAH_TRACEZ_INFO(ME, "Deleting DM instance %u for %s", instance_id, pLink->pSSID->Name);
            ASSERT_TRUE(instance_id > 0, amxd_status_unknown_error, ME, "wrong instance index");

            // Clear private pointer before applying
            pLink->AffObj->priv = NULL;

            amxd_status_t status = swl_object_delInstWithTransOnLocalDm(pLink->AffObj);

            if(status != amxd_status_ok) {
                SAH_TRACEZ_ERROR(ME, "Failed to delete AffiliatedAP instance %u for %s", instance_id, pLink->pSSID->Name);
                pLink->AffObj->priv = pLink;
                return amxd_status_unknown_error;
            }

            pLink->AffObj = NULL;
            SAH_TRACEZ_INFO(ME, "DM instance AffiliatedAP.%u deleted successfully", instance_id);

        } else {
            SAH_TRACEZ_INFO(ME, "MLD unit %d: Link ID %d: No DM object found to delete", pLink->pMld->unit, pLink->linkId);
        }

        it = next_it;          // move to next link
    }

    return status;
}

/**
 * @brief Clear runtime fields stored in an APMLD DM instance.
 *
 * Clears runtime information kept inside the APMLD object referenced by
 * pMld_internal while keeping the APMLD instance itself (persistent
 * configuration) intact. Typical use-case: the last SSID was unlinked
 * from an MLD and the runtime values must be sanitized.
 *
 * Actions performed:
 *  - sets MLDMACAddress to the zero MAC string "00:00:00:00:00:00",
 *  - updates AffiliatedAPNumberOfEntries to the current number of links,
 *  - logs the performed operations.
 *
 * @param pMld_internal Pointer to the internal MLD structure whose DM fields will be cleared.
 *
 * @return amxd_status_ok on success, amxd_status_unknown_error on failure.
 */
amxd_status_t wld_apMld_clearMld(wld_mld_t* pMld_internal) {

    ASSERTS_NOT_NULL(pMld_internal, amxd_status_object_not_found, ME, "pMld is NULL nothing to clear");
    ASSERTS_NOT_NULL(pMld_internal->object, amxd_status_object_not_found, ME, "pMld->object is NULL nothing to clear from DM");
    uint32_t instance_id = amxd_object_get_index(pMld_internal->object);
    wld_mld_t* pMld = pMld_internal;
    amxd_object_t* obj = pMld->object;
    const char* mldMacStr = "00:00:00:00:00:00";
    uint32_t numLinks = amxc_llist_size(&pMld->links);
    SAH_TRACEZ_INFO(ME, "Clearing DMs inside APMLD instance %u", instance_id);

    if(amxd_object_set_cstring_t(obj, "MLDMACAddress", mldMacStr) != amxd_status_ok) {
        SAH_TRACEZ_ERROR(ME, "Failed to clear MLDMACAddress in DM for unit %u", pMld->unit);
    } else {
        SAH_TRACEZ_INFO(ME, "Cleared MLDMACAddress=%s for APMLD unit %u", mldMacStr, pMld->unit);
    }
    if(amxd_object_set_uint32_t(obj, "AffiliatedAPNumberOfEntries", numLinks) != amxd_status_ok) {
        SAH_TRACEZ_ERROR(ME, "Failed to set AffiliatedAPNumberOfEntries=%u", numLinks);
    } else {
        SAH_TRACEZ_INFO(ME, "Updated AffiliatedAPNumberOfEntries=%u for APMLD unit %u", numLinks, pMld->unit);
    }

    SAH_TRACEZ_INFO(ME, "Cleared DMs inside APMLD instance %u", instance_id);
    return amxd_status_ok;
}

/**
 * @brief Update AffiliatedAP MLO statistics parameters in the Data Model.
 *
 * This helper function sets the Packets, Bytes, and Error counters
 * under the given AffiliatedAP object based on the provided MLO stats.
 *
 * @param obj   AffiliatedAP object in the Data Model.
 * @param stats Pointer to the collected MLO statistics structure.
 */
static void s_update_affAP_MloStats(amxd_object_t* const obj, wld_mloStats_t* stats) {
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "PacketsSent", stats->txPackets);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "PacketsReceived", stats->rxPackets);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "UnicastBytesSent", stats->txUbyte);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "UnicastBytesReceived", stats->rxUbyte);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "ErrorsSent", stats->txEbyte);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "MulticastBytesSent", stats->txMbyte);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "MulticastBytesReceived", stats->rxMbyte);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "BroadcastBytesSent", stats->txBbyte);
    SWLA_OBJECT_SET_PARAM_UINT32(obj, "BroadcastBytesReceived", stats->rxBbyte);
}

/**
 * @brief ORF callback to retrieve MLO statistics for an AffiliatedAP object.
 *
 * This function is invoked when reading parameters under
 * Device.WiFi.APMLD.{i}.AffiliatedAP.{i}. It collects per-link MLO
 * statistics via the driver hook and updates the DM object.
 *
 * @param object         Target AffiliatedAP object being read.
 * @param param          DM parameter being accessed.
 * @param reason         Action reason (must be read).
 * @param args           Optional input arguments.
 * @param action_retval  Action return value.
 * @param priv           Private context (unused).
 *
 * @return amxd_status_t
 *         - amxd_status_ok if handled successfully
 *         - amxd_status_function_not_implemented if not a read action
 *         - amxd_status_object_not_found if the parent link is missing
 */
amxd_status_t _wld_affAP_getMloStats_orf(amxd_object_t* const object,
                                         amxd_param_t* const param,
                                         amxd_action_t reason,
                                         const amxc_var_t* const args,
                                         amxc_var_t* const action_retval,
                                         void* priv) {
    SAH_TRACEZ_IN(ME);
    if(reason != action_object_read) {
        return amxd_status_function_not_implemented;
    }
    wld_mldLink_t* pLink = (wld_mldLink_t*) amxd_object_get_parent(object)->priv;
    ASSERT_NOT_NULL(pLink, amxd_status_object_not_found, ME, "AffiliatedAP object not found!");
    wld_mloStats_t mloStats;
    T_SSID* pSSID = pLink->pSSID;
    T_AccessPoint* pAP = pSSID->AP_HOOK;
    memset(&mloStats, 0, sizeof(wld_mloStats_t));
    if(pAP->pFA->mfn_wvap_getMloStats(pAP, &mloStats) >= SWL_RC_OK) {
        s_update_affAP_MloStats(object, &mloStats);
    }
    return amxd_action_object_read(object, param, reason, args, action_retval, priv);
}

/**
 * @brief Retrieve the MLD instance from its Data Model object.
 *
 * Ensures the object hierarchy is correct and safely returns the
 * internal MLD structure associated with the given DM object.
 *
 * @param mldObj   MLD DM object instance.
 * @return Pointer to the MLD structure, or NULL if validation fails.
 */
wld_mld_t* wld_apMld_getMldfromObj(amxd_object_t* mldObj) {
    ASSERTS_EQUALS(amxd_object_get_type(mldObj), amxd_object_instance, NULL, ME, "Not instance");
    amxd_object_t* parentObj = amxd_object_get_parent(mldObj);
    ASSERT_EQUALS(get_wld_object(), amxd_object_get_parent(parentObj), NULL, ME, "wrong location");
    const char* parentName = amxd_object_get_name(parentObj, AMXD_OBJECT_NAMED);
    ASSERT_TRUE(swl_str_matches(parentName, "APMLD"), NULL, ME, "invalid parent obj(%s)", parentName);
    wld_mld_t* pMld = (wld_mld_t*) mldObj->priv;
    ASSERTS_TRUE(pMld, NULL, ME, "NULL");
    return pMld;
}

/**
 * @brief Update runtime fields in an AffiliatedAP DM instance.
 *
 * Updates the "BSSID" and "LinkID" fields in the AffiliatedAP object
 * associated with the given MLD link. Typically used when link runtime
 * information (MAC address or link ID) changes.
 *
 * Actions performed:
 * - Retrieves AffiliatedAP DM object from the given link.
 * - Updates BSSID and LinkID fields via a local DM transaction.
 * - Logs the performed operations.
 *
 * @param pLink Pointer to the internal MLD link whose AffiliatedAP DM fields need to be updated.
 *
 * @return amxd_status_ok on success, amxd_status_object_not_found if the link or object is NULL.
 */
amxd_status_t wld_apMld_updateAffAP(wld_mldLink_t* pLink) {
    ASSERT_NOT_NULL(pLink, amxd_status_object_not_found, ME, "AffiliatedAP object not found!");
    amxd_object_t* affObj = pLink->AffObj;
    if(affObj != NULL) {
        const char* bssidStr = swl_typeMacBin_toBuf32Ref((swl_macBin_t*) pLink->pSSID->MACAddress).buf;
        amxd_trans_t trans;
        ASSERT_TRANSACTION_INIT(affObj, &trans, , ME, "Failed to init transaction for AffiliatedAP fields update");

        amxd_trans_set_cstring_t(&trans, "BSSID", bssidStr);
        amxd_trans_set_int8_t(&trans, "LinkID", pLink->linkId);

        ASSERT_TRANSACTION_LOCAL_DM_END(&trans, , ME, "Failed to apply transaction for AffiliatedAP fields update %s and %d", bssidStr, pLink->linkId);

        SAH_TRACEZ_INFO(ME, "Updated AffiliatedAP fields: BSSID=%s, LinkID=%d", bssidStr, pLink->linkId);
    } else {
        SAH_TRACEZ_ERROR(ME, "AffiliatedAP object is NULL; cannot update fields");
        return amxd_status_object_not_found;
    }
    return amxd_status_ok;
}

/**
 * @brief Updates the Multi-Link Device (MLD) information for the specified SSID.
 *
 * This function refreshes the MLD context of the given SSID by retrieving and
 * updating the latest MLD link information. It ensures that the SSID reflects
 * the current MLD configuration and link associations maintained by the system.
 *
 * @param pSSID Pointer to the SSID structure for which the MLD data is to be updated.
 *
 * @return swl_rc_ne
 *         - SWL_RC_OK on successful update.
 *         - SWL_RC_INVALID_PARAM if input parameters are invalid.
 *         - SWL_RC_ERROR for any transaction or update failures.
 */
swl_rc_ne wld_apMld_updateDM(T_SSID* pSSID) {
    if((pSSID == NULL) || (pSSID->pMldLink == NULL) || (pSSID->pMldLink->pMld == NULL)) {
        SAH_TRACEZ_WARNING(ME, "Cannot update MLDMACAddress: no MLD info for SSID %s", pSSID ? pSSID->Name : "NULL");
        return SWL_RC_INVALID_PARAM;
    }

    wld_mld_t* pMld = pSSID->pMldLink->pMld;
    wld_mldLink_t* pLink = pSSID->pMldLink;
    amxd_object_t* obj = pMld->object;
    amxd_object_t* affObj = pSSID->pMldLink->AffObj;
    T_AccessPoint* pAP = pSSID->AP_HOOK;

    if(obj == NULL) {
        SAH_TRACEZ_WARNING(ME, "No DM object associated with MLD unit %u", pMld->unit);
        return SWL_RC_ERROR;
    }

    if(affObj == NULL) {
        SAH_TRACEZ_WARNING(ME, "No AFF object is associated with MLD unit %u", pMld->unit);
        return SWL_RC_ERROR;
    }

    wld_nl80211_ifaceInfo_t mldIfaceInfo;
    int32_t linkId = -1;
    memset(&mldIfaceInfo, 0, sizeof(mldIfaceInfo));
    const char* mldMacStr = g_swl_macChar_null.cMac;

    swl_rc_ne rc = wld_ssid_nl80211_getMldIfaceInfo(pSSID, &mldIfaceInfo, &linkId);
    if((rc == SWL_RC_OK) && (mldIfaceInfo.nMloLinks > 0)) {
        swl_macBin_t* pMldMac = &mldIfaceInfo.mloLinks[0].link.mldMac;

        mldMacStr = swl_typeMacBin_toBuf32Ref(pMldMac).buf;

        SAH_TRACEZ_INFO(ME, "Fetched MLDMACAddress=%s for APMLD unit %u", mldMacStr, pMld->unit);
    } else {
        SAH_TRACEZ_WARNING(ME, "Failed to get MLD iface info or no MLO links for SSID %s", pSSID->Name);
    }

    amxd_trans_t trans;
    ASSERT_TRANSACTION_INIT(obj, &trans, SWL_RC_ERROR, ME, "Failed to init transaction for APMLD fields update");
    uint32_t numLinks = amxc_llist_size(&pMld->links);
    amxd_trans_set_cstring_t(&trans, "MLDMACAddress", mldMacStr);
    amxd_trans_set_uint32_t(&trans, "MLDID", pMld->unit);

    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, SWL_RC_ERROR, ME, "Failed to apply transaction for APMLD fields : MLDMACAddress %s and MLDID %d", mldMacStr, pMld->unit);

    SAH_TRACEZ_INFO(ME, "Updated APMLD fields: MLDMACAddress=%s, AffiliatedAPNumberOfEntries=%d and MLDID=%d", mldMacStr, numLinks, pMld->unit);
    //------APMLD Config-----------
    bool cfg_changed = false;
    if(pMld->Cfg.emlmrEnable != pAP->mldCfg.emlmrEnable) {
        pAP->mldCfg.emlmrEnable = pMld->Cfg.emlmrEnable;
        cfg_changed = true;
        SAH_TRACEZ_INFO(ME, "Updated : %s emlmr_enable: new value %d", pSSID->Name, pAP->mldCfg.emlmrEnable);
    } else {
        SAH_TRACEZ_INFO(ME, "%s emlmr_enable value : %d same as parent mld", pSSID->Name, pAP->mldCfg.emlmrEnable);
    }
    if(pMld->Cfg.emlsrEnable != pAP->mldCfg.emlsrEnable) {
        pAP->mldCfg.emlsrEnable = pMld->Cfg.emlsrEnable;
        cfg_changed = true;
        SAH_TRACEZ_INFO(ME, "Updated : %s emlsr_enable: new value %d", pSSID->Name, pAP->mldCfg.emlsrEnable);
    } else {
        SAH_TRACEZ_INFO(ME, "%s emlsr_enable value : %d same as parent mld", pSSID->Name, pAP->mldCfg.emlsrEnable);
    }
    if(pMld->Cfg.strEnable != pAP->mldCfg.strEnable) {
        pAP->mldCfg.strEnable = pMld->Cfg.strEnable;
        cfg_changed = true;
        SAH_TRACEZ_INFO(ME, "Updated : %s str_enable: new value %d", pSSID->Name, pAP->mldCfg.strEnable);
    } else {
        SAH_TRACEZ_INFO(ME, "%s str_enable value : %d same as parent mld", pSSID->Name, pAP->mldCfg.strEnable);
    }
    if(pMld->Cfg.nstrEnable != pAP->mldCfg.nstrEnable) {
        pAP->mldCfg.nstrEnable = pMld->Cfg.nstrEnable;
        cfg_changed = true;
        SAH_TRACEZ_INFO(ME, "Updated : %s nstr_enable: new value %d", pSSID->Name, pAP->mldCfg.nstrEnable);
    } else {
        SAH_TRACEZ_INFO(ME, "%s nstr_enable value : %d same as parent mld", pSSID->Name, pAP->mldCfg.nstrEnable);
    }

    if(cfg_changed) {
        pAP->pFA->mfn_wvap_setMldCfg(pAP);
        SAH_TRACEZ_INFO(ME, "LinkID: %d, SSID Name: %s Updated Config", pLink->linkId, pSSID->Name);
    }

    if(numLinks > 0) {
        wld_apMld_updateAffAP(pLink);
    }
    return SWL_RC_OK;
}
