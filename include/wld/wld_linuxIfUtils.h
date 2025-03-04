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

#ifndef INCLUDE_WLD_WLD_LINUXIFUTILS_H_
#define INCLUDE_WLD_WLD_LINUXIFUTILS_H_

#include "swla/swla_mac.h"

/*
 * @brief return inet socket to kernel
 *
 * @return sock fd
 */
int wld_linuxIfUtils_getNetSock();

/*
 * @brief check whether interface is up
 *
 * @param sock socket to kernel
 * @param intfName interface name
 *
 * @param 0 if interface is down
 *        1 if interface is up
 *        <0 (errno) in case of error
 */
int wld_linuxIfUtils_getState(int sock, char* intfName);

/*
 * @brief check whether interface is up
 * (socket is created internally)
 *
 * @param intfName interface name
 *
 * @param 0 if interface is down
 *        1 if interface is up
 *        <0 (errno) in case of error
 */
int wld_linuxIfUtils_getStateExt(char* intfName);

/*
 * @brief sets interface state (up or down)
 *
 * @param sock socket to kernel
 * @param intfName interface name
 * @param state 0 for down, 1 for up
 *
 * @param 0 in case of success
 *        <0 (errno) in case of error
 */
int wld_linuxIfUtils_setState(int sock, char* intfName, int state);

/*
 * @brief sets interface state (up or down)
 * (socket is created internally)
 *
 * @param intfName interface name
 * @param state 0 for down, 1 for up
 *
 * @param 0 in case of success
 *        <0 (errno) in case of error
 */
int wld_linuxIfUtils_setStateExt(char* intfName, int state);

/*
 * @brief sets interface's hw mac address
 *
 * @param sock socket to kernel
 * @param intfName interface name
 * @param macInfo mac address in binary format
 *
 * @param 0 in case of success
 *        <0 (errno) in case of error
 */
int wld_linuxIfUtils_setMac(int sock, char* intfName, swl_macBin_t* macInfo);

/*
 * @brief sets interface's hw mac address
 * (socket is created internally)
 *
 * @param intfName interface name
 * @param macInfo mac address in binary format
 *
 * @param 0 in case of success
 *        <0 (errno) in case of error
 */
int wld_linuxIfUtils_setMacExt(char* intfName, swl_macBin_t* macInfo);

/*
 * @brief gets interface's hw mac address
 *
 * @param sock socket to kernel
 * @param intfName interface name
 * @param (output) macInfo mac address in binary format
 *
 * @param 0 in case of success
 *        <0 (errno) in case of error
 */
int wld_linuxIfUtils_getMac(int sock, char* intfName, swl_macBin_t* macInfo);

/*
 * @brief gets interface's hw mac address
 * (socket is created internally)
 *
 * @param intfName interface name
 * @param (output) macInfo mac address in binary format
 *
 * @param 0 in case of success
 *        <0 (errno) in case of error
 */
int wld_linuxIfUtils_getMacExt(char* intfName, swl_macBin_t* macInfo);

/*
 * @brief check whether interface link (lower layer) is up.
 * It reflects a physical requirement (vs. administrative)
 * to make the interface up and operational.
 *
 * @param sock socket to kernel
 * @param intfName interface name
 *
 * @param 0 if interface link is down
 *        1 if interface link is up
 *        <0 (errno) in case of error
 */
int wld_linuxIfUtils_getLinkState(int sock, char* intfName);

/*
 * @brief check whether interface link (lower layer) is up.
 * (socket is created internally)
 *
 * @param intfName interface name
 *
 * @param 0 if interface link is down
 *        1 if interface link is up
 *        <0 (errno) in case of error
 */
int wld_linuxIfUtils_getLinkStateExt(char* intfName);

/*
 * @brief gets interface's netdev index
 *
 * @param sock socket to kernel
 * @param intfName interface name
 * @param (output) pointer netdev index
 *
 * @param 0 in case of success
 *        <0 (errno) in case of error
 */
int wld_linuxIfUtils_getIfIndex(int sock, char* intfName, int* pIfIndex);

/*
 * @brief gets interface's netdev index
 * (socket is created internally)
 *
 * @param intfName interface name
 * @param (output) pointer netdev index
 *
 * @param 0 in case of success
 *        <0 (errno) in case of error
 */
int wld_linuxIfUtils_getIfIndexExt(char* intfName, int* pIfIndex);

/*
 * @brief update interface's hw mac address
 * and toggle the interface if needed
 *
 * @param sock socket to kernel
 * @param intfName interface name
 * @param macAddress mac address in binary format
 *
 * @param 0 in case of success
 *        <0 (errno) in case of error
 */
int wld_linuxIfUtils_updateMac(int sock, char* intfName, swl_macBin_t* macAddress);

/*
 * @brief update interface's hw mac address
 * and toggle the interface if needed
 * (socket is created internally)
 *
 * @param intfName interface name
 * @param macAddress mac address in binary format
 *
 * @param 0 in case of success
 *        <0 (errno) in case of error
 */
int wld_linuxIfUtils_updateMacExt(char* intfName, swl_macBin_t* macAddress);

/*
 * @brief get Vendor name
 *
 * @param radioName Radio name
 *
 * @return Vendor name in case of success
 *        <empty string in case of error
 */
const char* wld_linuxIfUtils_getChipsetVendor(char* radioName);

/*
 * @brief check whether a network interface is a vlan interface
 * by checking node in procfs /proc/net/vlan/<vlan_iface>
 *
 * @param[in] pIfaceName Name of the network interface.
 *
 * @return True on success and false otherwise.
 */
bool wld_linuxIfUtils_isVlanIface(const char* ifname);

/**
 * @brief Get lower interfaces of given network iface
 * by getting all lower nodes in sysfs /sys/class/net/[main_iface]/lower_<lower_iface>
 *
 * @param[in] pIfaceName Name of the network interface.
 * @param[in, out] pppLowerIfaces target array of strings, filled with lower iface names. (to be freed by the caller)
 * @param[in, out] pnLowerIfaces pointer to target array of strings length
 *
 * @return True on success and false otherwise.
 */
bool wld_linuxIfUtils_getLowerIfaces(const char* ifname, char*** pppLowerIfaces, size_t* pnLowerIfaces);

/**
 * @brief Get lower interface of given vlan network iface
 * by getting the vlan lower node in sysfs /sys/class/net/[vlan_iface]/lower_<lower_iface>
 *
 * @param[in] pIfaceName Name of the vlan network interface.
 * @param[in, out] lowerIfaceBuf target buffer with lower iface name.
 * @param[in] lowerIfaceBufSize size of target buffer
 *
 * @return True on success and false otherwise.
 */
bool wld_linuxIfUtils_getVlanLowerIface(const char* ifname, char* lowerIfaceBuf, size_t lowerIfaceBufSize);

/*
 * @brief check if the interface is currently in bridge
 *
 * @param intfName interface name
 *
 * @return true in case of the interface is in bridge
 *         false otherwise
 */
bool wld_linuxIfUtils_inBridge(char* intfName);

#endif /* INCLUDE_WLD_WLD_LINUXIFUTILS_H_ */
