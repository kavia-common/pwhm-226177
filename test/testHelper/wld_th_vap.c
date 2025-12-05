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
#include <sys/signalfd.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>

#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>


#include "wld_th_vap.h"
#include "wld_util.h"
#include "wld.h"
#include "wld_radio.h"
#include "wld_ssid.h"
#include "wld_assocdev.h"
#include "wld_th_mockVendor.h"
#include "test-toolbox/ttb_mockTimer.h"
#include "test-toolbox/ttb.h"

#define ME "thVAP"

int wld_th_vap_vendorCb_bssid(T_Radio* pRad, T_AccessPoint* pAP, unsigned char* buf, int bufsize, int set) {
    SAH_TRACEZ_INFO(ME, "%p - %p - %p - %d - %d", pRad, pAP, buf, bufsize, set);
    if(!pRad && !pAP) {
        return SWL_RC_INVALID_PARAM;
    }
    if(set & SET) {
        return SWL_RC_NOT_IMPLEMENTED;
    }
    if(!buf || (bufsize < ETHER_ADDR_STR_LEN)) {
        return SWL_RC_INVALID_PARAM;
    }
    swl_macBin_t* srcBMac = NULL;
    if(pAP) {
        T_SSID* pSSID = (T_SSID*) pAP->pSSID;
        assert_non_null(pSSID);
        srcBMac = (swl_macBin_t*) pSSID->BSSID;
    } else {
        srcBMac = (swl_macBin_t*) pRad->MACAddr;
    }
    // String formatted or octet based ?
    swl_mac_binToCharSep((swl_macChar_t*) buf, srcBMac, false, ':');
    return SWL_RC_OK;
}

int wld_th_vap_vendorCb_addVapIf(T_Radio* rad _UNUSED, char* vap _UNUSED, int bufsize _UNUSED) {
    assert_non_null(rad);
    assert_non_null(vap);
    assert_true(bufsize > 0);
    int vapIdx = ((1 + amxc_llist_it_index_of(&rad->it)) * 10) + wld_rad_countIfaces(rad);
    if((bufsize > 0) && !swl_str_isEmpty(vap)) {
        T_AccessPoint* pAP = wld_rad_vap_from_name(rad, vap);
        if(pAP && pAP->pSSID) {
            // generate BSSID and save it
            swl_macBin_t macBin = SWL_MAC_BIN_NEW();
            wld_ssid_generateBssid(rad, pAP, pAP->pSSID->autoMacRefIndex + 1, &macBin);
            wld_ssid_setBssid(pAP->pSSID, &macBin);
        }
    }
    return vapIdx;
}

swl_rc_ne wld_th_vap_createHook(T_AccessPoint* pAP) {
    assert_non_null(pAP);
    printf("CREATE CREATE %s\n\n", pAP->name);
    pAP->vendorData = calloc(1, sizeof(wld_th_vap_vendorData_t));
    return SWL_RC_OK;
}

void wld_th_vap_destroyHook(T_AccessPoint* pAP) {
    assert_non_null(pAP);
    printf("DESTROY DESTROY %s\n\n", pAP->name);
    free(pAP->vendorData);
    pAP->vendorData = NULL;
}

T_AccessPoint* wld_th_vap_createVap(amxb_bus_ctx_t* const bus_ctx, wld_th_mockVendor_t* mockVendor _UNUSED, T_Radio* radio, const char* name) {
    assert_non_null(bus_ctx);
    assert_non_null(radio);
    assert_non_null(name);
    amxc_var_t args;
    amxc_var_init(&args);

    amxc_var_set_type(&args, AMXC_VAR_ID_HTABLE);


    amxc_var_add_key(cstring_t, &args, "vap", name);
    amxc_var_add_key(cstring_t, &args, "radio", radio->instanceName);


    assert_int_equal(amxb_call(bus_ctx, "WiFi", "addVAPIntf", &args, NULL, 5), 0);
    ttb_mockTimer_goToFutureMs(10);

    amxc_var_clean(&args);

    T_AccessPoint* vap = wld_getAccesspointByAlias(name);
    assert_non_null(vap);
    return vap;
}

int wld_th_vap_status(T_AccessPoint* pAP) {
    assert_non_null(pAP);
    return wld_ap_hasStackEnabled(pAP);
}

swl_rc_ne wld_th_vap_getStationStats(T_AccessPoint* pAP) {
    assert_non_null(pAP);
    wld_th_vap_vendorData_t* vendorD = wld_th_vap_getVendorData(pAP);
    assert_non_null(vendorD);

    T_Radio* pRad = (T_Radio*) pAP->pRadio;
    assert_non_null(pRad);

    bool errOnStaStats = wld_th_vap_getVendorData(pAP)->errorOnStaStats;

    printf("%s: request stats %u / %u\n", pAP->alias, pRad->status, errOnStaStats);
    if(errOnStaStats) {
        return SWL_RC_ERROR;
    } else if(vendorD->staStatsFileName == NULL) {

        wld_vap_mark_all_stations_unseen(pAP);

        for(int i = 0; i < pAP->AssociatedDeviceNumberOfEntries; i++) {

            T_AssociatedDevice* pAD = pAP->AssociatedDevice[i];
            if(!pAD) {
                printf("NULL %u\n", i);
                return SWL_RC_ERROR;
            }
            pAD->seen = true;
            pAD->Active = true;
        }

        wld_vap_update_seen(pAP);
        return 0;
    } else {
        wld_vap_mark_all_stations_unseen(pAP);
        if(vendorD->nrStaInFile == 0) {
            wld_vap_update_seen(pAP);
            return 0;
        }
        swl_print_args_t tmpArgs = g_swl_print_json;
        T_AssociatedDevice dataList[vendorD->nrStaInFile];
        memset(dataList, 0, sizeof(dataList));

        size_t nrParsed = 0;
        bool done = swl_type_arrayFromFileName(&gtWld_associatedDevice.type.type, dataList, vendorD->nrStaInFile, vendorD->staStatsFileName,
                                               &tmpArgs, &nrParsed);
        printf("%s: Reading stats from file %s: %zu seen, %u expected / done: %u\n", pAP->name, vendorD->staStatsFileName,
               nrParsed, vendorD->nrStaInFile, done);
        printf("%s / %s\n",
               swl_type_toBuf32(&gtSwl_type_macBin, dataList[0].MACAddress).buf,
               swl_type_toBuf32(&gtSwl_type_macBin, dataList[1].MACAddress).buf);

        printf("%s\n", swl_type_arrayToBuf128(&gtWld_associatedDevice.type.type, dataList, vendorD->nrStaInFile).buf);


        // Add new devices from driver maclist
        for(uint32_t i = 0; i < vendorD->nrStaInFile; i++) {
            T_AssociatedDevice* pAD = wld_vap_find_asociatedDevice(pAP, (swl_macBin_t*) dataList[i].MACAddress);
            if(!pAD) {
                pAD = wld_create_associatedDevice(pAP, (swl_macBin_t*) dataList[i].MACAddress);
                printf("created AD from assoclist %s @ %s\n", pAD->Name, pAP->alias);
            }
            if(!pAD->Active && dataList[i].Active) {
                wld_ad_add_connection_try(pAP, pAD);
            }
            if(!pAD->AuthenticationState && dataList[i].AuthenticationState) {
                wld_ad_add_connection_success(pAP, pAD);
            }
            swl_tupleType_copyByMask(pAD, &gtWld_associatedDevice.type, &dataList[i], -1);

            printf("%s - %s\n", swl_type_toBuf32(&gtSwl_type_macBin, &pAD->MACAddress).buf,
                   swl_type_toBuf32(&gtSwl_type_macBin, &dataList[i].MACAddress).buf);
            pAD->seen = 1;
            pAD->lastSampleTime = swl_timespec_getMonoVal();
        }

        wld_vap_update_seen(pAP);

        swl_type_arrayCleanup(&gtWld_associatedDevice.type.type, dataList, vendorD->nrStaInFile);

        return 0;
    }
}

void wld_th_vap_setSSIDEnable(T_AccessPoint* pAP, bool enable, bool commit) {
    assert_non_null(pAP);
    T_SSID* pSSID = pAP->pSSID;
    if(commit) {
        swl_typeUInt8_commitObjectParam(pSSID->pBus, "Enable", enable);
        ttb_mockTimer_goToFutureMs(10);
    } else {
        swl_typeUInt8_toObjectParam(pSSID->pBus, "Enable", enable);
    }
}

void wld_th_vap_setApEnable(T_AccessPoint* pAP, bool enable, bool commit) {
    assert_non_null(pAP);
    if(commit) {
        swl_typeUInt8_commitObjectParam(pAP->pBus, "Enable", enable);
        ttb_mockTimer_goToFutureMs(10);
    } else {
        swl_typeUInt8_toObjectParam(pAP->pBus, "Enable", enable);
    }
}

int wld_th_vap_enable(T_AccessPoint* pAP, int enable, int set) {
    assert_non_null(pAP);
    int ret = enable;
    printf("VAP:%s State:%d-->%d - Set:%d\n", pAP->alias, pAP->enable, enable, set);

    if((enable == 0) && (set == SET)) {
        setBitLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW, WLD_TH_FSM_SET_VAP_ENABLE_DOWN);
    } else {
        setBitLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW, WLD_TH_FSM_SET_VAP_ENABLE);
    }

    return ret;
}


int wld_th_vap_ssid(T_AccessPoint* pAP, char* buf, int bufsize, int set) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    T_SSID* pSSID = pAP->pSSID;
    ASSERT_NOT_NULL(pSSID, SWL_RC_INVALID_PARAM, ME, "NULL");

    if(set & SET) {
        if(pSSID->SSID != buf) {
            swl_str_copy(pSSID->SSID, sizeof(pSSID->SSID), buf);
            SAH_TRACEZ_INFO(ME, "%s - %s", pAP->alias, pSSID->SSID);
        }
    } else {
        swl_str_copy(buf, bufsize, pSSID->SSID);
    }
    return SWL_RC_OK;
}

void wld_th_vap_doFsmClean(T_AccessPoint* pAP) {
    assert_non_null(pAP);
    clearAllBitsLongArray(pAP->fsm.FSM_BitActionArray, FSM_BW);
}

wld_th_vap_vendorData_t* wld_th_vap_getVendorData(T_AccessPoint* pAP) {
    assert_non_null(pAP);
    return (wld_th_vap_vendorData_t*) pAP->vendorData;
}


void wld_th_vap_clearCommits(T_AccessPoint* pAP) {
    wld_th_vap_vendorData_t* vd = wld_th_vap_getVendorData(pAP);
    ttb_assert_non_null(vd);

    for(uint32_t i = 0; i < WLD_TH_FSM_MAX; i++) {
        vd->fsmCommits[i] = 0;
    }
}
void wld_th_vap_checkCommitAll(T_AccessPoint* pAP) {
    wld_th_vap_vendorData_t* vd = wld_th_vap_getVendorData(pAP);
    ttb_assert_non_null(vd);

    for(uint32_t i = 0; i < WLD_TH_FSM_MAX; i++) {
        ttb_assert_addPrint("Check %u", i);
        if(wld_th_fsm_actions[i].doVapFsmAction != NULL) {
            ttb_assert_int_eq(vd->fsmCommits[i], 1);
        } else {
            ttb_assert_int_eq(vd->fsmCommits[i], 0);
        }
        ttb_assert_removeLastPrint();
    }
}
void wld_th_vap_checkCommitted(T_AccessPoint* pAP, swl_mask_m commitMask) {
    wld_th_vap_vendorData_t* vd = wld_th_vap_getVendorData(pAP);
    ttb_assert_non_null(vd);

    for(uint32_t i = 0; i < WLD_TH_FSM_MAX; i++) {
        ttb_assert_addPrint("Check %u", i);
        if((wld_th_fsm_actions[i].doVapFsmAction != NULL) && SWL_BIT_IS_SET(commitMask, i)) {
            ttb_assert_int_eq(vd->fsmCommits[i], 1);
        } else {
            ttb_assert_int_eq(vd->fsmCommits[i], 0);
        }
        ttb_assert_removeLastPrint();
    }
}
