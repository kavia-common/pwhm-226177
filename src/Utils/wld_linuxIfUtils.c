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
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if_arp.h>
#include <linux/limits.h>
#include <dirent.h>

#include "swl/swl_common.h"
#include "swla/swla_mac.h"
#include "swl/swl_string.h"
#include "swl/fileOps/swl_fileUtils.h"

#include "wld.h"

#define ME "netUtil"

int wld_linuxIfUtils_getNetSock() {
    return socket(AF_INET, SOCK_DGRAM, 0);
}

#ifndef IFF_LOWER_UP
/* from linux/if.h */
#define IFF_LOWER_UP  (1 << 16)  /* driver signals L1 up */
#endif

static int s_getIfFlags(int sock, char* intfName, uint16_t* pIntfFlags) {
    ASSERTS_STR(intfName, SWL_RC_INVALID_PARAM, ME, "Empty");
    ASSERT_FALSE(sock < 0, SWL_RC_INVALID_PARAM, ME, "invalid socket");
    ASSERT_NOT_NULL(pIntfFlags, SWL_RC_INVALID_PARAM, ME, "NULL");
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    swl_str_copy(ifr.ifr_name, sizeof(ifr.ifr_name), intfName);
    int ret = ioctl(sock, SIOCGIFFLAGS, &ifr);
    ASSERT_FALSE((ret < 0), -errno, ME, "%s: SIOCGIFFLAGS failed (errno:%d:%s))", intfName, errno, strerror(errno));
    *pIntfFlags = ifr.ifr_flags;
    return ret;
}

int wld_linuxIfUtils_getState(int sock, char* intfName) {
    uint16_t intfFlags = 0;
    int ret = s_getIfFlags(sock, intfName, &intfFlags);
    ASSERTS_FALSE((ret < 0), ret, ME, "%s: fail to get intf flags");
    return ((intfFlags & IFF_UP) == IFF_UP);
}

int wld_linuxIfUtils_getStateExt(char* intfName) {
    int sock = wld_linuxIfUtils_getNetSock();
    ASSERT_FALSE(sock < 0, SWL_RC_ERROR, ME, "invalid socket");
    int ret = wld_linuxIfUtils_getState(sock, intfName);
    close(sock);
    return ret;
}

int wld_linuxIfUtils_getLinkState(int sock, char* intfName) {
    uint16_t intfFlags = 0;
    int ret = s_getIfFlags(sock, intfName, &intfFlags);
    ASSERTS_FALSE((ret < 0), ret, ME, "%s: fail to inft flags");
    /*
     * IFF_LOWER_UP      Driver signals L1 up (since Linux 2.6.17)
     */
    if((intfFlags & IFF_LOWER_UP) == IFF_LOWER_UP) {
        return true;
    }
    /*
     * IFF_RUNNING       Resources allocated.
     * This flag indicates that the interface is up and running.
     * It is reflecting the operational status on a network interface,
     * rather than its administrative one.
     */
    return ((intfFlags & IFF_RUNNING) == IFF_RUNNING);
}

int wld_linuxIfUtils_getLinkStateExt(char* intfName) {
    int sock = wld_linuxIfUtils_getNetSock();
    ASSERT_FALSE(sock < 0, SWL_RC_ERROR, ME, "invalid socket");
    int ret = wld_linuxIfUtils_getLinkState(sock, intfName);
    close(sock);
    return ret;
}

int wld_linuxIfUtils_setState(int sock, char* intfName, int state) {
    ASSERTS_STR(intfName, SWL_RC_INVALID_PARAM, ME, "Empty");
    ASSERT_FALSE(sock < 0, SWL_RC_INVALID_PARAM, ME, "invalid socket");
    SAH_TRACEZ_INFO(ME, "%s: set admin state %s", intfName, state ? "up" : "down");
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    swl_str_copy(ifr.ifr_name, sizeof(ifr.ifr_name), intfName);
    int ret = ioctl(sock, SIOCGIFFLAGS, &ifr);
    ASSERT_FALSE((ret < 0), -errno, ME, "%s: SIOCGIFFLAGS failed (errno:%d:%s))", intfName, errno, strerror(errno));
    ifr.ifr_flags &= ~IFF_UP;
    if(state) {
        ifr.ifr_flags |= IFF_UP;
    }
    ret = ioctl(sock, SIOCSIFFLAGS, &ifr);
    ASSERT_FALSE((ret < 0), -errno, ME, "%s: SIOCSIFFLAGS state (%d) failed (errno:%d:%s))", intfName, state, errno, strerror(errno));
    return SWL_RC_OK;
}

int wld_linuxIfUtils_setStateExt(char* intfName, int state) {
    int sock = wld_linuxIfUtils_getNetSock();
    ASSERT_FALSE(sock < 0, SWL_RC_ERROR, ME, "invalid socket");
    int ret = wld_linuxIfUtils_setState(sock, intfName, state);
    close(sock);
    return ret;
}

int wld_linuxIfUtils_setMac(int sock, char* intfName, swl_macBin_t* macInfo) {
    ASSERTS_STR(intfName, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(macInfo, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_FALSE(sock < 0, SWL_RC_INVALID_PARAM, ME, "invalid socket");
    struct ifreq ifr;
    memset(&ifr, 0, sizeof ifr);
    swl_str_copy(ifr.ifr_name, sizeof(ifr.ifr_name), intfName);
    ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
    memcpy(ifr.ifr_hwaddr.sa_data, macInfo->bMac, SWL_MAC_BIN_LEN);
    int ret = ioctl(sock, SIOCSIFHWADDR, &ifr);
    ASSERT_FALSE((ret < 0), -errno, ME, "%s: SIOCSIFHWADDR mac ["SWL_MAC_FMT "] (errno:%d:%s))",
                 intfName, SWL_MAC_ARG(macInfo->bMac), errno, strerror(errno));
    return SWL_RC_OK;
}

int wld_linuxIfUtils_setMacExt(char* intfName, swl_macBin_t* macInfo) {
    int sock = wld_linuxIfUtils_getNetSock();
    ASSERT_FALSE(sock < 0, SWL_RC_ERROR, ME, "invalid socket");
    int ret = wld_linuxIfUtils_setMac(sock, intfName, macInfo);
    close(sock);
    return ret;
}

int wld_linuxIfUtils_getMac(int sock, char* intfName, swl_macBin_t* macInfo) {
    ASSERTS_STR(intfName, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(macInfo, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_FALSE(sock < 0, SWL_RC_INVALID_PARAM, ME, "invalid socket");
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    swl_str_copy(ifr.ifr_name, sizeof(ifr.ifr_name), intfName);
    int ret = ioctl(sock, SIOCGIFHWADDR, &ifr);
    ASSERT_FALSE((ret < 0), -errno, ME, "%s: SIOCGIFHWADDR (errno:%d:%s))",
                 intfName, errno, strerror(errno));
    memcpy(macInfo->bMac, ifr.ifr_hwaddr.sa_data, SWL_MAC_BIN_LEN);
    return SWL_RC_OK;
}

int wld_linuxIfUtils_getMacExt(char* intfName, swl_macBin_t* macInfo) {
    int sock = wld_linuxIfUtils_getNetSock();
    ASSERT_FALSE(sock < 0, SWL_RC_ERROR, ME, "invalid socket");
    int ret = wld_linuxIfUtils_getMac(sock, intfName, macInfo);
    close(sock);
    return ret;
}

int wld_linuxIfUtils_getIfIndex(int sock, char* intfName, int* pIfIndex) {
    ASSERTS_STR(intfName, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERTS_NOT_NULL(pIfIndex, SWL_RC_INVALID_PARAM, ME, "NULL");
    ASSERT_FALSE(sock < 0, SWL_RC_INVALID_PARAM, ME, "invalid socket");
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    swl_str_copy(ifr.ifr_name, sizeof(ifr.ifr_name), intfName);
    int ret = ioctl(sock, SIOCGIFINDEX, &ifr);
    ASSERTI_FALSE((ret < 0), -errno, ME, "%s: SIOCGIFINDEX (errno:%d:%s))",
                  intfName, errno, strerror(errno));
    *pIfIndex = ifr.ifr_ifindex;
    return SWL_RC_OK;
}

int wld_linuxIfUtils_getIfIndexExt(char* intfName, int* pIfIndex) {
    int sock = wld_linuxIfUtils_getNetSock();
    ASSERT_FALSE(sock < 0, SWL_RC_ERROR, ME, "invalid socket");
    int ret = wld_linuxIfUtils_getIfIndex(sock, intfName, pIfIndex);
    close(sock);
    return ret;
}

int wld_linuxIfUtils_updateMac(int sock, char* intfName, swl_macBin_t* macAddress) {
    ASSERT_FALSE(sock < 0, SWL_RC_INVALID_PARAM, ME, "invalid socket");
    ASSERTS_STR(intfName, SWL_RC_INVALID_PARAM, ME, "Empty ifname");
    ASSERTS_NOT_NULL(macAddress, SWL_RC_INVALID_PARAM, ME, "NULL");

    swl_macBin_t curMac;
    int ret = wld_linuxIfUtils_getMac(sock, intfName, &curMac);
    ASSERT_FALSE(ret < SWL_RC_OK, ret, ME, "%s: fail to get current mac", intfName);
    if(memcmp(&curMac, macAddress, sizeof(curMac)) == 0) {
        return SWL_RC_OK;
    }
    bool restoreUp = false;
    ret = wld_linuxIfUtils_setMac(sock, intfName, macAddress);
    if(ret < 0) {
        ASSERT_EQUALS(ret, -EIO, SWL_RC_ERROR, ME, "%s:fail to set intf mac ["SWL_MAC_FMT "] (ret:%d)",
                      intfName, SWL_MAC_ARG(macAddress->bMac), ret);
        int state = wld_linuxIfUtils_getState(sock, intfName);
        ASSERT_TRUE(state >= 0, SWL_RC_ERROR, ME, "%s: can not get interface state (ret:%d)", intfName, state);
        ASSERTI_NOT_EQUALS(state, 0, SWL_RC_ERROR, ME, "%s: already down", intfName);
        SAH_TRACEZ_WARNING(ME, "%s: not down: toggle to apply mac", intfName);
        restoreUp = true;
        wld_linuxIfUtils_setState(sock, intfName, 0);
        ret = wld_linuxIfUtils_setMac(sock, intfName, macAddress);
        if(ret < 0) {
            SAH_TRACEZ_ERROR(ME, "%s:still fail to set intf mac ["SWL_MAC_FMT "] after switching off (%s) (ret:%d)",
                             intfName, SWL_MAC_ARG(macAddress->bMac), intfName, ret);
            wld_linuxIfUtils_setState(sock, intfName, 1);
        }
    }
    if(restoreUp) {
        wld_linuxIfUtils_setState(sock, intfName, 1);
    }
    return ret;
}

int wld_linuxIfUtils_updateMacExt(char* intfName, swl_macBin_t* macAddress) {
    int sock = wld_linuxIfUtils_getNetSock();
    ASSERT_FALSE(sock < 0, SWL_RC_ERROR, ME, "invalid socket");
    int ret = wld_linuxIfUtils_updateMac(sock, intfName, macAddress);
    close(sock);
    return ret;
}

static const wld_rad_chipVendorInfo_t driver2Vendor[] = {
    {"ath", "Qualcomm"},
    {"mtlk", "MaxLinear"},
    {"pcieh", "Broadcom"},
    {"iwlwifi", "Intel"},
    {"mt76", "MediaTek"},
    {"rt2x00", "Ralink"},
    {NULL, NULL}
};

const char* wld_linuxIfUtils_getChipsetVendor(char* radioName) {
    ASSERT_NOT_NULL(radioName, NULL, ME, "Radio Name is NULL");
    const char* pathPrefix = "/sys/class/net/";
    const char* pathSuffix = "/device/driver/";
    char path[512] = {0};

    snprintf(path, sizeof(path), "%s%s%s", pathPrefix, radioName, pathSuffix);

    const char* vendorName = "";
    char* driver = NULL;
    char* rp = realpath(path, NULL);
    ASSERT_NOT_NULL(rp, vendorName, ME, "fail to get realpath of %s", path);
    driver = strrchr(rp, '/');
    if(driver != NULL) {
        driver++;
    } else {
        driver = rp;
    }
    for(int i = 0; driver2Vendor[i].driver != NULL; i++) {
        if(swl_str_startsWith(driver, driver2Vendor[i].driver)) {
            SAH_TRACEZ_INFO(ME, "Driver(%s) to Vendor(%s) mapping found", driver, driver2Vendor[i].vendor);
            vendorName = driver2Vendor[i].vendor;
            break;
        }
    }
    free(rp);
    return vendorName;
}

bool wld_linuxIfUtils_isVlanIface(const char* ifname) {
    ASSERTS_STR(ifname, false, ME, "empty");
    char fPath[PATH_MAX] = {0};
    swl_str_catFormat(fPath, sizeof(fPath), "/proc/net/vlan/%s", ifname);
    return (access(fPath, F_OK) == 0);
}

static const char* s_getLowerIface(const char* ifname) {
    if(swl_str_startsWith(ifname, "lower_")) {
        return strchr(ifname, '_') + 1;
    }
    return "";
}

static int s_filterLowers(const struct dirent* pEntry) {
    if(pEntry->d_type != DT_LNK) {
        return 0;
    }
    return !swl_str_isEmpty(s_getLowerIface(pEntry->d_name));
}

bool wld_linuxIfUtils_getLowerIfaces(const char* ifname, char*** pppLowerIfaces, size_t* pnLowerIfaces) {
    ASSERTS_STR(ifname, false, ME, "empty");
    char fPath[PATH_MAX] = {0};
    swl_str_catFormat(fPath, sizeof(fPath), "/sys/class/net/%s/", ifname);
    struct dirent** namelist = NULL;
    int n = scandir(fPath, &namelist, s_filterLowers, alphasort);
    ASSERTI_FALSE(n < 0, false, ME, "fail to scan dir %s", fPath);
    char** ppLowerIfaces = NULL;
    if(pppLowerIfaces) {
        if(n > 0) {
            //last element shall be null
            ppLowerIfaces = calloc(n + 1, sizeof(char*));
            ASSERTI_NOT_NULL(ppLowerIfaces, false, ME, "fail to alloc %d results of %s", n, fPath);
        }
        W_SWL_SETPTR(pppLowerIfaces, ppLowerIfaces);
    }
    W_SWL_SETPTR(pnLowerIfaces, (size_t) n);
    for(int i = 0; i < n; i++) {
        if(ppLowerIfaces) {
            swl_str_copyMalloc(&ppLowerIfaces[i], s_getLowerIface(namelist[i]->d_name));
        }
        free(namelist[i]);
    }
    free(namelist);
    return true;
}

bool wld_linuxIfUtils_getVlanLowerIface(const char* ifname, char* lowerIfaceBuf, size_t lowerIfaceBufSize) {
    ASSERTS_TRUE(wld_linuxIfUtils_isVlanIface(ifname), false, ME, "invalid vlan name");
    char** lowers = NULL;
    size_t nLowers = 0;
    bool ret = wld_linuxIfUtils_getLowerIfaces(ifname, &lowers, &nLowers);
    if(ret && (nLowers > 0)) {
        if(nLowers > 1) {
            SAH_TRACEZ_WARNING(ME, "vlan iface %s has %zu lowers", ifname, nLowers);
        }
        //shall be only one lower for vlan interface
        swl_str_copy(lowerIfaceBuf, lowerIfaceBufSize, lowers[0]);
    }
    if(lowers) {
        for(size_t i = 0; i < nLowers; i++) {
            W_SWL_FREE(lowers[i]);
        }
        free(lowers);
    }
    return ret;
}

bool wld_linuxIfUtils_inBridge(char* intfName) {
    ASSERTS_STR(intfName, false, ME, "Empty ifname");
    char path[128] = {0};
    snprintf(path, sizeof(path), "/sys/class/net/%s/brport", intfName);
    return (swl_fileUtils_existsDir(path));
}
