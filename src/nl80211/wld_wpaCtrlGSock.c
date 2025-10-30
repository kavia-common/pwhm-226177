/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2025 SoftAtHome
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
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>

#include "swl/swl_common.h"
#include "swl/swl_string.h"
#include "wld_wpaCtrlMngr_priv.h"
#include "wld_wpaCtrlGSock_priv.h"
#include "wld_secDmn.h"
#include "wld_secDmnGrp.h"

#define ME "wpaCtrl"

#define GSOCK_PFX "global."
#define GSOCK_FIRST_DELAY_MS 100
#define GSOCK_RETRY_DELAY_MS 100

static amxc_llist_t sGlSkList = {NULL, NULL};

static wld_wpaCtrlGSock_t* s_fetchGSockByData(wld_wpaCtrlGSock_t* pGlSk) {
    ASSERTS_NOT_NULL(pGlSk, NULL, ME, "Null Data");
    amxc_llist_for_each(it, &sGlSkList) {
        wld_wpaCtrlGSock_t* pCtx = amxc_container_of(it, wld_wpaCtrlGSock_t, it);
        if(pCtx == pGlSk) {
            return pCtx;
        }
    }
    SAH_TRACEZ_INFO(ME, "not found gSock ctx %p", pGlSk);
    return NULL;
}

static wld_wpaCtrlGSock_t* s_fetchGSockByName(const char* name) {
    ASSERTS_TRUE(swl_str_startsWith(name, GSOCK_PFX), NULL, ME, "unmatch prefix in gock name (%s)", name ? : "");
    amxc_llist_for_each(it, &sGlSkList) {
        wld_wpaCtrlGSock_t* pCtx = amxc_container_of(it, wld_wpaCtrlGSock_t, it);
        if(swl_str_matches(pCtx->gName, name)) {
            return pCtx;
        }
    }
    SAH_TRACEZ_INFO(ME, "not found gSock named %s", name);
    return NULL;
}

bool wld_wpaCtrlGSock_checkByGName(const char* name) {
    return (s_fetchGSockByName(name) != NULL);
}

wld_wpaCtrlGSock_t* wld_wpaCtrlGSock_fetchByGName(const char* name) {
    return s_fetchGSockByName(name);
}

const char* wld_wpaCtrlGSock_getGName(wld_wpaCtrlGSock_t* pGlSk) {
    pGlSk = s_fetchGSockByData(pGlSk);
    ASSERTS_NOT_NULL(pGlSk, "", ME, "NULL");
    return pGlSk->gName;
}

wld_wpaCtrlInterface_t* wld_wpaCtrlGSock_getGIface(wld_wpaCtrlGSock_t* pGlSk) {
    pGlSk = s_fetchGSockByData(pGlSk);
    ASSERTS_NOT_NULL(pGlSk, NULL, ME, "NULL");
    return pGlSk->gIface;
}

const char* wld_wpaCtrlGSock_getGIfacePath(wld_wpaCtrlGSock_t* pGlSk) {
    return wld_wpaCtrlInterface_getPath(wld_wpaCtrlGSock_getGIface(pGlSk));
}

wld_wpaCtrlMngr_t* wld_wpaCtrlGSock_getGMgr(wld_wpaCtrlGSock_t* pGlSk) {
    pGlSk = s_fetchGSockByData(pGlSk);
    ASSERTS_NOT_NULL(pGlSk, NULL, ME, "NULL");
    return pGlSk->gMgr;
}

swl_rc_ne wld_wpaCtrlGSock_cleanup(wld_wpaCtrlGSock_t** ppGlSk) {
    ASSERTS_NOT_NULL(ppGlSk, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_wpaCtrlGSock_t* pGlSk = s_fetchGSockByData(*ppGlSk);
    if(pGlSk == NULL) {
        if(*ppGlSk == NULL) {
            return SWL_RC_DONE;
        }
        return SWL_RC_INVALID_PARAM;
    }
    amxc_llist_it_take(&pGlSk->it);
    wld_wpaCtrlInterface_cleanup(&pGlSk->gIface);
    wld_wpaCtrlMngr_cleanup(&pGlSk->gMgr);
    W_SWL_FREE(pGlSk->gName);
    free(pGlSk);
    *ppGlSk = NULL;
    return SWL_RC_OK;
}

swl_rc_ne wld_wpaCtrlGSock_setServerPath(wld_wpaCtrlGSock_t* pGlSk, const char* serverPath) {
    pGlSk = s_fetchGSockByData(pGlSk);
    ASSERTS_NOT_NULL(pGlSk, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTI_STR(pGlSk->gName, SWL_RC_INVALID_STATE, ME, "sock name uninitialized");
    swl_rc_ne rc = SWL_RC_OK;
    const char* connSrvPath = wld_wpaCtrlInterface_getConnectionDirPath(pGlSk->gIface);
    if(!swl_str_isEmpty(connSrvPath)) {
        if(swl_str_matches(connSrvPath, serverPath)) {
            return SWL_RC_DONE;
        }
        wld_wpaCtrlInterface_cleanup(&pGlSk->gIface);
    }
    if(!swl_str_isEmpty(serverPath)) {
        wld_wpaCtrlInterface_init(&pGlSk->gIface, pGlSk->gName, (char*) serverPath);
        wld_wpaCtrlMngr_registerInterface(pGlSk->gMgr, pGlSk->gIface);
        wld_wpaCtrlInterface_setEnable(pGlSk->gIface, true);
    }
    return rc;
}

static swl_rc_ne s_initGSock(wld_wpaCtrlGSock_t** ppGlSk, wld_secDmn_t* pSecDmn, wld_secDmnGrp_t* pSecDmnGrp, const char* serverPath) {
    ASSERTS_NOT_NULL(ppGlSk, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_wpaCtrlGSock_t* pGlSk = *ppGlSk;
    if(pGlSk == NULL) {
        pGlSk = calloc(1, sizeof(*pGlSk));
        ASSERT_NOT_NULL(pGlSk, SWL_RC_ERROR, ME, "fail to alloc gsock ctx");
        if((asprintf(&pGlSk->gName, "%s%p", GSOCK_PFX, pGlSk) < 0) ||
           (!wld_wpaCtrlMngr_init(&pGlSk->gMgr, pSecDmn)) ||
           (!wld_wpaCtrlMngr_setSecDmnGrp(pGlSk->gMgr, pSecDmnGrp))) {
            SAH_TRACEZ_ERROR(ME, "fail to build gsock ctx");
            wld_wpaCtrlGSock_cleanup(&pGlSk);
            return SWL_RC_ERROR;
        }
        amxc_llist_it_init(&pGlSk->it);
        amxc_llist_append(&sGlSkList, &pGlSk->it);
        *ppGlSk = pGlSk;
    } else {
        ASSERT_EQUALS(s_fetchGSockByData(pGlSk), pGlSk, SWL_RC_INVALID_PARAM, ME, "invalid gSock ctx");
    }
    wld_wpaCtrlGSock_setServerPath(pGlSk, serverPath);
    return SWL_RC_OK;
}

swl_rc_ne wld_wpaCtrlGSock_initWithSecDmn(wld_wpaCtrlGSock_t** ppGlSk, wld_secDmn_t* pSecDmn) {
    ASSERTS_NOT_NULL(ppGlSk, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pSecDmn, SWL_RC_INVALID_PARAM, ME, "Missing secDmn");
    return s_initGSock(ppGlSk, pSecDmn, wld_secDmn_getGrp(pSecDmn), wld_secDmn_getCtrlIfaceDirPath(pSecDmn));
}

swl_rc_ne wld_wpaCtrlGSock_initWithSecDmnGrp(wld_wpaCtrlGSock_t** ppGlSk, wld_secDmnGrp_t* pSecDmnGrp, const char* serverPath) {
    ASSERTS_NOT_NULL(ppGlSk, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pSecDmnGrp, SWL_RC_INVALID_PARAM, ME, "Missing secDmnGrp");
    return s_initGSock(ppGlSk, NULL, pSecDmnGrp, serverPath);
}

bool wld_wpaCtrlGSock_isReady(wld_wpaCtrlGSock_t* pGlSk) {
    pGlSk = s_fetchGSockByData(pGlSk);
    ASSERTS_NOT_NULL(pGlSk, false, ME, "NULL");
    return wld_wpaCtrlMngr_isReady(pGlSk->gMgr);
}

bool wld_wpaCtrlGSock_isConnected(wld_wpaCtrlGSock_t* pGlSk) {
    pGlSk = s_fetchGSockByData(pGlSk);
    ASSERTS_NOT_NULL(pGlSk, false, ME, "NULL");
    return wld_wpaCtrlMngr_isConnected(pGlSk->gMgr);
}

bool wld_wpaCtrlGSock_isConnecting(wld_wpaCtrlGSock_t* pGlSk) {
    pGlSk = s_fetchGSockByData(pGlSk);
    ASSERTS_NOT_NULL(pGlSk, false, ME, "NULL");
    return wld_wpaCtrlMngr_isConnecting(pGlSk->gMgr);
}

swl_rc_ne wld_wpaCtrlGSock_connect(wld_wpaCtrlGSock_t* pGlSk) {
    pGlSk = s_fetchGSockByData(pGlSk);
    ASSERTS_NOT_NULL(pGlSk, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_TRUE(pGlSk->gMgr && pGlSk->gIface, SWL_RC_INVALID_STATE, ME, "Missing full initialization");
    if(wld_wpaCtrlGSock_isConnected(pGlSk)) {
        return SWL_RC_DONE;
    }
    amxp_timer_set_interval(pGlSk->gMgr->connectTimer, GSOCK_RETRY_DELAY_MS);
    amxp_timer_start(pGlSk->gMgr->connectTimer, GSOCK_FIRST_DELAY_MS);
    return SWL_RC_OK;
}

swl_rc_ne wld_wpaCtrlGSock_stopConnecting(wld_wpaCtrlGSock_t* pGlSk) {
    pGlSk = s_fetchGSockByData(pGlSk);
    ASSERTS_NOT_NULL(pGlSk, SWL_RC_INVALID_PARAM, ME, "NULL");
    return (wld_wpaCtrlMngr_stopConnecting(pGlSk->gMgr) ? SWL_RC_OK : SWL_RC_ERROR);
}

swl_rc_ne wld_wpaCtrlGSock_disconnect(wld_wpaCtrlGSock_t* pGlSk) {
    pGlSk = s_fetchGSockByData(pGlSk);
    ASSERTS_NOT_NULL(pGlSk, SWL_RC_INVALID_PARAM, ME, "NULL");
    if(!wld_wpaCtrlMngr_isConnected(pGlSk->gMgr) && !wld_wpaCtrlMngr_isConnecting(pGlSk->gMgr)) {
        return SWL_RC_DONE;
    }
    return (wld_wpaCtrlMngr_disconnect(pGlSk->gMgr) ? SWL_RC_OK : SWL_RC_ERROR);
}

uint32_t wld_wpaCtrlGSock_countUsers(wld_wpaCtrlGSock_t* pGlSk) {
    pGlSk = s_fetchGSockByData(pGlSk);
    ASSERTS_NOT_NULL(pGlSk, 0, ME, "NULL");
    wld_secDmnGrp_t* pSecDmnGrp = wld_wpaCtrlMngr_getSecDmnGrp(pGlSk->gMgr);
    uint32_t count = 0;
    if(pSecDmnGrp != NULL) {
        for(uint32_t i = 0; i < wld_secDmnGrp_getMembersCount(pSecDmnGrp); i++) {
            wld_secDmn_t* pSecDmn = (wld_secDmn_t*) wld_secDmnGrp_getMemberByPos(pSecDmnGrp, i);
            count += wld_secDmn_isAlive(pSecDmn);
        }
        return count;
    }
    wld_secDmn_t* pSecDmn = wld_wpaCtrlMngr_getSecDmn(pGlSk->gMgr);
    return wld_secDmn_isAlive(pSecDmn);
}

swl_rc_ne wld_wpaCtrlGSock_disconnectIfUnused(wld_wpaCtrlGSock_t* pGlSk) {
    pGlSk = s_fetchGSockByData(pGlSk);
    ASSERTS_NOT_NULL(pGlSk, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTI_EQUALS(wld_wpaCtrlGSock_countUsers(pGlSk), 0, SWL_RC_INVALID_STATE, ME, "glSk %s still used", pGlSk->gName);
    return wld_wpaCtrlGSock_disconnect(pGlSk);
}

