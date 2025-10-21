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

#ifndef __WLD_NL80211_CORE_PRIV_H__
#define __WLD_NL80211_CORE_PRIV_H__

#include <errno.h>
#include <net/if.h>
#include <sys/socket.h>
#include "wld_nl80211_compat.h"
#include "wld_nl80211_core.h"
#include "wld_nl80211_utils.h"

#include <amxc/amxc.h>
#include <amxp/amxp.h>
#include <amxd/amxd_parameter.h>
#include <amxd/amxd_action.h>
#include <amxd/amxd_object_parameter.h>
#include <amxd/amxd_object_event.h>
#include <amxd/amxd_function.h>

typedef int (* wld_nl80211_nlSend_f)(struct nl_sock* sk, struct nl_msg* msg);
typedef ssize_t (* wld_nl80211_recv_f)(int socket, void* buffer, size_t length, int flags);

/*
 * @brief common nl80211 driver IDs
 *        Ids must be >=0 (-1 means not available)
 */
extern wld_nl80211_driverIds_t g_nl80211DriverIDs;

/*
 * @brief nl80211 socket manager context
 */
struct wld_nl80211_state {
    amxc_llist_it_t it;                   //iterator for list
    struct nl_sock* nl_sock;              //netlink socket
    int nl_event;                         //socket fd
    amxc_llist_t requests;                //list of current requests, sent over nl sock and waiting for reply
    amxc_llist_t listeners;               //list of registered listener for nl80211 events
    wld_nl80211_stateCounters_t counters; //nl msg statistics

    /* only for testing purpose */
    wld_nl80211_nlSend_f fNlSendPriv;     //private implem of nl_send api (used with nl mocker)
    wld_nl80211_recv_f fRecvPriv;         //private implem of recv api (used with nl mocker)
};

/*
 * @brief return the name of the nl80211 cmd/event id
 *
 * @param cmd id of nl80211 event (NL80211_CMD_xxx)
 *
 * @return string name of known nl80211 msg, "unknown" otherwise
 */
const char* wld_nl80211_msgName(uint32_t cmd);

#endif /* __WLD_NL80211_CORE_PRIV_H__ */
