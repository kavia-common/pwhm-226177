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

#include <stdlib.h>
#include <debug/sahtrace.h>

#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "wld.h"
#include "wld_util.h"
#include "wld_radio.h"
#include "Utils/wld_autoCommitMgr.h"
#include "swl/swl_assert.h"

#include "wld_radioOperatingStandards.h"

#define ME "radOStd"

// Checks whether given value is allowed for `OperatingStandards`.
// Callback called by the datamodel, see `.odl` file.
amxd_status_t _wld_rad_validateOperatingStandards_pvf(amxd_object_t* object _UNUSED,
                                                      amxd_param_t* param,
                                                      amxd_action_t reason _UNUSED,
                                                      const amxc_var_t* const args,
                                                      amxc_var_t* const retval _UNUSED,
                                                      void* priv _UNUSED) {
    ASSERTS_FALSE(amxc_var_is_null(args), amxd_status_invalid_value, ME, "invalid");
    ASSERTS_EQUALS(amxd_object_get_type(object), amxd_object_instance, amxd_status_ok, ME, "obj is not instance");
    amxd_status_t status = amxd_status_invalid_value;
    T_Radio* pRadio = wld_rad_fromObj(object);
    ASSERTI_NOT_NULL(pRadio, amxd_status_ok, ME, "No radio mapped");
    const char* currentValue = amxc_var_constcast(cstring_t, &param->value);
    ASSERT_NOT_NULL(currentValue, status, ME, "NULL");
    char* newValue = amxc_var_dyncast(cstring_t, args);
    ASSERT_NOT_NULL(newValue, status, ME, "NULL");
    swl_radioStandard_m radioStandards;
    if((swl_str_matches(currentValue, newValue)) ||
       (swl_radStd_fromCharAndValidate(&radioStandards, newValue,
                                       pRadio->operatingStandardsFormat, pRadio->supportedStandards,
                                       "validateRadioOperatingStandards"))) {
        status = amxd_status_ok;
    } else {
        SAH_TRACEZ_ERROR(ME, "%s: unsupported Operating Stds(%s)", pRadio->Name, newValue);
    }
    free(newValue);
    return status;
}

static void s_processOperatingStandards(T_Radio* pR, const char* newVal) {
    ASSERTS_NOT_NULL(pR, , ME, "NULL"); // silent fail if userdata not yet set
    ASSERT_TRUE(debugIsRadPointer(pR), , ME, "INVALID");
    ASSERT_STR(newVal, , ME, "Empty");
    swl_radioStandard_m newStandards;
    bool validOperatingStandards = swl_radStd_fromCharAndValidate(&newStandards, newVal,
                                                                  pR->operatingStandardsFormat, pR->supportedStandards, "setRadioOperatingStandards");
    ASSERT_TRUE(validOperatingStandards, , ME, "%s setRadioOperatingStandards : invalid operatingStandards", pR->Name);

    SAH_TRACEZ_INFO(ME, "%s setRadioOperatingStandards : set %s (%#x) was (%#x)",
                    pR->Name,
                    newVal, newStandards, pR->operatingStandards);


    if(pR->operatingStandards != newStandards) {
        pR->pFA->mfn_wrad_supstd(pR, newStandards);
        wld_autoCommitMgr_notifyRadEdit(pR);
        wld_rad_setAllMldLinksUnconfigured(pR);
    }
}

// Callback called when `OperatingStandardsFormat` or `OperatingStandards` change values.
void wld_rad_handleOperStdsAndFormatNewValues(void* priv _UNUSED, amxd_object_t* object, const amxc_var_t* const newParamValues) {
    SAH_TRACEZ_IN(ME);

    T_Radio* pR = wld_rad_fromObj(object);
    ASSERT_NOT_NULL(pR, , ME, "No radio mapped");

    const char* newOperStandards = GET_CHAR(newParamValues, "OperatingStandards");
    const char* newOperStandardsFormat = GET_CHAR(newParamValues, "OperatingStandardsFormat");

    if(newOperStandardsFormat == NULL) {
        ASSERTI_NOT_NULL(newOperStandards, , ME, "%s: no operStd changes detected", pR->Name);
    } else {
        const swl_radStd_format_e newVal = swl_radStd_charToFormat(newOperStandardsFormat);
        if(newVal != pR->operatingStandardsFormat) {
            pR->operatingStandardsFormat = newVal;
        }
    }

    char operStdsStr[128] = {0};
    if(newOperStandards == NULL) {
        swl_radStd_toChar(operStdsStr, sizeof(operStdsStr), pR->operatingStandards, pR->operatingStandardsFormat, pR->supportedStandards);
    } else {
        swl_str_copy(operStdsStr, sizeof(operStdsStr), newOperStandards);
    }
    ASSERT_STR(operStdsStr, , ME, "%s: empty operStd", pR->Name);

    SAH_TRACEZ_INFO(ME, "%s: OperStds: cfg(%s)/new(%s), operStdsFmt: cfg(%s)/new(%s)",
                    pR->Name, operStdsStr, newOperStandards,
                    swl_radStd_formatToChar(pR->operatingStandardsFormat), newOperStandardsFormat);

    s_processOperatingStandards(pR, operStdsStr);

    SAH_TRACEZ_OUT(ME);
}

// In case `operatingStandards` is "auto", set it to the default (which is the `supportedStandards`).
// This also updates the value in the datamodel, if change needed.
void wld_rad_update_operating_standard(T_Radio* pRad, amxd_trans_t* trans) {
    SAH_TRACEZ_IN(ME);
    ASSERT_NOT_NULL(pRad, , ME, "Radio NULL");
    ASSERT_FALSE(pRad->operatingStandards != M_SWL_RADSTD_AUTO && (pRad->operatingStandards & (~pRad->supportedStandards)), , ME,
                 "%s update_operating_standard : OperatingStandards %#x contains a standard not in SupportedStandards %#x",
                 pRad->Name, pRad->operatingStandards, pRad->supportedStandards);
    char* oldStandardsText = amxd_object_get_cstring_t(pRad->pBus, "OperatingStandards", NULL);
    ASSERT_NOT_NULL(oldStandardsText, , ME, "OperatingStandards not found in pRad object");
    free(oldStandardsText);
    swl_radioStandard_m oldStandards = pRad->operatingStandards;

    //If operatingStandards not yet set, ignore update.
    ASSERTI_NOT_EQUALS(pRad->operatingStandards, 0, , ME,
                       "%s opstd=0, ignoring update", pRad->Name);

    ASSERTS_EQUALS(pRad->operatingStandards, M_SWL_RADSTD_AUTO, , ME,
                   "%s opstd not 'auto', so no need to convert it", pRad->Name);

    pRad->operatingStandards = pRad->supportedStandards;

    amxc_string_t operatingStandardsText = swl_radStd_toStr(pRad->operatingStandards,
                                                            pRad->operatingStandardsFormat, pRad->supportedStandards);

    SAH_TRACEZ_INFO(ME, "%s update_operating_standard: update from %#x to %#x (%s)",
                    pRad->Name,
                    oldStandards,
                    pRad->operatingStandards, amxc_string_get(&operatingStandardsText, 0));
    if(trans != NULL) {
        amxd_trans_set_cstring_t(trans, "OperatingStandards", (char*) amxc_string_get(&operatingStandardsText, 0));
    } else {
        swl_typeCharPtr_commitObjectParam(pRad->pBus, "OperatingStandards", (char*) amxc_string_get(&operatingStandardsText, 0));
    }

    amxc_string_clean(&operatingStandardsText);

    SAH_TRACEZ_OUT(ME);
}

bool wld_rad_checkEnabledRadStd(T_Radio* pRad, swl_radStd_e radStd) {
    ASSERTS_NOT_NULL(pRad, false, ME, "NULL");
    return ((SWL_BIT_IS_SET(pRad->operatingStandards, radStd)) ||
            ((pRad->operatingStandards == M_SWL_RADSTD_AUTO) && (SWL_BIT_IS_SET(pRad->supportedStandards, radStd))));
}

