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

#include <string.h>
#include <swla/swla_mac.h>

#include "wld.h"
#include "wld_radio.h"
#include "wld_endpoint.h"
#include "wld_epProfile.h"
#include "wld_tinyRoam.h"
#include "wld_ssid.h"
#include "wld_wps.h"
#include "wld_util.h"
#include "swl/swl_assert.h"
#include "wld_eventing.h"
#include "wld_assocdev.h"
#include "wld_wpaSupp_cfgFile.h"
#include "wld_wpaSupp_cfgManager.h"
#include "wld_accesspoint.h"

#define ME "wldEPrf"

/**
 * @brief setEndPointProfileDefaults
 *
 * Set some default settings for the Endpoint Profile
 * Used when the profile is just created
 *
 * @param profile Endpoint profile
 */
static void s_setDefaults(T_EndPointProfile* profile) {
    SAH_TRACEZ_INFO(ME, "Setting EndpointProfile Defaults");

    profile->enable = 0;
    profile->priority = 0;
    profile->status = EPPS_DISABLED;
    memset(profile->SSID, 0, sizeof(profile->SSID));
    memset(profile->BSSID, 0, sizeof(profile->BSSID));
    memset(profile->keyPassPhrase, 0, sizeof(profile->keyPassPhrase));
    memset(profile->saePassphrase, 0, sizeof(profile->saePassphrase));
    memset(profile->alias, 0, sizeof(profile->alias));
    memset(profile->location, 0, sizeof(profile->location));
    memset(profile->WEPKey, 0, sizeof(profile->WEPKey));
    memset(profile->preSharedKey, 0, sizeof(profile->preSharedKey));
    profile->secModeEnabled = SWL_SECURITY_APMODE_NONE;
}

/**
 * @brief wld_endpoint_addProfileInstance_ocf
 *
 * Endpoint Profile instance Add Handler
 * Create a new EndpointProfile Instance
 * and set its defaults
 *
 * @param template_object EndpointProfile template object
 * @param instance_object EndpointProfile instance object
 * @return true on success, false otherwise
 */
static void s_addEpProfileInst_oaf(void* priv _UNUSED, amxd_object_t* object, const amxc_var_t* const intialParamValues _UNUSED) {
    SAH_TRACEZ_IN(ME);

    T_EndPoint* pEP = wld_ep_fromObj(amxd_object_get_parent(amxd_object_get_parent(object)));
    ASSERT_NOT_NULL(pEP, , ME, "Failed to find Endpoint structure");

    /* Set the T_EndPointProfile struct to the new instance */
    T_EndPointProfile* profile = (T_EndPointProfile*) calloc(1, sizeof(T_EndPointProfile));
    ASSERT_NOT_NULL(profile, , ME, "Failed to allocate T_EndpointProfile structure");

    object->priv = profile;
    profile->pBus = object;

    /* Interlinking */
    amxc_llist_append(&pEP->llProfiles, &profile->it);
    profile->endpoint = pEP;

    /* Set some defaults to the endpoint profile struct */
    s_setDefaults(profile);
    /* Set the current profile when a matching profile reference is set */
    wld_endpoint_setCurrentProfile(pEP->pBus, profile);

    SAH_TRACEZ_OUT(ME);
}

/**
 * @brief wld_endpoint_deleteProfileInstance_odf
 *
 * Delete handler on the Profile template object
 * Remove the linked profile data
 *
 * @param template_object
 * @param instance_object
 * @return true on success, false otherwise
 */
amxd_status_t _wld_endpoint_deleteProfileInstance_odf(amxd_object_t* object,
                                                      amxd_param_t* param,
                                                      amxd_action_t reason,
                                                      const amxc_var_t* const args,
                                                      amxc_var_t* const retval,
                                                      void* priv) {
    SAH_TRACEZ_IN(ME);

    amxd_status_t status = amxd_action_object_destroy(object, param, reason, args, retval, priv);
    ASSERT_EQUALS(status, amxd_status_ok, status, ME, "Fail to destroy ep profile st:%d", status);
    ASSERTS_EQUALS(amxd_object_get_type(object), amxd_object_instance, status, ME, "obj is not instance");

    T_EndPointProfile* pProf = (T_EndPointProfile*) object->priv;
    object->priv = NULL;
    ASSERTS_NOT_NULL(pProf, amxd_status_ok, ME, "No internal ctx");
    T_EndPoint* pEP = wld_ep_fromObj(amxd_object_get_parent(amxd_object_get_parent(object)));

    pProf->pBus = NULL;

    wld_epProfile_delete(pEP, pProf);

    SAH_TRACEZ_OUT(ME);
    return amxd_status_ok;
}

swl_rc_ne wld_epProfile_delete(T_EndPoint* pEP, T_EndPointProfile* pProfile) {
    ASSERT_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pProfile, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_TRUE(((void*) pEP) != ((void*) pProfile), SWL_RC_INVALID_PARAM, ME, "EQUALS");

    if(pEP->currentProfile == pProfile) {
        pEP->currentProfile = NULL;
        SAH_TRACEZ_WARNING(ME, "Current Active EndpointProfile gets deleted !!!");
        pEP->internalChange = true;
        swl_typeCharPtr_commitObjectParam(pEP->pBus, "ProfileReference", "");
        pEP->internalChange = false;
        wld_endpoint_reconfigure(pEP);
    }

    if(pProfile->pBus != NULL) {
        pProfile->pBus->priv = NULL;
        swl_object_delInstWithTransOnLocalDm(pProfile->pBus);
    }

    amxc_llist_it_take(&pProfile->it);
    free(pProfile);

    return SWL_RC_OK;
}


T_EndPointProfile* wld_epProfile_fromIt(amxc_llist_it_t* it) {
    ASSERTS_NOT_NULL(it, NULL, ME, "NULL");

    return amxc_llist_it_get_data(it, T_EndPointProfile, it);
}

SWLA_DM_HDLRS(sEpProfileDmHdlrs,
              ARR(),
              .instAddedCb = s_addEpProfileInst_oaf,
              .objChangedCb = wld_endpoint_setProfile_ocf,
              );

void _wld_ep_setProfileConf_ocf(const char* const sig_name,
                                const amxc_var_t* const data,
                                void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sEpProfileDmHdlrs, sig_name, data, priv);
}

SWLA_DM_HDLRS(sEpProfileSecDmHdlrs,
              ARR(),
              .objChangedCb = wld_endpoint_setProfileSecurity_ocf,
              );

void _wld_ep_setProfileSecurityConf_ocf(const char* const sig_name,
                                        const amxc_var_t* const data,
                                        void* const priv) {
    swla_dm_procObjEvtOfLocalDm(&sEpProfileSecDmHdlrs, sig_name, data, priv);
}

static bool s_checkEpProfileSecConf(T_EndPointProfile* pEpProf) {
    ASSERTS_NOT_NULL(pEpProf, false, ME, "NULL");
    T_EndPoint* pEP = pEpProf->endpoint;
    T_Radio* pRad = pEP ? pEP->pRadio : NULL;
    swl_security_apMode_m modesSupported = (pEP ? pEP->secModesSupported : 0);

    amxc_var_t params;
    amxc_var_init(&params);
    amxc_var_set_type(&params, AMXC_VAR_ID_HTABLE);
    char TBuf[256];

    swl_security_apModeMaskToString(TBuf, sizeof(TBuf), SWL_SECURITY_APMODEFMT_LEGACY, modesSupported);
    amxc_var_add_key(cstring_t, &params, "ModesSupported", TBuf);
    amxc_var_add_key(cstring_t, &params, "ModeEnabled", swl_security_apModeToString(pEpProf->secModeEnabled, SWL_SECURITY_APMODEFMT_LEGACY));
    amxc_var_add_key(cstring_t, &params, "WEPKey", pEpProf->WEPKey);
    amxc_var_add_key(cstring_t, &params, "PreSharedKey", pEpProf->preSharedKey);
    amxc_var_add_key(cstring_t, &params, "KeyPassPhrase", pEpProf->keyPassPhrase);
    amxc_var_add_key(cstring_t, &params, "SAEPassphrase", pEpProf->saePassphrase);
    amxc_var_add_key(cstring_t, &params, "MFPConfig", swl_security_mfpModeToString(pEpProf->mfpConfig));

    bool valid = wld_ap_sec_checkSecConfigParams(pEpProf->alias, &params, modesSupported, pRad);
    amxc_var_clean(&params);

    return valid;
}

bool wld_epProfile_hasValidConf(T_EndPointProfile* pEpProf) {
    ASSERTS_NOT_NULL(pEpProf, false, ME, "NULL");
    ASSERTI_FALSE(swl_str_isEmpty(pEpProf->SSID), false, ME, "%s: profile has no SSID", pEpProf->alias);
    ASSERTI_TRUE(s_checkEpProfileSecConf(pEpProf), false, ME, "%s: profile security is invalid", pEpProf->alias);
    return true;
}

