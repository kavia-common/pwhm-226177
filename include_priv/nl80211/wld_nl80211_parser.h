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
 * This file includes nl80211 msg parser definitions
 */

#ifndef INCLUDE_PRIV_NL80211_WLD_NL80211_PARSER_H_
#define INCLUDE_PRIV_NL80211_WLD_NL80211_PARSER_H_

#include "wld_nl80211_api_priv.h"
#include "wld_nl80211_types.h"

/*
 * @brief parse nl msg attributes into interface info struct
 *
 * @param tb array of attributes from parsed nl msg
 * @param pWlIface pointer to info struct to be filled
 *
 * @return SWL_RC_OK parsing done successfully
 *         <= SWL_RC_ERROR parsing error
 */
swl_rc_ne wld_nl80211_parseInterfaceInfo(struct nlattr* tb[], wld_nl80211_ifaceInfo_t* pWlIface);

/*
 * @brief parse nl channel definition attributes into chan spec struct
 *
 * @param tb array of attributes from parsed nl msg
 * @param pChanSpec pointer to chanspec  struct to be filled
 *
 * @return SWL_RC_DONE parsing done successfully
 *         SWL_RC_OK parsing done but some fields are missing
 *         <= SWL_RC_ERROR parsing error
 */
swl_rc_ne wld_nl80211_parseChanSpec(struct nlattr* tb[], wld_nl80211_chanSpec_t* pChanSpec);

/*
 * @brief parse nl msg attributes into wiphy info struct
 *
 * @param tb array of attributes from parsed nl msg
 * @param pWiphy pointer to wiphy info  struct to be filled
 *
 * @return SWL_RC_DONE parsing done successfully
 *         SWL_RC_OK parsing done but some fields are missing
 *         <= SWL_RC_ERROR parsing error
 */
swl_rc_ne wld_nl80211_parseWiphyInfo(struct nlattr* tb[], wld_nl80211_wiphyInfo_t* pWiphy);

/*
 * @brief parse nl msg attributes into station info struct
 *
 * @param tb array of attributes from parsed nl msg
 * @param pStation pointer to station info  struct to be filled
 *
 * @return SWL_RC_DONE parsing done successfully
 *         SWL_RC_OK parsing done but some fields are missing
 *         <= SWL_RC_ERROR parsing error
 */
swl_rc_ne wld_nl80211_parseStationInfo(struct nlattr* tb[], wld_nl80211_stationInfo_t* pStation);


/*
 * @brief get channel's survey info from nl msg attributes
 *
 * @param tb array of attributes from parsed nl msg
 * @param requestData pointer to channel survey info  struct to be filled
 *
 * @return SWL_RC_DONE parsing done successfully
 *         SWL_RC_OK parsing done but some fields are missing
 *         <= SWL_RC_ERROR parsing error
 */
swl_rc_ne wld_nl80211_parseChanSurveyInfo(struct nlattr* tb[], wld_nl80211_channelSurveyInfo_t* pChanSurveyInfo);

/*
 * @brief parse nl msg attributes into scan result struct
 *
 * @param tb array of attributes from parsed nl msg
 * @param pResult pointer to scan result struct to be filled
 *
 * @return SWL_RC_OK parsing done successfully
 *         SWL_RC_CONTINUE partial result to be skipped
 *         <= SWL_RC_ERROR parsing error
 */
swl_rc_ne wld_nl80211_parseScanResult(struct nlattr* tb[], wld_scanResultSSID_t* pResult);


/*
 * @brief parse nl msg attributes into scan result struct, and skip unmatched frequency band
 *
 * @param tb array of attributes from parsed nl msg
 * @param pResult pointer to scan result struct to be filled
 * @param band frequency band to be matched
 *
 * @return SWL_RC_OK parsing done successfully
 *         SWL_RC_CONTINUE partial result to be skipped
 *         <= SWL_RC_ERROR parsing error
 */
swl_rc_ne wld_nl80211_parseScanResultPerFreqBand(struct nlattr* tb[], wld_scanResultSSID_t* pResult, swl_freqBandExt_e band);

/*
 * @brief parse nl msg attributes into mgmt frame struct
 *
 * @param tb array of attributes from parsed nl msg
 * @param mgmtFrame pointer to mgmt frame struct to be filled
 *
 * @return SWL_RC_OK parsing done successfully
 *         <= SWL_RC_ERROR parsing error
 */
swl_rc_ne wld_nl80211_parseMgmtFrame(struct nlattr* tb[], wld_nl80211_mgmtFrame_t* mgmtFrame);

/*
 * @brief parse nl msg attributes into tx mgmt frame struct
 *
 * @param tb array of attributes from parsed nl msg
 * @param txMgmtFrame pointer to mgmt frame struct to be filled
 *
 * @return SWL_RC_OK parsing done successfully
 *         <= SWL_RC_ERROR parsing error
 */
swl_rc_ne wld_nl80211_parseMgmtFrameTxStatus(struct nlattr* tb[], wld_nl80211_mgmtFrameTxStatus_t* mgmtFrameTxStatus);

/*
 * @brief parse nl msg attributes into radar event struct
 *
 * @param tb array of attributes from parsed nl msg
 * @param pRadarEvtInfo pointer to radar event struct to be filled
 *
 * @return SWL_RC_OK parsing done successfully
 *         <= SWL_RC_ERROR parsing error
 */
swl_rc_ne wld_nl80211_parseRadarInfo(struct nlattr* tb[], wld_nl80211_radarEvtInfo_t* pRadarEvtInfo);

/*
 * @brief parse nl msg attributes into reg domain change event struct
 *
 * @param tb array of attributes from parsed nl msg
 * @param pRegChangeInfo pointer to reg domain changed event struct to be filled
 *
 * @return SWL_RC_OK parsing done successfully
 *         <= SWL_RC_ERROR parsing error
 */
swl_rc_ne wld_nl80211_parseRegChangeInfo(struct nlattr* tb[], wld_nl80211_regChangeInfo_t* pRegChangeInfo);

#endif /* INCLUDE_PRIV_NL80211_WLD_NL80211_PARSER_H_ */
