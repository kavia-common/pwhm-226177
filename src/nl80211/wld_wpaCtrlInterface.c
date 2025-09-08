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
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include "wld.h"
#include "wld_wpaCtrl_api.h"
#include "wld_wpaCtrlInterface_priv.h"
#include "wld_util.h"

#define ME "wpaCtrl"

typedef enum {
    WPA_CONNECTION_CMD = 0,
    WPA_CONNECTION_EVENT,
    WPA_CONNECTION_MAX
} wld_wpaCtrlConnectionType_e;

/**
 * @brief send command to wap_ctrl server
 *
 * @param pIface :the wpa_ctrl interface to which the command is sent
 * @param cmd :string command to be sent
 *
 * @return true if the command is successfully sent.
 *         false Otherwise (invalid param/state, or partial sending)
 */
bool wld_wpaCtrl_sendCmd(wld_wpaCtrlInterface_t* pIface, const char* cmd) {
    ASSERTS_NOT_NULL(pIface, false, ME, "NULL");
    return swl_rc_isOk(wld_wpaCtrlConnection_sendCmd(pIface->cmdConn, cmd));
}

/**
 * @brief send command to wpa_ctrl server and wait for the reply
 *
 * @param pIface :the wpa_ctrl interface to which the command is sent
 * @param cmd : string command to be sent
 * @param reply : buffer where to store the received answer
 * @param reply_len : max length of reply
 *
 * @return true if the command is answered, false otherwise
 */
bool wld_wpaCtrl_sendCmdSynced(wld_wpaCtrlInterface_t* pIface, const char* cmd, char* reply, size_t reply_len) {
    ASSERTS_NOT_NULL(pIface, false, ME, "NULL");
    return swl_rc_isOk(wld_wpaCtrlConnection_sendCmdSynced(pIface->cmdConn, cmd, reply, reply_len));
}

/**
 * @brief send synchronous command to wpa_ctrl server and check a parameter value in the reply
 *
 * @param pIface :the wpa_ctrl interface to which the command is sent
 * @param cmd : string command to be sent
 * @param key : parameter name to be fetched in the reply (key=value)
 * @param valStr : buffer where to store the parameter value
 * @param valStrSize : max length of parameter value
 *
 * @return true if the command is answered, false otherwise
 */
swl_rc_ne wld_wpaCtrl_getSyncCmdParamVal(wld_wpaCtrlInterface_t* pIface, const char* cmd, const char* key, char* valStr, size_t valStrSize) {
    ASSERT_NOT_NULL(pIface, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_STR(key, SWL_RC_INVALID_PARAM, ME, "No key");
    size_t maxMsgLen = wld_wpaCtrl_getMaxMsgLen();
    char reply[maxMsgLen];
    memset(reply, 0, sizeof(reply));
    bool ret = wld_wpaCtrl_sendCmdSynced(pIface, cmd, reply, sizeof(reply));
    ASSERT_TRUE(ret, SWL_RC_ERROR, ME, "failed to get cmd(%s) reply", cmd);
    int valStrLen = wld_wpaCtrl_getValueStr(reply, key, valStr, valStrSize);
    ASSERT_FALSE(valStrLen <= 0, SWL_RC_ERROR, ME, "%s: not found status field %s", pIface->name, key);
    SAH_TRACEZ_INFO(ME, "%s: %s = (%s)", pIface->name, key, valStr);
    ASSERT_TRUE(valStrLen < (int) valStrSize, SWL_RC_ERROR,
                ME, "%s: buffer too short for field %s (l:%d,s:%zu)", pIface->name, key, valStrLen, valStrSize);
    return SWL_RC_OK;
}

/**
 * @brief send synchronous command to wpa_ctrl server
 * and convert a parameter value in the reply (string in base 10) to int32
 * or return default value
 *
 * @param pIface :the wpa_ctrl interface to which the command is sent
 * @param cmd : string command to be sent
 * @param key : parameter name to be fetched in the reply (key=value)
 * @param pRetVal: pointer to resulting int value
 * @param defVal : default value returned on failure
 *
 * @return converted int32 value or defVal on failure
 */
swl_rc_ne wld_wpaCtrl_getSyncCmdParamValInt32Def(wld_wpaCtrlInterface_t* pIface, const char* cmd, const char* key, int32_t* pRetVal, int32_t defVal) {
    char valStr[32] = {0};
    W_SWL_SETPTR(pRetVal, defVal);
    swl_rc_ne rc = wld_wpaCtrl_getSyncCmdParamVal(pIface, cmd, key, valStr, sizeof(valStr));
    ASSERTS_TRUE(swl_rc_isOk(rc), rc, ME, "Not found");
    ASSERTS_STR(valStr, SWL_RC_NOT_AVAILABLE, ME, "Empty");
    int32_t valInt = defVal;
    rc = wldu_convStrToNum(valStr, &valInt, sizeof(valInt), 10, true);
    ASSERTS_TRUE(swl_rc_isOk(rc), SWL_RC_ERROR, ME, "fail to convert");
    W_SWL_SETPTR(pRetVal, valInt);
    return SWL_RC_OK;
}

/**
 * @brief send synchronous command to wpa_ctrl server and check the received reply
 * within a defined delay
 *
 * @param pIface :the wpa_ctrl interface to which the command is sent
 * @param cmd : string command to be sent
 * @param expectedResponse : string expected reply
 * @param tmOutMSec sync cmd timeout duration in milliseconds
 *
 * @return true when the command is answered as expected, false otherwise
 */
bool wld_wpaCtrl_sendCmdCheckResponseExt(wld_wpaCtrlInterface_t* pIface, char* cmd, char* expectedResponse, uint32_t tmOutMSec) {
    ASSERTS_NOT_NULL(pIface, false, ME, "NULL");
    return swl_rc_isOk(wld_wpaCtrlConnection_sendCmdCheckResponseExt(pIface->cmdConn, cmd, expectedResponse, tmOutMSec));
}

/**
 * @brief send synchronous command to wpa_ctrl server and check the received reply
 * within default delay (1s)
 *
 * @param pIface :the wpa_ctrl interface to which the command is sent
 * @param cmd : string command to be sent
 * @param expectedResponse : string expected reply
 *
 * @return true when the command is answered as expected, false otherwise
 */
bool wld_wpaCtrl_sendCmdCheckResponse(wld_wpaCtrlInterface_t* pIface, char* cmd, char* expectedResponse) {
    ASSERTS_NOT_NULL(pIface, false, ME, "NULL");
    return swl_rc_isOk(wld_wpaCtrlConnection_sendCmdCheckResponse(pIface->cmdConn, cmd, expectedResponse));
}

/**
 * @brief send formatted command to wpa_ctrl server over established or temporary connection
 * and check the received reply
 *
 * @param pIface :the wpa_ctrl interface to which the command is sent
 * @param expectedResponse : string expected reply
 * @param cmdFormat : string command format to be sent
 *
 * @return SWL_RC_OK when the command is answered as expected
 *         SWL_RC_ERROR when the command is rejected
 *         SWL_RC_INVALID_STATE when the wpactrl iface link is not ready
 *         SWL_RC_INVALID_PARAM when the command format is not applicable
 */
swl_rc_ne wld_wpaCtrl_sendCmdFmtCheckResponse(wld_wpaCtrlInterface_t* pIface, char* expectedResponse, const char* cmdFormat, ...) {
    const char* wpaCtrlIfName = wld_wpaCtrlInterface_getName(pIface);
    ASSERTS_TRUE(wld_wpaCtrlInterface_checkConnectionPath(pIface), SWL_RC_INVALID_STATE, ME, "%s: wpactrl link not ready", wpaCtrlIfName);
    ASSERTS_STR(cmdFormat, SWL_RC_INVALID_PARAM, ME, "empty cmd");
    char cmdStr[512] = {0};
    int32_t ret = 0;
    va_list args;
    va_start(args, cmdFormat);
    ret = vsnprintf(cmdStr, sizeof(cmdStr), cmdFormat, args);
    va_end(args);
    ASSERT_FALSE(ret < 0, SWL_RC_INVALID_PARAM, ME, "Fail to format cmd string");
    if(wld_wpaCtrlInterface_isReady(pIface)) {
        return wld_wpaCtrlConnection_sendCmdCheckResponse(pIface->cmdConn, cmdStr, expectedResponse);
    }
    size_t maxMsgLen = wld_wpaCtrl_getMaxMsgLen();
    char reply[maxMsgLen];
    memset(reply, 0, sizeof(reply));
    const char* serverPath = wld_wpaCtrlInterface_getConnectionDirPath(pIface);
    const char* sockName = wld_wpaCtrlInterface_getConnectionSockName(pIface);
    swl_rc_ne rc = wld_wpaCtrl_queryToSock(serverPath, sockName, cmdStr, reply, sizeof(reply));
    ASSERTS_TRUE(swl_rc_isOk(rc), rc, ME, "%s: fail to query (%s) to (%s/%s)",
                 wpaCtrlIfName, cmdStr, serverPath, sockName);
    ASSERT_TRUE(swl_str_matches(reply, expectedResponse), SWL_RC_ERROR, ME, "%s: query (%s) to (%s/%s) reply(%s): unmatch expect(%s)",
                wpaCtrlIfName, cmdStr, serverPath, sockName, reply, expectedResponse);
    return rc;
}

/**
 * @brief close the wpaCtrl interface to wpa_ctrl server
 * This function closes all relative connections (cmd and event)
 *
 * @param wpaCtrlInterface the interface to close
 *
 * @return void
 */
void wld_wpaCtrlInterface_close(wld_wpaCtrlInterface_t* pIface) {
    ASSERTS_NOT_NULL(pIface, , ME, "NULL");
    // Send DETACH before closing connection
    if(wld_wpaCtrlMngr_isConnected(pIface->pMgr) && pIface->isReady && (pIface->eventConn != NULL)) {
        const char* srvPath = wld_wpaCtrlConnection_getConnSrvPath(pIface->eventConn);
        if(!wld_wpaCtrlInterface_checkConnectionPath(pIface)) {
            SAH_TRACEZ_NOTICE(ME, "sock srv (%s) not found", srvPath);
        } else if(!swl_rc_isOk(wld_wpaCtrlConnection_sendCmdCheckResponse(pIface->eventConn, "DETACH", "OK"))) {
            SAH_TRACEZ_ERROR(ME, "detach failed from (%s)", srvPath);
        }
    }

    // Close command connection socket
    wld_wpaCtrlConnection_close(pIface->cmdConn);
    // Close event socket
    wld_wpaCtrlConnection_close(pIface->eventConn);
    pIface->isReady = false;
}

void wld_wpaCtrlInterface_cleanup(wld_wpaCtrlInterface_t** ppIface) {
    ASSERTS_NOT_NULL(ppIface, , ME, "NULL");
    wld_wpaCtrlInterface_t* pIface = *ppIface;
    ASSERTS_NOT_NULL(pIface, , ME, "NULL");
    wld_wpaCtrlInterface_close(pIface);
    wld_wpaCtrlConnection_cleanup(&pIface->cmdConn);
    wld_wpaCtrlConnection_cleanup(&pIface->eventConn);
    wld_wpaCtrlMngr_unregisterInterface(pIface->pMgr, pIface);
    free(pIface->name);
    free(pIface);
    *ppIface = NULL;
}

bool wld_wpaCtrlInterface_setConnectionInfo(wld_wpaCtrlInterface_t* pIface, const char* serverPath, const char* sockName) {
    ASSERT_NOT_NULL(pIface, false, ME, "NULL");
    ASSERT_STR(serverPath, false, ME, "%s: empty server path", pIface->name);
    ASSERT_STR(sockName, false, ME, "%s: empty socket name", pIface->name);
    wld_wpaCtrlInterface_close(pIface);
    pIface->enable = false;
    // init connection for events
    bool ret = swl_rc_isOk(wld_wpaCtrlConnection_init(&(pIface->eventConn), WPA_CONNECTION_EVENT, serverPath, sockName));
    // init connection for synchronous commands
    ret |= swl_rc_isOk(wld_wpaCtrlConnection_init(&(pIface->cmdConn), WPA_CONNECTION_CMD, serverPath, sockName));
    ASSERT_TRUE(ret, ret, ME, "%s: fail to init interface connections (%s/%s)", pIface->name, serverPath, sockName);

    wld_wpaCtrlConnection_evtHandlers_cb evtHdlrs = {
        .fReadDataCb = (wld_wpaCtrlConnection_readDataCb_f) wld_wpaCtrl_processMsg,
    };
    wld_wpaCtrlConnection_setEvtHandlers(pIface->eventConn, pIface, &evtHdlrs);
    wld_wpaCtrlConnection_setEvtHandlers(pIface->cmdConn, pIface, &evtHdlrs);

    return ret;
}

bool wld_wpaCtrlInterface_initWithSockName(wld_wpaCtrlInterface_t** ppIface, char* interfaceName, const char* serverPath, const char* sockName) {
    ASSERTS_NOT_NULL(ppIface, false, ME, "NULL");
    ASSERT_STR(interfaceName, false, ME, "empty iface name");
    ASSERT_STR(serverPath, false, ME, "%s: empty server path", interfaceName);
    ASSERT_STR(sockName, false, ME, "%s: empty socket name", interfaceName);

    wld_wpaCtrlInterface_t* pIface = *ppIface;
    if(pIface == NULL) {
        pIface = calloc(1, sizeof(wld_wpaCtrlInterface_t));
        ASSERT_NOT_NULL(pIface, false, ME, "%s: fail to allocate context", interfaceName);
        *ppIface = pIface;
    }
    swl_str_copyMalloc(&pIface->name, interfaceName);

    bool ret = wld_wpaCtrlInterface_setConnectionInfo(pIface, serverPath, sockName);
    ASSERT_TRUE(ret, ret, ME, "fail to init interface connections");

    return ret;
}

bool wld_wpaCtrlInterface_init(wld_wpaCtrlInterface_t** ppIface, char* interfaceName, char* serverPath) {
    return wld_wpaCtrlInterface_initWithSockName(ppIface, interfaceName, serverPath, interfaceName);
}

bool wld_wpaCtrlInterface_setEvtHandlers(wld_wpaCtrlInterface_t* pIface, void* userdata, wld_wpaCtrl_evtHandlers_cb* pHandlers) {
    ASSERTS_NOT_NULL(pIface, false, ME, "NULL");
    pIface->userData = userdata;
    if(pHandlers) {
        pIface->handlers = *pHandlers;
    } else {
        memset(&pIface->handlers, 0, sizeof(pIface->handlers));
    }
    return true;
}

bool wld_wpaCtrlInterface_getEvtHandlers(wld_wpaCtrlInterface_t* pIface, void** userdata, wld_wpaCtrl_evtHandlers_cb* pHandlers) {
    ASSERT_NOT_NULL(pIface, false, ME, "NULL");
    if(userdata != NULL) {
        *userdata = pIface->userData;
    }
    if(pHandlers != NULL) {
        *pHandlers = pIface->handlers;
    }
    return true;
}

bool wld_wpaCtrlInterface_ping(const wld_wpaCtrlInterface_t* pIface) {
    return ((pIface != NULL) &&
            (swl_rc_isOk(wld_wpaCtrlConnection_sendCmdCheckResponse(pIface->cmdConn, "PING", "PONG"))));
}

bool wld_wpaCtrlInterface_isReady(const wld_wpaCtrlInterface_t* pIface) {
    ASSERTS_NOT_NULL(pIface, false, ME, "NULL");
    return pIface->isReady;
}

const char* wld_wpaCtrlInterface_getPath(const wld_wpaCtrlInterface_t* pIface) {
    ASSERTS_NOT_NULL(pIface, "", ME, "NULL");
    return wld_wpaCtrlConnection_getConnSrvPath(pIface->cmdConn);
}

const char* wld_wpaCtrlInterface_getName(const wld_wpaCtrlInterface_t* pIface) {
    ASSERTS_NOT_NULL(pIface, "", ME, "NULL");
    return pIface->name;
}

wld_wpaCtrlMngr_t* wld_wpaCtrlInterface_getMgr(const wld_wpaCtrlInterface_t* pIface) {
    ASSERTS_NOT_NULL(pIface, NULL, ME, "NULL");
    return pIface->pMgr;
}

void wld_wpaCtrlInterface_setEnable(wld_wpaCtrlInterface_t* pIface, bool enable) {
    ASSERTS_NOT_NULL(pIface, , ME, "NULL");
    pIface->enable = enable;
}

bool wld_wpaCtrlInterface_isEnabled(const wld_wpaCtrlInterface_t* pIface) {
    ASSERTS_NOT_NULL(pIface, false, ME, "NULL");
    return pIface->enable;
}

const char* wld_wpaCtrlInterface_getConnectionDirPath(const wld_wpaCtrlInterface_t* pIface) {
    ASSERTS_NOT_NULL(pIface, "", ME, "NULL");
    return wld_wpaCtrlConnection_getConnSrvDirPath(pIface->cmdConn);
}

bool wld_wpaCtrlInterface_checkConnectionPath(const wld_wpaCtrlInterface_t* pIface) {
    const char* path = wld_wpaCtrlInterface_getPath(pIface);
    return wld_wpaCtrl_checkSockPath(path);
}

const char* wld_wpaCtrlInterface_getConnectionSockName(const wld_wpaCtrlInterface_t* pIface) {
    ASSERTS_NOT_NULL(pIface, "", ME, "NULL");
    return wld_wpaCtrlConnection_getConnSockName(pIface->cmdConn);
}

/**
 * @brief open the wpaCtrl interface of wpa_ctrl server
 * The first connection is for sending commands and the second one is for receiving unsolicited messages
 *
 * @param wpaCtrlInterface pointer to interface context (to be allocated and established)
 * @param interfaceName interface name
 * @param userdata the data used by the callback
 *
 * @return true on success. Otherwise, error.
 */
bool wld_wpaCtrlInterface_open(wld_wpaCtrlInterface_t* pIface) {
    ASSERTS_NOT_NULL(pIface, false, ME, "NULL");
    ASSERTW_TRUE(pIface->enable, false, ME, "%s: skip disabled interface", pIface->name);
    pIface->isReady = false;

    /*
     * create a socket for event
     * and send attach command to wpa_ctrl server to register for unsolicited msg
     */
    if((!swl_rc_isOk(wld_wpaCtrlConnection_open(pIface->eventConn))) ||
       (!swl_rc_isOk(wld_wpaCtrlConnection_sendCmdCheckResponse(pIface->eventConn, "ATTACH", "OK")))) {
        SAH_TRACEZ_ERROR(ME, "%s: fail to establish event connection", pIface->name);
        return false;
    }

    /*
     * create a socket for cmd
     */
    if(!swl_rc_isOk(wld_wpaCtrlConnection_open(pIface->cmdConn))) {
        SAH_TRACEZ_ERROR(ME, "%s: fail to establish cmd connection", pIface->name);
        return false;
    }

    SAH_TRACEZ_INFO(ME, "%s: Successfully connected to %s", pIface->name, wld_wpaCtrlInterface_getPath(pIface));
    pIface->isReady = true;

    return true;
}
