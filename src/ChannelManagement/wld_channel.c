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

#include "wld_radio.h"
#include "wld_util.h"
#include "wld_channel_lib.h"
#define ME "chan"

/**
 * This module is designed to handle channel information with regards to availability.
 *
 * This module must be initialized by calling wld_channel_init_channels() with a radio object that
 * has a valid set of possible channels. The possible channels list will be used to create the internal list of channels.
 *
 * Once init is done, you can start marking and unmarking channels. Note that this module does not mark DFS channels
 * by itself. It is up to the module user to use wld_channel_mark_radar_req to set the fact that a radar is required, and
 * wld_channel_mark_passive to mark that a channel is currently unavailable for use untill scanned.
 *
 * wld_channel_is_usable will return whether or not a channel can be used to at the given time.
 *
 * wld_channel_get_channel_to_clear can be used to determine which channel still need to be cleared.
 */


////////////////////////
// Internal flags and structures
////////////////////////

/**
 * Flag to  init channel
 */
#define WLD_CHAN_INIT 0x00
/**
 * Flag declaring the channel available. However, it might not be usable due to radar requirements
 */
#define WLD_CHAN_AVAILABLE_FLAG 0x01
/**
 * Flag declaring the channel requires radar clearing (DFS Channel).
 * It does not say whether or not it has been cleared, just that this a channel that must be cleared
 * before usage.
 */
#define WLD_CHAN_RADAR_REQUIRED 0x02
/**
 * Flag indicating this channel is passive, so has not been cleared yet. It is thus unusable.
 */
#define WLD_CHAN_PASSIVE 0x04
/**
 * Flag indicating that a radar is detected on this channel, and thus currently unusable.
 */
#define WLD_CHAN_RADAR_DETECTED 0x08


typedef struct {
    int channel;
    uint32_t flags;
    uint32_t clearTime;   // FG DFS clear time in milliseconds
    uint32_t bgClearTime; // BG DFS clear time in milliseconds
} wld_channel_data;

typedef struct {
    wld_channel_data* channels;
    uint32_t nr_channels;
} wld_band_data;


/**
 * Static array with a list of wld_band_data structures to return by get_band.
 * SWL_FREQ_BAND_MAX is the actual size of the band enum,
 * value of it automatically changes when elements are added/removed to/from the enum.
 */
static wld_band_data wldBandDataList[SWL_FREQ_BAND_MAX] = {
    {.channels = NULL, .nr_channels = 0},
    {.channels = NULL, .nr_channels = 0},
    {.channels = NULL, .nr_channels = 0},
};



////////////////////////////////////////////
// Internal methods
////////////////////////////////////////////
//Getters

static wld_channel_data* get_channel_data(int channel, swl_freqBandExt_e freqBand);

static bool is_flag_set_on_channel(int channel, swl_freqBandExt_e freqBand, uint32_t flag);
static bool is_flag_set_in_band_always(swl_chanspec_t chanspec, uint32_t flag);
static bool is_flag_set_in_band_anywhere(swl_chanspec_t chanspec, uint32_t flag);

//setters
static void chandata_remove_flag(wld_channel_data* data, uint32_t flag);
static void chandata_add_flag(wld_channel_data* data, uint32_t flag);

static void band_remove_flag(swl_chanspec_t chanspec, uint32_t flag);
static void band_add_flag(swl_chanspec_t chanspec, uint32_t flag);

static void chan_add_flag(int channel, swl_freqBandExt_e freqBand, uint32_t flag);
static void chan_remove_flag(int channel, swl_freqBandExt_e freqBand, uint32_t flag);


//Impl Getters
/*
 * Returns the equivalent wld_band_data upon the passed swl_freqBandExt_e type argument.
 */
static wld_band_data* get_band(swl_freqBandExt_e freqBand) {
    bool found;
    swl_freqBand_e tmpFreq = swl_chanspec_freqBandExtToFreqBand(freqBand, SWL_FREQ_BAND_2_4GHZ, &found);
    ASSERT_TRUE(found, NULL, ME, "illegal Freq band %u", freqBand);
    return &wldBandDataList[tmpFreq];
}

static wld_channel_data* get_channel_data(int channel, swl_freqBandExt_e freqBand) {
    wld_band_data* band = get_band(freqBand);
    ASSERTS_NOT_NULL(band, NULL, ME, "null freqband");
    wld_channel_data* chan_data;
    unsigned int i = 0;
    for(i = 0; i < band->nr_channels; i++) {
        chan_data = &(band->channels[i]);
        if(chan_data->channel == channel) {
            return chan_data;
        }
    }
    return NULL;
}

static bool is_flag_set_on_channel(int channel, swl_freqBandExt_e freqBand, uint32_t flag) {
    wld_channel_data* data = get_channel_data(channel, freqBand);
    ASSERTS_NOT_NULL(data, NULL, ME, "null freqband");
    return (data->flags & flag);
}

static bool is_flag_set_in_band_always(swl_chanspec_t chanspec, uint32_t flag) {
    swl_channel_t chan_list[WLD_CHAN_MAX_NR_CHANS_IN_USE];
    int nr_channels = swl_chanspec_getChannelsInChanspec(&chanspec, chan_list, SWL_ARRAY_SIZE(chan_list));
    int i = 0;
    for(i = 0; i < nr_channels; i++) {
        if(!is_flag_set_on_channel(chan_list[i], chanspec.band, flag)) {
            return false;
        }
    }
    return true;
}

static bool is_flag_set_in_band_anywhere(swl_chanspec_t chanspec, uint32_t flag) {
    swl_channel_t chan_list[WLD_CHAN_MAX_NR_CHANS_IN_USE];
    int nr_channels = swl_chanspec_getChannelsInChanspec(&chanspec, chan_list, SWL_ARRAY_SIZE(chan_list));
    int i = 0;
    for(i = 0; i < nr_channels; i++) {
        if(is_flag_set_on_channel(chan_list[i], chanspec.band, flag)) {
            return true;
        }
    }
    return false;
}

//Impl Setters

static inline void chandata_remove_flag(wld_channel_data* data, uint32_t flag) {
    data->flags = data->flags & (~flag);
}

static inline bool chandata_has_flag(wld_channel_data* data, uint32_t flag) {
    return (data->flags & flag) > 0;
}

static inline void chandata_add_flag(wld_channel_data* data, uint32_t flag) {
    /* Do not mark CAC needed or RADAR detected flags on non-DFS channels */
    if((!chandata_has_flag(data, WLD_CHAN_RADAR_REQUIRED)) &&
       ((flag == WLD_CHAN_PASSIVE) || (flag == WLD_CHAN_RADAR_DETECTED))) {
        return;
    }
    data->flags = data->flags | flag;
}

static void band_remove_flag(swl_chanspec_t chanspec, uint32_t flag) {
    swl_channel_t chan_list[WLD_CHAN_MAX_NR_CHANS_IN_USE];
    int nr_channels = swl_chanspec_getChannelsInChanspec(&chanspec, chan_list, SWL_ARRAY_SIZE(chan_list));
    int i;
    for(i = 0; i < nr_channels; i++) {
        wld_channel_data* channel = get_channel_data(chan_list[i], chanspec.band);
        if(channel != NULL) {
            chandata_remove_flag(channel, flag);
        }
    }
}

static void band_add_flag(swl_chanspec_t chanspec, uint32_t flag) {
    swl_channel_t chan_list[WLD_CHAN_MAX_NR_CHANS_IN_USE];
    int nr_channels = swl_chanspec_getChannelsInChanspec(&chanspec, chan_list, SWL_ARRAY_SIZE(chan_list));
    int i;
    for(i = 0; i < nr_channels; i++) {
        wld_channel_data* channel = get_channel_data(chan_list[i], chanspec.band);
        if(channel != NULL) {
            chandata_add_flag(channel, flag);
        }
    }
}

/**
 * mark a given channel with the given flag if possible
 */
static void chan_add_flag(int channel, swl_freqBandExt_e freqBand, uint32_t flag) {
    wld_channel_data* channel_info = get_channel_data(channel, freqBand);
    if(channel_info != NULL) {
        chandata_add_flag(channel_info, flag);
    }
}

/**
 * remove the given flag from the channel if possible
 */
static void chan_remove_flag(int channel, swl_freqBandExt_e freqBand, uint32_t flag) {
    wld_channel_data* channel_info = get_channel_data(channel, freqBand);
    if(channel_info != NULL) {
        chandata_remove_flag(channel_info, flag);
    }
}

/**
 * set clear time for given channel if possible
 */
static void s_chanSetClearTime(int channel, swl_freqBandExt_e freqBand, uint32_t time) {
    wld_channel_data* channel_info = get_channel_data(channel, freqBand);
    ASSERTS_NOT_NULL(channel_info, , ME, "NULL");
    channel_info->clearTime = time;
}

/**
 * get clear time for given channel if possible
 */
static int s_chanGetClearTime(int channel, swl_freqBandExt_e freqBand) {
    wld_channel_data* channel_info = get_channel_data(channel, freqBand);
    ASSERTS_NOT_NULL(channel_info, 0, ME, "NULL");
    return channel_info->clearTime;
}

/**
 * set clear time for given channel if possible
 */
static void s_chanSetBgClearTime(int channel, swl_freqBandExt_e freqBand, uint32_t time) {
    wld_channel_data* channel_info = get_channel_data(channel, freqBand);
    ASSERTS_NOT_NULL(channel_info, , ME, "NULL");
    channel_info->bgClearTime = time;
}

/**
 * get clear time for given channel if possible
 */
static int s_chanGetBgClearTime(int channel, swl_freqBandExt_e freqBand) {
    wld_channel_data* channel_info = get_channel_data(channel, freqBand);
    ASSERTS_NOT_NULL(channel_info, 0, ME, "NULL");
    return channel_info->bgClearTime;
}

//////////////////////////////////////////////////
// External methods
//////////////////////////////////////////////////





/**
 * Initialize all the channels on a given band. This will remove all current flags!!
 */
void wld_channel_init_channels(T_Radio* rad) {
    unsigned int i = 0;
    wld_band_data* band = get_band(rad->operatingFrequencyBand);
    ASSERT_NOT_NULL(band, , ME, "null freqband");
    if(band->channels != NULL) {
        free(band->channels);
        band->channels = NULL;
    }
    band->channels = calloc(rad->nrPossibleChannels, sizeof(wld_channel_data));
    ASSERT_NOT_NULL(band->channels, , ME, "NULL");
    band->nr_channels = rad->nrPossibleChannels;
    for(i = 0; i < band->nr_channels; i++) {
        band->channels[i].channel = rad->possibleChannels[i];
        band->channels[i].flags = WLD_CHAN_INIT;
        band->channels[i].clearTime = 0;
        band->channels[i].bgClearTime = 0;
    }
}

void wld_channel_cleanAll() {
    for(int i = 0; i < SWL_FREQ_BAND_MAX; i++) {
        free(wldBandDataList[i].channels);
        wldBandDataList[i].channels = NULL;
        wldBandDataList[i].nr_channels = 0;
    }

}

/**
 * clear all flags of the radio band of the given radio.
 */
void wld_channel_clear_flags(T_Radio* rad) {
    wld_band_data* band = get_band(rad->operatingFrequencyBand);
    ASSERTS_NOT_NULL(band, , ME, "null freqband");
    ASSERTS_NOT_NULL(band->channels, , ME, "null channel");
    for(uint32_t i = 0; i < band->nr_channels; i++) {
        band->channels[i].flags = WLD_CHAN_INIT;
    }
}


size_t wld_channel_get_cleared_channels(T_Radio* pRad, swl_channel_t* list, size_t list_size) {
    wld_band_data* band = get_band(pRad->operatingFrequencyBand);
    ASSERTS_NOT_NULL(band, 0, ME, "null freqband");
    size_t i = 0;
    size_t nr_cleared_channels = 0;
    wld_channel_data* channel;
    for(i = 0; (i < band->nr_channels) && (nr_cleared_channels < list_size); i++) {
        channel = &(band->channels[i]);
        if(chandata_has_flag(channel, WLD_CHAN_RADAR_REQUIRED)
           && !chandata_has_flag(channel, WLD_CHAN_PASSIVE)
           && !chandata_has_flag(channel, WLD_CHAN_RADAR_DETECTED)) {
            list[nr_cleared_channels] = channel->channel;
            nr_cleared_channels++;
        }
    }
    if(i < band->nr_channels) {
        SAH_TRACEZ_ERROR(ME, "Performed getCleared channels with too short list %s", pRad->Name);
    }
    return nr_cleared_channels;
}


size_t wld_channel_get_radartriggered_channels(T_Radio* pRad, swl_channel_t* list, size_t list_size) {
    wld_band_data* band = get_band(pRad->operatingFrequencyBand);
    ASSERTS_NOT_NULL(band, 0, ME, "null freqband");
    size_t i = 0;
    size_t nr_cleared_channels = 0;
    wld_channel_data* channel;
    for(i = 0; (i < band->nr_channels) && (nr_cleared_channels < list_size); i++) {
        ASSERTS_NOT_NULL(band->channels, 0, ME, "null channel list");
        channel = &(band->channels[i]);
        if(chandata_has_flag(channel, WLD_CHAN_RADAR_DETECTED)) {
            list[nr_cleared_channels] = channel->channel;
            nr_cleared_channels++;
        }
    }
    if(i < band->nr_channels) {
        SAH_TRACEZ_ERROR(ME, "Performed getRadar channels with too short list %s", pRad->Name);
    }
    return nr_cleared_channels;
}

/**
 * Mark the given channel as available in current locale
 */
void wld_channel_mark_available_channel(swl_chanspec_t chanspec) {
    chan_add_flag(chanspec.channel, chanspec.band, WLD_CHAN_AVAILABLE_FLAG);
}

/**
 * Mark given channel as unavailable in given locale
 */
void wld_channel_mark_unavailable_channel(swl_chanspec_t chanspec) {
    chan_remove_flag(chanspec.channel, chanspec.band, WLD_CHAN_AVAILABLE_FLAG);
}

/**
 * Mark channel as requiring radar. Note that it does not
 * automatically mark as needing to be cleared. This must be done separatly.
 */
void wld_channel_mark_radar_req_channel(swl_chanspec_t chanspec) {
    chan_add_flag(chanspec.channel, chanspec.band, WLD_CHAN_RADAR_REQUIRED);
}

/**
 * Mark that this channel is passive, i.e. that it still must be dfs cleared before
 * being usable
 */
void wld_channel_mark_passive_channel(swl_chanspec_t chanspec) {
    chan_add_flag(chanspec.channel, chanspec.band, WLD_CHAN_PASSIVE);
}

/**
 * Remove the passive requirement from this channel. Should be done
 * when ap can use this channel, so when it is not a dfs channel, or when the channel is cleared.
 */
void wld_channel_clear_passive_channel(swl_chanspec_t chanspec) {
    chan_remove_flag(chanspec.channel, chanspec.band, WLD_CHAN_PASSIVE);
}

/**
 * Mark the channel as radar detected.
 */
void wld_channel_mark_radar_detected_channel(swl_chanspec_t chanspec) {
    chan_add_flag(chanspec.channel, chanspec.band, WLD_CHAN_RADAR_DETECTED);
}

/**
 * Unmark the channel as radar detected
 */
void wld_channel_clear_radar_detected_channel(swl_chanspec_t chanspec) {
    chan_remove_flag(chanspec.channel, chanspec.band, WLD_CHAN_RADAR_DETECTED);
}

/**
 * Mark the entire band as available
 */
void wld_channel_mark_available_band(swl_chanspec_t chanspec) {
    band_add_flag(chanspec, WLD_CHAN_AVAILABLE_FLAG);
}

/**
 * Mark the entire band as unavailable
 */
void wld_channel_mark_unavailable_band(swl_chanspec_t chanspec) {
    band_remove_flag(chanspec, WLD_CHAN_AVAILABLE_FLAG);
}

/**
 * Mark that the given channel and band has radar requirements.
 * Note that this will mark the entire band as radar required.
 */
void wld_channel_mark_radar_req_band(swl_chanspec_t chanspec) {
    band_add_flag(chanspec, WLD_CHAN_RADAR_REQUIRED);
}

/**
 * Mark that a given channel and band is passive, so that we still have to do
 * a dfs clear before going there.
 */
void wld_channel_mark_passive_band(swl_chanspec_t chanspec) {
    band_add_flag(chanspec, WLD_CHAN_PASSIVE);
}

/**
 * Clear the passive flag from the given channel and band
 */
void wld_channel_clear_passive_band(swl_chanspec_t chanspec) {
    band_remove_flag(chanspec, WLD_CHAN_PASSIVE);
}

/**
 * Mark that a radar is detected in the given channel and band
 */
void wld_channel_mark_radar_detected_band(swl_chanspec_t chanspec) {
    band_add_flag(chanspec, WLD_CHAN_RADAR_DETECTED);
}

/**
 * Clear the radar detected flag from the given channel and band
 */
void wld_channel_clear_radar_detected_band(swl_chanspec_t chanspec) {
    band_remove_flag(chanspec, WLD_CHAN_RADAR_DETECTED);
}

/**
 * Returns whether the channel is usable
 */
bool wld_channel_is_usable(swl_chanspec_t chanspec) {
    return is_flag_set_on_channel(chanspec.channel, chanspec.band, WLD_CHAN_AVAILABLE_FLAG) && !is_flag_set_on_channel(chanspec.channel, chanspec.band, WLD_CHAN_PASSIVE);
}

/**
 * Returns whether the current band is usable.
 */
bool wld_channel_is_band_usable(swl_chanspec_t chanspec) {
    return is_flag_set_in_band_always(chanspec, WLD_CHAN_AVAILABLE_FLAG)
           && !is_flag_set_in_band_anywhere(chanspec, WLD_CHAN_PASSIVE)
           && !is_flag_set_in_band_anywhere(chanspec, WLD_CHAN_RADAR_DETECTED);
}

/**
 * Returns true if band is available, but just passive and can be cleared.
 */
bool wld_channel_is_band_passive(swl_chanspec_t chanspec) {
    return is_flag_set_in_band_always(chanspec, WLD_CHAN_AVAILABLE_FLAG)
           && is_flag_set_in_band_anywhere(chanspec, WLD_CHAN_PASSIVE)
           && !is_flag_set_in_band_anywhere(chanspec, WLD_CHAN_RADAR_DETECTED);
}

/**
 * Returns true if the channel has radar detected, false otherwise
 */
bool wld_channel_is_radar_detected(swl_chanspec_t chanspec) {
    return is_flag_set_on_channel(chanspec.channel, chanspec.band, WLD_CHAN_RADAR_DETECTED);
}

/**
 * Returns true if anywhere in the given band a radar signal is detected.
 * Note that when false is returned, the band could be unusable, or not even existing.
 * Verify with is useble to check if a band can actually be used.
 */
bool wld_channel_is_band_radar_detected(swl_chanspec_t chanspec) {
    return is_flag_set_in_band_anywhere(chanspec, WLD_CHAN_RADAR_DETECTED);
}

/**
 * Returns true if the given chanspec is not in banned chanspec list and
 * Channel is not in banned channel list and available but just passive can be cleared.
 */
int wld_channel_isValidClearChannel(T_Radio* pRad, int channel, swl_bandwidth_e operatingChannelBandwidth) {
    ASSERT_NOT_NULL(pRad, LFALSE, ME, "null radio");
    swl_chanspec_t chanspec = SWL_CHANSPEC_NEW(channel, operatingChannelBandwidth, pRad->operatingFrequencyBand);
    if(wld_channel_is_band_passive(chanspec) == false) {
        return LFALSE;
    }
    if(!wld_channel_is_long_wait_band(chanspec)) {
        return LFALSE;
    }

    return LTRUE;
}

/**
 * Returns a DFS channel number that can be cleared in the current bandwidth.
 * If no channel can currently be cleared, it returns zero
 */
int wld_channel_get_channel_to_clear(T_Radio* pRad, swl_bandwidth_e operatingChannelBandwidth) {
    swl_chanspec_t chanspec = SWL_CHANSPEC_NEW(WLD_CHAN_START_DFS, operatingChannelBandwidth, pRad->operatingFrequencyBand);

    int nr_channels = swl_chanspec_getNrChannelsInBand(&chanspec);
    if(pRad->nrPossibleChannels == 0) {
        return 0;
    }

    int i = 0;
    while(i < pRad->nrPossibleChannels) {
        int target_channel = pRad->possibleChannels[i];
        chanspec.channel = target_channel;
        if(wld_channel_isValidClearChannel(pRad, target_channel, operatingChannelBandwidth) > 0) {
            SAH_TRACEZ_INFO(ME, "returning chan %u %u", target_channel, nr_channels);
            return target_channel;
        }
        i++;
        while(i < pRad->nrPossibleChannels
              && wld_channel_is_chan_in_band(chanspec, pRad->possibleChannels[i])) {
            i++;
        }
    }

    return 0;
}

/**
 * Set the time in milliseconds that a channel should be cleared.
 */
void wld_channel_set_channel_clear_time(int channel, uint32_t time) {
    ASSERTS_TRUE(swl_channel_isDfs(channel), , ME, "%d not a DFS channel", channel);
    s_chanSetClearTime(channel, SWL_FREQ_BAND_EXT_5GHZ, time);
}

/**
 * Returns the time in milliseconds that a channel should be cleared.
 */
int wld_channel_get_channel_clear_time(int channel) {
    ASSERTS_TRUE(swl_channel_isDfs(channel), 0, ME, "%d not a DFS channel", channel);
    /**
     * Return the clear time provided previously by the WiFi driver, if exists.
     * Return the default values otherwise.
     */
    uint32_t clearTime = s_chanGetClearTime(channel, SWL_FREQ_BAND_EXT_5GHZ);
    if(clearTime > 0) {
        return clearTime;
    }
    if((channel >= WLD_CHAN_START_EXTENDED_CLEARTIME)
       && (channel <= WLD_CHAN_END_EXTENDED_CLEARTIME)) {
        return WLD_CHAN_DFS_EXTENDED_CLEAR_TIME_MS;
    } else {
        return WLD_CHAN_DFS_CLEAR_TIME_MS;
    }
}

/**
 * Set the time in milliseconds that a channel should be cleared.
 */
void wld_channel_set_channel_bg_clear_time(int channel, uint32_t time) {
    ASSERTS_TRUE(swl_channel_isDfs(channel), , ME, "%d not a DFS channel", channel);
    s_chanSetBgClearTime(channel, SWL_FREQ_BAND_EXT_5GHZ, time);
}

/**
 * Returns the time in milliseconds that a channel should be cleared.
 */
int wld_channel_get_channel_bg_clear_time(int channel) {
    ASSERTS_TRUE(swl_channel_isDfs(channel), 0, ME, "%d not a DFS channel", channel);
    uint32_t clearTime = s_chanGetBgClearTime(channel, SWL_FREQ_BAND_EXT_5GHZ);
    if(clearTime > 0) {
        return clearTime;
    }
    return s_chanGetClearTime(channel, SWL_FREQ_BAND_EXT_5GHZ);
}


bool wld_channel_areAdjacent(swl_chanspec_t chanspec1, swl_chanspec_t chanspec2) {
    swl_chanspec_t chanspec_low;
    swl_chanspec_t chanspec_high;
    memset(&chanspec_low, 0, sizeof(swl_chanspec_t));
    memset(&chanspec_high, 0, sizeof(swl_chanspec_t));
    ASSERTI_TRUE(chanspec1.band == chanspec1.band, false, ME, "Frequency bands are different");
    chanspec_low.band = chanspec1.band;
    chanspec_high.band = chanspec1.band;
    if(chanspec1.channel > chanspec2.channel) {
        chanspec_high.channel = chanspec1.channel;
        chanspec_low.channel = chanspec2.channel;
        chanspec_low.bandwidth = chanspec1.bandwidth;
        chanspec_high.bandwidth = chanspec2.bandwidth;
    } else {
        chanspec_high.channel = chanspec2.channel;
        chanspec_low.channel = chanspec1.channel;
        chanspec_low.bandwidth = chanspec2.bandwidth;
        chanspec_high.bandwidth = chanspec1.bandwidth;
    }

    int baseLow = swl_chanspec_getBaseChannel(&chanspec_low);
    int baseHigh = swl_chanspec_getBaseChannel(&chanspec_high);
    int nr_channels = swl_chanspec_getNrChannelsInBand(&chanspec_low);

    return baseHigh <= baseLow + nr_channels * CHANNEL_INCREMENT;
}

/**
 * Returns whether the channel is available
 */
bool wld_channel_is_available(swl_chanspec_t chanspec) {
    return is_flag_set_on_channel(chanspec.channel, chanspec.band, WLD_CHAN_AVAILABLE_FLAG);
}

/**
 * Returns whether the current band is usable.
 */
bool wld_channel_is_band_available(swl_chanspec_t chanspec) {
    return is_flag_set_in_band_always(chanspec, WLD_CHAN_AVAILABLE_FLAG);
}

// Debug

void wld_channel_do_for_each_channel(T_Radio* rad, void* data, void (* channel_function)(T_Radio* rad, void* data, int channel)) {
    unsigned int i = 0;
    wld_band_data* band = get_band(rad->operatingFrequencyBand);
    ASSERTS_NOT_NULL(band, , ME, "null freqband");
    for(i = 0; i < band->nr_channels; i++) {
        channel_function(rad, data, band->channels[i].channel);
    }
}


/**
 * Debug method to print the current channel state
 */
void wld_channel_print(T_Radio* rad) {
    unsigned int i = 0;
    wld_band_data* band = get_band(rad->operatingFrequencyBand);
    ASSERTS_NOT_NULL(band, , ME, "null freqband");
    for(i = 0; i < band->nr_channels; i++) {
        SAH_TRACEZ_ERROR(ME, "chan %u flag %u clear %u",
                         band->channels[i].channel,
                         band->channels[i].flags,
                         band->channels[i].clearTime
                         );
    }
}
