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

#include "wld_th_mockVendor.h"
#include "wld_th_radio.h"
#include "wld_th_vap.h"
#include "wld_th_ep.h"
#include "swl/swl_common.h"
#include "wld_eventing.h"
#include "Plugin/wifiGen_vap.h"
#include "wld_th_sensing.h"
#include "wld_th_fsm.h"

static T_CWLD_FUNC_TABLE s_functionTable = {
    .mfn_misc_has_support = wld_th_rad_miscHasSupport,
    .mfn_wrad_create_hook = wld_th_rad_create_hook,
    .mfn_wrad_destroy_hook = wld_th_rad_destroy_hook,
    .mfn_wrad_supports = wld_th_radio_vendorCb_supports,
    .mfn_wrad_poschans = wld_th_rad_vendorCb_poschans,
    .mfn_wrad_addendpointif = wld_th_radio_vendorCb_addEndpointIf,
    .mfn_wrad_delendpointif = wld_th_radio_vendorCb_delEndpointIf,
    .mfn_wrad_addvapif = wld_th_vap_vendorCb_addVapIf,
    .mfn_wrad_fsm = wld_th_wrad_fsm,
    .mfn_wrad_enable = wld_th_rad_enable,
    .mfn_wrad_start_scan = wld_th_rad_startScan,
    .mfn_wrad_scan_results = wld_th_rad_getScanResults,
    .mfn_wrad_airtimefairness = wld_th_mfn_wrad_airtimefairness,
    .mfn_wrad_intelligentAirtime = wld_th_mfn_wrad_intelligentAirtime,
    .mfn_wrad_supstd = wld_th_mfn_wrad_supstd,
    .mfn_wrad_regdomain = wld_th_mfn_wrad_regdomain,
    .mfn_wrad_setChanspec = wld_th_rad_setChanspec,
    .mfn_wrad_sensing_cmd = wld_th_rad_sensing_cmd,
    .mfn_wrad_sensing_addClient = wld_th_rad_sensing_addClient,
    .mfn_wrad_sensing_delClient = wld_th_rad_sensing_delClient,
    .mfn_wrad_sensing_csiStats = wld_th_rad_sensing_csiStats,
    .mfn_wrad_sensing_resetStats = wld_th_rad_sensing_resetStats,

    .mfn_wvap_create_hook = wld_th_vap_createHook,
    .mfn_wvap_destroy_hook = wld_th_vap_destroyHook,
    .mfn_wvap_enable = wld_th_vap_enable,
    .mfn_wvap_get_station_stats = wld_th_vap_getStationStats,
    .mfn_wvap_status = wld_th_vap_status,
    .mfn_wvap_ssid = wld_th_vap_ssid,
    .mfn_wendpoint_stats = wld_th_ep_getStats,
};

struct wld_th_mockVendor {
    char name[32];
    vendor_t* vendor;
};

static int32_t s_numberOfVendors = 0;

const char* wld_th_mockVendor_name(wld_th_mockVendor_t* mockVendor) {
    return mockVendor->name;
}

wld_th_mockVendor_t* wld_th_mockVendor_create(const char* name) {

    wld_th_mockVendor_t* mockVendor = calloc(1, sizeof(wld_th_mockVendor_t));

    swl_str_copy(mockVendor->name, sizeof(mockVendor->name), name);

    s_numberOfVendors++;

    return mockVendor;
}

vendor_t* wld_th_mockVendor_register(wld_th_mockVendor_t* mockVendor) {

    vendor_t* vendor = wld_registerVendor(wld_th_mockVendor_name(mockVendor), &s_functionTable);
    mockVendor->vendor = vendor;
    whm_th_fsm_doInit(vendor);
    return vendor;
}

vendor_t* wld_th_mockVendor_vendor(wld_th_mockVendor_t* mockVendor) {
    return mockVendor->vendor;
}


void wld_mockVendor_destroy(wld_th_mockVendor_t* mockVendor) {
    if((gWld_queue_rad_onStatusChange == NULL) || !amxc_llist_is_empty(&gWld_queue_rad_onStatusChange->subscribers)) {
        printf("You must first call wld_cleanup() because that function still uses vendor\n");
    }
    if(mockVendor->vendor != NULL) {
        wld_unregisterVendor(mockVendor->vendor);
    }

    free(mockVendor);

    s_numberOfVendors--;

}

bool wld_mockVendor_areThereStillVendors() {
    return s_numberOfVendors > 0;
}
