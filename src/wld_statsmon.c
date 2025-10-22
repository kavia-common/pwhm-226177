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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "wld_statsmon.h"
#include "wld_util.h"
#include "wld_radio.h"
#include "wld_linuxIfUtils.h"

/*Skip white space and word macros */
#define SKIP_WSP(a)  while((a) && ((*(a) == ' ') || (*(a) == '\t')))(a) ++
#define SKIP_WORD(a) while((a) && (isalnum(*(a)) || *(a) == '_'))(a) ++


static time_t latestStateChangeTime = 0;

static T_Stats* s_addRadioStats(T_Stats* pDst, const T_intf_txrxstats* pSrc) {
    if(pDst && pSrc) {
        pDst->BytesSent += pSrc->txBytes;
        pDst->BytesReceived += pSrc->rxBytes;
        pDst->PacketsSent += pSrc->txPackets;
        pDst->PacketsReceived += pSrc->rxPackets;
        pDst->ErrorsSent += pSrc->txErrs;
        pDst->ErrorsReceived += pSrc->rxErrs;
        pDst->DiscardPacketsSent += pSrc->txDrop;
        pDst->DiscardPacketsReceived += pSrc->rxDrop;
        pDst->MulticastPacketsSent += pSrc->txCompressed;
        pDst->MulticastPacketsReceived += pSrc->rxMulticast;
        pDst->BroadcastPacketsSent += pSrc->txFifo;
        pDst->BroadcastPacketsReceived += pSrc->rxFrame;
        return pDst;
    }
    return NULL;
}

static T_Stats* s_addTxRxStats2Stats(T_Stats* pDst, const T_intf_txrxstats* pSrc) {
    if(pDst && pSrc) {
        pDst->BytesSent += pSrc->txBytes;
        pDst->BytesReceived += pSrc->rxBytes;
        pDst->PacketsSent += pSrc->txPackets;
        pDst->PacketsReceived += pSrc->rxPackets;
        pDst->ErrorsSent += pSrc->txErrs;
        pDst->ErrorsReceived += pSrc->rxErrs;
        pDst->DiscardPacketsSent += pSrc->txDrop;
        pDst->DiscardPacketsReceived += pSrc->rxDrop;
        pDst->UnicastPacketsSent += 0;
        pDst->UnicastPacketsReceived += 0;
        pDst->MulticastPacketsSent += pSrc->txCompressed;
        pDst->MulticastPacketsReceived += pSrc->rxMulticast;
        pDst->BroadcastPacketsSent += pSrc->txFifo;
        pDst->BroadcastPacketsReceived += pSrc->rxFrame;
        pDst->UnknownProtoPacketsReceived += 0;

        pDst->latestStatsUpdateTime = swl_time_getMonoSec();
        return pDst;
    }
    return NULL;
}

static int s_getLinuxLineStats(char* pCh, T_Radio* pR, T_SSID* pSSID) {
    ASSERTS_STR(pCh, 0, ME, "Empty");
    T_intf_txrxstats intfStats;
    memset(&intfStats, 0, sizeof(intfStats));
    char* pT;

    pT = strchr(pCh, ':');
    ASSERTS_NOT_NULL(pT, 0, ME, "NULL");
    *pT = '\0';
    SKIP_WSP(pCh);
    swl_str_copy(intfStats.intfName, IFNAMSIZ, pCh);
    *pT = ':';
    pT++;
    sscanf(pT, "%llu %llu %lu %lu %lu %lu %lu %lu %llu %llu %lu %lu %lu %lu %lu %lu",
           &intfStats.rxBytes, &intfStats.rxPackets,
           &intfStats.rxErrs, &intfStats.rxDrop,
           &intfStats.rxFifo, &intfStats.rxFrame,
           &intfStats.rxCompressed, &intfStats.rxMulticast,
           &intfStats.txBytes, &intfStats.txPackets,
           &intfStats.txErrs, &intfStats.txDrop,
           &intfStats.txFifo, &intfStats.txCalls,
           &intfStats.txCarrier, &intfStats.txCompressed);

    if(pR != NULL) {
        if(pR == wld_rad_from_name(intfStats.intfName)) {
            s_addRadioStats(&pR->stats, &intfStats);
            return 1;
        }

        T_AccessPoint* pAP = wld_rad_vap_from_name(pR, intfStats.intfName);
        if(pAP != NULL) {
            s_addTxRxStats2Stats(&pAP->pSSID->stats, &intfStats);
            s_addRadioStats(&pR->stats, &intfStats);
            return 1;
        }

        if(wld_linuxIfUtils_isVlanIface(intfStats.intfName)) {
            wld_linuxIfUtils_getVlanLowerIface(intfStats.intfName, intfStats.intfName, sizeof(intfStats.intfName));
        }

        pAP = wld_rad_vap_from_wds_name(pR, intfStats.intfName);
        if(pAP != NULL) {
            s_addTxRxStats2Stats(&pAP->pSSID->stats, &intfStats);
            s_addRadioStats(&pR->stats, &intfStats);
            return 1;
        }

        T_EndPoint* pEP = wld_rad_ep_from_name(pR, intfStats.intfName);
        if(pEP != NULL) {
            memset(&pEP->pSSID->stats, 0, sizeof(T_Stats));
            s_addTxRxStats2Stats(&pEP->pSSID->stats, &intfStats);
            s_addRadioStats(&pR->stats, &intfStats);
            return 1;
        }
    } else if((pSSID != NULL) && ((pR = pSSID->RADIO_PARENT) != NULL)) {
        if(wld_rad_vap_from_name(pR, intfStats.intfName) != NULL) {
            s_addTxRxStats2Stats(&pSSID->stats, &intfStats);
            return 1;
        }
        if(wld_linuxIfUtils_isVlanIface(intfStats.intfName)) {
            wld_linuxIfUtils_getVlanLowerIface(intfStats.intfName, intfStats.intfName, sizeof(intfStats.intfName));
        }
        if((wld_rad_vap_from_wds_name(pR, intfStats.intfName) != NULL) ||
           (wld_rad_ep_from_name(pR, intfStats.intfName) != NULL)) {
            s_addTxRxStats2Stats(&pSSID->stats, &intfStats);
            return 1;
        }
    }

    return 0;
}

/*
    Read out the Linux system stats and store them in the TxRxStats buffer.
 */
static int s_getLinuxStats(T_Radio* pR, T_SSID* pSSID) {
    FILE* hf;
    char buf[512];

    if((time(NULL) - latestStateChangeTime) < TIMEOUT_LINUX_INTF_STATS) {
        return -1;
    }
    hf = fopen(STATS_FILE_PATH, "r");
    if(!hf) {
        return -errno;
    }
    if(pR) {
        T_AccessPoint* pAP = NULL;
        memset(&pR->stats, 0, sizeof(T_Stats));
        wld_rad_forEachAp(pAP, pR) {
            memset(&pAP->pSSID->stats, 0, sizeof(T_Stats));
        }
    }
    if(pSSID) {
        memset(&pSSID->stats, 0, sizeof(T_Stats));
    }
    // proc file system has no size so we can't optimize. If our timeout collaps we update all stats.
    while(fgets(buf, sizeof(buf), hf)) {
        s_getLinuxLineStats(buf, pR, pSSID);
    }
    fclose(hf);
    latestStateChangeTime = time(NULL);

    if(pR) {
        pR->stats.latestStatsUpdateTime = swl_time_getMonoSec();
    }
    return 0;
}

T_Stats* wld_statsmon_updateVAPStats(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, NULL, ME, "NULL");
    T_SSID* pSSID = pAP->pSSID;
    s_getLinuxStats(NULL, pSSID);
    return &pSSID->stats;
}

T_Stats* wld_statsmon_updateEPStats(T_EndPoint* pEP) {
    ASSERTS_NOT_NULL(pEP, NULL, ME, "NULL");
    T_SSID* pSSID = pEP->pSSID;
    s_getLinuxStats(NULL, pSSID);
    return &pSSID->stats;
}

T_Stats* wld_statsmon_updateRADStats(T_Radio* pR) {
    ASSERTS_NOT_NULL(pR, NULL, ME, "NULL");
    memset(&pR->stats, 0, sizeof(T_Stats));
    pR->stats.TemperatureDegreesCelsius = WLD_TEMP_INVALID_CELSIUS;    //set it to Invalid/Default value
    s_getLinuxStats(pR, NULL);
    return &pR->stats;
}

T_Stats* wld_updateVAPStats(T_AccessPoint* pAP, T_intf_txrxstats* pST) {
    _UNUSED_(pST);
    return wld_statsmon_updateVAPStats(pAP);
}

T_Stats* wld_updateEPStats(T_EndPoint* pEP, T_intf_txrxstats* pST) {
    _UNUSED_(pST);
    return wld_statsmon_updateEPStats(pEP);
}

T_Stats* wld_updateRadioStats(T_Radio* pR, T_intf_txrxstats* pST) {
    _UNUSED_(pST);
    return wld_statsmon_updateRADStats(pR);
}
