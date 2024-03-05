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
#include "wld/wld_radio.h"
#include "wld/wld_wpaSupp_cfgFile.h"
#include "wifiGen_hapd.h"

#define ME "genWsup"
#define WPASUPP_CONF_FILE_PATH_FORMAT "/tmp/%s_wpa_supplicant.conf"
#define WPASUPP_CMD "wpa_supplicant"
#define WPASUPP_ARGS_FORMAT_EXT "-ddK -b%s -i%s -Dnl80211 -c%s"
#define WPASUPP_ARGS_FORMAT "-ddK -i%s -Dnl80211 -c%s"
#define WPASUPP_CTRL_IFACE_DIR "/var/run/wpa_supplicant"

swl_rc_ne s_writeWpaSupArgsToBuf(char* args, size_t argsSize, char* confFilePath, size_t confFilePathSize, T_EndPoint* pEP) {
    ASSERT_NOT_NULL(pEP, SWL_RC_ERROR, ME, "NULL");
    bool ret;
    char cfgPath[128] = {0};
    ret = swl_str_catFormat(cfgPath, sizeof(cfgPath), WPASUPP_CONF_FILE_PATH_FORMAT, pEP->Name);
    ASSERTI_TRUE(ret, SWL_RC_ERROR, ME, "%s: writing wpaSupplicantConfigPath error", pEP->Name);

    if(confFilePath != NULL) {
        ret = swl_str_copy(confFilePath, confFilePathSize, cfgPath);
        ASSERTI_TRUE(ret, SWL_RC_ERROR, ME, "%s: writing wpaSupplicantConfigPath error", pEP->Name);
    }

    swl_str_copy(args, argsSize, "\0");
    if(!swl_str_isEmpty(pEP->bridgeName)) {
        ret = swl_str_catFormat(args, argsSize, WPASUPP_ARGS_FORMAT_EXT, pEP->bridgeName, pEP->Name, cfgPath);
    } else {
        ret = swl_str_catFormat(args, argsSize, WPASUPP_ARGS_FORMAT, pEP->Name, cfgPath);
    }
    ASSERTI_TRUE(ret, SWL_RC_ERROR, ME, "%s: writing wpaArgs error", pEP->Name);
    return SWL_RC_OK;
}
swl_rc_ne wifiGen_wpaSupp_init(T_EndPoint* pEP) {
    ASSERT_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    char startArgs[128] = {0};
    char confFilePath[128] = {0};
    swl_rc_ne rc = s_writeWpaSupArgsToBuf(startArgs, sizeof(startArgs), confFilePath, sizeof(confFilePath), pEP);
    ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "%s: Fail to set wpa_supplicant args", pEP->Name);
    rc = wld_secDmn_init(&pEP->wpaSupp, WPASUPP_CMD, startArgs, confFilePath, WPASUPP_CTRL_IFACE_DIR);
    ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "%s: Fail to init wpa_supplicant", pEP->Name);
    ASSERT_TRUE(wld_wpaCtrlInterface_init(&pEP->wpaCtrlInterface, pEP->Name, pEP->wpaSupp->ctrlIfaceDir),
                SWL_RC_ERROR, ME, "%s: Fail to init EP interface", pEP->Name);
    ASSERT_TRUE(wld_wpaCtrlMngr_registerInterface(pEP->wpaSupp->wpaCtrlMngr, pEP->wpaCtrlInterface),
                SWL_RC_ERROR, ME, "%s: fail to add wpa_ctrl interface to mngr", pEP->Name);
    return SWL_RC_OK;
}

void wifiGen_wpaSupp_cleanup(T_EndPoint* pEP) {
    ASSERT_NOT_NULL(pEP, , ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s: Destroy wpa_supplicant", pEP->Name);
    wld_secDmn_cleanup(&pEP->wpaSupp);
}

swl_rc_ne wifiGen_wpaSupp_startDaemon(T_EndPoint* pEP) {
    ASSERT_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_NOT_NULL(pEP->wpaSupp, SWL_RC_INVALID_STATE, ME, "%s: wpaSupp not initialized", pEP->Name);
    SAH_TRACEZ_WARNING(ME, "%s: Start wpa_supplicant", pEP->Name);
    char startArgs[128] = {0};
    swl_rc_ne rc = s_writeWpaSupArgsToBuf(startArgs, sizeof(startArgs), NULL, 0, pEP);
    ASSERT_FALSE(rc < SWL_RC_OK, rc, ME, "%s: Fail to set wpa_supplicant args", pEP->Name);
    wld_dmn_setArgList(pEP->wpaSupp->dmnProcess, startArgs);
    wld_wpaCtrlInterface_setEnable(pEP->wpaCtrlInterface, pEP->enable);
    return wld_secDmn_start(pEP->wpaSupp);
}

swl_rc_ne wifiGen_wpaSupp_stopDaemon(T_EndPoint* pEP) {
    ASSERTS_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    SAH_TRACEZ_WARNING(ME, "%s: Stop wpa_supplicant", pEP->Name);
    swl_rc_ne rc = wld_secDmn_stop(pEP->wpaSupp);
    ASSERTI_FALSE(rc < SWL_RC_OK, rc, ME, "%s: wpa_supplicant not running", pEP->Name);
    return SWL_RC_OK;
}

swl_rc_ne wifiGen_wpaSupp_reloadDaemon(T_EndPoint* pEP) {
    ASSERTS_NOT_NULL(pEP, SWL_RC_INVALID_PARAM, ME, "NULL");
    SAH_TRACEZ_INFO(ME, "%s : Reload wpa_supplicant", pEP->Name);
    return wld_secDmn_reload(pEP->wpaSupp);
}

void wifiGen_wpaSupp_writeConfig(T_EndPoint* pEP) {
    ASSERTS_NOT_NULL(pEP, , ME, "NULL");
    wld_wpaSupp_cfgFile_createExt(pEP);
}

bool wifiGen_wpaSupp_isRunning(T_EndPoint* pEP) {
    return ((pEP != NULL) && (wld_secDmn_isRunning(pEP->wpaSupp)));
}

bool wifiGen_wpaSupp_isAlive(T_EndPoint* pEP) {
    return ((pEP != NULL) && (wld_secDmn_isAlive(pEP->wpaSupp)));
}

