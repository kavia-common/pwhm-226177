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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <signal.h>
#include <debug/sahtrace.h>

#include "wld.h"
#include "wld_util.h"
#include "wld_radio.h"
#include "wld_ssid.h"
#include "wld_accesspoint.h"
#include "wld_endpoint.h"
#include "wld_wps.h"
#include "wld_rad_stamon.h"
#include "wld_eventing.h"
#include "wld_chanmgt.h"
#include "Utils/wld_autoCommitMgr.h"
#include "wld_nl80211_types.h"
#include "Features/wld_persist.h"
#include "wld/wld_vendorModule_mgr.h"
#include "wld/wld_linuxIfUtils.h"

#define ME "wld"

amxb_bus_ctx_t* wld_plugin_amx = NULL;
amxd_dm_t* wld_plugin_dm = NULL;
amxo_parser_t* wld_plugin_parser = NULL;
amxd_object_t* wifi = NULL;

// List of chipset vendor modules
static amxc_llist_t vendors = {NULL, NULL};

static vendor_t* s_getVdrFromIt(const amxc_llist_it_t* it) {
    ASSERTS_NOT_NULL(it, NULL, ME, "NULL");
    return amxc_llist_it_get_data(it, vendor_t, it);
}
vendor_t* wld_firstVdr() {
    return s_getVdrFromIt(amxc_llist_get_first(&vendors));
}
vendor_t* wld_lastVdr() {
    return s_getVdrFromIt(amxc_llist_get_last(&vendors));
}
vendor_t* wld_nextVdr(const vendor_t* pVdr) {
    ASSERTS_NOT_NULL(pVdr, NULL, ME, "NULL");
    return s_getVdrFromIt(amxc_llist_it_get_next(&pVdr->it));
}

static swl_macBin_t sWanAddr = {{0}};

/**
 * Time component got init
 */
static swl_timeSpecMono_t initTime;

T_CONST_WPS g_wpsConst;
static bool init = false;

static int32_t g_MaxNrSta = 32;             //default min 32 stations per radio
static uint32_t g_MaxNrAPs = 4;             //default min 4 BSSs per Radio when MBSS is supported
static uint32_t g_MaxNrEPs = MAXNROF_RADIO; //default 1 EP per Radio

SWL_TT_C(gtWld_staHistory, wld_staHistory_t, X_WLD_STA_HISTORY);
SWL_NTT_C(gtWld_associatedDevice, T_AssociatedDevice, X_T_ASSOCIATED_DEVICE_ANNOT)
SWL_ARRAY_TYPE_C(gtWld_acTrafficArray, gtSwl_type_uint32, WLD_AC_MAX, true, false);
SWL_ARRAY_TYPE_C(gtWld_signalStatArray, gtSwl_type_double, MAX_NR_ANTENNA, false, true);
SWL_NTT_C(gtWld_stats, T_Stats, X_WLD_STATS);

const swl_macBin_t* wld_getWanAddr() {
    return &sWanAddr;
}

T_Radio* wld_firstRad() {
    amxc_llist_it_t* it = amxc_llist_get_first(&g_radios);
    return wld_rad_fromIt(it);
}
T_Radio* wld_lastRad() {
    amxc_llist_it_t* it = amxc_llist_get_last(&g_radios);
    return wld_rad_fromIt(it);
}

T_Radio* wld_firstRadFromObjs() {
    amxd_object_t* pRadObjs = amxd_object_get(get_wld_object(), "Radio");
    amxc_llist_it_t* it = amxd_object_first_instance(pRadObjs);
    ASSERTS_NOT_NULL(it, NULL, ME, "none");
    amxd_object_t* radObj = amxc_container_of(it, amxd_object_t, it);
    return wld_rad_fromObj(radObj);
}

T_Radio* wld_lastRadFromObjs() {
    amxd_object_t* pRadObjs = amxd_object_get(get_wld_object(), "Radio");
    ASSERTS_NOT_NULL(pRadObjs, NULL, ME, "none");
    amxc_llist_it_t* it = amxc_llist_get_last(&pRadObjs->instances);
    ASSERTS_NOT_NULL(it, NULL, ME, "none");
    amxd_object_t* radObj = amxc_container_of(it, amxd_object_t, it);
    return wld_rad_fromObj(radObj);
}

T_Radio* wld_nextRad(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, NULL, ME, "NULL");
    amxc_llist_it_t* it = amxc_llist_it_get_next(&pRad->it);
    return wld_rad_fromIt(it);
}
T_Radio* wld_prevRad(T_Radio* pRad) {
    ASSERTS_NOT_NULL(pRad, NULL, ME, "NULL");
    amxc_llist_it_t* it = amxc_llist_it_get_previous(&pRad->it);
    return wld_rad_fromIt(it);
}

static void wld_plugin_set_busctx(amxo_parser_t* parser) {
    amxc_llist_for_each(it, amxo_parser_get_connections(parser)) {
        amxo_connection_t* con = amxc_container_of(it, amxo_connection_t, it);
        if(con->type == AMXO_BUS) {
            wld_plugin_amx = (amxb_bus_ctx_t*) con->priv;
        }
    }
}

void wld_plugin_init(amxd_dm_t* dm,
                     amxo_parser_t* parser) {
    wld_plugin_dm = dm;
    wld_plugin_parser = parser;
    wifi = amxd_dm_findf(wld_plugin_dm, "WiFi.");
    wld_plugin_set_busctx(parser);
}

bool wld_plugin_start() {
    ASSERT_FALSE(init, true, ME, "!!!!!! INIT ERROR !!!!!!!!!");
    init = true;

    const char* env = getenv(ME);
    if(env) {
        sahTraceAddZone(atol(env), ME);
    } else {
        sahTraceAddZone(TRACE_LEVEL_WARNING, ME);
    }
    swl_timespec_getMono(&initTime);

    SAH_TRACEZ_WARNING(ME, "Initializing wld (%s:%s)", __DATE__, __TIME__);

    swl_lib_initialize(wld_plugin_amx);

    /* Generate an initial default WPS pin */
    genSelfPIN();
    wld_event_init();

    // Set BaseMACAddress
    const char* MACStr = getenv("WLAN_ADDR"); /*backw compatible: */
    if(!swl_mac_charIsValidStaMac((swl_macChar_t*) MACStr)) {
        MACStr = getenv("WAN_ADDR");
    }
    if(swl_mac_charIsValidStaMac((swl_macChar_t*) MACStr)) {
        SWL_MAC_CHAR_TO_BIN(&sWanAddr, MACStr);
    } else {
        SAH_TRACEZ_ERROR(ME, "getenv(\"WLAN_ADDR\");failed and getenv(\"WAN_ADDR\");failed ");
    }

    return true;
}

vendor_t* wld_getVendorByName(const char* name) {
    ASSERTS_STR(name, NULL, ME, "Invalid");
    amxc_llist_for_each(it, &vendors) {
        vendor_t* pVendor = amxc_llist_it_get_data(it, vendor_t, it);
        if(pVendor && swl_str_matches(pVendor->name, name)) {
            return pVendor;
        }
    }
    return NULL;
}

const char* wld_getRootObjName() {
    return amxd_object_get_name(get_wld_object(), AMXD_OBJECT_NAMED);
}


void wld_cleanup() {
    SAH_TRACEZ_INFO(ME, "Cleaning up wld plugin");
    wld_deleteAllEps();
    wld_deleteAllVaps();
    wld_deleteAllRadios();
    wld_ssid_cleanAll();
    wld_event_destroy();
    wld_nl80211_cleanupAll();
    wld_channel_cleanAll();
    wld_unregisterAllVendors();
    swl_lib_cleanup();
    wld_plugin_dm = NULL;
    init = false;
    sahTraceRemoveAllZones();
}

vendor_t* wld_registerVendor(const char* name, T_CWLD_FUNC_TABLE* fta) {
    vendor_t* vendor = calloc(1, sizeof(vendor_t));
    if(!vendor) {
        SAH_TRACEZ_ERROR(ME, "calloc failed");
        return NULL;
    }
    vendor->name = strdup(name);
    if(!vendor->name) {
        SAH_TRACEZ_ERROR(ME, "strdup(%s) failed", name);
        free(vendor);
        return NULL;
    }
    SAH_TRACEZ_NOTICE(ME, "register vendor %s", vendor->name);
    wld_dmnMgt_initDmnExecInfo(&vendor->globalHostapd);
    wld_mld_initMgr(&vendor->pMldMgr);
    amxc_llist_append(&vendors, &vendor->it);

    wld_functionTable_init(vendor, fta);

    vendor->fta.mfn_wrad_fsm_delay_commit = fta->mfn_wrad_fsm_delay_commit;

    fta->mfn_sync_radio = vendor->fta.mfn_sync_radio = syncData_Radio2OBJ;
    fta->mfn_sync_ap = vendor->fta.mfn_sync_ap = SyncData_AP2OBJ;
    fta->mfn_sync_ssid = vendor->fta.mfn_sync_ssid = syncData_SSID2OBJ;
    fta->mfn_sync_ep = vendor->fta.mfn_sync_ep = syncData_EndPoint2OBJ;

    return vendor;
}

bool wld_isVendorUsed(vendor_t* vendor) {
    T_Radio* pRad;
    wld_for_eachRad(pRad) {
        if(vendor && pRad->vendor && (pRad->pFA == &vendor->fta)) {
            SAH_TRACEZ_NOTICE(ME, "Vendor %s FTA is still used by Radio %s", vendor->name, pRad->Name);
            return true;
        }
    }
    return false;
}

static bool s_cleanVendor(vendor_t* vendor) {
    if(!vendor) {
        return true;
    }
    ASSERT_FALSE(wld_isVendorUsed(vendor), false, ME, "Fail to clear vendor %s: still used", vendor->name);
    amxc_llist_it_take(&vendor->it);
    wld_dmnMgt_cleanupDmnExecInfo(&vendor->globalHostapd);
    wld_mld_deinitMgr(&vendor->pMldMgr);
    free(vendor->name);
    free(vendor);
    return true;
}

bool wld_unregisterVendor(vendor_t* vendor) {
    if(!vendor) {
        return true;
    }
    amxc_llist_for_each(it, &vendors) {
        if(vendor == amxc_container_of(it, vendor_t, it)) {
            return s_cleanVendor(vendor);
        }
    }
    return false;
}

bool wld_unregisterAllVendors() {
    amxc_llist_for_each(it, &vendors) {
        vendor_t* vendor = amxc_container_of(it, vendor_t, it);
        s_cleanVendor(vendor);
    }
    return (amxc_llist_is_empty(&vendors));
}

const char* radCounterNames[WLD_RAD_EV_MAX] = {
    "FsmCommit",
    "FsmReset",
    "DoubleAssoc",
    "EpAssocFail",
};

int32_t wld_getMaxNrSta() {
    return g_MaxNrSta;
}

uint32_t wld_getMaxNrAPs() {
    return g_MaxNrAPs;
}

uint32_t wld_getMaxNrEPs() {
    return g_MaxNrEPs;
}

uint32_t wld_getMaxNrSSIDs() {
    return (g_MaxNrAPs + g_MaxNrEPs);
}

swl_rc_ne wld_initRadioBaseMac(T_Radio* pR, int32_t idx) {
    ASSERT_NOT_NULL(pR, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_TRUE(idx >= 0, SWL_RC_INVALID_PARAM, ME, "%s: invalid mac indexi %d", pR->Name, idx);
    memcpy(pR->MACAddr, &wld_getWanAddr()->bMac, SWL_MAC_BIN_LEN);
    swl_mac_binAddVal((swl_macBin_t*) pR->MACAddr, idx, -1);
    return SWL_RC_OK;
}

T_Radio* wld_createRadio(const char* name, vendor_t* vendor, int idx) {
    ASSERT_NOT_NULL(name, NULL, ME, "NULL");
    ASSERT_NOT_NULL(vendor, NULL, ME, "NULL");

    T_Radio* pTmpR;
    if(idx < 0) {
        idx = 0;
        wld_for_eachRad(pTmpR) {
            idx = SWL_MAX(idx, (pTmpR->ref_index + 1));
        }
    } else {
        wld_for_eachRad(pTmpR) {
            ASSERT_NOT_EQUALS(idx, pTmpR->ref_index, NULL, ME, "%s: refIfx(%d) already used by radio(%s)", name, idx, pTmpR->Name);
        }
    }

    SAH_TRACEZ_INFO(ME, "Create new radio %s %u", name, idx);


    T_Radio* pR = calloc(1, sizeof(T_Radio));
    if(!pR) {
        SAH_TRACEZ_ERROR(ME, "calloc failed");
        return NULL;
    }
    pR->vendor = vendor;
    pR->wlRadio_SK = -1;
    snprintf(pR->instanceName, IFNAMSIZ, "wifi%d", idx);

    /*
     * init the radio base mac with radio device detection order
     * until it is mapped to dm object
     */
    wld_initRadioBaseMac(pR, idx);

    pR->debug = RAD_POINTER;
    pR->pFA = &vendor->fta;                         // Attach our vendor function table on it!
    swl_str_copy(pR->Name, sizeof(pR->Name), name); // Name of the RADIO!
    static uint32_t index = 0;
    pR->index = index++;
    pR->ref_index = idx;
    pR->wiphy = WLD_NL80211_ID_UNDEF;
    pR->detailedState = CM_RAD_UNKNOWN;
    pR->detailedStatePrev = CM_RAD_UNKNOWN;
    pR->status = RST_UNKNOWN;
    swl_timeMono_t now = swl_time_getMonoSec();
    pR->changeInfo.lastStatusChange = now;
    pR->changeInfo.lastStatusHistogramUpdate = now;
    pR->genericCounters.nrCounters = WLD_RAD_EV_MAX;
    pR->genericCounters.values = pR->counterList;
    pR->genericCounters.names = radCounterNames;
    pR->nrAntenna[COM_DIR_TRANSMIT] = -1;
    pR->nrAntenna[COM_DIR_RECEIVE] = -1;
    pR->nrActiveAntenna[COM_DIR_TRANSMIT] = -1;
    pR->nrActiveAntenna[COM_DIR_RECEIVE] = -1;
    pR->changeInfo.lastDisableTime = swl_time_getMonoSec();

    SAH_TRACEZ_WARNING(ME, "Creating new Radio context [%s, %s]", pR->Name, pR->instanceName);

    wld_scan_init(pR);
    wld_chanmgt_init(pR);
    wld_autoCommitMgr_init(pR);

    if(pR->pFA->mfn_wrad_create_hook(pR)) {
        SAH_TRACEZ_ERROR(ME, "Radio create hook failed");
        free(pR);
        return NULL;
    }

    /* Get our default WPS data */
    pR->wpsConst = &g_wpsConst;

    /* Generate Common WPS Self-PIN */
    wpsPinGen(pR->wpsConst->DefaultPin);

    pR->bgdfs_config.available = false;
    pR->bgdfs_config.enable = false;
    pR->bgdfs_config.channel = 0;
    pR->autoBwSelectMode = BW_SELECT_MODE_DEFAULT;

    /* Update our RO driver parameters in this T_Radio structure */
    pR->pFA->mfn_wrad_supports(pR, NULL, 0);
    pR->maxBitRate = 0; // Mark this as AUTO!

    /* Get Wifi firmware version */
    pR->pFA->mfn_wrad_firmwareVersion(pR);

    /* Get the wifi chip vendor name in standard way, if not already filled by the vendor module in mfn_wrad_supports */
    if(swl_str_isEmpty(pR->chipVendorName)) {
        swl_str_copy(pR->chipVendorName, sizeof(pR->chipVendorName), wld_linuxIfUtils_getChipsetVendor(pR->Name));
    }
    pR->multiUserMIMOSupported = (pR->pFA->mfn_misc_has_support(pR, NULL, "MU_MIMO", 0) == SWL_TRL_TRUE);
    pR->implicitBeamFormingSupported = (pR->pFA->mfn_misc_has_support(pR, NULL, "IMPL_BF", 0) == SWL_TRL_TRUE);
    /* Enabled by default, but not if it's not supported of course. */
    pR->implicitBeamFormingEnabled = pR->implicitBeamFormingSupported;
    pR->explicitBeamFormingSupported = (pR->pFA->mfn_misc_has_support(pR, NULL, "EXPL_BF", 0) == SWL_TRL_TRUE);
    pR->explicitBeamFormingEnabled = pR->explicitBeamFormingSupported;

    pR->pFA->mfn_wrad_beamforming(pR, beamforming_implicit, pR->implicitBeamFormingEnabled, SET);
    pR->pFA->mfn_wrad_beamforming(pR, beamforming_explicit, pR->explicitBeamFormingEnabled, SET);

    if(pR->maxChannelBandwidth == SWL_BW_AUTO) {
        if((pR->supportedFrequencyBands & RFB_5_GHZ) || (pR->supportedFrequencyBands & RFB_6_GHZ)) {
            if(pR->pFA->mfn_misc_has_support(pR, NULL, "320MHz", 0)) {
                pR->maxChannelBandwidth = SWL_BW_320MHZ;
            } else if(pR->pFA->mfn_misc_has_support(pR, NULL, "160MHz", 0)) {
                pR->maxChannelBandwidth = SWL_BW_160MHZ;
            } else {
                pR->maxChannelBandwidth = SWL_BW_80MHZ;
            }
        } else {
            pR->maxChannelBandwidth = (pR->supportedStandards & M_SWL_RADSTD_N) ? SWL_BW_40MHZ : SWL_BW_20MHZ;
        }
    }
    SAH_TRACEZ_INFO(ME, "%s: max bw: %u", pR->Name, pR->maxChannelBandwidth);

    amxc_llist_init(&pR->channelChangeList);

    pR->DFSChannelChangeEventCounter = 0;
    pR->dfsEventNbr = 0;
    pR->dfsFileEventNbr = 0;

    pR->airtimeFairnessEnabled = 0;
    pR->intAirtimeSchedEnabled = 0;
    pR->multiUserMIMOEnabled = 0;
    pR->operatingClass = 0;

    pR->macCfg.useLocalBitForGuest = 0;
    pR->macCfg.localGuestMacOffset = 256;

    if(pR->isAP) {
        if(pR->maxNrHwBss == 0) {
            pR->maxNrHwBss = g_MaxNrAPs;
        } else if(pR->maxNrHwBss > g_MaxNrAPs) {
            g_MaxNrAPs = pR->maxNrHwBss;
        }
        if(pR->maxStations == 0) {
            pR->maxStations = g_MaxNrSta;
        } else if(pR->maxStations > (int) g_MaxNrSta) {
            g_MaxNrSta = pR->maxStations;
        }
        pR->maxNrHwSta = pR->maxStations;
    }

    amxc_llist_init(&pR->scanState.stats.extendedStat);
    amxc_llist_init(&pR->scanState.spectrumResults);

    wld_extMod_initDataList(&pR->extDataList);

    wld_chanmgt_checkInitChannel(pR);

    return pR;
}

int wld_addRadio(const char* name, vendor_t* vendor, int idx) {
    T_Radio* pRad = wld_createRadio(name, vendor, idx);
    ASSERT_NOT_NULL(pRad, SWL_RC_ERROR, ME, "NULL");

    amxc_llist_append(&g_radios, &pRad->it);

    wld_sensing_init(pRad);

    wld_persist_onRadioCreation(pRad);

    pRad->isReady = true;

    SAH_TRACEZ_WARNING(ME, "%s: radInit vendor %s, index %d/%d", name, vendor->name, idx, pRad->ref_index);

    return SWL_RC_OK;
}

void wld_deleteRadioObj(T_Radio* pRad) {
    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    char name[IFNAMSIZ];
    memcpy(name, pRad->Name, sizeof(name));
    SAH_TRACEZ_INFO(ME, "start delete radio %s", name);

    wld_rad_triggerChangeEvent(pRad, WLD_RAD_CHANGE_DESTROY, NULL);

    wld_extMod_cleanupDataList(&pRad->extDataList, pRad);

    wld_radStaMon_destroy(pRad);
    wld_autoCommitMgr_destroy(pRad);
    wld_rad_clearSuppDrvCaps(pRad);
    wld_scan_destroy(pRad);

    amxc_llist_for_each(it, &pRad->scanState.stats.extendedStat) {
        wld_scanReasonStats_t* stat = amxc_llist_it_get_data(it, wld_scanReasonStats_t, it);
        amxc_llist_it_take(&stat->it);
        free(stat->scanReason);
        free(stat);
    }

    wld_chanmgt_cleanup(pRad);

    free(pRad->scanState.cfg.fastScanReasons);
    pRad->scanState.cfg.fastScanReasons = NULL;
    wld_scan_cleanupScanResults(&pRad->scanState.lastScanResults);
    wld_cleanupSurveyReport(&pRad->scanState.lastSurveyReport);
    wld_spectrum_cleanupResults(pRad);
    wld_sensing_cleanup(pRad);

    ASSERT_NOT_NULL(pRad, , ME, "NULL");
    ASSERT_NOT_NULL(pRad->pFA, , ME, "NULL");
    pRad->pFA->mfn_wrad_destroy_hook(pRad);

    if(pRad->pBus != NULL) {
        pRad->pBus->priv = NULL;
        pRad->pBus = NULL;
    }

    free(pRad->dbgOutput);
    pRad->dbgOutput = NULL;

    amxc_llist_it_take(&pRad->it);
    free(pRad);


    SAH_TRACEZ_INFO(ME, "finished delete radio %s", name);
}

void wld_deleteRadio(const char* name) {
    wld_deleteRadioObj(wld_rad_from_name(name));
}

void wld_deleteAllRadios() {
    amxc_llist_for_each(it, &g_radios) {
        T_Radio* pRad = wld_rad_fromIt(it);
        wld_deleteRadioObj(pRad);
    }
}

uint32_t wld_countRadios() {
    uint32_t count = 0;
    T_Radio* pRad = NULL;
    wld_for_eachRad(pRad) {
        count += (pRad->index > 0);
    }
    return count;
}

void wld_deleteAllVaps() {
    T_Radio* pRad;
    wld_for_eachRad(pRad) {
        amxc_llist_for_each(it, &pRad->llAP) {
            T_AccessPoint* pAP = wld_ap_fromIt(it);
            wld_ap_destroy(pAP);
        }
    }
}

void wld_deleteAllEps() {
    T_Radio* pRad;
    wld_for_eachRad(pRad) {
        amxc_llist_for_each(it, &pRad->llEndPoints) {
            T_EndPoint* pEP = wld_endpoint_fromIt(it);
            wld_endpoint_destroy(pEP);
        }
    }
}

/**
 * Return the first radio with the given netdev index
 * In case of duplicate, the first one in the list is returned.
 */
T_Radio* wld_getRadioByIndex(int index) {
    T_Radio* pRad;
    wld_for_eachRad(pRad) {
        if(pRad->index == index) {
            return pRad;
        }
    }
    return NULL;
}

/**
 * Return the first radio with the given netdev index of one of the interface built on top of it.
 * In case of duplicate, the first one in the list is returned.
 */
T_Radio* wld_getRadioOfIfaceIndex(int index) {
    ASSERTS_TRUE(index > 0, NULL, ME, "null");
    T_Radio* pRad;
    T_AccessPoint* pAP;
    T_EndPoint* pEP;
    wld_for_eachRad(pRad) {
        if(pRad->index == index) {
            return pRad;
        }
        wld_rad_forEachAp(pAP, pRad) {
            if(pAP->index == index) {
                return pAP->pRadio;
            }
        }
        wld_rad_forEachEp(pEP, pRad) {
            if(pEP->index == index) {
                return pEP->pRadio;
            }
        }
    }
    return NULL;
}

/**
 * Return the first radio with the given netdev ifName of one of the interface built on top of it.
 * In case of duplicate, the first one in the list is returned.
 */
T_Radio* wld_getRadioOfIfaceName(const char* ifName) {
    ASSERTS_STR(ifName, NULL, ME, "null");
    T_Radio* pRad = wld_rad_from_name(ifName);
    if(pRad != NULL) {
        return pRad;
    }
    T_AccessPoint* pAP = wld_vap_from_name(ifName);
    if(pAP != NULL) {
        return pAP->pRadio;
    }
    T_EndPoint* pEP = wld_vep_from_name(ifName);
    if(pEP != NULL) {
        return pEP->pRadio;
    }
    return NULL;
}

/**
 * Return the first radio with the given netdev index
 * In case of duplicate, the first one in the list is returned.
 */
T_Radio* wld_getRadioByName(const char* name) {
    T_Radio* pRad;
    wld_for_eachRad(pRad) {
        if(swl_str_matches(pRad->Name, name)) {
            return pRad;
        }
    }
    return NULL;
}

T_Radio* wld_getRadioByWiPhyId(int32_t wiPhyId) {
    T_Radio* pRad = NULL;
    wld_for_eachRad(pRad) {
        if(pRad->wiphy == (uint32_t) wiPhyId) {
            SAH_TRACEZ_INFO(ME, "Matched wiPhyId :%d pRad->wiphy:%d", wiPhyId, pRad->wiphy);
            return pRad;
        }
    }
    SAH_TRACEZ_INFO(ME, "Not Matched wiPhyId :%d with any pRad->wiphy", wiPhyId);
    return NULL;
}

T_Radio* wld_getUinitRadioByBand(swl_freqBandExt_e band) {
    T_Radio* pRadCand = NULL;
    T_Radio* pRad;
    wld_for_eachRad(pRad) {
        if((SWL_BIT_IS_SET(pRad->supportedFrequencyBands, band)) && (pRad->pBus == NULL)) {
            //prio to rad device initially set by driver in the required freqBand
            if(pRad->operatingFrequencyBand == band) {
                return pRad;
            } else if(pRadCand == NULL) {
                pRadCand = pRad;
            }
        }
    }
    return pRadCand;
}

T_EndPoint* wld_getEndpointByAlias(const char* name) {
    T_Radio* pRad = NULL;
    T_EndPoint* pEP = NULL;
    wld_for_eachRad(pRad) {
        wld_rad_forEachEp(pEP, pRad) {
            if(swl_str_matches(pEP->alias, name)) {
                return pEP;
            }
        }
    }
    return NULL;
}

T_AccessPoint* wld_getAccesspointByAlias(const char* name) {
    T_Radio* pRad = NULL;
    T_AccessPoint* pAP = NULL;
    wld_for_eachRad(pRad) {
        wld_rad_forEachAp(pAP, pRad) {
            if(swl_str_matches(pAP->alias, name)) {
                return pAP;
            }
        }
    }
    return NULL;
}

T_AccessPoint* wld_getAccesspointByAddress(unsigned char* macAddress) {
    ASSERTS_NOT_NULL(macAddress, NULL, ME, "NULL");
    T_Radio* pRad = NULL;
    T_AccessPoint* pAP = NULL;
    wld_for_eachRad(pRad) {
        wld_rad_forEachAp(pAP, pRad) {
            T_SSID* pBSSID = pAP->pSSID;
            if(pBSSID && SWL_MAC_BIN_MATCHES(pBSSID->BSSID, macAddress)) {
                return pAP;
            }
        }
    }
    return NULL;
}

T_Radio* wld_getRadioByAddress(unsigned char* macAddress) {
    ASSERTS_NOT_NULL(macAddress, NULL, ME, "NULL");
    T_Radio* pRad;
    wld_for_eachRad(pRad) {
        if(SWL_MAC_BIN_MATCHES(pRad->MACAddr, macAddress)) {
            return pRad;
        }
    }
    return NULL;
}

/**
 * get the first radio interface for given frequency.
 */
T_Radio* wld_getRadioByFrequency(swl_freqBand_e freqBand) {
    T_Radio* pRad;
    wld_for_eachRad(pRad) {
        if(pRad->operatingFrequencyBand == (swl_freqBandExt_e) freqBand) {
            return pRad;
        }
    }
    return NULL;
}

/**
 * Returns the number of Accesspoint, that have a positive id equal to
 * to id.
 * So negative (default non MLD) id will always return 0.
 */
uint32_t wld_getNrApMldLinksById(int32_t id) {
    if(id < 0) {
        return 0;
    }
    uint32_t nrAp = 0;
    T_Radio* pRad = NULL;
    wld_for_eachRad(pRad) {
        T_AccessPoint* pAP = NULL;
        if(!SWL_BIT_IS_SET(pRad->operatingStandards, SWL_RADSTD_BE)) {
            continue;
        }

        wld_rad_forEachAp(pAP, pRad) {
            if((pAP->pSSID != NULL) && (pAP->pSSID->mldUnit == id)) {
                nrAp++;
            }
        }
    }
    return nrAp;
}

amxd_object_t* get_wld_object() {
    return wifi;
}

amxb_bus_ctx_t* get_wld_plugin_bus() {
    return wld_plugin_amx;
}

amxd_dm_t* get_wld_plugin_dm() {
    return wld_plugin_dm;
}

amxo_parser_t* get_wld_plugin_parser() {
    return wld_plugin_parser;
}

/**
 * Get a char vendor parameter.
 * If pR or parameter are NULL, it returns NULL.
 * If the parameter does not exist, or the lenght of the result is zero, we return the default.
 * Otherwise it returns the parameter value from the data model.
 */
char* wld_getVendorParam(const T_Radio* pR, const char* parameter, const char* def) {
    ASSERT_NOT_NULL(pR, NULL, ME, "NULL");
    ASSERT_NOT_NULL(parameter, NULL, ME, "NULL");

    amxd_object_t* vendorObject = amxd_object_get(pR->pBus, "Vendor");
    char* value = amxd_object_get_cstring_t(vendorObject, parameter, NULL);

    if(!swl_str_isEmpty(value)) {
        return value;
    }
    free(value);

    if(def != NULL) {
        return strdup(def);
    }
    return NULL;
}

/**
 * Get an integer vendor parameter.
 * If the parameter does not exist, or pR or parameter is null, it returns default value.
 * Otherwise it returns the parameter value from the data model.
 */
int wld_getVendorParam_int(const T_Radio* pR, const char* parameter, const int def) {
    ASSERT_NOT_NULL(pR, def, ME, "NULL");
    ASSERT_NOT_NULL(parameter, def, ME, "NULL");

    amxd_object_t* vendorObject = amxd_object_get(pR->pBus, "Vendor");
    amxd_status_t s = amxd_status_unknown_error;
    int32_t value = amxd_object_get_int32_t(vendorObject, parameter, &s);

    if(s == amxd_status_ok) {
        return value;
    } else {
        return def;
    }
}

bool wld_isInternalBssidBin(const swl_macBin_t* bssid) {
    /* Search in all my radio 2.4GhZ and 5Ghz */
    amxc_llist_for_each(rad_it, &g_radios) {
        T_Radio* pRad = amxc_llist_it_get_data(rad_it, T_Radio, it);
        /* Search my own radio into into VAP list */
        amxc_llist_for_each(ap_it, &pRad->llAP) {
            T_AccessPoint* pAP = amxc_llist_it_get_data(ap_it, T_AccessPoint, it);
            T_SSID* pSSID = pAP->pSSID;
            SAH_TRACEZ_INFO(ME, "VAP " SWL_MAC_FMT " checking into storedAP list ...", SWL_MAC_ARG(pSSID->MACAddress));
            if(swl_mac_binMatches((swl_macBin_t*) pSSID->MACAddress, bssid)) {
                return true;
            }
        }
    }
    return false;
}

/**
 * @deprecated in favor of #wld_isInternalBssidBin
 */
bool wld_isInternalBSSID(const char bssid[ETHER_ADDR_STR_LEN]) {
    swl_macBin_t bssidBin = SWL_MAC_BIN_NEW();
    SWL_MAC_CHAR_TO_BIN(&bssidBin, bssid);
    return wld_isInternalBssidBin(&bssidBin);
}

/**
 * Function returning mask of the currently configured freq bands of all available radio's,
 * ignoring the radio provided. If ingoreRad is NULL, the or of all radio's is returned.
 *
 */
swl_freqBandExt_m wld_getAvailableFreqBands(T_Radio* ignoreRad) {
    swl_freqBandExt_m freqMask = 0;
    T_Radio* tmpRad = NULL;
    wld_for_eachRad(tmpRad) {
        if((tmpRad != ignoreRad) && (tmpRad != NULL)) {
            freqMask |= (1 << tmpRad->operatingFrequencyBand);
        }
    }
    return freqMask;
}


swl_timeSpecMono_t* wld_getInitTime() {
    return &initTime;
}

amxd_status_t _Reset(amxd_object_t* obj _UNUSED,
                     amxd_function_t* func _UNUSED,
                     amxc_var_t* args _UNUSED,
                     amxc_var_t* retval _UNUSED) {

    amxd_object_t* rootObj = amxd_dm_get_root(get_wld_plugin_dm());
    amxo_parser_t* parser = get_wld_plugin_parser();
    amxo_parser_parse_string(parser, "include '${definition_file}';", rootObj);
    amxo_parser_parse_string(parser, "include '${odl.dm-defaults}';", rootObj);
    /**
     * Wait for all the definition and default parameters to be processed
     * before continuing with internal syncing.
     */
    while(amxp_signal_read() == 0) {
    }
    wld_vendorModuleMgr_loadDefaultsAll();

    T_Radio* tmpRad = NULL;
    wld_for_eachRad(tmpRad) {
        tmpRad->fsmRad.FSM_SyncAll = TRUE;
        wld_autoCommitMgr_notifyRadEdit(tmpRad);
    }

    return amxd_status_ok;
}
