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
#include "wld_radio.h"
#include "swl/swl_assert.h"
#include "swla/swla_trans.h"
#include "wld_rad_stamon.h"
#include "wld_monitor.h"

#define ME "radStm"

/**
 * Retrieve a Device based on its string MACAddress in a given list.
 */
wld_nasta_t* wld_rad_staMon_getDeviceExt(const swl_macBin_t* pMacBin, amxc_llist_t* devList) {
    ASSERT_NOT_NULL(pMacBin, NULL, ME, "NULL");
    amxc_llist_for_each(it, devList) {
        wld_nasta_t* pMD = amxc_llist_it_get_data(it, wld_nasta_t, it);
        if(memcmp(pMacBin->bMac, pMD->MACAddress, ETHER_ADDR_LEN) == 0) {
            return pMD;
        }
    }

    return NULL;
}

wld_nasta_t* wld_rad_staMon_getDevice(const char* macAddrStr, amxc_llist_t* devList) {
    swl_macBin_t macAddr = SWL_MAC_BIN_NEW();
    ASSERT_TRUE(swl_typeMacBin_fromChar(&macAddr, macAddrStr), NULL, ME, "Bad Format MacAddress (%s)", macAddrStr);
    return wld_rad_staMon_getDeviceExt(&macAddr, devList);
}

T_NonAssociatedDevice* wld_rad_staMon_getNonAssociatedDevice(T_Radio* pRad, const char* macAddrStr) {
    return wld_rad_staMon_getDevice(macAddrStr, &pRad->naStations);
}

T_MonitorDevice* wld_rad_staMon_getMonitorDevice(T_Radio* pRad, const char* macAddrStr) {
    return wld_rad_staMon_getDevice(macAddrStr, &pRad->monitorDev);
}

static amxd_status_t s_staMon_addDevice(T_Radio* pRad, amxd_object_t* instance_object, amxc_llist_t* devList, const amxc_var_t* const args) {
    ASSERT_NOT_NULL(pRad, amxd_status_unknown_error, ME, "NULL");
    ASSERT_NOT_NULL(instance_object, amxd_status_unknown_error, ME, "NULL");
    ASSERT_NOT_NULL(devList, amxd_status_unknown_error, ME, "NULL");
    amxc_var_t* params = amxc_var_get_key(args, "parameters", AMXC_VAR_FLAG_DEFAULT);
    const char* macAddrStr = GET_CHAR(params, "MACAddress");
    swl_macBin_t macAddr = SWL_MAC_BIN_NEW();
    ASSERT_TRUE(swl_typeMacBin_fromChar(&macAddr, macAddrStr), amxd_status_unknown_error, ME, "macAddrStr %s is not accepted! ", macAddrStr);

    wld_nasta_t* pMD = calloc(1, sizeof(wld_nasta_t));
    ASSERT_NOT_NULL(pMD, amxd_status_unknown_error, ME, "NULL");
    amxc_llist_it_init(&pMD->it);
    amxc_llist_append(devList, &pMD->it);
    instance_object->priv = pMD;
    memcpy(pMD->MACAddress, macAddr.bMac, ETHER_ADDR_LEN);

    const char* bssidStr = GET_CHAR(params, "RequestedBSSID");
    if((bssidStr != NULL) && !swl_str_isEmpty(bssidStr)) {
        ASSERT_TRUE(swl_typeMacBin_fromChar(&pMD->bssid, bssidStr), amxd_status_unknown_error, ME, "bssidStr %s is not accepted! ", bssidStr);
    }

    pMD->SignalStrength = 0;
    pMD->TimeStamp = 0;
    pMD->channel = GET_UINT32(params, "Channel");
    pMD->operatingClass = GET_UINT32(params, "OperatingClass");

    pMD->obj = instance_object;
    pRad->pFA->mfn_wrad_add_stamon(pRad, pMD);
    return amxd_status_ok;
}

/**
 *  Called when a new NonAssociatedDevice is added.
 */
amxd_status_t _wld_rad_staMon_addNaSta_oaf(amxd_object_t* object,
                                           amxd_param_t* param,
                                           amxd_action_t reason,
                                           const amxc_var_t* const args,
                                           amxc_var_t* const retval,
                                           void* priv) {
    amxd_status_t status = amxd_status_ok;
    status = amxd_action_object_add_inst(object, param, reason, args, retval, priv);
    ASSERT_EQUALS(status, amxd_status_ok, status, ME, "Nasta creation failed with error: %d", status);
    T_Radio* pRad = wld_rad_fromObj(amxd_object_get_parent(amxd_object_get_parent(object)));
    ASSERT_NOT_NULL(pRad, status, ME, "NULL");

    amxd_object_t* instance = amxd_object_get_instance(object, NULL, GET_UINT32(retval, "index"));
    status = s_staMon_addDevice(pRad, instance, &pRad->naStations, args);

    return status;
}

static amxd_status_t s_staMon_deleteDevice(amxd_object_t* instanceObject) {
    ASSERT_NOT_NULL(instanceObject, amxd_status_unknown_error, ME, "NULL");
    wld_nasta_t* pMD = (wld_nasta_t*) instanceObject->priv;
    ASSERT_NOT_NULL(pMD, amxd_status_unknown_error, ME, "Not mapped to internal ctx");
    amxd_object_t* templateObject = amxd_object_get_parent(instanceObject);
    T_Radio* pRad = wld_rad_fromObj(amxd_object_get_parent(amxd_object_get_parent(templateObject)));
    ASSERT_NOT_NULL(pRad, amxd_status_unknown_error, ME, "Not mapped to radio ctx");

    amxc_llist_it_take(&(pMD->it));

    pRad->pFA->mfn_wrad_del_stamon(pRad, pMD);

    free(pMD);

    return amxd_status_ok;
}

/**
 *  Called when a NonAssociatedDevice is deleted
 */
amxd_status_t _wld_rad_staMon_deleteNaSta_odf(amxd_object_t* object,
                                              amxd_param_t* param _UNUSED,
                                              amxd_action_t reason _UNUSED,
                                              const amxc_var_t* const args _UNUSED,
                                              amxc_var_t* const retval _UNUSED,
                                              void* priv _UNUSED) {
    amxd_status_t status = amxd_action_object_destroy(object, param, reason, args, retval, priv);
    ASSERT_EQUALS(status, amxd_status_ok, status, ME, "Fail to destroy obj instance st:%d", status);
    ASSERTS_EQUALS(amxd_object_get_type(object), amxd_object_instance, status, ME, "obj is not instance");
    s_staMon_deleteDevice(object);
    return status;
}

static amxd_status_t s_stamon_createDevice(amxd_object_t* parent, const char* template, amxc_var_t* args, amxc_llist_t* devList) {
    ASSERT_NOT_NULL(parent, amxd_status_unknown_error, ME, "NULL");
    ASSERT_NOT_NULL(template, amxd_status_unknown_error, ME, "NULL");

    const char* macAddr = GET_CHAR(args, "macaddress");
    const char* bssid = GET_CHAR(args, "bssid");
    const uint8_t chan = GET_UINT32(args, "channel");
    ASSERT_TRUE(swl_mac_charIsValidStaMac((swl_macChar_t*) macAddr), amxd_status_invalid_value, ME, "invalid MACAddress (%s)", macAddr);
    SAH_TRACEZ_INFO(ME, "Creating Device instance with template %s and macAddr %s", template, macAddr);

    amxd_object_t* object;
    wld_nasta_t* pMD = wld_rad_staMon_getDevice(macAddr, devList);
    if(pMD != NULL) {
        SAH_TRACEZ_INFO(ME, "Found Device instance with macAddr %s", macAddr);
        object = pMD->obj;
        if(chan != pMD->channel) {
            SAH_TRACEZ_INFO(ME, "Existing Channel : %d Channel to update : %d", pMD->channel, chan);
            swl_typeUInt8_commitObjectParam(object, "Channel", chan);
        }
    } else {

        amxc_var_t values;
        amxc_var_init(&values);
        amxc_var_set_type(&values, AMXC_VAR_ID_HTABLE);
        char alias[swl_str_len(macAddr) + 2];
        snprintf(alias, sizeof(alias), "_%s", macAddr);
        amxc_var_add_key(cstring_t, &values, "Alias", alias);
        amxc_var_add_key(cstring_t, &values, "MACAddress", macAddr);
        amxc_var_add_key(cstring_t, &values, "RequestedBSSID", (bssid != NULL) ? bssid : "");

        amxc_var_t* varChan = GET_ARG(args, "channel");
        if(varChan != NULL) {
            amxc_var_add_key(uint8_t, &values, "Channel", amxc_var_dyncast(uint8_t, varChan));
        }
        amxc_var_t* varOpClass = GET_ARG(args, "operatingClass");
        if(varOpClass != NULL) {
            amxc_var_add_key(uint8_t, &values, "OperatingClass", amxc_var_dyncast(uint8_t, varOpClass));
        }

        amxd_object_t* templateObject = amxd_object_get(parent, template);
        // the following is just starting the on action add-inst function
        amxd_status_t status = amxd_object_add_instance(&object, templateObject, alias, 0, &values);
        amxc_var_clean(&values);
        if(object == NULL) {
            SAH_TRACEZ_ERROR(ME, "Failed to create new instance of %s with error %d", template, status);
            return status;
        }
    }

    return amxd_status_ok;
}


static amxd_status_t s_staMon_deleteNastaDevice(amxc_var_t* args, amxc_llist_t* devList) {
    const char* macAddr = GET_CHAR(args, "macaddress");
    ASSERT_TRUE(swl_mac_charIsValidStaMac((swl_macChar_t*) macAddr), amxd_status_unknown_error, ME, "invalid MACAddress (%s)", macAddr);
    SAH_TRACEZ_INFO(ME, "DeleteNastaDevice with macaddress %s", macAddr);

    wld_nasta_t* pMD = wld_rad_staMon_getDevice(macAddr, devList);
    ASSERTI_NOT_NULL(pMD, amxd_status_ok, ME, "No entry for mac %s", macAddr);
    swl_object_delInstWithTransOnLocalDm(pMD->obj);
    pMD->obj = NULL;

    return amxd_status_ok;
}


/**
 * This function create a new non associated station based on MACAddress.
 */
amxd_status_t _createNonAssociatedDevice(amxd_object_t* obj,
                                         amxd_function_t* func _UNUSED,
                                         amxc_var_t* args,
                                         amxc_var_t* ret _UNUSED) {
    SAH_TRACEZ_IN(ME);

    amxd_status_t state = amxd_status_unknown_error;
    T_Radio* pRad = wld_rad_fromObj(amxd_object_get_parent(obj));
    ASSERT_NOT_NULL(pRad, state, ME, "NULL");

    swl_chanspec_t spec = wld_rad_getSwlChanspec(pRad);
    amxc_var_t* var = GET_ARG(args, "channel");
    if(var == NULL) {
        amxc_var_add_new_key_uint8_t(args, "channel", spec.channel);
    } else {
        swl_channel_t reqChan = amxc_var_dyncast(uint8_t, var);
        if(!wld_rad_hasChannel(pRad, reqChan)) {
            SAH_TRACEZ_WARNING(ME, "%s: ignore invalid channel %d", pRad->Name, reqChan);
            amxc_var_set_uint8_t(var, spec.channel);
        } else {
            spec.channel = reqChan;
        }
    }
    var = GET_ARG(args, "bandwidth");
    if(var == NULL) {
        amxc_var_add_new_key_uint8_t(args, "bandwidth", swl_chanspec_bwToInt(spec.bandwidth));
    } else {
        uint8_t reqBw = amxc_var_dyncast(uint8_t, var);
        swl_bandwidth_e reqBwEnu = swl_chanspec_intToBw(reqBw);
        if((reqBwEnu == SWL_BW_AUTO) ||
           (((int32_t) reqBw) > swl_chanspec_bwToInt(pRad->maxChannelBandwidth))) {
            SAH_TRACEZ_WARNING(ME, "%s: ignore invalid bandwidth %d", pRad->Name, reqBw);
            amxc_var_set_uint8_t(var, swl_chanspec_bwToInt(spec.bandwidth));
        } else {
            spec.bandwidth = reqBwEnu;
        }
    }

    swl_operatingClass_t opClass = swl_chanspec_getOperClass(&spec);
    var = GET_ARG(args, "operatingClass");
    if(var == NULL) {
        amxc_var_add_new_key_uint8_t(args, "operatingClass", opClass);
    } else {
        swl_operatingClass_t reqOpClass = amxc_var_dyncast(uint8_t, var);
        swl_freqBandExt_e reqOpClassBand = swl_chanspec_operClassToFreq(reqOpClass);
        swl_bandwidth_e reqOpClassBw = swl_chanspec_operClassToBandwidth(reqOpClass);
        if((!reqOpClass) ||
           (reqOpClassBand != spec.band) ||
           (swl_chanspec_bwToInt(reqOpClassBw) > swl_chanspec_bwToInt(pRad->maxChannelBandwidth))) {
            SAH_TRACEZ_WARNING(ME, "%s: ignore invalid operClass %d", pRad->Name, opClass);
            amxc_var_set_uint8_t(var, opClass);
        } else {
            spec.bandwidth = reqOpClassBw;
        }
    }

    state = s_stamon_createDevice(obj, "NonAssociatedDevice", args, &pRad->naStations);

    SAH_TRACEZ_OUT(ME);
    return state;
}

/**
 * This function delete a non associated station based on MACAddress.
 */
amxd_status_t _deleteNonAssociatedDevice(amxd_object_t* obj,
                                         amxd_function_t* func _UNUSED,
                                         amxc_var_t* args,
                                         amxc_var_t* ret _UNUSED) {
    SAH_TRACEZ_IN(ME);

    T_Radio* pRad = wld_rad_fromObj(amxd_object_get_parent(obj));
    ASSERT_NOT_NULL(pRad, amxd_status_unknown_error, ME, "NULL");

    amxd_status_t state = s_staMon_deleteNastaDevice(args, &pRad->naStations);

    SAH_TRACEZ_OUT(ME);
    return state;
}

static amxd_status_t s_getDeviceStats(amxd_object_t* obj,
                                      amxc_var_t* retval,
                                      const char* path) {
    SAH_TRACEZ_IN(ME);

    amxc_var_set_type(retval, AMXC_VAR_ID_LIST);

    T_Radio* pRad = wld_rad_fromObj(amxd_object_get_parent(obj));
    ASSERT_NOT_NULL(pRad, amxd_status_unknown_error, ME, "Invalid Radio Pointer");
    ASSERTI_TRUE(pRad->stationMonitorEnabled, amxd_status_ok, ME, "%s stamon disabled", pRad->Name);
    ASSERTI_TRUE(!amxc_llist_is_empty(&pRad->naStations), amxd_status_ok, ME, "No station monitor entry is present");

    swl_rc_ne rc = pRad->pFA->mfn_wrad_update_mon_stats(pRad);
    ASSERTS_NOT_EQUALS(rc, SWL_RC_NOT_IMPLEMENTED, amxd_status_ok, ME, "%s: staMon not supported", pRad->Name);
    ASSERTI_FALSE(rc < SWL_RC_OK, amxd_status_unknown_error, ME, "%s: Update monitor stats failed", pRad->Name);

    amxd_trans_t trans;
    ASSERT_TRANSACTION_INIT(pRad->pBus, &trans, amxd_status_unknown_error, ME, "%s : trans init failure", pRad->Name);

    amxd_object_t* object = amxd_object_get(obj, path);
    amxd_object_for_each(instance, it, object) {
        amxd_object_t* instance = amxc_container_of(it, amxd_object_t, it);
        if(instance == NULL) {
            continue;
        }
        wld_nasta_t* pMD = (wld_nasta_t*) instance->priv;
        if(!pMD) {
            continue;
        }
        amxd_trans_select_object(&trans, instance);

        amxd_trans_set_int32_t(&trans, "SignalStrength", pMD->SignalStrength);
        amxd_trans_set_uint8_t(&trans, "Channel", pMD->channel);
        amxd_trans_set_uint8_t(&trans, "OperatingClass", pMD->operatingClass);
        swl_typeTimeMono_toTransParam(&trans, "TimeStamp", pMD->TimeStamp);
    }

    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, amxd_status_unknown_error, ME, "%s : trans apply failure", pRad->Name);

    amxd_object_for_each(instance, it, object) {
        amxd_object_t* instance = amxc_container_of(it, amxd_object_t, it);
        if(instance == NULL) {
            continue;
        }
        wld_nasta_t* pMD = (wld_nasta_t*) instance->priv;
        if(!pMD) {
            continue;
        }
        amxc_var_t tmpVar;
        amxc_var_init(&tmpVar);
        amxd_object_get_params(instance, &tmpVar, amxd_dm_access_private);
        amxc_var_add_new_amxc_htable_t(retval, &tmpVar.data.vm);
        amxc_var_clean(&tmpVar);
    }


    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

/**
 * Retrieve NonAssociatedDevice statistics from WLAN driver
 */
amxd_status_t _getNaStationStats(amxd_object_t* obj,
                                 amxd_function_t* func _UNUSED,
                                 amxc_var_t* args _UNUSED,
                                 amxc_var_t* retval) {
    return s_getDeviceStats(obj, retval, "NonAssociatedDevice");
}

amxd_status_t _clearNonAssociatedDevices(amxd_object_t* obj,
                                         amxd_function_t* func _UNUSED,
                                         amxc_var_t* args _UNUSED,
                                         amxc_var_t* retval _UNUSED) {
    SAH_TRACEZ_IN(ME);

    T_Radio* pRad = wld_rad_fromObj(amxd_object_get_parent(obj));
    ASSERT_TRUE(debugIsRadPointer(pRad), amxd_status_unknown_error, ME, "Invalid Radio Pointer");

    amxc_llist_it_t* it;
    it = amxc_llist_get_first(&pRad->naStations);
    while(it != NULL) {
        T_NonAssociatedDevice* pMD = amxc_llist_it_get_data(it, T_NonAssociatedDevice, it);
        if(pMD) {
            swl_object_delInstWithTransOnLocalDm(pMD->obj);
            pMD->obj = NULL;
        }
        it = amxc_llist_get_first(&pRad->naStations);
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

static void s_setEnable_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_Radio* pR = wld_rad_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pR, , ME, "NULL");
    bool enabled = amxc_var_dyncast(bool, newValue);

    pR->stationMonitorEnabled = enabled;

    swl_rc_ne ret = pR->pFA->mfn_wrad_setup_stamon(pR, enabled);
    ASSERTI_NOT_EQUALS(ret, SWL_RC_NOT_IMPLEMENTED, , ME, "%s: station monitor not supported", pR->Name);
    ASSERT_FALSE(ret < SWL_RC_OK, , ME, "%s: Failed to %s station monitor", pR->Name, enabled ? "enable" : "disable");
    wld_radStaMon_updateActive(pR);

    SAH_TRACEZ_OUT(ME);
}

void wld_radio_updateNaStaMonitor(T_Radio* pRad, amxd_trans_t* trans) {
    amxd_object_t* hwObject = amxd_object_findf(pRad->pBus, "NaStaMonitor");

    swla_trans_t tmpTrans;
    amxd_trans_t* targetTrans = swla_trans_init(&tmpTrans, trans, hwObject);
    ASSERT_NOT_NULL(targetTrans, , ME, "NULL");

    amxd_trans_set_bool(targetTrans, "OffChannelSupported", pRad->stationMonitorOffChannelSupported);

    swla_trans_finalize(&tmpTrans, NULL);
}

static int32_t wld_rad_staMon_getStats(amxc_var_t* myList, T_RssiEventing* ev, amxc_llist_t* devList) {
    int nrUpdates = 0;

    amxc_llist_for_each(it, devList) {
        wld_nasta_t* pMD = amxc_llist_it_get_data(it, wld_nasta_t, it);

        pMD->rssiAccumulator = wld_util_performFactorStep(
            pMD->rssiAccumulator, pMD->SignalStrength, ev->averagingFactor);
        int32_t rssi_val = WLD_ACC_TO_VAL(pMD->rssiAccumulator);

        uint32_t diff = abs(pMD->monRssi - rssi_val);
        SAH_TRACEZ_INFO(ME, "Check "MAC_PRINT_FMT " : %i : %i - %i ?>= %i",
                        MAC_PRINT_ARG(pMD->MACAddress), pMD->SignalStrength, rssi_val, pMD->monRssi, ev->rssiInterval);
        if(diff >= ev->rssiInterval) {
            pMD->monRssi = rssi_val;
            amxc_var_t* pMyMap = amxc_var_add(amxc_htable_t, myList, NULL);
            amxc_var_add_key(int32_t, pMyMap, "SignalStrength", pMD->monRssi);
            swl_typeMacBin_addToMapRef(pMyMap, "MACAddress", (swl_macBin_t*) pMD->MACAddress);
            swl_typeTimeSpecReal_addToMap(pMyMap, "TimeStamp", swl_timespec_getRealVal());
            nrUpdates++;
        }
    }
    return nrUpdates;
}

static void timeHandler(void* userdata) {
    T_Radio* pRad = (T_Radio*) userdata;
    ASSERT_NOT_NULL(pRad, , ME, "Invalid Radio Pointer");
    ASSERTI_TRUE(!amxc_llist_is_empty(&pRad->naStations), , ME, "No station monitor entry is present");

    T_RssiEventing* ev = &pRad->naStaRssiMonitor;

    SAH_TRACEZ_INFO(ME, "Time rssiMon %s", pRad->Name);

    swl_rc_ne result = pRad->pFA->mfn_wrad_update_mon_stats(pRad);
    if(result != SWL_RC_OK) {
        wld_mon_stop(&ev->monitor);
    }

    int nrUpdates = 0;

    amxc_var_t myList;
    amxc_var_init(&myList);
    amxc_var_set_type(&myList, AMXC_VAR_ID_LIST);

    nrUpdates = wld_rad_staMon_getStats(&myList, ev, &pRad->monitorDev);
    nrUpdates += wld_rad_staMon_getStats(&myList, ev, &pRad->naStations);

    // Only send update if necessary, don't want to continuously spam
    if(nrUpdates > 0) {
        amxd_object_t* eventObject = amxd_object_findf(pRad->pBus, "NaStaMonitor.RssiEventing");
        wld_mon_sendUpdateNotification(eventObject, "RssiUpdate", &myList);
    }
    //Cleanup
    amxc_var_clean(&myList);
}

void wld_radStaMon_updateActive(T_Radio* pRad) {
    bool targetActive = (pRad->status == RST_UP) && pRad->stationMonitorEnabled;
    wld_mon_writeActive(&(pRad->naStaRssiMonitor.monitor), targetActive);
}

void wld_radStaMon_init(T_Radio* pRad) {
    amxd_object_t* eventObject = amxd_object_findf(pRad->pBus, "NaStaMonitor.RssiEventing");
    T_RssiEventing* ev = &pRad->naStaRssiMonitor;
    char name[IFNAMSIZ + 16] = {0};
    snprintf(name, sizeof(name), "%s_RadRssiMon", pRad->Name);
    wld_mon_initMon(&ev->monitor, eventObject, name, pRad, timeHandler);
}

void wld_radStaMon_destroy(T_Radio* pRad) {
    SAH_TRACEZ_IN(ME);
    amxc_llist_it_t* it;
    while(!amxc_llist_is_empty(&pRad->naStations)) {
        it = amxc_llist_get_first(&pRad->naStations);
        wld_nasta_t* pNaSta = amxc_llist_it_get_data(it, wld_nasta_t, it);
        swl_object_delInstWithTransOnLocalDm(pNaSta->obj);
        pNaSta->obj = NULL;
    }

    while(!amxc_llist_is_empty(&pRad->monitorDev)) {
        it = amxc_llist_get_first(&pRad->monitorDev);
        wld_nasta_t* pMonDev = amxc_llist_it_get_data(it, wld_nasta_t, it);
        swl_object_delInstWithTransOnLocalDm(pMonDev->obj);
        pMonDev->obj = NULL;
    }
    T_RssiEventing* ev = &pRad->naStaRssiMonitor;
    wld_mon_destroyMon(&ev->monitor);
    SAH_TRACEZ_OUT(ME);
}

static void s_setRssiEventing_ocf(void* priv _UNUSED, amxd_object_t* object, const amxc_var_t* const newParamValues _UNUSED) {
    SAH_TRACEZ_IN(ME);
    T_Radio* pRad = wld_rad_fromObj(amxd_object_get_parent(amxd_object_get_parent(object)));
    ASSERT_NOT_NULL(pRad, , ME, "NULL");

    SAH_TRACEZ_INFO(ME, "%s: Update rssiMon", pRad->Name);

    T_RssiEventing* ev = &pRad->naStaRssiMonitor;
    ASSERT_NOT_NULL(ev, , ME, "NULL");

    ev->rssiInterval = amxd_object_get_uint32_t(object, "RssiInterval", NULL);
    ev->averagingFactor = amxd_object_get_uint32_t(object, "AveragingFactor", NULL);
    amxc_var_t* val = GET_ARG(newParamValues, "Interval");
    if(val != NULL) {
        wld_mon_setInterval_pwf(&ev->monitor, val);
    }
    val = GET_ARG(newParamValues, "Enable");
    if(val != NULL) {
        wld_mon_setEnable_pwf(&ev->monitor, val);
    }

    SAH_TRACEZ_OUT(ME);
}

SWLA_DM_HDLRS(sRssiEventingConfigDmHdlrs, ARR(), .objChangedCb = s_setRssiEventing_ocf);

void _wld_radStaMon_setRssiEventing_ocf(const char* const sig_name,
                                        const amxc_var_t* const data,
                                        void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sRssiEventingConfigDmHdlrs, sig_name, data, priv);
}

void wld_radStaMon_debug(T_Radio* pRad, amxc_var_t* retMap) {
    T_RssiEventing* ev = &pRad->naStaRssiMonitor;
    amxc_var_add_key(uint32_t, retMap, "RssiInterval", ev->rssiInterval);
    amxc_var_add_key(uint32_t, retMap, "AveragingFactor", ev->averagingFactor);
    wld_mon_debug(&ev->monitor, retMap);
}

amxd_status_t _wld_rad_staMon_addNastaMonDev_oaf(amxd_object_t* object,
                                                 amxd_param_t* param,
                                                 amxd_action_t reason,
                                                 const amxc_var_t* const args,
                                                 amxc_var_t* const retval,
                                                 void* priv) {
    amxd_status_t status = amxd_status_ok;
    status = amxd_action_object_add_inst(object, param, reason, args, retval, priv);
    ASSERT_EQUALS(status, amxd_status_ok, status, ME, "Nasta creation failed with error: %d", status);
    T_Radio* pRad = wld_rad_fromObj(amxd_object_get_parent(amxd_object_get_parent(object)));
    ASSERT_NOT_NULL(pRad, status, ME, "NULL");
    amxd_object_t* instance = amxd_object_get_instance(object, NULL, GET_UINT32(retval, "index"));
    status = s_staMon_addDevice(pRad, instance, &pRad->monitorDev, args);

    return status;
}

amxd_status_t _wld_rad_staMon_deleteNastaMonDev_odf(amxd_object_t* object,
                                                    amxd_param_t* param _UNUSED,
                                                    amxd_action_t reason _UNUSED,
                                                    const amxc_var_t* const args _UNUSED,
                                                    amxc_var_t* const retval _UNUSED,
                                                    void* priv _UNUSED) {
    amxd_status_t status = amxd_action_object_destroy(object, param, reason, args, retval, priv);
    ASSERT_EQUALS(status, amxd_status_ok, status, ME, "Fail to destroy obj instance st:%d", status);
    ASSERTS_EQUALS(amxd_object_get_type(object), amxd_object_instance, status, ME, "obj is not instance");
    return s_staMon_deleteDevice(object);
}

/**
 * Retrieve MonitorDevice statistics from WLAN driver
 */
amxd_status_t _getMonitorDeviceStats(amxd_object_t* obj,
                                     amxd_function_t* func _UNUSED,
                                     amxc_var_t* args _UNUSED,
                                     amxc_var_t* retval) {
    return s_getDeviceStats(obj, retval, "MonitorDevice");
}

/**
 * This function create a new monitor device based on MACAddress.
 */
amxd_status_t _createMonitorDevice(amxd_object_t* obj,
                                   amxd_function_t* func _UNUSED,
                                   amxc_var_t* args,
                                   amxc_var_t* retval _UNUSED) {
    SAH_TRACEZ_IN(ME);

    T_Radio* pRad = wld_rad_fromObj(amxd_object_get_parent(obj));
    ASSERT_NOT_NULL(pRad, amxd_status_unknown_error, ME, "NULL");
    amxd_status_t state = s_stamon_createDevice(obj, "MonitorDevice", args, &pRad->monitorDev);

    SAH_TRACEZ_OUT(ME);
    return state;
}

amxd_status_t _deleteMonitorDevice(amxd_object_t* obj,
                                   amxd_function_t* func _UNUSED,
                                   amxc_var_t* args _UNUSED,
                                   amxc_var_t* retval _UNUSED) {
    SAH_TRACEZ_IN(ME);

    T_Radio* pRad = wld_rad_fromObj(amxd_object_get_parent(obj));
    ASSERT_NOT_NULL(pRad, amxd_status_unknown_error, ME, "NULL");

    amxd_status_t state = s_staMon_deleteNastaDevice(args, &pRad->monitorDev);

    SAH_TRACEZ_OUT(ME);
    return state;
}

amxd_status_t _clearMonitorDevices(amxd_object_t* obj,
                                   amxd_function_t* func _UNUSED,
                                   amxc_var_t* args _UNUSED,
                                   amxc_var_t* retval _UNUSED) {
    SAH_TRACEZ_IN(ME);

    T_Radio* pRad = amxd_object_get_parent(obj)->priv;
    ASSERT_TRUE(debugIsRadPointer(pRad), amxd_status_unknown_error, ME, "Invalid Radio Pointer");

    amxc_llist_it_t* it;
    it = amxc_llist_get_first(&pRad->monitorDev);
    while(it != NULL) {
        T_MonitorDevice* pMD = amxc_llist_it_get_data(it, T_MonitorDevice, it);
        if(pMD) {
            swl_object_delInstWithTransOnLocalDm(pMD->obj);
        }
        it = amxc_llist_get_first(&pRad->monitorDev);
    }

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

SWLA_DM_HDLRS(sRadStaMonDmHdlrs,
              ARR(SWLA_DM_PARAM_HDLR("Enable", s_setEnable_pwf)));

void _wld_radStaMon_setConf_ocf(const char* const sig_name,
                                const amxc_var_t* const data,
                                void* const priv _UNUSED) {
    swla_dm_procObjEvtOfLocalDm(&sRadStaMonDmHdlrs, sig_name, data, priv);
}

