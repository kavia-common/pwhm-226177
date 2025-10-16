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
/*
 * This file includes nl80211 attribute definition, and a set of utilities to build basic and nested attributes
 */

#ifndef INCLUDE_WLD_WLD_NL80211_ATTR_H_
#define INCLUDE_WLD_WLD_NL80211_ATTR_H_

#include <linux/socket.h>
#include <linux/netlink.h>
#include "wld_nl80211.h"

#include "swl/swl_common.h"
#include "swl/swl_unLiList.h"

/*
 * @brief description of nl80211 attribute, filled when a nl80211 command or vendor subcmd is sent
 */
typedef swl_unLiList_t wld_nl80211_nlAttrList_t;
typedef struct {
    uint32_t type; //attribute type
    bool nested;   //flag for nested attribute
    //attribute data
    union {
        //raw data
        struct {
            uint32_t len;
            const void* ptr;
        } raw;
        //nested attributes
        wld_nl80211_nlAttrList_t attribs;
    } data;
} wld_nl80211_nlAttr_t;

/*
 * @brief initialize nl attribute
 *
 * @param pAttr pointer to nl attribute
 *
 * @return void
 */
void wld_nl80211_initNlAttr(wld_nl80211_nlAttr_t* pAttr);

/*
 * @brief clean nl attribute
 * (recursive function through nested attributes)
 *
 * @param pAttr pointer to nl attribute
 *
 * @return void
 */
void wld_nl80211_cleanNlAttr(wld_nl80211_nlAttr_t* pAttr);

/*
 * @brief initialize nl attributes unlinked list
 *
 * @param pAttr pointer to nl attributes unlinked list
 *
 * @return void
 */
void wld_nl80211_initNlAttrList(wld_nl80211_nlAttrList_t* pAttrList);

/*
 * @brief clean nl attributes unlinked list
 *
 * @param pAttr pointer to nl attribute unlinked list
 *
 * @return void
 */
void wld_nl80211_cleanNlAttrList(wld_nl80211_nlAttrList_t* pAttrList);

/*
 * @brief macro to initialize nl attributes list with array of attribute values
 */
#define NL_ATTRS_SET(pAttrList, attrValues) \
    wld_nl80211_initNlAttrList(pAttrList); \
    do { \
        wld_nl80211_nlAttr_t attrArray[] = attrValues; \
        for(uint32_t i = 0; i < SWL_ARRAY_SIZE(attrArray); i++) { \
            swl_unLiList_add(pAttrList, &(attrArray[i])); \
        }; \
    } while(0);

/*
 * @brief macro to add nl attribute to attributes list
 */
#define NL_ATTRS_ADD(pAttrList, attrVal) \
    do { \
        wld_nl80211_nlAttr_t attr = attrVal; \
        swl_unLiList_add(pAttrList, &attr); \
    } while(0);

/*
 * @brief macro to define nl empty attributes list
 */
#define NL_ATTRS_EMPTY(attrList) \
    wld_nl80211_nlAttrList_t attrList; \
    wld_nl80211_initNlAttrList(&attrList);

/*
 * @brief macro to define nl attributes list and fill it with array of attribute values
 */
#define NL_ATTRS(attrList, attrValues) \
    NL_ATTRS_EMPTY(attrList) \
    do { \
        wld_nl80211_nlAttr_t attrArray[] = attrValues; \
        for(uint32_t i = 0; i < SWL_ARRAY_SIZE(attrArray); i++) { \
            swl_unLiList_add(&attrList, &(attrArray[i])); \
        }; \
    } while(0);

/*
 * @brief define nl attribute with type sized value
 */
#define NL_ATTR_VAL(t, v) {.type = t, .data = {.raw = {.len = sizeof(v), .ptr = &(v)}}}

/*
 * @brief define nl attribute with sized data buffer
 */
#define NL_ATTR_DATA(t, l, p) {.type = t, .data = {.raw = {.len = (l), .ptr = (p)}}}

/*
 * @brief define nl attribute with no data
 */
#define NL_ATTR(t) {.type = t}

/*
 * @brief define nested nl attribute with no data
 */
#define NL_ATTR_NESTED(attr, t) \
    wld_nl80211_nlAttr_t attr; \
    wld_nl80211_initNlAttr(&attr); \
    attr.type = t; \
    attr.nested = true;

/*
 * @brief macro to clear nl attributes list
 */
#define NL_ATTRS_CLEAR(pAttrList) \
    wld_nl80211_cleanNlAttrList(pAttrList)

/*
 * @brief prototype of handler for request reply
 * (and for all chunks of multipart answer message)
 *
 * @param rc result of initial checks of the reply
 * @param nlh netlink header of received msg, to be parsed
 * @param priv user data provided when sending the request
 *
 * @return SWL_RC_OK when message is valid, handled, but waiting for next parts
 *         SWL_RC_CONTINUE when message is valid, but ignored
 *         SWL_RC_DONE when message is processed and all reply parts are received:
 *                     request is terminated and can be cleared
 *         SWL_RC_ERROR in case of error: request will be cancelled
 */
typedef swl_rc_ne (* wld_nl80211_handler_f) (swl_rc_ne rc, struct nlmsghdr* nlh, void* priv);

#endif /* INCLUDE_WLD_WLD_NL80211_ATTR_H_ */
