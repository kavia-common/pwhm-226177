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
#include "swla/swla_table.h"
#include "swl/swl_string.h"
#include "swl/swl_hex.h"
#include "swl/swl_genericFrameParser.h"
#include "wld_channel.h"
#include "wld_wps.h"
#include "wld_wpaCtrlInterface_priv.h"
#include "wld_wpaCtrlMngr_priv.h"
#include "wld_wpaCtrl_events.h"
#include "wld_wpaSupp_parser.h"

#define ME "wpaCtrl"

static int s_getValueStr(const char* pData, const char* pKey, char* pValue, int* length) {
    ASSERTS_STR(pData, 0, ME, "Empty data");
    ASSERTS_STR(pKey, 0, ME, "Empty key");
    char* paramStart = (char*) pData;
    while(((paramStart = strstr(paramStart, pKey)) != NULL) &&
          (paramStart != pData) && (isalnum(*(paramStart - 1)))) {
        //skip found key when it is a suffix
        paramStart += strlen(pKey);
    }
    ASSERTI_STR(paramStart, 0, ME, "Failed to find token %s", pKey);
    char* paramEnd = &paramStart[strlen(pKey)];
    ASSERTI_EQUALS(paramEnd[0], '=', 0, ME, "partial token %s", pKey);
    char* valueStart = &paramEnd[1];
    char* valueEnd = &paramEnd[strlen(paramEnd)];

    char* p;
    // locate value string end, by looking for next parameter
    // as value string may include spaces
    for(p = strchr(valueStart, '='); (p != NULL) && (p > valueStart); p--) {
        if(isspace(p[0])) {
            break;
        }
    }
    if(p != NULL) {
        valueEnd = p;
    }
    // clear the '\n' and spaces at the end of value string
    for(p = valueEnd; (p != NULL) && (p > valueStart); p--) {
        if(isspace(p[0])) {
            valueEnd = p;
            continue;
        }
        if(p[0] != 0) {
            break;
        }
    }
    ASSERTI_NOT_NULL(valueEnd, 0, ME, "Can locate string value end of token %s", pKey);

    int valueLen = valueEnd - valueStart;
    if(pValue == NULL) {
        if(length) {
            *length = 0;
        }
        return valueLen;
    }
    pValue[0] = 0;
    ASSERTS_NOT_NULL(length, valueLen, ME, "null dest len");
    ASSERTS_TRUE(*length > 0, valueLen, ME, "null dest len");
    if(*length > valueLen) {
        *length = valueLen;
    }
    strncpy(pValue, valueStart, *length);
    pValue[*length] = 0;

    return valueLen;
}

/* Extension functions We don't care about the length string.*/
int wld_wpaCtrl_getValueStr(const char* pData, const char* pKey, char* pValue, int length) {
    return s_getValueStr(pData, pKey, pValue, &length);
}
/* Extension function get an integer value */
bool wld_wpaCtrl_getValueIntExt(const char* pData, const char* pKey, int32_t* pVal) {
    char strbuf[64] = {0};
    ASSERTS_TRUE(wld_wpaCtrl_getValueStr(pData, pKey, strbuf, sizeof(strbuf)) > 0, false,
                 ME, "key (%s) not found", pKey);
    ASSERT_TRUE(swl_typeInt32_fromChar(pVal, strbuf), false,
                ME, "key (%s) val (%s) num conversion failed", pKey, strbuf);
    return true;
}

/* Extension function get an integer value , returns 0 as default value*/
int wld_wpaCtrl_getValueInt(const char* pData, const char* pKey) {
    int32_t val = 0;
    wld_wpaCtrl_getValueIntExt(pData, pKey, &val);
    return val;
}

#define CALL_IFACE(PIFACE, FNAME, ...) \
    if((PIFACE != NULL)) { \
        SWL_CALL(PIFACE->handlers.FNAME, PIFACE->userData, PIFACE->name, __VA_ARGS__); \
    }
#define CALL_IFACE_RET(PIFACE, ret, FNAME, ...) \
    { \
        ret = SWL_RC_INVALID_PARAM; \
        if(PIFACE != NULL) { \
            ret = SWL_RC_NOT_IMPLEMENTED; \
            if(PIFACE->handlers.FNAME != NULL) { \
                ret = PIFACE->handlers.FNAME(PIFACE->userData, PIFACE->name, __VA_ARGS__); \
            } \
        } \
    }

#define CALL_MGR_I(pIntf, fName, ...) \
    if(pIntf != NULL) { \
        CALL_MGR(pIntf->pMgr, pIntf->name, fName, __VA_ARGS__); \
    }
#define CALL_MGR_I_RET(pIntf, ret, fName, ...) \
    { \
        ret = SWL_RC_INVALID_PARAM; \
        if(pIntf != NULL) { \
            CALL_MGR_RET(pIntf->pMgr, pIntf->name, ret, fName, __VA_ARGS__); \
        } \
    }

#define CALL_MGR_I_NA(pIntf, fName) \
    if(pIntf != NULL) { \
        CALL_MGR_NA(pIntf->pMgr, pIntf->name, fName); \
    }

#define NOTIFY(PIFACE, FNAME, ...) \
    if((PIFACE != NULL)) { \
        /* for VAP/EP events */ \
        CALL_IFACE(PIFACE, FNAME, __VA_ARGS__); \
        /* for RAD/Glob events */ \
        CALL_MGR_I(PIFACE, FNAME, __VA_ARGS__); \
    }

#define NOTIFY_NA(PIFACE, FNAME, ...) \
    if((PIFACE != NULL)) { \
        /* for VAP/EP events */ \
        CALL_INTF_NA(PIFACE, FNAME); \
        /* for RAD/Glob events */ \
        CALL_MGR_I_NA(PIFACE, FNAME); \
    }

typedef void (* evtParser_f)(wld_wpaCtrlInterface_t* pInterface, char* event, char* params);

static void s_wpsInProgress(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params _UNUSED) {
    SAH_TRACEZ_INFO(ME, "%s WPS PBC In progress", pInterface->name);
    CALL_INTF_NA(pInterface, fWpsInProgressMsg);
}

static void s_wpsCancelEvent(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params _UNUSED) {
    SAH_TRACEZ_INFO(ME, "%s WPS CANCEL", pInterface->name);
    NOTIFY_NA(pInterface, fWpsCancelMsg);
}

static void s_wpsTimeoutEvent(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params _UNUSED) {
    SAH_TRACEZ_INFO(ME, "%s WPS TIMEOUT", pInterface->name);
    NOTIFY_NA(pInterface, fWpsTimeoutMsg);
}

static void s_wpsSuccess(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    char buf[SWL_MAC_CHAR_LEN] = {0};
    memcpy(buf, params, SWL_MAC_CHAR_LEN - 1);

    swl_macChar_t mac;
    swl_mac_charToStandard(&mac, buf);

    SAH_TRACEZ_INFO(ME, "%s WPS SUCCESS %s", pInterface->name, mac.cMac);
    CALL_INTF(pInterface, fWpsSuccessMsg, &mac);
}

static void s_wpsOverlapEvent(wld_wpaCtrlInterface_t* pInterface, char* event, char* params _UNUSED) {
    SAH_TRACEZ_INFO(ME, "%s: %s", pInterface->name, event);
    NOTIFY_NA(pInterface, fWpsOverlapMsg);
}

static void s_wpsFailEvent(wld_wpaCtrlInterface_t* pInterface, char* event, char* params _UNUSED) {
    //Example: WPS-FAIL msg=%d config_error=%d
    //         (+Optionally: reason=%d (%s))
    int32_t wpsMsgId = -1;
    /*
     *  WPS_M1 = 0x04,
     *  WPS_M2 = 0x05,
     *  WPS_M2D = 0x06,
     *  WPS_M3 = 0x07,
     *  WPS_M4 = 0x08,
     *  WPS_M5 = 0x09,
     *  WPS_M6 = 0x0a,
     *  WPS_M7 = 0x0b,
     *  WPS_M8 = 0x0c,
     *  WPS_WSC_DONE = 0x0f
     */
    int32_t wpsCfgErr = -1;
    /*
     *  WPS_CFG_NO_ERROR = 0,
     *  WPS_CFG_OOB_IFACE_READ_ERROR = 1,
     *  WPS_CFG_DECRYPTION_CRC_FAILURE = 2,
     *  WPS_CFG_24_CHAN_NOT_SUPPORTED = 3,
     *  WPS_CFG_50_CHAN_NOT_SUPPORTED = 4,
     *  WPS_CFG_SIGNAL_TOO_WEAK = 5,
     *  WPS_CFG_NETWORK_AUTH_FAILURE = 6,
     *  WPS_CFG_NETWORK_ASSOC_FAILURE = 7,
     *  WPS_CFG_NO_DHCP_RESPONSE = 8,
     *  WPS_CFG_FAILED_DHCP_CONFIG = 9,
     *  WPS_CFG_IP_ADDR_CONFLICT = 10,
     *  WPS_CFG_NO_CONN_TO_REGISTRAR = 11,
     *  WPS_CFG_MULTIPLE_PBC_DETECTED = 12,
     *  WPS_CFG_ROGUE_SUSPECTED = 13,
     *  WPS_CFG_DEVICE_BUSY = 14,
     *  WPS_CFG_SETUP_LOCKED = 15,
     *  WPS_CFG_MSG_TIMEOUT = 16,
     *  WPS_CFG_REG_SESS_TIMEOUT = 17,
     *  WPS_CFG_DEV_PASSWORD_AUTH_FAILURE = 18,
     *  WPS_CFG_60G_CHAN_NOT_SUPPORTED = 19,
     *  WPS_CFG_PUBLIC_KEY_HASH_MISMATCH = 20
     */
    wld_wpaCtrl_getValueIntExt(params, "msg", &wpsMsgId);
    wld_wpaCtrl_getValueIntExt(params, "config_error", &wpsCfgErr);
    SAH_TRACEZ_INFO(ME, "%s: %s wpsMsgId(%d) wpsCfgErrId(%d)", pInterface->name, event, wpsMsgId, wpsCfgErr);
    CALL_INTF_NA(pInterface, fWpsFailMsg);
}

static void s_wpsCredReceivedEvent(wld_wpaCtrlInterface_t* pInterface, char* event, char* params) {
    T_WPSCredentials creds;
    memset(&creds, 0, sizeof(creds));
    swl_rc_ne ret = wpaSup_parseWpsReceiveCredentialsEvt(&creds, params, strlen(params));
    SAH_TRACEZ_INFO(ME, "%s: %s ssid(%s) secMode(%d) key(%s)", pInterface->name, event, creds.ssid, creds.secMode, creds.key);
    CALL_INTF(pInterface, fWpsCredReceivedCb, &creds, ret);
}

static void s_wpsStationSuccessEvent(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params _UNUSED) {
    SAH_TRACEZ_INFO(ME, "%s: WPS SUCCESS", pInterface->name);
    CALL_INTF(pInterface, fWpsSuccessMsg, NULL);
}

static void s_wpsSetupUnlockedEvent(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params _UNUSED) {
    SAH_TRACEZ_INFO(ME, "%s WPS Setup UNLOCKED", pInterface->name);
    CALL_INTF(pInterface, fWpsSetupLockedMsg, FALSE);
}

static void s_wpsSetupLockedEvent(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params _UNUSED) {
    SAH_TRACEZ_INFO(ME, "%s WPS Setup LOCKED", pInterface->name);
    CALL_INTF(pInterface, fWpsSetupLockedMsg, TRUE);
}

static void s_apStationConnected(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    //Example: AP-STA-CONNECTED xx:xx:xx:xx:xx:xx
    char buf[SWL_MAC_CHAR_LEN] = {0};
    swl_macBin_t bStationMac;

    memcpy(buf, params, SWL_MAC_CHAR_LEN - 1);
    bool ret = swl_mac_charToBin(&bStationMac, (swl_macChar_t*) buf);
    ASSERT_TRUE(ret, , ME, "fail to convert mac address to binary");
    CALL_INTF(pInterface, fApStationConnectedCb, &bStationMac);
}

static void s_stationEapCompleted(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    //Example: EAPOL-4WAY-HS-COMPLETED xx:xx:xx:xx:xx:xx
    char buf[SWL_MAC_CHAR_LEN] = {0};
    swl_macBin_t bStationMac;

    memcpy(buf, params, SWL_MAC_CHAR_LEN - 1);
    bool ret = swl_mac_charToBin(&bStationMac, (swl_macChar_t*) buf);
    ASSERT_TRUE(ret, , ME, "fail to convert mac address to binary");
    CALL_INTF(pInterface, fApStationEapCompletedCb, &bStationMac);
}

static void s_apStationDisconnected(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    //Example: AP-STA-DISCONNECTED xx:xx:xx:xx:xx:xx
    char buf[SWL_MAC_CHAR_LEN] = {0};
    swl_macBin_t bStationMac;

    memcpy(buf, params, SWL_MAC_CHAR_LEN - 1);
    bool ret = swl_mac_charToBin(&bStationMac, (swl_macChar_t*) buf);
    ASSERT_TRUE(ret, , ME, "fail to convert mac address to binary");
    CALL_INTF(pInterface, fApStationDisconnectedCb, &bStationMac);
}

static void s_apStationAssocFailure(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    //Example: AP-STA-POSSIBLE-PSK-MISMATCH xx:xx:xx:xx:xx:xx
    //Example: AP-STA-POSSIBLE-PSK-MISMATCH xx:xx:xx:xx:xx:xx status=x reason=x
    //Example: CTRL-EVENT-SAE-UNKNOWN-PASSWORD-IDENTIFIER xx:xx:xx:xx:xx:xx
    char buf[SWL_MAC_CHAR_LEN] = {0};
    swl_macBin_t bStationMac;

    memcpy(buf, params, SWL_MAC_CHAR_LEN - 1);
    bool ret = swl_mac_charToBin(&bStationMac, (swl_macChar_t*) buf);
    ASSERT_TRUE(ret, , ME, "fail to convert mac address to binary");
    CALL_INTF(pInterface, fApStationAssocFailureCb, &bStationMac);
}

static void s_wdsStationInterfaceRemoved(wld_wpaCtrlInterface_t* pInterface, char* event, char* params) {
    //Example: WDS-STA-INTERFACE-REMOVED ifname=wlan0.sta1 sta_addr=d6:ee:57:0d:f2:aa
    char wdsIntfName[128] = {0};
    char staAddr[SWL_MAC_CHAR_LEN] = {0};
    swl_macBin_t bStationMac;

    ASSERTI_TRUE(wld_wpaCtrl_getValueStr(params, "ifname", wdsIntfName, sizeof(wdsIntfName)) > 0, , ME, "WDS interface not found");
    ASSERTI_TRUE(wld_wpaCtrl_getValueStr(params, "sta_addr", staAddr, sizeof(staAddr)) > 0, , ME, "STA addr not found");
    ASSERTI_TRUE(swl_mac_charToBin(&bStationMac, (swl_macChar_t*) staAddr), , ME, "fail to convert mac address to binary");
    SAH_TRACEZ_INFO(ME, "%s %s sta_addr='%s' wds_intf='%s'", pInterface->name, event, staAddr, wdsIntfName);
    CALL_INTF(pInterface, fWdsIfaceRemovedCb, wdsIntfName, &bStationMac);
}

static void s_wdsStationInterfaceAdded(wld_wpaCtrlInterface_t* pInterface, char* event, char* params) {
    //Example: WDS-STA-INTERFACE-ADDED ifname=wlan0.sta1 sta_addr=d6:ee:57:0d:f2:aa
    char wdsIntfName[128] = {0};
    char staAddr[SWL_MAC_CHAR_LEN] = {0};
    swl_macBin_t bStationMac;

    ASSERTI_TRUE(wld_wpaCtrl_getValueStr(params, "ifname", wdsIntfName, sizeof(wdsIntfName)) > 0, , ME, "WDS interface not found");
    ASSERTI_TRUE(wld_wpaCtrl_getValueStr(params, "sta_addr", staAddr, sizeof(staAddr)) > 0, , ME, "STA addr not found");
    ASSERTI_TRUE(swl_mac_charToBin(&bStationMac, (swl_macChar_t*) staAddr), , ME, "fail to convert mac address to binary");
    SAH_TRACEZ_INFO(ME, "%s %s sta_addr='%s' wds_intf='%s'", pInterface->name, event, staAddr, wdsIntfName);
    CALL_INTF(pInterface, fWdsIfaceAddedCb, wdsIntfName, &bStationMac);
}

static void s_btmResponse(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    // Example: BSS-TM-RESP <sta_mac_adr> dialog_token=x status_code=x bss_termination_delay=x target_bssid=<bsssid_mac_adr>
    char buf[SWL_MAC_CHAR_LEN] = {0};
    memcpy(buf, params, SWL_MAC_CHAR_LEN - 1);

    swl_macChar_t mac;
    swl_mac_charToStandard(&mac, buf);

    uint8_t replyCode = wld_wpaCtrl_getValueInt(params, "status_code");

    swl_macChar_t targetBssid = g_swl_macChar_null;
    wld_wpaCtrl_getValueStr(params, "target_bssid", targetBssid.cMac, sizeof(targetBssid));

    SAH_TRACEZ_INFO(ME, "%s BSS-TM-RESP %s status_code=%d target_bssid =%s", pInterface->name, mac.cMac, replyCode, targetBssid.cMac);

    CALL_INTF(pInterface, fBtmReplyCb, &mac, replyCode, &targetBssid);
}

SWL_TABLE(sChWidthMaps,
          ARR(uint32_t chWidthId; char* chWidthDesc; swl_bandwidth_e swlBw; ),
          ARR(swl_type_uint32, swl_type_charPtr, swl_type_uint32, ),
          ARR({0, "20 MHz (no HT)", SWL_BW_20MHZ}, //CHAN_WIDTH_20_NOHT
              {1, "20 MHz", SWL_BW_20MHZ},         //CHAN_WIDTH_20
              {2, "40 MHz", SWL_BW_40MHZ},         //CHAN_WIDTH_40
              {3, "80 MHz", SWL_BW_80MHZ},         //CHAN_WIDTH_80
              {4, "80+80 MHz", SWL_BW_160MHZ},     //CHAN_WIDTH_80P80
              {5, "160 MHz", SWL_BW_160MHZ},       //CHAN_WIDTH_160
              {10, "320 MHz", SWL_BW_320MHZ},      //CHAN_WIDTH_320
              ));
static swl_rc_ne s_freqParamToChanSpec(char* params, const char* key, swl_chanspec_t* pChanSpec) {
    ASSERTS_STR(params, SWL_RC_INVALID_PARAM, ME, "Empty");
    ASSERTS_NOT_NULL(pChanSpec, SWL_RC_INVALID_PARAM, ME, "NULL");
    uint32_t ctrlFreq = 0;
    ASSERTS_TRUE(wld_wpaCtrl_getValueIntExt(params, key, (int32_t*) &ctrlFreq), SWL_RC_ERROR,
                 ME, "Missing %s param", key);
    swl_chanspec_t chanSpec;
    swl_rc_ne rc = swl_chanspec_channelFromMHz(&chanSpec, ctrlFreq);
    ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "fail to get chanspec for freq(%d)", ctrlFreq);
    pChanSpec->channel = chanSpec.channel;
    pChanSpec->band = chanSpec.band;
    return SWL_RC_OK;
}

swl_rc_ne wld_wpaCtrl_freqParamToChanSpec(char* params, const char* key, swl_chanspec_t* pChanSpec) {
    return s_freqParamToChanSpec(params, key, pChanSpec);
}

static swl_rc_ne s_chWidthDescToChanSpec(char* params, const char* key, swl_chanspec_t* pChanSpec) {
    ASSERTS_STR(params, SWL_RC_INVALID_PARAM, ME, "Empty");
    ASSERTS_NOT_NULL(pChanSpec, SWL_RC_INVALID_PARAM, ME, "NULL");
    char chWidthStr[64] = {0};
    ASSERTS_TRUE(wld_wpaCtrl_getValueStr(params, key, chWidthStr, sizeof(chWidthStr)) > 0, SWL_RC_ERROR,
                 ME, "Missing %s param", key);
    swl_bandwidth_e* pBwEnu = (swl_bandwidth_e*) swl_table_getMatchingValue(&sChWidthMaps, 2, 1, chWidthStr);
    ASSERTS_NOT_NULL(pBwEnu, SWL_RC_ERROR, ME, "unknown channel width desc (%s)", chWidthStr);
    pChanSpec->bandwidth = *pBwEnu;

    // Central frequency is needed to differentiate between 320MHz-1 and 320MHz-2
    ASSERTS_EQUALS(pChanSpec->bandwidth, SWL_BW_320MHZ, SWL_RC_OK, ME, "Not using 320MHz");
    uint32_t centerFreq = 0;
    ASSERTS_TRUE(wld_wpaCtrl_getValueIntExt(params, "cf1", (int32_t*) &centerFreq), SWL_RC_ERROR,
                 ME, "Missing cf1 param");
    swl_chanspec_t chanSpecCenter;
    swl_rc_ne rc = swl_chanspec_fromMHz(&chanSpecCenter, centerFreq);
    ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "fail to get chanspec for freq(%d)", centerFreq);
    pChanSpec->extensionHigh = chanSpecCenter.extensionHigh;
    return SWL_RC_OK;
}

swl_rc_ne wld_wpaCtrl_chWidthDescToChanSpec(char* params, const char* key, swl_chanspec_t* pChanSpec) {
    return s_chWidthDescToChanSpec(params, key, pChanSpec);
}

static swl_rc_ne s_chWidthIdToChanSpec(char* params, const char* key, swl_chanspec_t* pChanSpec) {
    ASSERTS_STR(params, SWL_RC_INVALID_PARAM, ME, "Empty");
    ASSERTS_NOT_NULL(pChanSpec, SWL_RC_INVALID_PARAM, ME, "NULL");
    uint32_t chWId = 0;
    ASSERTS_TRUE(wld_wpaCtrl_getValueIntExt(params, key, (int32_t*) &chWId), SWL_RC_ERROR,
                 ME, "Missing %s param", key);
    swl_bandwidth_e* pBwEnu = (swl_bandwidth_e*) swl_table_getMatchingValue(&sChWidthMaps, 2, 0, &chWId);
    ASSERTS_NOT_NULL(pBwEnu, SWL_RC_ERROR, ME, "unknown channel width id (%d)", chWId);
    pChanSpec->bandwidth = *pBwEnu;
    return SWL_RC_OK;
}

swl_rc_ne wld_wpaCtrl_chWidthIdToChanSpec(char* params, const char* key, swl_chanspec_t* pChanSpec) {
    return s_chWidthIdToChanSpec(params, key, pChanSpec);
}

static void s_chanSwitchStartedEvtCb(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    // Example: CTRL-EVENT-STARTED-CHANNEL-SWITCH freq=5260 ht_enabled=1 ch_offset=1 ch_width=80 MHz cf1=5290 cf2=0 dfs=1
    swl_chanspec_t chanSpec = SWL_CHANSPEC_EMPTY;
    ASSERT_FALSE(s_freqParamToChanSpec(params, "freq", &chanSpec) < SWL_RC_OK, , ME, "fail to get freq");
    s_chWidthDescToChanSpec(params, "ch_width", &chanSpec);
    SAH_TRACEZ_INFO(ME, "%s: channel=%d width=%d band=%d", pInterface->name, chanSpec.channel,
                    chanSpec.bandwidth, chanSpec.band);

    CALL_MGR_I(pInterface, fChanSwitchStartedCb, &chanSpec);
}

static void s_chanSwitchEvtCb(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    // Example: CTRL-EVENT-CHANNEL-SWITCH freq=2427 ht_enabled=0 ch_offset=0 ch_width=20 MHz (no HT) cf1=2427 cf2=0 dfs=0
    swl_chanspec_t chanSpec = SWL_CHANSPEC_EMPTY;
    ASSERT_FALSE(s_freqParamToChanSpec(params, "freq", &chanSpec) < SWL_RC_OK, , ME, "fail to get freq");
    s_chWidthDescToChanSpec(params, "ch_width", &chanSpec);

    SAH_TRACEZ_INFO(ME, "%s: channel=%d width=%d band=%d", pInterface->name, chanSpec.channel,
                    chanSpec.bandwidth, chanSpec.band);

    CALL_MGR_I(pInterface, fChanSwitchCb, &chanSpec);
}

static void s_apCsaFinishedEvtCb(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    // Example: AP-CSA-FINISHED freq=2427 dfs=0
    swl_chanspec_t chanSpec = SWL_CHANSPEC_EMPTY;
    ASSERT_FALSE(s_freqParamToChanSpec(params, "freq", &chanSpec) < SWL_RC_OK, , ME, "fail to get freq");

    SAH_TRACEZ_INFO(ME, "%s: channel=%d", pInterface->name, chanSpec.channel);

    CALL_MGR_I(pInterface, fApCsaFinishedCb, &chanSpec);
}

static void s_apEnabledEvt(wld_wpaCtrlInterface_t* pInterface, char* event, char* params _UNUSED) {
    // Example: AP-ENABLED
    // This event notifies that hapd main interface has setup completed
    SAH_TRACEZ_INFO(ME, "%s: %s", pInterface->name, event);
    if(pInterface == wld_wpaCtrlMngr_getDefaultInterface(pInterface->pMgr)) {
        CALL_MGR_I_NA(pInterface, fMainApSetupCompletedCb);
    }
    CALL_INTF_NA(pInterface, fApEnabledCb);
}

static void s_apDisabledEvt(wld_wpaCtrlInterface_t* pInterface, char* event, char* params _UNUSED) {
    // Example: AP-DISABLED
    // This event notifies that hapd main interface is disabled, or that secondary bss is removed from conf
    SAH_TRACEZ_INFO(ME, "%s: %s", pInterface->name, event);
    CALL_INTF_NA(pInterface, fApDisabledCb);
    if(pInterface == wld_wpaCtrlMngr_getDefaultInterface(pInterface->pMgr)) {
        CALL_MGR_I_NA(pInterface, fMainApDisabledCb);
    }
}

static void s_apIfaceEnabledEvt(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params _UNUSED) {
    // Example: INTERFACE-ENABLED
    // This event notifies that main interface is going up (netlink status)
    SAH_TRACEZ_INFO(ME, "%s: %s", pInterface->name, event);
    CALL_INTF_NA(pInterface, fApEnabledCb);
}

static void s_apIfaceDisabledEvt(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params _UNUSED) {
    // Example: INTERFACE-DISABLED
    // This event notifies that main interface is going down (netlink status)
    SAH_TRACEZ_INFO(ME, "%s: %s", pInterface->name, event);
    CALL_INTF_NA(pInterface, fApDisabledCb);
}

static void s_ifaceTerminatingEvt(wld_wpaCtrlInterface_t* pInterface, char* event, char* params _UNUSED) {
    // Example: CTRL-EVENT-TERMINATING
    if(!pInterface->pMgr || !pInterface->pMgr->pSecDmn) {
        return;
    }
    SAH_TRACEZ_WARNING(ME, "%s: %s", pInterface->name, event);
    bool isMgrTerm = (pInterface->isReady && (wld_wpaCtrlMngr_countReadyInterfaces(pInterface->pMgr) == 1));
    pInterface->isReady = false;
    wld_wpaCtrlInterface_reset(pInterface);
    if(isMgrTerm) {
        CALL_MGR_I(pInterface, fMngrReadyCb, false);
    }
}

static void s_radDfsCacStartedEvt(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    // Example: DFS-CAC-START freq=5500 chan=100 sec_chan=1, width=1, seg0=106, seg1=0, cac_time=60s
    swl_chanspec_t chanSpec = SWL_CHANSPEC_EMPTY;
    ASSERT_FALSE(s_freqParamToChanSpec(params, "freq", &chanSpec) < SWL_RC_OK, , ME, "fail to get freq");
    s_chWidthIdToChanSpec(params, "width", &chanSpec);
    uint32_t cac_time = wld_wpaCtrl_getValueInt(params, "cac_time");
    SAH_TRACEZ_INFO(ME, "%s: cac started on channel=%d for %d sec",
                    pInterface->name, chanSpec.channel, cac_time);
    CALL_MGR_I(pInterface, fDfsCacStartedCb, &chanSpec, cac_time);
}
static void s_radDfsCacDoneEvt(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    // Example: DFS-CAC-COMPLETED success=1 freq=5500 ht_enabled=0 chan_offset=0 chan_width=3 cf1=5530 cf2=0
    swl_chanspec_t chanSpec = SWL_CHANSPEC_EMPTY;
    ASSERT_FALSE(s_freqParamToChanSpec(params, "freq", &chanSpec) < SWL_RC_OK, , ME, "fail to get freq");
    s_chWidthIdToChanSpec(params, "chan_width", &chanSpec);
    bool success = wld_wpaCtrl_getValueInt(params, "success");
    SAH_TRACEZ_INFO(ME, "%s: CAC done channel=%d width=%d band=%d result=%d",
                    pInterface->name, chanSpec.channel, chanSpec.bandwidth, chanSpec.band, success);
    CALL_MGR_I(pInterface, fDfsCacDoneCb, &chanSpec, success);
}
static void s_radDfsCacExpiredEvt(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    // Example: DFS-PRE-CAC-EXPIRED freq=5500 ht_enabled=0 chan_offset=0 chan_width=0 cf1=5500 cf2=0
    swl_chanspec_t chanSpec = SWL_CHANSPEC_EMPTY;
    ASSERT_FALSE(s_freqParamToChanSpec(params, "freq", &chanSpec) < SWL_RC_OK, , ME, "fail to get freq");
    s_chWidthIdToChanSpec(params, "chan_width", &chanSpec);
    SAH_TRACEZ_INFO(ME, "%s: previous CAC expired on channel=%d width=%d band=%d",
                    pInterface->name, chanSpec.channel, chanSpec.bandwidth, chanSpec.band);
    CALL_MGR_I(pInterface, fDfsCacExpiredCb, &chanSpec);
}
static void s_radDfsRadarDetectedEvt(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    // Example: DFS-RADAR-DETECTED freq=5540 ht_enabled=0 chan_offset=0 chan_width=3 cf1=5530 cf2=0
    swl_chanspec_t chanSpec = SWL_CHANSPEC_EMPTY;
    ASSERT_FALSE(s_freqParamToChanSpec(params, "freq", &chanSpec) < SWL_RC_OK, , ME, "fail to get freq");
    s_chWidthIdToChanSpec(params, "chan_width", &chanSpec);
    SAH_TRACEZ_INFO(ME, "%s: DFS radar detected on channel=%d width=%d band=%d",
                    pInterface->name, chanSpec.channel, chanSpec.bandwidth, chanSpec.band);
    CALL_MGR_I(pInterface, fDfsRadarDetectedCb, &chanSpec);
}
static void s_radDfsNopFinishedEvt(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    // Example: DFS-NOP-FINISHED freq=5500 ht_enabled=0 chan_offset=0 chan_width=0 cf1=5500 cf2=0
    swl_chanspec_t chanSpec = SWL_CHANSPEC_EMPTY;
    ASSERT_FALSE(s_freqParamToChanSpec(params, "freq", &chanSpec) < SWL_RC_OK, , ME, "fail to get freq");
    s_chWidthIdToChanSpec(params, "chan_width", &chanSpec);
    SAH_TRACEZ_INFO(ME, "%s: DFS Non-Occupancy Period is over on channel=%d width=%d",
                    pInterface->name, chanSpec.channel, chanSpec.bandwidth);
    CALL_MGR_I(pInterface, fDfsNopFinishedCb, &chanSpec);
}

static void s_radDfsNewChannelEvt(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    // Example: DFS-NEW-CHANNEL freq=5180 chan=36 sec_chan=1
    swl_chanspec_t chanSpec = SWL_CHANSPEC_EMPTY;
    ASSERT_FALSE(s_freqParamToChanSpec(params, "freq", &chanSpec) < SWL_RC_OK, , ME, "fail to get freq");
    SAH_TRACEZ_INFO(ME, "%s: DFS New channel switch expected to channel=%d, after radar detection",
                    pInterface->name, chanSpec.channel);
    CALL_MGR_I(pInterface, fDfsNewChannelCb, &chanSpec);
}

static void s_mgtFrameEvt(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    // Example: <3>AP-MGMT-FRAME-RECEIVED buf=b0003c0000000000001012d4159a59e7000000000010a082000001000000
    char data[swl_str_len(params) + 1];
    memset(data, 0, sizeof(data));
    size_t len = wld_wpaCtrl_getValueStr(params, "buf", data, sizeof(data));
    ASSERT_TRUE(len > 1, , ME, "%s: frame buf field empty", pInterface->name);
    size_t binLen = (len / 2) + (len % 2);
    swl_bit8_t binData[binLen];
    bool success = swl_hex_toBytesSep(binData, sizeof(binData), data, len, 0, &binLen);
    ASSERT_TRUE(success, , ME, "%s: frame HEX CONVERT FAIL", pInterface->name);
    swl_80211_mgmtFrame_t* mgmtFrame = swl_80211_getMgmtFrame(binData, binLen);
    ASSERT_NOT_NULL(mgmtFrame, , ME, "%s: invalid mgmt frame (length:%zu)", pInterface->name, binLen);
    SAH_TRACEZ_INFO(ME, "%s MGMT-FRAME stype=0x%x", pInterface->name, mgmtFrame->fc.subType);
    CALL_INTF(pInterface, fMgtFrameReceivedCb, mgmtFrame, binLen, data);
}

static bool s_skipPfx(const char* pfx, char** msg) {
    if(swl_str_startsWith(*msg, pfx)) {
        *msg += swl_str_len(pfx);
        return true;
    }
    return false;
}

static int32_t s_skipPfxList(const char* pfxList[], size_t pfxListLen, char** msg) {
    for(uint32_t i = 0; i < pfxListLen; i++) {
        if(s_skipPfx(pfxList[i], msg)) {
            return i;
        }
    }
    return -1;
}

static void s_stationAssociated(wld_wpaCtrlInterface_t* pInterface, char* event, char* params) {
    //endpoint case: msg format: <3>Associated with 96:83:c4:14:95:11
    //endpoint case: msg format: <3>Associated to a new BSS: BSSID=96:83:c4:14:95:11
    SAH_TRACEZ_INFO(ME, "%s: %s", pInterface->name, event);
    swl_macBin_t bssidBin = SWL_MAC_BIN_NEW();
    swl_macChar_t bssidStr = {{0}};
    const char* msgPfx[] = {"with ", "to a new BSS: BSSID=", };
    s_skipPfxList(msgPfx, SWL_ARRAY_SIZE(msgPfx), &params);
    strncpy(bssidStr.cMac, params, SWL_MAC_CHAR_LEN - 1);
    swl_mac_charToBin(&bssidBin, &bssidStr);
    SAH_TRACEZ_INFO(ME, "%s: Associated with BSSID(%s)",
                    pInterface->name, swl_typeMacBin_toBuf32Ref(&bssidBin).buf);

    CALL_INTF(pInterface, fStationAssociatedCb, &bssidBin, 0);
}

static void s_stationDisconnected(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    // Example: <3>CTRL-EVENT-DISCONNECTED bssid=xx:xx:xx:xx:xx:xx reason=3 locally_generated=1
    char bssid[SWL_MAC_CHAR_LEN] = {0};
    wld_wpaCtrl_getValueStr(params, "bssid", bssid, sizeof(bssid));
    swl_macBin_t bBssidMac;
    swl_mac_charToBin(&bBssidMac, (swl_macChar_t*) bssid);
    uint8_t reasonCode = wld_wpaCtrl_getValueInt(params, "reason");
    CALL_INTF(pInterface, fStationDisconnectedCb, &bBssidMac, reasonCode);
}

static void s_stationConnected(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    // Example: <3>CTRL-EVENT-CONNECTED - Connection to 98:42:65:2d:27:b0 completed [id=0 id_str=]
    swl_macBin_t bBssidMac = SWL_MAC_BIN_NEW();
    const char* msgPfx = "- Connection to ";
    if(swl_str_startsWith(params, msgPfx)) {
        char bssid[SWL_MAC_CHAR_LEN] = {0};
        swl_str_copy(bssid, SWL_MAC_CHAR_LEN, &params[strlen(msgPfx)]);
        SWL_MAC_CHAR_TO_BIN(&bBssidMac, bssid);
    }

    /* multiApProfile is configured and only updated from connection event, when included */
    int32_t multiApProfile = 0;
    int32_t multiApPrimaryVlanId = 0;

    if(wld_wpaCtrl_getValueIntExt(params, "multi_ap_profile", &multiApProfile) &&
       wld_wpaCtrl_getValueIntExt(params, "multi_ap_primary_vlanid", &multiApPrimaryVlanId)) {
        CALL_INTF(pInterface, fStationMultiApInfoCb, multiApProfile, multiApPrimaryVlanId);
    }
    CALL_INTF(pInterface, fStationConnectedCb, &bBssidMac, 0);
}

static void s_stationScanFailed(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    // Example:<3>CTRL-EVENT-SCAN-FAILED ret=-16 retry=1
    int error = wld_wpaCtrl_getValueInt(params, "ret");
    NOTIFY(pInterface, fStationScanFailedCb, error);
}

static void s_stationAuthenticating(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    //endpoint case: msg format: <3>SME: Trying to authenticate with 98:42:65:2d:23:43 (SSID='ssid' freq=5500 MHz)
    swl_chanspec_t chanSpec = SWL_CHANSPEC_EMPTY;
    swl_macBin_t bssidBin = SWL_MAC_BIN_NEW();
    char ssid[SWL_80211_SSID_STR_LEN * 2] = {0};
    s_skipPfx("with ", &params);
    swl_macChar_t bssidStr = {{0}};
    strncpy(bssidStr.cMac, params, SWL_MAC_CHAR_LEN - 1);
    swl_mac_charToBin(&bssidBin, &bssidStr);
    if(wld_wpaCtrl_getValueStr(params, "SSID", ssid, sizeof(ssid)) > 0) {
        swl_str_stripChar(ssid, '\'');
    }
    char freqStr[64] = {0};
    if(wld_wpaCtrl_getValueStr(params, "freq", freqStr, sizeof(freqStr)) > 0) {
        uint32_t freq = 0;
        if(swl_typeUInt32_fromChar(&freq, strtok(freqStr, " "))) {
            swl_chanspec_channelFromMHz(&chanSpec, freq);
        }
    }
    SAH_TRACEZ_INFO(ME, "%s: Authenticating with BSSID(%s) SSID(%s) chspec(%s)",
                    pInterface->name, swl_typeMacBin_toBuf32Ref(&bssidBin).buf, ssid, swl_typeChanspecExt_toBuf32Ref(&chanSpec).buf);
    NOTIFY(pInterface, fStationStartConnCb, ssid, &bssidBin, &chanSpec);
}

static void s_stationAuthenticationFailure(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params _UNUSED) {
    //endpoint case: msg format: <3>SME: Authentication request to the driver failed
    SAH_TRACEZ_INFO(ME, "%s: %s", pInterface->name, event);
    int error = -EBUSY;
    NOTIFY(pInterface, fStationStartConnFailedCb, error);
}

static void s_stationAssociating(wld_wpaCtrlInterface_t* pInterface, char* event, char* params) {
    //endpoint case: msg format: <3>Trying to associate with 98:42:65:2d:23:43 (SSID='ssid' freq=5500 MHz)
    //endpoint case: msg format: <3>Trying to associate with SSID '%s'
    SAH_TRACEZ_INFO(ME, "%s: %s", pInterface->name, event);
    swl_chanspec_t chanSpec = SWL_CHANSPEC_EMPTY;
    swl_macBin_t bssidBin = SWL_MAC_BIN_NEW();
    char ssid[SWL_80211_SSID_STR_LEN * 2] = {0};
    if(s_skipPfx("with SSID ", &params)) {
        swl_str_copy(ssid, sizeof(ssid), params);
        swl_str_stripChar(ssid, '\'');
    } else if(s_skipPfx("with ", &params)) {
        swl_macChar_t bssidStr = {{0}};
        strncpy(bssidStr.cMac, params, SWL_MAC_CHAR_LEN - 1);
        swl_mac_charToBin(&bssidBin, &bssidStr);
        if(wld_wpaCtrl_getValueStr(params, "SSID", ssid, sizeof(ssid)) > 0) {
            swl_str_stripChar(ssid, '\'');
        }
        char freqStr[64] = {0};
        if(wld_wpaCtrl_getValueStr(params, "freq", freqStr, sizeof(freqStr)) > 0) {
            uint32_t freq = 0;
            if(swl_typeUInt32_fromChar(&freq, strtok(freqStr, " "))) {
                swl_chanspec_channelFromMHz(&chanSpec, freq);
            }
        }
    }
    SAH_TRACEZ_INFO(ME, "%s: Associating with BSSID(%s) SSID(%s) chspec(%s) ",
                    pInterface->name, swl_typeMacBin_toBuf32Ref(&bssidBin).buf, ssid, swl_typeChanspec_toBuf32Ref(&chanSpec).buf);
}

static void s_beaconResponseEvt(wld_wpaCtrlInterface_t* pInterface, char* event _UNUSED, char* params) {
    // Example: BEACON-RESP-RX be:e9:46:09:57:8a 1 00 00010000000000000000640002bb00d47bb0bf73f00048dc020001b28741084de2000000640011150017536f66744174486f6d652d314332392d4d53637265656e010882848b962430486c0301010504000300000706455520010d1420010023020f002a010432040c12186030140100000fac040100000fac040100000fac020c000b0501008d0000460532000000002d1aad0117ffffff00000000000000000000000000000000000000003d16010804000000000000000000000000000000000000007f080400000000000040
    // Example: <3>BEACON-RESP-RX 8c:7c:92:68:a8:13 1 00
    char buf[SWL_MAC_CHAR_LEN] = {0};
    swl_macBin_t station;
    memcpy(buf, params, SWL_MAC_CHAR_LEN - 1);
    swl_mac_charToBin(&station, (swl_macChar_t*) buf);
    // skip The measurement token and measurement report mode
    char* data = strstr(params, " ");
    ASSERTS_NOT_NULL(data, , ME, "NULL");
    data = strstr(data + 1, " ");
    ASSERTS_NOT_NULL(data, , ME, "NULL");
    data = strstr(data + 1, " ");
    ASSERTS_NOT_NULL(data, , ME, "NULL");

    wld_wpaCtrl_rrmBeaconRsp_t rrmBeaconResponse;
    memset(&rrmBeaconResponse, 0, sizeof(wld_wpaCtrl_rrmBeaconRsp_t));
    if(*(++data) != '\0') {
        size_t dataLen = swl_str_len(data);
        size_t binLen = (dataLen / 2) + (dataLen % 2);
        swl_bit8_t binData[binLen];
        bool success = swl_hex_toBytes(binData, binLen, data, dataLen);
        ASSERT_TRUE(success, , ME, "HEX CONVERT FAIL");
        ASSERT_TRUE(binLen >= sizeof(rrmBeaconResponse), , ME, "Too short report");
        // !!!! please check copy for big endian targets
        memcpy(&rrmBeaconResponse, binData, sizeof(rrmBeaconResponse));
    }

    CALL_INTF(pInterface, fBeaconResponseCb, &station, &rrmBeaconResponse);
}

SWL_TABLE(sWpaCtrlEvents,
          ARR(char* evtName; void* evtParser; ),
          ARR(swl_type_charPtr, swl_type_voidPtr),
          ARR(
              {"WPS-CANCEL", &s_wpsCancelEvent},
              {"WPS-TIMEOUT", &s_wpsTimeoutEvent},
              {"WPS-REG-SUCCESS", &s_wpsSuccess},
              {"WPS-OVERLAP-DETECTED", &s_wpsOverlapEvent},
              {"WPS-FAIL", &s_wpsFailEvent},
              {"WPS-CRED-RECEIVED", &s_wpsCredReceivedEvent},
              {"WPS-SUCCESS", &s_wpsStationSuccessEvent},
              {"WPS-PBC-ACTIVE", &s_wpsInProgress},
              {"WPS-AP-SETUP-UNLOCKED", &s_wpsSetupUnlockedEvent},
              {"WPS-AP-SETUP-LOCKED", &s_wpsSetupLockedEvent},
              {"AP-STA-CONNECTED", &s_apStationConnected},
              {"EAPOL-4WAY-HS-COMPLETED", &s_stationEapCompleted},
              {"AP-STA-DISCONNECTED", &s_apStationDisconnected},
              {"WDS-STA-INTERFACE-ADDED", &s_wdsStationInterfaceAdded},
              {"WDS-STA-INTERFACE-REMOVED", &s_wdsStationInterfaceRemoved},
              {"AP-STA-POSSIBLE-PSK-MISMATCH", &s_apStationAssocFailure},
              {"CTRL-EVENT-SAE-UNKNOWN-PASSWORD-IDENTIFIER", &s_apStationAssocFailure},
              {"BSS-TM-RESP", &s_btmResponse},
              {"CTRL-EVENT-STARTED-CHANNEL-SWITCH", &s_chanSwitchStartedEvtCb},
              {"CTRL-EVENT-CHANNEL-SWITCH", &s_chanSwitchEvtCb},
              {"AP-CSA-FINISHED", &s_apCsaFinishedEvtCb},
              {"AP-MGMT-FRAME-RECEIVED", &s_mgtFrameEvt},
              {"AP-ENABLED", &s_apEnabledEvt},
              {"AP-DISABLED", &s_apDisabledEvt},
              {"INTERFACE-ENABLED", &s_apIfaceEnabledEvt},
              {"INTERFACE-DISABLED", &s_apIfaceDisabledEvt},
              {"CTRL-EVENT-TERMINATING", &s_ifaceTerminatingEvt},
              {"DFS-CAC-START", &s_radDfsCacStartedEvt},
              {"DFS-CAC-COMPLETED", &s_radDfsCacDoneEvt},
              {"DFS-PRE-CAC-EXPIRED", &s_radDfsCacExpiredEvt},
              {"DFS-RADAR-DETECTED", &s_radDfsRadarDetectedEvt},
              {"DFS-NOP-FINISHED", &s_radDfsNopFinishedEvt},
              {"DFS-NEW-CHANNEL", &s_radDfsNewChannelEvt},
              {"SME: Trying to authenticate", &s_stationAuthenticating},
              {"SME: Authentication request to the driver failed", &s_stationAuthenticationFailure},
              {"Trying to associate", &s_stationAssociating},
              {"Associated", &s_stationAssociated},
              {"CTRL-EVENT-DISCONNECTED", &s_stationDisconnected},
              {"CTRL-EVENT-CONNECTED", &s_stationConnected},
              {"CTRL-EVENT-SCAN-FAILED", &s_stationScanFailed},
              {"BEACON-RESP-RX", &s_beaconResponseEvt},
              ));

static evtParser_f s_getEventParser(char* eventName) {
    evtParser_f* pfEvtHdlr = (evtParser_f*) swl_table_getMatchingValue(&sWpaCtrlEvents, 1, 0, eventName);
    ASSERTS_NOT_NULL(pfEvtHdlr, NULL, ME, "no internal hdlr defined for evt(%s)", eventName);
    return *pfEvtHdlr;
}

/*
 * @brief parse a wpactrl msg and fetch the event name from a provided list
 * then copy the event name (delimited with separator), and the argument list (starting with separator)
 * The event name is potentially prefixed.
 * The expected message format is "[PREFIX]<EVENT_NAME><separator><EVENT_ARGS...>"
 *
 * @param pData wp ctrl message
 * @param prefix optional string prefixing the event name (typically the loglevel id , eg: <3>)
 * @param separator optional string (or char) separating event name and next event arguments
 * @param evtList optional array of event name strings to fetch in priority, before splitting msg with separator
 * @param evtListLen length of optional array of known event name strings
 * @param pEvtName pointer to output string allocated and filled with the event name (to be freed by the caller)
 * @param pEvtArgs pointer to output string allocated and filled with the event arguments (to be freed by the caller)
 *
 * @return -1,  when event name is unknown or not found
 *         index < evtListLen, when event name is found in the provided list
 */
int32_t wld_wpaCtrl_fetchEvent(const char* pData, const char* prefix, const char* sep, char* evtList[], size_t evtListLen, char** pEvtName, char** pEvtArgs) {
    int evtPos = -1;
    if(pEvtName != NULL) {
        W_SWL_FREE(*pEvtName);
    }
    if(pEvtArgs != NULL) {
        W_SWL_FREE(*pEvtArgs);
    }
    ASSERTS_STR(pData, evtPos, ME, "Empty");
    const char* start = (char*) pData;
    ssize_t pos = 0;
    if(!swl_str_isEmpty(prefix)) {
        pos = swl_str_find(start, prefix);
        if(pos < 0) {
            SAH_TRACEZ_ERROR(ME, "prefix (%s) not found in (%s)", prefix, pData);
            start = NULL;
        } else {
            start = &start[pos + swl_str_len(prefix)];
        }
    }
    const char* startArgs = NULL;
    size_t evtNameLen = swl_str_len(start);
    for(uint32_t i = 0; i < evtListLen; i++) {
        if(swl_str_startsWith(start, evtList[i])) {
            evtPos = i;
            break;
        }
    }
    if(!swl_str_isEmpty(sep)) {
        uint32_t begin = (evtPos >= 0) ? swl_str_len(evtList[evtPos]) : 0;
        pos = swl_str_find(&start[begin], sep);
        if((pos >= 0) && ((begin + pos) > 0)) {
            evtNameLen = begin + pos;
            startArgs = &start[evtNameLen + swl_str_len(sep)];
        }
    }
    if(pEvtArgs != NULL) {
        swl_str_copyMalloc(pEvtArgs, startArgs);
    }
    if(pEvtName != NULL) {
        const char* resEvtName = NULL;
        char eventName[evtNameLen + 1];
        if(evtNameLen > 0) {
            swl_str_ncopy(eventName, sizeof(eventName), start, evtNameLen);
            resEvtName = eventName;
        }
        swl_str_copyMalloc(pEvtName, resEvtName);
    }
    ASSERTI_TRUE(evtNameLen > 0, evtPos, ME, "no event name in (%s)", pData);
    return evtPos;
}

/*
 * @brief parse a wpactrl msg and copy the event name, and the argument list
 * The event name is potentially prefixed.
 * The expected message format is "[PREFIX]<EVENT_NAME><separator><EVENT_ARGS...>"
 *
 * @param pData wp ctrl message
 * @param prefix optional string prefixing the event name (typically the loglevel id , eg: <3>)
 * @param separator optional string (or char) separating event name and next event arguments
 * @param pEvtName pointer to output string allocated and filled with the event name (to be freed by the caller)
 * @param pEvtArgs pointer to output string allocated and filled with the event arguments (to be freed by the caller)
 *
 * @return true when event name is found, false otherwise
 */
bool wld_wpaCtrl_parseMsg(const char* pData, const char* prefix, const char* sep, char** pEvtName, char** pEvtArgs) {
    char* evtName = NULL;
    wld_wpaCtrl_fetchEvent(pData, prefix, sep, NULL, 0, &evtName, pEvtArgs);
    bool ret = (evtName != NULL);
    if(pEvtName != NULL) {
        swl_str_copyMalloc(pEvtName, evtName);
    }
    W_SWL_FREE(evtName);
    return ret;
}

static bool s_processCustomEvent(wld_wpaCtrlInterface_t* pInterface, char* msgData) {
    //if custom event handler was registered, then call it
    bool custProc = false;
    /* for VAP/EP events */
    swl_rc_ne rc;
    CALL_IFACE_RET(pInterface, rc, fCustProcEvtMsg, msgData);
    custProc |= (rc == SWL_RC_DONE);
    /* for RAD/Glob events */
    CALL_MGR_I_RET(pInterface, rc, fCustProcEvtMsg, msgData);
    custProc |= (rc == SWL_RC_DONE);
    return custProc;
}

static void s_processStdEvent(wld_wpaCtrlInterface_t* pInterface, char* msgData) {
    char* eventName = NULL;
    char* pParams = NULL;
    size_t nStdEvts = swl_table_getSize(&sWpaCtrlEvents);
    char* evtList[nStdEvts];
    swl_table_columnToArray(evtList, nStdEvts, &sWpaCtrlEvents, 0);
    // All wpa msgs including events are sent with level MSG_INFO (3)
    wld_wpaCtrl_fetchEvent(msgData, WPA_MSG_LEVEL_INFO, " ", evtList, nStdEvts, &eventName, &pParams);
    ASSERTI_NOT_NULL(eventName, , ME, "%s: no standard wpa_ctrl event in msg (%s)", wld_wpaCtrlInterface_getName(pInterface), msgData);
    evtParser_f fEvtParser = s_getEventParser(eventName);
    if(fEvtParser) {
        fEvtParser(pInterface, eventName, pParams);
    } else {
        SAH_TRACEZ_NOTICE(ME, "No parser for evt(%s)", eventName);
    }
    NOTIFY(pInterface, fProcStdEvtMsg, eventName, msgData);
    W_SWL_FREE(eventName);
    W_SWL_FREE(pParams);
}

static void s_processRefEvent(wld_wpaCtrlInterface_t* pInterface, char* msgData) {
    ASSERTS_NOT_NULL(pInterface, , ME, "NULL");
    ASSERTS_STR(msgData, , ME, "Empty msg");
    wld_wpaCtrlMngr_t* pMgr = wld_wpaCtrlInterface_getMgr(pInterface);
    wld_wpaCtrlInterface_t* pRefIface = wld_wpaCtrlMngr_getEventRefIface(pMgr, msgData);
    ASSERTS_NOT_NULL(pRefIface, , ME, "NULL");
    wld_wpaCtrlMngr_t* pRefMgr = wld_wpaCtrlInterface_getMgr(pRefIface);
    ASSERTS_NOT_NULL(pRefMgr, , ME, "NULL");
    char* eventName = NULL;
    wld_wpaCtrl_fetchEvent(msgData, WPA_MSG_LEVEL_INFO, " ", NULL, 0, &eventName, NULL);
    ASSERTS_NOT_NULL(eventName, , ME, "%s: no ref evt in msg (%s)", wld_wpaCtrlInterface_getName(pInterface), msgData);
    if(swl_str_matches(eventName, "AP-ENABLED")) {
        if(!wld_wpaCtrlInterface_isReady(pRefIface)) {
            SAH_TRACEZ_WARNING(ME, "detect msg for disconnected iface(%s), restore connection",
                               wld_wpaCtrlInterface_getName(pRefIface));
            wld_wpaCtrlMngr_resumeConnect(pRefMgr, 0);
        }
    } else if(swl_str_matches(eventName, "AP-DISABLED")) {
        s_processStdEvent(pRefIface, msgData);
    }
    W_SWL_FREE(eventName);
}

/**
 * @brief process msgData received from wpa_ctrl server
 *
 * @param pInterface Pointer to source interface
 * @param msgData the event received
 *
 * @return void
 */
void wld_wpaCtrl_processMsg(wld_wpaCtrlInterface_t* pInterface, char* msgData, size_t len) {
    ASSERTS_NOT_NULL(pInterface, , ME, "NULL");
    ASSERTS_STR(msgData, , ME, "empty");
    ASSERTS_TRUE(len > 0, , ME, "null length");
    SAH_TRACEZ_NOTICE(ME, "%s: receive event len: %zu (%s)", wld_wpaCtrlInterface_getName(pInterface), len, msgData);

    // 1) prepare custom msg for processing: target interface, msg content
    char* newIfName = NULL;
    char* newMsgData = NULL;
    size_t newLen = 0;
    swl_rc_ne rc;
    CALL_IFACE_RET(pInterface, rc, fPreProcEvtMsg, msgData, len, &newIfName, &newMsgData, &newLen);
    if(rc == SWL_RC_DONE) {
        if(newIfName != NULL) {
            wld_wpaCtrlInterface_t* pNewInterface = wld_wpaCtrlMngr_getInterfaceByName(pInterface->pMgr, newIfName);
            if(pNewInterface != NULL) {
                SAH_TRACEZ_NOTICE(ME, "%s: redirected event to iface %s", pInterface->name, pNewInterface->name);
                pInterface = pNewInterface;
            }
        }
        if((newMsgData != NULL) && (newLen > 0)) {
            msgData = newMsgData;
            len = newLen;
            SAH_TRACEZ_NOTICE(ME, "%s: modified event len: %zu (%s)", pInterface->name, len, msgData);
        }
    }
    W_SWL_FREE(newIfName);

    // 1.1) redirect msg to referenced iface in global socket event
    s_processRefEvent(pInterface, msgData);

    // 2) try to process msg as custom, then as standard
    if(!s_processCustomEvent(pInterface, msgData)) {
        s_processStdEvent(pInterface, msgData);
    }

    // 3) if post-proc event handler was registered, then call it
    NOTIFY(pInterface, fProcEvtMsg, msgData);

    W_SWL_FREE(newMsgData);
}
