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
/*
 * This file implements nl80211 api debug functions (dump)
 */

#include "wld_nl80211_debug.h"
#include "wld_channel.h"
#include "swl/swl_common.h"

#define ME "nlDbg"

static void s_dumpVar(amxc_var_t* retVar) {
    amxc_var_t retStr;
    amxc_var_init(&retStr);
    amxc_var_convert(&retStr, retVar, AMXC_VAR_ID_CSTRING);
    SAH_TRACEZ_INFO(ME, "%s", amxc_var_constcast(cstring_t, &retStr));
    amxc_var_clean(&retStr);
}

static void s_dumpMap(amxc_var_t* retMap) {
    amxc_var_t localVar;
    amxc_var_init(&localVar);
    amxc_var_set_type(&localVar, AMXC_VAR_ID_HTABLE);

    amxc_var_copy(&localVar, retMap);
    s_dumpVar(&localVar);
    amxc_var_clean(&localVar);
}

swl_rc_ne wld_nl80211_dumpIfaceInfo(wld_nl80211_ifaceInfo_t* pIfaceInfo, amxc_var_t* retMap) {
    ASSERTS_NOT_NULL(pIfaceInfo, SWL_RC_INVALID_PARAM, ME, "NULL");
    amxc_var_t localVar;
    amxc_var_init(&localVar);
    amxc_var_set_type(&localVar, AMXC_VAR_ID_HTABLE);

    amxc_var_t* pMap = (retMap ? retMap : &localVar);
    ASSERTS_NOT_NULL(pMap, SWL_RC_ERROR, ME, "NULL");
    amxc_var_add_key(cstring_t, pMap, "name", pIfaceInfo->name);
    amxc_var_add_key(uint32_t, pMap, "wiphy", pIfaceInfo->wiphy);
    amxc_var_add_key(uint32_t, pMap, "ifIndex", pIfaceInfo->ifIndex);
    swl_macChar_t macChar;
    swl_mac_binToChar(&macChar, &pIfaceInfo->mac);
    amxc_var_add_key(cstring_t, pMap, "mac", macChar.cMac);
    amxc_string_t typeStr;
    amxc_string_init(&typeStr, 0);
    amxc_string_appendf(&typeStr, "%s", (pIfaceInfo->isMain ? "main" : "virtual"));
    amxc_string_appendf(&typeStr, "%s", (pIfaceInfo->isAp ? "AP" : ""));
    amxc_string_appendf(&typeStr, "%s", (pIfaceInfo->isSta ? "STA" : ""));
    amxc_var_add_key(cstring_t, pMap, "type", amxc_string_get(&typeStr, 0));
    amxc_string_clean(&typeStr);
    amxc_var_add_key(uint32_t, pMap, "freq", pIfaceInfo->chanSpec.ctrlFreq);
    amxc_var_add_key(uint32_t, pMap, "bw", pIfaceInfo->chanSpec.chanWidth);
    amxc_var_add_key(uint32_t, pMap, "txPwr", pIfaceInfo->txPower);
    amxc_var_add_key(uint32_t, pMap, "nMloLinks", pIfaceInfo->nMloLinks);
    amxc_var_t* mloLinksList = amxc_var_add_key(amxc_llist_t, pMap, "mloLinks", NULL);
    for(uint32_t i = 0; i < pIfaceInfo->nMloLinks; i++) {
        wld_nl80211_ifaceMloLinkInfo_t* pLinkInfo = &pIfaceInfo->mloLinks[i];
        amxc_var_t* mloLinkMap = amxc_var_add(amxc_htable_t, mloLinksList, NULL);
        amxc_var_add_key(uint8_t, mloLinkMap, "linkId", pLinkInfo->link.linkId);
        swl_mac_binToChar(&macChar, &pLinkInfo->link.linkMac);
        amxc_var_add_key(cstring_t, mloLinkMap, "linkMac", macChar.cMac);
        amxc_var_add_key(uint32_t, mloLinkMap, "freq", pLinkInfo->chanSpec.ctrlFreq);
        amxc_var_add_key(uint32_t, mloLinkMap, "bw", pLinkInfo->chanSpec.chanWidth);
        amxc_var_add_key(uint32_t, mloLinkMap, "txPwr", pLinkInfo->txPower);
    }
    if(retMap) {
        s_dumpMap(pMap);
    } else {
        s_dumpVar(&localVar);
    }
    amxc_var_clean(&localVar);
    return SWL_RC_OK;
}

swl_rc_ne wld_nl80211_dumpWiphyInfo(wld_nl80211_wiphyInfo_t* pWiphyInfo, amxc_var_t* retMap) {
    ASSERTS_NOT_NULL(pWiphyInfo, SWL_RC_INVALID_PARAM, ME, "NULL");
    amxc_var_t localVar;
    amxc_var_init(&localVar);
    amxc_var_set_type(&localVar, AMXC_VAR_ID_HTABLE);

    amxc_var_t* pMap = (retMap ? retMap : &localVar);
    ASSERTS_NOT_NULL(pMap, SWL_RC_ERROR, ME, "NULL");
    amxc_var_add_key(cstring_t, pMap, "name", pWiphyInfo->name);
    amxc_var_add_key(uint32_t, pMap, "wiphy", pWiphyInfo->wiphy);
    amxc_var_t* nAntMap = amxc_var_add_key(amxc_htable_t, pMap, "nrAntenna", NULL);
    for(uint32_t i = 0; i < COM_DIR_MAX; i++) {
        amxc_var_add_key(int32_t, nAntMap, com_dir_char[i], pWiphyInfo->nrAntenna[i]);
    }
    amxc_var_t* nActAntMap = amxc_var_add_key(amxc_htable_t, pMap, "nrActiveAntenna", NULL);
    for(uint32_t i = 0; i < COM_DIR_MAX; i++) {
        amxc_var_add_key(int32_t, nActAntMap, com_dir_char[i], pWiphyInfo->nrActiveAntenna[i]);
    }
    amxc_var_add_key(uint32_t, pMap, "nrApMax", pWiphyInfo->nApMax);
    amxc_var_add_key(uint32_t, pMap, "nrEpMax", pWiphyInfo->nEpMax);
    amxc_var_add_key(uint32_t, pMap, "nrStaMax", pWiphyInfo->nStaMax);
    amxc_var_add_key(bool, pMap, "suppUAPSD", pWiphyInfo->suppUAPSD);
    amxc_var_add_key(bool, pMap, "suppMLO", pWiphyInfo->suppMlo);
    amxc_var_add_key(bool, pMap, "suppEMLSR[AP]", pWiphyInfo->extCapas.emlsrSupport[WLD_WIPHY_IFTYPE_AP]);
    amxc_var_add_key(bool, pMap, "suppEMLMR[AP]", pWiphyInfo->extCapas.emlmrSupport[WLD_WIPHY_IFTYPE_AP]);
    amxc_var_add_key(bool, pMap, "suppEMLSR[STA]", pWiphyInfo->extCapas.emlsrSupport[WLD_WIPHY_IFTYPE_STATION]);
    amxc_var_add_key(bool, pMap, "suppEMLMR[STA]", pWiphyInfo->extCapas.emlmrSupport[WLD_WIPHY_IFTYPE_STATION]);

    amxc_var_t* cipherSuitesList = amxc_var_add_key(amxc_llist_t, pMap, "suppCiphers", NULL);

    amxc_string_t cipherStr;
    amxc_string_init(&cipherStr, 0);
    for(uint32_t i = 0; i < pWiphyInfo->nCipherSuites; i++) {
        amxc_string_clean(&cipherStr);
        amxc_string_appendf(&cipherStr, "%08x", pWiphyInfo->cipherSuites[i]);
        amxc_var_add(cstring_t, cipherSuitesList, amxc_string_get(&cipherStr, 0));
    }
    amxc_string_clean(&cipherStr);

    char buffer[128] = {0};
    swl_conv_maskToChar(buffer, sizeof(buffer), pWiphyInfo->freqBandsMask, swl_freqBand_str, SWL_FREQ_BAND_MAX);
    amxc_var_add_key(cstring_t, pMap, "freqBands", buffer);
    amxc_var_t* bandsMap = amxc_var_add_key(amxc_htable_t, pMap, "Bands", NULL);
    for(uint32_t i = 0; i < SWL_FREQ_BAND_MAX; i++) {
        if(pWiphyInfo->bands[i].nChans == 0) {
            continue;
        }
        amxc_var_t* bandMap = amxc_var_add_key(amxc_htable_t, bandsMap, swl_freqBand_str[pWiphyInfo->bands[i].freqBand], NULL);
        swl_conv_maskToChar(buffer, sizeof(buffer), pWiphyInfo->bands[i].radStdsMask, swl_radStd_str, SWL_RADSTD_MAX);
        amxc_var_add_key(cstring_t, bandMap, "suppRadStds", buffer);
        swl_conv_maskToChar(buffer, sizeof(buffer), pWiphyInfo->bands[i].chanWidthMask, swl_bandwidth_str, SWL_BW_MAX);
        amxc_var_add_key(cstring_t, bandMap, "suppChanWidth", buffer);
        amxc_var_add_key(uint32_t, bandMap, "nrSSMax", pWiphyInfo->bands[i].nSSMax);
        uint32_t j;
        amxc_var_t* mcsStdsList = amxc_var_add_key(amxc_llist_t, bandMap, "suppMcsStds", NULL);
        for(j = 0; j < SWL_MCS_STANDARD_MAX; j++) {
            if(pWiphyInfo->bands[i].mcsStds[j].standard == SWL_MCS_STANDARD_UNKNOWN) {
                continue;
            }
            if(swl_mcs_toChar(buffer, sizeof(buffer), &pWiphyInfo->bands[i].mcsStds[j]) > 0) {
                amxc_var_add(cstring_t, mcsStdsList, buffer);
            }
        }
        amxc_var_t* bfMap = amxc_var_add_key(amxc_htable_t, bandMap, "suppBeamForming", NULL);
        for(j = 0; j < COM_DIR_MAX; j++) {
            swl_conv_maskToChar(buffer, sizeof(buffer), pWiphyInfo->bands[i].bfCapsSupported[j],
                                g_str_wld_rad_bf_cap, RAD_BF_CAP_MAX);
            amxc_var_add_key(cstring_t, bfMap, com_dir_char[j], buffer);
        }
        amxc_var_t* chanList = amxc_var_add_key(amxc_llist_t, bandMap, "suppChannels", NULL);
        for(j = 0; j < pWiphyInfo->bands[i].nChans; j++) {
            amxc_var_t* chanMap = amxc_var_add(amxc_htable_t, chanList, NULL);
            amxc_var_add_key(uint32_t, chanMap, "ctrlFreq", pWiphyInfo->bands[i].chans[j].ctrlFreq);
            amxc_var_add_key(uint32_t, chanMap, "maxTxPwr", pWiphyInfo->bands[i].chans[j].maxTxPwr);
            if(pWiphyInfo->bands[i].chans[j].isDfs) {
                amxc_var_t* dfsInfoMap = amxc_var_add_key(amxc_htable_t, chanMap, "dfs", NULL);
                amxc_var_add_key(cstring_t, dfsInfoMap, "status",
                                 g_str_wld_nl80211_chan_status_bf_cap[pWiphyInfo->bands[i].chans[j].status]);
                amxc_var_add_key(uint32_t, dfsInfoMap, "stateTime", pWiphyInfo->bands[i].chans[j].dfsTime);
                amxc_var_add_key(uint32_t, dfsInfoMap, "cacTime", pWiphyInfo->bands[i].chans[j].dfsCacTime);
            }
        }
    }

    if(retMap) {
        s_dumpMap(pMap);
    } else {
        s_dumpVar(&localVar);
    }
    amxc_var_clean(&localVar);
    return SWL_RC_OK;
}

static swl_rc_ne wld_nl80211_dumpRateInfo(wld_nl80211_rateInfo_t* pRateInfo, amxc_var_t* retMap) {
    ASSERTS_NOT_NULL(pRateInfo, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(retMap, SWL_RC_ERROR, ME, "NULL");
    amxc_var_add_key(uint32_t, retMap, "Bitrate", pRateInfo->bitrate);
    amxc_var_add_key(cstring_t, retMap, "mcsInfo", swl_typeMcs_toBuf32(pRateInfo->mcsInfo).buf);
    amxc_var_add_key(uint8_t, retMap, "HeDcm", pRateInfo->heDcm);
    amxc_var_add_key(uint8_t, retMap, "OFDMA", pRateInfo->ofdma);
    return SWL_RC_OK;
}

swl_rc_ne wld_nl80211_dumpStationInfo(wld_nl80211_stationInfo_t* pStationInfo, amxc_var_t* retMap) {
    ASSERTS_NOT_NULL(pStationInfo, SWL_RC_INVALID_PARAM, ME, "NULL");
    amxc_var_t localVar;
    amxc_var_init(&localVar);
    amxc_var_set_type(&localVar, AMXC_VAR_ID_HTABLE);

    amxc_var_t* pMap = (retMap ? retMap : &localVar);
    ASSERTS_NOT_NULL(pMap, SWL_RC_ERROR, ME, "NULL");

    amxc_var_add_key(cstring_t, pMap, "MacAddress",
                     swl_typeMacBin_toBuf32Ref(&pStationInfo->macAddr).buf);
    amxc_var_add_key(cstring_t, pMap, "MldAddress",
                     swl_typeMacBin_toBuf32Ref(&pStationInfo->macMld).buf);
    amxc_var_add_key(int8_t, pMap, "linkId", pStationInfo->linkId);

    amxc_var_t* varLinksList = amxc_var_add_new_key_amxc_llist_t(pMap, "links", NULL);
    for(int i = 0; i < pStationInfo->nrLinks; i++) {
        amxc_var_t* var = amxc_var_add_new_amxc_htable_t(varLinksList, NULL);
        amxc_var_add_key(uint8_t, var, "id", pStationInfo->linksInfo[i].linkId);
        amxc_var_add_key(cstring_t, var, "mld", swl_typeMacBin_toBuf32Ref(&pStationInfo->linksInfo[i].mldMac).buf);
        amxc_var_add_key(cstring_t, var, "mac", swl_typeMacBin_toBuf32Ref(&pStationInfo->linksInfo[i].linkMac).buf);
        amxc_var_add_key(uint64_t, var, "rxBytes", pStationInfo->linksInfo[i].stats.rxBytes);
        amxc_var_add_key(uint64_t, var, "txBytes", pStationInfo->linksInfo[i].stats.txBytes);
        amxc_var_add_key(uint32_t, var, "rxPackets", pStationInfo->linksInfo[i].stats.rxPackets);
        amxc_var_add_key(uint32_t, var, "txPackets", pStationInfo->linksInfo[i].stats.txPackets);
        amxc_var_add_key(uint32_t, var, "txRetries", pStationInfo->linksInfo[i].stats.txRetries);
        amxc_var_add_key(uint64_t, var, "rxFailed", pStationInfo->linksInfo[i].stats.rxErrors);
        amxc_var_add_key(uint32_t, var, "txFailed", pStationInfo->linksInfo[i].stats.txErrors);
        amxc_var_add_key(int8_t, var, "signal", pStationInfo->linksInfo[i].stats.rssiDbm);
        amxc_var_add_key(int8_t, var, "signalAverage", pStationInfo->linksInfo[i].stats.rssiAvgDbm);
    }
    amxc_var_add_key(uint32_t, pMap, "inactiveTimeMs", pStationInfo->inactiveTime);
    amxc_var_add_key(uint64_t, pMap, "rxBytes", pStationInfo->rxBytes);
    amxc_var_add_key(uint64_t, pMap, "txBytes", pStationInfo->txBytes);
    amxc_var_add_key(uint32_t, pMap, "rxPackets", pStationInfo->rxPackets);
    amxc_var_add_key(uint32_t, pMap, "txPackets", pStationInfo->txPackets);
    amxc_var_add_key(uint32_t, pMap, "txRetries", pStationInfo->txRetries);
    amxc_var_add_key(uint64_t, pMap, "rxFailed", pStationInfo->rxErrors);
    amxc_var_add_key(uint32_t, pMap, "txFailed", pStationInfo->txFailed);
    amxc_var_add_key(uint64_t, pMap, "rxFailed", pStationInfo->rxErrors);
    amxc_var_add_key(int8_t, pMap, "rssiDbm", pStationInfo->rssiDbm);
    amxc_var_add_key(int8_t, pMap, "rssiAvgDbm", pStationInfo->rssiAvgDbm);
    amxc_var_t* rxRate = amxc_var_add_key(amxc_htable_t, pMap, "rxRate", NULL);
    wld_nl80211_dumpRateInfo(&pStationInfo->rxRate, rxRate);
    amxc_var_t* txRate = amxc_var_add_key(amxc_htable_t, pMap, "txRate", NULL);
    wld_nl80211_dumpRateInfo(&pStationInfo->txRate, txRate);
    if(retMap) {
        s_dumpMap(pMap);
    } else {
        s_dumpVar(&localVar);
    }
    amxc_var_clean(&localVar);
    return SWL_RC_OK;
}

swl_rc_ne wld_nl80211_dumpAllStationInfo(wld_nl80211_stationInfo_t* pAllStationInfo, uint32_t nStation, amxc_var_t* retMap) {
    ASSERTS_NOT_NULL(pAllStationInfo, SWL_RC_OK, ME, "No data");
    ASSERTS_NOT_EQUALS(nStation, 0, SWL_RC_OK, ME, "No data");
    amxc_var_t localVar;
    amxc_var_init(&localVar);
    amxc_var_set_type(&localVar, AMXC_VAR_ID_HTABLE);
    amxc_var_t* pMap = (retMap ? retMap : &localVar);
    ASSERTS_NOT_NULL(pMap, SWL_RC_ERROR, ME, "NULL");

    swl_macChar_t macStr;
    for(uint32_t i = 0; i < nStation; i++) {
        SWL_MAC_BIN_TO_CHAR(&macStr, &pAllStationInfo[i].macAddr);
        amxc_var_t* staMap = amxc_var_add_key(amxc_htable_t, pMap, macStr.cMac, NULL);
        wld_nl80211_dumpStationInfo(&pAllStationInfo[i], staMap);
    }
    if(retMap) {
        s_dumpMap(pMap);
    } else {
        s_dumpVar(&localVar);
    }
    amxc_var_clean(&localVar);
    return SWL_RC_OK;
}

