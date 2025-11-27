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

#include "wld.h"
#include "wld_mld.h"
#include "wld_ssid.h"
#include "wld_util.h"
#include "wld_radioOperatingStandards.h"
#include "wld_chanmgt.h"
#include "wld_eventing.h"
#include "wld_apMld.h"

#define ME "mld"

static const char* sGroupNames[WLD_SSID_TYPE_MAX] = {"UNKNOWN", "APMLD", "STAMLD"};

static void s_sendChangeEvent(wld_mldChangeEvent_e event, wld_ssidType_e mldType, int32_t mldUnit, T_SSID* pEvtLinkSsid) {
    wld_mldChange_t change;
    memset(&change, 0, sizeof(change));
    change.event = event;
    change.mldType = mldType;
    change.mldUnit = mldUnit;
    change.pEvtLinkSsid = pEvtLinkSsid;
    SAH_TRACEZ_INFO(ME, "send mld notif %d (type:%s unit:%d evtLinkSsid:(%s))",
                    event, sGroupNames[mldType], mldUnit, (pEvtLinkSsid ? pEvtLinkSsid->Name : ""));
    wld_event_trigger_callback(gWld_queue_mld_onChangeEvent, &change);
}

static wld_mldGroup_t* s_getGroup(wld_mld_t* pMld) {
    ASSERTS_NOT_NULL(pMld, NULL, ME, "NULL");
    ASSERTS_TRUE(amxc_llist_it_is_in_list(&pMld->it), NULL, ME, "Not a list item");
    ASSERTS_TRUE(pMld->pGroup, NULL, ME, "Not a list item");
    ASSERT_EQUALS(&pMld->pGroup->mlds, pMld->it.llist, NULL, ME, "unmatched list");
    return pMld->pGroup;
}

const char* s_getMldGroupName(wld_mld_t* pMld) {
    ASSERTS_NOT_NULL(pMld, "", ME, "NULL");
    wld_mldGroup_t* pGroup = s_getGroup(pMld);
    ASSERTS_NOT_NULL(pGroup, "", ME, "no mld group");
    ASSERT_TRUE(pGroup->type < SWL_ARRAY_SIZE(sGroupNames), "", ME, "type is out of range");
    return sGroupNames[pGroup->type];
}

const char* s_getLinkName(wld_mldLink_t* pLink) {
    const char* name = "unknown";
    ASSERTS_NOT_NULL(pLink, name, ME, "NULL");
    ASSERTS_NOT_NULL(pLink->pSSID, name, ME, "NULL");
    return pLink->pSSID->Name;
}

const char* wld_mld_getLinkName(wld_mldLink_t* pLink) {
    return s_getLinkName(pLink);
}

static wld_mldMgr_t* s_getMgr(T_SSID* pSSID) {
    if((pSSID == NULL) ||
       (pSSID->RADIO_PARENT == NULL) ||
       (pSSID->RADIO_PARENT->vendor == NULL) ||
       (pSSID->RADIO_PARENT->vendor->pMldMgr == NULL) ||
       (!pSSID->RADIO_PARENT->vendor->pMldMgr->init)) {
        return NULL;
    }
    return pSSID->RADIO_PARENT->vendor->pMldMgr;
}

static wld_mldGroup_t* s_findGroupInMgr(wld_mldMgr_t* pMgr, wld_ssidType_e type) {
    ASSERTS_NOT_NULL(pMgr, NULL, ME, "NULL");
    ASSERT_TRUE(pMgr->init, NULL, ME, "Not initialized");
    for(uint32_t i = 0; i < WLD_SSID_TYPE_MAX; i++) {
        if(pMgr->groups[i].type == type) {
            return &pMgr->groups[i];
        }
    }
    return NULL;
}

static wld_mldGroup_t* s_getLinkGroup(wld_mldLink_t* pLink) {
    ASSERTS_NOT_NULL(pLink, NULL, ME, "NULL");
    return s_getGroup(pLink->pMld);
}

static bool s_isTypedLink(wld_mldLink_t* pLink) {
    ASSERTS_NOT_NULL(pLink, false, ME, "NULL");
    wld_ssidType_e type = wld_ssid_getType(pLink->pSSID);
    ASSERTS_NOT_EQUALS(type, WLD_SSID_TYPE_UNKNOWN, false, ME, "ssid not typed");
    wld_mldGroup_t* pGroup = s_getLinkGroup(pLink);
    ASSERTS_NOT_NULL(pGroup, false, ME, "no mld group");
    return (pGroup->type == type);
}

static swl_rc_ne s_findMldInGroup(wld_mldGroup_t* pGroup, int32_t unit, wld_mld_t** ppMld, wld_mld_t** ppNextMld) {
    W_SWL_SETPTR(ppMld, NULL);
    W_SWL_SETPTR(ppNextMld, NULL);
    ASSERTS_NOT_NULL(pGroup, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_TRUE(unit >= 0, SWL_RC_INVALID_PARAM, ME, "invalid mld unit");
    wld_mld_t* pNextMld = NULL;
    amxc_llist_for_each(it, &pGroup->mlds) {
        wld_mld_t* pMld = amxc_container_of(it, wld_mld_t, it);
        if(pMld->unit == unit) {
            W_SWL_SETPTR(ppMld, pMld);
        }
        //mlds are sorted by mld unit
        if((pMld->unit > unit) && (pNextMld == NULL)) {
            pNextMld = pMld;
            W_SWL_SETPTR(ppNextMld, pNextMld);
        }
    }
    return SWL_RC_OK;
}

static wld_mld_t* s_setMldInGroup(wld_mldGroup_t* pGroup, int32_t unit) {
    ASSERTS_NOT_NULL(pGroup, NULL, ME, "NULL");
    ASSERTS_TRUE(unit >= 0, NULL, ME, "invalid mld unit");
    wld_mld_t* pMld = NULL;
    wld_mld_t* pNextMld = NULL;
    s_findMldInGroup(pGroup, unit, &pMld, &pNextMld);
    ASSERTI_NULL(pMld, pMld, ME, "mld %d (%p) already in list %d", unit, pMld, pGroup->type);
    SAH_TRACEZ_INFO(ME, "create mld %d in list %d", unit, pGroup->type);
    pMld = calloc(1, sizeof(*pMld));
    ASSERT_NOT_NULL(pMld, pMld, ME, "alloc failure");
    amxc_llist_init(&pMld->links);
    pMld->unit = unit;
    pMld->pGroup = pGroup;
    if(pNextMld == NULL) {
        amxc_llist_append(&pGroup->mlds, &pMld->it);
    } else {
        //mlds are sorted by mld unit
        amxc_llist_it_insert_before(&pNextMld->it, &pMld->it);
    }
    return pMld;
}

static wld_mld_t* s_getTargetMld(T_SSID* pSSID, int32_t unit) {
    ASSERTS_NOT_NULL(pSSID, NULL, ME, "NULL");
    ASSERTS_TRUE(unit >= 0, NULL, ME, "No mld unit");
    T_Radio* pRad = pSSID->RADIO_PARENT;
    ASSERTS_NOT_NULL(pRad, NULL, ME, "NULL");
    ASSERTS_TRUE(SWL_BIT_IS_SET(pRad->supportedStandards, SWL_RADSTD_BE), NULL, ME, "11be not supp");
    wld_ssidType_e mldType = wld_ssid_getType(pSSID);
    wld_mldGroup_t* pGroup = s_findGroupInMgr(s_getMgr(pSSID), mldType);
    ASSERTS_NOT_NULL(pGroup, NULL, ME, "no group available for mld type %d", mldType);
    return s_setMldInGroup(pGroup, unit);
}

static void s_deinitMld(wld_mld_t* pMld);

static wld_mldLink_t* s_takeLink(wld_mldLink_t* pLink) {
    ASSERTS_NOT_NULL(pLink, NULL, ME, "NULL");
    wld_mld_t* pMld = pLink->pMld;
    wld_apMld_deleteAffiliatedAPObjects(pLink);
    wld_mld_resetLinkId(pLink);
    amxc_llist_it_take(&pLink->it);
    pLink->configured = false;
    pLink->pMld = NULL;
    if(pMld != NULL) {
        SAH_TRACEZ_INFO(ME, "take link %d (%p) out of mld %p", pLink->linkId, pLink, pMld);
        if(amxc_llist_is_empty(&pMld->links)) {
            wld_mldGroup_t* pGroup = pMld->pGroup;
            uint8_t unit = pMld->unit;
            wld_apMld_clearMld(pMld);
            s_deinitMld(pMld);
            if(pGroup != NULL) {
                s_sendChangeEvent(WLD_MLD_EVT_DEL, pGroup->type, unit, pLink->pSSID);
            }
        }
    }
    return pLink;
}

static void s_deinitLink(wld_mldLink_t* pLink) {
    ASSERTS_NOT_NULL(pLink, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "deinit link %d (%p)", pLink->linkId, pLink);
    s_takeLink(pLink);
    if(pLink->pSSID) {
        pLink->pSSID->pMldLink = NULL;
    }
    free(pLink);
}

static void s_deinitLinkIt(amxc_llist_it_t* it) {
    ASSERTS_NOT_NULL(it, , ME, "NULL");
    wld_mldLink_t* pLink = amxc_container_of(it, wld_mldLink_t, it);
    s_deinitLink(pLink);
}

static void s_deinitMld(wld_mld_t* pMld) {
    ASSERTS_NOT_NULL(pMld, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "deinit mld %d (%p) including %zu links ", pMld->unit, pMld, amxc_llist_size(&pMld->links));
    amxc_llist_it_take(&pMld->it);
    amxc_llist_clean(&pMld->links, s_deinitLinkIt);
    pMld->pGroup = NULL;
    free(pMld);
}

static void s_deinitMldIt(amxc_llist_it_t* it) {
    ASSERTS_NOT_NULL(it, , ME, "NULL");
    wld_mld_t* pMld = amxc_container_of(it, wld_mld_t, it);
    s_deinitMld(pMld);
}

static void s_deinitMldGroup(wld_mldGroup_t* pGroup) {
    ASSERTS_NOT_NULL(pGroup, , ME, "NULL");
    amxc_llist_clean(&pGroup->mlds, s_deinitMldIt);
}

static void s_initMldGroup(wld_mldGroup_t* pGroup, wld_ssidType_e type) {
    ASSERTS_NOT_NULL(pGroup, , ME, "NULL");
    pGroup->type = type;
    amxc_llist_init(&pGroup->mlds);
}

swl_rc_ne wld_mld_initMgr(wld_mldMgr_t** ppMgr) {
    ASSERT_NOT_NULL(ppMgr, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_mldMgr_t* pMgr = *ppMgr;
    if(pMgr == NULL) {
        pMgr = calloc(1, sizeof(*pMgr));
        ASSERT_NOT_NULL(pMgr, SWL_RC_ERROR, ME, "fail to alloc mld mgr");
        *ppMgr = pMgr;
    }
    ASSERTI_FALSE(pMgr->init, SWL_RC_OK, ME, "already initialized");
    for(wld_ssidType_e type = WLD_SSID_TYPE_UNKNOWN; type < WLD_SSID_TYPE_MAX; type++) {
        s_initMldGroup(&pMgr->groups[type], type);
    }
    pMgr->init = true;
    return SWL_RC_OK;
}

swl_rc_ne wld_mld_deinitMgr(wld_mldMgr_t** ppMgr) {
    ASSERT_NOT_NULL(ppMgr, SWL_RC_INVALID_PARAM, ME, "NULL");
    wld_mldMgr_t* pMgr = *ppMgr;
    ASSERTI_NOT_NULL(pMgr, SWL_RC_OK, ME, "already deinitialized");
    for(wld_ssidType_e type = WLD_SSID_TYPE_UNKNOWN; type < WLD_SSID_TYPE_MAX; type++) {
        s_deinitMldGroup(&pMgr->groups[type]);
    }
    pMgr->init = false;
    free(pMgr);
    *ppMgr = NULL;
    return SWL_RC_OK;
}

wld_mldLink_t* wld_mld_registerLink(T_SSID* pSSID, int32_t unit) {
    ASSERTS_NOT_NULL(pSSID, NULL, ME, "NULL");
    wld_mld_t* pTgtMld = s_getTargetMld(pSSID, unit);
    wld_mldLink_t* pLink = pSSID->pMldLink;
    wld_mld_t* pMld = NULL;
    if(pLink != NULL) {
        if(pTgtMld == NULL) {
            SAH_TRACEZ_INFO(ME, "%s: remove link %p, set with unit %d", pSSID->Name, pLink, unit);
            s_deinitLink(pLink);
            return NULL;
        }
        pMld = pLink->pMld;
        if(pMld == pTgtMld) {
            SAH_TRACEZ_INFO(ME, "%s: same target mld %p (%d) for link %p, set with unit %d", pSSID->Name, pMld, pMld->unit, pLink, unit);
            return pLink;
        }
        if(pMld != NULL) {
            SAH_TRACEZ_INFO(ME, "%s: extract link %p from mld %p, unit %d->%d", pSSID->Name, pLink, pMld, pMld->unit, unit);
            s_takeLink(pLink);
            pMld = NULL;
        } else {
            SAH_TRACEZ_ERROR(ME, "%s: link %p exists without mld => remove it", pSSID->Name, pLink);
            s_deinitLink(pLink);
            pLink = NULL;
        }
    }
    if(pLink == NULL) {
        ASSERTI_NOT_NULL(pTgtMld, pLink, ME, "%s: no target unit %d", pSSID->Name, unit);
        pLink = calloc(1, sizeof(*pLink));
        ASSERT_NOT_NULL(pLink, pLink, ME, "%s: fail to alloc pLink (unit %d)", pSSID->Name, unit);
        pLink->linkId = NO_LINK_ID;
    }
    SAH_TRACEZ_INFO(ME, "%s: append link %p to mld %p, unit %d", pSSID->Name, pLink, pTgtMld, pTgtMld->unit);
    amxc_llist_append(&pTgtMld->links, &pLink->it);
    pLink->pMld = pTgtMld;
    pLink->pSSID = pSSID;
    pSSID->pMldLink = pLink;
    if(amxc_llist_size(&pTgtMld->links) == 1) {
        s_sendChangeEvent(WLD_MLD_EVT_ADD, pTgtMld->pGroup->type, unit, pSSID);
        wld_apMld_notifyChange(pTgtMld, WLD_MLD_EVT_ADD, "First SSID linked to MLD");
    }
    return pLink;
}

swl_rc_ne wld_mld_unregisterLink(T_SSID* pSSID) {
    ASSERTS_NOT_NULL(pSSID, SWL_RC_INVALID_PARAM, ME, "NULL");
    s_deinitLink(pSSID->pMldLink);
    pSSID->pMldLink = NULL;
    return SWL_RC_OK;
}

wld_mldLink_t* wld_mld_getNeighLinkByRad(wld_mldLink_t* pLink, T_Radio* pRad) {
    ASSERTS_NOT_NULL(pLink, NULL, ME, "NULL");
    ASSERTS_NOT_NULL(pRad, NULL, ME, "NULL");
    ASSERTS_TRUE(amxc_llist_it_is_in_list(&pLink->it), NULL, ME, "Not a list item");
    amxc_llist_for_each(it, pLink->it.llist) {
        wld_mldLink_t* pLink = amxc_container_of(it, wld_mldLink_t, it);
        if((pLink->pSSID != NULL) && (pLink->pSSID->RADIO_PARENT == pRad)) {
            return pLink;
        }
    }
    return NULL;
}

wld_mldLink_t* wld_mld_getNeighLinkByMacAddress(wld_mldLink_t* pLink, swl_macBin_t* macBin) {
    ASSERTS_NOT_NULL(pLink, NULL, ME, "NULL");
    ASSERTS_NOT_NULL(macBin, NULL, ME, "NULL");
    ASSERTS_TRUE(amxc_llist_it_is_in_list(&pLink->it), NULL, ME, "Not a list item");
    amxc_llist_for_each(it, pLink->it.llist) {
        wld_mldLink_t* pLink = amxc_container_of(it, wld_mldLink_t, it);
        if((pLink->pSSID != NULL) && SWL_MAC_BIN_MATCHES(pLink->pSSID->BSSID, macBin)) {
            return pLink;
        }
    }
    return NULL;
}

swl_rc_ne wld_mld_setPrimaryLink(wld_mldLink_t* pLink) {
    ASSERTS_NOT_NULL(pLink, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_TRUE(wld_mld_isLinkEnabled(pLink), SWL_RC_ERROR, ME, "disabled link %s can not be set as primary", s_getLinkName(pLink));
    wld_mld_t* pMld = pLink->pMld;
    ASSERT_NOT_NULL(pMld, SWL_RC_ERROR, ME, "NULL");
    SAH_TRACEZ_INFO(ME, "set primary link %s of mld unit %d",
                    s_getLinkName(pLink), pMld->unit);
    pMld->pPrimLink = pLink;
    return SWL_RC_OK;
}

wld_mldLink_t* wld_mld_getPrimaryLink(wld_mldLink_t* pLink) {
    ASSERTS_NOT_NULL(pLink, NULL, ME, "NULL");
    ASSERTI_TRUE(s_isTypedLink(pLink), NULL, ME, "untyped link %s has no primary link", s_getLinkName(pLink));
    wld_mld_t* pMld = pLink->pMld;
    ASSERT_NOT_NULL(pMld, NULL, ME, "NULL");
    amxc_llist_for_each(it, &pMld->links) {
        wld_mldLink_t* pTmpLink = amxc_container_of(it, wld_mldLink_t, it);
        if(pMld->pPrimLink == pTmpLink) {
            return pTmpLink;
        }
    }
    return NULL;
}

T_SSID* wld_mld_getLinkSsid(wld_mldLink_t* pLink) {
    ASSERTS_NOT_NULL(pLink, NULL, ME, "NULL");
    return pLink->pSSID;
}

T_SSID* wld_mld_getLinkSsidByLinkId(wld_mldLink_t* pLink, int8_t linkId) {
    ASSERTS_FALSE(linkId < 0, NULL, ME, "invalid link ID");
    ASSERTS_NOT_NULL(pLink, NULL, ME, "NULL");
    amxc_llist_for_each(it, pLink->it.llist) {
        wld_mldLink_t* pNeighLink = amxc_container_of(it, wld_mldLink_t, it);
        if(wld_mld_getLinkId(pNeighLink) == linkId) {
            return wld_mld_getLinkSsid(pNeighLink);
        }
    }
    return NULL;
}

T_SSID* wld_mld_getPrimaryLinkSsid(wld_mldLink_t* pLink) {
    return wld_mld_getLinkSsid(wld_mld_getPrimaryLink(pLink));
}

const char* wld_mld_getPrimaryLinkIfName(wld_mldLink_t* pLink) {
    return wld_ssid_getIfName(wld_mld_getPrimaryLinkSsid(pLink));
}

int32_t wld_mld_getPrimaryLinkIfIndex(wld_mldLink_t* pLink) {
    return wld_ssid_getIfIndex(wld_mld_getPrimaryLinkSsid(pLink));
}

bool wld_mld_isLinkActive(wld_mldLink_t* pLink) {
    ASSERTS_NOT_NULL(pLink, false, ME, "NULL");
    return (pLink->linkId >= 0);
}

bool wld_mld_isLinkEnabled(wld_mldLink_t* pLink) {
    ASSERTS_TRUE(s_isTypedLink(pLink), false, ME, "NULL");
    T_SSID* pSSID = pLink->pSSID;
    ASSERTS_NOT_NULL(pSSID, false, ME, "NULL");
    T_Radio* pRad = pSSID->RADIO_PARENT;
    ASSERTS_NOT_NULL(pRad, false, ME, "NULL");
    return (pSSID->enable && pRad->enable);
}

uint32_t wld_mld_countNeighLinks(wld_mldLink_t* pLink) {
    ASSERTS_NOT_NULL(pLink, 0, ME, "NULL");
    ASSERTS_TRUE(amxc_llist_it_is_in_list(&pLink->it), 0, ME, "Not a list item");
    return amxc_llist_size(pLink->it.llist);
}

uint32_t wld_mld_countNeighActiveLinks(wld_mldLink_t* pLink) {
    uint32_t count = 0;
    ASSERTS_NOT_NULL(pLink, count, ME, "NULL");
    ASSERTS_TRUE(amxc_llist_it_is_in_list(&pLink->it), count, ME, "Not a list item");
    amxc_llist_for_each(it, pLink->it.llist) {
        wld_mldLink_t* pLink = amxc_container_of(it, wld_mldLink_t, it);
        count += wld_mld_isLinkActive(pLink);
    }
    return count;
}

uint32_t wld_mld_countNeighEnabledLinks(wld_mldLink_t* pLink) {
    uint32_t count = 0;
    ASSERTS_NOT_NULL(pLink, count, ME, "NULL");
    ASSERTS_TRUE(amxc_llist_it_is_in_list(&pLink->it), count, ME, "Not a list item");
    amxc_llist_for_each(it, pLink->it.llist) {
        wld_mldLink_t* pLink = amxc_container_of(it, wld_mldLink_t, it);
        count += wld_mld_isLinkEnabled(pLink);
    }
    return count;
}

uint32_t wld_mld_countNeighUsableLinks(wld_mldLink_t* pLink) {
    uint32_t count = 0;
    ASSERTS_NOT_NULL(pLink, count, ME, "NULL");
    ASSERTS_TRUE(amxc_llist_it_is_in_list(&pLink->it), count, ME, "Not a list item");
    amxc_llist_for_each(it, pLink->it.llist) {
        wld_mldLink_t* pLink = amxc_container_of(it, wld_mldLink_t, it);
        count += wld_mld_isLinkUsable(pLink);
    }
    return count;
}

swl_rc_ne wld_mld_setLinkId(wld_mldLink_t* pLink, int32_t linkId) {
    ASSERTS_NOT_NULL(pLink, SWL_RC_INVALID_PARAM, ME, "NULL");
    if((!s_isTypedLink(pLink)) && (linkId != NO_LINK_ID)) {
        SAH_TRACEZ_WARNING(ME, "link %s is untyped: resetting linkId (in:%d)",
                           s_getLinkName(pLink), linkId);
        linkId = NO_LINK_ID;
    }
    if(linkId >= 0) {
        wld_mld_saveLinkConfigured(pLink, true);
        wld_apMld_createAffiliatedAPObject(pLink, linkId);
    }
    ASSERTS_NOT_EQUALS(pLink->linkId, linkId, SWL_RC_OK, ME, "same value");
    if(linkId == NO_LINK_ID) {
        wld_ssid_resetMloStats(pLink->pSSID);
        if(pLink->pMld->pPrimLink == pLink) {
            SAH_TRACEZ_WARNING(ME, "reset primary link %s", s_getLinkName(pLink));
            pLink->pMld->pPrimLink = NULL;
        }
    }
    SAH_TRACEZ_INFO(ME, "set link %s id: %d => %d", s_getLinkName(pLink), pLink->linkId, linkId);
    /*
     * if linkId is set to -1 when it was primary (ie current linkId value is 0)
     * then a notification for MLDPrimaryChange must be sent so that other MLD Links update the primary link
     */
    pLink->linkId = linkId;
    wld_apMld_updateAffAP(pLink);
    return SWL_RC_OK;
}

swl_rc_ne wld_mld_resetLinkId(wld_mldLink_t* pLink) {
    return wld_mld_setLinkId(pLink, NO_LINK_ID);
}

int16_t wld_mld_getLinkId(const wld_mldLink_t* pLink) {
    ASSERTS_NOT_NULL(pLink, NO_LINK_ID, ME, "NULL");
    return pLink->linkId;
}

const char* wld_mld_getLinkIfName(wld_mldLink_t* pLink) {
    ASSERTS_NOT_NULL(pLink, "", ME, "NULL");
    return wld_ssid_getIfName(pLink->pSSID);
}

bool wld_mld_checkUsableLinkBasicConditions(wld_mldLink_t* pLink) {
    ASSERTS_NOT_NULL(pLink, false, ME, "NULL");
    if(!wld_mld_isLinkEnabled(pLink)) {
        return false;
    }
    T_SSID* pSSID = pLink->pSSID;
    if(!wld_ssid_hasMloSupport(pSSID)) {
        return false;
    }
    if(pSSID->mldUnit < 0) {
        return false;
    }
    if(swl_mac_binIsNull((swl_macBin_t*) pSSID->MACAddress)) {
        return false;
    }
    return true;
}

bool wld_mld_isLinkUsable(wld_mldLink_t* pLink) {
    if(!wld_mld_checkUsableLinkBasicConditions(pLink)) {
        return false;
    }
    if(wld_ssid_getType(pLink->pSSID) == WLD_SSID_TYPE_AP) {
        return wld_apMld_hasSharedConnectionConf(pLink->pSSID->AP_HOOK);
    }
    return true;
}

bool wld_mld_saveLinkConfigured(wld_mldLink_t* pLink, bool flag) {
    ASSERTS_NOT_NULL(pLink, false, ME, "NULL");
    flag &= wld_mld_isLinkEnabled(pLink);
    ASSERTI_NOT_EQUALS(pLink->configured, flag, true, ME, "same link %s config value %d", s_getLinkName(pLink), flag);
    SAH_TRACEZ_INFO(ME, "%s: set link configured %d", s_getLinkName(pLink), flag);
    pLink->configured = flag;
    wld_mld_t* pMld = pLink->pMld;
    ASSERTI_NOT_NULL(pMld, !flag, ME, "link %s has no mld", s_getLinkName(pLink));
    amxc_llist_t* pList = &pMld->links;
    if(!flag) {
        amxc_llist_append(pList, &pLink->it);
    } else {
        bool sorted = false;
        amxc_llist_for_each_reverse(it, pList) {
            wld_mldLink_t* pLinkRef = amxc_container_of(it, wld_mldLink_t, it);
            if((pLinkRef != pLink) && (pLinkRef->configured)) {
                amxc_llist_it_insert_after(it, &pLink->it);
                sorted = true;
                break;
            }
        }
        if(!sorted) {
            amxc_llist_prepend(pList, &pLink->it);
        }
    }
    return true;
}

bool wld_mld_setLinkConfigured(wld_mldLink_t* pLink, bool flag) {
    bool ret = wld_mld_saveLinkConfigured(pLink, flag);
    ASSERTS_TRUE(ret, false, ME, "fail to save new link config flag %d", flag);
    wld_mld_t* pMld = pLink->pMld;
    if(pMld->pGroup != NULL) {
        s_sendChangeEvent(WLD_MLD_EVT_UPDATE, pMld->pGroup->type, pMld->unit, pLink->pSSID);
    }
    return true;
}

bool wld_mld_isLinkConfigured(wld_mldLink_t* pLink) {
    ASSERTS_NOT_NULL(pLink, false, ME, "NULL");
    return pLink->configured;
}

bool wld_mld_isLinkActiveInMultiLink(wld_mldLink_t* pLink) {
    ASSERTS_TRUE(wld_mld_isLinkActive(pLink), false, ME, "NULL");
    return (wld_mld_countNeighActiveLinks(pLink) > 1);
}

static wld_mldLink_t* s_linkFromIt(amxc_llist_it_t* it) {
    ASSERTS_NOT_NULL(it, NULL, ME, "NULL");
    return amxc_llist_it_get_data(it, wld_mldLink_t, it);
}

wld_mldLink_t* wld_mld_firstNeighLink(wld_mldLink_t* pLink) {
    ASSERTS_NOT_NULL(pLink, NULL, ME, "NULL");
    wld_mld_t* pMld = pLink->pMld;
    ASSERT_NOT_NULL(pMld, NULL, ME, "NULL");
    return s_linkFromIt(amxc_llist_get_first(&pMld->links));
}

wld_mldLink_t* wld_mld_nextNeighLink(wld_mldLink_t* pLink) {
    ASSERTS_NOT_NULL(pLink, NULL, ME, "NULL");
    amxc_llist_it_t* it = amxc_llist_it_get_next(&pLink->it);
    return s_linkFromIt(it);
}

