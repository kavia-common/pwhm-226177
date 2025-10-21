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


#include "wld_channel.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <swl/swl_common.h>
#include "wld_channel_lib.h"
#include "wld_radio.h"

#include <debug/sahtrace.h>

typedef struct {
    swl_bandwidth_e wldBw;
    int chanWidth;
} chanwidthData_t;

const chanwidthData_t nr_channels_ahead[] = {
    {SWL_BW_AUTO, 1},
    {SWL_BW_20MHZ, 1},
    {SWL_BW_40MHZ, 2},
    {SWL_BW_80MHZ, 4},
    {SWL_BW_160MHZ, 8},
    {SWL_BW_320MHZ, 16}
};

SWL_ASSERT_STATIC(SWL_ARRAY_SIZE(nr_channels_ahead) == SWL_BW_RAD_MAX, "nr_channels_ahead not correctly defined");

#define ME "chanInf"

/*
 * Operating class table for regulatory region to operating class conversion.
 */
struct operating_class_table {
    uint8_t index;
    uint8_t bandwidth;
    uint8_t chan_set[64];
};

/**
 * Returns whether a given band contains DFS channels.
 */
bool wld_channel_is_dfs_band(int channel, swl_bandwidth_e bandwidth) {
    if(swl_channel_isDfs(channel)) {
        return true;
    }
    if(bandwidth == SWL_BW_160MHZ) {
        return true;
    }

    return false;
}

/**
 * Return by which percentage a band is dfs.
 */
uint32_t wld_channel_getDfsPercentage(int channel, swl_bandwidth_e bandwidth) {
    if(bandwidth <= SWL_BW_80MHZ) {
        return swl_channel_isDfs(channel) ? 100 : 0;
    }
    if(bandwidth == SWL_BW_160MHZ) {
        return swl_channel_isHighPower(channel) ? 100 : 50;
    }
    return 0;
}

/**
 * Get the frequency from a channel (in Hertz).
 */
uint32_t wld_channel_getFrequencyOfChannel(swl_chanspec_t chanspec) {
    uint32_t freqMHz = 0;
    swl_chanspec_channelToMHz(&chanspec, &freqMHz);
    return freqMHz;
}

/**
 * Get the center channel of a band
 * OBSOLETE: please use swl_chanspec_getCentreChannel
 */
int wld_channel_get_center_channel(swl_chanspec_t chanspec) {
    return swl_chanspec_getCentreChannel(&chanspec);
}

/**
 * Run action for every channel of chanspec.
 */
typedef void (* chanActionCb_f)(swl_chanspec_t chanspec, swl_channel_t chan, void* data);

static void s_runPerBandChan(swl_chanspec_t chanspec, chanActionCb_f fct, void* data) {
    swl_channel_t temp_channel = swl_chanspec_getBaseChannel(&chanspec);
    for(uint8_t i = 0; i < swl_chanspec_getNrChannelsInBand(&chanspec); i++) {
        SWL_CALL(fct, chanspec, temp_channel, data);
        temp_channel += CHANNEL_INCREMENT;
    }
}

/**
 * Returns the time in milliseconds that it takes to clear this band.
 */
static void s_getChanCacTime(swl_chanspec_t chanspec _UNUSED, swl_channel_t chan, void* data) {
    ASSERTS_NOT_NULL(data, , ME, "NULL");
    int* pTime = (int*) data;
    int temp_time = wld_channel_get_channel_clear_time(chan);
    if(temp_time > *pTime) {
        *pTime = temp_time;
    }
}

int wld_channel_get_band_clear_time(swl_chanspec_t chanspec) {
    if(chanspec.band != SWL_FREQ_BAND_EXT_5GHZ) {
        return 0;
    }
    int time = 0;
    s_runPerBandChan(chanspec, s_getChanCacTime, &time);
    return time;
}

/**
 * Returns the time in milliseconds that it takes to clear this band, in background.
 */
static void s_getChanBgCacTime(swl_chanspec_t chanspec _UNUSED, swl_channel_t chan, void* data) {
    ASSERTS_NOT_NULL(data, , ME, "NULL");
    int* pTime = (int*) data;
    int temp_time = wld_channel_get_channel_bg_clear_time(chan);
    if(temp_time > *pTime) {
        *pTime = temp_time;
    }
}

int wld_channel_get_band_bg_clear_time(swl_chanspec_t chanspec) {
    if(chanspec.band != SWL_FREQ_BAND_EXT_5GHZ) {
        return 0;
    }
    int time = 0;
    s_runPerBandChan(chanspec, s_getChanBgCacTime, &time);
    return time;
}

//Deprecated
bool wld_channel_is_long_wait(swl_chanspec_t chanspec) {
    return wld_channel_is_long_wait_band(chanspec);
}

/**
 * Returns true if a band is a long wait channel.
 * A band is a long wait band if any channel is the band is in the extend cleartime band.
 */
bool wld_channel_is_long_wait_band(swl_chanspec_t chanspec) {
    if(chanspec.band != SWL_FREQ_BAND_EXT_5GHZ) {
        return false;
    }

    int nrChannels = swl_chanspec_getNrChannelsInBand(&chanspec);
    swl_chanspec_t tmpChanspec;
    memcpy(&tmpChanspec, &chanspec, sizeof(swl_chanspec_t));
    tmpChanspec.channel = swl_chanspec_getBaseChannel(&chanspec);

    int i = 0;
    for(i = 0; i < nrChannels; i++) {

        if(wld_channel_is_long_wait_channel(tmpChanspec)) {
            return true;
        } else {
            tmpChanspec.channel += CHANNEL_INCREMENT;
        }
    }
    return false;
}

bool wld_channel_is_long_wait_channel(swl_chanspec_t chanspec) {
    if(chanspec.band != SWL_FREQ_BAND_EXT_5GHZ) {
        return false;
    }
    return ((chanspec.channel >= WLD_CHAN_START_EXTENDED_CLEARTIME)
            && (chanspec.channel <= WLD_CHAN_END_EXTENDED_CLEARTIME));
}



/**
 * Returns true if the testChannel is in the band determing by the input channel and bandwidth.
 * Returns false otherwise.
 * OBSOLETE: please use swl_channel_isInChanspec
 */
bool wld_channel_is_chan_in_band(swl_chanspec_t chanspec, int testChannel) {
    return swl_channel_isInChanspec(&chanspec, testChannel);
}

bool wld_channel_hasChannelWidthCovered(swl_chanspec_t chspec, swl_bandwidth_e chW) {
    return (swl_chanspec_bwToInt(chspec.bandwidth) >= swl_chanspec_bwToInt(chW));
}

wld_channel_extensionPos_e wld_channel_getExtensionChannel(swl_chanspec_t chspec, wld_channel_extensionPos_e currExtChanPos) {
    ASSERTI_TRUE(wld_channel_hasChannelWidthCovered(chspec, SWL_BW_40MHZ), WLD_CHANNEL_EXTENTION_POS_NONE, ME, "not supported");
    if(chspec.band != SWL_FREQ_BAND_EXT_2_4GHZ) {
        if((chspec.channel / 4) % 2) {
            return WLD_CHANNEL_EXTENTION_POS_ABOVE;
        }
        return WLD_CHANNEL_EXTENTION_POS_BELOW;
    }
    if(currExtChanPos == WLD_CHANNEL_EXTENTION_POS_AUTO) {
        if(chspec.channel < 7) {
            return WLD_CHANNEL_EXTENTION_POS_ABOVE;
        }
        return WLD_CHANNEL_EXTENTION_POS_BELOW;
    }
    return currExtChanPos;
}
