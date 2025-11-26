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

#ifndef SRC_INCLUDE_WLD_WLD_MLD_H_
#define SRC_INCLUDE_WLD_WLD_MLD_H_

#include "wld_types.h"
#include "swl/swl_mlo.h"

#define NO_LINK_ID (-1)

typedef struct {
    swl_trl_e emlmrEnable;
    swl_trl_e emlsrEnable;
    swl_trl_e strEnable;
    swl_trl_e nstrEnable;
} wld_apMldCfg_t;

/**
 * @brief Group of MLD instances per SSID type.
 */
typedef struct {
    wld_ssidType_e type;              /* SSID type this group belongs to (e.g. AP or STA) */
    amxc_llist_t mlds;                /* List of MLD instances registered under this group */
} wld_mldGroup_t;

/**
 * @brief Global MLD manager tracking all MLD groups.
 */
struct wld_mldMgr {
    bool init;                                /* Initialization flag */
    wld_mldGroup_t groups[WLD_SSID_TYPE_MAX]; /* Groups for each SSID type */
};

/**
 * @brief Structure representing a Multi-Link Device (MLD).
 */
typedef struct {
    amxc_llist_it_t it;
    amxd_object_t* object;
    uint8_t unit;                 /* Unique MLD unit identifier */
    amxc_llist_t links;           /* List of affiliated links belonging to this MLD */
    wld_mldGroup_t* pGroup;       /* Back pointer to parent group */
    wld_mldLink_t* pPrimLink;     /* Primary link for this MLD */
    wld_apMldCfg_t Cfg;
} wld_mld_t;

/**
 * @brief Structure representing an individual link within an MLD.
 */
struct wld_mldLink {
    amxc_llist_it_t it;
    amxd_object_t* AffObj;
    T_SSID* pSSID;                /* Pointer to SSID object associated with this link */
    int16_t linkId;               /* Link identifier */
    wld_mld_t* pMld;              /* Back pointer to parent MLD */
    bool configured;              /* Indicates whether link has been configured */
};

/*
 * event needed to make mld members react when one link / mld change
 */
typedef enum {
    WLD_MLD_EVT_ADD,           // when mld is added: first link added (registered to mldUnit)
    WLD_MLD_EVT_UPDATE,        // when mld is updated: one of the member links is added/removed/disabled/enabled
    WLD_MLD_EVT_DEL,           // when mld is removed: last link removed (unregistered from mldUnit)
} wld_mldChangeEvent_e;

/**
 * Event called when a mld unit or link has changed
 */
typedef struct {
    wld_mldChangeEvent_e event;
    wld_ssidType_e mldType;
    int32_t mldUnit;
    T_SSID* pEvtLinkSsid;
} wld_mldChange_t;

swl_rc_ne wld_mld_initMgr(wld_mldMgr_t** ppMgr);
swl_rc_ne wld_mld_deinitMgr(wld_mldMgr_t** ppMgr);

wld_mldLink_t* wld_mld_registerLink(T_SSID* pSSID, int32_t unit);
swl_rc_ne wld_mld_unregisterLink(T_SSID* pSSID);
swl_mlo_intfMldStatus_e wld_mld_checkMLDStatus(T_SSID* pSSID);
T_SSID* wld_mld_getLinkSsid(wld_mldLink_t* pLink);
T_SSID* wld_mld_getLinkSsidByLinkId(wld_mldLink_t* pLink, int8_t linkId);
const char* wld_mld_getLinkName(wld_mldLink_t* pLink);
swl_rc_ne wld_mld_setPrimaryLink(wld_mldLink_t* pLink);
wld_mldLink_t* wld_mld_getPrimaryLink(wld_mldLink_t* pLink);
T_SSID* wld_mld_getPrimaryLinkSsid(wld_mldLink_t* pLink);
const char* wld_mld_getPrimaryLinkIfName(wld_mldLink_t* pLink);
int32_t wld_mld_getPrimaryLinkIfIndex(wld_mldLink_t* pLink);
swl_rc_ne wld_mld_setLinkId(wld_mldLink_t* pLink, int32_t linkId);
swl_rc_ne wld_mld_resetLinkId(wld_mldLink_t* pLink);
int16_t wld_mld_getLinkId(const wld_mldLink_t* pLink);
const char* wld_mld_getLinkIfName(wld_mldLink_t* pLink);
bool wld_mld_isLinkActive(wld_mldLink_t* pLink);
bool wld_mld_isLinkEnabled(wld_mldLink_t* pLink);
bool wld_mld_isLinkUsable(wld_mldLink_t* pLink);
bool wld_mld_checkUsableLinkBasicConditions(wld_mldLink_t* pLink);
bool wld_mld_saveLinkConfigured(wld_mldLink_t* pLink, bool flag);
bool wld_mld_setLinkConfigured(wld_mldLink_t* pLink, bool flag);
bool wld_mld_isLinkConfigured(wld_mldLink_t* pLink);
bool wld_mld_isLinkActiveInMultiLink(wld_mldLink_t* pLink);
uint32_t wld_mld_countNeighLinks(wld_mldLink_t* pLink);
uint32_t wld_mld_countNeighActiveLinks(wld_mldLink_t* pLink);
uint32_t wld_mld_countNeighEnabledLinks(wld_mldLink_t* pLink);
uint32_t wld_mld_countNeighUsableLinks(wld_mldLink_t* pLink);
wld_mldLink_t* wld_mld_getNeighLinkByRad(wld_mldLink_t* pLink, T_Radio* pRad);
wld_mldLink_t* wld_mld_getNeighLinkByMacAddress(wld_mldLink_t* pLink, swl_macBin_t* macBin);

/**
 * @brief Get first usable neighboring MLD link sorted by frequency band
 *
 * Finds the first configured and usable neighboring MLD link with the
 * lowest frequency band among all available neighbor links.
 *
 * @param pLink reference MLD link to search neighbors from
 *
 * @return pointer to first usable neighbor link by frequency, NULL if none found
 */
wld_mldLink_t* wld_mld_firstUsableNeighLinkByFreq(wld_mldLink_t* pLink);

wld_mldLink_t* wld_mld_firstNeighLink(wld_mldLink_t* pLink);
wld_mldLink_t* wld_mld_nextNeighLink(wld_mldLink_t* pLink);

/**
 * @brief Retrieve MLD structure by type and unit number
 *
 * Searches through all radio vendors and MLD managers to find an MLD
 * structure matching the specified type and unit number.
 *
 * @param mldType the SSID type of the MLD to search for
 * @param mldUnit the unit number of the MLD to retrieve
 *
 * @return pointer to MLD structure if found, NULL otherwise
 */
wld_mld_t* wld_mld_getMldByUnit(wld_ssidType_e mldType, int32_t mldUnit);

#define wld_for_eachNeighMldLink(pNeighLink, pRefLink) \
    for(pNeighLink = wld_mld_firstNeighLink(pRefLink); pNeighLink; pNeighLink = wld_mld_nextNeighLink(pNeighLink))

#define wld_for_eachNeighMldLink_safe(pNgLink, pRefLink) \
    for(wld_mldLink_t* pNgLink = wld_mld_firstNeighLink(pRefLink), \
        * pNgLink ## _next = wld_mld_nextNeighLink(pNgLink); \
        pNgLink; \
        pNgLink = pNgLink ## _next, \
        pNgLink ## _next = wld_mld_nextNeighLink(pNgLink))

#endif /* SRC_INCLUDE_WLD_WLD_MLD_H_ */
