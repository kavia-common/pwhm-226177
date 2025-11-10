/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2024 SoftAtHome
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
#include <libgen.h>
#include <unistd.h>
#include "wld.h"
#include "wld_wpaCtrlConnection_priv.h"
#include <swl/fileOps/swl_fileUtils.h>

#define ME "wpaCtrl"

#define MIN_MSG_LENGTH (4 * 1024)       //4KB
#define DEFAULT_MSG_LENGTH (12 * 1024)  //12KB
#define MAX_MSG_LENGTH (64 * 1024)      //64KB

static size_t sMaxMsgLen = DEFAULT_MSG_LENGTH;

static const char* s_getConnCliPath(wpaCtrlConnection_t* pConn) {
    ASSERTS_NOT_NULL(pConn, "", ME, "NULL");
    return pConn->clientAddr.sun_path;
}

static const char* s_getConnSrvPath(wpaCtrlConnection_t* pConn) {
    ASSERTS_NOT_NULL(pConn, "", ME, "NULL");
    return pConn->serverAddr.sun_path;
}

size_t wld_wpaCtrl_getMaxMsgLen() {
    return sMaxMsgLen;
}

swl_rc_ne wld_wpaCtrl_setMaxMsgLen(size_t msgLen) {
    ASSERT_TRUE(msgLen >= MIN_MSG_LENGTH, SWL_RC_INVALID_PARAM, ME, "%zu lower than limit %d", msgLen, MIN_MSG_LENGTH);
    ASSERT_TRUE(msgLen <= MAX_MSG_LENGTH, SWL_RC_INVALID_PARAM, ME, "%zu greater than limit %d", msgLen, MAX_MSG_LENGTH);
    sMaxMsgLen = msgLen;
    return SWL_RC_OK;
}

const char* wld_wpaCtrlConnection_getConnCliPath(wpaCtrlConnection_t* pConn) {
    return s_getConnCliPath(pConn);
}

const char* wld_wpaCtrlConnection_getConnSrvPath(wpaCtrlConnection_t* pConn) {
    return s_getConnSrvPath(pConn);
}

const char* wld_wpaCtrlConnection_getConnCliDirPath(wpaCtrlConnection_t* pConn _UNUSED) {
    return CTRL_IFACE_CLIENT_DIR;
}

const char* wld_wpaCtrlConnection_getConnSrvDirPath(wpaCtrlConnection_t* pConn) {
    ASSERTS_NOT_NULL(pConn, "", ME, "NULL");
    return pConn->srvDirPath;
}

const char* wld_wpaCtrlConnection_getConnSockName(wpaCtrlConnection_t* pConn) {
    ASSERTS_NOT_NULL(pConn, "", ME, "NULL");
    const char* path = s_getConnSrvPath(pConn);
    char* pSep = strrchr(path, '/');
    return (pSep ? &pSep[1] : path);
}

swl_rc_ne wld_wpaCtrlConnection_close(wpaCtrlConnection_t* pConn) {
    ASSERTS_NOT_NULL(pConn, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_TRUE(pConn->wpaPeer > 0, SWL_RC_OK, ME, "NULL");
    SAH_TRACEZ_INFO(ME, "close connection (%s) to (%s)",
                    s_getConnCliPath(pConn),
                    s_getConnSrvPath(pConn));
    amxo_connection_remove(get_wld_plugin_parser(), pConn->wpaPeer);
    /* clear pending replies before closing the non-blocking socket */
    char dropBuf[128];
    while(recv(pConn->wpaPeer, dropBuf, sizeof(dropBuf), 0) > 0) {
    }
    close(pConn->wpaPeer);
    pConn->wpaPeer = 0;
    unlink(pConn->clientAddr.sun_path);
    return SWL_RC_OK;
}

swl_rc_ne wld_wpaCtrlConnection_cleanup(wpaCtrlConnection_t** ppConn) {
    ASSERTS_NOT_NULL(ppConn, SWL_RC_INVALID_PARAM, ME, "NULL");
    wpaCtrlConnection_t* pConn = *ppConn;
    ASSERTS_NOT_NULL(pConn, SWL_RC_OK, ME, "NULL");
    wld_wpaCtrlConnection_close(pConn);
    SAH_TRACEZ_INFO(ME, "cleanup connection (%s) to (%s)",
                    s_getConnCliPath(pConn),
                    s_getConnSrvPath(pConn));
    W_SWL_FREE(pConn->srvDirPath);
    W_SWL_FREE(*ppConn);
    return SWL_RC_OK;
}

swl_rc_ne wld_wpaCtrlConnection_init(wpaCtrlConnection_t** ppConn, uint32_t connId, const char* serverPath, const char* sockName) {
    ASSERT_NOT_NULL(ppConn, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_STR(serverPath, SWL_RC_INVALID_PARAM, ME, "empty server path");
    ASSERT_STR(sockName, SWL_RC_INVALID_PARAM, ME, "empty sock name");

    wpaCtrlConnection_t* pConn = *ppConn;
    if(pConn == NULL) {
        pConn = calloc(1, sizeof(wpaCtrlConnection_t));
        ASSERT_NOT_NULL(pConn, SWL_RC_ERROR, ME, "%s: fail to alloc wpa_ctrl connection", sockName);
        *ppConn = pConn;
    } else if(pConn->wpaPeer != 0) {
        wld_wpaCtrlConnection_close(pConn);
    }

    swl_str_copyMalloc(&pConn->srvDirPath, serverPath);
    pConn->serverAddr.sun_family = AF_UNIX;
    snprintf(pConn->serverAddr.sun_path, sizeof(pConn->serverAddr.sun_path), "%s/%s", serverPath, sockName);
    pConn->clientAddr.sun_family = AF_UNIX;
    snprintf(pConn->clientAddr.sun_path, sizeof(pConn->clientAddr.sun_path), CTRL_IFACE_CLIENT "%s-%u", sockName, connId);

    SAH_TRACEZ_INFO(ME, "%s: init connection (%s) to (%s)", sockName,
                    s_getConnCliPath(pConn),
                    s_getConnSrvPath(pConn));

    return SWL_RC_OK;
}

swl_rc_ne wld_wpaCtrlConnection_setEvtHandlers(wpaCtrlConnection_t* pConn, void* userData, wld_wpaCtrlConnection_evtHandlers_cb* pEvtHdlrs) {
    ASSERTS_NOT_NULL(pConn, SWL_RC_INVALID_PARAM, ME, "NULL");
    pConn->userData = userData;
    if(pEvtHdlrs != NULL) {
        pConn->evtHdlrs = *pEvtHdlrs;
    } else {
        memset(&pConn->evtHdlrs, 0, sizeof(*pEvtHdlrs));
    }
    return SWL_RC_OK;
}

static void s_readCtrl(int fd, void* priv _UNUSED) {
    SAH_TRACEZ_IN(ME);
    size_t maxMsgLen = wld_wpaCtrl_getMaxMsgLen();
    char msgData[maxMsgLen];
    memset(msgData, 0, sizeof(msgData));
    ssize_t msgDataLen;
    ASSERTS_TRUE(fd > 0, , ME, "fd <= 0");

    msgDataLen = recv(fd, msgData, (sizeof(msgData) - 1), MSG_DONTWAIT);
    ASSERTS_FALSE(msgDataLen <= 0, , ME, "recv() failed (%d:%s)", errno, strerror(errno));
    msgData[msgDataLen] = '\0';
    amxo_connection_t* con = amxo_connection_get(get_wld_plugin_parser(), fd);
    ASSERT_NOT_NULL(con, , ME, "con NULL");
    wpaCtrlConnection_t* pConn = (wpaCtrlConnection_t*) con->priv;
    ASSERT_NOT_NULL(pConn, , ME, "failed to get connection context of fd:%d", fd);
    const char* srvPath = s_getConnSrvPath(pConn);
    SAH_TRACEZ_INFO(ME, "received data(%s) from (%s)", msgData, srvPath);
    ASSERTS_NOT_NULL(pConn, , ME, "NULL");
    SWL_CALL(pConn->evtHdlrs.fReadDataCb, pConn->userData, msgData, msgDataLen);
    SAH_TRACEZ_OUT(ME);
}

swl_rc_ne wld_wpaCtrlConnection_open(wpaCtrlConnection_t* pConn) {
    ASSERT_NOT_NULL(pConn, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_FALSE(pConn->wpaPeer > 0, SWL_RC_OK, ME, "already connected");
    int fd = socket(PF_UNIX, SOCK_DGRAM, 0);
    ASSERT_FALSE(fd < 0, SWL_RC_ERROR, ME, "socket(PF_UNIX, SOCK_DGRAM, 0) failed");
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);

    int ret = bind(fd, (struct sockaddr*) &(pConn->clientAddr), sizeof(struct sockaddr_un));
    if((ret < 0) && (errno == EADDRINUSE)) {
        unlink(pConn->clientAddr.sun_path);
        ret = bind(fd, (struct sockaddr*) &(pConn->clientAddr), sizeof(struct sockaddr_un));
    }
    if((ret < 0) ||
       (connect(fd, (struct sockaddr*) &(pConn->serverAddr), sizeof(struct sockaddr_un)) < 0) ||
       (amxo_connection_add(get_wld_plugin_parser(), fd, s_readCtrl, "readCtrl", AMXO_CUSTOM, pConn) != 0)) {
        SAH_TRACEZ_INFO(ME, "failed to open connection (%s) to (%s) err:%d:%s",
                        s_getConnCliPath(pConn),
                        s_getConnSrvPath(pConn),
                        errno, strerror(errno));
        close(fd);
        wld_wpaCtrlConnection_close(pConn);
        return SWL_RC_ERROR;
    }
    pConn->wpaPeer = fd;

    SAH_TRACEZ_INFO(ME, "open connection (%s) to (%s)",
                    s_getConnCliPath(pConn),
                    s_getConnSrvPath(pConn));
    return SWL_RC_OK;
}

swl_rc_ne wld_wpaCtrlConnection_sendCmd(wpaCtrlConnection_t* pConn, const char* cmd) {
    ASSERTS_NOT_NULL(pConn, SWL_RC_INVALID_PARAM, ME, "NULL");
    int fd = pConn->wpaPeer;
    ASSERTS_TRUE(fd > 0, SWL_RC_INVALID_STATE, ME, "fd <= 0");

    const char* srvPath = s_getConnSrvPath(pConn);
    size_t len = strlen(cmd);
    ssize_t ret = send(fd, cmd, len, 0);
    SAH_TRACEZ_INFO(ME, "send cmd (%s) to (%s)", cmd, srvPath);
    ASSERT_EQUALS(ret, (ssize_t) len, SWL_RC_ERROR, ME, "Failed to send cmd (%s) to (%s): ret(%d), err(%d:%s)",
                  cmd, srvPath,
                  (int32_t) ret, errno, strerror(errno));

    return SWL_RC_OK;
}

swl_rc_ne wld_wpaCtrlConnection_sendCmdSyncedExt(wpaCtrlConnection_t* pConn, const char* cmd, char* reply, size_t replyLen, uint32_t tmOutMSec) {
    SAH_TRACEZ_IN(ME);
    ASSERT_NOT_NULL(reply, SWL_RC_INVALID_PARAM, ME, "empty reply buf");
    ASSERT_TRUE(replyLen > 0, SWL_RC_INVALID_PARAM, ME, "null reply buf len");
    ASSERT_NOT_NULL(pConn, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_STR(cmd, SWL_RC_INVALID_PARAM, ME, "empty cmd");
    const char* sockName = wld_wpaCtrlConnection_getConnSockName(pConn);
    int fd = pConn->wpaPeer;
    ASSERT_TRUE(fd > 0, SWL_RC_INVALID_STATE, ME, "%s: invalid fd for cmd (%s)", sockName, cmd);

    struct timeval tv;
    uint32_t tvMs = tmOutMSec;
    int res;
    fd_set rfds;

    /* clear pending replies to avoid getting answer of timeouted request */
    char dropBuf[replyLen];
    while(recv(fd, dropBuf, replyLen, 0) > 0) {
    }

    swl_rc_ne rc = wld_wpaCtrlConnection_sendCmd(pConn, cmd);
    ASSERT_TRUE(swl_rc_isOk(rc), rc, ME, "%s: fail to send sync cmd(%s)", sockName, cmd);
    if(!tmOutMSec) {
        SAH_TRACEZ_INFO(ME, "%s: cmd(%s) sent, not waiting for reply", sockName, cmd);
        return SWL_RC_CONTINUE;
    }

    do {
        tvMs = SWL_MAX(tvMs, (uint32_t) DFLT_SYNC_CMD_TMOUT_MS);
        tv.tv_sec = (tvMs / 1000);
        tv.tv_usec = ((tvMs % 1000) * 1000);
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        res = select(fd + 1, &rfds, NULL, NULL, &tv);
        tvMs = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
        if((res < 0) && (errno == EINTR)) {
            continue;
        }
        ASSERT_FALSE(res < 0, SWL_RC_NOT_AVAILABLE, ME, "%s: select err(%d:%s)", sockName, errno, strerror(errno));
        ASSERTW_NOT_EQUALS(res, 0, SWL_RC_NOT_AVAILABLE, ME, "%s: cmd(%s) timed out", sockName, cmd);
        ASSERT_NOT_EQUALS(FD_ISSET(fd, &rfds), 0, SWL_RC_NOT_AVAILABLE, ME, "%s: missing reply on fd(%d)", sockName, fd);
        ssize_t nr = recv(fd, reply, replyLen, 0);
        ASSERT_FALSE(nr < 0, SWL_RC_ERROR, ME, "%s: recv err(%d:%s)", sockName, errno, strerror(errno));
        /* ignore reply when looking like a wpactrl event */
        if(((nr > 0) && (reply[0] == '<')) ||
           ((nr > 6) && ( strncmp(reply, "IFNAME=", 7) == 0))) {
            continue;
        }
        nr = SWL_MIN(nr, (ssize_t) replyLen - 1);
        reply[nr] = '\0';
        /* remove systematic carriage return at end of buffer */
        if((nr > 0) && (reply[nr - 1] == '\n')) {
            reply[nr - 1] = '\0';
        }
        break;
    } while(1);

    SAH_TRACEZ_OUT(ME);
    if(swl_str_startsWith(reply, "UNKNOWN COMMAND")) {
        return SWL_RC_NOT_IMPLEMENTED;
    }
    return SWL_RC_OK;
}

swl_rc_ne wld_wpaCtrlConnection_sendCmdSynced(wpaCtrlConnection_t* pConn, const char* cmd, char* reply, size_t replyLen) {
    return wld_wpaCtrlConnection_sendCmdSyncedExt(pConn, cmd, reply, replyLen, DFLT_SYNC_CMD_TMOUT_MS);
}

swl_rc_ne wld_wpaCtrlConnection_sendCmdCheckResponseExt(wpaCtrlConnection_t* pConn, char* cmd, char* expectedResponse, uint32_t tmOutMSec) {
    ASSERT_NOT_NULL(pConn, SWL_RC_INVALID_PARAM, ME, "NULL");
    SAH_TRACEZ_IN(ME);
    size_t maxMsgLen = wld_wpaCtrl_getMaxMsgLen();
    char reply[maxMsgLen];
    memset(reply, 0, sizeof(reply));
    swl_rc_ne rc = wld_wpaCtrlConnection_sendCmdSyncedExt(pConn, cmd, reply, sizeof(reply) - 1, tmOutMSec);
    // send the command
    ASSERTS_TRUE(swl_rc_isOk(rc), rc, ME, "sending cmd %s failed", cmd);
    if(rc != SWL_RC_CONTINUE) {
        // check the response
        ASSERT_TRUE(swl_str_matches(reply, expectedResponse), SWL_RC_ERROR, ME, "cmd(%s) reply(%s): unmatch expect(%s)", cmd, reply, expectedResponse);
    }

    return rc;
}

swl_rc_ne wld_wpaCtrlConnection_sendCmdCheckResponse(wpaCtrlConnection_t* pConn, char* cmd, char* expectedResponse) {
    return wld_wpaCtrlConnection_sendCmdCheckResponseExt(pConn, cmd, expectedResponse, DFLT_SYNC_CMD_TMOUT_MS);
}

bool wld_wpaCtrl_checkSockPath(const char* sockPath) {
    ASSERTS_STR(sockPath, false, ME, "empty");
    return (access(sockPath, F_OK) == 0);
}

swl_rc_ne wld_wpaCtrl_queryToSockExt(const char* serverPath, const char* sockName, const char* cmd, char* reply, size_t replyLen, uint32_t tmOutMSec) {
    swl_str_copy(reply, replyLen, NULL);
    char sockPath[swl_str_len(serverPath) + swl_str_len(sockName) + 2];
    swl_str_copy(sockPath, sizeof(sockPath), serverPath);
    swl_str_cat(sockPath, sizeof(sockPath), "/");
    swl_str_cat(sockPath, sizeof(sockPath), sockName);
    ASSERT_TRUE(wld_wpaCtrl_checkSockPath(sockPath), SWL_RC_INVALID_PARAM, ME, "wrong socket path (%s)", sockPath);
    wpaCtrlConnection_t* pConn = NULL;
    swl_rc_ne rc;
    if(((rc = wld_wpaCtrlConnection_init(&pConn, -1, serverPath, sockName)) >= SWL_RC_OK) &&
       ((rc = wld_wpaCtrlConnection_open(pConn)) >= SWL_RC_OK)) {
        rc = wld_wpaCtrlConnection_sendCmdSyncedExt(pConn, cmd, reply, replyLen, tmOutMSec);
    }
    wld_wpaCtrlConnection_cleanup(&pConn);
    return rc;
}

swl_rc_ne wld_wpaCtrl_queryToSock(const char* serverPath, const char* sockName, const char* cmd, char* reply, size_t replyLen) {
    return wld_wpaCtrl_queryToSockExt(serverPath, sockName, cmd, reply, replyLen, DFLT_SYNC_CMD_TMOUT_MS);
}

/*
 * @brief detect support of keyword strings (command, cfg parameter) fetched in security daemon binary
 *
 * @param mapKwSup in/out mapCharInt32 to save to support result
 * @param exeBinPath executable binary full path
 * @param reqKws array of keyword strings to be fetched
 * @param nReqKws number of elements in keywords array
 * @param optKwPfx optional common prefix string to speed up scanning keywords in binary file
 *
 * @return number of matched keywords, -1 in case of error
 */
int32_t wld_wpaCtrl_detectKeywordsSupp(swl_mapCharInt32_t* mapKwSup, const char* exeBinPath, const char* reqKws[], uint32_t nReqKws, const char* optKwPfx) {
    ASSERT_STR(exeBinPath, -1, ME, "Empty exec bin path");
    ASSERT_TRUE(reqKws && nReqKws, -1, ME, "No requested keywords");
    ASSERT_EQUALS(access(exeBinPath, F_OK), 0, -1, ME, "not found exeBinPath (%s)", exeBinPath);
    uint32_t nMatch = 0;
    char foundKws[1024] = {0};
    if(!swl_str_isEmpty(optKwPfx)) {
        swl_fileUtils_findStrInFile(foundKws, sizeof(foundKws), exeBinPath, optKwPfx, true);
    }
    for(uint32_t i = 0; i < nReqKws; i++) {
        const char* reqKw = reqKws[i];
        if(swl_str_isEmpty(reqKw)) {
            continue;
        }
        if(swl_str_isEmpty(optKwPfx)) {
            memset(foundKws, 0, sizeof(foundKws));
            swl_fileUtils_findStrInFile(foundKws, sizeof(foundKws), exeBinPath, reqKw, true);
        }
        swl_trl_e supp = (swl_strlst_contains(foundKws, ",", reqKw) ? SWL_TRL_TRUE : SWL_TRL_FALSE);
        if(mapKwSup != NULL) {
            swl_mapCharInt32_addOrSet(mapKwSup, (char*) reqKw, supp);
        }
        SAH_TRACEZ_INFO(ME, "%smatch keyword(%s) in file(%s) : pfx(%s),foundKwds(%s)",
                        (!supp ? "un" : ""), reqKw, exeBinPath, (optKwPfx ? : ""), foundKws);
        nMatch += (supp == SWL_TRL_TRUE);
    }
    return nMatch;
}

