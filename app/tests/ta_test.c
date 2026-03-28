/*
 * Copyright (c) 2026 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#include <arch.h>
#include <arch/ops.h>
#include <lk/console_cmd.h>
#include <lk/debug.h>
#include <lk/err.h>
#include <platform.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tee_client_api.h>

#define TA_FFA_UUID { 0xa3c6f5ec, 0x669a, 0xc14d, \
        { 0x9f, 0xd2, 0x8f, 0x90, 0xf3, 0xeb, 0x5f, 0x05} }

static int do_ta_test(void) {
	TEEC_Result res;
	TEEC_Context ctx;
	TEEC_Session sess;
	TEEC_Operation op;
	TEEC_UUID uuid = TA_FFA_UUID;
	uint32_t err_origin;

    res = TEEC_InitializeContext(NULL, &ctx);
    if (res != TEEC_SUCCESS) {
        printf("Failed to initialize\n");
        return -1;
    }

    res = TEEC_OpenSession(&ctx, &sess, &uuid, TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
	if (res != TEEC_SUCCESS) {
		printf("TEEC_Opensession failed with code 0x%x origin 0x%x\n", res, err_origin);
        return -1;
    }

    memset(&op, 0, sizeof(op));

    op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT, TEEC_VALUE_OUTPUT,
					 TEEC_NONE, TEEC_NONE);
	op.params[0].value.a = 42;

    printf("Invoking TA to increment %d\n", op.params[0].value.a);

    res = TEEC_InvokeCommand(&sess, 0, &op, &err_origin);
	if (res != TEEC_SUCCESS) {
		printf("TEEC_InvokeCommand failed with code 0x%x origin 0x%x", res, err_origin);
        return 1;
    }

	printf("TA incremented value to %d\n", op.params[1].value.a);

    TEEC_CloseSession(&sess);

	TEEC_FinalizeContext(&ctx);

    return 0;
}

int ta_test(int argc, const console_cmd_args *argv) {
    return do_ta_test();
}
