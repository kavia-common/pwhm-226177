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
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <setjmp.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cmocka.h>

#include <debug/sahtrace.h>

#include "wld.h"
#include "wld_daemon.h"

static void test_wld_daemon_setArgList(void** state) {
    (void) state;

    char* command = "hostapd";
    wld_process_t dmnProcess = {0};
    assert_true(wld_dmn_initializeDeamon(&dmnProcess, command));
    assert_ptr_equal((void*) ((uintptr_t) amxc_var_get_const_uint64_t(dmnProcess.settings)), &dmnProcess);

    char* args = "-ddt -s";
    wld_dmn_setArgList(&dmnProcess, args);

    assert_string_equal(dmnProcess.args, args);
    assert_int_equal(dmnProcess.argLen, strlen(args));
    assert_int_equal(dmnProcess.nrArgs, 2);
    assert_string_equal(dmnProcess.argList[1], "-ddt"); // argList[0] is the cmd
    assert_string_equal(dmnProcess.argList[2], "-s");

    wld_dmn_cleanupDaemon(&dmnProcess);
}

static void test_wld_daemon_setArgList_reset(void** state) {
    (void) state;

    char* command = "hostapd";
    wld_process_t dmnProcess = {0};
    assert_true(wld_dmn_initializeDeamon(&dmnProcess, command));

    char* args = "-ddt -s";
    wld_dmn_setArgList(&dmnProcess, args);

    assert_string_equal(dmnProcess.args, args);
    assert_int_equal(dmnProcess.argLen, strlen(args));
    assert_int_equal(dmnProcess.nrArgs, 2);
    assert_string_equal(dmnProcess.argList[1], "-ddt"); // argList[0] is the cmd
    assert_string_equal(dmnProcess.argList[2], "-s");

    char* newArgs = "-ddt";
    wld_dmn_setArgList(&dmnProcess, newArgs);

    assert_string_equal(dmnProcess.args, newArgs);
    assert_int_equal(dmnProcess.argLen, strlen(newArgs));
    assert_int_equal(dmnProcess.nrArgs, 1);
    assert_string_equal(dmnProcess.argList[1], "-ddt"); // argList[0] is the cmd

    wld_dmn_cleanupDaemon(&dmnProcess);
}

static int s_setupSuite(void** state) {
    (void) state;
    return 0;
}

static int s_teardownSuite(void** state) {
    (void) state;
    return 0;
}

int main(int argc _UNUSED, char* argv[] _UNUSED) {
    sahTraceOpen(__FILE__, TRACE_TYPE_STDERR);
    if(!sahTraceIsOpen()) {
        fprintf(stderr, "FAILED to open SAH TRACE\n");
    }
    sahTraceSetLevel(TRACE_LEVEL_WARNING);
    sahTraceSetTimeFormat(TRACE_TIME_APP_SECONDS);
    sahTraceAddZone(sahTraceLevel(), "wldDmn");
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_wld_daemon_setArgList),
        cmocka_unit_test(test_wld_daemon_setArgList_reset),
    };
    int rc = cmocka_run_group_tests(tests, s_setupSuite, s_teardownSuite);
    sahTraceClose();
    return rc;
}
