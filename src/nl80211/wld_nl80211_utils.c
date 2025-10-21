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
 * This file includes nl80211 utility apis for:
 * - type conversion
 * - common attributes fetching
 * - ...
 */

#include "wld_nl80211_parser.h"
#include "wld_nl80211_utils.h"
#include "swl/swl_common_table.h"

#define ME "nlUtils"

SWL_TABLE(sChannelWidthMap,
          ARR(uint32_t nl80211Bw; uint32_t bw; swl_bandwidth_e swlBw; ),
          ARR(swl_type_uint32, swl_type_uint32, swl_type_uint32, ),
          ARR({NL80211_CHAN_WIDTH_20, 20, SWL_BW_20MHZ},
              {NL80211_CHAN_WIDTH_40, 40, SWL_BW_40MHZ},
              {NL80211_CHAN_WIDTH_80, 80, SWL_BW_80MHZ},
              {NL80211_CHAN_WIDTH_160, 160, SWL_BW_160MHZ},
              {NL80211_CHAN_WIDTH_320, 320, SWL_BW_320MHZ},
              {NL80211_CHAN_WIDTH_10, 10, SWL_BW_10MHZ},
              {NL80211_CHAN_WIDTH_5, 5, SWL_BW_5MHZ},
              {NL80211_CHAN_WIDTH_2, 2, SWL_BW_2MHZ},
              //specific cases
              {NL80211_CHAN_WIDTH_20_NOHT, 20, SWL_BW_20MHZ},
              {NL80211_CHAN_WIDTH_80P80, 160, SWL_BW_AUTO},
              ));

/*
 * @brief convert NL80211_CHAN_WIDTH_xxx into swl bandwidth enum ID
 *
 * @param nl80211Bw nl80211 bandwidth id
 *
 * @return valid SWL_BW_ or SWL_BW_MAX if not match is found
 */
swl_bandwidth_e wld_nl80211_bwNlToSwl(uint32_t nl80211Bw) {
    uint32_t* pSwlBw = (uint32_t*) swl_table_getMatchingValue(&sChannelWidthMap, 2, 0, &nl80211Bw);
    ASSERT_NOT_NULL(pSwlBw, SWL_BW_MAX, ME, "unmatch nl bw %d", nl80211Bw);
    return (swl_bandwidth_e) (*pSwlBw);
}

/*
 * @brief convert swl bandwidth enum ID into NL80211_CHAN_WIDTH_xxx
 *
 * @param swlBw swl bandwidth enum id
 *
 * @return valid NL80211_CHAN_WIDTH_xxx or -1 if not match is found
 */
int32_t wld_nl80211_bwSwlToNl(swl_bandwidth_e swlBw) {
    ASSERTI_FALSE(swlBw <= SWL_BW_AUTO, -1, ME, "unmatch swl bw %d", swlBw);
    ASSERT_FALSE(swlBw >= SWL_BW_MAX, -1, ME, "unmatch swl bw %d", swlBw);
    uint32_t bwe = swlBw;
    uint32_t* pNlBw = (uint32_t*) swl_table_getMatchingValue(&sChannelWidthMap, 0, 2, &bwe);
    ASSERT_NOT_NULL(pNlBw, -1, ME, "unmatch swl bw %d", swlBw);
    return (int32_t) *pNlBw;
}

/*
 * @brief convert NL80211_CHAN_WIDTH_xxx into channel bandwidth value in MHz
 *
 * @param nl80211Bw nl80211 bandwidth id
 *
 * @return valid channel bandwidth value or 0 if not match is found
 */
uint32_t wld_nl80211_bwNlToVal(uint32_t nl80211Bw) {
    uint32_t* pBwVal = (uint32_t*) swl_table_getMatchingValue(&sChannelWidthMap, 1, 0, &nl80211Bw);
    ASSERT_NOT_NULL(pBwVal, 0, ME, "unmatch nl bw %d", nl80211Bw);
    return *pBwVal;
}

/*
 * @brief convert channel bandwidth value in MHz into NL80211_CHAN_WIDTH_xxx
 *
 * @param bwVal channel bandwidth value in MHz
 *
 * @return valid NL80211_CHAN_WIDTH_xxx enum or -1 if not match is found
 */
int32_t wld_nl80211_bwValToNl(uint32_t bwVal) {
    uint32_t* pNlBw = (uint32_t*) swl_table_getMatchingValue(&sChannelWidthMap, 0, 1, &bwVal);
    ASSERT_NOT_NULL(pNlBw, -1, ME, "unmatch bw val %d", bwVal);
    return (int32_t) *pNlBw;
}

SWL_TABLE(sBandsMap,
          ARR(uint32_t nl80211Band; swl_freqBand_e swlBand; ),
          ARR(swl_type_uint32, swl_type_uint32, ),
          ARR({NL80211_BAND_2GHZ, SWL_FREQ_BAND_2_4GHZ},
              {NL80211_BAND_5GHZ, SWL_FREQ_BAND_5GHZ},
              {NL80211_BAND_6GHZ, SWL_FREQ_BAND_6GHZ},
              ));

/*
 * @brief convert swl frequency band enum ID into NL80211_BAND_xxx
 *
 * @param swlFb swl frequency band  enum id
 *
 * @return valid NL80211_BAND_xxx or -1 if not match is found
 */
int32_t wld_nl80211_freqBandSwlToNl(swl_freqBand_e swlFb) {
    ASSERT_FALSE((uint32_t) swlFb >= SWL_BW_MAX, -1, ME, "unmatch swl fb %d", swlFb);
    uint32_t fb = swlFb;
    uint32_t* pNlBand = (uint32_t*) swl_table_getMatchingValue(&sBandsMap, 0, 1, &fb);
    ASSERT_NOT_NULL(pNlBand, -1, ME, "unmatch swl fb %d", swlFb);
    return (int32_t) *pNlBand;
}

/*
 * @brief convert nl frequency band enum ID NL80211_BAND_xxx into swl freq band enum id
 *
 * @param swlFb nl frequency band enum id NL80211_BAND_xxx
 *
 * @return valid swl freq band enum id or SWL_FREQ_BAND_MAX if not match is found
 */
swl_freqBand_e wld_nl80211_freqBandNlToSwl(uint32_t nl80211Fb) {
    uint32_t* pSwlFb = (uint32_t*) swl_table_getMatchingValue(&sBandsMap, 1, 0, &nl80211Fb);
    ASSERTS_NOT_NULL(pSwlFb, SWL_FREQ_BAND_MAX, ME, "unmatch nl fb %d", nl80211Fb);
    return (swl_freqBand_e) (*pSwlFb);
}

