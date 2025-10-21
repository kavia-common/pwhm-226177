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
#!/usr/bin/amxrt

#include "global_amxb_timeouts.odl";

%config {
    name = "wld";
    wld = {
        mod-dir = "${prefix}${plugin-dir}/${name}/modules",
        external-mod-dir = "${prefix}${plugin-dir}/modules"
    };

    // odl parser config options
    import-dbg = false;
    dm-eventing-enabled = true;
    dm-events-before-start = true;

    populate-behavior = {
        duplicate-instance = "update"
    };

    storage-path = "${rw_data_path}/${name}";
    storage-type = "odl";
    %global odl = {
        dm-load = false,
        load-dm-events = true,
        dm-save = false,
m4_ifelse(DISABLE_PERSIST,y,``
        dm-save-on-changed = false,'',``
        dm-save-on-changed = true,''
)
        dm-save-delay = 1000,
        dm-defaults = "${name}_defaults/",
        directory = "${storage-path}/odl"
    };

    //main files
    definition_file = "${name}_definitions.odl";

    privileges = {
        keep-all = true
    };

    sahtrace = {
        type = "syslog",
        level = 500
    };
    trace-zones = {"ad" = 200,
                   "all" = 200,
                   "ap11k" = 200,
                   "ap11v" = 200,
                   "ap" = 200,
                   "apMf" = 200,
                   "apNeigh" = 200,
                   "apRssi" = 200,
                   "apSec" = 200,
                   "chan" = 200,
                   "chanDfs" = 200,
                   "chanInf" = 200,
                   "chanMgt" = 200,
                   "csi" = 200,
                   "dm_trans" = 200,
                   "ep" = 200,
                   "fileMgr" = 200,
                   "gen" = 200,
                   "genEp" = 200,
                   "genEvt" = 200,
                   "genFsm" = 200,
                   "genHapd" = 200,
                   "genMld" = 200,
                   "genRadI" = 200,
                   "genStaC" = 200,
                   "genVap" = 200,
                   "genWsup" = 200,
                   "genZwd" = 200,
                   "hapdAP" = 200,
                   "hapdCfg" = 200,
                   "hapdRad" = 200,
                   "linuxIfStats" = 200,
                   "mapCFmt" = 200,
                   "mapCGen" = 200,
                   "mld" = 200,
                   "netUtil" = 200,
                   "nlAP" = 200,
                   "nlApi" = 200,
                   "nlAttr" = 200,
                   "nlCore" = 200,
                   "nlDbg" = 200,
                   "nlEvt" = 200,
                   "nlParser" = 200,
                   "nlRad" = 200,
                   "nlScan" = 200,
                   "nlSsid" = 200,
                   "nlUtils" = 200,
                   "rad" = 200,
                   "radCaps" = 200,
                   "radEvtH" = 200,
                   "radIfM" = 200,
                   "radMacC" = 200,
                   "radOStd" = 200,
                   "radPrb" = 200,
                   "radStm" = 200,
                   "ROpStd" = 200,
                   "secDmn" = 200,
                   "secDmnG" = 200,
                   "ssid" = 200,
                   "swl802x" = 200,
                   "swlBit" = 200,
                   "swlChan" = 200,
                   "swlConv" = 200,
                   "swlCryp" = 200,
                   "swlCTbl" = 200,
                   "swlDmEv" = 200,
                   "swlExec" = 200,
                   "swlFile" = 200,
                   "swlHash" = 200,
                   "swlHex" = 200,
                   "swlHstG" = 200,
                   "swlLib" = 200,
                   "swlMac" = 200,
                   "swlMap" = 200,
                   "swlMapK" = 200,
                   "swlMath" = 200,
                   "swlMcs" = 200,
                   "swlNc" = 200,
                   "swl_oo" = 200,
                   "swl_oui" = 200,
                   "swlPrnt" = 200,
                   "swlRc" = 200,
                   "swlRStd" = 200,
                   "swlSec" = 200,
                   "swlStr" = 200,
                   "swlTbl" = 200,
                   "swlTDef" = 200,
                   "swlTEnm" = 200,
                   "swlTime" = 200,
                   "swlTimeSpec" = 200,
                   "swlTpl" = 200,
                   "swlTrl" = 200,
                   "swlType" = 200,
                   "swlULLi" = 200,
                   "swlUspC" = 200,
                   "swlUuid" = 200,
                   "swlWps" = 200,
                   "tyRoam" = 200,
                   "util" = 200,
                   "utilEvt" = 200,
                   "utilMon" = 200,
                   "wld" = 200,
                   "wld_acm" = 200,
                   "wld_autoNeigh" = 200,
                   "wldChan" = 200,
                   "wldDly" = 200,
                   "wldDmn" = 200,
                   "wldDMgt" = 200,
                   "wldEPrf" = 200,
                   "wldFsm" = 200,
                   "wldScan" = 200,
                   "wldTrap" = 200,
                   "wldZwd" = 200,
                   "wpaCtrl" = 200,
                   "wpaSupp" = 200,
                   "wps" = 200,
                   "wSupCfg" = 200
                   };
                   
m4_ifelse(DISABLE_NETMODEL,y,,``
    NetModel =  "nm_radio,nm_ssid";
    nm_ssid = {
        InstancePath = "WiFi.SSID.",
        Tags = "ssid netdev",
        Prefix = "ssid-"
    };
    nm_radio = {
        InstancePath = "WiFi.Radio.",
        Tags = "radio",
        Prefix = "radio-"
    };
'')

    import-dirs = [
        ".",
        "../src/",
        "../src/Plugin/",
        "${prefix}${plugin-dir}/${name}",
        "${prefix}${plugin-dir}/modules"
    ];

    m4_ifelse(ENABLE_PCB_SERVER_SOCKET,y,
        listen += ["pcb:/var/run/pwhm_pcb.sock"];
    )
    m4_ifelse(ENABLE_USP_SERVER_SOCKET,y,
        listen += ["usp:/var/run/pwhm_usp.sock"];
    )

    m4_ifelse(ENABLE_USP_SERVER_SOCKET,y,
    backend-dir = [
        "/usr/bin/mods/amxb/"`,'
        "/usr/bin/mods/usp/"
    ];)
m4_ifelse(DISABLE_PERSIST,y,,``
    //persistent storage
    pcm_svc_config = {
        "Objects" = "WiFi",
        "BackupFiles" = "wld"
    };'')
}

import "${name}.so" as "${name}";
m4_ifelse(DISABLE_NETMODEL,y,,
import "mod-netmodel.so" as "mnm";
)
import "mod-dmext.so";

#include "mod_sahtrace.odl";

include "${definition_file}";


%define {
    entry-point wld.wld_main;
m4_ifelse(DISABLE_NETMODEL,y,,``
    entry-point mnm.mod_netmodel_main;'')
}


m4_ifelse(DISABLE_PERSIST,y,,`` 
#include "mod_pcm_svc.odl";'')

