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
#include "wld.h"
#include "wld_util.h"
#include "wld_wpaSupp_cfgFile.h"
#include "wld_wpaSupp_ep_api.h"
#include "wld_wpaCtrl_api.h"
#include "wld_endpoint.h"
#include "wld_radio.h"

#define ME "wpaSupp"


bool s_sendWpaSuppCommand(T_EndPoint* pEP, char* cmd, const char* reason) {
    ASSERTS_NOT_NULL(pEP, false, ME, "NULL");
    ASSERTS_TRUE(wld_wpaCtrlInterface_isReady(pEP->wpaCtrlInterface), false, ME, "Connection with hostapd not yet established");
    T_Radio* pR = pEP->pRadio;
    ASSERTS_NOT_NULL(pR, false, ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: send wpaSupp cmd %s for %s", pR->Name, cmd, reason);
    return wld_wpaCtrl_sendCmdCheckResponse(pEP->wpaCtrlInterface, cmd, "OK");
}

/**
 * @brief Disconnect the endpoint from an AP
 *
 * @param pEP endpoint
 * @return - SWL_RC_OK when the command is sent successfully
 *         - Otherwise SWL_RC_ERROR
 */
swl_rc_ne wld_wpaSupp_ep_disconnect(T_EndPoint* pEP) {
    ASSERT_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pEP->pSSID, SWL_RC_ERROR, ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: Disconnecting from "MAC_PRINT_FMT, pEP->Name, MAC_PRINT_ARG(pEP->pSSID->BSSID));

    bool ret = s_sendWpaSuppCommand(pEP, "DISCONNECT", "");
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: failed to send disconnect command", pEP->Name);
    return SWL_RC_OK;
}

/*
 * @brief get the EP's global status information
 * eg: when ep is connected
 * bssid=98:42:65:2d:23:43
 * freq=5260
 * ssid=prplos
 * id=0
 * mode=station
 * multi_ap_profile=0
 * multi_ap_primary_vlanid=0
 * wifi_generation=6
 * pairwise_cipher=CCMP
 * group_cipher=CCMP
 * key_mgmt=WPA2-PSK
 * wpa_state=COMPLETED
 * address=4e:ba:7d:80:80:87
 * uuid=4c5b5a59-4f5c-f048-ff5c-50484c5b5a59
 * ieee80211ac=1
 *
 * @param pEP endpoint
 * @param reply output buffer
 * @param maxReplySize output buffer size
 *
 * @return SWL_RC_OK on success, error code otherwise
 */
swl_rc_ne wld_wpaSupp_ep_getAllStatusDetails(T_EndPoint* pEP, char* reply, size_t replySize) {
    ASSERTS_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: Send STATUS command", pEP->Name);
    bool ret = wld_wpaCtrl_sendCmdSynced(pEP->wpaCtrlInterface, "STATUS", reply, replySize);
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: failed to query ep status", pEP->Name);
    return SWL_RC_OK;
}

swl_rc_ne wld_wpaSupp_ep_getOneStatusDetail(T_EndPoint* pEP, const char* key, char* valStr, size_t valStrSize) {
    char reply[1024] = {0};
    swl_rc_ne rc = wld_wpaSupp_ep_getAllStatusDetails(pEP, reply, sizeof(reply));
    ASSERT_TRUE(swl_rc_isOk(rc), rc, ME, "failed to get global ep status");
    int valStrLen = wld_wpaCtrl_getValueStr(reply, key, valStr, valStrSize);
    ASSERT_FALSE(valStrLen <= 0, SWL_RC_ERROR, ME, "%s: not found status field %s", pEP->Name, key);
    SAH_TRACEZ_INFO(ME, "%s: %s = (%s)", pEP->Name, key, valStr);
    ASSERT_TRUE(valStrLen < (int) valStrSize, SWL_RC_ERROR,
                ME, "%s: buffer too short for field %s (l:%d,s:%zu)", pEP->Name, key, valStrLen, valStrSize);
    return SWL_RC_OK;
}

/*
 * @brief Increase the WPS credentials security mode to WPA2-WPA3-Personal
 * if the connected AP is broadcasting WPA2-WPA3-Personal.
 *
 * @param pEP endpoint
 * @param creds current WPS credentials to modify
 *
 * @return SWL_RC_OK on success, error code otherwise
 */
swl_rc_ne wld_wpaSupp_ep_increaseSecurityModeInCreds(T_EndPoint* pEP, T_WPSCredentials* creds) {
    ASSERT_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(creds, SWL_RC_INVALID_PARAM, ME, "NULL");

    wld_scanResults_t res;
    amxc_llist_init(&res.ssids);

    swl_rc_ne rc = wld_wpaSupp_ep_getScanResults(pEP, &res);
    if(rc == SWL_RC_OK) {
        amxc_llist_for_each(it, &res.ssids) {
            wld_scanResultSSID_t* pResult = amxc_container_of(it, wld_scanResultSSID_t, it);
            char ssidStr[SSID_NAME_LEN];
            memset(ssidStr, 0, sizeof(ssidStr));
            convSsid2Str(pResult->ssid, pResult->ssidLen, ssidStr, sizeof(ssidStr));
            if(swl_str_matches(ssidStr, creds->ssid)
               && (pResult->secModeEnabled == SWL_SECURITY_APMODE_WPA2_WPA3_P)) {
                SAH_TRACEZ_INFO(ME, "Increase security mode to WPA2-WPA3 from WPS credentials");
                creds->secMode = SWL_SECURITY_APMODE_WPA2_WPA3_P;
                break;
            }
        }
    }

    wld_radio_scanresults_cleanup(&res);
    return rc;
}

/**
 * @brief get the scan results from wpa_supplicant
 * @param pEP endpoint
 * @param wld_scanResults_t output scan results
 *
 * @return SWL_RC_OK on success, error code otherwise
 */
swl_rc_ne wld_wpaSupp_ep_getScanResults(T_EndPoint* pEP, wld_scanResults_t* res) {
    ASSERT_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(res, SWL_RC_INVALID_PARAM, ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: get scan results", pEP->Name);

    size_t maxMsgLen = wld_wpaCtrl_getMaxMsgLen();
    char reply[maxMsgLen + 1];
    memset(reply, 0, sizeof(reply));
    bool ret = wld_wpaCtrl_sendCmdSynced(pEP->wpaCtrlInterface, "SCAN_RESULTS", reply, sizeof(reply));
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: failed to get scan results", pEP->Name);

    char* tmpStr = reply;
    while(tmpStr != NULL && (*tmpStr != '\0')) {
        int32_t rssi = 0;
        int32_t frequency = 0;
        char security[128] = {0};
        char ssid[SSID_NAME_LEN] = {0};
        char bssid[ETHER_ADDR_STR_LEN] = {0};
        if(sscanf(tmpStr, "%s \t %d \t %d \t %s \t %s", bssid, &frequency, &rssi, security, ssid) == 5) {
            wld_scanResultSSID_t* result = calloc(1, sizeof(wld_scanResultSSID_t));
            if(result != NULL) {
                memcpy(result->ssid, ssid, SSID_NAME_LEN);
                result->ssidLen = strlen(ssid);
                SWL_MAC_CHAR_TO_BIN(&result->bssid, bssid);
                result->rssi = rssi;
                swl_chanspec_t chanspec;
                swl_chanspec_channelFromMHz(&chanspec, (uint32_t) frequency);
                result->channel = chanspec.channel;
                wld_wpaSupp_ep_getBssScanInfo(pEP, &result->bssid, result);
                if(swl_str_find(security, "WPA2-SAE") >= 0) {
                    result->secModeEnabled = SWL_SECURITY_APMODE_WPA3_P;
                } else if(swl_str_find(security, "WPA2-PSK+SAE") >= 0) {
                    result->secModeEnabled = SWL_SECURITY_APMODE_WPA2_WPA3_P;
                } else if(swl_str_find(security, "WPA2-PSK") >= 0) {
                    result->secModeEnabled = SWL_SECURITY_APMODE_WPA2_P;
                }
                amxc_llist_it_init(&result->it);
                amxc_llist_append(&res->ssids, &result->it);
            }
        }
        tmpStr = strstr(tmpStr, "\n");
        if(tmpStr != NULL) {
            tmpStr++;
        }
    }

    return SWL_RC_OK;
}

/**
 * @brief get the scanned bss details
 *
 * @param pInterface Wpa ctrl interface used to send command
 * @param pMacBin the bssid bin value
 * @param[out] pResult pointer to output result struct
 * @param pWirelessDevIE optional pointer to fetched wireless dev IEs from last ProbeResp/Beacon
 * @return - SWL_RC_OK when the value is retrieved successfully (valid)
 *         - Otherwise SWL_RC_ERROR
 */
swl_rc_ne wld_wpaSupp_getBssScanInfoExt(wld_wpaCtrlInterface_t* pInterface, swl_macBin_t* pMacBin, wld_scanResultSSID_t* pResult, swl_wirelessDevice_infoElements_t* pWirelessDevIE) {
    ASSERTS_NOT_NULL(pMacBin, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_FALSE(swl_mac_binIsNull(pMacBin), SWL_RC_INVALID_PARAM, ME, "invalid mac");
    char buf[2048] = {0};
    swl_str_catFormat(buf, sizeof(buf), "BSS %s", swl_typeMacBin_toBuf32Ref(pMacBin).buf);
    ASSERTI_TRUE(wld_wpaCtrl_sendCmdSynced(pInterface, buf, buf, sizeof(buf)), SWL_RC_ERROR, ME, "fail to send request");
    wld_scanResultSSID_t result;
    memset(&result, 0, sizeof(result));
    if(pResult) {
        memcpy(&result, pResult, sizeof(result));
    }

    swl_chanspec_t chanSpec = SWL_CHANSPEC_EMPTY;
    swl_chanspec_fromMHz(&chanSpec, wld_wpaCtrl_getValueInt(buf, "freq"));
    result.channel = chanSpec.channel;

    char str[2048] = {0};
    ssize_t strL = 0;
    if(((strL = wld_wpaCtrl_getValueStr(buf, "ie", str, sizeof(str))) > 0) ||
       ((strL = wld_wpaCtrl_getValueStr(buf, "beacon_ie", str, sizeof(str))) > 0)) {
        swl_wirelessDevice_infoElements_t wirelessDevIE;
        memset(&wirelessDevIE, 0, sizeof(wirelessDevIE));
        size_t iesLen = strL / 2;
        swl_bit8_t iesData[iesLen + (strL % 2)];
        if(swl_hex_toBytes(iesData, sizeof(iesData), str, strL)) {
            swl_parsingArgs_t parsingArgs;
            swl_parsingArgs_t* pParsingArgs = NULL;
            if(chanSpec.channel > 0) {
                parsingArgs.seenOnChanspec = chanSpec;
                pParsingArgs = &parsingArgs;
            }
            ssize_t parsedLen = swl_80211_parseInfoElementsBuffer(&wirelessDevIE, pParsingArgs, iesLen, iesData);
            if(parsedLen < (ssize_t) iesLen) {
                SAH_TRACEZ_WARNING(ME, "Error while parsing probe/beacon rcvd IEs (%zi < %zu)", parsedLen, iesLen);
            }
            if(parsedLen > 0) {
                wld_util_copyScanInfoFromIEs(&result, &wirelessDevIE);
                W_SWL_SETPTR(pWirelessDevIE, wirelessDevIE);
            }
        }
    }

    if((strL = wld_wpaCtrl_getValueStr(buf, "ssid", str, sizeof(str))) > 0) {
        result.ssidLen = strL;
        swl_str_ncopy((char*) result.ssid, sizeof(result.ssid), str, strL);
    }
    if((wld_wpaCtrl_getValueStr(buf, "bssid", str, sizeof(str)) > 0) &&
       (swl_mac_charIsValidStaMac((swl_macChar_t*) str))) {
        SWL_MAC_CHAR_TO_BIN(&result.bssid, str);
    }
    result.snr = wld_wpaCtrl_getValueInt(buf, "snr");
    result.noise = wld_wpaCtrl_getValueInt(buf, "noise");
    result.rssi = wld_wpaCtrl_getValueInt(buf, "level");
    result.linkrate = wld_wpaCtrl_getValueInt(buf, "est_throughput");

    W_SWL_SETPTR(pResult, result);
    return SWL_RC_OK;
}

/**
 * @brief get the scanned bss details
 *
 * @param pInterface Wpa ctrl interface used to send command
 * @param pMacBin the bssid bin value
 * @param pResult pointer to output result struct
 * @return - SWL_RC_OK when the value is retrieved successfully (valid)
 *         - Otherwise SWL_RC_ERROR
 */
swl_rc_ne wld_wpaSupp_getBssScanInfo(wld_wpaCtrlInterface_t* pInterface, swl_macBin_t* pMacBin, wld_scanResultSSID_t* pResult) {
    return wld_wpaSupp_getBssScanInfoExt(pInterface, pMacBin, pResult, NULL);
}

/**
 * @brief get the scanned bss details
 *
 * @param pEP endpoint
 * @param pMacBin the bssid bin value
 * @param pResult pointer to output result struct
 * @param pWirelessDevIE optional pointer to fetched wireless dev IEs from last ProbeResp/Beacon
 * @return - SWL_RC_OK when the value is retrieved successfully (valid)
 *         - Otherwise SWL_RC_ERROR
 */
swl_rc_ne wld_wpaSupp_ep_getBssScanInfoExt(T_EndPoint* pEP, swl_macBin_t* pMacBin, wld_scanResultSSID_t* pResult, swl_wirelessDevice_infoElements_t* pWirelessDevIE) {
    ASSERT_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_wpaSupp_getBssScanInfoExt(pEP->wpaCtrlInterface, pMacBin, pResult, pWirelessDevIE);
}

/**
 * @brief get the scanned bss details
 *
 * @param pEP endpoint
 * @param pMacBin the bssid bin value
 * @param[out] pResult pointer to output result struct
 * @return - SWL_RC_OK when the value is retrieved successfully (valid)
 *         - Otherwise SWL_RC_ERROR
 */
swl_rc_ne wld_wpaSupp_ep_getBssScanInfo(T_EndPoint* pEP, swl_macBin_t* pMacBin, wld_scanResultSSID_t* pResult) {
    return wld_wpaSupp_ep_getBssScanInfoExt(pEP, pMacBin, pResult, NULL);
}

swl_rc_ne wld_wpaSupp_getBssidFromStatusDetails(const char* statusInfo, swl_macBin_t* pBssid) {
    swl_macChar_t bssidStr = SWL_MAC_CHAR_NEW();
    swl_macBin_t bssid = SWL_MAC_BIN_NEW();
    if((wld_wpaCtrl_getValueStr(statusInfo, "bssid", bssidStr.cMac, SWL_MAC_CHAR_LEN) <= 0) ||
       (!swl_mac_charIsValidStaMac(&bssidStr)) ||
       (!swl_mac_charToBin(&bssid, &bssidStr))) {
        SAH_TRACEZ_INFO(ME, "no remote bssid");
        return SWL_RC_INVALID_STATE;
    }
    SAH_TRACEZ_INFO(ME, "bssid(%s)", bssidStr.cMac);
    W_SWL_SETPTR(pBssid, bssid);
    return SWL_RC_OK;
}

SWL_TABLE(sWifiGenOperStdMaps,
          ARR(char* wifiGenStr; swl_radStd_e operStd; ),
          ARR(swl_type_charPtr, swl_type_uint32, ),
          ARR({"4", SWL_RADSTD_N},
              {"5", SWL_RADSTD_AC},
              {"6", SWL_RADSTD_AX},
              {"7", SWL_RADSTD_BE},
              ));
swl_rc_ne wld_wpaSupp_getOperStdFromStatusDetails(const char* statusInfo, swl_radStd_e* pRes) {
    ASSERTS_STR(statusInfo, SWL_RC_INVALID_PARAM, ME, "empty");
    char wifiGeneration[16] = {0};
    if(wld_wpaCtrl_getValueStr(statusInfo, "wifi_generation", wifiGeneration, sizeof(wifiGeneration)) <= 0) {
        SAH_TRACEZ_INFO(ME, "no wifi_gen");
        return SWL_RC_ERROR;
    }
    swl_radStd_e* pOperStd = (swl_radStd_e*) swl_table_getMatchingValue(&sWifiGenOperStdMaps, 1, 0, wifiGeneration);
    ASSERTI_NOT_NULL(pOperStd, SWL_RC_INVALID_PARAM, ME, "unknown wifi_gen (%s)", wifiGeneration);
    SAH_TRACEZ_INFO(ME, "wifiGeneration(%s) -> operStd(%s)", wifiGeneration, swl_radStd_unknown_str[*pOperStd]);
    W_SWL_SETPTR(pRes, *pOperStd);
    return SWL_RC_OK;
}

/**
 * @brief get the AP's bssid to which the endpoint is connected
 *
 * @param pEP endpoint
 * @param bssid the bssid string value
 * @return - SWL_RC_OK when the value is retrieved successfully (valid)
 *         - Otherwise SWL_RC_ERROR
 */
swl_rc_ne wld_wpaSupp_ep_getBssid(T_EndPoint* pEP, swl_macChar_t* bssid) {
    swl_macChar_t tmpBssid = SWL_MAC_CHAR_NEW();
    swl_rc_ne rc = wld_wpaSupp_ep_getOneStatusDetail(pEP, "bssid", tmpBssid.cMac, SWL_MAC_CHAR_LEN);
    ASSERT_TRUE(swl_rc_isOk(rc), rc, ME, "failed to get endpoint bssid");
    ASSERT_TRUE(swl_mac_charIsValidStaMac(&tmpBssid), SWL_RC_ERROR, ME, "%s: invalid bssid (%s)", pEP->Name, tmpBssid.cMac);
    W_SWL_SETPTR(bssid, tmpBssid);
    return SWL_RC_OK;
}

/**
 * @brief get the AP's SSID to which the endpoint is connected
 *
 * @param pEP endpoint
 * @param ssid the remote AP ssid buffer
 * @param ssidSize the remote AP ssid buffer size
 * @return - SWL_RC_OK when the value is retrieved successfully (valid)
 *         - Otherwise SWL_RC_ERROR
 */
swl_rc_ne wld_wpaSupp_ep_getSsid(T_EndPoint* pEP, char* ssid, size_t ssidSize) {
    return wld_wpaSupp_ep_getOneStatusDetail(pEP, "ssid", ssid, ssidSize);
}

SWL_TABLE(sWpaStateDescMaps,
          ARR(char* wpaStateDesc; wld_epConnectionStatus_e epConnState; ),
          ARR(swl_type_charPtr, swl_type_uint32, ),
          ARR({"DISCONNECTED", EPCS_DISCONNECTED},
              {"INACTIVE", EPCS_IDLE},
              {"INTERFACE_DISABLED", EPCS_IDLE},
              {"SCANNING", EPCS_DISCOVERING},
              {"AUTHENTICATING", EPCS_CONNECTING},
              {"ASSOCIATING", EPCS_CONNECTING},
              {"ASSOCIATED", EPCS_CONNECTING},
              {"4WAY_HANDSHAKE", EPCS_CONNECTING},
              {"GROUP_HANDSHAKE", EPCS_CONNECTING},
              {"COMPLETED", EPCS_CONNECTED},
              ));

swl_rc_ne wld_wpaSupp_getConnStatusFromStatusDetails(const char* statusInfo, wld_epConnectionStatus_e* pRes) {
    ASSERTS_STR(statusInfo, SWL_RC_INVALID_PARAM, ME, "empty");
    char state[64] = {0};
    if(wld_wpaCtrl_getValueStr(statusInfo, "wpa_state", state, sizeof(state)) <= 0) {
        return SWL_RC_ERROR;
    }
    wld_epConnectionStatus_e* pConnSt = (wld_epConnectionStatus_e*) swl_table_getMatchingValue(&sWpaStateDescMaps, 1, 0, state);
    ASSERTI_NOT_NULL(pConnSt, SWL_RC_INVALID_PARAM, ME, "unknown wpa_state (%s)", state);
    SAH_TRACEZ_INFO(ME, "state(%s) -> connState(%s)", state, cstr_EndPoint_connectionStatus[*pConnSt]);
    W_SWL_SETPTR(pRes, *pConnSt);
    return SWL_RC_OK;
}

/**
 * @brief get the EP's current connection status
 * "DISCONNECTED", "INACTIVE", "INTERFACE_DISABLED", "SCANNING", "AUTHENTICATING",
 * "ASSOCIATING", "ASSOCIATED", "4WAY_HANDSHAKE", "GROUP_HANDSHAKE", "COMPLETED"
 * @param pEP endpoint
 * @param pEPConnState output endpoint connection state based on retrieved wpa_state
 * @return - SWL_RC_OK when the wpa_state is found and parsed successfully
 *         - Otherwise SWL_RC_ERROR
 */
swl_rc_ne wld_wpaSupp_ep_getConnState(T_EndPoint* pEP, wld_epConnectionStatus_e* pEPConnState) {
    ASSERTS_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTI_TRUE(wld_wpaCtrlInterface_isReady(pEP->wpaCtrlInterface), SWL_RC_ERROR,
                 ME, "%s: main wpactrl iface is not ready", pEP->Name);
    char reply[1024] = {0};
    swl_rc_ne rc = wld_wpaSupp_ep_getAllStatusDetails(pEP, reply, sizeof(reply));
    ASSERT_TRUE(swl_rc_isOk(rc), rc, ME, "%s: failed to get global ep status", pEP->Name);
    return wld_wpaSupp_getConnStatusFromStatusDetails(reply, pEPConnState);
}

/**
 * @brief Reload wpa_supp configuration
 *
 * @param pEP endpoint
 * @return - SWL_RC_OK when the command is sent successfully
 *         - Otherwise SWL_RC_ERROR
 */
swl_rc_ne wld_wpaSupp_ep_reconfigure(T_EndPoint* pEP) {
    ASSERT_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: reconfiguring EP", pEP->Name);

    bool ret = s_sendWpaSuppCommand(pEP, "RECONFIGURE", "");
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: failed to send reconfigure command", pEP->Name);
    return SWL_RC_OK;
}

/**
 * @brief wld_wpaSupp_ep_startWpsPbc
 *
 * send wps_pbc command to wpa_supplicant
 *
 * @param pEP: Pointer to the endpoint.
 * @param bssid: The BSSID to connect to (optional)
 * @return - SWL_RC_OK when the command is sent successfully
 *         - Otherwise SWL_RC_ERROR
 */
swl_rc_ne wld_wpaSupp_ep_startWpsPbc(T_EndPoint* pEP, swl_macChar_t* bssid) {
    SAH_TRACEZ_INFO(ME, "EndPoint %s %s send wps_pbc", pEP->alias, pEP->Name);

    char cmd[64] = {0};
    swl_str_catFormat(cmd, sizeof(cmd), "WPS_PBC");
    if(pEP->multiAPEnable) {
        swl_str_catFormat(cmd, sizeof(cmd), " multi_ap=1");
    }
    if(!swl_mac_charIsNull(bssid)) {
        swl_str_catFormat(cmd, sizeof(cmd), " %s", bssid->cMac);
    }
    bool ret = s_sendWpaSuppCommand(pEP, cmd, "WPS_PBC");
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: failed to send wps_pbc command", pEP->Name);
    return SWL_RC_OK;
}

/**
 * @brief wld_wpaSupp_ep_startWpsPin
 *
 * send wps_pin command to wpa_supplicant
 *
 * @param pEP: Pointer to the endpoint.
 * @param pin: The PIN to use
 * @param bssid: The BSSID to connect to (optional)
 * @return - SWL_RC_OK when the command is sent successfully
 *         - Otherwise SWL_RC_ERROR
 */
swl_rc_ne wld_wpaSupp_ep_startWpsPin(T_EndPoint* pEP, char* pin, swl_macChar_t* bssid) {
    ASSERT_STR(pin, SWL_RC_INVALID_PARAM, ME, "NULL PIN");
    SAH_TRACEZ_INFO(ME, "%s: send wps start pin %s", pEP->Name, pin);
    char cmd[64] = {0};
    //WPS client PIN started with a default wps session walk time
    char* bssidStr = "any";
    if(!swl_mac_charIsNull(bssid)) {
        bssidStr = bssid->cMac;
    }
    swl_str_catFormat(cmd, sizeof(cmd), "WPS_PIN %s %s", bssidStr, pin);
    bool ret = s_sendWpaSuppCommand(pEP, cmd, "WPS_PIN");
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: failed to send wps_pin command", pEP->Name);
    return SWL_RC_OK;
}

/**
 * @brief wld_wpaSupp_ep_cancelWps
 *
 * send wps_cancel command to wpa_supplicant
 *
 * @param pEP: Pointer to the endpoint
 * @return - SWL_RC_OK when the command is sent successfully
 *         - Otherwise SWL_RC_ERROR
 */
swl_rc_ne wld_wpaSupp_ep_cancelWps(T_EndPoint* pEP) {
    SAH_TRACEZ_INFO(ME, "%s: send wps stop", pEP->Name);

    bool ret = s_sendWpaSuppCommand(pEP, "WPS_CANCEL", "WPS_CANCEL");
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "%s: failed to send wps_cancel command", pEP->Name);
    return SWL_RC_OK;
}
