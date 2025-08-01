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
#include "swl/swl_common.h"

#include "wld/wld_common.h"
#include "wld/wld_linuxIfUtils.h"
#include "wld/wld_rad_hostapd_api.h"
#include "wld/wld_hostapd_ap_api.h"
#include "wld/wld_hostapd_cfgFile.h"
#include "wld/wld_wpaSupp_ep_api.h"
#include "wld/wld_wpaCtrl_api.h"
#include "wld/wld_rad_nl80211.h"
#include "wld/wld_ap_nl80211.h"
#include "wld/wld_ep_nl80211.h"
#include "wld/wld_chanmgt.h"
#include "wld/wld_wpaCtrlGSock.h"
#include "wld/wld_secDmn.h"

#include "wifiGen_fsm.h"

#include "wifiGen_rad.h"
#include "wifiGen_vap.h"
#include "wifiGen_hapd.h"
#include "wifiGen_wpaSupp.h"
#include "wifiGen_events.h"
#include "wifiGen_zwdfs.h"
#include "wifiGen_mld.h"

#define ME "genFsm"

typedef struct {
    T_Radio* pRad;
    swl_chanspec_t chanspec;
} chanspecInfo_t;

/*
 * List of actions that can be used to apply hostapd configuration,
 * starting from the most global (most critical)
 */
static const wifiGen_fsmStates_e sApplyActions[] = {
    GEN_FSM_START_HOSTAPD,    // leads to restarting hostapd
    GEN_FSM_ADD_HOSTAPD,      // leads to re-adding hostapd iface config
    GEN_FSM_UPDATE_HOSTAPD,   // leads to refreshing hostpad with SIGHUP (reloads conf file)
    GEN_FSM_ENABLE_HOSTAPD,   // leads to enabling (usually toggling) hostapd main interface
    GEN_FSM_RELOAD_AP_SECKEY, // leads to reloading AP secret key conf from memory
    GEN_FSM_UPDATE_BEACON,    // leads to refresh AP beacon frame from mem conf
};
/*
 * List of params (fsm actions) that may be warm applied,
 * at runtime, without restarting hostapd
 */
static const wifiGen_fsmStates_e sDynConfActions[] = {
    GEN_FSM_MOD_SSID,
    GEN_FSM_ENABLE_AP,
    GEN_FSM_MOD_SEC,
    GEN_FSM_MOD_AP,
    GEN_FSM_MOD_MF_LIST,
};
/*
 * return lowest equivalent action in order to prevent
 * implicit skip of equivalent actions
 * eg: implicit skip of hostapd enabling when reloading config
 */
static wifiGen_fsmStates_e s_getLowestEqvAction(wifiGen_fsmStates_e action) {
    switch(action) {
    case GEN_FSM_UPDATE_HOSTAPD: return GEN_FSM_ENABLE_HOSTAPD;
    default: return action;
    }
}

/*
 * returns the index of the first met fsm action (among provided actionArray),
 * that is more global than the action argument (i.e: implicitly applying it)
 */
static int s_fetchAction(const wifiGen_fsmStates_e* actionArr, uint32_t actionArrSize,
                         wifiGen_fsmStates_e action) {
    ASSERTS_NOT_NULL(actionArr, -1, ME, "NULL");
    while(actionArrSize > 0) {
        if(actionArr[--actionArrSize] == action) {
            return actionArrSize;
        }
    }
    return -1;
}
/*
 * returns the enum id of the first met scheduled fsm action (among provided actionArray),
 * that is more global than the provided indexed action (i.e: implicitly applying it)
 */
static int s_fetchHigherAction(unsigned long* bitMapArr, uint32_t bitMapArrSize,
                               const wifiGen_fsmStates_e* actionArr, uint32_t actionArrSize,
                               int32_t minId) {
    ASSERTS_FALSE(minId < 0, -1, ME, "out of order");
    int selId = -1;
    for(int32_t i = minId; i >= 0 && i < (int32_t) actionArrSize; i--) {
        if(isBitSetLongArray(bitMapArr, bitMapArrSize, actionArr[i])) {
            selId = i;
        }
    }
    return selId;
}
/*
 * returns the enum id of the first met scheduled fsm action (among provided actionArray),
 * that is more global than the provided enumerated action (i.e: implicitly applying it)
 */
static int s_fetchHigherActionExt(unsigned long* bitMapArr, uint32_t bitMapArrSize,
                                  const wifiGen_fsmStates_e* actionArr, uint32_t actionArrSize,
                                  wifiGen_fsmStates_e minAction) {
    int pos = s_fetchAction(actionArr, actionArrSize, minAction);
    if(pos <= 0) {
        return -1;
    }
    do {
        //take the next higher action
        if(--pos < 0) {
            return -1;
        }
        //if still equivalent, then continue
    } while(s_getLowestEqvAction(actionArr[pos]) == minAction);
    return s_fetchHigherAction(bitMapArr, bitMapArrSize, actionArr, actionArrSize, pos);
}
/*
 * clear from fsm actions bitmap, all fsm actions (among action array) implicitly applied
 * by provided indexed action (index in action array)
 */
static void s_clearLowerActions(unsigned long* bitMapArr, uint32_t bitMapArrSize,
                                const wifiGen_fsmStates_e* actionArr, uint32_t actionArrSize,
                                int32_t minId) {
    for(int32_t i = minId; i >= 0 && i < (int32_t) actionArrSize; i++) {
        clearBitLongArray(bitMapArr, bitMapArrSize, actionArr[i]);
    }
}
/*
 * clear from fsm actions bitmap, all fsm actions (among action array) implicitly applied
 * by provided enumerated action
 */
static void s_clearLowerActionsExt(unsigned long* bitMapArr, uint32_t bitMapArrSize,
                                   const wifiGen_fsmStates_e* actionArr, uint32_t actionArrSize,
                                   wifiGen_fsmStates_e minAction) {
    int pos = s_fetchAction(actionArr, actionArrSize, minAction);
    ASSERTS_FALSE(pos < 0, , ME, "not found");
    s_clearLowerActions(bitMapArr, bitMapArrSize, actionArr, actionArrSize, pos + 1);
}
/*
 * fetch first met scheduled conf action implicitly applying fsm action
 */
static int s_fetchHigherApplyAction(unsigned long* bitMapArr, uint32_t bitMapArrSize, wifiGen_fsmStates_e action) {
    return s_fetchHigherActionExt(bitMapArr, bitMapArrSize, sApplyActions, SWL_ARRAY_SIZE(sApplyActions), action);
}
/*
 * clear conf applying fsm actions, that will be implicitly applied with than provided action
 */
static void s_clearLowerApplyActions(unsigned long* bitMapArr, uint32_t bitMapArrSize, wifiGen_fsmStates_e action) {
    action = s_getLowestEqvAction(action);
    s_clearLowerActionsExt(bitMapArr, bitMapArrSize, sApplyActions, SWL_ARRAY_SIZE(sApplyActions), action);
}
/*
 * schedule fsm conf applying action, if no more global actions are already scheduled.
 * If successful, less global fsm conf applying actions are cleared (because implicit)
 */
static bool s_setApplyAction(unsigned long* bitMapArr, uint32_t bitMapArrSize, wifiGen_fsmStates_e action) {
    if(s_fetchHigherApplyAction(bitMapArr, bitMapArrSize, action) >= 0) {
        return false;
    }
    setBitLongArray(bitMapArr, bitMapArrSize, action);
    s_clearLowerApplyActions(bitMapArr, bitMapArrSize, action);
    return true;
}
/*
 * fetch for scheduled conf applying fsm action
 */
static int s_fetchApplyAction(unsigned long* bitMapArr, uint32_t bitMapArrSize) {
    int32_t size = SWL_ARRAY_SIZE(sApplyActions);
    return s_fetchHigherAction(bitMapArr, bitMapArrSize, sApplyActions, size, size - 1);
}
/*
 * fetch for scheduled dynamically configurable param fsm action
 */
static int s_fetchDynConfAction(unsigned long* bitMapArr, uint32_t bitMapArrSize) {
    int32_t size = SWL_ARRAY_SIZE(sDynConfActions);
    return s_fetchHigherAction(bitMapArr, bitMapArrSize, sDynConfActions, size, size - 1);
}
/*
 * clear all dynamically configurable param fsm actions
 * This is used when the conf will be entirely applied by restarting hostapd
 */
static void s_clearDynConfActions(unsigned long* bitMapArr, uint32_t bitMapArrSize) {
    s_clearLowerActions(bitMapArr, bitMapArrSize, sDynConfActions, SWL_ARRAY_SIZE(sDynConfActions), 0);
}
/*
 * clear conf applying fsm actions, that will be implicitly applied with than provided action
 * in Rad/APs/EPs ACtive fsm bitmaps
 */
static void s_clearRadLowerApplyActions(T_Radio* pRad, wifiGen_fsmStates_e action) {
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: clear apply actions lower than %s",
                    pRad->Name,
                    pRad->vendor->fsmMngr->actionList[action].name);
    s_clearLowerApplyActions(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, action);
    T_AccessPoint* pAP = NULL;
    wld_rad_forEachAp(pAP, pRad) {
        s_clearLowerApplyActions(pAP->fsm.FSM_AC_BitActionArray, FSM_BW, action);
    }
    T_EndPoint* pEP = NULL;
    wld_rad_forEachEp(pEP, pRad) {
        s_clearLowerApplyActions(pEP->fsm.FSM_AC_BitActionArray, FSM_BW, action);
    }
}
/*
 * clear all dynamically configurable param fsm actions
 * in Rad/APs/EPs ACtive fsm bitmaps
 */
static void s_clearRadDynConfActions(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: clear all dyn conf actions", pRad->Name);
    s_clearDynConfActions(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW);
    T_AccessPoint* pAP = NULL;
    wld_rad_forEachAp(pAP, pRad) {
        s_clearDynConfActions(pAP->fsm.FSM_AC_BitActionArray, FSM_BW);
    }
    T_EndPoint* pEP = NULL;
    wld_rad_forEachEp(pEP, pRad) {
        s_clearDynConfActions(pEP->fsm.FSM_AC_BitActionArray, FSM_BW);
    }
}

/**
 * @brief check whether a radio has a number of usable(being configured)
 * or active(already running) apmld links
 *
 * @param pRad pointer to radio context
 * @param minNLinks minimum number of mlo links required in the check
 *
 * @return bool true when radio has at least minNLinks mlo links usable or active
 *              false otherwise (no mlo support or less links that requested)
 */
static bool s_checkApMld(T_Radio* pRad, uint32_t minNLinks) {
    return (wld_rad_isMloCapable(pRad) &&
            (wld_rad_hasUsableApMld(pRad, minNLinks) ||
             wld_rad_hostapd_hasActiveApMld(pRad, minNLinks)));
}

/**
 * @brief check whether a radio has a bss involved in multi-link apmld (ie at least 2 links)
 * This allows to detect whether a radio reconf may concern mlo neighbor links on other radios
 *
 * @param pRad pointer to radio context
 *
 * @return bool true when radio has at least 2 mlo links usable or active
 *              false otherwise (no mlo support or less links that requested)
 */
static bool s_hasMultiLinkApMld(T_Radio* pRad) {
    return s_checkApMld(pRad, 2);
}

/**
 * @brief check whether the radio AND one of its APs have a provided scheduled action
 * in their ACtive bitmaps (ie actions up to be executed)
 *
 * @param pRad pointer to radio context
 * @param action id of action to checked in the ACtive bitmap
 *
 * @return bool true when action is found in ACtive bitmaps
 *              false otherwise
 */
static bool s_checkApAcAction(T_Radio* pRad, wifiGen_fsmStates_e action) {
    if(isBitSetLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, action)) {
        T_AccessPoint* pAP = NULL;
        wld_rad_forEachAp(pAP, pRad) {
            if(isBitSetLongArray(pAP->fsm.FSM_AC_BitActionArray, FSM_BW, action)) {
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief clear a provided action from all radio's APs ACtive bitmaps
 *
 * @param pRad pointer to radio context
 * @param action id of action to be cleared
 *
 * @return void
 */
static void s_clearApAcAction(T_Radio* pRad, wifiGen_fsmStates_e action) {
    T_AccessPoint* pAP = NULL;
    wld_rad_forEachAp(pAP, pRad) {
        clearBitLongArray(pAP->fsm.FSM_AC_BitActionArray, FSM_BW, action);
    }
}

/**
 * @brief check whether wpactrl cmd "LINK_REMOVE" is supported by radio's hostapd instance:
 * when not yet learned, the checking is done in the hostapd help output message
 *
 * @param pRad pointer to radio context
 *
 * @return bool true when wpactrl cmd "LINK_REMOVE" is supported
 *              false otherwise
 */
static bool s_isLinkRemoveSupported(T_Radio* pRad) {
    swl_trl_e supp = wld_secDmn_getCmdSupp(pRad->hostapd, "LINK_REMOVE");
    if(supp == SWL_TRL_UNKNOWN) {
        if(wld_secDmn_learnCmdSuppFromHelpMsg("hostapd_cli", "-h", "link_remove", &supp)) {
            wld_secDmn_setCmdSupp(pRad->hostapd, "LINK_REMOVE", supp);
            SAH_TRACEZ_INFO(ME, "%s: LINK_REMOVE supp(%d)", pRad->Name, supp);
        }
    }
    return (supp == SWL_TRL_TRUE);
}

/*
 * schedule next conf applying fsm action after setting a dynamic param
 */
static void s_schedNextAction(wld_secDmn_action_rc_ne action, T_AccessPoint* pAP, T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    switch(action) {
    case SECDMN_ACTION_OK_NEED_RESTART:
        if(s_setApplyAction(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_START_HOSTAPD)) {
            setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_STOP_HOSTAPD);
            //clear remaining dyn conf actions, as hostapd is up to be restarted
            s_clearRadLowerApplyActions(pRad, GEN_FSM_START_HOSTAPD);
            s_clearRadDynConfActions(pRad);
            wld_secDmn_setRestartNeeded(pRad->hostapd, true);
        }
        break;
    case SECDMN_ACTION_OK_NEED_RE_ADD:
        s_setApplyAction(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_ADD_HOSTAPD);
        setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_REMOVE_HOSTAPD);
        //clear remaining dyn conf actions, as hostapd conf is up to be re-added
        s_clearRadLowerApplyActions(pRad, GEN_FSM_ADD_HOSTAPD);
        s_clearRadDynConfActions(pRad);
        break;
    case SECDMN_ACTION_OK_NEED_RE_ADD_LINK:
        /*
         * if LINK_REMOVE is not supported, then turn action RE_ADD_LINK into RESTART
         */
        if(!s_isLinkRemoveSupported(pRad)) {
            SAH_TRACEZ_WARNING(ME, "%s: link remove not supported => need to restart isof re-add link", pRad->Name);
            s_schedNextAction(SECDMN_ACTION_OK_NEED_RESTART, pAP, pRad);
            return;
        }
        s_schedNextAction(SECDMN_ACTION_OK_NEED_RE_ADD, pAP, pRad);
        break;
    case SECDMN_ACTION_OK_NEED_TOGGLE:
        /*
         * convert toggle into restart, when rad has ap member of MultiLink MLO (current or expected):
         * links from other radios have to rebuilt
         */
        if(s_hasMultiLinkApMld(pRad)) {
            SAH_TRACEZ_WARNING(ME, "%s: 11be/mlo enabled => need to restart isof toggle", pRad->Name);
            s_schedNextAction(SECDMN_ACTION_OK_NEED_RESTART, pAP, pRad);
            return;
        }
        if(s_setApplyAction(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_ENABLE_HOSTAPD)) {
            setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_DISABLE_HOSTAPD);
        }
        break;
    case SECDMN_ACTION_OK_NEED_SIGHUP:
        s_setApplyAction(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_UPDATE_HOSTAPD);
        break;
    case SECDMN_ACTION_OK_NEED_RELOAD_SECKEY:
        if(s_fetchHigherApplyAction(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_RELOAD_AP_SECKEY) >= 0) {
            return;
        }
        if(s_setApplyAction(pAP->fsm.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_RELOAD_AP_SECKEY)) {
            setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_RELOAD_AP_SECKEY);
            setBitLongArray(pAP->fsm.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_UPDATE_BEACON);
            setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_UPDATE_BEACON);
        }
        break;
    case SECDMN_ACTION_OK_NEED_UPDATE_BEACON:
        if(s_fetchHigherApplyAction(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_RELOAD_AP_SECKEY) >= 0) {
            return;
        }
        if(s_setApplyAction(pAP->fsm.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_UPDATE_BEACON)) {
            setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_UPDATE_BEACON);
        }
        break;
    default:
        return;
    }
}

static bool s_doEnableAp(T_AccessPoint* pAP, T_Radio* pRad) {
    ASSERTI_TRUE(pRad->enable, true, ME, "%s: radio disabled", pRad->Name);
    ASSERTI_TRUE(wifiGen_hapd_isAlive(pRad), true, ME, "%s: hostapd stopped", pRad->Name);
    T_AccessPoint* pMainAPCur = wld_rad_hostapd_getRunMainVap(pRad);
    ASSERT_NOT_NULL(pMainAPCur, true, ME, "%s: no main vap iface for rad %s", pAP->alias, pRad->Name);
    bool enable = wld_ap_isEnabledWithRef(pAP);
    SAH_TRACEZ_INFO(ME, "%s: enable vap %d", pAP->alias, enable);
    wld_secDmn_action_rc_ne rc;
    T_AccessPoint* pMainAPCfg = wld_rad_hostapd_getCfgMainVap(pRad);
    bool mainIfaceChanged = ((pMainAPCur != pMainAPCfg) && ((pMainAPCur == NULL) || (pAP == pMainAPCur) || (pAP == pMainAPCfg)));
    bool wpaCtrlEnaChanged = (wld_wpaCtrlInterface_checkConnectionPath(pAP->wpaCtrlInterface) != wld_hostapd_ap_needWpaCtrlIface(pAP));
    if(mainIfaceChanged || wpaCtrlEnaChanged) {
        wifiGen_hapd_enableVapWpaCtrlIface(pAP);
        if(mainIfaceChanged) {
            SAH_TRACEZ_WARNING(ME, "%s: Main iface changed: sched re-add radio %s", pAP->alias, pRad->Name);
            s_schedNextAction(SECDMN_ACTION_OK_NEED_RE_ADD, pAP, pRad);
        } else if(s_hasMultiLinkApMld(pRad)) {
            SAH_TRACEZ_INFO(ME, "%s: has multi-band APMLD: need to restart hostapd", pAP->alias);
            s_schedNextAction(SECDMN_ACTION_OK_NEED_RE_ADD_LINK, pAP, pRad);
        } else {
            SAH_TRACEZ_WARNING(ME, "%s: sched reload all radio %s VAPs to update wpaCtrl ifaces", pAP->alias, pRad->Name);
            s_schedNextAction(SECDMN_ACTION_OK_NEED_SIGHUP, pAP, pRad);
            setBitLongArray(pRad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_SYNC_STATE);
        }
        return true;
    }
    ASSERTI_TRUE(wld_wpaCtrlInterface_isReady(pAP->wpaCtrlInterface), true, ME, "%s: wpaCtrl disconnected", pAP->alias);
    /*
     * first, set dyn ena/disabling vap params, before applying any action
     */
    if((rc = wld_ap_hostapd_setEnableVap(pAP, enable)) < SECDMN_ACTION_OK_DONE) {
        SAH_TRACEZ_ERROR(ME, "%s: fail to save enable %d rc %d", pAP->alias, enable, rc);
    }
    if(!enable) {
        wld_ap_hostapd_deauthAllStations(pAP);
        rc = wld_ap_hostapd_enableVap(pAP, false);
    } else {
        int currState = pAP->pFA->mfn_wvap_enable(pAP, enable, GET | DIRECT);
        if(!currState) {
            // we have to re-enable bss net interface before trying to (re)start beaconing
            pAP->pFA->mfn_wvap_enable(pAP, enable, SET | DIRECT);
        }
        if(((rc = wld_ap_hostapd_enableVap(pAP, true)) < SECDMN_ACTION_OK_DONE) && (!currState)) {
            //if failed to start beaconing, restore the net iface disabled state.
            pAP->pFA->mfn_wvap_enable(pAP, currState, SET | DIRECT);
        }
    }
    setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_SYNC_STATE);
    wld_vap_updateState(pAP);
    ASSERT_FALSE(rc < SECDMN_ACTION_OK_DONE, true, ME, "%s: fail to apply enable %d", pAP->alias, enable);
    s_schedNextAction(rc, pAP, pRad);
    return true;
}

static bool s_doRadDisable(T_Radio* pRad) {
    SAH_TRACEZ_INFO(ME, "%s: disable rad", pRad->Name);
    pRad->pFA->mfn_wrad_enable(pRad, false, SET | DIRECT);
    return true;
}

static bool s_doRadEnable(T_Radio* pRad) {
    ASSERTS_TRUE(pRad->enable, true, ME, "%s: rad disabled", pRad->Name);
    bool upperEnabled = (wld_rad_hasEnabledEp(pRad));
    // When VAPs are enabled, Rad enable requires no config
    // as Hostapd takes care of enabling required interfaces
    ASSERTS_TRUE(upperEnabled, true, ME, "%s: rad has no upperL enabled", pRad->Name);
    pRad->pFA->mfn_wrad_enable(pRad, true, SET | DIRECT);
    return true;
}

static bool s_doRadSync(T_Radio* pRad) {
    SAH_TRACEZ_INFO(ME, "%s: sync rad conf", pRad->Name);
    pRad->pFA->mfn_wrad_sync(pRad, SET | DIRECT);
    if(wifiGen_hapd_isAlive(pRad)) {
        wld_rad_hostapd_setMiscParams(pRad);
        wld_rad_hostapd_setChannel(pRad);
    }
    return true;
}

static bool s_doStopHostapd(T_Radio* pRad) {
    ASSERTI_TRUE(wifiGen_hapd_isRunning(pRad), true, ME, "%s: hapd stopped", pRad->Name);

    /*
     * as we are stopping radio, no need to wait for deauth notif
     * so we can cleanup ap's AD list
     */
    wifiGen_hapd_deauthKnownStations(pRad, true);
    wld_scan_stop(pRad);

    swl_rc_ne rc;
    if(wld_secDmn_checkRestartNeeded(pRad->hostapd)) {
        SAH_TRACEZ_INFO(ME, "%s: restarting hostapd", pRad->Name);
        if(!wld_secDmn_isEnabled(pRad->hostapd)) {
            //force restart of expected group process, by starting member
            wld_secDmn_start(pRad->hostapd);
            //no need for wpactrl connection when restarting
            wld_wpaCtrlMngr_stopConnecting(wld_secDmn_getWpaCtrlMgr(pRad->hostapd));
        }
        rc = wld_secDmn_restart(pRad->hostapd);
        SAH_TRACEZ_INFO(ME, "%s: restart hostapd returns rc : %d", pRad->Name, rc);
        return true;
    }
    if(wld_secDmn_isRestarting(pRad->hostapd)) {
        SAH_TRACEZ_WARNING(ME, "%s: hostapd instance is restarting: let's stop afterwards", pRad->Name);
        return true;
    }

    bool hasApMldAction = s_checkApAcAction(pRad, GEN_FSM_MOD_MLD);
    bool hasApMldMultiLink = s_hasMultiLinkApMld(pRad);
    if(!wifiGen_hapd_isStarted(pRad)) {
        SAH_TRACEZ_INFO(ME, "%s: hostapd instance not started", pRad->Name);
        /*
         * if hostapd instance is being started,
         * with having Apmld reconf action for a non multi-link,
         * then the start action will override the mlo reconf action
         */
        if(isBitSetLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_START_HOSTAPD) &&
           hasApMldAction && !hasApMldMultiLink) {
            SAH_TRACEZ_WARNING(ME, "%s: skip mld handler as no multilink ", pRad->Name);
            s_clearApAcAction(pRad, GEN_FSM_MOD_MLD);
        }
        return true;
    }
    SAH_TRACEZ_INFO(ME, "%s: stop hostapd", pRad->Name);
    rc = wifiGen_hapd_stopDaemon(pRad);
    SAH_TRACEZ_INFO(ME, "%s: stop hostapd returns rc : %d", pRad->Name, rc);
    /*
     * stop request accepted but delayed (hostapd still used by other radios)
     * Therefore, hapd rad iface need to be disabled to force stop all vaps
     */
    if((rc == SWL_RC_CONTINUE) && (wifiGen_hapd_countGrpMembers(pRad) > 1)) {
        /*
         * if hostapd instance is being stopped,
         * with having Apmld reconf action for a non multi-link,
         * then the stop action will override the mlo reconf action
         */
        if(hasApMldAction) {
            if(hasApMldMultiLink) {
                SAH_TRACEZ_INFO(ME, "%s: let mld handler manage radio disabling", pRad->Name);
                wld_rad_fsm_clearFsmBitForAll(pRad, GEN_FSM_DISABLE_HOSTAPD);
                return true;
            }
            SAH_TRACEZ_WARNING(ME, "%s: skip mld handler as no multilink ", pRad->Name);
            s_clearApAcAction(pRad, GEN_FSM_MOD_MLD);
        }
        if(wld_secDmn_hasReadyGlobalCtrlIface(pRad->hostapd)) {
            SAH_TRACEZ_INFO(ME, "%s: need to remove config", pRad->Name);
            setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_REMOVE_HOSTAPD);
            return true;
        }
        SAH_TRACEZ_INFO(ME, "%s: need to disable hostapd", pRad->Name);
        setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_DISABLE_HOSTAPD);
    }
    pRad->fsmRad.timeout_msec = 100;
    return true;
}

static bool s_doConfHostapd(T_Radio* pRad) {
    SAH_TRACEZ_INFO(ME, "%s: write hostapd conf", pRad->Name);
    wifiGen_hapd_writeConfig(pRad);
    return true;
}

static bool s_doUpdateHostapd(T_Radio* pRad) {
    ASSERTI_TRUE(wifiGen_hapd_isRunning(pRad), true, ME, "%s: hostapd stopped", pRad->Name);
    ASSERTI_TRUE(wifiGen_hapd_isStarted(pRad), true, ME, "%s: hostapd instance not started", pRad->Name);
    if(wld_secDmn_isGrpRestarting(pRad->hostapd)) {
        SAH_TRACEZ_WARNING(ME, "%s: hostapd group is restarting: skip immediate update", pRad->Name);
        return true;
    }

    wifiGen_hapd_restoreMainIface(pRad);
    SAH_TRACEZ_INFO(ME, "%s: reload hostapd", pRad->Name);
    wifiGen_hapd_reloadDaemon(pRad);
    wld_wpaCtrlMngr_connect(wld_secDmn_getWpaCtrlMgr(pRad->hostapd));
    //delay before restore warm applicable params after reloading conf file with sighup
    pRad->fsmRad.timeout_msec = 500;
    return true;
}

static void s_syncFixedChannel(chanspecInfo_t* pUserConf) {
    ASSERTS_NOT_NULL(pUserConf, , ME, "NULL");
    swl_chanspec_t userChanspec = pUserConf->chanspec;
    T_Radio* pRad = pUserConf->pRad;
    free(pUserConf);
    ASSERTI_FALSE(swl_typeChanspec_equals(pRad->targetChanspec.chanspec, userChanspec), , ME, "%s: Chanspec equality %s", pRad->Name, swl_typeChanspecExt_toBuf32(userChanspec).buf);
    SAH_TRACEZ_WARNING(ME, "%s: restore user config %s", pRad->Name, swl_typeChanspecExt_toBuf32(userChanspec).buf);
    wld_chanmgt_setTargetChanspec(pRad, userChanspec, false, CHAN_REASON_MANUAL, NULL);
}

static void s_syncOnRadUp(void* userData, char* ifName, bool state) {
    T_Radio* pRad = (T_Radio*) userData;
    T_AccessPoint* pAP = wld_rad_vap_from_name(pRad, ifName);
    ASSERT_NOT_NULL(pAP, , ME, "NULL");

    bool needCommit = (pRad->fsmRad.FSM_State != FSM_RUN);
    unsigned long* actionArray = (needCommit ? pRad->fsmRad.FSM_BitActionArray : pRad->fsmRad.FSM_AC_BitActionArray);

    if(state != wifiGen_hapd_isStartable(pRad)) {
        if(state) {
            SAH_TRACEZ_WARNING(ME, "%s: disable hostapd iface %s not expected to start", pRad->Name, ifName);
            setBitLongArray(pRad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_DISABLE_HOSTAPD);
        } else if(pRad->detailedState != CM_RAD_DOWN) {
            SAH_TRACEZ_WARNING(ME, "%s: hostapd connected but main iface not yet ready", pRad->Name);
            return;
        } else {
            SAH_TRACEZ_WARNING(ME, "%s: enable hostapd iface %s expected to start", pRad->Name, ifName);
            setBitLongArray(pRad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_ENABLE_HOSTAPD);
        }
        if(wld_rad_hasMloSupport(pRad)) {
            T_AccessPoint* pTmpAP = NULL;
            wld_rad_forEachAp(pTmpAP, pRad) {
                if(!pTmpAP->pSSID || wld_vap_isDummyVap(pTmpAP)) {
                    continue;
                }
                if(!state && !wld_mld_isLinkUsable(pTmpAP->pSSID->pMldLink)) {
                    continue;
                }
                setBitLongArray(pTmpAP->fsm.FSM_BitActionArray, FSM_BW, GEN_FSM_MOD_MLD);
            }
        }
        wld_rad_doCommitIfUnblocked(pRad);
        return;
    }

    // check and apply dyn detected cfg params
    char unkSupParamsStr[512] = {0};
    uint32_t nUnkSup = 0;
    if((nUnkSup = wld_secDmn_getCfgParamsListBySuppVal(pRad->hostapd, unkSupParamsStr, sizeof(unkSupParamsStr), SWL_TRL_UNKNOWN)) > 0) {
        uint32_t nDetSup = 0;
        char* unkSupParamsArray[nUnkSup];
        memset(unkSupParamsArray, 0, sizeof(unkSupParamsArray));
        swl_typeCharPtr_arrayFromCharSep(unkSupParamsArray, SWL_ARRAY_SIZE(unkSupParamsArray), unkSupParamsStr, ",");
        SAH_TRACEZ_INFO(ME, "%s: try to detect and apply dyn cfg params", pRad->Name);
        wifiGen_hapd_writeConfig(pRad);
        for(uint32_t i = 0; i < nUnkSup; i++) {
            swl_trl_e trl = wld_secDmn_getCfgParamSupp(pRad->hostapd, unkSupParamsArray[i]);
            wld_secDmn_setCfgParamSupp(pRad->hostapd, unkSupParamsArray[i], trl);
            nDetSup += (trl == SWL_TRL_TRUE);
        }
        swl_typeCharPtr_arrayCleanup(unkSupParamsArray, nUnkSup);
        if(nDetSup > 0) {
            wifiGen_hapd_reloadDaemon(pRad);
        }
    }

    if(!pRad->autoChannelEnable
       && (pRad->userChanspec.channel != 0)
       && pRad->externalAcsMgmt && !pRad->autoChannelSetByUser
       && ((pRad->channelShowing == CHANNEL_INTERNAL_STATUS_SYNC) || (pRad->channelShowing == CHANNEL_INTERNAL_STATUS_TARGET))) {
        chanspecInfo_t* userChanspec = calloc(1, sizeof(chanspecInfo_t));
        if(userChanspec != NULL) {
            userChanspec->pRad = pRad;
            userChanspec->chanspec = pRad->userChanspec;
            swla_delayExec_addTimeout((swla_delayExecFun_cbf) s_syncFixedChannel, userChanspec, 100);
        }
    }

    setBitLongArray(actionArray, FSM_BW, GEN_FSM_SYNC_STATE);
    if(needCommit) {
        wld_rad_doCommitIfUnblocked(pRad);
    }
}

static bool s_checkHapdDown(T_Radio* pRad) {
    chanmgt_rad_state radDetState = CM_RAD_UNKNOWN;
    swl_rc_ne rc = wifiGen_hapd_getRadState(pRad, &radDetState);
    return ((!swl_rc_isOk(rc)) || (radDetState == CM_RAD_DOWN));
}

static bool s_checkHapdActive(T_Radio* pRad) {
    chanmgt_rad_state radDetState = CM_RAD_UNKNOWN;
    swl_rc_ne rc = wifiGen_hapd_getRadState(pRad, &radDetState);
    return ((swl_rc_isOk(rc)) && (radDetState != CM_RAD_DOWN));
}

static void s_disableFronthaul(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    if(wld_scan_isRunning(pRad)) {
        wld_scan_stop(pRad);
    }
    ASSERTS_TRUE(wifiGen_hapd_isAlive(pRad), , ME, "%s: hapd not running", pRad->Name);
    if(s_checkHapdActive(pRad)) {
        // disable BSSs to allow wpa_supplicant to do scan
        SAH_TRACEZ_INFO(ME, "%s: disable hostapd ifaces to let wpa_supplicant take the control", pRad->Name);
        setBitLongArray(pRad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_DISABLE_HOSTAPD);
        wld_rad_doCommitIfUnblocked(pRad);
    }
}

static void s_restoreFronthaul(T_Radio* pRad) {
    if(wifiGen_hapd_isAlive(pRad) && s_checkHapdDown(pRad)) {
        wld_epConnectionStatus_e connState = EPCS_DISABLED;
        wld_wpaSupp_ep_getConnState(wld_rad_getEnabledEndpoint(pRad), &connState);
        if((connState == EPCS_IDLE) || (connState == EPCS_DISABLED) ||
           (connState == EPCS_CONNECTED) || (connState == EPCS_DISCONNECTED)) {
            SAH_TRACEZ_INFO(ME, "%s: restore fronthaul chanspec conf", pRad->Name);
            pRad->pFA->mfn_wrad_setChanspec(pRad, true);
        }
    }
}

static void s_delayRestoreFronthaul(T_Radio* pRad) {
    swla_delayExec_addTimeout((swla_delayExecFun_cbf) s_restoreFronthaul, pRad, 1500);
}

static void s_syncOnEpRadUp(void* userData, char* ifName, bool state _UNUSED) {
    T_Radio* pRad = (T_Radio*) userData;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    T_EndPoint* pEP = wld_rad_ep_from_name(pRad, ifName);
    ASSERT_NOT_NULL(pEP, , ME, "NULL");
}

static void s_registerHadpRadEvtHandlers(wld_secDmn_t* hostapd) {
    ASSERT_NOT_NULL(hostapd, , ME, "no hostapd ctx");
    void* userdata = NULL;
    wld_wpaCtrl_radioEvtHandlers_cb handlers = {0};
    if(wld_wpaCtrlMngr_getEvtHandlers(hostapd->wpaCtrlMngr, &userdata, &handlers)) {
        handlers.fSyncOnRadioUp = s_syncOnRadUp;
        wld_wpaCtrlMngr_setEvtHandlers(hostapd->wpaCtrlMngr, userdata, &handlers);
    }
}

static bool s_doStartHostapd(T_Radio* pRad) {
    wld_secDmn_setRestartNeeded(pRad->hostapd, false);
    ASSERTI_TRUE(wifiGen_hapd_isStartable(pRad), true, ME, "%s: missing enabling conds", pRad->Name);
    if(wld_secDmn_isRestarting(pRad->hostapd)) {
        SAH_TRACEZ_INFO(ME, "%s: hostapd already is restarting: no need to force immediate start", pRad->Name);
        return true;
    }
    if(wifiGen_hapd_isStarted(pRad) && wld_secDmn_hasAvailableCtrlIface(pRad->hostapd)) {
        SAH_TRACEZ_INFO(ME, "%s: hostapd already started", pRad->Name);
        return true;
    }
    SAH_TRACEZ_INFO(ME, "%s: start hostapd", pRad->Name);
    swl_rc_ne rc = wifiGen_hapd_startDaemon(pRad);
    SAH_TRACEZ_INFO(ME, "%s: start hostapd returns rc : %d", pRad->Name, rc);
    /*
     * start request accepted but process already started ie (hostapd already used by other radios)
     * Therefore, hapd rad iface need to be enabled to force starting main rad iface and vaps
     */
    if((rc == SWL_RC_DONE) && (wifiGen_hapd_countGrpMembers(pRad) > 1)) {
        wld_wpaCtrlMngr_t* pMgr = wld_secDmn_getWpaCtrlMgr(pRad->hostapd);
        if(!wld_wpaCtrlMngr_isConnected(pMgr)) {
            wld_wpaCtrlMngr_checkAllIfaces(pMgr);
        }
        chanmgt_rad_state detState = CM_RAD_UNKNOWN;
        if(wld_secDmn_hasGlobalCtrlIface(pRad->hostapd)) {
            if(wld_secDmn_hasAvailableCtrlIface(pRad->hostapd)) {
                SAH_TRACEZ_WARNING(ME, "%s: need to remove old conf and add new one", pRad->Name);
                s_schedNextAction(SECDMN_ACTION_OK_NEED_RE_ADD, NULL, pRad);
            } else {
                SAH_TRACEZ_WARNING(ME, "%s: need to add config", pRad->Name);
                setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_ADD_HOSTAPD);
            }
        } else if(swl_rc_isOk(wifiGen_hapd_getRadState(pRad, &detState)) && (detState == CM_RAD_DOWN)) {
            SAH_TRACEZ_INFO(ME, "%s: need to enable hostapd", pRad->Name);
            wld_rad_hostapd_enable(pRad);
            //update hostapd conf to consider changed configs while it was disabled
            //+reconnect wpactrlMgr to update wpactrl connections (of added/removed BSSs)
            wld_wpaCtrlMngr_disconnect(pMgr);
            setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_UPDATE_HOSTAPD);
        }
    }

    s_registerHadpRadEvtHandlers(pRad->hostapd);
    if(rc == SWL_RC_OK) {
        pRad->fsmRad.timeout_msec = 500;
    }
    return true;
}

/**
 * @brief fsm execution handler for action GEN_FSM_ADD_HOSTAPD
 *
 * @param pRad pointer to radio context
 *
 * @param bool true when action is handled
 *             false when action is not handled and needs to be retried later
 */
static bool s_doAddHostapd(T_Radio* pRad) {
    ASSERTS_TRUE(wifiGen_hapd_isRunning(pRad), true, ME, "%s: hostapd stopped", pRad->Name);
    ASSERTI_TRUE(wifiGen_hapd_isStartable(pRad), true, ME, "%s: missing enabling conds", pRad->Name);
    if(wld_secDmn_isGrpRestarting(pRad->hostapd)) {
        SAH_TRACEZ_WARNING(ME, "%s: hostapd group is restarting: skip immediate adding conf", pRad->Name);
        return true;
    }
    wld_wpaCtrlInterface_t* pgIface = wld_secDmn_getGlobalCtrlIface(pRad->hostapd);
    ASSERTW_TRUE(wld_wpaCtrlInterface_isReady(pgIface), true, ME, "%s: gsock disconnected", pRad->Name);
    wifiGen_hapd_restoreMainIface(pRad);
    wifiGen_hapd_enableWpaCtrlIfaces(pRad);
    SAH_TRACEZ_INFO(ME, "%s: add config to hostapd", pRad->Name);
    if(wld_rad_hostapd_addConf(pRad) < 0) {
        SAH_TRACEZ_ERROR(ME, "%s: warm apply failed to add iface conf => restart to recover", pRad->Name);
        s_schedNextAction(SECDMN_ACTION_OK_NEED_RESTART, NULL, pRad);
        return true;
    }
    wld_wpaCtrlMngr_connect(wld_secDmn_getWpaCtrlMgr(pRad->hostapd));
    pRad->fsmRad.timeout_msec = 100;
    return true;
}

/**
 * @brief fsm execution handler for action GEN_FSM_REMOVE_HOSTAPD
 *
 * @param pRad pointer to radio context
 *
 * @param bool true when action is handled
 *             false when action is not handled and needs to be retried later
 */
static bool s_doRemoveHostapd(T_Radio* pRad) {
    ASSERTS_TRUE(wifiGen_hapd_isRunning(pRad), true, ME, "%s: hostapd stopped", pRad->Name);
    if(wld_secDmn_isGrpRestarting(pRad->hostapd)) {
        SAH_TRACEZ_WARNING(ME, "%s: hostapd group is restarting: skip immediate adding conf", pRad->Name);
        return true;
    }
    wld_wpaCtrlInterface_t* pgIface = wld_secDmn_getGlobalCtrlIface(pRad->hostapd);
    ASSERTW_TRUE(wld_wpaCtrlInterface_isReady(pgIface), true, ME, "%s: gsock disconnected", pRad->Name);
    wld_wpaCtrlMngr_t* pMgr = wld_secDmn_getWpaCtrlMgr(pRad->hostapd);
    ASSERTI_NOT_NULL(wld_wpaCtrlMngr_getFirstAvailableInterface(pMgr), true, ME, "%s: no conf has been added yet", pRad->Name);
    SAH_TRACEZ_INFO(ME, "%s: remove config from hostapd", pRad->Name);
    if(wld_rad_hostapd_removeConf(pRad) < 0) {
        SAH_TRACEZ_ERROR(ME, "%s: warm apply failed to remove iface conf => restart to recover", pRad->Name);
        s_schedNextAction(SECDMN_ACTION_OK_NEED_RESTART, NULL, pRad);
        return true;
    }
    pRad->fsmRad.timeout_msec = 500;
    return true;
}

static bool s_doDisableHostapd(T_Radio* pRad) {
    ASSERTS_TRUE(wifiGen_hapd_isRunning(pRad), true, ME, "%s: hostapd stopped", pRad->Name);
    ASSERTI_FALSE(s_checkHapdDown(pRad), true, ME, "%s: hapd iface already disabled", pRad->Name);
    if(wld_secDmn_isGrpRestarting(pRad->hostapd)) {
        SAH_TRACEZ_WARNING(ME, "%s: hostapd group is restarting: skip immediate disabling", pRad->Name);
        return true;
    }
    /*
     * as we are stopping/toggling radio, no need to wait for deauth notif
     * so we can cleanup ap's AD list
     */
    wifiGen_hapd_deauthKnownStations(pRad, true);
    wld_rad_hostapd_disable(pRad);
    pRad->fsmRad.timeout_msec = 500;
    return true;
}

static bool s_doEnableHostapd(T_Radio* pRad) {
    ASSERTS_TRUE(wifiGen_hapd_isAlive(pRad), true, ME, "%s: hostapd stopped", pRad->Name);
    if(wld_secDmn_isGrpRestarting(pRad->hostapd)) {
        SAH_TRACEZ_WARNING(ME, "%s: hostapd group is restarting: skip immediate enabling", pRad->Name);
        return true;
    }
    wld_rad_hostapd_enable(pRad);
    pRad->fsmRad.timeout_msec = 500;
    return true;
}

static bool s_doSetCountryCode(T_Radio* pRad) {
    pRad->pFA->mfn_wrad_regdomain(pRad, NULL, 0, SET | DIRECT);
    //Here radio shall be down: so toggle shortly to force reloading nl80211 wiphy channels
    if(wld_linuxIfUtils_getState(wld_rad_getSocket(pRad), pRad->Name) == 0) {
        wld_linuxIfUtils_setState(wld_rad_getSocket(pRad), pRad->Name, 1);
        wld_linuxIfUtils_setState(wld_rad_getSocket(pRad), pRad->Name, 0);
    }
    pRad->pFA->mfn_wrad_poschans(pRad, NULL, 0);
    if(pRad->operatingChannelBandwidth == SWL_RAD_BW_AUTO) {
        SAH_TRACEZ_INFO(ME, "%s: refresh target chanspec with autobw after regdomain setting", pRad->Name);
        swl_chanspec_t chanspec = swl_chanspec_fromDm(wld_chanmgt_getTgtChannel(pRad), pRad->operatingChannelBandwidth, pRad->operatingFrequencyBand);
        wld_chanmgt_setTargetChanspec(pRad, chanspec, true, pRad->targetChanspec.reason, pRad->targetChanspec.reasonExt);
    }
    return true;
}

static bool s_doSetBssid(T_AccessPoint* pAP, T_Radio* pRad _UNUSED) {
    ASSERTI_TRUE(pAP && pAP->pSSID && !swl_mac_binIsNull((swl_macBin_t*) pAP->pSSID->BSSID),
                 true, ME, "Null mac");
    SAH_TRACEZ_INFO(ME, "%s: set bssid", pAP->alias);
    wifiGen_vap_setBssid(pAP);
    return true;
}

static bool s_doSetSsid(T_AccessPoint* pAP, T_Radio* pRad _UNUSED) {
    ASSERTS_NOT_NULL(pAP, true, ME, "NULL");
    ASSERTI_TRUE(wld_wpaCtrlInterface_isReady(pAP->wpaCtrlInterface), true, ME, "%s: wpaCtrl disconnected", pAP->alias);
    T_SSID* pSSID = pAP->pSSID;
    ASSERTS_NOT_NULL(pSSID, true, ME, "%s: no ssid ctx", pAP->alias);
    SAH_TRACEZ_INFO(ME, "%s: set ssid (%s)", pSSID->Name, pSSID->SSID);
    wld_secDmn_action_rc_ne rc = wld_ap_hostapd_setSsid(pAP, pSSID->SSID);
    ASSERT_FALSE(rc < SECDMN_ACTION_OK_DONE, true, ME, "%s: fail to set ssid", pSSID->Name);
    s_schedNextAction(rc, pAP, pRad);
    return true;
}

static bool s_doSetMfList(T_AccessPoint* pAP, T_Radio* pRad _UNUSED) {
    ASSERTS_NOT_NULL(pAP, true, ME, "NULL");
    ASSERTI_TRUE(wld_wpaCtrlInterface_isReady(pAP->wpaCtrlInterface), true, ME, "%s: wpaCtrl disconnected", pAP->alias);
    SAH_TRACEZ_INFO(ME, "%s: sync ACL", pAP->alias);
    wld_ap_hostapd_setMacFilteringList(pAP);
    return true;
}

static bool s_doSetApSec(T_AccessPoint* pAP, T_Radio* pRad _UNUSED) {
    ASSERTS_NOT_NULL(pAP, true, ME, "NULL");
    ASSERTI_TRUE(wld_wpaCtrlInterface_isReady(pAP->wpaCtrlInterface), true, ME, "%s: wpaCtrl disconnected", pAP->alias);
    wld_ap_hostapd_sendCommand(pAP, "PMKSA_FLUSH", "refreshConfig");
    wld_secDmn_action_rc_ne rc = wld_ap_hostapd_setSecParams(pAP);
    ASSERT_FALSE(rc < SECDMN_ACTION_OK_DONE, true, ME, "%s: fail to set secret key", pAP->alias);

    s_schedNextAction(rc, pAP, pRad);
    return true;
}

static bool s_doSetApMld(T_AccessPoint* pAP, T_Radio* pRad) {
    ASSERTS_NOT_NULL(pAP, true, ME, "NULL");
    ASSERTI_TRUE(wifiGen_hapd_isRunning(pRad), true, ME, "%s: hostapd stopped", pRad->Name);
    ASSERTI_TRUE(wld_secDmn_hasGlobalCtrlIface(pRad->hostapd), true, ME, "%s: hapd has no available global socket", pRad->Name);
    ASSERTI_TRUE(wld_rad_isMloCapable(pRad), true, ME, "%s: not mlo capable", pRad->Name);
    SAH_TRACEZ_INFO(ME, "%s: checking mld conf changes", pAP->alias);
    wld_secDmn_action_rc_ne rc = wld_ap_hostapd_setMldParams(pAP);
    ASSERT_FALSE(rc < SECDMN_ACTION_OK_DONE, true, ME, "%s: fail to set common params", pAP->alias);
    s_schedNextAction(rc, pAP, pRad);
    return true;
}

static bool s_doSyncAp(T_AccessPoint* pAP, T_Radio* pRad _UNUSED) {
    ASSERTS_NOT_NULL(pAP, true, ME, "NULL");
    ASSERTI_TRUE(wld_wpaCtrlInterface_isReady(pAP->wpaCtrlInterface), true, ME, "%s: wpaCtrl disconnected", pAP->alias);
    wld_ap_hostapd_updateMaxNbrSta(pAP);
    wld_secDmn_action_rc_ne rc = wld_ap_hostapd_setNoSecParams(pAP);
    ASSERT_FALSE(rc < SECDMN_ACTION_OK_DONE, true, ME, "%s: fail to set common params", pAP->alias);
    s_schedNextAction(rc, pAP, pRad);
    return true;
}

static bool s_doReloadApSecKey(T_AccessPoint* pAP, T_Radio* pRad _UNUSED) {
    ASSERTS_NOT_NULL(pAP, true, ME, "NULL");
    ASSERTI_TRUE(wld_wpaCtrlInterface_isReady(pAP->wpaCtrlInterface), true, ME, "%s: wpaCtrl disconnected", pAP->alias);
    wld_ap_hostapd_reloadSecKey(pAP, "reloadSecKey");
    return true;
}

static bool s_doUpdateBeacon(T_AccessPoint* pAP, T_Radio* pRad _UNUSED) {
    ASSERTS_NOT_NULL(pAP, true, ME, "NULL");
    ASSERTI_TRUE(wld_wpaCtrlInterface_isReady(pAP->wpaCtrlInterface), true, ME, "%s: wpaCtrl disconnected", pAP->alias);
    chanmgt_rad_state detRadState = CM_RAD_UNKNOWN;
    bool apStackEna = wld_ap_hasStackEnabled(pAP);
    if((!apStackEna) || (wifiGen_hapd_getRadState(pRad, &detRadState) < SWL_RC_OK) || (detRadState != CM_RAD_UP)) {
        SAH_TRACEZ_INFO(ME, "%s: missing enable conds apE:%d stackE:%d radDetS:%d", pAP->alias, pAP->enable, apStackEna, detRadState);
        return true;
    }
    SAH_TRACEZ_INFO(ME, "%s: start/update beaconing", pAP->alias);
    wld_ap_hostapd_updateBeacon(pAP, "updateBeacon");
    return true;
}

static bool s_doSetChannel(T_Radio* pRad) {
    SAH_TRACEZ_INFO(ME, "%s: set channel %d", pRad->Name, pRad->channel);
    pRad->pFA->mfn_wrad_setChanspec(pRad, true);
    return true;
}

static bool s_doSyncState(T_Radio* pRad) {
    chanmgt_rad_state fhDetSta = CM_RAD_UNKNOWN;
    if(!wld_secDmn_isEnabled(pRad->hostapd)) {
        fhDetSta = CM_RAD_DOWN;
    } else {
        wifiGen_hapd_getRadState(pRad, &fhDetSta);
    }
    /*
     * when hostapd is alive (i.e radio administratively enabled)
     * and handling DFS clearing (i.e driver does not support DFS_OFFLOAD)
     * and that current band channels are no more usable
     * (previous CAC have expired, or channel not available)
     * => we have to recover by toggling hapd (which eventually triggers a new dfs cac),
     *    otherwise radio remains passively down
     */
    if((wld_rad_is_5ghz(pRad)) && (wifiGen_hapd_isAlive(pRad)) && (fhDetSta == CM_RAD_DOWN)) {
        swl_chanspec_t chanSpec;
        _UNUSED_(chanSpec);
        if((!wld_channel_is_band_usable(wld_rad_getSwlChanspec(pRad)) ||
            (pRad->pFA->mfn_wrad_getChanspec(pRad, &chanSpec) < SWL_RC_OK))) {
            SAH_TRACEZ_WARNING(ME, "%s: curr band [%s] is unusable", pRad->Name, pRad->channelsInUse);
            if(!pRad->pFA->mfn_misc_has_support(pRad, NULL, "DFS_OFFLOAD", 0)) {
                SAH_TRACEZ_WARNING(ME, "%s: no dfs_offload => need to toggle hostapd recover radio", pRad->Name);
                s_schedNextAction(SECDMN_ACTION_OK_NEED_TOGGLE, NULL, pRad);
                return true;
            }
        }
    }
    wifiGen_hapd_syncVapStates(pRad);
    return true;
}

static bool s_doEnableEp(T_EndPoint* pEP, T_Radio* pRad _UNUSED) {
    bool enaConds = (wld_endpoint_hasStackEnabled(pEP) && (pEP->index > 0));
    wld_endpoint_setConnectionStatus(pEP, enaConds ? EPCS_IDLE : EPCS_DISABLED, pEP->error);
    ASSERTS_TRUE(enaConds, true, ME, "%d: ep not ready", pEP->Name);
    wld_endpoint_resetStats(pEP);
    SAH_TRACEZ_INFO(ME, "%s: enable endpoint", pEP->Name);
    return true;
}

static bool s_doSetEpMAC(T_EndPoint* pEP, T_Radio* pRadio) {
    ASSERT_NOT_NULL(pEP, true, ME, "NULL");
    T_SSID* pSSID = pEP->pSSID;
    ASSERT_NOT_NULL(pSSID, true, ME, "NULL");
    ASSERT_NOT_NULL(pRadio, true, ME, "NULL");
    swl_macBin_t* mac = (swl_macBin_t*) pSSID->MACAddress;
    ASSERT_FALSE(swl_mac_binIsNull(mac), true, ME, "%s: null mac", pEP->Name);
    SAH_TRACEZ_WARNING(ME, "%s: set endpoint mac "SWL_MAC_FMT, pEP->Name, SWL_MAC_ARG(pSSID->MACAddress));
    if(wld_linuxIfUtils_updateMac(wld_rad_getSocket(pRadio), pEP->Name, mac) < 0) {
        SAH_TRACEZ_ERROR(ME, "%s: fail to set ep intf mac ["SWL_MAC_FMT "]",
                         pEP->Name, SWL_MAC_ARG(pSSID->MACAddress));
    }
    return true;
}

static bool s_doConnectEp(T_EndPoint* pEP, T_Radio* pRad _UNUSED) {
    ASSERTI_TRUE(wifiGen_wpaSupp_isAlive(pEP), true, ME, "%s: wpasupp not running", pEP->Name);
    ASSERTW_TRUE(wld_endpoint_isReady(pEP), true, ME, "%s: endpoint profile is not ready for connection", pEP->Name);
    wld_wpaCtrl_sendCmdCheckResponse(pEP->wpaCtrlInterface, "ENABLE_NETWORK all", "OK");
    wld_wpaCtrl_sendCmdCheckResponse(pEP->wpaCtrlInterface, "SELECT_NETWORK any", "OK");
    if(wld_linuxIfUtils_getStateExt(pEP->Name) == 0) {
        wld_linuxIfUtils_setStateExt(pEP->Name, true);
    }
    return true;
}

static bool s_doStopWpaSupp(T_EndPoint* pEP, T_Radio* pRad _UNUSED) {
    ASSERTS_TRUE(wifiGen_wpaSupp_isRunning(pEP), true, ME, "%s: wpa_supplicant stopped", pEP->Name);
    SAH_TRACEZ_INFO(ME, "%s: stop wpa_supplicant", pEP->Name);
    wifiGen_wpaSupp_stopDaemon(pEP);
    return true;
}

static swl_chanspec_t s_getEpBssTgtChanspec(T_EndPoint* pEP, swl_macBin_t* bssid) {
    swl_chanspec_t chanSpec = SWL_CHANSPEC_EMPTY;
    if(!pEP || !bssid || swl_mac_binIsNull(bssid) || swl_mac_binIsBroadcast(bssid)) {
        return chanSpec;
    }
    wld_scanResultSSID_t result;
    memset(&result, 0, sizeof(result));
    if(swl_rc_isOk(wld_wpaSupp_ep_getBssScanInfo(pEP, bssid, &result))) {
        swl_chanspec_fromFreqCtrlCentre(&chanSpec, swl_chanspec_operClassToFreq(result.operClass), result.channel, result.centreChannel);
        if(chanSpec.band == SWL_FREQ_BAND_EXT_2_4GHZ) {
            //only pre-connect to 2g/20MHz
            chanSpec = (swl_chanspec_t) SWL_CHANSPEC_NEW(chanSpec.channel, SWL_BW_20MHZ, chanSpec.band);
        }
    }
    return chanSpec;
}

static void s_syncOnEpConnected(void* userData, char* ifName, bool state) {
    ASSERTS_TRUE(state, , ME, "not connected");
    T_Radio* pRad = (T_Radio*) userData;
    T_EndPoint* pEP = wld_rad_ep_from_name(pRad, ifName);
    ASSERT_NOT_NULL(pEP, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: connected endpoint", pEP->Name);
    wld_nl80211_ifaceInfo_t epIfInfo;
    swl_rc_ne rc = wld_ep_nl80211_getInterfaceInfo(pEP, &epIfInfo);
    if(swl_rc_isOk(rc)) {
        const wld_nl80211_chanSpec_t* pNlChspec = &epIfInfo.chanSpec;
        if(pNlChspec->noHT) {
            SAH_TRACEZ_WARNING(ME, "%s: re-establish ep connection to recover full chanwidth", pEP->Name);
            wld_wpaCtrl_sendCmdCheckResponse(pEP->wpaCtrlInterface, "REATTACH", "OK");
            return;
        }

        /*
         * manage case where endpoint connects with reduced bw, although larger is supported:
         * This may happen when fronthaul is still active and configuring radio to different set of channels
         * The driver may decide to select to shared channels range, which reduces the final bandwidth
         */

        /*
         * get the current chanspec from nl info
         * and the target chanspec from last wpa_s bss scan results
         */
        swl_chanspec_t curChspec = SWL_CHANSPEC_EMPTY;
        wld_nl80211_chanSpecNlToSwl(&curChspec, (wld_nl80211_chanSpec_t*) pNlChspec);
        swl_macBin_t* bssid = (swl_macBin_t*) pEP->pSSID->BSSID;
        swl_chanspec_t tgtChspec = s_getEpBssTgtChanspec(pEP, bssid);

        if((curChspec.channel == tgtChspec.channel) &&
           (curChspec.bandwidth >= tgtChspec.bandwidth)) {
            tgtChspec.bandwidth = curChspec.bandwidth;
            tgtChspec.extensionHigh = curChspec.extensionHigh;
        }

        /*
         * proceed to reconnect using the scan tgt chanspec, if:
         *    - mismatching the current ep connection chanspe
         *    - supported and applicable
         *    - not already tried for sync
         *    - fronthaul is still active
         */
        if((curChspec.channel > 0) && (tgtChspec.channel > 0) &&
           (!swl_typeChanspec_equals(tgtChspec, curChspec))) {
            SAH_TRACEZ_WARNING(ME, "%s: connected to %s but chspec mismatching scan (used:%s vs scan:%s)",
                               pEP->Name, swl_typeMacBin_toBuf32Ref(bssid).buf,
                               swl_typeChanspecExt_toBuf32(curChspec).buf,
                               swl_typeChanspecExt_toBuf32(tgtChspec).buf);
            if(!((!wld_channel_is_band_usable(tgtChspec)) ||
                 (tgtChspec.bandwidth > pRad->maxChannelBandwidth) ||
                 ((swl_typeChanspec_equals(wld_chanmgt_getTgtChspec(pRad), tgtChspec)) &&
                  (swl_str_matches(pRad->targetChanspec.reasonExt, "ep preConnect")) &&
                  (pRad->targetChanspec.isApplied == SWL_TRL_FALSE)) ||
                 (!wifiGen_hapd_isAlive(pRad)) || (!s_checkHapdActive(pRad)))) {
                wld_chanmgt_setTargetChanspec(pRad, tgtChspec, false, CHAN_REASON_EP_MOVE, "ep preConnect");
                if((swl_typeChanspec_equals(wld_chanmgt_getTgtChspec(pRad), tgtChspec)) &&
                   (pRad->targetChanspec.isApplied != SWL_TRL_FALSE)) {
                    /*
                     * 1) disconnect ep to release the radio config
                     * 2) schedule reconfiguring radio chanspec through the fronthaul
                     * 3) schedule restoring the ep connection just after
                     *
                     * => this allows recovering the applicable chanspec as detected in the scan results
                     */
                    SAH_TRACEZ_WARNING(ME, "%s: reconnect ep to sync chanspec", pEP->Name);
                    wld_endpoint_reconfigure(pEP);
                    return;
                }
            }
        }

        int32_t linkId = NO_LINK_ID;
        swl_mlo_role_e mldRole = SWL_MLO_ROLE_NONE;
        if(epIfInfo.nMloLinks > 0) {
            swl_freqBandExt_e band = pRad->operatingFrequencyBand;
            if(band >= SWL_FREQ_BAND_EXT_NONE) {
                band = wld_chanmgt_getCurChspec(pRad).band;
            }
            const wld_nl80211_ifaceMloLinkInfo_t* pLinkInfo =
                wld_nl80211_fetchIfaceMloLinkByFreqBand(&epIfInfo, band);
            if(pLinkInfo) {
                if(pLinkInfo->linkPos == 0) {
                    mldRole = SWL_MLO_ROLE_PRIMARY;
                } else {
                    mldRole = SWL_MLO_ROLE_AUXILIARY;
                }
                linkId = pLinkInfo->link.linkId;
            }
        }
        wld_ssid_setMLDLinkID(pEP->pSSID, linkId);
        wld_ssid_setMLDRole(pEP->pSSID, mldRole);
    }
    s_delayRestoreFronthaul(pRad);
}

static void s_syncOnEpDisconnected(void* userData, char* ifName, bool state) {
    ASSERTS_FALSE(state, , ME, "still connected");
    T_Radio* pRad = (T_Radio*) userData;
    T_EndPoint* pEP = wld_rad_ep_from_name(pRad, ifName);
    ASSERT_NOT_NULL(pEP, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: disconnected endpoint", pEP->Name);
    wld_ssid_setMLDLinkID(pEP->pSSID, NO_LINK_ID);
    wld_ssid_setMLDRole(pEP->pSSID, SWL_MLO_ROLE_NONE);
    ASSERTS_TRUE(wifiGen_hapd_isAlive(pRad), , ME, "%s: hapd not running", pRad->Name);
    chanmgt_rad_state detRadState = CM_RAD_UNKNOWN;
    if((wifiGen_hapd_getRadState(pRad, &detRadState) == SWL_RC_OK) &&
       (detRadState != CM_RAD_UP)) {
        SAH_TRACEZ_INFO(ME, "%s: restore fronthaul chanspec conf", pEP->Name);
        //restore tgt chanspec after disconnection
        s_delayRestoreFronthaul(pRad);
    }
}

static void s_syncOnEpStartConn(void* userData, char* ifName, const char* ssid, swl_macBin_t* bssid, swl_chanspec_t* chanSpec) {
    T_Radio* pRad = (T_Radio*) userData;
    T_EndPoint* pEP = wld_rad_ep_from_name(pRad, ifName);
    ASSERT_NOT_NULL(pEP, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: endpoint is connecting to (%s) %s over chspec(%s)",
                    pEP->Name, ssid, swl_typeMacBin_toBuf32Ref(bssid).buf, swl_typeChanspecExt_toBuf32Ref(chanSpec).buf);
    ASSERTS_TRUE(wifiGen_hapd_isAlive(pRad), , ME, "%s: hapd not running", pRad->Name);

    swl_chanspec_t currChanSpec = SWL_CHANSPEC_EMPTY;
    pRad->pFA->mfn_wrad_getChanspec(pRad, &currChanSpec);

    if((chanSpec->channel > 0) && !swl_channel_isInChanspec(&currChanSpec, chanSpec->channel)) {
        SAH_TRACEZ_INFO(ME, "%s: rad(%s) currentChanspec(%s) mismatch remoteChanspec(%s)",
                        pEP->Name, pRad->Name,
                        swl_typeChanspecExt_toBuf32Ref(&currChanSpec).buf,
                        swl_typeChanspecExt_toBuf32Ref(chanSpec).buf);
    }
}

static void s_syncOnEpScanFail(void* userData, char* ifName, int error) {
    T_Radio* pRad = (T_Radio*) userData;
    T_EndPoint* pEP = wld_rad_ep_from_name(pRad, ifName);
    ASSERT_NOT_NULL(pEP, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: EP failed to start scan: error(%d)", pEP->Name, error);
    if(error == -EBUSY) {
        if(wld_scan_isRunning(pRad)) {
            wld_scan_stop(pRad);
        }
        ASSERTS_TRUE(wifiGen_hapd_isAlive(pRad), , ME, "%s: hapd not running", pRad->Name);
        chanmgt_rad_state detRadState = CM_RAD_UNKNOWN;
        wifiGen_hapd_getRadState(pRad, &detRadState);
        if((detRadState == CM_RAD_FG_CAC) ||
           ((detRadState == CM_RAD_UP) && swl_chanspec_isDfs(wld_chanmgt_getCurChspec(pRad)))) {
            swl_chanspec_t noDfsChspec = SWL_CHANSPEC_NEW(
                wld_chanmgt_getDefaultSupportedChannel(pRad),
                wld_chanmgt_getDefaultSupportedBandwidth(pRad),
                pRad->operatingFrequencyBand);
            SAH_TRACEZ_INFO(ME, "%s: switch to noDFS %s to allow wpa_supplicant to start scan",
                            pEP->Name, swl_typeChanspecExt_toBuf32(noDfsChspec).buf);
            wld_chanmgt_setTargetChanspec(pRad, noDfsChspec, true, CHAN_REASON_EP_MOVE, "force noDfs to allow ep scan");
        }
    } else if(error == -ENOENT) {
        s_delayRestoreFronthaul(pRad);
    }
}

static void s_syncOnEpStartConnFail(void* userData, char* ifName, int error) {
    T_Radio* pRad = (T_Radio*) userData;
    T_EndPoint* pEP = wld_rad_ep_from_name(pRad, ifName);
    ASSERT_NOT_NULL(pEP, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: EP failed to start connection: error(%d)", pEP->Name, error);
    ASSERTS_TRUE(wifiGen_hapd_isAlive(pRad), , ME, "%s: hapd not running", pRad->Name);
    if(error == -EBUSY) {
        swl_macBin_t* bssid = &pEP->currConnBssid;
        swl_chanspec_t chanSpec = s_getEpBssTgtChanspec(pEP, bssid);
        if((chanSpec.channel != 0) && (chanSpec.band != wld_chanmgt_getCurChspec(pRad).band)) {
            SAH_TRACEZ_WARNING(ME, "%s: ignore target ep chanspec %s out of current band",
                               pEP->Name, swl_typeChanspecExt_toBuf32(chanSpec).buf);
            return;
        }
        /*
         * try moving fronthaul to backhaul chanspec, only if backhaul chanspsec is:
         * 0) define
         * 1) not dfs: prevent blocking the ep scan while fronthaul doing CAC
         * 2) not yet applied on fronthaul
         * 3) not yet tried on fronthaul for ep pre-connect, and rejected
         * 4) not rejected on direct application
         * Otherwise, the fronthaul is disabled in favor of backhaul, letting it take the radio control
         */
        if(!((chanSpec.channel == 0) ||
             (!wld_channel_is_band_usable(chanSpec)) ||
             (swl_typeChanspec_equals(wld_chanmgt_getCurChspec(pRad), chanSpec)) ||
             ((swl_typeChanspec_equals(wld_chanmgt_getTgtChspec(pRad), chanSpec)) &&
              (swl_str_matches(pRad->targetChanspec.reasonExt, "ep preConnect")) &&
              (pRad->targetChanspec.isApplied == SWL_TRL_FALSE)))) {
            SAH_TRACEZ_INFO(ME, "%s: switch fronthaul to chspec %s to allow wpa_supplicant to connect to %s",
                            pEP->Name, swl_typeChanspecExt_toBuf32(chanSpec).buf,
                            swl_typeMacBin_toBuf32Ref(bssid).buf
                            );
            wld_chanmgt_setTargetChanspec(pRad, chanSpec, true, CHAN_REASON_EP_MOVE, "ep preConnect");
            if((swl_typeChanspec_equals(wld_chanmgt_getTgtChspec(pRad), chanSpec)) &&
               (pRad->targetChanspec.isApplied == SWL_TRL_TRUE)) {
                return;
            }
        }
        // disable BSSs to allow wpa_supplicant to do start connection
        SAH_TRACEZ_INFO(ME, "%s: disable fronthaul to allow wpa_supplicant to connect on chspec %s to %s",
                        pEP->Name, swl_typeChanspecExt_toBuf32(chanSpec).buf,
                        swl_typeMacBin_toBuf32Ref(bssid).buf);
        s_disableFronthaul(pRad);
    }
}

static void s_syncOnWpsFail(void* userData, char* ifName) {
    T_Radio* pRad = (T_Radio*) userData;
    T_EndPoint* pEP = wld_rad_ep_from_name(pRad, ifName);
    if(pEP != NULL) {
        SAH_TRACEZ_INFO(ME, "%s: EP wps failed", pEP->Name);
        ASSERTS_TRUE(wifiGen_hapd_isAlive(pRad), , ME, "%s: hapd not running", pRad->Name);
        s_restoreFronthaul(pRad);
    }
}

static void s_registerWpaSuppRadEvtHandlers(wld_secDmn_t* wpaSupp) {
    ASSERT_NOT_NULL(wpaSupp, , ME, "no wpaSupp ctx");
    void* userdata = NULL;
    wld_wpaCtrl_radioEvtHandlers_cb handlers = {0};
    if(wld_wpaCtrlMngr_getEvtHandlers(wpaSupp->wpaCtrlMngr, &userdata, &handlers)) {
        handlers.fSyncOnEpConnected = s_syncOnEpConnected;
        handlers.fSyncOnRadioUp = s_syncOnEpRadUp;
        handlers.fSyncOnEpDisconnected = s_syncOnEpDisconnected;
        handlers.fStationStartConnCb = s_syncOnEpStartConn;
        handlers.fStationScanFailedCb = s_syncOnEpScanFail;
        handlers.fStationStartConnFailedCb = s_syncOnEpStartConnFail;
        handlers.fWpsCancelMsg = s_syncOnWpsFail;
        handlers.fWpsTimeoutMsg = s_syncOnWpsFail;
        handlers.fWpsOverlapMsg = s_syncOnWpsFail;
        handlers.fWpsFailMsg = s_syncOnWpsFail;
        wld_wpaCtrlMngr_setEvtHandlers(wpaSupp->wpaCtrlMngr, userdata, &handlers);
    }
}

static bool s_doStartWpaSupp(T_EndPoint* pEP, T_Radio* pRad _UNUSED) {
    bool enaConds = (pRad->enable && pEP->enable && (pEP->index > 0));
    ASSERTS_TRUE(enaConds, true, ME, "%d: ep not ready", pEP->Name);
    SAH_TRACEZ_INFO(ME, "%s: start wpa_supplicant", pEP->Name);
    wifiGen_wpaSupp_startDaemon(pEP);
    s_registerWpaSuppRadEvtHandlers(pEP->wpaSupp);
    return true;
}

static bool s_doConfWpaSupp(T_EndPoint* pEP, T_Radio* pRad _UNUSED) {
    SAH_TRACEZ_INFO(ME, "%s: write wpa_supplicant config", pEP->Name);
    wifiGen_wpaSupp_writeConfig(pEP);
    return true;
}

static bool s_doReloadWpaSupp(T_EndPoint* pEP, T_Radio* pRad _UNUSED) {
    ASSERTS_TRUE(wifiGen_wpaSupp_isRunning(pEP), true, ME, "%s: wpa_supplicant stopped", pEP->Name);
    SAH_TRACEZ_INFO(ME, "%s: try to reconfigure wpa_supplicant", pEP->Name);
    if((!wifiGen_wpaSupp_isAlive(pEP)) || (wld_wpaSupp_ep_reconfigure(pEP) < SWL_RC_OK)) {
        SAH_TRACEZ_INFO(ME, "%s: force reload wpa_supplicant", pEP->Name);
        wifiGen_wpaSupp_reloadDaemon(pEP);
    }
    pRad->fsmRad.timeout_msec = 500;
    return true;
}
/*
 * Check if we need to do radio up down actions. Need to happen if it's first interface that goes up.
 */
static void s_checkPreRadDependency(T_Radio* pRad _UNUSED) {
    if(isBitSetLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_ENABLE_RAD) || (pRad->status == RST_ERROR)) {
        SAH_TRACEZ_ERROR(ME, "%s : no pre dep change", pRad->Name);
        return;
    }

    bool targetFthEnable = wifiGen_hapd_isStartable(pRad);
    bool targetBkhEnable = (pRad->enable && wld_rad_hasEnabledEp(pRad));
    bool targetEnable = (targetFthEnable || targetBkhEnable);
    bool currentEnable = wld_rad_isUpExt(pRad) || (wld_rad_hasActiveEp(pRad) && (!wld_rad_hasActiveVap(pRad)));
    SAH_TRACEZ_WARNING(ME, "%s: setEnable curr %u , tgt %u (fth %u / bkh %u) , dm %u, status %s", pRad->Name,
                       currentEnable, targetEnable,
                       targetFthEnable, targetBkhEnable,
                       pRad->enable, wld_status_str[pRad->status]);

    if(targetEnable != currentEnable) {
        setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_ENABLE_RAD);
    }
    if(wifiGen_hapd_isStarted(pRad) != targetFthEnable) {
        setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_START_HOSTAPD);
    }
    if(wifiGen_wpaSupp_isRunning(wld_rad_getFirstEp(pRad)) != targetBkhEnable) {
        setBitLongArray(pRad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_START_WPASUPP);
    }
}

static void s_checkApDependency(T_AccessPoint* pAP, T_Radio* pRad) {
    /*
     * Disabling/enabling radio will implicitly interact with hostapd,
     * which will manage the related VAPs
     */
    if(isBitSetLongArray(pAP->fsm.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_MOD_BSSID)) {
        setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_START_HOSTAPD);
    }
    if((isBitSetLongArray(pAP->fsm.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_MOD_MLD)) ||
       (s_fetchDynConfAction(pAP->fsm.FSM_AC_BitActionArray, FSM_BW) >= 0)) {
        setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_MOD_HOSTAPD);
        if(!wifiGen_hapd_isAlive(pRad)) {
            setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_START_HOSTAPD);
        }
        T_SSID* pSSID = pAP->pSSID;
        ASSERTS_NOT_NULL(pSSID, , ME, "%s: no ssid ctx", pAP->alias);
        wld_mld_setLinkConfigured(pSSID->pMldLink, wld_mld_isLinkUsable(pSSID->pMldLink));
        wld_ssid_setMLDStatus(pSSID, wld_mld_checkMLDStatus(pSSID));
    }
}

void s_checkEpDependency(T_EndPoint* pEP, T_Radio* pRad _UNUSED) {
    if(isBitSetLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_ENABLE_RAD)) {
        setBitLongArray(pEP->fsm.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_ENABLE_EP);
    }
    if(isBitSetLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_MOD_COUNTRYCODE)) {
        setBitLongArray(pEP->fsm.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_START_WPASUPP);
    }
    if(isBitSetLongArray(pEP->fsm.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_CONNECT_EP)) {
        if(!wifiGen_wpaSupp_isRunning(pEP)) {
            setBitLongArray(pEP->fsm.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_START_WPASUPP);
        }
    }
    if(isBitSetLongArray(pEP->fsm.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_ENABLE_EP)) {
        setBitLongArray(pEP->fsm.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_START_WPASUPP);
    }
    if(isBitSetLongArray(pEP->fsm.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_START_WPASUPP)) {
        clearBitLongArray(pEP->fsm.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_UPDATE_WPASUPP);
        clearBitLongArray(pEP->fsm.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_CONNECT_EP);
        setBitLongArray(pEP->fsm.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_STOP_WPASUPP);
        setBitLongArray(pEP->fsm.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_MOD_WPASUPP);
    }
    if(isBitSetLongArray(pEP->fsm.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_START_WPASUPP) ||
       isBitSetLongArray(pEP->fsm.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_CONNECT_EP)) {
        wld_ssid_setMLDStatus(pEP->pSSID, wld_mld_checkMLDStatus(pEP->pSSID));
    }
}

static void s_checkRadDependency(T_Radio* pRad) {

    if(isBitSetLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_ENABLE_RAD)) {
        setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_DISABLE_RAD);
        setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_START_HOSTAPD);
    }
    if(isBitSetLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_START_HOSTAPD)) {
        setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_STOP_HOSTAPD);
    }
    if(isBitSetLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_MOD_COUNTRYCODE) ||
       isBitSetLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_SYNC_RAD)) {
        if(!isBitSetLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_START_HOSTAPD)) {
            s_schedNextAction(SECDMN_ACTION_OK_NEED_TOGGLE, NULL, pRad);
        }
        setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_DISABLE_RAD);
        setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_MOD_HOSTAPD);
    }

    int applyAction = s_fetchApplyAction(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW);
    if(applyAction >= 0) {
        applyAction = sApplyActions[applyAction];
    }
    int dynConfAction = s_fetchDynConfAction(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW);
    if(dynConfAction >= 0) {
        dynConfAction = sDynConfActions[dynConfAction];
    }
    if((applyAction >= 0) || (dynConfAction >= 0) ||
       (isBitSetLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_MOD_CHANNEL))) {
        setBitLongArray(pRad->fsmRad.FSM_AC_BitActionArray, FSM_BW, GEN_FSM_MOD_HOSTAPD);
    }
    if(applyAction >= 0) {
        s_clearRadLowerApplyActions(pRad, applyAction);
        if((applyAction == GEN_FSM_START_HOSTAPD) && (dynConfAction >= 0)) {
            s_clearRadDynConfActions(pRad);
        }
    }
}

wld_fsmMngr_action_t actions[GEN_FSM_MAX] = {
    {FSM_ACTION(GEN_FSM_STOP_HOSTAPD), .doRadFsmAction = s_doStopHostapd},
    {FSM_ACTION(GEN_FSM_STOP_WPASUPP), .doEpFsmAction = s_doStopWpaSupp},
    {FSM_ACTION(GEN_FSM_REMOVE_HOSTAPD), .doRadFsmAction = s_doRemoveHostapd},
    {FSM_ACTION(GEN_FSM_DISABLE_HOSTAPD), .doRadFsmAction = s_doDisableHostapd},
    {FSM_ACTION(GEN_FSM_DISABLE_RAD), .doRadFsmAction = s_doRadDisable},
    {FSM_ACTION(GEN_FSM_SYNC_RAD), .doRadFsmAction = s_doRadSync, .doVapFsmAction = s_doSetApSec},
    {FSM_ACTION(GEN_FSM_MOD_COUNTRYCODE), .doRadFsmAction = s_doSetCountryCode},
    {FSM_ACTION(GEN_FSM_MOD_BSSID), .doVapFsmAction = s_doSetBssid},
    {FSM_ACTION(GEN_FSM_MOD_MLD), .doVapFsmAction = s_doSetApMld},
    {FSM_ACTION(GEN_FSM_MOD_SEC), .doVapFsmAction = s_doSetApSec},
    {FSM_ACTION(GEN_FSM_MOD_AP), .doVapFsmAction = s_doSyncAp},
    {FSM_ACTION(GEN_FSM_MOD_SSID), .doVapFsmAction = s_doSetSsid},
    {FSM_ACTION(GEN_FSM_MOD_MF_LIST), .doVapFsmAction = s_doSetMfList},
    {FSM_ACTION(GEN_FSM_MOD_CHANNEL), .doRadFsmAction = s_doSetChannel},
    {FSM_ACTION(GEN_FSM_MOD_HOSTAPD), .doRadFsmAction = s_doConfHostapd},
    {FSM_ACTION(GEN_FSM_MOD_WPASUPP), .doEpFsmAction = s_doConfWpaSupp},
    {FSM_ACTION(GEN_FSM_RELOAD_AP_SECKEY), .doVapFsmAction = s_doReloadApSecKey},
    {FSM_ACTION(GEN_FSM_MOD_EP_MACADDR), .doEpFsmAction = s_doSetEpMAC},
    {FSM_ACTION(GEN_FSM_UPDATE_BEACON), .doVapFsmAction = s_doUpdateBeacon},
    {FSM_ACTION(GEN_FSM_UPDATE_WPASUPP), .doEpFsmAction = s_doReloadWpaSupp},
    {FSM_ACTION(GEN_FSM_ENABLE_HOSTAPD), .doRadFsmAction = s_doEnableHostapd},
    {FSM_ACTION(GEN_FSM_ENABLE_RAD), .doRadFsmAction = s_doRadEnable},
    {FSM_ACTION(GEN_FSM_ENABLE_AP), .doVapFsmAction = s_doEnableAp},
    {FSM_ACTION(GEN_FSM_ENABLE_EP), .doEpFsmAction = s_doEnableEp},
    {FSM_ACTION(GEN_FSM_UPDATE_HOSTAPD), .doRadFsmAction = s_doUpdateHostapd},
    {FSM_ACTION(GEN_FSM_ADD_HOSTAPD), .doRadFsmAction = s_doAddHostapd},
    {FSM_ACTION(GEN_FSM_START_HOSTAPD), .doRadFsmAction = s_doStartHostapd},
    {FSM_ACTION(GEN_FSM_SYNC_STATE), .doRadFsmAction = s_doSyncState},
    {FSM_ACTION(GEN_FSM_CONNECT_EP), .doEpFsmAction = s_doConnectEp},
    {FSM_ACTION(GEN_FSM_START_WPASUPP), .doEpFsmAction = s_doStartWpaSupp},
};

wld_fsmMngr_t mngr = {
    .checkPreDependency = s_checkPreRadDependency,
    .checkEpDependency = s_checkEpDependency,
    .checkVapDependency = s_checkApDependency,
    .checkRadDependency = s_checkRadDependency,
    .actionList = actions,
    .nrFsmBits = GEN_FSM_MAX,
};

static void s_vapStatusUpdateCb(wld_vap_status_change_event_t* event) {
    ASSERT_NOT_NULL(event, , ME, "NULL");
    T_AccessPoint* pAP = event->vap;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    T_SSID* pSSID = pAP->pSSID;
    ASSERT_NOT_NULL(pSSID, , ME, "NULL");
    T_Radio* pRad = pAP->pRadio;
    ASSERTS_EQUALS(pRad->vendor->fsmMngr, &mngr, , ME, "%s: radio managed by vendor specific FSM", pRad->Name);


    if((pAP->status == APSTI_ENABLED) && (pSSID->status == RST_UP)) {
        wifiGen_vap_postUpActions(pAP);
    } else if(pSSID->status == RST_DOWN) {
        wifiGen_vap_postDownActions(pAP);
    }
}

static wld_event_callback_t s_vapStatusCbContainer = {
    .callback = (wld_event_callback_fun) s_vapStatusUpdateCb
};

static void s_writeHapdConfFileCb(wld_secDmn_t* pSecDmn _UNUSED, void* userdata) {
    T_Radio* pRad = (T_Radio*) userdata;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    wifiGen_hapd_writeConfig(pRad);
}

/**
 * @brief hostapd process pre start handler:
 * called just before forking to run hostapd process
 * This allows to take imminent actions required for a proper startup
 *
 * @param pSecDmn pointer to security daemon context
 * @param userdata pointer to registered userData
 *
 * @return void
 */
static void s_preStartHapdCb(wld_secDmn_t* pSecDmn _UNUSED, void* userdata) {
    T_Radio* pRad = (T_Radio*) userdata;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    /*
     * prepare ifaces even for non-startable radios
     * to avoid later delay when adding conf
     */
    wifiGen_hapd_prepareIfaces(pRad, false);
}

static void s_updateHapdDmnEvtHandlers(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, , ME, "NULL");
    ASSERTS_NOT_NULL(pRad->hostapd, , ME, "NULL");
    if(pRad->hostapd->handlers.writeCfgCb == NULL) {
        pRad->hostapd->handlers.writeCfgCb = s_writeHapdConfFileCb;
    }
    if(pRad->hostapd->handlers.preStartCb == NULL) {
        pRad->hostapd->handlers.preStartCb = s_preStartHapdCb;
    }
}

static void s_radioChange(wld_rad_changeEvent_t* event) {
    ASSERT_NOT_NULL(event, , ME, "NULL");
    T_Radio* pRad = event->pRad;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    ASSERTS_EQUALS(pRad->vendor->fsmMngr, &mngr, , ME, "%s: radio managed by vendor specific FSM", pRad->Name);
    if(event->changeType == WLD_RAD_CHANGE_INIT) {
        s_updateHapdDmnEvtHandlers(pRad);
        wld_event_add_callback(gWld_queue_vap_onStatusChange, &s_vapStatusCbContainer);
        s_registerHadpRadEvtHandlers(pRad->hostapd);
        wifiGen_zwdfs_init(pRad);
    } else if(event->changeType == WLD_RAD_CHANGE_DESTROY) {
        wld_event_remove_callback(gWld_queue_vap_onStatusChange, &s_vapStatusCbContainer);
        wifiGen_zwdfs_deinit(pRad);
    }
}

static wld_event_callback_t s_onRadioChange = {
    .callback = (wld_event_callback_fun) s_radioChange,
};

static void s_updateMaxStations(amxp_timer_t* timer, void* priv) {
    T_Radio* pRad = (T_Radio*) priv;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    wld_rad_hostapd_updateMaxNumStations(pRad);
    amxp_timer_delete(&timer);
    pRad->setMaxNumStaTimer = NULL;
}

static void s_delayedSetMaxStationsTask(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    if(pRad->setMaxNumStaTimer == NULL) {
        amxp_timer_new(&pRad->setMaxNumStaTimer, s_updateMaxStations, pRad);
    }
    amxp_timer_start(pRad->setMaxNumStaTimer, 1000);
}

static void s_stationChange(wld_ad_changeEvent_t* event) {
    ASSERT_NOT_NULL(event, , ME, "NULL");
    T_AccessPoint* pAP = event->vap;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    T_Radio* pRad = pAP->pRadio;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    ASSERTS_EQUALS(pRad->vendor->fsmMngr, &mngr, , ME, "%s: radio managed by vendor specific FSM", pRad->Name);
    if((event->changeType == WLD_AD_CHANGE_EVENT_ASSOC) || (event->changeType == WLD_AD_CHANGE_EVENT_DISASSOC)) {
        s_delayedSetMaxStationsTask(pRad);
    }
}

static wld_event_callback_t s_onStationChange = {
    .callback = (wld_event_callback_fun) s_stationChange,
};

static void s_apChange(wld_vap_changeEvent_t* event) {
    ASSERT_NOT_NULL(event, , ME, "NULL");
    T_AccessPoint* pAP = event->vap;
    ASSERT_NOT_NULL(pAP, , ME, "NULL");
    T_Radio* pRad = pAP->pRadio;
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    ASSERTS_EQUALS(pRad->vendor->fsmMngr, &mngr, , ME, "%s: radio managed by vendor specific FSM", pRad->Name);
    SAH_TRACEZ_INFO(ME, "%s: receiving event %d", pAP->alias, event->changeType);
    if((event->changeType == WLD_VAP_CHANGE_EVENT_DEINIT) &&
       (wld_wpaCtrlInterface_isEnabled(pAP->wpaCtrlInterface))) {
        setBitLongArray(pRad->fsmRad.FSM_BitActionArray, FSM_BW, GEN_FSM_UPDATE_HOSTAPD);
    }
}

static wld_event_callback_t s_onApChange = {
    .callback = (wld_event_callback_fun) s_apChange,
};

static void s_mldChange(wld_mldChange_t* event) {
    ASSERT_NOT_NULL(event, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "detect mld event %d mldtype %d unit %d", event->event, event->mldType, event->mldUnit);
    ASSERTI_NOT_EQUALS(event->mldType, WLD_SSID_TYPE_UNKNOWN, , ME, "untyped mld");
    T_SSID* pSSID = event->pEvtLinkSsid;
    ASSERT_NOT_NULL(pSSID, , ME, "No link ssid");
    T_Radio* pRad = pSSID->RADIO_PARENT;
    ASSERT_NOT_NULL(pRad, , ME, "No link radio");
    ASSERTS_EQUALS(pRad->vendor->fsmMngr, &mngr, , ME, "%s: radio managed by vendor specific FSM", pRad->Name);
    if(event->event == WLD_MLD_EVT_UPDATE) {
        wifiGen_mld_reconfigureNeighLinkSSIDs(pSSID);
    }
}

static wld_event_callback_t s_onMldChange = {
    .callback = (wld_event_callback_fun) s_mldChange,
};

void wifiGen_fsm_doInit(vendor_t* vendor) {
    ASSERT_NOT_NULL(vendor, , ME, "NULL");
    wld_fsm_init(vendor, &mngr);
    wld_event_add_callback(gWld_queue_rad_onChangeEvent, &s_onRadioChange);
    wld_event_add_callback(gWld_queue_sta_onChangeEvent, &s_onStationChange);
    wld_event_add_callback(gWld_queue_vap_onChangeEvent, &s_onApChange);
    wld_event_add_callback(gWld_queue_mld_onChangeEvent, &s_onMldChange);
}
