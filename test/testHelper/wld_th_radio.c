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

#include "wld.h"
#include "wld_th_radio.h"
#include "wld_util.h"
#include "wld_th_mockVendor.h"
#include "test-toolbox/ttb.h"
#include "wld_th_vap.h"
#include "wld_th_ep.h"
#include "wld_radio.h"
#include "wld_chanmgt.h"
#include "swl/swl_common_chanspec.h"




static swl_llist_t s_capList = {NULL, NULL};


static int s_getNextIndex() {
    // this is basically a hidden global, so not very pretty, but ok...
    static int nextIndex = 0;
    int lastNextIndex = nextIndex;
    nextIndex++;
    return lastNextIndex;
}

/** Implements #PFN_WRAD_ADDENDPOINTIF */
int wld_th_radio_vendorCb_addEndpointIf(T_Radio* rad _UNUSED, char* endpoint _UNUSED, int bufsize _UNUSED) {
    return 0;
}

/** Implements #PFN_WRAD_DELENDPOINTIF */
int wld_th_radio_vendorCb_delEndpointIf(T_Radio* radio _UNUSED, char* endpoint _UNUSED) {
    return 0;
}

/** Implements #PFN_WRAD_ADDVAPIF */
int wld_th_radio_vendorCb_addVapIf(T_Radio* rad _UNUSED, char* vap _UNUSED, int bufsize _UNUSED) {
    return 0;
}

int wld_th_mfn_wrad_airtimefairness(T_Radio* rad _UNUSED, int val _UNUSED, int set _UNUSED) { \
    return 0; \
}

int wld_th_mfn_wrad_intelligentAirtime(T_Radio* rad _UNUSED, int val _UNUSED, int set _UNUSED) { \
    return 0; \
}

int wld_th_mfn_wrad_supstd(T_Radio* rad, swl_radioStandard_m radioStandards) {
    rad->operatingStandards = radioStandards;
    return 1;
}

swl_rc_ne wld_th_mfn_wrad_regdomain(T_Radio* pRad, char* val, int bufsize, int set) {
    if(set & SET) {
        const char* countryName = getShortCountryName(pRad->regulatoryDomainIdx);
        if(!swl_str_isEmpty(countryName)) {
            swl_str_copy(pRad->regulatoryDomain, sizeof(pRad->regulatoryDomain), countryName);
            return SWL_RC_OK;
        }
        return SWL_RC_INVALID_PARAM;
    }
    swl_str_copy(val, bufsize, pRad->regulatoryDomain);
    return SWL_RC_OK;
}

swl_channel_t possibleChannels2[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
swl_channel_t possibleChannels5[] = {36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140};
swl_channel_t possibleChannels6[] = {1, 5, 9, 13, 17, 21, 25, 29, 33, 37, 41, 45, 49, 53, 57, 61, 65, 69, 73, 77, 81, 85, 89, 93, 97, 101, 105, 109, 113, 117, 121, 125, 129, 133, 137, 141, 145, 149, 153, 157, 161, 165, 169, 173, 177, 181, 185, 189, 193, 197, 201, 205, 209, 213, 217, 221, 225, 229, 233};


void s_readChanInfo(T_Radio* pRad) {

    for(int i = 0; i < pRad->nrPossibleChannels; i++) {
        swl_chanspec_t cs = swl_chanspec_fromDm(pRad->possibleChannels[i], pRad->operatingChannelBandwidth, pRad->operatingFrequencyBand);
        wld_channel_mark_available_channel(cs);
        if((pRad->operatingFrequencyBand == SWL_FREQ_BAND_EXT_5GHZ) && swl_channel_isDfs(pRad->possibleChannels[i])) {
            wld_channel_mark_radar_req_channel(cs);
            wld_channel_mark_passive_channel(cs);
        }
    }
}

int wld_th_rad_create_hook(T_Radio* pRad) {
    ttb_assert_non_null(pRad);
    ttb_assert_null(pRad->vendorData);
    pRad->vendorData = calloc(1, sizeof(wld_th_rad_vendorData_t));
    return 0;
}
void wld_th_rad_destroy_hook(T_Radio* pRad) {
    ttb_assert_non_null(pRad);
    ttb_assert_non_null(pRad->vendorData);
    free(pRad->vendorData);
    pRad->vendorData = NULL;
}

void wld_th_radio_addCustomCap(wld_th_radCap_t* cap) {
    swl_llist_append(&s_capList, &cap->it);
}

wld_th_radCap_t* s_findCap(const char* radName) {
    swl_llist_iterator_t* it;
    swl_llist_for_each(it, &s_capList) {
        wld_th_radCap_t* cap = swl_llist_item_data(it, wld_th_radCap_t, it);
        if(swl_str_matches(cap->name, radName)) {
            return cap;
        }
    }
    return NULL;
}

int wld_th_rad_vendorCb_poschans(T_Radio* rad, uint8_t* buf _UNUSED, int bufsize _UNUSED) {
    assert_non_null(rad);
    wld_channel_init_channels(rad);
    s_readChanInfo(rad);
    wld_rad_write_possible_channels(rad);
    wld_chanmgt_writeDfsChannels(rad);
    return 0;
}

/** Implements #PFN_WRAD_SUPPORTS */
int wld_th_radio_vendorCb_supports(T_Radio* rad, char* buf _UNUSED, int bufsize _UNUSED) {
    assert_non_null(rad);
    rad->driverCfg.skipSocketIO = true;

    wld_th_radCap_t* cap = s_findCap(rad->Name);
    if(cap != NULL) {
        printf("%s: loading custom cap\n", rad->Name);
        rad->operatingFrequencyBand = cap->operatingFrequencyBand;
        rad->supportedFrequencyBands = cap->supportedFrequencyBands;
        memcpy(rad->possibleChannels, cap->possibleChannels, cap->nrPossibleChannels);
        rad->nrPossibleChannels = cap->nrPossibleChannels;
        rad->maxChannelBandwidth = cap->maxChannelBandwidth;
        rad->operatingChannelBandwidth = cap->operatingChannelBandwidth;
        rad->channel = cap->channel;
        rad->supportedStandards = cap->supportedStandards;
        rad->supportedChannelBandwidth = cap->supportedChannelBandwidth;
        rad->supportedDataTransmitRates = cap->supportedDataTransmitRates;
        memcpy(&rad->cap, &cap->cap, sizeof(wld_radioCap_t));
        wld_th_rad_vendorCb_poschans(rad, NULL, 0);
        return 0;
    }



    // In wld's radio cration, this callback is called, after which it syncs to PCB,
    // after which it commits the PCB object, which fails if the band is invalid.
    // So write the band here, so it's synced, so the commit succeeds.
    printf("SUPP TEST -%s-\n", rad->Name);
    if(swl_str_matches(rad->Name, "wifi2")) {
        rad->operatingFrequencyBand = SWL_FREQ_BAND_EXT_6GHZ;
        rad->supportedFrequencyBands = M_SWL_FREQ_BAND_6GHZ;
        memcpy(rad->possibleChannels, possibleChannels6, SWL_ARRAY_SIZE(possibleChannels6));
        rad->nrPossibleChannels = SWL_ARRAY_SIZE(possibleChannels6);
        rad->maxChannelBandwidth = SWL_BW_160MHZ;
        rad->operatingChannelBandwidth = SWL_RAD_BW_80MHZ;
        rad->supportedChannelBandwidth = ( 1 << (SWL_BW_160MHZ + 1)) - 1;
        rad->channel = 1;
        rad->supportedStandards = M_SWL_RADSTD_AX;
    } else if(swl_str_matches(rad->Name, "wifi1")) {
        rad->operatingFrequencyBand = SWL_FREQ_BAND_EXT_5GHZ;
        rad->supportedFrequencyBands = M_SWL_FREQ_BAND_5GHZ;
        memcpy(rad->possibleChannels, possibleChannels5, SWL_ARRAY_SIZE(possibleChannels5));
        rad->nrPossibleChannels = SWL_ARRAY_SIZE(possibleChannels5);
        rad->maxChannelBandwidth = SWL_BW_160MHZ;
        rad->operatingChannelBandwidth = SWL_RAD_BW_80MHZ;
        rad->supportedChannelBandwidth = ( 1 << (SWL_BW_160MHZ + 1)) - 1;
        rad->channel = 36;
        rad->supportedStandards = M_SWL_RADSTD_A | M_SWL_RADSTD_N | M_SWL_RADSTD_AC | M_SWL_RADSTD_AX;
    } else {
        rad->operatingFrequencyBand = SWL_FREQ_BAND_EXT_2_4GHZ;
        rad->supportedFrequencyBands = M_SWL_FREQ_BAND_2_4GHZ;
        memcpy(rad->possibleChannels, possibleChannels2, SWL_ARRAY_SIZE(possibleChannels2));
        rad->nrPossibleChannels = SWL_ARRAY_SIZE(possibleChannels2);
        rad->maxChannelBandwidth = SWL_BW_40MHZ;
        rad->operatingChannelBandwidth = SWL_RAD_BW_20MHZ;
        rad->supportedChannelBandwidth = ( 1 << (SWL_BW_40MHZ + 1)) - 1;
        rad->channel = 1;
        rad->supportedStandards = M_SWL_RADSTD_B | M_SWL_RADSTD_G | M_SWL_RADSTD_N | M_SWL_RADSTD_AX;
    }

    wld_th_rad_vendorCb_poschans(rad, NULL, 0);


    rad->detailedState = CM_RAD_DOWN;
    wld_rad_updateState(rad, false);
    return 0;
}

T_Radio* wld_th_radio_create(amxb_bus_ctx_t* const bus_ctx, wld_th_mockVendor_t* mockVendor, const char* name) {
    assert_non_null(bus_ctx);
    assert_non_null(mockVendor);
    assert_non_null(name);

    int idx = s_getNextIndex();

    bool ok = ( wld_addRadio(name, wld_th_mockVendor_vendor(mockVendor), idx) >= 0);
    assert_true(ok);

    T_Radio* radio = wld_rad_get_radio(name);
    assert_non_null(radio);

    return radio;
}

FSM_STATE wld_th_wrad_fsm(T_Radio* rad) {
    assert_non_null(rad);
    printf("%s: do commit\n", rad->Name);
    rad->detailedState = rad->enable ? CM_RAD_UP : CM_RAD_DOWN;

    clearAllBitsLongArray(rad->fsmRad.FSM_BitActionArray, FSM_BW);

    T_AccessPoint* pAP;
    wld_rad_forEachAp(pAP, rad) {
        wld_th_vap_doFsmClean(pAP);
    }

    T_EndPoint* pEP;
    wld_rad_forEachEp(pEP, rad) {
        wld_th_ep_doFsmClean(pEP);
    }

    wld_rad_updateState(rad, true);
    return FSM_IDLE;
}

int wld_th_rad_enable(T_Radio* rad, int val, int set) {
    assert_non_null(rad);

    printf("RAD: %d --> %d -  %d\n", rad->enable, val, set);
    if(set & SET) {
        rad->enable = val;
    }
    setBitLongArray(rad->fsmRad.FSM_BitActionArray, FSM_BW, 1);
    return rad->enable;
}

void wld_th_rad_setRadEnable(T_Radio* rad, bool enable, bool commit) {
    assert_non_null(rad);
    if(commit) {
        swl_typeUInt8_commitObjectParam(rad->pBus, "Enable", enable);
        ttb_mockTimer_goToFutureMs(10);
    } else {
        swl_typeUInt8_toObjectParam(rad->pBus, "Enable", enable);
    }
}

int wld_th_rad_startScan(T_Radio* rad) {
    assert_non_null(rad);
    return SWL_RC_OK;
}

int wld_th_rad_getScanResults(T_Radio* pRad, wld_scanResults_t* results) {
    assert_non_null(pRad);
    assert_non_null(results);
    amxc_llist_for_each(it, &pRad->scanState.lastScanResults.ssids) {
        wld_scanResultSSID_t* pResult = amxc_container_of(it, wld_scanResultSSID_t, it);
        wld_scanResultSSID_t* pCopy = calloc(1, sizeof(wld_scanResultSSID_t));
        assert_non_null(pCopy);
        memcpy(pCopy, pResult, sizeof(*pCopy));
        amxc_llist_init(&pCopy->vendorIEs);

        amxc_llist_for_each(vendorIt, &pResult->vendorIEs) {
            wld_vendorIe_t* vendorIE = amxc_llist_it_get_data(vendorIt, wld_vendorIe_t, it);
            wld_vendorIe_t* newVendorIE = calloc(1, sizeof(wld_vendorIe_t));
            snprintf(newVendorIE->oui, SWL_OUI_STR_LEN, "%s", vendorIE->oui);
            snprintf(newVendorIE->data, strlen(vendorIE->data) + 1, "%s", vendorIE->data);
            amxc_llist_append(&pCopy->vendorIEs, &newVendorIE->it);
        }




        amxc_llist_it_init(&pCopy->it);
        amxc_llist_append(&results->ssids, &pCopy->it);
    }
    return SWL_RC_OK;
}

int wld_th_rad_setChanspec(T_Radio* rad, bool direct) {
    if(!direct) {
        return SWL_RC_OK;
    }
    wld_chanmgt_reportCurrentChanspec(rad, rad->targetChanspec.chanspec, rad->targetChanspec.reason);
    return SWL_RC_OK;
}

wld_th_rad_vendorData_t* wld_th_rad_getVendorData(T_Radio* rad) {
    if(rad == NULL) {
        return NULL;
    }
    return (wld_th_rad_vendorData_t*) rad->vendorData;
}

void wld_th_rad_clearCommits(T_Radio* rad) {
    wld_th_rad_vendorData_t* vd = wld_th_rad_getVendorData(rad);
    ttb_assert_non_null(vd);

    for(uint32_t i = 0; i < WLD_TH_FSM_MAX; i++) {
        vd->fsmCommits[i] = 0;
    }
}
void wld_th_rad_checkCommitAll(T_Radio* rad) {
    wld_th_rad_vendorData_t* vd = wld_th_rad_getVendorData(rad);
    ttb_assert_non_null(vd);

    for(uint32_t i = 0; i < WLD_TH_FSM_MAX; i++) {
        ttb_assert_addPrint("Check %u", i);
        if(wld_th_fsm_actions[i].doRadFsmAction != NULL) {
            ttb_assert_int_eq(vd->fsmCommits[i], 1);
        } else {
            ttb_assert_int_eq(vd->fsmCommits[i], 0);
        }
        ttb_assert_removeLastPrint();

    }
}
void wld_th_rad_checkCommitted(T_Radio* rad, swl_mask_m commitMask) {
    wld_th_rad_vendorData_t* vd = wld_th_rad_getVendorData(rad);
    ttb_assert_non_null(vd);

    for(uint32_t i = 0; i < WLD_TH_FSM_MAX; i++) {
        ttb_assert_addPrint("Check %u", i);
        if((wld_th_fsm_actions[i].doRadFsmAction != NULL) && SWL_BIT_IS_SET(commitMask, i)) {
            ttb_assert_int_eq(vd->fsmCommits[i], 1);
        } else {
            ttb_assert_int_eq(vd->fsmCommits[i], 0);
        }
        ttb_assert_removeLastPrint();

    }
}

