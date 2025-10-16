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
#ifndef __WLD_TESTHELPER_RADIO_H__
#define __WLD_TESTHELPER_RADIO_H__
#include "wld_types.h"
#include "wld_th_types.h"
#include "swl/swl_common_radioStandards.h"

#include <amxc/amxc_macros.h>
#include <amxc/amxc.h>
#include <amxp/amxp.h>
#include <amxd/amxd_dm.h>
#include <amxd/amxd_object.h>
#include <amxd/amxd_path.h>
#include <amxd/amxd_object_event.h>
#include <amxo/amxo.h>
#include <amxo/amxo_save.h>
#include <amxb/amxb.h>

#include "wld_th_fsm.h"

typedef struct {
    swl_llist_iterator_t it;
    char name[32];
    swl_channel_t possibleChannels[WLD_MAX_POSSIBLE_CHANNELS];
    uint32_t nrPossibleChannels;
    swl_freqBandExt_m supportedFrequencyBands;
    swl_freqBandExt_e operatingFrequencyBand;
    swl_channel_t startChannel;
    swl_bandwidth_e maxChannelBandwidth;
    swl_radBw_e operatingChannelBandwidth;
    swl_channel_t channel;
    swl_radioStandard_m supportedStandards;
    wld_radioCap_t cap;
    swl_radBw_m supportedChannelBandwidth;
    swl_mcs_legacyIndex_m supportedDataTransmitRates;
} wld_th_radCap_t;


typedef struct {
    bool csiEnable;
    swl_macChar_t macAddr;
    uint32_t monitorInterval;
    uint32_t maxClientsNbrs;
    uint32_t maxMonitorInterval;
    wld_csiState_t csimonState;
} wld_th_rad_sensing_vendorData_t;

typedef struct {
    wld_th_rad_sensing_vendorData_t sensingData;
    uint32_t fsmCommits[WLD_TH_FSM_MAX];
} wld_th_rad_vendorData_t;

T_Radio* wld_th_radio_create(amxb_bus_ctx_t* const bus_ctx, wld_th_mockVendor_t* mockVendor, const char* name);

int wld_th_radio_vendorCb_addEndpointIf(T_Radio* rad, char* endpoint, int bufsize);
int wld_th_radio_vendorCb_delEndpointIf(T_Radio* radio, char* endpoint);
int wld_th_radio_vendorCb_addVapIf(T_Radio* rad, char* vap, int bufsize);
int wld_th_rad_create_hook(T_Radio* pRad);
void wld_th_rad_destroy_hook(T_Radio* pRad);
void wld_th_radio_addCustomCap(wld_th_radCap_t* cap);
int wld_th_radio_vendorCb_supports(T_Radio* rad, char* buf, int bufsize);
int wld_th_mfn_wrad_airtimefairness(T_Radio* rad, int val, int set);
int wld_th_mfn_wrad_intelligentAirtime(T_Radio* rad, int val, int set);
int wld_th_mfn_wrad_supstd(T_Radio* rad, swl_radioStandard_m radioStandards);
swl_rc_ne wld_th_mfn_wrad_regdomain(T_Radio* pRad, char* val, int bufsize, int set);
FSM_STATE wld_th_wrad_fsm(T_Radio* rad);
int wld_th_rad_enable(T_Radio* rad, int val, int set);
int wld_th_rad_vendorCb_poschans(T_Radio* pRad, uint8_t* buf, int bufsize);
int wld_th_rad_miscHasSupport(T_Radio* pRad, T_AccessPoint* pAp, char* buf, int bufsize);


void wld_th_rad_setRadEnable(T_Radio* rad, bool enable, bool commit);
int wld_th_rad_startScan(T_Radio* rad);
int wld_th_rad_getScanResults(T_Radio* rad, wld_scanResults_t* results);
int wld_th_rad_setChanspec(T_Radio* rad, bool direct);

wld_th_rad_vendorData_t* wld_th_rad_getVendorData(T_Radio* rad);
void wld_th_rad_clearCommits(T_Radio* rad);
void wld_th_rad_checkCommitAll(T_Radio* rad);
void wld_th_rad_checkCommitted(T_Radio* rad, swl_mask_m commitMask);
#endif
