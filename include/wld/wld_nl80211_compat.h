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
 * nl80211 compatibility header file
 * It ensures building properly regardless the kernel version:
 * Ref: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/include/uapi/linux/nl80211.h
 */

#ifndef INCLUDE_WLD_WLD_NL80211_COMPAT_H_
#define INCLUDE_WLD_WLD_NL80211_COMPAT_H_

#include <linux/socket.h>
#include <linux/netlink.h>
#include "wld_nl80211.h"
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>

/*
 * This header file replaces with "define" all required nl80211 "enum", used in the code,
 * that are potentially undefined in some old kernel version.
 * Those already defined, are keeping same official values.
 * In fact:
 * nl80211 cmd and attribute IDs are defined in enum , not macros
 * so DEFINE same enum values, if they are lower than enum MAX,
 * else consider them by default UNSPECIFIED
 */
#define VAL(v, m, d) (((v) > (m)) ? (d) : (v))
#define NL80211_CMD(c) VAL(c, NL80211_CMD_MAX, NL80211_CMD_UNSPEC)
#define NL80211_ATTR(a) VAL(a, NL80211_ATTR_MAX, NL80211_ATTR_UNSPEC)
#define NL80211_BAND_ATTR(a) VAL(a, NL80211_BAND_ATTR_MAX, __NL80211_BAND_ATTR_INVALID)
#define NL80211_BAND_IFTYPE_ATTR(a) VAL(a, NL80211_BAND_IFTYPE_ATTR_MAX, __NL80211_BAND_IFTYPE_ATTR_INVALID)
#define NL80211_FREQUENCY_ATTR(a) VAL(a, NL80211_FREQUENCY_ATTR_MAX, __NL80211_FREQUENCY_ATTR_INVALID)
#define NL80211_RATE_ATTR(a) VAL(a, NL80211_RATE_INFO_MAX, __NL80211_RATE_INFO_INVALID)
#define NL80211_STA_INFO(a) VAL(a, NL80211_STA_INFO_MAX, __NL80211_STA_INFO_INVALID)
#define NL80211_STA_FLAG(a) VAL(a, NL80211_STA_FLAG_MAX, __NL80211_STA_FLAG_INVALID)
#define NL80211_SURVEY_INFO(a) VAL(a, NL80211_SURVEY_INFO_MAX, __NL80211_SURVEY_INFO_INVALID)

#define IS_SPECIFIED(v, m, u) (((v) <= (m)) && ((v) > (u)))
#define NL80211_CMD_IS_SPECIFIED(c) IS_SPECIFIED(c, NL80211_CMD_MAX, NL80211_CMD_UNSPEC)
#define NL80211_ATTR_IS_SPECIFIED(a) IS_SPECIFIED(a, NL80211_ATTR_MAX, NL80211_ATTR_UNSPEC)
#define NL80211_BAND_ATTR_IS_SPECIFIED(a) IS_SPECIFIED(a, NL80211_BAND_ATTR_MAX, __NL80211_BAND_ATTR_INVALID)
#define NL80211_BAND_IFTYPE_ATTR_IS_SPECIFIED(a) IS_SPECIFIED(a, NL80211_BAND_IFTYPE_ATTR_MAX, __NL80211_BAND_IFTYPE_ATTR_INVALID)
#define NL80211_RATE_INFO_IS_SPECIFIED(a) IS_SPECIFIED(a, NL80211_RATE_INFO_MAX, __NL80211_RATE_INFO_INVALID)

/* define needed macros when compilation headers are too old */
#ifndef NL_AUTO_PORT
#define NL_AUTO_PORT 0
#endif //NL_AUTO_PORT

//defined since kernel >= 2.6.14
#ifndef SOL_NETLINK
#define SOL_NETLINK 270
#endif //SOL_NETLINK

//defined since kernel >= 3.6
#define NL80211_BAND_ATTR_VHT_MCS_SET NL80211_BAND_ATTR(7)
#define NL80211_BAND_ATTR_VHT_CAPA NL80211_BAND_ATTR(8)
#define NL80211_RATE_INFO_BITRATE32 NL80211_RATE_ATTR(5)

//defined since kernel >= 3.7
#define NL80211_CMD_CH_SWITCH_NOTIFY NL80211_CMD(88)
#define NL80211_CMD_CONN_FAILED NL80211_CMD(91)
#define NL80211_ATTR_WDEV NL80211_ATTR(153)

//defined since kernel >= 3.8
#define NL80211_ATTR_SCAN_FLAGS NL80211_ATTR(158)
#define NL80211_ATTR_CHANNEL_WIDTH NL80211_ATTR(159)
#define NL80211_ATTR_CENTER_FREQ1 NL80211_ATTR(160)
#define NL80211_ATTR_CENTER_FREQ2 NL80211_ATTR(161)
#define NL80211_CHAN_WIDTH_20_NOHT 0
#define NL80211_CHAN_WIDTH_20 1
#define NL80211_CHAN_WIDTH_40 2
#define NL80211_CHAN_WIDTH_80 3
#define NL80211_CHAN_WIDTH_80P80 4
#define NL80211_CHAN_WIDTH_160 5
#define NL80211_RATE_INFO_VHT_MCS NL80211_RATE_ATTR(6)
#define NL80211_RATE_INFO_VHT_NSS NL80211_RATE_ATTR(7)
#define NL80211_RATE_INFO_80_MHZ_WIDTH NL80211_RATE_ATTR(8)
#define NL80211_RATE_INFO_80P80_MHZ_WIDTH NL80211_RATE_ATTR(9)
#define NL80211_RATE_INFO_160_MHZ_WIDTH NL80211_RATE_ATTR(10)
#define NL80211_FEATURE_SAE (1 << 5)
#define NL80211_SCAN_FLAG_FLUSH (1 << 1)
#define NL80211_SCAN_FLAG_AP    (1 << 2)

//defined since kernel >= 3.9
#define NL80211_CMD_RADAR_DETECT NL80211_CMD(94)
#define NL80211_ATTR_RADAR_EVENT NL80211_ATTR(168)
#define NL80211_DFS_USABLE 0
#define NL80211_DFS_UNAVAILABLE 1
#define NL80211_DFS_AVAILABLE 2
#define NL80211_FREQUENCY_ATTR_DFS_STATE NL80211_FREQUENCY_ATTR(7)
#define NL80211_FREQUENCY_ATTR_DFS_TIME NL80211_FREQUENCY_ATTR(8)
#define NL80211_STA_FLAG_ASSOCIATED NL80211_STA_FLAG(7)
#define NL80211_STA_INFO_RX_BYTES64 NL80211_STA_INFO(23)
#define NL80211_STA_INFO_TX_BYTES64 NL80211_STA_INFO(24)

//defined since kernel >= 3.10
#define NL80211_CMD_FT_EVENT NL80211_CMD(97)
#define NL80211_CMD_CRIT_PROTOCOL_STOP NL80211_CMD(99)
#define NL80211_ATTR_SPLIT_WIPHY_DUMP NL80211_ATTR(174)

//defined since kernel >= 3.11
#ifndef NL80211_GENL_NAME
#define NL80211_GENL_NAME "nl80211"
#endif //NL80211_GENL_NAME
#define NL80211_CHAN_WIDTH_5 6
#define NL80211_CHAN_WIDTH_10 7
#define NL80211_STA_INFO_CHAIN_SIGNAL NL80211_STA_INFO(25)
#define NL80211_STA_INFO_CHAIN_SIGNAL_AVG NL80211_STA_INFO(26)

//defined since kernel >= 3.12
#define NL80211_CMD_CHANNEL_SWITCH NL80211_CMD(102)

//defined since kernel >= 3.14
#ifndef NL80211_FREQUENCY_ATTR_NO_IR
#define NL80211_FREQUENCY_ATTR_NO_IR NL80211_FREQUENCY_ATTR(3)
#endif //NL80211_FREQUENCY_ATTR_NO_IR
#define __NL80211_FREQUENCY_ATTR_NO_IBSS NL80211_FREQUENCY_ATTR(4)
#define NL80211_CMD_SET_QOS_MAP NL80211_CMD(104)
#define NL80211_CMD_VENDOR NL80211_CMD(103)
#define NL80211_ATTR_VENDOR_ID NL80211_ATTR(195)
#define NL80211_ATTR_VENDOR_SUBCMD NL80211_ATTR(196)
#define NL80211_ATTR_VENDOR_DATA NL80211_ATTR(197)

//defined since kernel >= 3.15
#define NL80211_ATTR_MAX_AP_ASSOC_STA NL80211_ATTR(202)
#define NL80211_FREQUENCY_ATTR_DFS_CAC_TIME NL80211_FREQUENCY_ATTR(13)

//defined since kernel >= 3.19
#define NL80211_CMD_CH_SWITCH_STARTED_NOTIFY NL80211_CMD(110)
#define NL80211_ATTR_SOCKET_OWNER NL80211_ATTR(204)

//defined since kernel >= 4.0
#define NL80211_ATTR_EXT_FEATURES NL80211_ATTR(217)
#ifndef NL80211_MULTICAST_GROUP_CONFIG
#define NL80211_MULTICAST_GROUP_CONFIG "config"
#endif //NL80211_MULTICAST_GROUP_CONFIG
#ifndef NL80211_MULTICAST_GROUP_SCAN
#define NL80211_MULTICAST_GROUP_SCAN "scan"
#endif //NL80211_MULTICAST_GROUP_SCAN
#ifndef NL80211_MULTICAST_GROUP_MLME
#define NL80211_MULTICAST_GROUP_MLME "mlme"
#endif //NL80211_MULTICAST_GROUP_MLME
#ifndef NL80211_MULTICAST_GROUP_VENDOR
#define NL80211_MULTICAST_GROUP_VENDOR "vendor"
#endif //NL80211_MULTICAST_GROUP_VENDOR

#define NL80211_RATE_INFO_10_MHZ_WIDTH NL80211_RATE_ATTR(11)
#define NL80211_RATE_INFO_5_MHZ_WIDTH NL80211_RATE_ATTR(12)
#define NL80211_SURVEY_INFO_TIME_SCAN NL80211_SURVEY_INFO(9)
#define NL80211_STA_INFO_RX_DROP_MISC NL80211_STA_INFO(28)

//defined since kernel >= 4.3
#ifndef NETLINK_CAP_ACK
#define NETLINK_CAP_ACK 10
#endif //NETLINK_CAP_ACK

//defined since kernel >= 4.5
#define NL80211_CMD_ABORT_SCAN NL80211_CMD(114)

/* defined since kernel >= 4.5 */
#define NL80211_ATTR_IFTYPE_EXT_CAPA  NL80211_ATTR(230)

//defined since kernel >= 4.8
#define NL80211_ATTR_MEASUREMENT_DURATION NL80211_ATTR(235)
#define NL80211_ATTR_MEASUREMENT_DURATION_MANDATORY NL80211_ATTR(236)
#define NL80211_EXT_FEATURE_SET_SCAN_DWELL 5
#define NL80211_EXT_FEATURE_RADAR_BACKGROUND 61

//defined since kernel >= 4.10
#define NL80211_ATTR_BSSID NL80211_ATTR(245)

//defined since kernel >= 4.12
#ifndef NETLINK_EXT_ACK
#define NETLINK_EXT_ACK 11
#endif //NETLINK_EXT_ACK

//defined since kernel >= 4.17
#define NL80211_EXT_FEATURE_DFS_OFFLOAD 25
#define __NL80211_BAND_IFTYPE_ATTR_INVALID 0
#define NL80211_BAND_IFTYPE_ATTR_IFTYPES 1

//defined since kernel >= 4.18
#define NL80211_ATTR_TXQ_STATS NL80211_ATTR(265)

//defined since kernel >= 4.19
#define NL80211_BAND_ATTR_IFTYPE_DATA 9
#define NL80211_BAND_IFTYPE_ATTR_HE_CAP_PHY 3
#define NL80211_BAND_IFTYPE_ATTR_HE_CAP_MCS_SET 4
#define NL80211_RATE_INFO_HE_MCS NL80211_RATE_ATTR(13)
#define NL80211_RATE_INFO_HE_NSS NL80211_RATE_ATTR(14)
#define NL80211_RATE_INFO_HE_GI NL80211_RATE_ATTR(15)
#define NL80211_RATE_INFO_HE_DCM NL80211_RATE_ATTR(16)
#define NL80211_RATE_INFO_HE_RU_ALLOC NL80211_RATE_ATTR(17)
#define NL80211_RATE_INFO_HE_GI_0_8 0
#define NL80211_RATE_INFO_HE_GI_1_6 1
#define NL80211_RATE_INFO_HE_GI_3_2 2

//defined since kernel >= 5.4
#define NL80211_BAND_6GHZ 3
#define NL80211_SURVEY_INFO_TIME_BSS_RX NL80211_SURVEY_INFO(11)

/* defined since kernel >= 5.7 */
#define NL80211_BAND_IFTYPE_ATTR_HE_6GHZ_CAPA 6

//defined since kernel >= 5.9
#define NL80211_CHAN_WIDTH_1 8
#define NL80211_CHAN_WIDTH_2 9

//defined since kernel >= 5.11
#define NL80211_ATTR_SAE_PWE NL80211_ATTR(298)

//defined since kernel >= 5.16
#define NL80211_ATTR_MBSSID_CONFIG NL80211_ATTR(306)
#define NL80211_MBSSID_CONFIG_ATTR_MAX_INTERFACES 1
#define NL80211_MBSSID_CONFIG_ATTR_MAX_EMA_PROFILE_PERIODICITY 2

//defined since kernel >= 5.17
#define NL80211_ATTR_RADAR_BACKGROUND NL80211_ATTR(308)

/* defined since kernel >= 5.18 */
#define NL80211_BAND_IFTYPE_ATTR_EHT_CAP_MAC 8
#define NL80211_BAND_IFTYPE_ATTR_EHT_CAP_PHY 9
#define NL80211_BAND_IFTYPE_ATTR_EHT_CAP_MCS_SET 10
#define NL80211_BAND_IFTYPE_ATTR_EHT_CAP_PPE 11
#define NL80211_CHAN_WIDTH_320 13
#define NL80211_RATE_INFO_320_MHZ_WIDTH NL80211_RATE_ATTR(18)
#define NL80211_RATE_INFO_EHT_MCS NL80211_RATE_ATTR(19)
#define NL80211_RATE_INFO_EHT_NSS NL80211_RATE_ATTR(20)
#define NL80211_RATE_INFO_EHT_GI NL80211_RATE_ATTR(21)
#define NL80211_RATE_INFO_EHT_RU_ALLOC NL80211_RATE_ATTR(22)
#define NL80211_RATE_INFO_EHT_GI_0_8 0
#define NL80211_RATE_INFO_EHT_GI_1_6 1
#define NL80211_RATE_INFO_EHT_GI_3_2 2

//defined since kernel >= 6.0
#define NL80211_ATTR_MLO_LINKS NL80211_ATTR(312)
#define NL80211_ATTR_MLO_LINK_ID NL80211_ATTR(313)
#define NL80211_ATTR_MLD_ADDR NL80211_ATTR(314)
#define NL80211_ATTR_MLO_SUPPORT NL80211_ATTR(315)
#define NL80211_ATTR_EML_CAPABILITY NL80211_ATTR(317)
#define NL80211_ATTR_MLD_CAPA_AND_OPS NL80211_ATTR(318)

//to be up-streamed, not yet in kernel 6.17
#define NL80211_CMD_STOP_BGRADAR_DETECT \
    (!NL80211_ATTR_IS_SPECIFIED(NL80211_ATTR_RADAR_BACKGROUND) ? NL80211_CMD_UNSPEC : \
     ((NL80211_CMD_MAX >= 161) ? NL80211_CMD(158) : ((NL80211_CMD_MAX >= 157) ? NL80211_CMD(157) : NL80211_CMD(155))))

#endif /* INCLUDE_WLD_WLD_NL80211_COMPAT_H_ */
