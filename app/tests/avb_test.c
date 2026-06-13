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

/* FFA UUID ecf5c6a3-9a66-4dc1-9fd2-8f90f3eb5f05 */
/* AVB UUID 023f8f1a-292a-432b-8fc4-de8471358067 */
#define TA_AVB_UUID { 0x1a8f3f02, 0x292a, 0x2b43, \
        { 0x8f, 0xc4, 0xde, 0x84, 0x71, 0x35, 0x80, 0x67} }

#define TA_AVB_CMD_READ_ROLLBACK_INDEX  0
#define TA_AVB_CMD_WRITE_ROLLBACK_INDEX 1
#define TA_AVB_CMD_READ_LOCK_STATE      2
#define TA_AVB_CMD_WRITE_LOCK_STATE     3
#define TA_AVB_CMD_READ_PERSIST_VALUE   4
#define TA_AVB_CMD_WRITE_PERSIST_VALUE  5

static int do_avb_test(void) {
TEEC_Context ctx;
    TEEC_Session sess;
    TEEC_Operation op;
    TEEC_Result res;
    uint32_t err_origin;
    TEEC_UUID uuid = TA_AVB_UUID;

    printf("Initialize context...\n");
    res = TEEC_InitializeContext(NULL, &ctx);
    if (res != TEEC_SUCCESS) {
        printf("TEEC_InitializeContext failed with code 0x%x", res);
        return res;
    }

    printf("Open session...\n");
    res = TEEC_OpenSession(&ctx, &sess, &uuid,
                   TEEC_LOGIN_PUBLIC, NULL, NULL, &err_origin);
    if (res != TEEC_SUCCESS) {
        printf("TEEC_Opensession failed with code 0x%x origin 0x%x", res, err_origin);
        return res;
    }

    printf("\n--- Testing Lock State ---\n");
    uint32_t test_lock_state = 1;
    
    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);
    op.params[0].value.a = test_lock_state;

    res = TEEC_InvokeCommand(&sess, TA_AVB_CMD_WRITE_LOCK_STATE, &op, &err_origin);
    if (res != TEEC_SUCCESS) {
        printf("TEEC_InvokeCommand(WRITE_LOCK_STATE) failed 0x%x", res);
        return res;
    }
    printf("Successfully wrote lock state: %u\n", test_lock_state);

    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_OUTPUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);
    
    res = TEEC_InvokeCommand(&sess, TA_AVB_CMD_READ_LOCK_STATE, &op, &err_origin);
    if (res != TEEC_SUCCESS) {
        printf("TEEC_InvokeCommand(READ_LOCK_STATE) failed 0x%x", res);
        return res;
    }
    printf("Successfully read lock state: %u\n", op.params[0].value.a);

    printf("\n--- Testing Rollback Index ---\n");
    uint32_t slot = 0;
    uint64_t write_idx = 0x123456789ABCDEF0; /* 64-bit test value */
    
    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT, TEEC_VALUE_INPUT, TEEC_NONE, TEEC_NONE);
    op.params[0].value.a = slot;
    op.params[1].value.a = (uint32_t)(write_idx >> 32); /* High 32 bits */
    op.params[1].value.b = (uint32_t)(write_idx & 0xFFFFFFFF); /* Low 32 bits */

    res = TEEC_InvokeCommand(&sess, TA_AVB_CMD_WRITE_ROLLBACK_INDEX, &op, &err_origin);
    if (res != TEEC_SUCCESS) {
        printf("TEEC_InvokeCommand(WRITE_ROLLBACK_INDEX) failed 0x%x", res);
        return res;
    }
    printf("Successfully wrote rollback index 0x%llx to slot %u\n", write_idx, slot);

    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INPUT, TEEC_VALUE_OUTPUT, TEEC_NONE, TEEC_NONE);
    op.params[0].value.a = slot;

    res = TEEC_InvokeCommand(&sess, TA_AVB_CMD_READ_ROLLBACK_INDEX, &op, &err_origin);
    if (res != TEEC_SUCCESS) {
        printf("TEEC_InvokeCommand(READ_ROLLBACK_INDEX) failed 0x%x", res);
        return res;
    }

    uint64_t read_idx = ((uint64_t)op.params[1].value.a << 32) | op.params[1].value.b;
    printf("Successfully read rollback index 0x%llx from slot %u\n", read_idx, slot);

    printf("\n--- Testing Persistent Values ---\n");
    char *prop_name = "test_property";
    char *prop_value = "Hello OP-TEE World!";
    char read_buffer[64] = {0};

    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT, TEEC_MEMREF_TEMP_INPUT, TEEC_NONE, TEEC_NONE);
    op.params[0].tmpref.buffer = prop_name;
    op.params[0].tmpref.size = strlen(prop_name) + 1;
    op.params[1].tmpref.buffer = prop_value;
    op.params[1].tmpref.size = strlen(prop_value) + 1;

    res = TEEC_InvokeCommand(&sess, TA_AVB_CMD_WRITE_PERSIST_VALUE, &op, &err_origin);
    if (res != TEEC_SUCCESS) {
        printf("TEEC_InvokeCommand(WRITE_PERSIST_VALUE) failed 0x%x", res);
        return res;
    }
    printf("Successfully wrote persistent value '%s' to '%s'\n", prop_value, prop_name);

    memset(&op, 0, sizeof(op));
    op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT, TEEC_MEMREF_TEMP_INOUT, TEEC_NONE, TEEC_NONE);
    op.params[0].tmpref.buffer = prop_name;
    op.params[0].tmpref.size = strlen(prop_name) + 1;
    op.params[1].tmpref.buffer = read_buffer;
    op.params[1].tmpref.size = sizeof(read_buffer);

    res = TEEC_InvokeCommand(&sess, TA_AVB_CMD_READ_PERSIST_VALUE, &op, &err_origin);
    if (res != TEEC_SUCCESS) {
        printf("TEEC_InvokeCommand(READ_PERSIST_VALUE) failed 0x%x", res);
        return res;
    }
    printf("Successfully read persistent value '%s' from '%s'\n", read_buffer, prop_name);


    printf("\nClosing session and context...\n");
    TEEC_CloseSession(&sess);
    TEEC_FinalizeContext(&ctx);
    
    printf("All tests passed successfully.\n");
    return 0;
}

int avb_test(int argc, const console_cmd_args *argv) {
    return do_avb_test();
}
