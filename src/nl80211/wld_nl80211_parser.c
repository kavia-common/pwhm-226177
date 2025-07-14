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
 * This file implement nl80211 msg parsing
 */

#include "wld_nl80211_parser.h"
#include "wld_nl80211_utils.h"
#include "wld_util.h"
#include "swl/swl_genericFrameParser.h"

#define ME "nlParser"
#define WLAN_CAPABILITY_ESS BIT(0)
#define WLAN_CAPABILITY_IBSS BIT(1)

/*
 * @brief fetch Wiphy ID from nl80211 attributes
 *
 * @param tb nl attributes array
 *
 * @return uint32 valid wiphy ID if found
 *         WLD_NL80211_ID_UNDEF (-1) in case of error
 */
uint32_t wld_nl80211_getWiphy(struct nlattr* tb[]) {
    ASSERTS_NOT_NULL(tb, WLD_NL80211_ID_UNDEF, ME, "NULL");
    if(tb[NL80211_ATTR_WIPHY]) {
        return nla_get_u32(tb[NL80211_ATTR_WIPHY]);
    }
    /*
     * in some msg, wiphy attribute is missing, but wider wdev identifier
     * is provided, which includes the id
     */
    if(tb[NL80211_ATTR_WDEV]) {
        uint64_t wdev_id = nla_get_u64(tb[NL80211_ATTR_WDEV]);
        return (uint32_t) (wdev_id >> 32);
    }
    return WLD_NL80211_ID_UNDEF;
}

/*
 * @brief fetch interface netdev index from nl80211 attributes
 *
 * @param tb nl attributes array
 *
 * @return uint32 valid netdev index if found
 *         WLD_NL80211_ID_UNDEF (-1) in case of error
 */
uint32_t wld_nl80211_getIfIndex(struct nlattr* tb[]) {
    uint32_t ifIndex = WLD_NL80211_ID_UNDEF;
    ASSERTS_NOT_NULL(tb, ifIndex, ME, "NULL");
    NLA_GET_VAL(ifIndex, nla_get_u32, tb[NL80211_ATTR_IFINDEX]);
    return ifIndex;
}

/*
 * @brief fetch interface netdev name from nl80211 attributes
 *
 * @param tb nl attributes array
 * @param tgtBuf target buffer
 * @param tgtBufSize target buffer size
 *
 * @return size_t length of the retrieved string
 */
size_t wld_nl80211_getIfName(struct nlattr* tb[], char* tgtBuf, size_t tgtBufSize) {
    ASSERTS_NOT_NULL(tb, 0, ME, "NULL");
    ASSERTS_NOT_NULL(tgtBuf, 0, ME, "NULL");
    ASSERTS_TRUE(tgtBufSize > 0, 0, ME, "NULL");
    memset(tgtBuf, 0, tgtBufSize);
    NLA_GET_DATA(tgtBuf, tb[NL80211_ATTR_IFNAME], tgtBufSize - 1);
    return swl_str_len(tgtBuf);
}

swl_rc_ne wld_nl80211_parseChanSpec(struct nlattr* tb[], wld_nl80211_chanSpec_t* pChanSpec) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERTS_NOT_NULL(tb, rc, ME, "NULL");
    ASSERTS_NOT_NULL(pChanSpec, rc, ME, "NULL");
    rc = SWL_RC_OK;
    ASSERTS_NOT_NULL(tb[NL80211_ATTR_WIPHY_FREQ], rc, ME, "no freq");
    pChanSpec->ctrlFreq = nla_get_u32(tb[NL80211_ATTR_WIPHY_FREQ]);
    ASSERTS_NOT_NULL(tb[NL80211_ATTR_CHANNEL_WIDTH], rc, ME, "no chan width");
    uint32_t bwVal = wld_nl80211_bwNlToVal(nla_get_u32(tb[NL80211_ATTR_CHANNEL_WIDTH]));
    if(bwVal > 0) {
        pChanSpec->chanWidth = bwVal;
    }
    NLA_GET_VAL(pChanSpec->centerFreq1, nla_get_u32, tb[NL80211_ATTR_CENTER_FREQ1]);
    NLA_GET_VAL(pChanSpec->centerFreq2, nla_get_u32, tb[NL80211_ATTR_CENTER_FREQ2]);
    return SWL_RC_DONE;
}

swl_rc_ne wld_nl80211_formatChanSpec(wld_nl80211_chanSpec_t* pChanSpec, wld_nl80211_nlAttrList_t* pAttrList) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_NOT_NULL(pAttrList, rc, ME, "NULL");
    ASSERT_NOT_NULL(pChanSpec, rc, ME, "NULL");
    ASSERT_TRUE(pChanSpec->ctrlFreq > 0, rc, ME, "invalid freq");
    NL_ATTRS_ADD(pAttrList, NL_ATTR_VAL(NL80211_ATTR_WIPHY_FREQ, pChanSpec->ctrlFreq));
    ASSERTI_TRUE(pChanSpec->chanWidth > 0, SWL_RC_OK, ME, "no chan width");
    int32_t bwId = wld_nl80211_bwValToNl(pChanSpec->chanWidth);
    ASSERT_NOT_EQUALS(bwId, -1, rc, ME, "unknown chan width (%d)", pChanSpec->chanWidth);
    NL_ATTRS_ADD(pAttrList, NL_ATTR_DATA(NL80211_ATTR_CHANNEL_WIDTH, sizeof(bwId), &bwId));
    if(pChanSpec->centerFreq1 > 0) {
        NL_ATTRS_ADD(pAttrList, NL_ATTR_VAL(NL80211_ATTR_CENTER_FREQ1, pChanSpec->centerFreq1));
    }
    if(pChanSpec->centerFreq2 > 0) {
        NL_ATTRS_ADD(pAttrList, NL_ATTR_VAL(NL80211_ATTR_CENTER_FREQ2, pChanSpec->centerFreq2));
    }
    return SWL_RC_DONE;
}

swl_rc_ne wld_nl80211_parseMgmtFrame(struct nlattr* tb[], wld_nl80211_mgmtFrame_t* mgmtFrame) {
    ASSERTS_NOT_NULL(mgmtFrame, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(tb, SWL_RC_INVALID_PARAM, ME, "NULL");
    mgmtFrame->frameLen = nla_len(tb[NL80211_ATTR_FRAME]);
    mgmtFrame->frame = swl_80211_getMgmtFrame((swl_bit8_t*) nla_data(tb[NL80211_ATTR_FRAME]), mgmtFrame->frameLen);
    ASSERTS_NOT_NULL(mgmtFrame->frame, SWL_RC_INVALID_PARAM, ME, "Invalid frame");
    uint32_t rssi = 0;
    NLA_GET_VAL(rssi, nla_get_u32, tb[NL80211_ATTR_RX_SIGNAL_DBM]);
    ASSERTS_NOT_EQUALS(rssi, 0, SWL_RC_INVALID_PARAM, ME, "Invalid RSSI");
    mgmtFrame->rssi = (int32_t) rssi;
    return SWL_RC_OK;
}

swl_rc_ne wld_nl80211_parseMgmtFrameTxStatus(struct nlattr* tb[], wld_nl80211_mgmtFrameTxStatus_t* mgmtFrameTxStatus) {
    ASSERTS_NOT_NULL(mgmtFrameTxStatus, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(tb, SWL_RC_INVALID_PARAM, ME, "NULL");
    mgmtFrameTxStatus->frameLen = nla_len(tb[NL80211_ATTR_FRAME]);
    mgmtFrameTxStatus->frame = swl_80211_getMgmtFrame((swl_bit8_t*) nla_data(tb[NL80211_ATTR_FRAME]), mgmtFrameTxStatus->frameLen);
    ASSERTS_NOT_NULL(mgmtFrameTxStatus->frame, SWL_RC_INVALID_PARAM, ME, "Invalid frame");
    mgmtFrameTxStatus->ack = (tb[NL80211_ATTR_ACK] != NULL);
    return SWL_RC_OK;
}

static swl_rc_ne s_parseMloLink(struct nlattr* tb[], wld_nl80211_mloLinkInfo_t* pMloLink) {
    ASSERTS_NOT_NULL(pMloLink, SWL_RC_INVALID_PARAM, ME, "NULL");
    memset(pMloLink, 0, sizeof(*pMloLink));
    pMloLink->linkId = -1;
    ASSERTS_NOT_NULL(tb, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(tb[NL80211_ATTR_MLO_LINK_ID], SWL_RC_ERROR, ME, "no link id attrib");
    pMloLink->linkId = nla_get_u8(tb[NL80211_ATTR_MLO_LINK_ID]);
    NLA_GET_DATA(pMloLink->linkMac.bMac, tb[NL80211_ATTR_MAC], SWL_MAC_BIN_LEN);
    NLA_GET_DATA(pMloLink->mldMac.bMac, tb[NL80211_ATTR_MLD_ADDR], SWL_MAC_BIN_LEN);
    NLA_GET_VAL(pMloLink->stats.txBytes, nla_get_u64, tb[NL80211_STA_INFO_TX_BYTES64]);
    NLA_GET_VAL(pMloLink->stats.rxBytes, nla_get_u64, tb[NL80211_STA_INFO_RX_BYTES64]);
    NLA_GET_VAL(pMloLink->stats.txPackets, nla_get_u32, tb[NL80211_STA_INFO_TX_PACKETS]);
    NLA_GET_VAL(pMloLink->stats.rxPackets, nla_get_u32, tb[NL80211_STA_INFO_RX_PACKETS]);
    NLA_GET_VAL(pMloLink->stats.txRetries, nla_get_u32, tb[NL80211_STA_INFO_TX_RETRIES]);
    NLA_GET_VAL(pMloLink->stats.rxRetries, nla_get_u32, tb[NL80211_STA_INFO_RX_RETRIES]);
    NLA_GET_VAL(pMloLink->stats.txErrors, nla_get_u32, tb[NL80211_STA_INFO_TX_FAILED]);
    NLA_GET_VAL(pMloLink->stats.rxErrors, nla_get_u64, tb[NL80211_STA_INFO_RX_DROP_MISC]);
    NLA_GET_VAL(pMloLink->stats.rssiDbm, nla_get_u8, tb[NL80211_STA_INFO_SIGNAL]);
    NLA_GET_VAL(pMloLink->stats.rssiAvgDbm, nla_get_u8, tb[NL80211_STA_INFO_SIGNAL_AVG]);
    return SWL_RC_OK;
}

static uint32_t s_parseMloLinks(struct nlattr* tb[], wld_nl80211_ifaceMloLinkInfo_t pIfaceMloLinks[], uint32_t maxLinks) {
    uint32_t nMloLinks = 0;
    ASSERTS_NOT_NULL(tb, nMloLinks, ME, "NULL");
    ASSERTS_NOT_NULL(pIfaceMloLinks, nMloLinks, ME, "null target");
    ASSERTS_TRUE(maxLinks > 0, nMloLinks, ME, "empty target");
    memset(pIfaceMloLinks, 0, maxLinks * sizeof(*pIfaceMloLinks));
    ASSERTS_NOT_NULL(tb[NL80211_ATTR_MLO_LINKS], nMloLinks, ME, "No attr to parse");
    int rem = 0;
    struct nlattr* list;
    struct nlattr* link[NL80211_ATTR_MAX + 1];
    nla_for_each_nested(list, tb[NL80211_ATTR_MLO_LINKS], rem) {
        nla_parse_nested(link, NL80211_ATTR_MAX, list, NULL);
        wld_nl80211_mloLinkInfo_t linkInfo;
        if((s_parseMloLink(link, &linkInfo) < SWL_RC_OK) || (linkInfo.linkId < 0) || (swl_mac_binIsNull(&linkInfo.linkMac))) {
            SAH_TRACEZ_WARNING(ME, "skip link %d: missing info", linkInfo.linkId);
            continue;
        }
        if(nMloLinks >= maxLinks) {
            SAH_TRACEZ_WARNING(ME, "skip linkId %d: nlinks exceeds max %d", linkInfo.linkId, maxLinks);
            continue;
        }
        if(swl_mac_binIsNull(&linkInfo.mldMac)) {
            NLA_GET_DATA(linkInfo.mldMac.bMac, tb[NL80211_ATTR_MAC], SWL_MAC_BIN_LEN);
        }
        wld_nl80211_ifaceMloLinkInfo_t* pIfaceLinkInfo = &pIfaceMloLinks[nMloLinks];
        pIfaceLinkInfo->link = linkInfo;
        pIfaceLinkInfo->linkPos = nMloLinks;
        pIfaceLinkInfo->mldIfIndex = wld_nl80211_getIfIndex(tb);
        wld_nl80211_parseChanSpec(link, &pIfaceLinkInfo->chanSpec);
        NLA_GET_VAL(pIfaceLinkInfo->txPower, nla_get_u32, link[NL80211_ATTR_WIPHY_TX_POWER_LEVEL]);
        pIfaceLinkInfo->txPower /= 100;
        nMloLinks++;
    }
    return nMloLinks;
}

swl_rc_ne wld_nl80211_parseInterfaceInfo(struct nlattr* tb[], wld_nl80211_ifaceInfo_t* pWlIface) {
    ASSERT_NOT_NULL(pWlIface, SWL_RC_INVALID_PARAM, ME, "NULL");
    memset(pWlIface, 0, sizeof(*pWlIface));
    ASSERT_NOT_NULL(tb, SWL_RC_INVALID_PARAM, ME, "NULL");
    pWlIface->wiphy = wld_nl80211_getWiphy(tb);
    pWlIface->ifIndex = wld_nl80211_getIfIndex(tb);
    wld_nl80211_getIfName(tb, pWlIface->name, sizeof(pWlIface->name));
    uint32_t ifType = NL80211_IFTYPE_UNSPECIFIED;
    NLA_GET_VAL(ifType, nla_get_u32, tb[NL80211_ATTR_IFTYPE]);
    pWlIface->isAp = (ifType == NL80211_IFTYPE_AP);
    pWlIface->isSta = (ifType == NL80211_IFTYPE_STATION);
    uint64_t wdev_id = 0;
    NLA_GET_VAL(wdev_id, nla_get_u64, tb[NL80211_ATTR_WDEV]);
    pWlIface->wDevId = wdev_id;
    pWlIface->isMain = ((wdev_id & 0xffffffff) == 1);
    NLA_GET_DATA(pWlIface->mac.bMac, tb[NL80211_ATTR_MAC], SWL_MAC_BIN_LEN);
    wld_nl80211_parseChanSpec(tb, &pWlIface->chanSpec);
    NLA_GET_VAL(pWlIface->txPower, nla_get_u32, tb[NL80211_ATTR_WIPHY_TX_POWER_LEVEL]);
    pWlIface->txPower /= 100;
    NLA_GET_VAL(pWlIface->use4Mac, nla_get_u8, tb[NL80211_ATTR_4ADDR]);
    NLA_GET_DATA(pWlIface->ssid, tb[NL80211_ATTR_SSID], (sizeof(pWlIface->ssid) - 1));
    pWlIface->nMloLinks = s_parseMloLinks(tb, pWlIface->mloLinks, SWL_ARRAY_SIZE(pWlIface->mloLinks));
    return SWL_RC_OK;
}

swl_rc_ne s_parseIfTypes(struct nlattr* tb[], wld_nl80211_wiphyInfo_t* pWiphy) {
    ASSERTS_NOT_NULL(tb, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pWiphy, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(tb[NL80211_ATTR_SUPPORTED_IFTYPES], SWL_RC_OK, ME, "No attr to parse");
    struct nlattr* nlMode;
    int rem;
    nla_for_each_nested(nlMode, tb[NL80211_ATTR_SUPPORTED_IFTYPES], rem) {
        if(nla_type(nlMode) == NL80211_IFTYPE_STATION) {
            pWiphy->suppEp = true;
        } else if(nla_type(nlMode) == NL80211_IFTYPE_AP) {
            pWiphy->suppAp = true;
        }
    }
    return SWL_RC_OK;
}
swl_rc_ne s_parseIfCombi(struct nlattr* tb[], wld_nl80211_wiphyInfo_t* pWiphy) {
    ASSERTS_NOT_NULL(tb, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pWiphy, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(tb[NL80211_ATTR_INTERFACE_COMBINATIONS], SWL_RC_OK, ME, "No attr to parse");
    struct nlattr* nlComb;
    int remComb;
    nla_for_each_nested(nlComb, tb[NL80211_ATTR_INTERFACE_COMBINATIONS], remComb) {
        static struct nla_policy ifaceCombinationPolicy[NUM_NL80211_IFACE_COMB] = {
            [NL80211_IFACE_COMB_LIMITS] = { .type = NLA_NESTED },
            [NL80211_IFACE_COMB_MAXNUM] = { .type = NLA_U32 },
            [NL80211_IFACE_COMB_NUM_CHANNELS] = { .type = NLA_U32 },
        };
        struct nlattr* tbComb[NUM_NL80211_IFACE_COMB];
        static struct nla_policy ifaceLimitPolicy[NUM_NL80211_IFACE_LIMIT] = {
            [NL80211_IFACE_LIMIT_TYPES] = { .type = NLA_NESTED },
            [NL80211_IFACE_LIMIT_MAX] = { .type = NLA_U32 },
        };
        int err = nla_parse_nested(tbComb, MAX_NL80211_IFACE_COMB,
                                   nlComb, ifaceCombinationPolicy);
        if(err || !tbComb[NL80211_IFACE_COMB_LIMITS] || !tbComb[NL80211_IFACE_COMB_MAXNUM]) {
            continue;
        }
        struct nlattr* tbLimit[NUM_NL80211_IFACE_LIMIT];
        struct nlattr* nlLimit;
        int remLimit;
        nla_for_each_nested(nlLimit, tbComb[NL80211_IFACE_COMB_LIMITS], remLimit) {
            err = nla_parse_nested(tbLimit, MAX_NL80211_IFACE_LIMIT,
                                   nlLimit, ifaceLimitPolicy);
            if(err || !tbLimit[NL80211_IFACE_LIMIT_TYPES]) {
                break;
            }
            struct nlattr* nlMode;
            int remMode;
            nla_for_each_nested(nlMode, tbLimit[NL80211_IFACE_LIMIT_TYPES], remMode) {
                uint32_t limit = nla_get_u32(tbLimit[NL80211_IFACE_LIMIT_MAX]);
                if(nla_type(nlMode) == NL80211_IFTYPE_AP) {
                    pWiphy->nApMax = limit;
                } else if(nla_type(nlMode) == NL80211_IFTYPE_STATION) {
                    pWiphy->nEpMax = limit;
                }
            }
        }
    }
    return SWL_RC_OK;
}

typedef struct {
    uint16_t b_0 : 1;
    uint16_t support_ch_width_set : 1;
    uint16_t sm_power_save : 2;
    uint16_t ht_greenfield : 1;
    uint16_t short_gi20mhz : 1;
    uint16_t short_gi40mhz : 1;
    uint16_t b_7_15 : 9;
} __attribute__((packed)) htCapabilityInfo_t;

/*
 * As defined in 7.3.2.57.4 Supported MCS Set field:
 * BitMask: B0..B76 + 3 reserved bits => 10 Bytes
 */
#define HT_MCS_BITMASK_LEN 10

swl_rc_ne s_parseHtAttrs(struct nlattr* tbBand[], wld_nl80211_bandDef_t* pBand) {
    ASSERTS_NOT_NULL(tbBand, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pBand, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(tbBand[NL80211_BAND_ATTR_HT_CAPA], SWL_RC_OK, ME, "no ht cap");
    ASSERTS_NOT_NULL(tbBand[NL80211_BAND_ATTR_HT_MCS_SET], SWL_RC_OK, ME, "no ht mcs set");
    swl_mcs_t* pMcsStd = &pBand->mcsStds[SWL_MCS_STANDARD_HT];
    htCapabilityInfo_t htCapInfo = {0};
    NLA_GET_DATA(&htCapInfo, tbBand[NL80211_BAND_ATTR_HT_CAPA], sizeof(htCapInfo));
    pMcsStd->standard = SWL_MCS_STANDARD_HT;
    //HT20 / HT40
    pMcsStd->bandwidth = SWL_BW_20MHZ;
    if(htCapInfo.support_ch_width_set) {
        pMcsStd->bandwidth = SWL_BW_40MHZ;
        pBand->chanWidthMask |= M_SWL_BW_40MHZ;
    }

    memcpy(&pBand->htCapabilities, &htCapInfo, sizeof(pBand->htCapabilities));// since structures are the same, safe to do a memcpy

    //RX HT20 SGI / RX HT40 SGI
    pMcsStd->guardInterval = SWL_SGI_800;
    if(htCapInfo.short_gi20mhz || htCapInfo.short_gi40mhz) {
        pMcsStd->guardInterval = SWL_SGI_400;
    }

    NLA_GET_DATA(&pBand->htMcsSet, tbBand[NL80211_BAND_ATTR_HT_MCS_SET], sizeof(pBand->htMcsSet));
    uint32_t htRxMcsBitMask[3] = {0};
    memcpy(htRxMcsBitMask, pBand->htMcsSet, HT_MCS_BITMASK_LEN);
    int32_t highestIdx = swl_bitArr32_getHighest(htRxMcsBitMask, SWL_ARRAY_SIZE(htRxMcsBitMask));
    pMcsStd->mcsIndex = SWL_MAX(0, highestIdx);
    pMcsStd->numberOfSpatialStream = (pMcsStd->mcsIndex / 8) + 1;

    pBand->radStdsMask |= M_SWL_RADSTD_N;
    pBand->nSSMax = SWL_MAX(pBand->nSSMax, pMcsStd->numberOfSpatialStream);
    return SWL_RC_DONE;
}

typedef struct {
    uint32_t max_mpdu_len : 2;
    uint32_t support_ch_width_set : 2;
    uint32_t rx_ldpc : 1;
    uint32_t short_gi80mhz_tvht_mode4c : 1;
    uint32_t short_gi160mhz80_80mhz : 1;
    uint32_t tx_stbc : 1;
    uint32_t rx_stbc : 3;
    uint32_t su_beamformer : 1;           // SU beamformer capable
    uint32_t su_beamformee : 1;           // SU beamformee capable
    uint32_t beamformee_sts : 3;          // Beamformee STS capability
    uint32_t sound_dimensions : 3;        // Number of sounding dimensions
    uint32_t mu_beamformer : 1;           // MU beamformer capable
    uint32_t mu_beamformee : 1;           // MU beamformee capable
    uint32_t b_22_32 : 11;
} __attribute__((packed)) vhtCapabilityInfo_t;

swl_rc_ne s_parseVhtAttrs(struct nlattr* tbBand[], wld_nl80211_bandDef_t* pBand) {
    ASSERTS_NOT_NULL(tbBand, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pBand, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(tbBand[NL80211_BAND_ATTR_VHT_CAPA], SWL_RC_OK, ME, "no vht cap");
    ASSERTS_NOT_NULL(tbBand[NL80211_BAND_ATTR_VHT_MCS_SET], SWL_RC_OK, ME, "no vht mcs set");
    ASSERTI_NOT_EQUALS(pBand->freqBand, SWL_FREQ_BAND_2_4GHZ, SWL_RC_OK, ME, "skip vht cap in 2.4g band");
    swl_mcs_t* pMcsStd = &pBand->mcsStds[SWL_MCS_STANDARD_VHT];
    vhtCapabilityInfo_t vhtCapInfo = {0};
    NLA_GET_DATA(&vhtCapInfo, tbBand[NL80211_BAND_ATTR_VHT_CAPA], sizeof(vhtCapInfo));
    pMcsStd->standard = SWL_MCS_STANDARD_VHT;
    //Supported Channel Width
    pMcsStd->bandwidth = SWL_BW_80MHZ;
    pBand->chanWidthMask |= (M_SWL_BW_40MHZ | M_SWL_BW_80MHZ);
    switch(vhtCapInfo.support_ch_width_set) {
    //(160 MHz )or (160 MHz/80+80 MHz)
    case 1:
    case 2: {
        pMcsStd->bandwidth = SWL_BW_160MHZ;
        pBand->chanWidthMask |= M_SWL_BW_160MHZ;
        break;
    }
    default: break;
    }
    //short GI (80 MHz) / short GI (160/80+80 MHz)
    pMcsStd->guardInterval = SWL_SGI_800;
    if(vhtCapInfo.short_gi80mhz_tvht_mode4c || vhtCapInfo.short_gi160mhz80_80mhz) {
        pMcsStd->guardInterval = SWL_SGI_400;
    }
    //SU Beamformer
    if(vhtCapInfo.su_beamformer) {
        pBand->bfCapsSupported[COM_DIR_TRANSMIT] |= M_RAD_BF_CAP_VHT_SU;
    }
    //SU Beamformee
    if(vhtCapInfo.su_beamformee) {
        pBand->bfCapsSupported[COM_DIR_RECEIVE] |= M_RAD_BF_CAP_VHT_SU;
    }
    //MU Beamformer
    if(vhtCapInfo.mu_beamformer) {
        pBand->bfCapsSupported[COM_DIR_TRANSMIT] |= M_RAD_BF_CAP_VHT_MU;
    }
    //MU Beamformee
    if(vhtCapInfo.mu_beamformee) {
        pBand->bfCapsSupported[COM_DIR_RECEIVE] |= M_RAD_BF_CAP_VHT_MU;
    }

    memcpy(&pBand->vhtCapabilities, &vhtCapInfo, sizeof(pBand->vhtCapabilities));// since structures are the same, safe to do a memcpy

    /*
     * As defined in 9.4.2.157.3 Supported VHT-MCS and NSS Set field: B0..B63
     * Rx VHT-MCS Map: Figure 9-611—Rx VHT-MCS Map: B0..B16
     */
    NLA_GET_DATA(&pBand->vhtMcsNssSet, tbBand[NL80211_BAND_ATTR_VHT_MCS_SET], sizeof(pBand->vhtMcsNssSet));
    uint16_t mcsMapRx = 0xffff;
    memcpy(&mcsMapRx, pBand->vhtMcsNssSet, sizeof(mcsMapRx));

    pMcsStd->numberOfSpatialStream = 1;
    pMcsStd->mcsIndex = 0;
    for(uint8_t ssId = 0; ssId < 8; ssId++) {  // up to 8ss
        uint8_t mcsMapPerSS = (mcsMapRx >> (2 * ssId)) & 0x03;
        if(mcsMapPerSS < 3) {
            pMcsStd->numberOfSpatialStream = SWL_MAX((int32_t) pMcsStd->numberOfSpatialStream, (ssId + 1));
            pMcsStd->mcsIndex = SWL_MAX((int32_t) pMcsStd->mcsIndex, (mcsMapPerSS + 7));
        }
    }

    pBand->radStdsMask |= M_SWL_RADSTD_AC;
    pBand->nSSMax = SWL_MAX(pBand->nSSMax, pMcsStd->numberOfSpatialStream);
    return SWL_RC_DONE;
}

typedef struct {
    uint8_t b_0 : 1;
    uint8_t twt_requester_support : 1;
    uint8_t twt_responder_support : 1;
    uint8_t b_3_7 : 5;
    uint16_t b_8_23;
    uint16_t b_24_39;
    uint8_t b_40_47;
} SWL_PACKED heMacCapInfo_t;

typedef struct {
    uint8_t b_0 : 1;
    uint8_t supp_40mhz_in_2_4ghz : 1;
    uint8_t supp_40_80mhz_in_5ghz : 1;
    uint8_t supp_160mhz_in_5ghz : 1;
    uint8_t supp_160_80p80mhz_in_5ghz : 1;
    uint8_t b_5_7 : 3;
    uint16_t b_8_21 : 14;
    uint16_t full_bw_ul_mumimo : 1;
    uint16_t partial_bw_ul_mumimo : 1;
    uint16_t b_24_30 : 7;
    uint16_t su_beamformer : 1;
    uint16_t su_beamformee : 1;
    uint16_t mu_beamformer : 1;
    uint16_t beamformee_sts_le_80mhz : 3;
    uint16_t beamformee_sts_gt_80mhz : 3;
    uint16_t b_40_55;
    uint16_t b_56_71;
    uint16_t b_72_87;
} SWL_PACKED hePhyCapInfo_t;

static bool s_isSupportedNlIfType(struct nlattr* tbIfType) {
    struct nlattr* ift = NULL;
    int rem = 0;
    if(tbIfType == NULL) {
        return true;
    }
    nla_for_each_nested(ift, tbIfType, rem) {
        if((nla_type(ift) == NL80211_IFTYPE_STATION) ||
           (nla_type(ift) == NL80211_IFTYPE_AP)) {
            return true;
        }
    }
    return false;
}

swl_rc_ne s_parseHeAttrs(struct nlattr* tbBand[], wld_nl80211_bandDef_t* pBand) {
    ASSERTS_NOT_NULL(tbBand, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pBand, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(tbBand[NL80211_BAND_ATTR_IFTYPE_DATA], SWL_RC_OK, ME, "no iftype data");
    struct nlattr* nlIfType = NULL;
    int rem = 0;
    struct nlattr* tbIfType[NL80211_BAND_IFTYPE_ATTR_HE_6GHZ_CAPA + 1] = {};
    nla_for_each_nested(nlIfType, tbBand[NL80211_BAND_ATTR_IFTYPE_DATA], rem) {
        nla_parse(tbIfType, NL80211_BAND_IFTYPE_ATTR_HE_6GHZ_CAPA, nla_data(nlIfType), nla_len(nlIfType), NULL);
        if(!s_isSupportedNlIfType(tbIfType[NL80211_BAND_IFTYPE_ATTR_IFTYPES])) {
            continue;
        }
        struct nlattr* heCapMacAttr = tbIfType[NL80211_BAND_IFTYPE_ATTR_HE_CAP_MAC];
        struct nlattr* heCapPhyAttr = tbIfType[NL80211_BAND_IFTYPE_ATTR_HE_CAP_PHY];
        struct nlattr* heCapMcsSetAttr = tbIfType[NL80211_BAND_IFTYPE_ATTR_HE_CAP_MCS_SET];
        if(!heCapMacAttr || !heCapPhyAttr || !heCapMcsSetAttr) {
            continue;
        }

        /* 9.4.2.248.2 HE MAC Capabilities Information field: B0..B47 */
        heMacCapInfo_t heMacCapInfo = {0};
        NLA_GET_DATA(&heMacCapInfo, heCapMacAttr, sizeof(heMacCapInfo));
        /* since structures are the same, it is safe to do a memcpy() */
        memcpy(&pBand->heMacCapabilities, &heMacCapInfo, sizeof(pBand->heMacCapabilities));

        /*
         * As defined in 9.4.2.248.3 HE PHY Capabilities Information field: B0..B87
         */
        swl_mcs_t* pMcsStd = &pBand->mcsStds[SWL_MCS_STANDARD_HE];
        hePhyCapInfo_t hePhyCapInfo = {0};
        NLA_GET_DATA(&hePhyCapInfo, heCapPhyAttr, sizeof(hePhyCapInfo));
        memcpy(&pBand->hePhyCapabilities, &hePhyCapInfo, sizeof(pBand->hePhyCapabilities));

        pMcsStd->standard = SWL_MCS_STANDARD_HE;
        pMcsStd->bandwidth = SWL_BW_20MHZ;
        if(pBand->freqBand == SWL_FREQ_BAND_2_4GHZ) {
            if(hePhyCapInfo.supp_40mhz_in_2_4ghz) {
                pMcsStd->bandwidth = SWL_BW_40MHZ;
                pBand->chanWidthMask |= M_SWL_BW_40MHZ;
            }
        } else {
            if(hePhyCapInfo.supp_40_80mhz_in_5ghz) {
                pMcsStd->bandwidth = SWL_BW_80MHZ;
                pBand->chanWidthMask |= (M_SWL_BW_40MHZ | M_SWL_BW_80MHZ);
            }
            if(hePhyCapInfo.supp_160mhz_in_5ghz || hePhyCapInfo.supp_160_80p80mhz_in_5ghz) {
                pMcsStd->bandwidth = SWL_BW_160MHZ;
                pBand->chanWidthMask |= M_SWL_BW_160MHZ;
            }
        }
        /*
         * GI 800ns mandatory in HE
         * Cf: IEEE80211 27.3.12 Data field
         * An HE STA shall support Data field OFDM symbols in an HE PPDU with guard interval
         * durations of 0.8 µs, 1.6 µs, and 3.2 µs.
         */
        pMcsStd->guardInterval = SWL_SGI_800;
        //SU Beamformer
        if(hePhyCapInfo.su_beamformer) {
            pBand->bfCapsSupported[COM_DIR_TRANSMIT] |= M_RAD_BF_CAP_HE_SU;
        }
        //SU Beamformee
        if(hePhyCapInfo.su_beamformee) {
            pBand->bfCapsSupported[COM_DIR_RECEIVE] |= M_RAD_BF_CAP_HE_SU;
        }
        //MU Beamformer
        if(hePhyCapInfo.mu_beamformer) {
            pBand->bfCapsSupported[COM_DIR_TRANSMIT] |= M_RAD_BF_CAP_HE_MU;
        }
        //Beamformee STS (<= 80Mhz or > 80MHz)
        if(hePhyCapInfo.beamformee_sts_le_80mhz || hePhyCapInfo.beamformee_sts_gt_80mhz) {
            pBand->bfCapsSupported[COM_DIR_RECEIVE] |= M_RAD_BF_CAP_HE_MU;
        }

        /*
         * As defined in 9.4.2.248.4 Supported HE-MCS And NSS Set field:
         * RX/TX MCS Maps, per supported chan width group
         */
        NLA_GET_DATA(&pBand->heMcsCaps, heCapMcsSetAttr, sizeof(pBand->heMcsCaps));

        pMcsStd->numberOfSpatialStream = 1;
        pMcsStd->mcsIndex = 0;
        for(uint8_t ssId = 0; ssId < 8; ssId++) {  // up to 8ss
            uint8_t mcsMapPerSS = (pBand->heMcsCaps[0].rxMcsMap >> (2 * ssId)) & 0x03;
            if(mcsMapPerSS < 3) {
                pMcsStd->numberOfSpatialStream = SWL_MAX((int32_t) pMcsStd->numberOfSpatialStream, (ssId + 1));
                pMcsStd->mcsIndex = SWL_MAX((int32_t) pMcsStd->mcsIndex, (7 + (mcsMapPerSS * 2)));
            }
        }

        pBand->nSSMax = SWL_MAX(pBand->nSSMax, pMcsStd->numberOfSpatialStream);
        pBand->radStdsMask |= M_SWL_RADSTD_AX;

        if(tbIfType[NL80211_BAND_IFTYPE_ATTR_HE_6GHZ_CAPA] != NULL) {
            /* TODO */
        }
    }

    return SWL_RC_DONE;
}

static void s_parseEhtCapPhy(const struct nlattr* tbIfType, wld_nl80211_bandDef_t* pBand) {
    ASSERTS_NOT_NULL(tbIfType, , ME, "NULL)");

    swl_80211_ehtcap_phyCapInfo_t* phyCaps = &pBand->ehtPhyCapabilities;
    swl_mcs_t* pMcsStd = &pBand->mcsStds[SWL_MCS_STANDARD_EHT];
    NLA_GET_DATA(phyCaps, tbIfType, sizeof(swl_80211_ehtcap_phyCapInfo_t));

    if(memcmp(phyCaps, &((swl_80211_ehtcap_phyCapInfo_t) {0}), sizeof(swl_80211_ehtcap_phyCapInfo_t)) == 0) {
        return;
    }

    pBand->radStdsMask |= M_SWL_RADSTD_BE;

    /* Support For 320MHz In 6 GHz */
    if(phyCaps->support_320mhz &&
       (pBand->freqBand == SWL_FREQ_BAND_6GHZ)) {
        pBand->chanWidthMask |= M_SWL_BW_320MHZ;
        pMcsStd->bandwidth = SWL_BW_320MHZ;
    }
    /* Rx 1024-QAM in wider Bandwidth DL OFDMA */
    if(phyCaps->rx_1024qam_wider_bw_dl_ofdma_support) {
        pMcsStd->mcsIndex = SWL_MAX(pMcsStd->mcsIndex, (uint32_t) 11);
    }
    /* Rx 4096-QAM in wider Bandwidth DL OFDMA */
    if(phyCaps->rx_4096qam_wider_bw_dl_ofdma_support) {
        pMcsStd->mcsIndex = SWL_MAX(pMcsStd->mcsIndex, (uint32_t) 13);
    }
    /* Support Of EHT DUP (EHT-MCS 14) in 6 GHz */
    if(phyCaps->support_ehtdup_mcs14_6ghz &&
       (pBand->freqBand == SWL_FREQ_BAND_6GHZ)) {
        pMcsStd->mcsIndex = SWL_MAX(pMcsStd->mcsIndex, (uint32_t) 14);
        pMcsStd->numberOfSpatialStream = 1;
    }
    /* Support Of EHT-MCS 15 in MRU */
    if(phyCaps->support_eht_mcs15_mru != 0) {
        pMcsStd->mcsIndex = SWL_MAX(pMcsStd->mcsIndex, (uint32_t) 15);
        pMcsStd->numberOfSpatialStream = 1;
    }
    /* SU Beamformer */
    if(phyCaps->su_beamformer) {
        pBand->bfCapsSupported[COM_DIR_TRANSMIT] |= M_RAD_BF_CAP_EHT_SU;
    }
    /* SU Beamformee */
    if(phyCaps->su_beamformee) {
        pBand->bfCapsSupported[COM_DIR_RECEIVE] |= M_RAD_BF_CAP_EHT_SU;
    }
    /* MU Beamformer (<= 80MHz) */
    if(phyCaps->mu_beamformer_80mhz) {
        pBand->bfCapsSupported[COM_DIR_TRANSMIT] |= M_RAD_BF_CAP_EHT_MU_80MHZ;
    }
    /* MU Beamformer (160MHz) */
    if(phyCaps->mu_beamformer_160mhz) {
        pBand->bfCapsSupported[COM_DIR_TRANSMIT] |= M_RAD_BF_CAP_EHT_MU_160MHZ;
    }
    /* MU Beamformer (320MHz) */
    if(phyCaps->mu_beamformer_320mhz) {
        pBand->bfCapsSupported[COM_DIR_TRANSMIT] |= M_RAD_BF_CAP_EHT_MU_320MHZ;
    }
}

static void s_parseEhtCapMcsSet(const struct nlattr* tbIfType, wld_nl80211_bandDef_t* pBand) {
    ASSERTS_NOT_NULL(tbIfType, , ME, "NULL)");
    swl_mcs_t* pMcsStd = &pBand->mcsStds[SWL_MCS_STANDARD_EHT];
    uint8_t mcsSet[13] = {0};
    NLA_GET_DATA(mcsSet, tbIfType, sizeof(mcsSet));
    if(memcmp(mcsSet, &((uint8_t[13]) {0}), sizeof(mcsSet)) == 0) {
        return;
    }
    pBand->radStdsMask |= M_SWL_RADSTD_BE;
    pMcsStd->standard = SWL_MCS_STANDARD_EHT;
    pMcsStd->numberOfSpatialStream = 1;
    pMcsStd->mcsIndex = SWL_MAX(pMcsStd->mcsIndex, pBand->mcsStds[SWL_MCS_STANDARD_HE].mcsIndex);
    pMcsStd->bandwidth = SWL_MAX(pMcsStd->bandwidth, pBand->mcsStds[SWL_MCS_STANDARD_HE].bandwidth);
    pMcsStd->guardInterval = pBand->mcsStds[SWL_MCS_STANDARD_HE].guardInterval;
    /* use max supported Spatial Stream found (can be different by MCS!) */
    for(uint32_t i = 0; i < SWL_ARRAY_SIZE(mcsSet); i++) {
        pMcsStd->numberOfSpatialStream = SWL_MAX(pMcsStd->numberOfSpatialStream,
                                                 (uint32_t) (mcsSet[i] & 0xf));
    }
    pBand->nSSMax = SWL_MAX(pBand->nSSMax, pMcsStd->numberOfSpatialStream);
}

static swl_rc_ne s_parseEhtAttrs(struct nlattr* tbBand[], wld_nl80211_bandDef_t* pBand) {
    ASSERTS_NOT_NULL(tbBand, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pBand, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(tbBand[NL80211_BAND_ATTR_IFTYPE_DATA], SWL_RC_OK, ME, "no iftype data");
    struct nlattr* nlIfType = NULL;
    int rem = 0;
    struct nlattr* tbIfType[NL80211_BAND_IFTYPE_ATTR_EHT_CAP_MCS_SET + 1] = {};
    nla_for_each_nested(nlIfType, tbBand[NL80211_BAND_ATTR_IFTYPE_DATA], rem) {
        nla_parse(tbIfType, NL80211_BAND_IFTYPE_ATTR_EHT_CAP_MCS_SET, nla_data(nlIfType), nla_len(nlIfType), NULL);
        if(!s_isSupportedNlIfType(tbIfType[NL80211_BAND_IFTYPE_ATTR_IFTYPES])) {
            continue;
        }
        s_parseEhtCapPhy(tbIfType[NL80211_BAND_IFTYPE_ATTR_EHT_CAP_PHY], pBand);
        s_parseEhtCapMcsSet(tbIfType[NL80211_BAND_IFTYPE_ATTR_EHT_CAP_MCS_SET], pBand);
    }

    return SWL_RC_DONE;
}

SWL_TABLE(sChanDfsStatusMap,
          ARR(uint32_t nl80211ChanStatus; wld_nl80211_chanStatus_e chanStatus; ),
          ARR(swl_type_uint32, swl_type_uint32, ),
          ARR({NL80211_DFS_USABLE, WLD_NL80211_CHAN_USABLE},
              {NL80211_DFS_UNAVAILABLE, WLD_NL80211_CHAN_UNAVAILABLE},
              {NL80211_DFS_AVAILABLE, WLD_NL80211_CHAN_AVAILABLE},
              ));
const char* g_str_wld_nl80211_chan_status_bf_cap[WLD_NL80211_CHAN_STATUS_MAX] = {
    "Unknown", "Disabled", "Usable", "Unavailable", "Available",
};
swl_rc_ne s_parseChans(struct nlattr* tbBand[], wld_nl80211_bandDef_t* pBand) {
    ASSERTS_NOT_NULL(tbBand, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pBand, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(tbBand[NL80211_BAND_ATTR_FREQS], SWL_RC_OK, ME, "No freqs to parse");
    struct nlattr* nlFreq;
    int remFreq;
    struct nlattr* tbFreq[NL80211_FREQUENCY_ATTR_MAX + 1];
    static struct nla_policy freqPolicy[NL80211_FREQUENCY_ATTR_MAX + 1] = {
        [NL80211_FREQUENCY_ATTR_FREQ] = { .type = NLA_U32 },
        [NL80211_FREQUENCY_ATTR_DISABLED] = { .type = NLA_FLAG },
        [NL80211_FREQUENCY_ATTR_NO_IR] = { .type = NLA_FLAG },
        [__NL80211_FREQUENCY_ATTR_NO_IBSS] = { .type = NLA_FLAG },
        [NL80211_FREQUENCY_ATTR_RADAR] = { .type = NLA_FLAG },
        [NL80211_FREQUENCY_ATTR_MAX_TX_POWER] = { .type = NLA_U32 },
    };
    freqPolicy[NL80211_FREQUENCY_ATTR_DFS_STATE].type = NLA_U32;
    freqPolicy[NL80211_FREQUENCY_ATTR_DFS_TIME].type = NLA_U32;
    freqPolicy[NL80211_FREQUENCY_ATTR_DFS_CAC_TIME].type = NLA_U32;
    wld_nl80211_chanDesc_t* pChan = &pBand->chans[0];
    if(pBand->nChans > 0) {
        pChan = &pBand->chans[pBand->nChans - 1];
    }
    nla_for_each_nested(nlFreq, tbBand[NL80211_BAND_ATTR_FREQS], remFreq) {
        uint32_t freq;
        nla_parse(tbFreq, NL80211_FREQUENCY_ATTR_MAX, nla_data(nlFreq), nla_len(nlFreq), freqPolicy);
        // Ignore "channels" that are disabled or with no frequency.
        if(!tbFreq[NL80211_FREQUENCY_ATTR_FREQ] || tbFreq[NL80211_FREQUENCY_ATTR_DISABLED]) {
            continue;
        }
        freq = nla_get_u32(tbFreq[NL80211_FREQUENCY_ATTR_FREQ]);
        bool isDfs = tbFreq[NL80211_FREQUENCY_ATTR_RADAR];
        bool noIr = (tbFreq[NL80211_FREQUENCY_ATTR_NO_IR] || tbFreq[__NL80211_FREQUENCY_ATTR_NO_IBSS]);
        //Filter out chanels that are not DFS but still have the NO_IR flag
        if(!isDfs && noIr) {
            continue;
        }
        if(pChan->ctrlFreq != freq) {
            if(pBand->nChans >= WLD_MAX_POSSIBLE_CHANNELS) {
                continue;
            }
            pChan = &pBand->chans[pBand->nChans++];
            pChan->ctrlFreq = freq;
            pChan->isDfs = isDfs;
        }
        if(!(pChan->isDfs)) {
            pChan->status = WLD_NL80211_CHAN_AVAILABLE;
        } else {
            if(tbFreq[NL80211_FREQUENCY_ATTR_DFS_STATE]) {
                uint32_t state = nla_get_u32(tbFreq[NL80211_FREQUENCY_ATTR_DFS_STATE]);
                uint32_t* pStateVal = (uint32_t*) swl_table_getMatchingValue(&sChanDfsStatusMap, 1, 0, &state);
                if(pStateVal) {
                    pChan->status = *pStateVal;
                }
            }
            NLA_GET_VAL(pChan->dfsTime, nla_get_u32, tbFreq[NL80211_FREQUENCY_ATTR_DFS_TIME]);
            NLA_GET_VAL(pChan->dfsCacTime, nla_get_u32, tbFreq[NL80211_FREQUENCY_ATTR_DFS_CAC_TIME]);
        }
        if(tbFreq[NL80211_FREQUENCY_ATTR_MAX_TX_POWER]) {
            //convert from mbm to dbm
            pChan->maxTxPwr = nla_get_u32(tbFreq[NL80211_FREQUENCY_ATTR_MAX_TX_POWER]) / 100;
        }
    }
    return SWL_RC_OK;
}

swl_rc_ne s_parseDataTransmitRates(struct nlattr* tbBand[], wld_nl80211_bandDef_t* pBand) {
    ASSERTS_NOT_NULL(tbBand, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pBand, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(tbBand[NL80211_BAND_ATTR_RATES], SWL_RC_OK, ME, "No freqs to parse");

    struct nlattr* nlRate;
    int remRate;
    struct nlattr* tbRate[NL80211_BITRATE_ATTR_MAX + 1];
    static struct nla_policy ratePolicy[NL80211_BITRATE_ATTR_MAX + 1] = {
        [NL80211_BITRATE_ATTR_RATE] = { .type = NLA_U32 },
        [NL80211_BITRATE_ATTR_2GHZ_SHORTPREAMBLE] = { .type = NLA_FLAG },
    };

    u_int32_t idx = 0;
    char dataTransmitRates[64] = {'\0'};
    nla_for_each_nested(nlRate, tbBand[NL80211_BAND_ATTR_RATES], remRate) {
        nla_parse(tbRate, NL80211_BITRATE_ATTR_MAX, nla_data(nlRate), nla_len(nlRate), ratePolicy);
        if(!tbRate[NL80211_BITRATE_ATTR_RATE]) {
            continue;
        }

        float rate = 0.1 * nla_get_u32(tbRate[NL80211_BITRATE_ATTR_RATE]);
        const char* format = ((rate == (int) rate)) ? "%s%.0f" : "%s%.1f";
        swl_str_catFormat(dataTransmitRates, sizeof(dataTransmitRates), format, idx > 0 ? "," : "", rate);
        idx++;
    }
    pBand->supportedDataTransmitRates = swl_conv_charToMask(dataTransmitRates, swl_mcs_legacyStrList, SWL_MCS_LEGACY_LIST_SIZE);
    return SWL_RC_OK;
}

swl_rc_ne s_parseWiphyBands(struct nlattr* tb[], wld_nl80211_wiphyInfo_t* pWiphy) {
    ASSERTS_NOT_NULL(tb, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pWiphy, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(tb[NL80211_ATTR_WIPHY_BANDS], SWL_RC_OK, ME, "No attr to parse");
    struct nlattr* nlBand;
    int remBand;
    struct nlattr* tbBand[NL80211_BAND_ATTR_MAX + 1] = {};
    nla_for_each_nested(nlBand, tb[NL80211_ATTR_WIPHY_BANDS], remBand) {
        uint32_t nlFreqBand = nlBand->nla_type;
        swl_freqBand_e freqBand = wld_nl80211_freqBandNlToSwl(nlFreqBand);
        if(freqBand >= SWL_FREQ_BAND_MAX) {
            continue;
        }
        wld_nl80211_bandDef_t* pBand = &pWiphy->bands[freqBand];
        pBand->freqBand = freqBand;
        switch(freqBand) {
        case SWL_FREQ_BAND_2_4GHZ: {
            pBand->radStdsMask |= M_SWL_RADSTD_B | M_SWL_RADSTD_G;
            break;
        }
        case SWL_FREQ_BAND_5GHZ: {
            pBand->radStdsMask |= M_SWL_RADSTD_A;
            break;
        }
        case SWL_FREQ_BAND_6GHZ: {
            pBand->radStdsMask |= M_SWL_RADSTD_AX;
            break;
        }
        default:
            break;
        }
        pBand->nSSMax = SWL_MAX((int) pBand->nSSMax, 1);
        pBand->chanWidthMask |= M_SWL_BW_20MHZ;
        nla_parse(tbBand, NL80211_BAND_ATTR_MAX, nla_data(nlBand), nla_len(nlBand), NULL);
        s_parseHtAttrs(tbBand, pBand);
        s_parseVhtAttrs(tbBand, pBand);
        s_parseHeAttrs(tbBand, pBand);
        s_parseEhtAttrs(tbBand, pBand);
        s_parseChans(tbBand, pBand);
        s_parseDataTransmitRates(tbBand, pBand);
    }
    for(uint32_t i = 0; i < SWL_ARRAY_SIZE(pWiphy->bands); i++) {
        if(pWiphy->bands[i].nChans > 0) {
            pWiphy->freqBandsMask |= SWL_BIT_SHIFT(pWiphy->bands[i].freqBand);
            pWiphy->nSSMax = SWL_MAX(pWiphy->nSSMax, pWiphy->bands[i].nSSMax);
        }
    }
    return SWL_RC_OK;
}

static swl_rc_ne s_parseAntMask(struct nlattr* pAttr, int32_t* pNAnt) {
    ASSERTS_NOT_NULL(pNAnt, SWL_RC_INVALID_PARAM, ME, "NULL");
    if(!pAttr) {
        if(*pNAnt == 0) {
            *pNAnt = -1;
        }
        return SWL_RC_ERROR;
    }
    *pNAnt = swl_bit32_getNrSet(nla_get_u32(pAttr));
    return SWL_RC_OK;
}

static int s_extFeatureIsSet(const uint8_t* extFeat, uint32_t extFeatLen, uint32_t ftIdx) {
    ASSERTS_TRUE((ftIdx / 8) < extFeatLen, 0, ME, "out of range");
    return ((extFeat[(ftIdx / 8)] & (1 << (ftIdx % 8))) != 0);
}

static swl_rc_ne s_parseSuppFeatures(struct nlattr* tb[], wld_nl80211_wiphyInfo_t* pWiphy) {
    ASSERTS_NOT_NULL(tb, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pWiphy, SWL_RC_INVALID_PARAM, ME, "NULL");
    struct nlattr* attr;
    if((attr = tb[NL80211_ATTR_FEATURE_FLAGS]) != NULL) {
        uint32_t flags = nla_get_u32(attr);
        pWiphy->suppFeatures.sae = ((flags & NL80211_FEATURE_SAE) != 0);
        if(pWiphy->suppFeatures.sae) {
            pWiphy->suppFeatures.sae_pwe = NL80211_ATTR_IS_SPECIFIED(NL80211_ATTR_SAE_PWE);
        }
    }
    if((attr = tb[NL80211_ATTR_EXT_FEATURES]) != NULL) {
        uint8_t* extFeat = nla_data(attr);
        uint32_t extFeatLen = nla_len(attr);
        if(SWL_BIT_IS_SET(pWiphy->freqBandsMask, SWL_FREQ_BAND_5GHZ)) {
            pWiphy->suppFeatures.dfsOffload = s_extFeatureIsSet(extFeat, extFeatLen, NL80211_EXT_FEATURE_DFS_OFFLOAD);
        }
        pWiphy->suppFeatures.scanDwell = s_extFeatureIsSet(extFeat, extFeatLen, NL80211_EXT_FEATURE_SET_SCAN_DWELL);
        pWiphy->suppFeatures.backgroundRadar = s_extFeatureIsSet(extFeat, extFeatLen, NL80211_EXT_FEATURE_RADAR_BACKGROUND);

    }
    return SWL_RC_OK;
}

SWL_TABLE(sNlIfTypeMaps,
          ARR(uint32_t nlVal; wld_iftype_e wldVal; ),
          ARR(swl_type_uint32, swl_type_uint16, ),
          ARR({NL80211_IFTYPE_AP, WLD_WIPHY_IFTYPE_AP},
              {NL80211_IFTYPE_STATION, WLD_WIPHY_IFTYPE_STATION},
              ));

/* IEEE P802.11be/D5.01, March 2024: Figure 9-1072j - EML Capabilities subfield format */
typedef struct {
    uint16_t emlsr_support : 1;
    uint16_t b_1_6 : 6;
    uint16_t emlmr_support : 1;
    uint16_t b_8_15 : 8;
} SWL_PACKED ehtEmlCapInfo_t;

static swl_rc_ne s_parseIfTypeExtCapa(struct nlattr* tb[], wld_nl80211_wiphyInfo_t* pWiphy) {
    ASSERTS_NOT_NULL(tb, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pWiphy, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(tb[NL80211_ATTR_IFTYPE_EXT_CAPA], SWL_RC_OK, ME, "No extra capabilities to parse");
    struct nlattr* nlCapa;
    int rem;
    struct nlattr* tbCapa[NL80211_ATTR_MAX + 1];
    nla_for_each_nested(nlCapa, tb[NL80211_ATTR_IFTYPE_EXT_CAPA], rem) {
        nla_parse(tbCapa, NL80211_ATTR_MAX, nla_data(nlCapa), nla_len(nlCapa), NULL);
        /* NL80211_ATTR_IFTYPE,
         * NL80211_ATTR_EXT_CAPA,
         * NL80211_ATTR_EXT_CAPA_MASK,
         * NL80211_ATTR_EML_CAPABILITY,
         * NL80211_ATTR_MLD_CAPA_AND_OPS,
         */
        wld_iftype_e* ifType = NULL;
        if(tbCapa[NL80211_ATTR_IFTYPE] != NULL) {
            uint32_t nlIfType = nla_get_u32(tbCapa[NL80211_ATTR_IFTYPE]);
            ifType = (wld_iftype_e*) swl_table_getMatchingValue(&sNlIfTypeMaps, 1, 0, &nlIfType);
        }
        ASSERTS_NOT_NULL(ifType, SWL_RC_ERROR, ME, "IfType not supported");
        if(tbCapa[NL80211_ATTR_EML_CAPABILITY] != NULL) {
            ehtEmlCapInfo_t emlCapInfo = {0};
            NLA_GET_DATA(&emlCapInfo, tbCapa[NL80211_ATTR_EML_CAPABILITY], sizeof(emlCapInfo));
            pWiphy->extCapas.emlsrSupport[*ifType] = emlCapInfo.emlsr_support;
            pWiphy->extCapas.emlmrSupport[*ifType] = emlCapInfo.emlmr_support;
        }
    }
    return SWL_RC_OK;
}

static swl_rc_ne s_parseSuppCmds(struct nlattr* tb[], wld_nl80211_wiphyInfo_t* pWiphy) {
    ASSERTS_NOT_NULL(tb, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pWiphy, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(tb[NL80211_ATTR_SUPPORTED_COMMANDS], SWL_RC_OK, ME, "No cmds to parse");
    struct nlattr* nlCmd;
    int remCmd;
    nla_for_each_nested(nlCmd, tb[NL80211_ATTR_SUPPORTED_COMMANDS], remCmd) {

        uint32_t nl80211Cmd = nla_get_u32(nlCmd);
        if(nl80211Cmd == NL80211_CMD_UNSPEC) {
            continue;
        }

        if(nl80211Cmd == NL80211_CMD_CHANNEL_SWITCH) {
            pWiphy->suppCmds.channelSwitch = true;
        } else if(nl80211Cmd == NL80211_CMD_GET_SURVEY) {
            pWiphy->suppCmds.survey = true;
        } else if(nl80211Cmd == NL80211_CMD_SET_QOS_MAP) {
            pWiphy->suppCmds.WMMCapability = true;
        }
    }
    return SWL_RC_OK;
}

static swl_rc_ne s_parseMbssidAds(struct nlattr* tb[], wld_nl80211_wiphyInfo_t* pWiphy) {
    ASSERT_NOT_NULL(pWiphy, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(tb, SWL_RC_INVALID_PARAM, ME, "NULL");
    pWiphy->wiphy = wld_nl80211_getWiphy(tb);
    struct nlattr* pMbssidConfAttr = tb[NL80211_ATTR_MBSSID_CONFIG];
    ASSERTS_NOT_NULL(pMbssidConfAttr, SWL_RC_OK, ME, "No MBSSID info to parse");
    swl_rc_ne rc = SWL_RC_OK;

    struct nlattr* pMbssidInfo[NL80211_MBSSID_CONFIG_ATTR_MAX_EMA_PROFILE_PERIODICITY + 1] = {};

    static struct nla_policy mbssidConfPolicy[NL80211_MBSSID_CONFIG_ATTR_MAX_EMA_PROFILE_PERIODICITY + 1] = {
        [NL80211_MBSSID_CONFIG_ATTR_MAX_INTERFACES] = { .type = NLA_U8 },
        [NL80211_MBSSID_CONFIG_ATTR_MAX_EMA_PROFILE_PERIODICITY] = { .type = NLA_U8 },
    };

    rc = nla_parse_nested(pMbssidInfo, NL80211_MBSSID_CONFIG_ATTR_MAX_EMA_PROFILE_PERIODICITY, pMbssidConfAttr, mbssidConfPolicy);
    if(rc != SWL_RC_OK) {
        SAH_TRACEZ_ERROR(ME, "Failed to parse nested MBSSID Conf attributes!");
        return SWL_RC_ERROR;
    }

    if(pMbssidInfo[NL80211_MBSSID_CONFIG_ATTR_MAX_INTERFACES]) {
        uint8_t maxMbssidAdsIfaces = nla_get_u8(pMbssidInfo[NL80211_MBSSID_CONFIG_ATTR_MAX_INTERFACES]);
        uint8_t maxMbssidEmaPeriod = 0;
        if(pMbssidInfo[NL80211_MBSSID_CONFIG_ATTR_MAX_EMA_PROFILE_PERIODICITY]) {
            maxMbssidEmaPeriod = nla_get_u8(pMbssidInfo[NL80211_MBSSID_CONFIG_ATTR_MAX_EMA_PROFILE_PERIODICITY]);
        }
        /*
         * Driver should indicate MBSSID support by setting
         * wiphy->mbssid_max_interfaces to a value more than or equal to 2.
         */
        if(maxMbssidAdsIfaces >= 2) {
            pWiphy->maxMbssidAdsIfaces = maxMbssidAdsIfaces;
            /*
             * Driver should indicate EMA support to the userspace
             * by setting wiphy->ema_max_profile_periodicity to a non-zero value.
             */
            pWiphy->maxMbssidEmaPeriod = maxMbssidEmaPeriod;
        }
        SAH_TRACEZ_INFO(ME, "%s: maxMbssidAdsIfaces %u maxMbssidEmaPeriod %u",
                        pWiphy->name, maxMbssidAdsIfaces, maxMbssidEmaPeriod);
    }
    return SWL_RC_OK;
}

swl_rc_ne wld_nl80211_parseWiphyInfo(struct nlattr* tb[], wld_nl80211_wiphyInfo_t* pWiphy) {
    ASSERT_NOT_NULL(pWiphy, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(tb, SWL_RC_INVALID_PARAM, ME, "NULL");
    pWiphy->wiphy = wld_nl80211_getWiphy(tb);
    NLA_GET_DATA(pWiphy->name, tb[NL80211_ATTR_WIPHY_NAME], (sizeof(pWiphy->name) - 1));
    NLA_GET_VAL(pWiphy->maxScanSsids, nla_get_u8, tb[NL80211_ATTR_MAX_NUM_SCAN_SSIDS]);
    NLA_GET_VAL(pWiphy->maxScanIeLen, nla_get_u16, tb[NL80211_ATTR_MAX_SCAN_IE_LEN]);
    s_parseAntMask(tb[NL80211_ATTR_WIPHY_ANTENNA_AVAIL_TX], &pWiphy->nrAntenna[COM_DIR_TRANSMIT]);
    s_parseAntMask(tb[NL80211_ATTR_WIPHY_ANTENNA_AVAIL_RX], &pWiphy->nrAntenna[COM_DIR_RECEIVE]);
    s_parseAntMask(tb[NL80211_ATTR_WIPHY_ANTENNA_TX], &pWiphy->nrActiveAntenna[COM_DIR_TRANSMIT]);
    s_parseAntMask(tb[NL80211_ATTR_WIPHY_ANTENNA_RX], &pWiphy->nrActiveAntenna[COM_DIR_RECEIVE]);
    if(tb[NL80211_ATTR_SUPPORT_AP_UAPSD]) {
        pWiphy->suppUAPSD = true;
    }
    if(tb[NL80211_ATTR_CIPHER_SUITES]) {
        pWiphy->nCipherSuites = SWL_MIN(WLD_NL80211_CIPHERS_MAX, (int) (nla_len(tb[NL80211_ATTR_CIPHER_SUITES]) / sizeof(uint32_t)));
        memcpy(pWiphy->cipherSuites, nla_data(tb[NL80211_ATTR_CIPHER_SUITES]), pWiphy->nCipherSuites * sizeof(uint32_t));
    }
    NLA_GET_VAL(pWiphy->nStaMax, nla_get_u32, tb[NL80211_ATTR_MAX_AP_ASSOC_STA]);
    /* check MLO support */
    if(tb[NL80211_ATTR_MLO_SUPPORT]) {
        pWiphy->suppMlo = true;
    }

    s_parseIfTypes(tb, pWiphy);
    s_parseIfCombi(tb, pWiphy);
    s_parseWiphyBands(tb, pWiphy);
    s_parseSuppFeatures(tb, pWiphy);
    s_parseSuppCmds(tb, pWiphy);
    s_parseIfTypeExtCapa(tb, pWiphy);
    s_parseMbssidAds(tb, pWiphy);
    return SWL_RC_OK;
}

/*
 * @brief parse nl msg rate attributes into station info struct
 *
 * @param pBitrateAttributre pointer to rate info struct
 * @param pRate pointer to rateInfo structure to be filled
 *
 * @return SWL_RC_DONE parsing done successfully
 *         SWL_RC_OK parsing done but some fields are missing
 *         <= SWL_RC_ERROR parsing error
 */
static swl_rc_ne s_parseRateInfo(struct nlattr* pBitrateAttributre, wld_nl80211_rateInfo_t* pRate) {
    ASSERTS_NOT_NULL(pBitrateAttributre, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pRate, SWL_RC_INVALID_PARAM, ME, "NULL");

    swl_rc_ne rc = SWL_RC_OK;

    struct nlattr* pRinfo[NL80211_RATE_INFO_MAX + 1];

    static struct nla_policy ratePolicy[NL80211_RATE_INFO_MAX + 1] = {
        [NL80211_RATE_INFO_BITRATE] = { .type = NLA_U16 },
        [NL80211_RATE_INFO_MCS] = { .type = NLA_U8 },
        [NL80211_RATE_INFO_40_MHZ_WIDTH] = { .type = NLA_FLAG },
        [NL80211_RATE_INFO_SHORT_GI] = { .type = NLA_FLAG },
        [NL80211_RATE_INFO_BITRATE32] = { .type = NLA_U32 },

    };
    ratePolicy[NL80211_RATE_INFO_VHT_MCS].type = NLA_U8;
    ratePolicy[NL80211_RATE_INFO_VHT_NSS].type = NLA_U8;
    ratePolicy[NL80211_RATE_INFO_HE_MCS].type = NLA_U8;
    ratePolicy[NL80211_RATE_INFO_HE_NSS].type = NLA_U8;
    ratePolicy[NL80211_RATE_INFO_HE_GI].type = NLA_U8;
    ratePolicy[NL80211_RATE_INFO_HE_DCM].type = NLA_U8;
    ratePolicy[NL80211_RATE_INFO_80_MHZ_WIDTH].type = NLA_FLAG;
    ratePolicy[NL80211_RATE_INFO_80P80_MHZ_WIDTH].type = NLA_FLAG;
    ratePolicy[NL80211_RATE_INFO_160_MHZ_WIDTH].type = NLA_FLAG;
    ratePolicy[NL80211_RATE_INFO_10_MHZ_WIDTH].type = NLA_FLAG;
    ratePolicy[NL80211_RATE_INFO_5_MHZ_WIDTH].type = NLA_FLAG;
    ratePolicy[NL80211_RATE_INFO_EHT_MCS].type = NLA_U8;
    ratePolicy[NL80211_RATE_INFO_EHT_NSS].type = NLA_U8;
    ratePolicy[NL80211_RATE_INFO_EHT_GI].type = NLA_U8;
    ratePolicy[NL80211_RATE_INFO_EHT_RU_ALLOC].type = NLA_U8;


    rc = nla_parse_nested(pRinfo, NL80211_RATE_INFO_MAX, pBitrateAttributre, ratePolicy);
    if(rc != SWL_RC_OK) {
        SAH_TRACEZ_ERROR(ME, "Failed to parse nested RATE attributes!");
        return SWL_RC_ERROR;
    }

    // Get bitrate value in Kbps
    if(pRinfo[NL80211_RATE_INFO_BITRATE32]) {
        pRate->bitrate = 100 * nla_get_u32(pRinfo[NL80211_RATE_INFO_BITRATE32]);
    } else if(pRinfo[NL80211_RATE_INFO_BITRATE]) {
        pRate->bitrate = 100 * nla_get_u16(pRinfo[NL80211_RATE_INFO_BITRATE]);
    } else {
        SAH_TRACEZ_NOTICE(ME, "NL80211_RATE_INFO_BITRATE attribute is missing");
    }

    // Get protocol info
    swl_mcs_clean(&pRate->mcsInfo);
    pRate->mcsInfo.numberOfSpatialStream = 1;
    // legacy: to allow default stats for a/b/g std and passive rx devices
    pRate->mcsInfo.standard = SWL_MCS_STANDARD_LEGACY;

    if(pRinfo[NL80211_RATE_INFO_EHT_MCS]) {
        pRate->mcsInfo.mcsIndex = (uint32_t) nla_get_u8(pRinfo[NL80211_RATE_INFO_EHT_MCS]);
        pRate->mcsInfo.standard = SWL_MCS_STANDARD_EHT;
        if(pRinfo[NL80211_RATE_INFO_EHT_NSS]) {
            pRate->mcsInfo.numberOfSpatialStream = nla_get_u8(pRinfo[NL80211_RATE_INFO_EHT_NSS]);
        }
        pRate->mcsInfo.guardInterval = SWL_SGI_3200;
    } else if(pRinfo[NL80211_RATE_INFO_HE_MCS]) {
        pRate->mcsInfo.mcsIndex = (uint32_t) nla_get_u8(pRinfo[NL80211_RATE_INFO_HE_MCS]);
        pRate->mcsInfo.standard = SWL_MCS_STANDARD_HE;
        if(pRinfo[NL80211_RATE_INFO_HE_NSS]) {
            pRate->mcsInfo.numberOfSpatialStream = nla_get_u8(pRinfo[NL80211_RATE_INFO_HE_NSS]);
        }
        pRate->mcsInfo.guardInterval = SWL_SGI_3200;
    } else if(pRinfo[NL80211_RATE_INFO_VHT_MCS]) {
        pRate->mcsInfo.mcsIndex = (uint32_t) nla_get_u8(pRinfo[NL80211_RATE_INFO_VHT_MCS]);
        pRate->mcsInfo.standard = SWL_MCS_STANDARD_VHT;

        if(pRinfo[NL80211_RATE_INFO_VHT_NSS]) {
            pRate->mcsInfo.numberOfSpatialStream = nla_get_u8(pRinfo[NL80211_RATE_INFO_VHT_NSS]);
        }
        pRate->mcsInfo.guardInterval = SWL_SGI_800;
    } else if(pRinfo[NL80211_RATE_INFO_MCS]) {
        pRate->mcsInfo.mcsIndex = (uint32_t) nla_get_u8(pRinfo[NL80211_RATE_INFO_MCS]);
        pRate->mcsInfo.standard = SWL_MCS_STANDARD_HT;
        pRate->mcsInfo.numberOfSpatialStream = 1 + (pRate->mcsInfo.mcsIndex / 8);
        pRate->mcsInfo.guardInterval = SWL_SGI_800;
    }

    // Get guard interval
    if(pRinfo[NL80211_RATE_INFO_EHT_GI]) {
        uint8_t sgi = nla_get_u8(pRinfo[NL80211_RATE_INFO_EHT_GI]);
        if(sgi == NL80211_RATE_INFO_EHT_GI_0_8) {
            pRate->mcsInfo.guardInterval = SWL_SGI_800;
        } else if(sgi == NL80211_RATE_INFO_EHT_GI_1_6) {
            pRate->mcsInfo.guardInterval = SWL_SGI_1600;
        } else if(sgi == NL80211_RATE_INFO_EHT_GI_3_2) {
            pRate->mcsInfo.guardInterval = SWL_SGI_3200;
        }
    } else if(pRinfo[NL80211_RATE_INFO_HE_GI]) {
        uint8_t sgi = nla_get_u8(pRinfo[NL80211_RATE_INFO_HE_GI]);
        if(sgi == NL80211_RATE_INFO_HE_GI_0_8) {
            pRate->mcsInfo.guardInterval = SWL_SGI_800;
        } else if(sgi == NL80211_RATE_INFO_HE_GI_1_6) {
            pRate->mcsInfo.guardInterval = SWL_SGI_1600;
        } else if(sgi == NL80211_RATE_INFO_HE_GI_3_2) {
            pRate->mcsInfo.guardInterval = SWL_SGI_3200;
        }
    } else if(pRinfo[NL80211_RATE_INFO_SHORT_GI]) {
        pRate->mcsInfo.guardInterval = SWL_SGI_400;
    }

    // Get HE DCM (u8, 0/1)
    if(pRinfo[NL80211_RATE_INFO_HE_DCM]) {
        pRate->heDcm = nla_get_u8(pRinfo[NL80211_RATE_INFO_HE_DCM]);
    }

    /* HE/EHT OFDMA */
    if((pRinfo[NL80211_RATE_INFO_HE_RU_ALLOC] != NULL) ||
       (pRinfo[NL80211_RATE_INFO_EHT_RU_ALLOC] != NULL)) {
        pRate->ofdma = 1;
    }

    // Get Bandwidth value
    if((pRinfo[NL80211_RATE_INFO_80P80_MHZ_WIDTH]) || (pRinfo[NL80211_RATE_INFO_160_MHZ_WIDTH])) {
        pRate->mcsInfo.bandwidth = SWL_BW_160MHZ;
    } else if(pRinfo[NL80211_RATE_INFO_80_MHZ_WIDTH]) {
        pRate->mcsInfo.bandwidth = SWL_BW_80MHZ;
    } else if(pRinfo[NL80211_RATE_INFO_40_MHZ_WIDTH]) {
        pRate->mcsInfo.bandwidth = SWL_BW_40MHZ;
    } else if(pRinfo[NL80211_RATE_INFO_10_MHZ_WIDTH]) {
        pRate->mcsInfo.bandwidth = SWL_BW_10MHZ;
    } else if(pRinfo[NL80211_RATE_INFO_5_MHZ_WIDTH]) {
        pRate->mcsInfo.bandwidth = SWL_BW_5MHZ;
    } else {
        pRate->mcsInfo.bandwidth = SWL_BW_20MHZ;
    }

    swl_mcs_checkMcsIndexes(&pRate->mcsInfo);

    return rc;
}

static swl_rc_ne s_parseSignalPerChain(struct nlattr* pSigPChain, int8_t* sigArray, uint32_t maxNChain, uint32_t* pNChains) {
    ASSERTS_NOT_NULL(pSigPChain, SWL_RC_INVALID_PARAM, ME, "NULL");
    struct nlattr* attr;
    uint32_t i = 0;
    int rem = 0;
    nla_for_each_nested(attr, pSigPChain, rem) {
        if(i >= maxNChain) {
            break;
        }
        sigArray[i++] = (int8_t) nla_get_u8(attr);
    }
    if(pNChains) {
        *pNChains = i;
    }
    return SWL_RC_OK;
}

swl_rc_ne wld_nl80211_parseStationInfo(struct nlattr* tb[], wld_nl80211_stationInfo_t* pStation) {
    ASSERTS_NOT_NULL(tb, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pStation, SWL_RC_INVALID_PARAM, ME, "NULL");

    swl_rc_ne rc = SWL_RC_OK;

    struct nlattr* pSinfo[NL80211_STA_INFO_MAX + 1];

    static struct nla_policy statsPolicy[NL80211_STA_INFO_MAX + 1] = {
        [NL80211_STA_INFO_INACTIVE_TIME] = { .type = NLA_U32},
        [NL80211_STA_INFO_RX_BYTES] = { .type = NLA_U32 },
        [NL80211_STA_INFO_TX_BYTES] = { .type = NLA_U32 },
        [NL80211_STA_INFO_RX_PACKETS] = { .type = NLA_U32 },
        [NL80211_STA_INFO_TX_PACKETS] = { .type = NLA_U32 },
        [NL80211_STA_INFO_TX_RETRIES] = { .type = NLA_U32 },
        [NL80211_STA_INFO_TX_FAILED] = { .type = NLA_U32 },
        [NL80211_STA_INFO_RX_DROP_MISC] = { .type = NLA_U64 },
        [NL80211_STA_INFO_SIGNAL] = { .type = NLA_U8  },
        [NL80211_STA_INFO_SIGNAL_AVG] = { .type = NLA_U8 },
        [NL80211_STA_INFO_TX_BITRATE] = { .type = NLA_NESTED },
        [NL80211_STA_INFO_RX_BITRATE] = { .type = NLA_NESTED },
        [NL80211_STA_INFO_CONNECTED_TIME] = { .type = NLA_U32},
        [NL80211_STA_INFO_STA_FLAGS] = { .minlen = sizeof(struct nl80211_sta_flag_update) },

    };
    statsPolicy[NL80211_STA_INFO_RX_BYTES64].type = NLA_U64;
    statsPolicy[NL80211_STA_INFO_TX_BYTES64].type = NLA_U64;
    statsPolicy[NL80211_STA_INFO_CHAIN_SIGNAL].type = NLA_NESTED;
    statsPolicy[NL80211_STA_INFO_CHAIN_SIGNAL_AVG].type = NLA_NESTED;

    if(nla_parse_nested(pSinfo, NL80211_STA_INFO_MAX, tb[NL80211_ATTR_STA_INFO], statsPolicy) != SWL_RC_OK) {
        SAH_TRACEZ_ERROR(ME, "Failed to parse nested STA attributes!");
        return SWL_RC_ERROR;
    }
    NLA_GET_DATA(pStation->macAddr.bMac, tb[NL80211_ATTR_MAC], SWL_MAC_BIN_LEN);
    pStation->linkId = -1;
    if(tb[NL80211_ATTR_MLO_LINK_ID] != NULL) {
        pStation->linkId = nla_get_u8(tb[NL80211_ATTR_MLO_LINK_ID]);
    }
    if(tb[NL80211_ATTR_MLD_ADDR] != NULL) {
        NLA_GET_DATA(pStation->macMld.bMac, tb[NL80211_ATTR_MLD_ADDR], SWL_MAC_BIN_LEN);
    }
    if(tb[NL80211_ATTR_MLO_LINKS] != NULL) {
        int rem = 0;
        struct nlattr* list;
        struct nlattr* link[NL80211_ATTR_MAX + 1];
        uint32_t i = 0;
        nla_for_each_nested(list, tb[NL80211_ATTR_MLO_LINKS], rem) {
            if(i >= SWL_ARRAY_SIZE(pStation->linksInfo)) {
                SAH_TRACEZ_WARNING(ME, "skip nlinks exceeds max %zu", SWL_ARRAY_SIZE(pStation->linksInfo));
                break;
            }
            nla_parse_nested(link, NL80211_ATTR_MAX, list, NULL);
            if((s_parseMloLink(link, &pStation->linksInfo[i]) < SWL_RC_OK) ||
               (pStation->linksInfo[i].linkId < 0) ||
               (swl_mac_binIsNull(&pStation->linksInfo[i].linkMac))) {
                SAH_TRACEZ_WARNING(ME, "skip link %d: missing info", pStation->linksInfo[i].linkId);
                continue;
            }
            pStation->nrLinks = ++i;
        }
    }
    if(pSinfo[NL80211_STA_INFO_INACTIVE_TIME]) {
        pStation->inactiveTime = nla_get_u32(pSinfo[NL80211_STA_INFO_INACTIVE_TIME]);
    }
    if(pSinfo[NL80211_STA_INFO_RX_BYTES64]) {
        pStation->rxBytes = nla_get_u64(pSinfo[NL80211_STA_INFO_RX_BYTES64]);
    } else if(pSinfo[NL80211_STA_INFO_RX_BYTES]) {
        pStation->rxBytes = nla_get_u32(pSinfo[NL80211_STA_INFO_RX_BYTES]);
    }
    if(pSinfo[NL80211_STA_INFO_TX_BYTES64]) {
        pStation->txBytes = nla_get_u64(pSinfo[NL80211_STA_INFO_TX_BYTES64]);
    } else if(pSinfo[NL80211_STA_INFO_TX_BYTES]) {
        pStation->txBytes = nla_get_u32(pSinfo[NL80211_STA_INFO_TX_BYTES]);
    }
    if(pSinfo[NL80211_STA_INFO_RX_PACKETS]) {
        pStation->rxPackets = nla_get_u32(pSinfo[NL80211_STA_INFO_RX_PACKETS]);
    }
    if(pSinfo[NL80211_STA_INFO_TX_PACKETS]) {
        pStation->txPackets = nla_get_u32(pSinfo[NL80211_STA_INFO_TX_PACKETS]);
    }
    if(pSinfo[NL80211_STA_INFO_TX_RETRIES]) {
        pStation->txRetries = nla_get_u32(pSinfo[NL80211_STA_INFO_TX_RETRIES]);
    }
    if(pSinfo[NL80211_STA_INFO_TX_FAILED]) {
        pStation->txFailed = nla_get_u32(pSinfo[NL80211_STA_INFO_TX_FAILED]);
    }
    if(pSinfo[NL80211_STA_INFO_RX_DROP_MISC]) {
        pStation->rxFailed = nla_get_u64(pSinfo[NL80211_STA_INFO_RX_DROP_MISC]);
    }
    if(pSinfo[NL80211_STA_INFO_SIGNAL]) {
        pStation->rssiDbm = nla_get_u8(pSinfo[NL80211_STA_INFO_SIGNAL]);
    }
    if(pSinfo[NL80211_STA_INFO_SIGNAL_AVG]) {
        pStation->rssiAvgDbm = nla_get_u8(pSinfo[NL80211_STA_INFO_SIGNAL_AVG]);
    }
    if(pSinfo[NL80211_STA_INFO_TX_BITRATE]) {
        s_parseRateInfo(pSinfo[NL80211_STA_INFO_TX_BITRATE], &pStation->txRate);
    }
    if(pSinfo[NL80211_STA_INFO_RX_BITRATE]) {
        s_parseRateInfo(pSinfo[NL80211_STA_INFO_RX_BITRATE], &pStation->rxRate);
    }
    if(pSinfo[NL80211_STA_INFO_CONNECTED_TIME]) {
        pStation->connectedTime = nla_get_u32(pSinfo[NL80211_STA_INFO_CONNECTED_TIME]);
    }
    struct nl80211_sta_flag_update* pSFlags = NULL;
    pStation->flags.authorized = SWL_TRL_UNKNOWN;
    pStation->flags.authenticated = SWL_TRL_UNKNOWN;
    pStation->flags.associated = SWL_TRL_UNKNOWN;
    pStation->flags.wme = SWL_TRL_UNKNOWN;
    pStation->flags.mfp = SWL_TRL_UNKNOWN;
    if(pSinfo[NL80211_STA_INFO_STA_FLAGS]) {
        pSFlags = (struct nl80211_sta_flag_update*) nla_data(pSinfo[NL80211_STA_INFO_STA_FLAGS]);
        if(SWL_BIT_IS_SET(pSFlags->mask, NL80211_STA_FLAG_AUTHORIZED)) {
            pStation->flags.authorized = SWL_BIT_IS_SET(pSFlags->set, NL80211_STA_FLAG_AUTHORIZED);
        }
        if(SWL_BIT_IS_SET(pSFlags->mask, NL80211_STA_FLAG_AUTHENTICATED)) {
            pStation->flags.authenticated = SWL_BIT_IS_SET(pSFlags->set, NL80211_STA_FLAG_AUTHENTICATED);
        }
        if(SWL_BIT_IS_SET(pSFlags->mask, NL80211_STA_FLAG_ASSOCIATED)) {
            pStation->flags.associated = SWL_BIT_IS_SET(pSFlags->set, NL80211_STA_FLAG_ASSOCIATED);
        }
        if(SWL_BIT_IS_SET(pSFlags->mask, NL80211_STA_FLAG_WME)) {
            pStation->flags.wme = SWL_BIT_IS_SET(pSFlags->set, NL80211_STA_FLAG_WME);
        }
        if(SWL_BIT_IS_SET(pSFlags->mask, NL80211_STA_FLAG_MFP)) {
            pStation->flags.mfp = SWL_BIT_IS_SET(pSFlags->set, NL80211_STA_FLAG_MFP);
        }
    }
    s_parseSignalPerChain(pSinfo[NL80211_STA_INFO_CHAIN_SIGNAL], pStation->rssiDbmByChain, MAX_NR_ANTENNA, &pStation->nrSignalChains);
    s_parseSignalPerChain(pSinfo[NL80211_STA_INFO_CHAIN_SIGNAL_AVG], pStation->rssiAvgDbmByChain, MAX_NR_ANTENNA, NULL);

    return rc;
}

swl_rc_ne wld_nl80211_parseChanSurveyInfo(struct nlattr* tb[], wld_nl80211_channelSurveyInfo_t* pChanSurveyInfo) {
    ASSERTS_NOT_NULL(tb, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pChanSurveyInfo, SWL_RC_INVALID_PARAM, ME, "NULL");

    swl_rc_ne rc = SWL_RC_OK;

    struct nlattr* pSinfo[NL80211_SURVEY_INFO_MAX + 1];

    static struct nla_policy sp[NL80211_SURVEY_INFO_MAX + 1] = {
        [NL80211_SURVEY_INFO_FREQUENCY] = { .type = NLA_U32 },
        [NL80211_SURVEY_INFO_NOISE] = { .type = NLA_U8  },
        [NL80211_SURVEY_INFO_CHANNEL_TIME] = { .type = NLA_U64 },
        [NL80211_SURVEY_INFO_CHANNEL_TIME_BUSY] = { .type = NLA_U64 },
        [NL80211_SURVEY_INFO_CHANNEL_TIME_EXT_BUSY] = { .type = NLA_U64 },
        [NL80211_SURVEY_INFO_CHANNEL_TIME_RX] = { .type = NLA_U64 },
        [NL80211_SURVEY_INFO_CHANNEL_TIME_TX] = { .type = NLA_U64 },
    };
    sp[NL80211_SURVEY_INFO_TIME_SCAN].type = NLA_U64;
    sp[NL80211_SURVEY_INFO_TIME_BSS_RX].type = NLA_U64;

    if(nla_parse_nested(pSinfo, NL80211_SURVEY_INFO_MAX, tb[NL80211_ATTR_SURVEY_INFO], sp) != SWL_RC_OK) {
        SAH_TRACEZ_ERROR(ME, "Failed to parse nested survey info attributes!");
        return SWL_RC_ERROR;
    }

    ASSERT_NOT_NULL(pSinfo[NL80211_SURVEY_INFO_FREQUENCY], SWL_RC_ERROR, ME, "SURVEY_INFO_FREQUENCY attribute is missing");
    pChanSurveyInfo->inUse = !!pSinfo[NL80211_SURVEY_INFO_IN_USE];
    NLA_GET_VAL(pChanSurveyInfo->frequencyMHz, nla_get_u32, pSinfo[NL80211_SURVEY_INFO_FREQUENCY]);
    NLA_GET_VAL(pChanSurveyInfo->noiseDbm, (int8_t) nla_get_u8, pSinfo[NL80211_SURVEY_INFO_NOISE]);
    NLA_GET_VAL(pChanSurveyInfo->timeOn, nla_get_u64, pSinfo[NL80211_SURVEY_INFO_CHANNEL_TIME]);
    NLA_GET_VAL(pChanSurveyInfo->timeBusy, nla_get_u64, pSinfo[NL80211_SURVEY_INFO_CHANNEL_TIME_BUSY]);
    NLA_GET_VAL(pChanSurveyInfo->timeExtBusy, nla_get_u64, pSinfo[NL80211_SURVEY_INFO_CHANNEL_TIME_EXT_BUSY]);
    NLA_GET_VAL(pChanSurveyInfo->timeRx, nla_get_u64, pSinfo[NL80211_SURVEY_INFO_CHANNEL_TIME_RX]);
    NLA_GET_VAL(pChanSurveyInfo->timeTx, nla_get_u64, pSinfo[NL80211_SURVEY_INFO_CHANNEL_TIME_TX]);
    NLA_GET_VAL(pChanSurveyInfo->timeScan, nla_get_u64, pSinfo[NL80211_SURVEY_INFO_TIME_SCAN]);
    NLA_GET_VAL(pChanSurveyInfo->timeRxInBss, nla_get_u64, pSinfo[NL80211_SURVEY_INFO_TIME_BSS_RX]);
    if(pSinfo[NL80211_SURVEY_INFO_NOISE]) {
        pChanSurveyInfo->filled |= SURVEY_HAS_NF;
    }
    if(pSinfo[NL80211_SURVEY_INFO_CHANNEL_TIME]) {
        pChanSurveyInfo->filled |= SURVEY_HAS_CHAN_TIME;
    }
    if(pSinfo[NL80211_SURVEY_INFO_CHANNEL_TIME_BUSY]) {
        pChanSurveyInfo->filled |= SURVEY_HAS_CHAN_TIME_BUSY;
    }
    if(pSinfo[NL80211_SURVEY_INFO_CHANNEL_TIME_RX]) {
        pChanSurveyInfo->filled |= SURVEY_HAS_CHAN_TIME_RX;
    }
    if(pSinfo[NL80211_SURVEY_INFO_CHANNEL_TIME_TX]) {
        pChanSurveyInfo->filled |= SURVEY_HAS_CHAN_TIME_TX;
    }
    return rc;
}

static void s_copyScanInfoFromIEs(wld_scanResultSSID_t* pResult, swl_wirelessDevice_infoElements_t* pWirelessDevIE) {
    if(pWirelessDevIE->operChanInfo.channel > 0) {
        pResult->channel = pWirelessDevIE->operChanInfo.channel;
    }
    if(pWirelessDevIE->operChanInfo.bandwidth != SWL_BW_AUTO) {
        pResult->bandwidth = swl_chanspec_bwToInt(pWirelessDevIE->operChanInfo.bandwidth);
        pResult->extensionChannel = pWirelessDevIE->operChanInfo.extensionHigh;
    }
    swl_chanspec_t chanSpec = SWL_CHANSPEC_NEW_EXT(pResult->channel, pWirelessDevIE->operChanInfo.bandwidth, pWirelessDevIE->operChanInfo.band, pResult->extensionChannel, 0);
    pResult->centreChannel = swl_chanspec_getCentreChannel(&chanSpec);
    swl_operatingClass_t operClass = swl_chanspec_getOperClass(&chanSpec);
    if(operClass > 0) {
        pResult->operClass = operClass;
    }
    pResult->ssidLen = SWL_MIN((uint8_t) sizeof(pResult->ssid), pWirelessDevIE->ssidLen);
    memcpy(pResult->ssid, pWirelessDevIE->ssid, pResult->ssidLen);
    pResult->operatingStandards = pWirelessDevIE->operatingStandards;
    pResult->supportedStandards = pWirelessDevIE->supportedStandards;
    pResult->secModeEnabled = pWirelessDevIE->secModeEnabled;
    pResult->WPS_ConfigMethodsEnabled = pWirelessDevIE->WPS_ConfigMethodsEnabled;
    pResult->dtimPeriod = pWirelessDevIE->dtimPeriod;
    pResult->basicDataTransferRates = pWirelessDevIE->basicDataTransferRates;
    pResult->supportedDataTransferRates = pWirelessDevIE->supportedDataTransferRates;

    if((pWirelessDevIE->channelUtilization != 0) || (pWirelessDevIE->stationCount != 0)) {
        //IE has BSSLOAD element
        pResult->channelUtilization = pWirelessDevIE->channelUtilization;
        pResult->stationCount = pWirelessDevIE->stationCount;
    }
}

swl_rc_ne wld_nl80211_parseScanResultPerFreqBand(struct nlattr* tb[], wld_scanResultSSID_t* pResult, swl_freqBandExt_e band) {
    swl_rc_ne rc = SWL_RC_INVALID_PARAM;
    ASSERT_NOT_NULL(tb, rc, ME, "NULL");
    ASSERT_NOT_NULL(pResult, rc, ME, "NULL");
    uint32_t wiphy = wld_nl80211_getWiphy(tb);
    ASSERT_NOT_EQUALS(wiphy, WLD_NL80211_ID_UNDEF, rc, ME, "missing wiphy");
    ASSERT_NOT_NULL(tb[NL80211_ATTR_BSS], rc, ME, "no bss info in scan result");
    struct nlattr* bss[NL80211_BSS_MAX + 1];
    static struct nla_policy bss_policy[NL80211_BSS_MAX + 1] = {
        [NL80211_BSS_TSF] = { .type = NLA_U64 },
        [NL80211_BSS_FREQUENCY] = { .type = NLA_U32 },
        [NL80211_BSS_BSSID] = {.type = NLA_UNSPEC  },
        [NL80211_BSS_BEACON_INTERVAL] = { .type = NLA_U16 },
        [NL80211_BSS_CAPABILITY] = { .type = NLA_U16 },
        [NL80211_BSS_INFORMATION_ELEMENTS] = { .type = NLA_UNSPEC },
        [NL80211_BSS_SIGNAL_MBM] = { .type = NLA_U32 },
        [NL80211_BSS_SIGNAL_UNSPEC] = { .type = NLA_U8 },
        [NL80211_BSS_STATUS] = { .type = NLA_U32 },
        [NL80211_BSS_SEEN_MS_AGO] = { .type = NLA_U32 },
        [NL80211_BSS_BEACON_IES] = { .type = NLA_UNSPEC },
    };

    //skip invalid/partially parsed entries
    rc = SWL_RC_CONTINUE;
    if(nla_parse_nested(bss, NL80211_BSS_MAX, tb[NL80211_ATTR_BSS], bss_policy)) {
        SAH_TRACEZ_ERROR(ME, "failed to parse nested bss attributes");
        return rc;
    }
    ASSERT_NOT_NULL(bss[NL80211_BSS_BSSID], rc, ME, "missing bssid in scan result");
    memcpy(pResult->bssid.bMac, nla_data(bss[NL80211_BSS_BSSID]), SWL_MAC_BIN_LEN);

    // get signal strength, signal strength units not specified, scaled to 0-100
    if(bss[NL80211_BSS_SIGNAL_UNSPEC]) {
        //signal strength of the probe response/beacon in unspecified units, scaled to 0..100 <u8>
        pResult->rssi = convQuality2Signal((int32_t) nla_get_u8(bss[NL80211_BSS_SIGNAL_UNSPEC]));
    } else if(bss[NL80211_BSS_SIGNAL_MBM]) {
        //signal strength of probe response/beacon in mBm (100 * dBm) <s32>
        int32_t rssiDbm = (int32_t) nla_get_u32(bss[NL80211_BSS_SIGNAL_MBM]);
        if(rssiDbm >= 0) {
            // If driver reports a scaled RSSI instead of a dBm RSSI
            pResult->rssi = convQuality2Signal(rssiDbm);
        } else {
            pResult->rssi = rssiDbm / 100;
        }
    } else {
        SAH_TRACEZ_ERROR(ME, "missing signal strength of "SWL_MAC_FMT, SWL_MAC_ARG(pResult->bssid.bMac));
    }

    ASSERT_NOT_NULL(bss[NL80211_BSS_FREQUENCY], rc, ME, "missing frequency in scan result");

    uint32_t freq = 0;
    uint16_t caps = 0;
    NLA_GET_VAL(freq, nla_get_u32, bss[NL80211_BSS_FREQUENCY]);
    swl_chanspec_t chanSpec = SWL_CHANSPEC_EMPTY;
    if(swl_chanspec_channelFromMHz(&chanSpec, freq) == SWL_RC_OK) {
        if(band < SWL_FREQ_BAND_EXT_AUTO) {
            ASSERTS_EQUALS(chanSpec.band, band, rc, ME, "skip mismatched freq band");
        }
        chanSpec.bandwidth = SWL_BW_20MHZ;
        pResult->channel = chanSpec.channel;
        pResult->centreChannel = chanSpec.channel;
        pResult->bandwidth = swl_chanspec_bwToInt(chanSpec.bandwidth);
        pResult->operClass = swl_chanspec_getOperClass(&chanSpec);
    }

    //get information elements from probe_resp or from beacon
    pResult->ssidLen = SWL_80211_SSID_STR_LEN; // border-sized to detect ssid missing (even when empty (hidden))
    if(bss[NL80211_BSS_BEACON_INTERVAL]) {
        pResult->beaconInterval = nla_get_u16(bss[NL80211_BSS_BEACON_INTERVAL]);
        SAH_TRACEZ_INFO(ME, "Beacon interval %u", pResult->beaconInterval);
    }

    if(bss[NL80211_BSS_CAPABILITY]) {
        caps = nla_get_u16(bss[NL80211_BSS_CAPABILITY]);
        if(caps & WLAN_CAPABILITY_ESS) {
            SAH_TRACEZ_ERROR(ME, "ESS capability set");
            pResult->adhoc = false;
        }
        if(caps & WLAN_CAPABILITY_IBSS) {
            SAH_TRACEZ_ERROR(ME, "IBSS capability set");
            pResult->adhoc = true;
        }
    }

    if(bss[NL80211_BSS_BEACON_IES] || bss[NL80211_BSS_INFORMATION_ELEMENTS]) {
        uint32_t iesAttr = (bss[NL80211_BSS_INFORMATION_ELEMENTS])
            ? NL80211_BSS_INFORMATION_ELEMENTS
            : NL80211_BSS_BEACON_IES;
        size_t iesLen = nla_len(bss[iesAttr]);
        uint8_t* iesData = (uint8_t*) nla_data(bss[iesAttr]);
        swl_wirelessDevice_infoElements_t wirelessDevIE;
        swl_parsingArgs_t parsingArgs = {
            .seenOnChanspec = SWL_CHANSPEC_NEW(chanSpec.channel, chanSpec.bandwidth, chanSpec.band),
        };
        ssize_t parsedLen = swl_80211_parseInfoElementsBuffer(&wirelessDevIE, &parsingArgs, iesLen, iesData);
        if(parsedLen < (ssize_t) iesLen) {
            SAH_TRACEZ_WARNING(ME, "Error while parsing probe/beacon rcvd IEs");
            return SWL_RC_CONTINUE;
        }
        s_copyScanInfoFromIEs(pResult, &wirelessDevIE);
    }

    if(!pResult->channel || (pResult->ssidLen >= SWL_80211_SSID_STR_LEN)) {
        pResult->ssidLen = 0;
        SAH_TRACEZ_NOTICE(ME, "missing channel and ssid info");
        return SWL_RC_CONTINUE;
    }

    return SWL_RC_OK;
}

swl_rc_ne wld_nl80211_parseScanResult(struct nlattr* tb[], wld_scanResultSSID_t* pResult) {
    return wld_nl80211_parseScanResultPerFreqBand(tb, pResult, SWL_FREQ_BAND_EXT_AUTO);
}

swl_rc_ne wld_nl80211_parseRadarInfo(struct nlattr* tb[], wld_nl80211_radarEvtInfo_t* pRadarEvtInfo) {
    ASSERT_NOT_NULL(pRadarEvtInfo, SWL_RC_INVALID_PARAM, ME, "NULL");
    memset(pRadarEvtInfo, 0, sizeof(*pRadarEvtInfo));
    ASSERT_NOT_NULL(tb, SWL_RC_INVALID_PARAM, ME, "NULL");
    pRadarEvtInfo->wiphy = wld_nl80211_getWiphy(tb);
    pRadarEvtInfo->ifIndex = wld_nl80211_getIfIndex(tb);
    pRadarEvtInfo->event = WLD_NL80211_RADAR_EVT_UNKNOWN;
    NLA_GET_VAL(pRadarEvtInfo->event, nla_get_u32, tb[NL80211_ATTR_RADAR_EVENT]);
    wld_nl80211_parseChanSpec(tb, &pRadarEvtInfo->chanSpec);
    if(tb[NL80211_ATTR_RADAR_BACKGROUND]) {
        pRadarEvtInfo->isBackground = true;
    }
    return SWL_RC_OK;
}

