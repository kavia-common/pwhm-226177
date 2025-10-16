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

#ifndef _WLD_CHANMGT_PCB_H_
#define _WLD_CHANMGT_PCB_H_

#include "wld.h"
#include "swl/swl_returnCode.h"

typedef struct {
    amxc_llist_it_t it;
    amxd_object_t* object;
    T_Radio* pRad;
    swl_chanspec_t old;
    swl_chanspec_t new;
    swl_chanspec_t target;
    swl_timeMono_t changeTime;
    swl_timeMono_t targetChangeTime;
    wld_channelChangeReason_e reason;
    char reasonExt[128];
    uint16_t nrSta;
    uint16_t nrVid;
} wld_rad_chanChange_t;


void wld_chanmgt_init(T_Radio* pRad);
void wld_chanmgt_cleanup(T_Radio* pRad);

/**
 * Write the currently cleared dfs channels to pcb
 */
void wld_chanmgt_writeDfsChannels(T_Radio* pRad);

swl_rc_ne wld_chanmgt_reportCurrentChanspec(T_Radio* pR, swl_chanspec_t chanspec, wld_channelChangeReason_e reason);
swl_rc_ne wld_chanmgt_setTargetChanspec(T_Radio* pR, swl_chanspec_t chanspec, bool direct, wld_channelChangeReason_e reason, const char* reasonExt);

void wld_chanmgt_saveChanges(T_Radio* pRad, amxd_trans_t* trans);
void wld_chanmgt_checkInitChannel(T_Radio* pRad);
swl_channel_t wld_chanmgt_getCurChannel(T_Radio* pRad);
swl_bandwidth_e wld_chanmgt_getCurBw(T_Radio* pRad);
swl_channel_t wld_chanmgt_getTgtChannel(T_Radio* pRad);
swl_bandwidth_e wld_chanmgt_getTgtBw(T_Radio* pRad);
swl_chanspec_t wld_chanmgt_getCurChspec(T_Radio* pRad);
swl_chanspec_t wld_chanmgt_getTgtChspec(T_Radio* pRad);
swl_radBw_e wld_chanmgt_getAutoBwExt(wld_rad_bwSelectMode_e autoBwMode, swl_bandwidth_e maxBw, swl_chanspec_t tgtChspec);
swl_radBw_e wld_chanmgt_getAutoBw(T_Radio* pR, swl_chanspec_t tgtChspec);
swl_channel_t wld_chanmgt_getBetterDefaultChannel(swl_freqBandExt_e freqBand, swl_channel_t curChan, swl_channel_t newChan);
swl_channel_t wld_chanmgt_getDefaultSupportedChannel(T_Radio* pRad);
swl_bandwidth_e wld_chanmgt_getDefaultSupportedBandwidth(T_Radio* pRad);

#endif /* _WLD_CHANMGT_PCB_H_ */
