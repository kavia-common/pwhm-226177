/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2023 SoftAtHome
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
#include "wld_channel.h"
#include "swl/swl_assert.h"
#include "wld_channel_lib.h"
#include <stdlib.h>
#include <string.h>
#include "wld_chanmgt.h"
#include "wld_ap_rssiMonitor.h"
#include "wld_rad_stamon.h"
#include "wld_eventing.h"
#include "wld_radio.h"
#include "wld_accesspoint.h"
#include "wld_assocdev.h"
#include "swla/swla_delayExec.h"
#include "swla/swla_trans.h"
#include "wld_dm_trans.h"
#include "Utils/wld_autoCommitMgr.h"

#define ME "chanMgt"

/* timeout in msec for the setChanspec */
#define WLD_CHANMGT_REQ_CHANGE_CS_TIMEOUT 10000

#define WLD_CHANMGT_DEF_6G_FCC_CHAN 37

static void s_saveClearedChannels(T_Radio* pRad, amxd_trans_t* pTrans) {
    swl_channel_t clearList[50];
    memset(clearList, 0, sizeof(clearList));
    size_t nr_chans = wld_channel_get_cleared_channels(pRad, clearList, sizeof(clearList));
    char buf[256] = {0};
    swl_conv_uint8ArrayToChar(buf, sizeof(buf), clearList, nr_chans);
    amxd_trans_set_value(cstring_t, pTrans, "ClearedDfsChannels", buf);
}

static void s_saveRadarChannels(T_Radio* pRad, amxd_trans_t* pTrans) {
    char buf[256] = {0};
    swl_conv_uint8ArrayToChar(buf, sizeof(buf), pRad->radarDetectedChannels, pRad->nrRadarDetectedChannels);
    amxd_trans_set_value(cstring_t, pTrans, "RadarTriggeredDfsChannels", buf);
}

static void s_writeRadarChannels(T_Radio* pRad) {
    uint8_t nrLastChannels = pRad->nrRadarDetectedChannels;
    swl_channel_t currDfsChannels[WLD_MAX_POSSIBLE_CHANNELS];
    memcpy(currDfsChannels, pRad->radarDetectedChannels, WLD_MAX_POSSIBLE_CHANNELS);

    memset(pRad->radarDetectedChannels, 0, WLD_MAX_POSSIBLE_CHANNELS);

    pRad->nrRadarDetectedChannels = wld_channel_get_radartriggered_channels(pRad, pRad->radarDetectedChannels, WLD_MAX_POSSIBLE_CHANNELS);

    if(!swl_typeUInt8_arrayEquals(currDfsChannels, nrLastChannels, pRad->radarDetectedChannels, pRad->nrRadarDetectedChannels)) {
        pRad->nrLastRadarChannelsAdded = swl_typeUInt8_arrayDiff(pRad->lastRadarChannelsAdded, SWL_BW_CHANNELS_MAX,
                                                                 pRad->radarDetectedChannels, pRad->nrRadarDetectedChannels, currDfsChannels, nrLastChannels);
    }
}

static void s_saveDfsChanInfo(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    ASSERTI_TRUE(pRad->hasDmReady, , ME, "%s: radio dm obj not ready for updates", pRad->Name);
    amxd_object_t* chanObject = amxd_object_findf(pRad->pBus, "ChannelMgt");
    ASSERTI_NOT_NULL(chanObject, , ME, "NULL");
    amxd_trans_t trans;
    ASSERT_TRANSACTION_INIT(chanObject, &trans, , ME, "%s : trans init failure", pRad->Name);
    s_saveClearedChannels(pRad, &trans);
    s_saveRadarChannels(pRad, &trans);
    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, , ME, "%s : trans apply failure", pRad->Name);
}

void wld_chanmgt_writeDfsChannels(T_Radio* pRad) {
    ASSERTI_NOT_NULL(pRad, , ME, "Radio null");
    s_writeRadarChannels(pRad);
    s_saveDfsChanInfo(pRad);
}

static void s_sendNotification(wld_rad_chanChange_t* change) {
    ASSERTS_NOT_NULL(change, , ME, "NULL");
    T_Radio* pRad = change->pRad;
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    ASSERTS_NOT_NULL(pRad->pBus, , ME, "NULL");

    amxc_var_t params;
    amxc_var_init(&params);
    amxc_var_set_type(&params, AMXC_VAR_ID_HTABLE);

    amxc_var_t* my_map = amxc_var_add_key(amxc_htable_t, &params, "Updates", NULL);

    swl_type_addToMap(&gtSwl_type_timeMono, my_map, "TimeStamp", &change->changeTime);
    amxc_var_add_key(uint32_t, my_map, "OldChannel", change->old.channel);
    amxc_var_add_key(uint32_t, my_map, "NewChannel", change->new.channel);
    amxc_var_add_key(cstring_t, my_map, "ChannelChangeReason", g_wld_channelChangeReason_str[change->reason]);
    amxc_var_add_key(cstring_t, my_map, "OldBandwidth", Rad_SupBW[change->old.bandwidth]);
    amxc_var_add_key(cstring_t, my_map, "NewBandwidth", Rad_SupBW[change->new.bandwidth]);
    amxc_var_add_key(uint32_t, my_map, "NrAssociatedStations", change->nrSta);
    amxc_var_add_key(uint32_t, my_map, "NrAssociatedVideoStations", change->nrVid);

    amxd_object_trigger_signal(pRad->pBus, "Channel change event", &params);

    SAH_TRACEZ_INFO(ME, "Send channel change notification");

    amxc_var_clean(&params);
}


const char* chanspecShowing_str[CHANNEL_INTERNAL_STATUS_MAX] = {"Current", "Target", "Sync"};

static bool s_isChanspecSync(T_Radio* pR) {
    return swl_typeChanspecExt_equals(pR->targetChanspec.chanspec, pR->currentChanspec.chanspec);
}

static void s_updateChannelShowing(T_Radio* pR) {
    amxd_object_t* object = amxd_object_get_child(pR->pBus, "ChannelMgt");
    swl_conv_objectParamSetEnum(object, "ChanspecShowing", pR->channelShowing, chanspecShowing_str, CHANNEL_INTERNAL_STATUS_MAX);
}

static void s_updateChanDetailed(wld_rad_detailedChanState_t chanDet, amxd_object_t* object) {

    amxd_trans_t trans;
    ASSERT_TRANSACTION_INIT(object, &trans, , ME, "trans init failure");

    amxd_trans_set_uint16_t(&trans, "Channel", chanDet.chanspec.channel);
    amxd_trans_set_cstring_t(&trans, "Bandwidth", swl_radBw_str[swl_chanspec_toRadBw(&chanDet.chanspec)]);
    amxd_trans_set_cstring_t(&trans, "Frequency", swl_freqBandExt_str[chanDet.chanspec.band]);
    amxd_trans_set_cstring_t(&trans, "Reason", g_wld_channelChangeReason_str[chanDet.reason]);
    amxd_trans_set_cstring_t(&trans, "ReasonExt", chanDet.reasonExt);
    swl_type_toObjectParam(&gtSwl_type_timeMono, object, "LastChangeTime", &chanDet.changeTime);


    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, , ME, "trans apply failure");

}

static void s_updateTargetChanspec(T_Radio* pR) {
    s_updateChanDetailed(pR->targetChanspec, amxd_object_findf(pR->pBus, "ChannelMgt.TargetChanspec"));
}

static void s_updateCurrentChanspec(T_Radio* pR) {
    s_updateChanDetailed(pR->currentChanspec, amxd_object_findf(pR->pBus, "ChannelMgt.CurrentChanspec"));
}

static void s_cleanChannelListTillSize(T_Radio* pRad, size_t maxSize) {

    amxc_llist_t* changeList = &(pRad->channelChangeList);
    amxc_llist_it_t* it = amxc_llist_get_first(changeList);
    while(amxc_llist_size(changeList) > maxSize && it != NULL) {
        amxc_llist_it_t* nextIt = it->next;
        wld_rad_chanChange_t* change = amxc_llist_it_get_data(it, wld_rad_chanChange_t, it);
        amxc_llist_it_take(&change->it);
        swl_object_delInstWithTransOnLocalDm(change->object);
        change->object = NULL;
        free(change);
        it = nextIt;
    }
}

static swl_rc_ne s_writeChangeToOdl(wld_rad_chanChange_t* change) {
    ASSERTS_NOT_NULL(change, SWL_RC_INVALID_PARAM, ME, "NULL");
    T_Radio* pRad = change->pRad;
    ASSERTS_NOT_NULL(pRad, SWL_RC_ERROR, ME, "NULL");
    ASSERTS_NOT_NULL(pRad->pBus, SWL_RC_ERROR, ME, "NULL");
    amxd_object_t* template = amxd_object_findf(pRad->pBus, "ChannelMgt.ChannelChanges");

    amxd_trans_t trans;
    ASSERT_TRANSACTION_INIT(template, &trans, SWL_RC_ERROR, ME, "%s : trans init failure", pRad->Name);

    amxd_trans_add_inst(&trans, pRad->totalNrCurrentChanspecChanges, NULL);

    swl_typeTimeMono_toTransParam(&trans, "TimeStamp", change->changeTime);

    amxd_trans_set_uint8_t(&trans, "OldChannel", change->old.channel);
    amxd_trans_set_cstring_t(&trans, "OldBandwidth", Rad_SupBW[change->old.bandwidth]);
    amxd_trans_set_uint32_t(&trans, "NewChannel", change->new.channel);
    amxd_trans_set_cstring_t(&trans, "NewBandwidth", Rad_SupBW[change->new.bandwidth]);
    amxd_trans_set_cstring_t(&trans, "ChannelChangeReason", g_wld_channelChangeReason_str[change->reason]);
    amxd_trans_set_cstring_t(&trans, "ChannelChangeReasonExt", change->reasonExt);
    amxd_trans_set_uint16_t(&trans, "NrSta", change->nrSta);
    amxd_trans_set_uint16_t(&trans, "NrVideoSta", change->nrVid);

    amxd_trans_set_uint32_t(&trans, "TargetChannel", change->target.channel);
    amxd_trans_set_cstring_t(&trans, "TargetBandwidth", Rad_SupBW[change->target.bandwidth]);
    swl_typeTimeMono_toTransParam(&trans, "TargetChangeTime", change->targetChangeTime);

    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, SWL_RC_ERROR, ME, "%s : trans apply failure", pRad->Name);


    change->object = amxd_object_get_instance(template, NULL, pRad->totalNrCurrentChanspecChanges);
    return SWL_RC_OK;
}

static void s_saveChannelChange(wld_rad_chanChange_t* change) {
    ASSERT_NOT_NULL(change, , ME, "NULL");
    ASSERTS_FALSE(s_writeChangeToOdl(change) < SWL_RC_OK, , ME, "Fail to save channel change");
    s_sendNotification(change);
}

static void s_saveCurrentChanspec(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    s_updateCurrentChanspec(pRad);
    s_updateChannelShowing(pRad);
    wld_rad_chan_update_model(pRad, NULL);
}

static wld_rad_chanChange_t* s_logChannelChange(T_Radio* pRad, swl_chanspec_t oldSpec) {
    ASSERT_NOT_NULL(pRad, NULL, ME, "NULL");

    uint32_t maxChangelistSize = pRad->channelChangeListSize;
    ASSERTI_NOT_EQUALS(maxChangelistSize, 0, NULL, ME, "Log size 0");

    wld_rad_chanChange_t* change = calloc(1, sizeof(wld_rad_chanChange_t));
    ASSERTS_NOT_NULL(change, NULL, ME, "NULL");

    change->pRad = pRad;
    change->old = oldSpec;
    change->new = pRad->currentChanspec.chanspec;
    change->target = pRad->targetChanspec.chanspec;
    change->changeTime = swl_time_getMonoSec();
    change->targetChangeTime = pRad->targetChanspec.changeTime;
    change->reason = pRad->currentChanspec.reason;
    snprintf(change->reasonExt, sizeof(change->reasonExt), "%s", pRad->currentChanspec.reasonExt);
    change->nrSta = wld_rad_get_nb_active_stations(pRad);
    change->nrVid = wld_rad_get_nb_active_video_stations(pRad);

    amxc_llist_t* changeList = &(pRad->channelChangeList);
    amxc_llist_append(changeList, &change->it);

    s_cleanChannelListTillSize(pRad, maxChangelistSize);

    return change;
}

static void s_onUpdateChannelChangeConfig(T_Radio* pRad) {
    ASSERTI_NOT_NULL(pRad, , ME, "NULL");
    s_cleanChannelListTillSize(pRad, pRad->channelChangeListSize);
}

static const char* s_isTargetChanspecValid(T_Radio* pR, swl_chanspec_t chanspec, swl_rc_ne* pRc) {
    bool isValid = ((pR->operatingFrequencyBand == SWL_FREQ_BAND_EXT_2_4GHZ) && swl_channel_is2g(chanspec.channel)) ||
        ((pR->operatingFrequencyBand == SWL_FREQ_BAND_EXT_5GHZ) && swl_channel_is5g(chanspec.channel)) ||
        ((pR->operatingFrequencyBand == SWL_FREQ_BAND_EXT_6GHZ) && swl_channel_is6g(chanspec.channel));
    if(!isValid) {
        W_SWL_SETPTR(pRc, SWL_RC_INVALID_PARAM);
        return "Illegal channel for band";
    }

    if(wld_channel_is_band_radar_detected(chanspec)) {
        W_SWL_SETPTR(pRc, SWL_RC_INVALID_STATE);
        return "Chanspec not available due to radar";
    }

    if(swl_typeChanspec_equals(pR->targetChanspec.chanspec, chanspec)) {
        if((pR->targetChanspec.isApplied == SWL_TRL_TRUE) && swl_typeChanspec_equals(pR->currentChanspec.chanspec, chanspec)) {
            W_SWL_SETPTR(pRc, SWL_RC_DONE);
            return "same chanspec, applied";
        }
        if(pR->targetChanspec.isApplied == SWL_TRL_UNKNOWN) {
            W_SWL_SETPTR(pRc, SWL_RC_CONTINUE);
            return "same chanspec, not yet applied";
        }
    }

    W_SWL_SETPTR(pRc, SWL_RC_OK);
    return NULL;
}

static void s_setChanspecDone(T_Radio* pR, amxd_status_t status) {
    ASSERT_NOT_NULL(pR, , ME, "NULL");
    if(pR->targetChanspec.isApplied == SWL_TRL_UNKNOWN) {
        pR->targetChanspec.isApplied = (status == amxd_status_ok) ? SWL_TRL_TRUE : SWL_TRL_FALSE;
    }

    amxp_timer_delete(&pR->timerReqChanspec);
    pR->timerReqChanspec = NULL;

    if((pR->callIdReqChanspec.status != SWL_FUNCTION_DEFERRED_STATUS_STARTED) &&
       (pR->callIdReqChanspec.status != SWL_FUNCTION_DEFERRED_STATUS_CANCELLED)) {
        return;
    }

    amxc_var_t ret;
    amxc_var_init(&ret);
    amxc_var_set_type(&ret, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(bool, &ret, "Result", (status == amxd_status_ok));

    SAH_TRACEZ_INFO(ME, "%s: setChanspec() call end: %u", pR->Name, status);
    swl_function_deferDone(&pR->callIdReqChanspec, status, NULL, &ret);

    SAH_TRACEZ_INFO(ME, "%s: notify channel switch done, Result=%u",
                    pR->Name, (status == amxd_status_ok));
    amxd_object_trigger_signal(pR->pBus, "ChannelSwitchComplete", &ret);

    amxc_var_clean(&ret);
}

static swl_rc_ne s_refreshCurrentChanspec(T_Radio* pR, swl_chanspec_t chanspec, wld_channelChangeReason_e reason) {
    ASSERT_NOT_NULL(pR, SWL_RC_ERROR, ME, "NULL");

    if(swl_typeChanspec_equals(pR->targetChanspec.chanspec, chanspec)) {
        pR->targetChanspec.isApplied = SWL_TRL_TRUE;
    } else if(pR->targetChanspec.isApplied == SWL_TRL_UNKNOWN) {
        pR->targetChanspec.isApplied = SWL_TRL_FALSE;
    }

    if(swl_typeChanspec_equals(chanspec, pR->currentChanspec.chanspec)) {
        SAH_TRACEZ_INFO(ME, "%s: report same chanspec, %s (reason %s)", pR->Name,
                        swl_typeChanspecExt_toBuf32(chanspec).buf,
                        g_wld_channelChangeReason_str[reason]);
        return SWL_RC_INVALID_STATE;
    }

    swl_chanspec_t oldSpec = pR->currentChanspec.chanspec;

    if(pR->currentChanspec.chanspec.channel != chanspec.channel) {
        pR->channelChangeReason = reason;
    }
    if(pR->currentChanspec.chanspec.bandwidth != chanspec.bandwidth) {
        pR->channelBandwidthChangeReason = reason;
    }

    SAH_TRACEZ_WARNING(ME, "%s: set cur chanspec %s -> %s (reason <%s>, target %s, nrSta %u/%u)",
                       pR->Name,
                       swl_typeChanspecExt_toBuf32(pR->currentChanspec.chanspec).buf,
                       swl_typeChanspecExt_toBuf32(chanspec).buf,
                       g_wld_channelChangeReason_str[reason],
                       swl_typeChanspecExt_toBuf32(pR->targetChanspec.chanspec).buf,
                       wld_rad_get_nb_active_stations(pR), wld_rad_get_nb_active_video_stations(pR)
                       );

    pR->totalNrCurrentChanspecChanges++;
    pR->channelChangeCounters[reason]++;

    if(pR->operatingChannelBandwidth != SWL_RAD_BW_AUTO) {
        pR->operatingChannelBandwidth = swl_chanspec_toRadBw(&chanspec);
    }

    pR->currentChanspec.chanspec = chanspec;
    pR->currentChanspec.reason = reason;
    pR->currentChanspec.changeTime = swl_time_getMonoSec();
    pR->currentChanspec.isApplied = SWL_TRL_TRUE;
    pR->channel = chanspec.channel;
    pR->runningChannelBandwidth = swl_chanspec_toRadBw(&chanspec);

    if((reason == pR->targetChanspec.reason) && swl_typeChanspec_equals(pR->targetChanspec.chanspec, pR->currentChanspec.chanspec)) {
        swl_str_cat(pR->currentChanspec.reasonExt, sizeof(pR->currentChanspec.reasonExt), pR->targetChanspec.reasonExt);
    } else {
        memset(pR->targetChanspec.reasonExt, 0, sizeof(pR->targetChanspec.reasonExt));
    }

    if(s_isChanspecSync(pR)) {
        pR->channelShowing = CHANNEL_INTERNAL_STATUS_SYNC;
        swl_str_copy(pR->currentChanspec.reasonExt, sizeof(pR->currentChanspec.reasonExt), pR->targetChanspec.reasonExt);
    } else {
        pR->channelShowing = CHANNEL_INTERNAL_STATUS_CURRENT;
    }

    wld_rad_updateChannelsInUse(pR);

    if(pR->hasDmReady) {
        s_saveChannelChange(s_logChannelChange(pR, oldSpec));
        s_saveCurrentChanspec(pR);
    }

    /* notify about channel change status */
    wld_rad_triggerChangeEvent(pR, WLD_RAD_CHANGE_CHANSPEC, NULL);

    return SWL_RC_OK;

}

static swl_rc_ne s_checkTargetChanspec(T_Radio* pR, swl_chanspec_t chanspec, wld_channelChangeReason_e reason) {
    ASSERT_NOT_NULL(pR, SWL_RC_ERROR, ME, "NULL");

    if(swl_function_deferIsActive(&pR->callIdReqChanspec)) {
        swl_chanspec_t tmpChSpec = chanspec;
        if(pR->targetChanspec.chanspec.extensionHigh == SWL_CHANSPEC_EXT_AUTO) {
            tmpChSpec.extensionHigh = SWL_CHANSPEC_EXT_AUTO;
        }
        bool success = swl_type_equals(swl_type_chanspec, &pR->targetChanspec.chanspec, &tmpChSpec);
        success &= (pR->targetChanspec.reason == reason);
        amxd_status_t status = success ? amxd_status_ok : amxd_status_unknown_error;
        s_setChanspecDone(pR, status);
    }

    if((reason == CHAN_REASON_OBSS_COEX)) {
        pR->targetChanspec.chanspec.bandwidth = chanspec.bandwidth;
    }

    /* Reset to default channel if an invalid channel switch occurred */
    if(reason == CHAN_REASON_INVALID) {
        swl_chanspec_t resetChanspec = SWL_CHANSPEC_NEW(wld_chanmgt_getDefaultSupportedChannel(pR),
                                                        pR->targetChanspec.chanspec.bandwidth,
                                                        pR->operatingFrequencyBand);
        SAH_TRACEZ_ERROR(ME, "%s: Invalid channel switch <%u>, reset to default chanspec <%s>", pR->Name, pR->channel,
                         swl_typeChanspecExt_toBuf32(resetChanspec).buf);
        return wld_chanmgt_setTargetChanspec(pR, resetChanspec, true, CHAN_REASON_RESET, NULL);
    }

    if((reason == CHAN_REASON_EP_MOVE) && !swl_typeChanspec_equals(pR->targetChanspec.chanspec, chanspec)) {
        return wld_chanmgt_setTargetChanspec(pR, chanspec, true, reason, "force EP chanspec");
    }

    if((reason == CHAN_REASON_DFS) && !swl_typeChanspec_equals(pR->targetChanspec.chanspec, chanspec)) {
        return wld_chanmgt_setTargetChanspec(pR, chanspec, false, reason, "DFS radar detected");
    }

    if((reason == CHAN_REASON_INITIAL) && (pR->targetChanspec.chanspec.channel == 0)) {
        return wld_chanmgt_setTargetChanspec(pR, chanspec, false, CHAN_REASON_INITIAL, NULL);
    }

    return SWL_RC_OK;

}

swl_rc_ne wld_chanmgt_reportCurrentChanspec(T_Radio* pR, swl_chanspec_t chanspec, wld_channelChangeReason_e reason) {
    ASSERT_NOT_NULL(pR, SWL_RC_ERROR, ME, "NULL");
    swl_rc_ne rc = s_refreshCurrentChanspec(pR, chanspec, reason);
    s_checkTargetChanspec(pR, chanspec, reason);
    return rc;
}

static void s_updateTargetDm(void* data) {
    T_Radio* pR = (T_Radio*) data;
    ASSERT_TRUE(debugIsRadPointer(pR), , ME, "ERR");

    s_updateChannelShowing(pR);
    s_updateTargetChanspec(pR);
}

/*
 * @brief select channel width when Auto bw is used
 *
 * @param autoBwMode auto bw selection mode
 * @param maxBw max supported channel width
 * @param tgtChspec input target chanspec:
 *                  used reference channel with auto bw mode MaxCleared
 *                          and filled with deduced channel width
 *
 * @return selected channel width, or SWL_RAD_BW_AUTO in case of error
 */
swl_radBw_e wld_chanmgt_getAutoBwExt(wld_rad_bwSelectMode_e autoBwMode, swl_bandwidth_e maxBw, swl_chanspec_t tgtChspec) {
    ASSERT_TRUE(autoBwMode < BW_SELECT_MODE_MAX, SWL_RAD_BW_AUTO, ME, "Invalid autoBwMode %d", autoBwMode);
    ASSERT_FALSE(tgtChspec.band >= SWL_FREQ_BAND_EXT_NONE, SWL_RAD_BW_AUTO, ME, "target chanspec has unknown freqBand");

    const char* autoBwModeStr = wld_rad_autoBwSelectMode_str[autoBwMode];
    swl_bandwidth_e dfltBw = swl_bandwidth_defaults[(swl_freqBand_e) tgtChspec.band];
    dfltBw = maxBw ? SWL_MIN(dfltBw, maxBw) : dfltBw;
    if(autoBwMode == BW_SELECT_MODE_DEFAULT) {
        tgtChspec.bandwidth = dfltBw;
    } else {
        ASSERT_NOT_EQUALS(maxBw, SWL_BW_AUTO, SWL_RAD_BW_AUTO, ME, "unknown maxBw");
        swl_chanspec_t testChspec = tgtChspec;
        testChspec.bandwidth = maxBw;
        if((testChspec.band == SWL_FREQ_BAND_EXT_6GHZ) &&
           (testChspec.bandwidth == SWL_BW_320MHZ) &&
           (testChspec.extensionHigh == SWL_CHANSPEC_EXT_AUTO)) {
            // first try 320-1
            testChspec.extensionHigh = SWL_CHANSPEC_EXT_LOW;
            if(!wld_channel_is_band_available(testChspec)) {
                SAH_TRACEZ_INFO(ME, "bw 320-1 not available on channel 6/%u", testChspec.channel);
                // if not available, try 320-2
                testChspec.extensionHigh = SWL_CHANSPEC_EXT_HIGH;
                if(!wld_channel_is_band_available(testChspec)) {
                    SAH_TRACEZ_WARNING(ME, "bw 320-2 not available on channel 6/%u", testChspec.channel);
                    // if not available, reset value to auto, should not matter, but just to be clean
                    testChspec.extensionHigh = SWL_CHANSPEC_EXT_AUTO;
                }
            }
        }
        // for all max mode variants, check first available channels (valid and without radar detection)
        while(((!wld_channel_is_band_available(testChspec)) ||
               (wld_channel_is_band_radar_detected(testChspec))) &&
              (testChspec.bandwidth > 0)) {
            testChspec.bandwidth--;
        }
        ASSERT_TRUE(testChspec.bandwidth > 0, SWL_RAD_BW_AUTO,
                    ME, "can not select %s auto bw: no available channel in tgt chanspec %s",
                    autoBwModeStr, swl_typeChanspecExt_toBuf32(tgtChspec).buf);
        swl_chanspec_t availChSpec = testChspec;
        if(autoBwMode == BW_SELECT_MODE_MAXAVAILABLE) {
            tgtChspec = availChSpec;
        } else if(autoBwMode == BW_SELECT_MODE_MAXCLEARED) {
            while(!wld_channel_is_band_usable(testChspec) && testChspec.bandwidth > 0) {
                testChspec.bandwidth--;
            }
            if(testChspec.bandwidth == SWL_BW_AUTO) {
                testChspec.bandwidth = SWL_MIN(availChSpec.bandwidth, dfltBw);
                SAH_TRACEZ_NOTICE(ME, "%s not cleared but clearable", swl_typeChanspecExt_toBuf32(testChspec).buf);
            }
            tgtChspec = testChspec;
        }
    }
    SAH_TRACEZ_INFO(ME, "auto bw %s %d (max:%d) (tgtChspec:%s)",
                    autoBwModeStr,
                    swl_chanspec_bwToInt(tgtChspec.bandwidth),
                    swl_chanspec_bwToInt(maxBw),
                    swl_typeChanspecExt_toBuf32(tgtChspec).buf
                    );
    return swl_chanspec_toRadBw(&tgtChspec);
}

swl_radBw_e wld_chanmgt_getAutoBw(T_Radio* pR, swl_chanspec_t tgtChspec) {
    ASSERT_NOT_NULL(pR, SWL_RAD_BW_AUTO, ME, "NULL");
    swl_bandwidth_e maxAppBw = wld_chanmgt_getHighestApplicableBw(pR);
    return wld_chanmgt_getAutoBwExt(pR->autoBwSelectMode, maxAppBw, tgtChspec);
}

/**
 * Set target chanspec for a specific radio
 *
 * Notes:
 * - frequency band switching is not supported
 * - reasonExt is not used for the moment
 */
swl_rc_ne wld_chanmgt_setTargetChanspec(T_Radio* pR, swl_chanspec_t chanspec, bool direct, wld_channelChangeReason_e reason, const char* reasonExt) {
    ASSERT_NOT_NULL(pR, SWL_RC_ERROR, ME, "NULL");
    ASSERT_TRUE(pR->isReady || reason == CHAN_REASON_INITIAL, SWL_RC_ERROR, ME, "%s: radio is not configured yet", pR->Name);

    swl_rc_ne rc = SWL_RC_OK;

    swl_chanspec_t tgtChanspec = chanspec;

    bool bwChangeRequested = ((reason == CHAN_REASON_MANUAL) && tgtChanspec.bandwidth != SWL_BW_AUTO);

    if(tgtChanspec.bandwidth == SWL_BW_AUTO) {
        swl_radBw_e bw = wld_chanmgt_getAutoBw(pR, tgtChanspec);
        if(bw <= SWL_RAD_BW_160MHZ) {
            tgtChanspec.bandwidth = (swl_bandwidth_e) bw;
        } else if(bw == SWL_RAD_BW_320MHZ1) {
            tgtChanspec.bandwidth = SWL_BW_320MHZ;
            tgtChanspec.extensionHigh = SWL_CHANSPEC_EXT_LOW;
        } else if(bw == SWL_RAD_BW_320MHZ2) {
            tgtChanspec.bandwidth = SWL_BW_320MHZ;
            tgtChanspec.extensionHigh = SWL_CHANSPEC_EXT_HIGH;
        }
    }
    while(!wld_channel_is_band_available(tgtChanspec) && tgtChanspec.bandwidth > 0) {
        tgtChanspec.bandwidth--;
    }
    ASSERT_TRUE(tgtChanspec.bandwidth > 0, SWL_RC_ERROR, ME, "%s: channel / bw not available %s",
                pR->Name, swl_type_toBuf32(&gtSwl_type_chanspecExt, &chanspec).buf);
    swl_bandwidth_e maxAppBw = wld_chanmgt_getHighestApplicableBw(pR);
    if(tgtChanspec.bandwidth > maxAppBw) {
        SAH_TRACEZ_WARNING(ME, "%s: reduce tgt bw %s to max runtime applicable bw %s",
                           pR->Name,
                           swl_bandwidth_str[tgtChanspec.bandwidth],
                           swl_bandwidth_str[maxAppBw]);
        tgtChanspec.bandwidth = maxAppBw;
    }

    const char* checkReason = s_isTargetChanspecValid(pR, tgtChanspec, &rc);
    if(rc != SWL_RC_OK) {
        SAH_TRACEZ_ERROR(ME, "%s: %s <%s>", pR->Name, checkReason, swl_type_toBuf32(&gtSwl_type_chanspecExt, &chanspec).buf);
        if((rc == SWL_RC_CONTINUE) && direct) {
            SAH_TRACEZ_WARNING(ME, "%s: Force direct apply of pending <%s>", pR->Name, swl_type_toBuf32(&gtSwl_type_chanspecExt, &chanspec).buf);
        } else {
            return SWL_RC_ERROR;
        }
    }

    if(!wld_rad_firstCommitFinished(pR) && (reason == CHAN_REASON_MANUAL) && (pR->totalNrTargetChanspecChanges < 2)) {
        reason = CHAN_REASON_PERSISTANCE;
    }

    SAH_TRACEZ_WARNING(ME, "%s: set tgt chanspec %s -> %s (reason <%s>, direct <%d>, current %s, in %s)",
                       pR->Name,
                       swl_typeChanspecExt_toBuf32(pR->targetChanspec.chanspec).buf,
                       swl_typeChanspecExt_toBuf32(tgtChanspec).buf,
                       g_wld_channelChangeReason_str[reason], direct,
                       swl_typeChanspecExt_toBuf32(pR->currentChanspec.chanspec).buf,
                       swl_typeChanspecExt_toBuf32(chanspec).buf
                       );

    pR->totalNrTargetChanspecChanges++;

    pR->channel = tgtChanspec.channel;

    if((pR->operatingChannelBandwidth != SWL_RAD_BW_AUTO) || bwChangeRequested) {
        pR->operatingChannelBandwidth = swl_chanspec_toRadBw(&tgtChanspec);
    }

    memset(&pR->targetChanspec.chanspec, 0, sizeof(swl_chanspec_t));
    pR->targetChanspec.chanspec = tgtChanspec;
    pR->targetChanspec.reason = reason;
    pR->targetChanspec.changeTime = swl_time_getMonoSec();
    pR->targetChanspec.isApplied = SWL_TRL_UNKNOWN;
    /* set timeout for the call
     * if the band is cleared use default timeout
     * if not, wait clear time in addition */
    pR->targetChanspec.estimatedChangeDuration = WLD_CHANMGT_REQ_CHANGE_CS_TIMEOUT;
    pR->targetChanspec.estimatedChangeDuration += wld_channel_is_band_passive(chanspec) ? wld_channel_get_band_clear_time(chanspec) : 0;

    pR->channelChangeReason = reason;
    if(reasonExt != NULL) {
        swl_str_copy(pR->targetChanspec.reasonExt, sizeof(pR->targetChanspec.reasonExt), reasonExt);
    } else {
        memset(pR->targetChanspec.reasonExt, 0, sizeof(pR->targetChanspec.reasonExt));
    }

    if(pR->pBus != NULL) {
        swl_radBw_e dmRadBw = swl_conv_objectParamEnum(pR->pBus, "OperatingChannelBandwidth", swl_radBw_str, SWL_RAD_BW_MAX, SWL_RAD_BW_AUTO);
        if((tgtChanspec.channel == amxd_object_get_uint8_t(pR->pBus, "Channel", NULL)) && (pR->operatingChannelBandwidth == dmRadBw)) {
            /* operating channel did not change yet, exposed channel in DM is the target */
            pR->channelShowing = CHANNEL_INTERNAL_STATUS_TARGET;
        } else {
            /* exposed channel in DM is not up to date and still print current value */
            pR->channelShowing = CHANNEL_INTERNAL_STATUS_CURRENT;
        }
    }

    /* Change channel */
    if(reason != CHAN_REASON_INITIAL) {
        rc = pR->pFA->mfn_wrad_setChanspec(pR, direct);
        if(swl_rc_isOk(rc)) {
            if(!direct) {
                wld_autoCommitMgr_notifyRadEdit(pR);
            }
        } else if(direct) {
            pR->targetChanspec.isApplied = SWL_TRL_FALSE;
        }
    }
    if(rc == SWL_RC_DONE) {
        pR->targetChanspec.isApplied = SWL_TRL_TRUE;
    }

    if(s_isChanspecSync(pR)) {
        pR->channelShowing = CHANNEL_INTERNAL_STATUS_SYNC;
    }

    swla_delayExec_add(s_updateTargetDm, pR);

    return rc;
}

static void s_setChanspecCanceled(swl_function_deferredInfo_t* info, void* const priv) {
    T_Radio* pR = (T_Radio*) priv;
    ASSERT_NOT_NULL(pR, , ME, "NULL");
    ASSERT_EQUALS(&pR->callIdReqChanspec, info, , ME, "not matching callInfo");
    s_setChanspecDone(pR, amxd_status_unknown_error);
}

static void s_setChanspecTimeout(amxp_timer_t* timer, void* userdata) {
    T_Radio* pR = (T_Radio*) userdata;
    ASSERT_NOT_NULL(pR, , ME, "NULL");
    ASSERT_EQUALS(pR->timerReqChanspec, timer, , ME, "INVALID");
    SAH_TRACEZ_WARNING(ME, "%s: chanspec timeout", pR->Name);
    s_setChanspecDone(pR, amxd_status_unknown_error);
}

amxd_status_t _Radio_setChanspec(amxd_object_t* obj,
                                 amxd_function_t* func,
                                 amxc_var_t* args,
                                 amxc_var_t* ret) {
    T_Radio* pR = obj->priv;
    ASSERT_NOT_NULL(pR, amxd_status_unknown_error, ME, "NULL");

    uint16_t channel = GET_UINT32(args, "channel");
    const char* bandwidth_str = GET_CHAR(args, "bandwidth");
    const char* reason_str = GET_CHAR(args, "reason");
    const char* reasonExt = GET_CHAR(args, "reasonExt");
    bool direct = GET_BOOL(args, "direct");

    if(!wld_rad_hasChannel(pR, channel)) {
        SAH_TRACEZ_ERROR(ME, "%s: Invalid channel %d", pR->Name, channel);
        return amxd_status_invalid_arg;
    }

    if(wld_rad_hasRunningEndpoint(pR)) {
        SAH_TRACEZ_WARNING(ME, "%s: EP connected, do not change channel", pR->Name);
        return amxd_status_invalid_action;
    }

    swl_freqBandExt_e freqBand = pR->operatingFrequencyBand;

    swl_radBw_e radBw = swl_conv_charToEnum(bandwidth_str, swl_radBw_str, SWL_RAD_BW_MAX, SWL_RAD_BW_AUTO);
    if(radBw > SWL_RAD_BW_MAX) {
        SAH_TRACEZ_ERROR(ME, "%s: Invalid bandwidth %s - %d (max: %d)", pR->Name, bandwidth_str, radBw, SWL_RAD_BW_MAX);
        return amxd_status_invalid_arg;
    }
    swl_bandwidth_e bandwidth = swl_radBw_toBw[radBw];

    if(swl_chanspec_bwToInt(bandwidth) > swl_chanspec_bwToInt(pR->maxChannelBandwidth)) {
        SAH_TRACEZ_ERROR(ME, "%s: Invalid bandwidth %s (max: %s)", pR->Name,
                         swl_bandwidth_str[bandwidth], swl_bandwidth_str[pR->maxChannelBandwidth]);
        return amxd_status_invalid_arg;
    }

    swl_chanspec_t chanspec = swl_chanspec_fromDm(channel, radBw, freqBand);
    wld_channelChangeReason_e reason = swl_conv_charToEnum(reason_str, g_wld_channelChangeReason_str, CHAN_REASON_MAX, CHAN_REASON_MANUAL);

    SAH_TRACEZ_INFO(ME, "%s: request chanspec <%s> reason <%s> direct <%d>",
                    pR->Name, swl_type_toBuf32(&gtSwl_type_chanspecExt, &chanspec).buf, g_wld_channelChangeReason_str[reason], direct);

    swl_rc_ne rc = wld_chanmgt_setTargetChanspec(pR, chanspec, direct, reason, reasonExt);
    ASSERT_FALSE(rc < SWL_RC_OK, amxd_status_unknown_error,
                 ME, "%s: fail to set channel %d", pR->Name, channel);
    chanspec = pR->targetChanspec.chanspec;
    pR->userChanspec = pR->targetChanspec.chanspec;

    /* release/notify old call, if any.*/
    if(swl_function_deferIsActive(&pR->callIdReqChanspec)) {
        amxd_function_deferred_remove(pR->callIdReqChanspec.callId);
    }

    /* register callId & cancel callback */
    swl_function_deferCb(&pR->callIdReqChanspec, func, ret, s_setChanspecCanceled, pR);

    amxp_timer_new(&pR->timerReqChanspec, s_setChanspecTimeout, pR);
    amxp_timer_start(pR->timerReqChanspec, pR->targetChanspec.estimatedChangeDuration);

    if(!direct) {
        /* schedule action */
        wld_autoCommitMgr_notifyRadEdit(pR);
    }

    /* Used for extra logging */
    if(reasonExt) {
        SAH_TRACEZ_INFO(ME, "%s: Reason detailed:", pR->Name);
        SAH_TRACEZ_INFO(ME, "\t%s", reasonExt);
    }

    return amxd_status_deferred;
}

static void s_setChannelChangeLogSize_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);

    T_Radio* pR = wld_rad_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pR, , ME, "NULL");
    pR->channelChangeListSize = amxc_var_dyncast(uint32_t, newValue);
    s_onUpdateChannelChangeConfig(pR);
    SAH_TRACEZ_INFO(ME, "%s: Update channel changes list size to %d", pR->Name, pR->channelChangeListSize);

    SAH_TRACEZ_OUT(ME);
}

static void s_setAcsBootChannel_pwf(void* priv _UNUSED, amxd_object_t* object, amxd_param_t* param _UNUSED, const amxc_var_t* const newValue) {
    SAH_TRACEZ_IN(ME);
    T_Radio* pR = wld_rad_fromObj(amxd_object_get_parent(object));
    ASSERT_NOT_NULL(pR, , ME, "NULL");
    pR->acsBootChannel = amxc_var_dyncast(int32_t, newValue);
    SAH_TRACEZ_INFO(ME, "%s: Update ACS Boot channel to %d", pR->Name, pR->acsBootChannel);
    SAH_TRACEZ_OUT(ME);
}

/*
 * default channel:
 * - must belong to supported channel list
 * - for 5GHz freqBand, the first non-dfs supported channel is selected
 * - for 6GHz freqBand, the first supported PSC channel greater or equal to 37,
 *                      otherwise, take the first PSC
 * - otherwise, take the first supported channel
 */
swl_channel_t wld_chanmgt_getBetterDefaultChannel(swl_freqBandExt_e freqBand, swl_channel_t curChan, swl_channel_t newChan) {
    if((curChan == 0) ||
       ((freqBand == SWL_FREQ_BAND_EXT_5GHZ) &&
        (!swl_channel_isDfs(newChan)) && (swl_channel_isDfs(curChan))) ||
       ((freqBand == SWL_FREQ_BAND_EXT_6GHZ) &&
        (swl_channel_is6gPsc(newChan)) &&
        ((!swl_channel_is6gPsc(curChan)) || (curChan < WLD_CHANMGT_DEF_6G_FCC_CHAN)))) {
        return newChan;
    }
    return curChan;
}

static void s_selectDefaultChannel(T_Radio* pRad, void* data, int channel) {
    swl_channel_t* pChan = (swl_channel_t*) data;
    swl_chanspec_t chanspec = SWL_CHANSPEC_NEW(channel, SWL_BW_AUTO, pRad->operatingFrequencyBand);
    ASSERTS_TRUE(wld_channel_is_available(chanspec), , ME, "chan %d not available", channel);
    *pChan = wld_chanmgt_getBetterDefaultChannel(pRad->operatingFrequencyBand, *pChan, chanspec.channel);
}

swl_channel_t wld_chanmgt_getDefaultSupportedChannel(T_Radio* pRad) {
    swl_channel_t defChan = SWL_CHANNEL_INVALID;
    ASSERTS_NOT_NULL(pRad, defChan, ME, "NULL");
    wld_channel_do_for_each_channel(pRad, &defChan, s_selectDefaultChannel);
    if(!defChan) {
        defChan = swl_channel_defaults[wld_rad_getFreqBand(pRad)];
        SAH_TRACEZ_WARNING(ME, "%s: rad has not supp chans, assume default chan %d", pRad->Name, defChan);
    }
    return defChan;
}

swl_bandwidth_e wld_chanmgt_getDefaultSupportedBandwidth(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, SWL_BW_AUTO, ME, "NULL");
    swl_bandwidth_e defBw = swl_bandwidth_defaults[wld_rad_getFreqBand(pRad)];
    ASSERTW_TRUE(pRad->supportedChannelBandwidth > M_SWL_BW_AUTO, SWL_BW_20MHZ, ME, "%s: no info about supported BW: assume default 20MHz", pRad->Name);
    while(defBw > SWL_BW_AUTO) {
        swl_chanspec_t chSpec = SWL_CHANSPEC_NEW(0, defBw, SWL_FREQ_BAND_EXT_AUTO);
        swl_radBw_e radBw = swl_chanspec_toRadBw(&chSpec);
        if(SWL_BIT_IS_SET(pRad->supportedChannelBandwidth, radBw)) {
            break;
        }
        defBw--;
    }
    return defBw;
}

/*
 * returns bitmask of applicable channel bandwidths (dm bw)
 * among the supported ones, based on the enabled operating standards
 */
swl_radBw_m wld_chanmgt_getApplicableRadBwMask(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, 0, ME, "NULL");
    swl_radStd_m enaStds = swl_radStd_getEnabledRadStd(pRad->supportedStandards, pRad->operatingStandards);
    if(pRad->operatingFrequencyBand < SWL_FREQ_BAND_EXT_NONE) {
        enaStds &= swl_freqBand_radStd[pRad->operatingFrequencyBand];
    }
    if(SWL_BIT_IS_SET(enaStds, SWL_RADSTD_BE) && !wld_rad_is11beUsable(pRad)) {
        W_SWL_BIT_CLEAR(enaStds, SWL_RADSTD_BE);
    }
    /*
     * ensure as minimum the legacy bw if no match between supported and selected rad std list
     */
    if(enaStds < M_SWL_RADSTD_AUTO) {
        swl_radStd_m legRadStdMask = SWL_BIT_SHIFT(swl_mcs_radStdFromMcsStd(SWL_MCS_STANDARD_LEGACY, pRad->operatingFrequencyBand));
        enaStds |= swl_radStd_getEnabledRadStd(pRad->supportedStandards, legRadStdMask);
    }
    return swl_chanspec_getApplicableRadBwMask(pRad->supportedChannelBandwidth, enaStds);
}

static void s_updateDmAppRadBws(T_Radio* pR) {
    if(!(pR && pR->pBus && pR->hasDmReady)) {
        return;
    }
    swl_radBw_m appRadBws = wld_chanmgt_getApplicableRadBwMask(pR);
    amxd_trans_t trans;
    ASSERT_TRANSACTION_INIT(pR->pBus, &trans, , ME, "%s : trans init failure", pR->Name);
    swl_conv_transParamSetMask(&trans, "ApplicableOperatingChannelBandwidths", appRadBws, swl_radBw_str, SWL_RAD_BW_MAX);
    ASSERT_TRANSACTION_LOCAL_DM_END(&trans, , ME, "%s : trans apply failure", pR->Name);
}

static void s_mldChange(wld_mldChange_t* event) {
    ASSERT_NOT_NULL(event, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "detect mld event %d mldtype %d unit %d", event->event, event->mldType, event->mldUnit);
    ASSERTI_NOT_EQUALS(event->mldType, WLD_SSID_TYPE_UNKNOWN, , ME, "untyped mld");
    T_SSID* pSSID = event->pEvtLinkSsid;
    ASSERT_NOT_NULL(pSSID, , ME, "No link ssid");
    T_Radio* pRad = pSSID->RADIO_PARENT;
    ASSERT_NOT_NULL(pRad, , ME, "No link radio");
    wld_chanmgt_updateApplicableRadBwMask(pRad);
}

static wld_event_callback_t s_onMldChange = {
    .callback = (wld_event_callback_fun) s_mldChange,
};

swl_rc_ne wld_chanmgt_initApplicableRadBwMask(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    pRad->applicableChannelBandwidths = wld_chanmgt_getApplicableRadBwMask(pRad);
    wld_event_add_callback(gWld_queue_mld_onChangeEvent, &s_onMldChange);
    return SWL_RC_OK;
}

swl_rc_ne wld_chanmgt_updateApplicableRadBwMask(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    swl_radBw_m appRadBws = wld_chanmgt_getApplicableRadBwMask(pRad);
    if(appRadBws != pRad->applicableChannelBandwidths) {
        swla_delayExec_add((swla_delayExecFun_cbf) s_updateDmAppRadBws, pRad);
        return SWL_RC_CONTINUE;
    }
    return SWL_RC_DONE;
}

swl_radBw_e wld_chanmgt_getHighestApplicableRadBw(T_Radio* pRad) {
    if(pRad->applicableChannelBandwidths <= M_SWL_RAD_BW_AUTO) {
        wld_chanmgt_initApplicableRadBwMask(pRad);
    }
    swl_radBw_e resBw = SWL_RAD_BW_AUTO;
    uint32_t resBwInt = 0;
    for(uint32_t i = 0; i < SWL_RAD_BW_MAX; i++) {
        if(SWL_BIT_IS_SET(pRad->applicableChannelBandwidths, i)) {
            uint32_t tmpBwInt = swl_chanspec_radBwToInt(i);
            if(tmpBwInt > resBwInt) {
                resBwInt = tmpBwInt;
                resBw = i;
            }
        }
    }
    return resBw;
}

swl_bandwidth_e wld_chanmgt_getHighestApplicableBw(T_Radio* pRad) {
    return swl_chanspec_radBwToPhyBw(wld_chanmgt_getHighestApplicableRadBw(pRad));
}

/**
 * Ensure current channels is properly filled in with default.
 */
void wld_chanmgt_checkInitChannel(T_Radio* pRad) {
    if(pRad->currentChanspec.chanspec.channel != 0) {
        return;
    }
    pRad->channelChangeListSize = 10;

    swl_chanspec_t defaultChanspec = SWL_CHANSPEC_NEW(wld_chanmgt_getDefaultSupportedChannel(pRad),
                                                      wld_chanmgt_getDefaultSupportedBandwidth(pRad),
                                                      pRad->operatingFrequencyBand);
    //consider the default chanspec from the driver
    //otherwise fall back to implicit default chanspec per freqBand
    swl_chanspec_t radChanspec = wld_rad_getSwlChanspec(pRad);
    if(swl_chanspec_channelToMHzDef(&radChanspec, 0) != 0) {
        defaultChanspec.channel = radChanspec.channel;
    }
    wld_chanmgt_reportCurrentChanspec(pRad, defaultChanspec, CHAN_REASON_INITIAL);
}

/**
 * update of the radio data model after create
 */
void wld_chanmgt_saveChanges(T_Radio* pRad, amxd_trans_t* trans) {
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    ASSERT_NOT_NULL(pRad->pBus, , ME, "NULL");
    ASSERTI_TRUE(pRad->hasDmReady, , ME, "%s: radio dm obj not ready for updates", pRad->Name);
    s_updateCurrentChanspec(pRad);
    s_updateTargetDm(pRad);

    amxc_llist_for_each(it, &pRad->channelChangeList) {
        wld_rad_chanChange_t* change = amxc_llist_it_get_data(it, wld_rad_chanChange_t, it);
        if(change->object == NULL) {
            s_writeChangeToOdl(change);
        }
    }

    swla_trans_t tmpTrans;
    amxd_trans_t* targetTrans = swla_trans_init(&tmpTrans, trans, pRad->pBus);
    ASSERT_NOT_NULL(targetTrans, , ME, "NULL");

    swl_channel_t channel = amxd_object_get_uint32_t(pRad->pBus, "Channel", NULL);
    if((channel == 0) || (channel == pRad->channel)) {
        if((channel == 0) && (pRad->channel != 0)) {
            amxd_trans_set_uint32_t(targetTrans, "Channel", pRad->channel);
        }
        amxd_trans_set_cstring_t(targetTrans, "ChannelsInUse", pRad->channelsInUse);
        amxd_trans_set_cstring_t(targetTrans, "ChannelChangeReason", g_wld_channelChangeReason_str[pRad->channelChangeReason]);
    }

    swl_radBw_e radBw = swl_conv_objectParamEnum(pRad->pBus, "OperatingChannelBandwidth", swl_radBw_str, SWL_RAD_BW_MAX, SWL_RAD_BW_AUTO);
    if((radBw == SWL_RAD_BW_AUTO) || (radBw == pRad->operatingChannelBandwidth)) {
        if(radBw == SWL_RAD_BW_AUTO) {
            amxd_trans_set_cstring_t(targetTrans, "OperatingChannelBandwidth", swl_radBw_str[pRad->operatingChannelBandwidth]);
        }
        amxd_trans_set_cstring_t(targetTrans, "CurrentOperatingChannelBandwidth", swl_radBw_str[pRad->runningChannelBandwidth]);
        amxd_trans_set_cstring_t(targetTrans, "ChannelBandwidthChangeReason", g_wld_channelChangeReason_str[pRad->channelBandwidthChangeReason]);
    }

    swla_trans_finalize(&tmpTrans, NULL);
}

/**
 * Return the current chanspec
 */
swl_chanspec_t wld_chanmgt_getCurChspec(T_Radio* pRad) {
    swl_chanspec_t chspec = SWL_CHANSPEC_EMPTY;
    ASSERTS_NOT_NULL(pRad, chspec, ME, "NULL");
    return pRad->currentChanspec.chanspec;
}

/**
 * Return the target chanspec
 */
swl_chanspec_t wld_chanmgt_getTgtChspec(T_Radio* pRad) {
    swl_chanspec_t chspec = SWL_CHANSPEC_EMPTY;
    ASSERTS_NOT_NULL(pRad, chspec, ME, "NULL");
    return pRad->targetChanspec.chanspec;
}

/**
 * Return the current channel
 */
swl_channel_t wld_chanmgt_getCurChannel(T_Radio* pRad) {
    return wld_chanmgt_getCurChspec(pRad).channel;
}

/**
 * Return the current bandwidth
 */
swl_bandwidth_e wld_chanmgt_getCurBw(T_Radio* pRad) {
    return wld_chanmgt_getCurChspec(pRad).bandwidth;
}

/**
 * Return the target channel
 */
swl_channel_t wld_chanmgt_getTgtChannel(T_Radio* pRad) {
    return wld_chanmgt_getTgtChspec(pRad).channel;
}

/**
 * Return the target bandwidth
 */
swl_bandwidth_e wld_chanmgt_getTgtBw(T_Radio* pRad) {
    return wld_chanmgt_getTgtChspec(pRad).bandwidth;
}

void wld_chanmgt_cleanup(T_Radio* pRad) {
    amxc_llist_for_each(it, &pRad->channelChangeList) {
        wld_rad_chanChange_t* change = amxc_llist_it_get_data(it, wld_rad_chanChange_t, it);
        amxc_llist_it_take(it);
        free(change);
    }
}

SWLA_DM_HDLRS(sChanMgtDmHdlrs,
              ARR(SWLA_DM_PARAM_HDLR("ChangeLogSize", s_setChannelChangeLogSize_pwf),
                  SWLA_DM_PARAM_HDLR("AcsBootChannel", s_setAcsBootChannel_pwf)));

void _wld_chanmgt_setConf_ocf(const char* const sig_name,
                              const amxc_var_t* const data,
                              void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sChanMgtDmHdlrs, sig_name, data, priv);
}

amxd_status_t _wld_chanmgt_validateAcsBootChannel_pvf(amxd_object_t* object,
                                                      amxd_param_t* param _UNUSED,
                                                      amxd_action_t reason _UNUSED,
                                                      const amxc_var_t* const args,
                                                      amxc_var_t* const retval _UNUSED,
                                                      void* priv _UNUSED) {
    ASSERTS_FALSE(amxc_var_is_null(args), amxd_status_invalid_value, ME, "invalid");
    ASSERTS_EQUALS(amxd_object_get_type(object), amxd_object_singleton, amxd_status_ok, ME, "obj is not singleton");
    int bootChannel = amxc_var_dyncast(int32_t, args);
    ASSERTI_FALSE((bootChannel < -1) || (bootChannel > 255), amxd_status_invalid_value, ME, "invalid bootChannel %d", bootChannel);
    ASSERTS_TRUE(bootChannel != -1, amxd_status_ok, ME, "accept AcsBootChannel disabling value");
    T_Radio* pRad = wld_rad_fromObj(amxd_object_get_parent(object));
    ASSERTI_NOT_NULL(pRad, amxd_status_ok, ME, "No radio mapped");
    ASSERTI_TRUE(pRad->hasDmReady, amxd_status_ok, ME, "%s: radio config not yet fully loaded", pRad->Name);
    ASSERTI_TRUE(wld_rad_hasChannel(pRad, bootChannel), amxd_status_invalid_value, ME, "%s: not supported bootChannel %d", pRad->Name, bootChannel);
    return amxd_status_ok;
}



void wld_chanmgt_init(T_Radio* pR) {
    swl_function_deferInit(&pR->callIdReqChanspec);
}
