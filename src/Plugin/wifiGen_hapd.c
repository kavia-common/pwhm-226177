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
#include "wld/wld.h"
#include "wld/wld_util.h"
#include "wld/wld_radio.h"
#include "wld/wld_ssid.h"
#include "wld/wld_accesspoint.h"
#include "wld/wld_linuxIfUtils.h"
#include "wld/wld_rad_nl80211.h"
#include "wld/wld_hostapd_cfgFile.h"
#include "wld/wld_rad_hostapd_api.h"
#include "wld/wld_wpaCtrl_api.h"
#include "wld/wld_wpaCtrl_events.h"
#include "wld/wld_wpaCtrlGSock.h"
#include "wld/wld_hostapd_ap_api.h"
#include "wifiGen_hapd.h"
#include "wifiGen_events.h"
#include "wifiGen_fsm.h"
#include "wifiGen_rad.h"
#include "wld/wld_common.h"

#define ME "genHapd"
#define HOSTAPD_CONF_FILE_PATH_FORMAT "/tmp/%s_hapd.conf"
#define HOSTAPD_ARGS_FORMAT "-ddt"

#define HOSTAPD_EXIT_REASON_SUCCESS 0
/*
 * hostapd exits with retcode -1, when failing to:
 * - alloc memory for internal usage
 * - init global ctx
 * - init wpa ctrl interface (comm sock)
 */
#define HOSTAPD_EXIT_REASON_INIT_FAIL -1
/*
 * hostapd exits with retcode 1, when failing to:
 * - detect local interfaces
 * - parse config file or load config sections of global/iface/bss
 */
#define HOSTAPD_EXIT_REASON_LOAD_FAIL 1

/*
 * MLD Link wpa socket name format "<IFACEX>_link<Y>"
 */

static char sSockNameLinkPfx[64] = {"_link"};

bool wifiGen_hapd_parseSockName(const char* sockName, char* linkName, size_t linkNameSize, int32_t* pLinkId) {
    W_SWL_SETPTR(pLinkId, -1);
    swl_str_copy(linkName, linkNameSize, NULL);
    ASSERT_STR(sockName, false, ME, "empty");
    ssize_t linkInfoPos = swl_str_find(sockName, sSockNameLinkPfx);
    if(linkInfoPos < 0) {
        return swl_str_copy(linkName, linkNameSize, sockName);
    }
    int32_t linkId = -1;
    const char* linkIdStr = &sockName[linkInfoPos + swl_str_len(sSockNameLinkPfx)];
    if(!swl_rc_isOk(wldu_convStrToNum(linkIdStr, &linkId, sizeof(linkId), 10, true))) {
        SAH_TRACEZ_ERROR(ME, "fail to fetch link id in sock name %s", sockName);
        return false;
    }
    W_SWL_SETPTR(pLinkId, linkId);
    return swl_str_ncopy(linkName, linkNameSize, sockName, linkInfoPos);
}

T_AccessPoint* wifiGen_hapd_fetchSockApLink(T_AccessPoint* pAPMld, const char* sockName) {
    ASSERT_STR(sockName, NULL, ME, "empty socket name");
    ASSERT_NOT_NULL(pAPMld, NULL, ME, "NULL");
    wld_wpaCtrlMngr_t* pMgr = wld_wpaCtrlInterface_getMgr(pAPMld->wpaCtrlInterface);
    wld_secDmn_t* pSecDmn = wld_wpaCtrlMngr_getSecDmn(pMgr);
    const char* serverPath = wld_secDmn_getCtrlIfaceDirPath(pSecDmn);
    ASSERT_STR(serverPath, NULL, ME, "%s: empty wpa server path", pAPMld->alias);
    T_SSID* pMldSSID = pAPMld->pSSID;
    ASSERT_NOT_NULL(pMldSSID, NULL, ME, "NULL");
    T_SSID* pLinkSSID = NULL;
    size_t maxMsgLen = wld_wpaCtrl_getMaxMsgLen();
    char reply[maxMsgLen];
    memset(reply, 0, sizeof(reply));
    swl_rc_ne rc = wld_wpaCtrl_queryToSock(serverPath, sockName, "GET_CONFIG", reply, sizeof(reply));
    ASSERTI_TRUE(swl_rc_isOk(rc), NULL, ME, "%s: fail to get hostapd config over %s/%s", pAPMld->alias, serverPath, sockName);
    char valStr[128] = {0};
    char ssidStr[64] = {0};
    swl_macBin_t macBin = SWL_MAC_BIN_NEW();
    if((wld_wpaCtrl_getValueStr(reply, "bssid", valStr, sizeof(valStr)) > 0) &&
       (swl_typeMacBin_fromChar(&macBin, valStr)) && (!swl_mac_binIsNull(&macBin)) &&
       ((pLinkSSID = wld_ssid_getSsidByMacAddress(&macBin)) != NULL)) {
        SAH_TRACEZ_INFO(ME, "sock(%s): GET_CONFIG linkMac(%s) => pSSID(%s)",
                        sockName, swl_typeMacBin_toBuf32(macBin).buf, pLinkSSID->Name);
        return pLinkSSID->AP_HOOK;
    }
    wld_wpaCtrl_getValueStr(reply, "ssid", ssidStr, sizeof(ssidStr));
    rc = wld_wpaCtrl_queryToSock(serverPath, sockName, "STATUS", reply, sizeof(reply));
    ASSERTI_TRUE(swl_rc_isOk(rc), NULL, ME, "%s: fail to get hostapd status over %s/%s", pAPMld->alias, serverPath, sockName);
    swl_chanspec_t chspec = SWL_CHANSPEC_EMPTY;
    int32_t freq = 0;
    if((wld_wpaCtrl_getValueIntExt(reply, "freq", &freq)) &&
       (swl_chanspec_channelFromMHz(&chspec, freq) >= SWL_RC_OK)) {
        SAH_TRACEZ_INFO(ME, "sock(%s): STATUS: bssid(%s) ssid(%s) freq:%d chspec(%s)",
                        sockName, swl_typeMacBin_toBuf32(macBin).buf, ssidStr,
                        freq, swl_typeChanspecExt_toBuf32(chspec).buf);
    }
    ASSERTI_TRUE(chspec.channel > 0, NULL, ME, "%s: no channel configured", pAPMld->alias);
    T_Radio* pRad = NULL;
    wld_for_eachRad(pRad) {
        if(pRad->operatingFrequencyBand != chspec.band) {
            continue;
        }
        T_AccessPoint* pAP = NULL;
        wld_rad_forEachAp(pAP, pRad) {
            pLinkSSID = pAP->pSSID;
            if((pLinkSSID != NULL) && (pLinkSSID->mldUnit == pMldSSID->mldUnit) &&
               ((swl_str_isEmpty(ssidStr) || (swl_str_matches(pLinkSSID->SSID, ssidStr))))) {
                SAH_TRACEZ_INFO(ME, "sock(%s): BASIC MATCH ssid(%s) mldUnit(%d) => pSSID(%s)",
                                sockName, pLinkSSID->SSID, pLinkSSID->mldUnit, pLinkSSID->Name);
                return pLinkSSID->AP_HOOK;
            }
        }
    }
    return NULL;
}

bool wifiGen_hapd_isStartable(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, false, ME, "NULL");
    ASSERT_NOT_NULL(pRad->hostapd, false, ME, "NULL");
    bool radEnaDm = amxd_object_get_bool(pRad->pBus, "Enable", NULL);
    return (radEnaDm && wld_rad_hasEnabledVap(pRad));
}

bool wifiGen_hapd_isStarted(T_Radio* pRad) {
    return ((pRad != NULL) && (wld_secDmn_isEnabled(pRad->hostapd)));
}

static void s_restoreMainAp(T_AccessPoint* pMainAP) {
    ASSERTI_NOT_NULL(pMainAP, , ME, "No vaps");
    T_Radio* pRad = pMainAP->pRadio;
    ASSERTI_NOT_NULL(pRad, , ME, "No rad");
    wifiGen_hapd_enableVapWpaCtrlIface(pMainAP);
    ASSERTI_FALSE(pMainAP->index > 0, , ME, "%s already created", pMainAP->alias);
    SAH_TRACEZ_WARNING(ME, "%s: main iface %s => must be re-created", pRad->Name, pMainAP->alias);
    wifiGen_rad_addVap(pRad, pMainAP);
    if(swl_str_matches(pRad->Name, pMainAP->alias)) {
        pRad->index = pMainAP->index;
        pRad->wDevId = pMainAP->wDevId;
    }
}

/**
 * @brief prepare hostapd interface before startup, adding/updating conf
 * this allows to pre-create main netdev interface,
 * and optionnally remove passive BSS's netdev iface (shall be created by hostapd)
 *
 * @param pRad pointer to radio context
 * @param clean bool optional flag indication whether to clear passive BSS's netdev iface
 *
 * @return void
 */
void wifiGen_hapd_prepareIfaces(T_Radio* pRad, bool clean) {
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    ASSERT_NOT_NULL(pRad->hostapd, , ME, "NULL");
    SWL_CALL(pRad->hostapd->handlers.writeCfgCb, pRad->hostapd, pRad);
    T_AccessPoint* pMainVap = wld_rad_hostapd_getCfgMainVap(pRad);
    T_AccessPoint* pAP = NULL;
    wld_rad_forEachAp(pAP, pRad) {
        bool doClean = clean;
        T_AccessPoint* pAPLink = wld_vap_get_vap(wld_hostapd_ap_selectApLinkIface(pAP));
        // pre-create Radio main interface if matching the main link iface
        // (case NoMLO, or radio disabled)
        if(pAP == pMainVap) {
            if(pAP == pAPLink) {
                s_restoreMainAp(pAP);
                continue;
            }
        }
        // pre-create APMLD main link iface if used by more that one link
        // otherwise hostapd will create it (case SLO)
        if(pAP->pSSID && wld_mld_isLinkUsable(pAP->pSSID->pMldLink) && (wld_mld_countNeighUsableLinks(pAP->pSSID->pMldLink) > 1)) {
            s_restoreMainAp(pAPLink);
            if(pAP == pAPLink) {
                continue;
            }
        }
        if(doClean && !wld_vap_isDummyVap(pAP) && (pAP->index > 0) && (!swl_str_matches(pAP->alias, pRad->Name))) {
            SAH_TRACEZ_WARNING(ME, "%s: delete unused vap iface %s", pRad->Name, pAP->alias);
            pRad->pFA->mfn_wrad_delvapif(pRad, pAP->alias);
        }
    }
}

void wifiGen_hapd_restoreMainIface(T_Radio* pRad) {
    wifiGen_hapd_prepareIfaces(pRad, false);
}

void wifiGen_hapd_deauthKnownStations(T_Radio* pRad, bool noAck) {
    T_AccessPoint* pAP;
    wld_rad_forEachAp(pAP, pRad) {
        wld_ap_hostapd_deauthKnownStations(pAP);
        if(noAck) {
            wld_vap_remove_all(pAP);
        }
    }
}

static const char* s_getMainIface(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, NULL, ME, "NULL");
    const char* mainIface = pRad->Name;
    T_AccessPoint* pMainAP = wld_rad_hostapd_getCfgMainVap(pRad);
    if(pMainAP != NULL) {
        mainIface = pMainAP->alias;
    }
    return mainIface;
}

static void s_restartHapdCb(wld_secDmn_t* pSecDmn, void* userdata) {
    T_Radio* pRad = (T_Radio*) userdata;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    const char* mainIface = s_getMainIface(pRad);
    ASSERTW_TRUE(wifiGen_hapd_isStartable(pRad), , ME, "%s: hostapd iface %s is not startable", pRad->Name, mainIface);
    ASSERTW_FALSE(wifiGen_hapd_isRunning(pRad), , ME, "%s: hostapd running", mainIface);
    SAH_TRACEZ_WARNING(ME, "%s: restarting hostapd", mainIface);
    wld_secDmn_restartCb(pSecDmn);
}

static void s_onStopHapdCb(wld_secDmn_t* pSecDmn, void* userdata) {
    T_Radio* pRad = (T_Radio*) userdata;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    const char* mainIface = s_getMainIface(pRad);
    SAH_TRACEZ_WARNING(ME, "%s: hostapd stopped", mainIface);
    wld_deamonExitInfo_t* pExitInfo = &pSecDmn->dmnProcess->lastExitInfo;
    if((pExitInfo != NULL) && (pExitInfo->isExited) &&
       (pExitInfo->exitStatus == HOSTAPD_EXIT_REASON_LOAD_FAIL)) {
        SAH_TRACEZ_ERROR(ME, "%s: invalid hostapd configuration", mainIface);
    }

    //remove previously used server sockets after hostapd stopped
    //to prevent any confusion on next startup
    wld_wpaCtrlMngr_t* pMgr = wld_secDmn_getWpaCtrlMgr(pSecDmn);
    for(uint32_t i = 0; i < wld_wpaCtrlMngr_countInterfaces(pMgr); i++) {
        wld_wpaCtrlInterface_t* pIface = wld_wpaCtrlMngr_getInterface(pMgr, i);
        const char* pIfacePath = wld_wpaCtrlInterface_getPath(pIface);
        if(wld_wpaCtrl_checkSockPath(pIfacePath)) {
            SAH_TRACEZ_INFO(ME, "%s: remove wpactrl srv sock: %s", mainIface, pIfacePath);
            unlink(pIfacePath);
        }
    }

    //finalize hapd cleanup
    wifiGen_hapd_stopDaemon(pRad);
    wld_rad_updateState(pRad, true);
}

static void s_onStartHapdCb(wld_secDmn_t* pSecDmn _UNUSED, void* userdata) {
    T_Radio* pRad = (T_Radio*) userdata;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    ASSERT_NOT_NULL(pRad->hostapd, , ME, "NULL");
    const char* mainIface = s_getMainIface(pRad);
    SAH_TRACEZ_WARNING(ME, "%s: hostapd started", mainIface);
    wld_wpaCtrlMngr_t* pMgr = wld_secDmn_getWpaCtrlMgr(pRad->hostapd);
    bool hapdHasGSock = wld_wpaCtrlGSock_checkGPathInDmnArgs(pRad->hostapd);
    if(hapdHasGSock && !wifiGen_hapd_isStartable(pRad)) {
        wld_wpaCtrlMngr_disconnect(pMgr);
    } else if(wld_wpaCtrlMngr_connect(pMgr) && hapdHasGSock) {
        wld_wpaCtrlGSock_connect(pRad->hostapd->glSk);
    }
}

static char* s_getHapdArgsCb(wld_secDmn_t* pSecDmn, void* userdata _UNUSED) {
    char* args = NULL;
    ASSERT_NOT_NULL(pSecDmn, args, ME, "NULL");
    char startArgs[256] = {0};
    //set default start args
    swl_str_copy(startArgs, sizeof(startArgs), HOSTAPD_ARGS_FORMAT);
    const char* gSockPath = wld_wpaCtrlGSock_getGIfacePath(pSecDmn->glSk);
    if(!swl_str_isEmpty(gSockPath)) {
        swl_strlst_catFormat(startArgs, sizeof(startArgs), " ", "-g %s", gSockPath);
    }
    if(!swl_str_isEmpty(pSecDmn->cfgFile)) {
        swl_strlst_cat(startArgs, sizeof(startArgs), " ", pSecDmn->cfgFile);
    }
    swl_str_copyMalloc(&args, startArgs);
    return args;
}

static bool s_stopHapdCb(wld_secDmn_t* pSecDmn, void* userdata _UNUSED) {
    T_Radio* pRad = (T_Radio*) userdata;
    ASSERT_NOT_NULL(pRad, false, ME, "NULL");
    wld_wpaCtrlMngr_t* pMgr = wld_secDmn_getWpaCtrlMgr(pSecDmn);
    bool ret = false;
    char sockName[128] = {0};
    wld_wpaCtrlInterface_t* pIface = wld_wpaCtrlMngr_getFirstReadyInterface(pMgr);
    if(wld_wpaCtrlInterface_checkConnectionPath(pIface) && wld_secDmn_isActiveAlone(pSecDmn)) {
        swl_str_copy(sockName, sizeof(sockName), wld_wpaCtrlInterface_getConnectionSockName(pIface));
    }

    wld_scan_stop(pRad);
    wifiGen_hapd_deauthKnownStations(pRad, true);
    T_AccessPoint* pAP = NULL;
    wld_rad_forEachAp(pAP, pRad) {
        if(pAP->pSSID) {
            wld_mld_resetLinkId(pAP->pSSID->pMldLink);
        }
    }
    if(!swl_str_isEmpty(sockName)) {
        wld_wpaCtrlInterface_t* pIface = wld_wpaCtrlGSock_getGIface(pSecDmn->glSk);
        SAH_TRACEZ_WARNING(ME, "terminating hostapd over %s", wld_wpaCtrlInterface_getName(pIface));
        ret = wld_wpaCtrl_sendCmd(pIface, "TERMINATE");
    }
    wld_wpaCtrlMngr_disconnect(pMgr);
    return ret;
}

void wifiGen_hapd_initDynCfgParamSupp(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    ASSERTS_NOT_NULL(pRad->hostapd, , ME, "NULL");
    /*
     * here declare hostapd cfg params to check (or to define) as supported or not
     * wld_secDmn_setCfgParamSupp(pRad->hostapd, "custom_param", SWL_TRL_UNKNOWN);
     */
    bool hasRnr = (swl_bit32_getHighest(pRad->supportedStandards) >= SWL_RADSTD_AX);
    wld_secDmn_setCfgParamSupp(pRad->hostapd, "rnr", hasRnr ? SWL_TRL_TRUE : SWL_TRL_UNKNOWN);
    wld_secDmn_setCfgParamSupp(pRad->hostapd, "config_id", SWL_TRL_TRUE);

    const char* mrsnoParams[] = {
        "rsn_override_key_mgmt", "rsn_override_pairwise", "rsn_override_mfp",
        "rsn_override_key_mgmt_2", "rsn_override_pairwise_2", "rsn_override_mfp_2",
        "rsn_override_omit_rsnxe",
    };
    uint32_t nMrsnoParams = sizeof(mrsnoParams) / sizeof(mrsnoParams[0]);
    if(wld_secDmn_detectCfgParamsSupp(pRad->hostapd, mrsnoParams, nMrsnoParams, "rsn_override_") > 0) {
        wld_rad_addSuppDrvCap(pRad, wld_rad_getFreqBand(pRad), "MRSNO");
    }

    wld_secDmn_detectCfgParamsSupp(pRad->hostapd, (const char* []) {"mld_addr"}, 1, NULL);
}

swl_rc_ne wifiGen_hapd_init(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    char confFilePath[128] = {0};
    swl_str_catFormat(confFilePath, sizeof(confFilePath), HOSTAPD_CONF_FILE_PATH_FORMAT, pRad->Name);
    swl_rc_ne rc = wld_secDmn_init(&pRad->hostapd, HOSTAPD_CMD, NULL, confFilePath, HOSTAPD_CTRL_IFACE_DIR);
    ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "%s: Fail to init hostapd", pRad->Name);
    wld_secDmnEvtHandlers handlers;
    memset(&handlers, 0, sizeof(handlers));
    handlers.restartCb = s_restartHapdCb;
    handlers.stopCb = s_onStopHapdCb;
    handlers.startCb = s_onStartHapdCb;
    handlers.getArgs = s_getHapdArgsCb;
    handlers.stop = s_stopHapdCb;
    wld_secDmn_setEvtHandlers(pRad->hostapd, &handlers, pRad);
    return SWL_RC_OK;
}

swl_rc_ne wifiGen_hapd_initVAP(T_AccessPoint* pAP) {
    ASSERT_NOT_NULL(pAP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pAP->pRadio, SWL_RC_ERROR, ME, "No radio");
    ASSERT_NOT_NULL(pAP->pRadio->hostapd, SWL_RC_ERROR, ME, "hostapd not initialized");
    ASSERT_TRUE(wld_wpaCtrlInterface_init(&pAP->wpaCtrlInterface, pAP->alias, pAP->pRadio->hostapd->ctrlIfaceDir),
                SWL_RC_ERROR, ME, "%s: fail to init wpa_ctrl interface", pAP->alias);
    ASSERT_TRUE(wld_wpaCtrlMngr_registerInterface(pAP->pRadio->hostapd->wpaCtrlMngr, pAP->wpaCtrlInterface),
                SWL_RC_ERROR, ME, "%s: fail to add wpa_ctrl interface to mngr", pAP->alias);
    return SWL_RC_OK;
}

void wifiGen_hapd_cleanup(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: Destroy hostapd", pRad->Name);
    wld_secDmn_cleanup(&pRad->hostapd);
}

void wifiGen_hapd_enableVapWpaCtrlIface(T_AccessPoint* pAP) {
    ASSERTS_NOT_NULL(pAP, , ME, "NULL");
    T_Radio* pRad = pAP->pRadio;
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    bool ena = wld_hostapd_ap_needWpaCtrlIface(pAP);
    wld_wpaCtrlInterface_setEnable(pAP->wpaCtrlInterface, ena);
}

/**
 * @brief mark as enabled all required radio AP's wpactrl interfaces
 * (so that wpactrl mngr will check their establishment)
 *
 * @param pRad pointer to radio context
 *
 * @return void
 */
void wifiGen_hapd_enableWpaCtrlIfaces(T_Radio* pRad) {
    T_AccessPoint* pAP = NULL;
    wld_rad_forEachAp(pAP, pRad) {
        wifiGen_hapd_enableVapWpaCtrlIface(pAP);
    }
}

swl_rc_ne wifiGen_hapd_startDaemon(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    SAH_TRACEZ_WARNING(ME, "%s: Start hostapd", pRad->Name);
    wifiGen_hapd_enableWpaCtrlIfaces(pRad);
    return wld_secDmn_start(pRad->hostapd);
}

swl_rc_ne wifiGen_hapd_stopDaemon(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    SAH_TRACEZ_WARNING(ME, "%s: Stop hostapd", pRad->Name);
    swl_rc_ne rc = wld_secDmn_stop(pRad->hostapd);
    T_AccessPoint* pAP = NULL;
    wld_rad_forEachAp(pAP, pRad) {
        if(pAP->pSSID) {
            wld_mld_resetLinkId(pAP->pSSID->pMldLink);
        }
    }
    ASSERTI_NOT_EQUALS(rc, SWL_RC_CONTINUE, rc, ME, "%s: hostapd being stopped", pRad->Name);
    wld_rad_forEachAp(pAP, pRad) {
        if(pAP->index > 0) {
            wld_linuxIfUtils_setState(wld_rad_getSocket(pRad), pAP->alias, 0);
        }
    }
    ASSERTI_FALSE(rc < SWL_RC_OK, rc, ME, "%s: hostapd not running", pRad->Name);
    return rc;
}

swl_rc_ne wifiGen_hapd_reloadDaemon(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s : Reload Hostapd", pRad->Name);
    swl_rc_ne rc = SWL_RC_OK;
    wifiGen_hapd_deauthKnownStations(pRad, true);
    if((wifiGen_hapd_countGrpMembers(pRad) < 2) ||
       ((rc = wld_rad_hostapd_reconfigure(pRad)) < SWL_RC_OK)) {
        rc = wld_secDmn_reload(pRad->hostapd);
    }
    return rc;
}

void wifiGen_hapd_writeConfig(T_Radio* pRad) {
    wld_hostapd_cfgFile_createExt(pRad);
}

static wld_wpaCtrlInterface_t* s_mainInterface(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, NULL, ME, "NULL");
    ASSERTS_NOT_NULL(pRad->hostapd, NULL, ME, "NULL");
    return wld_wpaCtrlMngr_getDefaultInterface(pRad->hostapd->wpaCtrlMngr);
}

bool wifiGen_hapd_isRunning(T_Radio* pRad) {
    return ((pRad != NULL) && (wld_secDmn_isRunning(pRad->hostapd)));
}

bool wifiGen_hapd_isAlive(T_Radio* pRad) {
    return ((pRad != NULL) && (wld_secDmn_isAlive(pRad->hostapd)));
}

SWL_TABLE(sHapdStateDescMaps,
          ARR(char* hapdStateDesc; chanmgt_rad_state radDetState; ),
          ARR(swl_type_charPtr, swl_type_uint32, ),
          ARR({"UNKNOWN", CM_RAD_UNKNOWN},
              {"UNINITIALIZED", CM_RAD_ERROR},
              {"COUNTRY_UPDATE", CM_RAD_CONFIGURING},
              {"ACS", CM_RAD_CONFIGURING},
              {"HT_SCAN", CM_RAD_CONFIGURING},
              {"DFS", CM_RAD_FG_CAC},
              {"DISABLED", CM_RAD_DOWN},
              {"ENABLED", CM_RAD_UP},
              ));
swl_rc_ne wifiGen_hapd_getRadState(T_Radio* pRad, chanmgt_rad_state* pDetailedState) {
    ASSERTS_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_wpaCtrlInterface_t* mainIface = s_mainInterface(pRad);
    ASSERTI_NOT_NULL(mainIface, SWL_RC_ERROR, ME, "%s: No main hapd wpactrl iface", pRad->Name);
    char reply[1024] = {0};
    char state[64] = {0};
    if((!wld_wpaCtrl_sendCmdSynced(mainIface, "STATUS", reply, sizeof(reply) - 1)) ||
       (wld_wpaCtrl_getValueStr(reply, "state", state, sizeof(state)) <= 0)) {
        SAH_TRACEZ_INFO(ME, "%s: status not yet available", pRad->Name);
        return SWL_RC_ERROR;
    }
    chanmgt_rad_state* pRadDetState = (chanmgt_rad_state*) swl_table_getMatchingValue(&sHapdStateDescMaps, 1, 0, state);
    ASSERTI_NOT_NULL(pRadDetState, SWL_RC_ERROR, ME, "%s: unknown hapd state(%s)", pRad->Name, state);
    W_SWL_SETPTR(pDetailedState, *pRadDetState);
    SAH_TRACEZ_INFO(ME, "%s: hapd state(%s) -> radDetState(%d)", pRad->Name, state, *pRadDetState);
    return SWL_RC_OK;
}

swl_rc_ne wifiGen_hapd_syncVapStates(T_Radio* pRad) {
    swl_rc_ne ret = SWL_RC_OK;

    /*
     * in case we missed BSS re-creation nl80211 event, refresh relative info (ifIndex, wdevId)
     */
    wifiGen_refreshVapsIfIdx(pRad);

    wld_rad_hostapd_updateAllVapsConfigId(pRad);

    chanmgt_rad_state detRadState = CM_RAD_UNKNOWN;
    wifiGen_hapd_getRadState(pRad, &detRadState);

    /*
     * Now as ifIndex are up to date, we can sync the enabling status
     */
    T_AccessPoint* pAP = NULL;
    wld_rad_forEachAp(pAP, pRad) {
        if(!wld_wpaCtrlInterface_isReady(pAP->wpaCtrlInterface) || (pAP->index <= 0) || (pAP->pBus == NULL)) {
            continue;
        }
        if((!pAP->enable) &&
           (pAP->pFA->mfn_wvap_enable(pAP, pAP->enable, GET | DIRECT) > 0)) {
            if(pAP->pFA->mfn_wvap_enable(pAP, pAP->enable, SET | DIRECT) == 0) {
                SAH_TRACEZ_INFO(ME, "%s: sync disable vap", pAP->alias);
            }
        } else if((pAP->enable) &&
                  (pAP->pFA->mfn_wvap_status(pAP) == 0) && (detRadState == CM_RAD_UP)) {
            //need to restart broadcasting the enabled bss,
            //that were potentially stopped by hapd when disabling one AP
            SAH_TRACEZ_INFO(ME, "%s: sync enable vap", pAP->alias);
            pAP->pFA->mfn_wvap_enable(pAP, pAP->enable, SET | DIRECT);
            wld_secDmn_action_rc_ne rc;
            /*
             * first, set dyn ena/disabling vap params, before applying any action
             */
            if((rc = wld_ap_hostapd_setEnableVap(pAP, pAP->enable)) < SECDMN_ACTION_OK_DONE) {
                SAH_TRACEZ_ERROR(ME, "%s: fail to save enable %d rc %d", pAP->alias, pAP->enable, rc);
            }

            if(!wld_ap_hostapd_updateBeacon(pAP, "syncAp")) {
                ret = SWL_RC_ERROR;
            }
            /*
             * As iface has been stopped, nl80211 level has lost the station list, but not hostapd.
             * So stations need to re-authenticate to be able to retrieve connection status and stats from nl80211.
             */
            wld_ap_hostapd_deauthAllStations(pAP);
        }
        wld_vap_updateState(pAP);
    }
    return ret;
}

uint32_t wifiGen_hapd_countGrpMembers(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, 0, ME, "NULL");
    wld_secDmn_t* pSecDmn = pRad->hostapd;
    ASSERTS_NOT_NULL(pSecDmn->dmnProcess, 0, ME, "NULL");
    if(wld_secDmn_isGrpMember(pSecDmn)) {
        return wld_secDmnGrp_getMembersCount(wld_secDmn_getGrp(pSecDmn));
    }
    //default: one hostapd instance per radio
    return 1;
}

/**
 * @brief return valid radio context registered as secDmn userdata
 * (or security dameon group member)
 *
 * @param pGmb pointer to security daemon context
 *
 * @return pointer to valid radio context used by the security daemon
 *                 null otherwise
 */
static T_Radio* s_getGrpMemberRadObj(wld_secDmn_t* pGmb) {
    return (pGmb && debugIsRadPointer(pGmb->userData)) ? (T_Radio*) pGmb->userData : NULL;
}
static uint32_t s_getGrpMemberRadObjIdx(const void* e) {
    wld_secDmn_t* pGmb = e ? *((wld_secDmn_t**) e) : NULL;
    T_Radio* pR = s_getGrpMemberRadObj(pGmb);
    return (pR ? amxd_object_get_index(pR->pBus) : 0);
}
static int s_grpMemberRadObjIdxCmp(const void* e1, const void* e2) {
    uint32_t gmb1oIdx1 = s_getGrpMemberRadObjIdx(e1);
    uint32_t gmb1oIdx2 = s_getGrpMemberRadObjIdx(e2);
    if((!gmb1oIdx1) || (!gmb1oIdx2)) {
        return (gmb1oIdx2 - gmb1oIdx1);
    }
    return (gmb1oIdx1 - gmb1oIdx2);
}
static char* s_getGlobHapdArgsCb(wld_secDmnGrp_t* pSecDmnGrp, void* userData _UNUSED, const wld_process_t* pProc _UNUSED) {
    char* args = NULL;
    ASSERT_NOT_NULL(pSecDmnGrp, args, ME, "NULL");
    char startArgs[256] = {0};
    //set default start args
    swl_str_copy(startArgs, sizeof(startArgs), HOSTAPD_ARGS_FORMAT);
    wld_secDmn_t* grpMembers[wld_secDmnGrp_getMembersCount(pSecDmnGrp) + 1];
    uint32_t nGrpMembers = 0;
    for(uint32_t i = 0; i < wld_secDmnGrp_getMembersCount(pSecDmnGrp); i++) {
        wld_secDmn_t* pSecDmn = (wld_secDmn_t*) wld_secDmnGrp_getMemberByPos(pSecDmnGrp, i);
        if((pSecDmn != NULL) && (!swl_str_isEmpty(pSecDmn->cfgFile))) {
            grpMembers[nGrpMembers++] = pSecDmn;
        }
    }
    /*
     * sort grp member with their relative obj instance index
     * following the datamodel order, which is also the fsm order
     */
    qsort(grpMembers, nGrpMembers, sizeof(wld_secDmn_t*), s_grpMemberRadObjIdxCmp);
    const char* gSockPath = wld_wpaCtrlGSock_getGIfacePath(wld_secDmnGrp_getGlSk(pSecDmnGrp));
    if(!swl_str_isEmpty(gSockPath)) {
        swl_strlst_catFormat(startArgs, sizeof(startArgs), " ", "-g %s", gSockPath);
    }
    for(uint32_t i = 0; i < nGrpMembers; i++) {
        T_Radio* pR = s_getGrpMemberRadObj(grpMembers[i]);
        if(!pR || !wifiGen_hapd_isStartable(pR)) {
            continue;
        }
        //concat all startable radio ifaces conf files
        swl_strlst_cat(startArgs, sizeof(startArgs), " ", grpMembers[i]->cfgFile);
    }
    swl_str_copyMalloc(&args, startArgs);
    return args;
}

static bool s_isHapdIfaceStartable(wld_secDmnGrp_t* pSecDmnGrp _UNUSED, void* userData _UNUSED, wld_secDmn_t* pSecDmn) {
    ASSERT_NOT_NULL(pSecDmn, false, ME, "NULL");
    T_Radio* pRad = (T_Radio*) pSecDmn->userData;
    ASSERT_TRUE(debugIsRadPointer(pRad), false, ME, "INVALID");
    wifiGen_hapd_enableWpaCtrlIfaces(pRad);
    return wifiGen_hapd_isStartable(pRad);
}

static bool s_hasRtmSchedState(T_Radio* pRad, wifiGen_fsmStates_e fsmAc) {
    ASSERT_NOT_NULL(pRad, false, ME, "NULL");
    ASSERTS_NOT_EQUALS(pRad->fsmRad.FSM_State, FSM_IDLE, false, ME, "%s: fsm is idle", pRad->Name);
    if((pRad->fsmRad.FSM_SyncAll) ||
       (((pRad->fsmRad.FSM_State < FSM_DEPENDENCY) && isBitSetLongArray(pRad->fsmRad.FSM_BitActionArray, FSM_BW, fsmAc)) ||
        ((pRad->fsmRad.FSM_State >= FSM_DEPENDENCY) && isBitSetLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, fsmAc)))) {
        SAH_TRACEZ_INFO(ME, "%s: fsm state:%d, fsmAc:%d ongoing", pRad->Name, pRad->fsmRad.FSM_State, fsmAc);
        return true;
    }
    return false;
}

static bool s_hasHapdSchedRestart(wld_secDmnGrp_t* pSecDmnGrp _UNUSED, void* userData _UNUSED, wld_secDmn_t* pSecDmn) {
    ASSERT_NOT_NULL(pSecDmn, false, ME, "NULL");
    T_Radio* pRad = (T_Radio*) pSecDmn->userData;
    ASSERT_TRUE(debugIsRadPointer(pRad), false, ME, "INVALID");
    ASSERTI_TRUE(wld_secDmn_isRunning(pSecDmn), false, ME, "%s: secDmn not running", pRad->Name);
    return s_hasRtmSchedState(pRad, GEN_FSM_START_HOSTAPD) || s_hasRtmSchedState(pRad, GEN_FSM_MOD_HOSTAPD);
}

static wld_secDmnGrp_EvtHandlers_t sGHapdEvtCbs = {
    .getArgsCb = s_getGlobHapdArgsCb,
    .isMemberStartableCb = s_isHapdIfaceStartable,
    .hasSchedRestartCb = s_hasHapdSchedRestart,
};

static swl_rc_ne s_initGlobalHapdGrp(vendor_t* pVdr, bool forceGlob) {
    swl_rc_ne rc = SWL_RC_ERROR;
    ASSERTS_NOT_NULL(pVdr, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_dmnMgt_dmnExecInfo_t* gHapd = pVdr->globalHostapd;
    ASSERT_NOT_NULL(gHapd, SWL_RC_INVALID_PARAM, ME, "No glob hapd ctx");
    if((gHapd->globalDmnSupported == SWL_TRL_TRUE) || forceGlob) {
        if(gHapd->pGlobalDmnGrp == NULL) {
            SAH_TRACEZ_INFO(ME, "%s: initialize glob hostapd", pVdr->name);
            char grpName[128] = {0};
            swl_str_catFormat(grpName, sizeof(grpName), "%s-%s", HOSTAPD_CMD, pVdr->name);
            rc = wld_secDmnGrp_init(&gHapd->pGlobalDmnGrp, HOSTAPD_CMD, HOSTAPD_ARGS_FORMAT, grpName);
            wld_secDmnGrp_setEvtHandlers(gHapd->pGlobalDmnGrp, &sGHapdEvtCbs, pVdr);
        }
    } else if(gHapd->pGlobalDmnGrp != NULL) {
        SAH_TRACEZ_INFO(ME, "%s: cleanup glob hostapd", pVdr->name);
        rc = wld_secDmnGrp_cleanup(&gHapd->pGlobalDmnGrp);
    }
    return rc;
}

/**
 * @brief schedule restarting hostapd instance used by the radio context
 *
 * @param pRad pointer to radio context
 *
 * @return void
 */
void wifiGen_hapd_restartDaemon(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "restart %s", pRad->Name);
    if(wld_secDmn_isRunning(pRad->hostapd)) {
        wld_secDmn_restart(pRad->hostapd);
    } else if(wifiGen_hapd_isStartable(pRad)) {
        setBitLongArray(pRad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_START_HOSTAPD);
        setBitLongArray(pRad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_MOD_AP);
        wld_rad_doCommitIfUnblocked(pRad);
    }
}

/**
 * @brief schedule restarting hostapd instances used by all the radio
 * registered by a specific vendor
 *
 * @param pVdr pointer to vendor context
 *
 * @return void
 */
void wifiGen_hapd_restartAllDaemons(vendor_t* pVdr) {
    T_Radio* pRad;
    wld_for_eachRad(pRad) {
        if((pRad == NULL) || (pRad->vendor != pVdr)) {
            continue;
        }
        wifiGen_hapd_restartDaemon(pRad);
    }
}

/**
 * @brief handle the change of UseGlobalInstance field, switching from single or multiple instance
 *
 * @param pVdr pointer to vendor context
 * @param pCfg pointer to daemon execution settings
 *
 * @return true when a restart is needed to apply change
 */
static bool s_handleUseGlobalInstance(vendor_t* pVdr, wld_dmnMgt_dmnExecSettings_t* pCfg) {
    ASSERT_NOT_NULL(pVdr, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_dmnMgt_dmnExecInfo_t* gHapd = pVdr->globalHostapd;
    ASSERT_NOT_NULL(gHapd, SWL_RC_INVALID_PARAM, ME, "No glob hapd ctx");
    bool restartNeeded = false;
    bool forceGlob = (pCfg->useGlobalInstance == SWL_TRL_TRUE);
    s_initGlobalHapdGrp(pVdr, forceGlob);
    if(!forceGlob) {
        ASSERTS_EQUALS(gHapd->globalDmnSupported, SWL_TRL_TRUE, SWL_RC_OK, ME, "glob hapd not supported on vdr %s", pVdr->name);
    }
    bool enableGlobHapd = ((pCfg->useGlobalInstance == SWL_TRL_TRUE) ||
                           ((pCfg->useGlobalInstance == SWL_TRL_AUTO) && gHapd->globalDmnRequired));
    if(wld_secDmnGrp_isEnabled(gHapd->pGlobalDmnGrp) != enableGlobHapd) {
        if(!enableGlobHapd) {
            SAH_TRACEZ_INFO(ME, "drop all members of gHapd %s", pVdr->name);
            wld_secDmnGrp_dropMembers(gHapd->pGlobalDmnGrp);
        }
        T_Radio* pRad;
        wld_for_eachRad(pRad) {
            if(pRad->vendor == pVdr) {
                if(enableGlobHapd) {
                    if(wld_secDmnGrp_hasMember(gHapd->pGlobalDmnGrp, pRad->hostapd)) {
                        SAH_TRACEZ_INFO(ME, "member %s already added to gHapd %s", pRad->Name, pVdr->name);
                        continue;
                    }
                    SAH_TRACEZ_INFO(ME, "add member %s to gHapd %s", pRad->Name, pVdr->name);
                    wld_secDmn_addToGrp(pRad->hostapd, gHapd->pGlobalDmnGrp, pRad->Name);
                }
                SAH_TRACEZ_INFO(ME, "restart needed %s gHapd %s", pRad->Name, pVdr->name);
                restartNeeded = true;
            }
        }
    }
    return restartNeeded;
}

/**
 * @brief handle the conf changes of daemon execution settings, switching from single or multiple instance
 *
 * @param pVdr pointer to vendor context
 * @param pCfg pointer to daemon exection settings
 *
 * @return SWL_RC_OK on success, error code otherwise
 */
swl_rc_ne wifiGen_hapd_setGlobDmnSettings(vendor_t* pVdr, wld_dmnMgt_dmnExecSettings_t* pCfg) {
    ASSERT_NOT_NULL(pVdr, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pCfg, SWL_RC_INVALID_PARAM, ME, "NULL");

    bool restartNeeded = s_handleUseGlobalInstance(pVdr, pCfg);
    if(restartNeeded) {
        wifiGen_hapd_restartAllDaemons(pVdr);
    }
    return SWL_RC_OK;
}

swl_rc_ne wifiGen_hapd_initGlobDmnCap(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pRad->vendor, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_dmnMgt_initDmnExecInfo(&pRad->vendor->globalHostapd);
    wld_dmnMgt_dmnExecInfo_t* gHapd = pRad->vendor->globalHostapd;
    if(gHapd->globalDmnSupported == SWL_TRL_FALSE) {
        SAH_TRACEZ_INFO(ME, "%s: glob hapd %s not supported", pRad->Name, pRad->vendor->name);
        gHapd->globalDmnRequired = false;
        return SWL_RC_OK;
    }
    //global hostapd required for WiFi7 and WiFi6E (For RNR Mgmt)
    if((SWL_BIT_IS_SET(pRad->supportedStandards, SWL_RADSTD_BE)) ||
       ((SWL_BIT_IS_SET(pRad->supportedStandards, SWL_RADSTD_AX)) &&
        (SWL_BIT_IS_SET(pRad->supportedFrequencyBands, SWL_FREQ_BAND_EXT_6GHZ)))) {
        gHapd->globalDmnRequired = true;
    }
    //global hostapd assumed supported since WiFi6
    if(gHapd->globalDmnSupported == SWL_TRL_UNKNOWN) {
        if((gHapd->globalDmnRequired) ||
           (SWL_BIT_IS_SET(pRad->supportedStandards, SWL_RADSTD_AX))) {
            gHapd->globalDmnSupported = SWL_TRL_TRUE;
        }
    }
    SAH_TRACEZ_INFO(ME, "%s: glob hapd %s supp:%d, req:%d", pRad->Name, pRad->vendor->name, gHapd->globalDmnSupported, gHapd->globalDmnRequired);
    return SWL_RC_OK;
}

swl_rc_ne wifiGen_hapd_getConfiguredCountryCode(T_Radio* pRad, char* country, size_t countrySize) {
    ASSERTS_NOT_NULL(country, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_TRUE(countrySize > 0, SWL_RC_INVALID_PARAM, ME, "NULL");
    return wld_rad_hostapd_getCfgParamStr(pRad, "country_code", country, countrySize);
}
