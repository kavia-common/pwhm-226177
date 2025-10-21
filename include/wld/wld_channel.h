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

#ifndef WLD_CHANNEL_H_
#define WLD_CHANNEL_H_

#include "wld.h"
#include "wld_channel_types.h"

#define WLD_CHAN_MAX_NR_CHANS_IN_USE 16
#define WLD_CHAN_MAX_NR_INTERF_CHAN 11

#define NR_INTERFERENCE_ELEMENT_24_20 7
extern const wld_chan_interference_t interferenceMatrix_24_20Mhz[NR_INTERFERENCE_ELEMENT_24_20];

#define NR_INTERFERENCE_ELEMENT_24_40 11
extern const wld_chan_interference_t interferenceMatrix_24_40Mhz[NR_INTERFERENCE_ELEMENT_24_40];

//Getters
bool wld_channel_is_dfs_band(int channel, swl_bandwidth_e bandwidth);
uint32_t wld_channel_getDfsPercentage(int channel, swl_bandwidth_e bandwidth);
bool wld_channel_is_usable(swl_chanspec_t chanspec);
bool wld_channel_is_band_usable(swl_chanspec_t chanspec);
bool wld_channel_is_band_passive(swl_chanspec_t chanspec);

bool wld_channel_is_chan_in_band(swl_chanspec_t chanspec, int testChannel);
bool wld_channel_is_available(swl_chanspec_t chanspec);
bool wld_channel_is_band_available(swl_chanspec_t chanspec);
bool wld_channel_is_radar_detected(swl_chanspec_t chanspec);
bool wld_channel_is_band_radar_detected(swl_chanspec_t chanspec);
int wld_channel_isValidClearChannel(T_Radio* pRad, int channel, swl_bandwidth_e operatingChannelBandwidth);

uint32_t wld_channel_getFrequencyOfChannel(swl_chanspec_t chanspec);
uint32_t wld_channel_getBandwidthValFromEnum(swl_bandwidth_e radioBw);
swl_bandwidth_e wld_channel_getBandwidthEnumFromVal(uint32_t val);
int wld_channel_get_center_channel(swl_chanspec_t chanspec);
int wld_channel_get_channel_to_clear(T_Radio* pRad, swl_bandwidth_e operatingChannelBandwidth);
void wld_channel_set_channel_clear_time(int channel, uint32_t time);
int wld_channel_get_channel_clear_time(int channel);
int wld_channel_get_band_clear_time(swl_chanspec_t chanspec);
void wld_channel_set_channel_bg_clear_time(int channel, uint32_t time);
int wld_channel_get_channel_bg_clear_time(int channel);
int wld_channel_get_band_bg_clear_time(swl_chanspec_t chanspec);

bool wld_channel_is_long_wait(swl_chanspec_t chanspec); //Deprecated
bool wld_channel_is_long_wait_band(swl_chanspec_t chanspec);
bool wld_channel_is_long_wait_channel(swl_chanspec_t chanspec);

bool wld_channel_areAdjacent(swl_chanspec_t chanspec1, swl_chanspec_t chanspec2);
bool wld_channel_hasChannelWidthCovered(swl_chanspec_t chspec, swl_bandwidth_e chW);
wld_channel_extensionPos_e wld_channel_getExtensionChannel(swl_chanspec_t chspec, wld_channel_extensionPos_e currExtChanPos);

//Setters

void wld_channel_init_channels(T_Radio* pRad);
void wld_channel_cleanAll();
void wld_channel_clear_flags(T_Radio* rad);
size_t wld_channel_get_cleared_channels(T_Radio* rad, swl_channel_t* list, size_t list_size);
size_t wld_channel_get_radartriggered_channels(T_Radio* pRad, swl_channel_t* list, size_t list_size);

void wld_channel_mark_available_channel(swl_chanspec_t chanspec);
void wld_channel_mark_unavailable_channel(swl_chanspec_t chanspec);
void wld_channel_mark_radar_req_channel(swl_chanspec_t chanspec);
void wld_channel_mark_passive_channel(swl_chanspec_t chanspec);
void wld_channel_clear_passive_channel(swl_chanspec_t chanspec);
void wld_channel_mark_radar_detected_channel(swl_chanspec_t chanspec);
void wld_channel_clear_radar_detected_channel(swl_chanspec_t chanspec);

void wld_channel_mark_available_band(swl_chanspec_t chanspec);
void wld_channel_mark_unavailable_band(swl_chanspec_t chanspec);
void wld_channel_mark_radar_req_band(swl_chanspec_t chanspec);
void wld_channel_mark_passive_band(swl_chanspec_t chanspec);
void wld_channel_clear_passive_band(swl_chanspec_t chanspec);
void wld_channel_mark_radar_detected_band(swl_chanspec_t chanspec);
void wld_channel_clear_radar_detected_band(swl_chanspec_t chanspec);

void wld_channel_do_for_each_channel(T_Radio* rad, void* data, void (* channel_function)(T_Radio* rad, void* data, int channel));

//Debug
void wld_channel_print(T_Radio* rad);

#endif /* WLD_CHANNEL_H_ */
