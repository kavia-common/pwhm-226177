/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2023 SoftAtHome
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

#include <stdarg.h>    // needed for cmocka
#include <sys/types.h> // needed for cmocka
#include <setjmp.h>    // needed for cmocka
#include <cmocka.h>
#include "wld.h"
#include "wld_accesspoint.h"
#include "wld_util.h"
#include "test-toolbox/ttb_mockTimer.h"
#include "../testHelper/wld_th_mockVendor.h"
#include "../testHelper/wld_th_radio.h"
#include "../testHelper/wld_th_vap.h"
#include "../testHelper/wld_th_dm.h"
#include "swl/ttb/swl_ttb.h"
#include "swl/swl_common.h"
#include "amxd/amxd_object_expression.h"

static wld_th_dm_t dm;
typedef struct {
    swl_macChar_t mac;
    bool expecAdd;
    bool expecDel;
} mfDesc_t;

/*
 * possible MF actions mask
 */
#define M_ADD   0x0001
#define M_DEL   0x0002

/*
 * possible MF action applying methods:
 */
#define M_MAC   0x0100 //remove MF by clearing mac
#define M_TRANS 0x0200 //add/del MF by creating/deleting relative instance
#define M_RPC   0x0400 //add/del MF by calling appropriate rpc

/*
 * operation mask: including the required action and the application method
 */
typedef swl_mask_m mfOp_m;

static int s_setupSuite(void** state _UNUSED) {
    assert_true(wld_th_dm_init(&dm));
    return 0;
}

static int s_teardownSuite(void** state _UNUSED) {
    wld_th_dm_destroy(&dm);
    return 0;
}

char mfAddrLst[256] = {0};

static bool s_checkMfAddrList(T_AccessPoint* vap, const char* expecMfAddrList) {
    char* curMfAddrLst = amxd_object_get_cstring_t(vap->pBus, "MACFilterAddressList", NULL);
    bool ret = swl_str_matches(curMfAddrLst, expecMfAddrList);
    free(curMfAddrLst);
    return ret;
}

/*
 * @brief manage MF/PF Entry/TempEntry by using datamodel RPCs
 *
 * @param categFObj object of "AccessPoint.MACFiltering" or "AccessPoint.ProbeFilteringFiltering"
 * @param rpc name of RPC to use: "adEntry", "delEntry", "addTempEntry", "delTempEntry"
 * @param pMf pointer to filter entry description
 *
 * @return true on success
 */
static bool s_applyMfRpc(amxd_object_t* categFObj, const char* rpc, mfDesc_t* pMf) {
    assert_non_null(categFObj);
    ttb_var_t* args = ttb_object_createArgs();
    assert_non_null(args);
    amxc_var_set_type(args, AMXC_VAR_ID_HTABLE);
    amxc_var_add_key(cstring_t, args, "mac", pMf->mac.cMac);
    ttb_var_t* replyVar = NULL;
    ttb_reply_t* reply = ttb_object_callFun(dm.ttbBus, categFObj, (char*) rpc, &args, &replyVar);
    bool success = ttb_object_replySuccess(reply);
    ttb_object_cleanReply(&reply, &replyVar);
    return success;
}

/*
 * @brief add MF/PF Entry by creating instance (filled with MAC) via transaction
 *
 * @param listFObj template object of
 * "AccessPoint.MACFiltering.Entry", "AccessPoint.MACFiltering.TempEntry",
 * "AccessPoint.ProbeFilteringFiltering.TempEntry"
 * @param pMf pointer to filter entry description
 *
 * @return true on success
 */
static bool s_applyMfAddInstTrans(amxd_object_t* listFObj, mfDesc_t* pMf) {
    amxd_object_t* inst = NULL;
    amxd_object_add_instance(&inst, listFObj, NULL, 0, NULL);
    return swl_typeCharPtr_commitObjectParam(inst, "MACAddress", (char*) pMf->mac.cMac);
}

/*
 * @brief remove MF/PF entry by deleting the relative object instance
 *
 * @param mfObj mac filter entry instance
 *
 * @return true on success
 */
static bool s_applyMfDelInstTrans(amxd_object_t* mfObj) {
    return (swl_object_delInstWithTransOnLocalDm(mfObj) == amxd_status_ok);
}

/*
 * @brief remove MF/PF entry by clearing its relative mac value
 *
 * @param mfObj mac filter entry instance
 *
 * @return true on success
 */
static bool s_applyMfDelUnsetMac(amxd_object_t* mfObj) {
    assert_non_null(mfObj);
    return swl_typeCharPtr_commitObjectParam(mfObj, "MACAddress", "");
}

static void s_execOperMf(T_AccessPoint* vap, mfDesc_t* pMf, const char* categF, mfOp_m op, bool temp, amxd_object_t** pListFObj, int** ppCount) {
    amxd_object_t* categFObj = amxd_object_get(vap->pBus, categF);
    assert_non_null(categFObj);
    const char* list;
    const char* rpc;
    int* pCount;
    swl_macBin_t* pAddrList;
    if(!temp) {
        list = "Entry";
        if(op & M_ADD) {
            rpc = "addEntry";
        } else if(op & M_DEL) {
            rpc = "delEntry";
        }
    } else {
        list = "TempEntry";
        if(op & M_ADD) {
            rpc = "addTempEntry";
        } else if(op & M_DEL) {
            rpc = "delTempEntry";
        }
    }
    if(swl_str_matches(categF, "MACFiltering")) {
        if(!temp) {
            pCount = &vap->MF_EntryCount;
            pAddrList = (swl_macBin_t*) vap->MF_Entry;
        } else {
            pCount = &vap->MF_TempEntryCount;
            pAddrList = (swl_macBin_t*) vap->MF_Temp_Entry;
        }
    } else if(swl_str_matches(categF, "ProbeFiltering")) {
        if(temp) {
            pCount = &vap->PF_TempEntryCount;
            pAddrList = (swl_macBin_t*) vap->PF_Temp_Entry;
        }
    }
    assert_non_null(pCount);
    assert_non_null(pAddrList);

    amxd_object_t* listFObj = amxd_object_get(categFObj, list);
    assert_non_null(listFObj);

    W_SWL_SETPTR(ppCount, pCount);
    W_SWL_SETPTR(pListFObj, listFObj);

    bool success;
    if(op & M_RPC) {
        //add/del with rpc
        success = s_applyMfRpc(categFObj, rpc, pMf);
    } else if(op & M_ADD) {
        //add with transaction
        success = s_applyMfAddInstTrans(listFObj, pMf);
    } else if(op & M_DEL) {
        amxp_expr_t expression;
        amxp_expr_buildf(&expression, "MACAddress=='%s'", pMf->mac.cMac);
        amxd_object_t* mfObj = amxd_object_find_instance(listFObj, &expression);
        amxp_expr_clean(&expression);
        if(op & M_MAC) {
            //remove by clearing MACAddress
            success = s_applyMfDelUnsetMac(mfObj);
        } else if(op & M_TRANS) {
            //remove by deleting instance
            success = s_applyMfDelInstTrans(mfObj);
        }
    }
    bool expecSuccess = ((op & M_DEL) ? pMf->expecDel : pMf->expecAdd);
    assert_int_equal(success, expecSuccess);
    if(!expecSuccess) {
        return;
    }
}

static void s_checkEntryMf(mfDesc_t* pMf, amxd_object_t* listFObj, mfOp_m op, char* outMfAddrLst, size_t outMfAddrLstSize, int32_t* pCount) {

    /*
     * checking phase: go through the required MF/PF Entry/TempEntry instance list
     * and check the entries count, and the MAC values matching for each entry
     * against the relative list in the internal AP context
     */
    swl_str_copy(outMfAddrLst, outMfAddrLstSize, NULL);
    int32_t count = 0;
    amxd_object_t* mfObj = NULL;
    amxd_object_for_each(instance, it, listFObj) {
        amxd_object_t* mfentry = amxc_container_of(it, amxd_object_t, it);
        char* macstr = amxd_object_get_cstring_t(mfentry, "MACAddress", NULL);
        swl_macChar_t macChar;
        if(swl_typeMacChar_fromChar(&macChar, macstr)) {
            count++;
            if(pMf && SWL_MAC_CHAR_MATCHES(&macChar, &pMf->mac)) {
                if(op & M_ADD) {
                    //on addition: only one instance per mac
                    assert_null(mfObj);
                } else if(op & M_DEL) {
                    //on deletion: no instance shall be found
                    assert_false(true);
                }
                mfObj = mfentry;
            }
            swl_strlst_cat(outMfAddrLst, outMfAddrLstSize, ",", macstr);
        }
        free(macstr);
    }
    W_SWL_SETPTR(pCount, count);
}

static void s_manageMf(T_AccessPoint* vap, mfDesc_t* pMf, const char* categF, mfOp_m op, bool temp) {
    amxd_object_t* listFObj = NULL;
    int* pCount = NULL;
    s_execOperMf(vap, pMf, categF, op, temp, &listFObj, &pCount);
    assert_non_null(pCount);
    ttb_mockTimer_goToFutureMs(100);
    mfAddrLst[0] = 0;
    int32_t count = 0;
    s_checkEntryMf(pMf, listFObj, op, mfAddrLst, sizeof(mfAddrLst), &count);
    assert_int_equal(*pCount, count);
    assert_true(s_checkMfAddrList(vap, mfAddrLst));
}

void test_addDelMfPf(void** state _UNUSED) {

    T_AccessPoint* vap = dm.bandList[SWL_FREQ_BAND_2_4GHZ].vapPriv;

    mfDesc_t input[] = {
        {.mac.cMac = "00:11:22:AA:BB:01", .expecAdd = true, .expecDel = true, },
        {.mac.cMac = "00:11:22:AA:BB:02", .expecAdd = true, .expecDel = true, },
        {.mac.cMac = "00:11:22:AA:BB:03", .expecAdd = true, .expecDel = true, },
        {.mac.cMac = "00:11:22:AA:BB:04", .expecAdd = true, .expecDel = true, },
        {.mac.cMac = "00:11:22:AA:zz:04", .expecAdd = false, .expecDel = false, }, //invalid mac
        {.mac.cMac = "00:11:22:AA:BB:05", .expecAdd = true, .expecDel = true, },
    };

    /* Test adding MF entries */
    for(uint32_t i = 0; i < SWL_ARRAY_SIZE(input); i++) {
        /* alternate for addition between RPC and Transactions */
        mfOp_m op = M_ADD;
        if(i % 2 == 0) {
            op |= M_RPC;
        } else {
            op |= M_TRANS;
        }
        //MF.addEntry
        s_manageMf(vap, &input[i], "MACFiltering", op, false);
        //MF.addTempEntry
        s_manageMf(vap, &input[i], "MACFiltering", op, true);
        //PF.addTempEntry
        s_manageMf(vap, &input[i], "ProbeFiltering", op, true);
    }

    /* Test rejection when trying to duplicate MF entries */
    for(uint32_t i = 0; i < SWL_ARRAY_SIZE(input); i++) {
        /* alternate for addition between RPC and Transactions */
        mfOp_m op = M_ADD;
        if(i % 2 == 0) {
            op |= M_RPC;
        } else {
            op |= M_TRANS;
            input[i].expecAdd = false;
        }
        //duplicated MF.addEntry: expect error
        s_manageMf(vap, &input[i], "MACFiltering", op, false);
        //duplicated MF.addTempEntry: expect error
        s_manageMf(vap, &input[i], "MACFiltering", op, true);
        //duplicated PF.addTempEntry: expect error
        s_manageMf(vap, &input[i], "ProbeFiltering", op, true);
    }

    /* Test deleting MF entries */
    for(uint32_t i = 0; i < SWL_ARRAY_SIZE(input); i++) {
        /* alternate for deletion between RPC, Transactions and clearing MAC */
        mfOp_m op = M_DEL;
        if(i % 3 == 0) {
            op |= M_RPC;
        } else if(i % 3 == 1) {
            op |= M_TRANS;
        } else {
            op |= M_MAC;
        }
        //MF.delEntry
        s_manageMf(vap, &input[i], "MACFiltering", op, false);
        //MF.delTempEntry
        s_manageMf(vap, &input[i], "MACFiltering", op, true);
        //PF.delTempEntry
        s_manageMf(vap, &input[i], "ProbeFiltering", op, true);
    }
}

static void test_addMultiMf(void** state _UNUSED) {

    T_AccessPoint* vap = dm.bandList[SWL_FREQ_BAND_2_4GHZ].vapPriv;

    mfDesc_t input[] = {
        {.mac.cMac = "00:11:22:AA:CC:01", .expecAdd = true, .expecDel = true, },
        {.mac.cMac = "00:11:22:AA:CC:02", .expecAdd = true, .expecDel = true, },
        {.mac.cMac = "00:11:22:AA:CC:03", .expecAdd = true, .expecDel = true, },
        {.mac.cMac = "00:11:22:AA:CC:04", .expecAdd = true, .expecDel = true, },
        {.mac.cMac = "00:11:22:AA:CC:05", .expecAdd = true, .expecDel = true, },
        {.mac.cMac = "00:11:22:AA:CC:06", .expecAdd = true, .expecDel = true, },
        {.mac.cMac = "00:11:22:AA:CC:07", .expecAdd = true, .expecDel = true, },
        {.mac.cMac = "00:11:22:AA:CC:08", .expecAdd = true, .expecDel = true, },
    };

    amxd_object_t* listFObj = NULL;
    int* pCtxCount = NULL;

    mfOp_m op = M_ADD | M_TRANS;

    /* Test adding MF entries */
    for(uint32_t i = 0; i < SWL_ARRAY_SIZE(input); i++) {
        //MF.addEntry in sequence
        s_execOperMf(vap, &input[i], "MACFiltering", op, false, &listFObj, &pCtxCount);
        ttb_mockTimer_goToFutureMs(i % 3 ? 150 : 50);
    }

    ttb_mockTimer_goToFutureMs(1000);

    mfAddrLst[0] = 0;
    int32_t dmCount = 0;

    s_checkEntryMf(NULL, listFObj, op, mfAddrLst, sizeof(mfAddrLst), &dmCount);

    assert_int_equal(*pCtxCount, dmCount);
    assert_true(s_checkMfAddrList(vap, mfAddrLst));
}

static void test_setMfAddrList(void** state _UNUSED) {
    T_AccessPoint* vap = dm.bandList[SWL_FREQ_BAND_2_4GHZ].vapPriv;
    char mfAddrListStr[128] = "00:11:22:AA:BB:a0,00:11:22:AA:BB:a1";
    assert_true(swl_typeCharPtr_commitObjectParam(vap->pBus, "MACFilterAddressList", mfAddrListStr));
    ttb_mockTimer_goToFutureMs(100);
    swl_macBin_t macBin;
    char* p = mfAddrListStr;
    uint32_t id = 0;
    while(p) {
        swl_typeMacBin_fromChar(&macBin, strsep(&p, ","));
        assert_memory_equal(vap->MF_Entry[id], macBin.bMac, SWL_MAC_BIN_LEN);
        id++;
    }
    assert_int_equal(vap->MF_EntryCount, id);
    assert_true(swl_typeCharPtr_commitObjectParam(vap->pBus, "MACFilterAddressList", ""));
    ttb_mockTimer_goToFutureMs(100);
    assert_int_equal(vap->MF_EntryCount, 0);
}

static void test_changeMfAddrList(void** state _UNUSED) {
    T_AccessPoint* vap = dm.bandList[SWL_FREQ_BAND_2_4GHZ].vapPriv;

    char mfAddrListStr1[128] = "00:11:22:AA:BB:CC";
    assert_true(swl_typeCharPtr_commitObjectParam(vap->pBus, "MACFilterAddressList", mfAddrListStr1));
    ttb_mockTimer_goToFutureMs(100);
    assert_string_equal(vap->MF_AddressList, mfAddrListStr1);
    assert_int_equal(vap->MF_EntryCount, 1);
    swl_macBin_t macBin1;
    swl_typeMacBin_fromChar(&macBin1, mfAddrListStr1);
    assert_memory_equal(vap->MF_Entry[0], macBin1.bMac, SWL_MAC_BIN_LEN);

    char mfAddrListStr2[128] = "00:11:22:AA:BB:CD";
    assert_true(swl_typeCharPtr_commitObjectParam(vap->pBus, "MACFilterAddressList", mfAddrListStr2));
    ttb_mockTimer_goToFutureMs(100);
    assert_string_equal(vap->MF_AddressList, mfAddrListStr2);
    assert_int_equal(vap->MF_EntryCount, 1);
    swl_macBin_t macBin2;
    swl_typeMacBin_fromChar(&macBin2, mfAddrListStr2);
    assert_memory_equal(vap->MF_Entry[0], macBin2.bMac, SWL_MAC_BIN_LEN);

    // remove entries to free memory for valgrind
    assert_true(swl_typeCharPtr_commitObjectParam(vap->pBus, "MACFilterAddressList", ""));
    ttb_mockTimer_goToFutureMs(100);
    assert_int_equal(vap->MF_EntryCount, 0);
}

int main(int argc _UNUSED, char* argv[] _UNUSED) {
    sahTraceSetLevel(TRACE_LEVEL_INFO);
    sahTraceAddZone(sahTraceLevel(), "apMf");
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_addDelMfPf),
        cmocka_unit_test(test_addMultiMf),
        cmocka_unit_test(test_setMfAddrList),
        cmocka_unit_test(test_changeMfAddrList),
    };
    int rc = cmocka_run_group_tests(tests, s_setupSuite, s_teardownSuite);
    sahTraceClose();
    return rc;
}

