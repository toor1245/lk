#include <tee_client_api.h>

#include <string.h>
#include <stdlib.h>

#include <dev/tee.h>

#include <lk/err.h>

#include "tee_log.h"

static status_t pack_tee_operation(struct tee_device *dev, TEEC_Operation *teec_op, struct tee_operation *tee_op) {
    if (teec_op == NULL)
        return ERR_INVALID_ARGS;

    status_t err = 0;

    tee_op->started = teec_op->started;
    tee_op->paramTypes = teec_op->paramTypes;

    for (int i = 0; i < 4; i++) {
        uint32_t ptype = TEEC_PARAM_TYPE_GET(teec_op->paramTypes, i);

        if (ptype == TEEC_MEMREF_TEMP_INPUT || 
            ptype == TEEC_MEMREF_TEMP_OUTPUT || 
            ptype == TEEC_MEMREF_TEMP_INOUT) {
            
            size_t sz = teec_op->params[i].tmpref.size;
            struct tee_shm *bounce_shm = NULL;
            err = dev->ops->shm_alloc(dev, sz, &bounce_shm);
            if (err) {
                err = ERR_GENERIC;
                goto cleanup;
            }

            if (ptype != TEEC_MEMREF_TEMP_OUTPUT && teec_op->params[i].tmpref.buffer != NULL) {
                memcpy((void *)bounce_shm->addr, teec_op->params[i].tmpref.buffer, sz);
            }

            tee_op->params[i].tmem.buffer = (void *)bounce_shm->addr;
            tee_op->params[i].tmem.size = sz;
            tee_op->params[i].tmem.shm = bounce_shm;
        } else if (ptype == TEEC_VALUE_INPUT || 
                   ptype == TEEC_VALUE_OUTPUT || 
                   ptype == TEEC_VALUE_INOUT) {
            tee_op->params[i].value.a = teec_op->params[i].value.a;
            tee_op->params[i].value.b = teec_op->params[i].value.b;
        }
    }

    return NO_ERROR;

cleanup:
    tee_dev_free_operation(dev, tee_op);

    return err;
}

static void unpack_tee_operation(struct tee_device *dev, struct tee_operation *tee_op, TEEC_Operation *teec_op) {
    if (tee_op == NULL || teec_op == NULL)
        return;

    for (int i = 0; i < 4; i++) {
        uint32_t ptype = TEEC_PARAM_TYPE_GET(teec_op->paramTypes, i);

        if (ptype == TEEC_MEMREF_TEMP_OUTPUT ||
            ptype == TEEC_MEMREF_TEMP_INOUT ||
            ptype == TEEC_MEMREF_TEMP_INPUT) {
            
            if (ptype == TEEC_MEMREF_TEMP_OUTPUT || ptype == TEEC_MEMREF_TEMP_INOUT) {
                size_t ret_sz = tee_op->params[i].tmem.size;
                teec_op->params[i].tmpref.size = ret_sz;

                if (tee_op->params[i].tmem.shm && teec_op->params[i].tmpref.buffer) {
                    memcpy(teec_op->params[i].tmpref.buffer, 
                           (void *)tee_op->params[i].tmem.shm->addr, ret_sz);
                }
            }
        } else if (ptype == TEEC_VALUE_OUTPUT || ptype == TEEC_VALUE_INOUT) {
            teec_op->params[i].value.a = tee_op->params[i].value.a;
            teec_op->params[i].value.b = tee_op->params[i].value.b;
        }
    }
}

TEEC_Result TEEC_InitializeContext(
    const char *name,
    TEEC_Context *context)
{
    struct tee_device *dev = name ?
        tee_dev_get_by_name(name) : tee_dev_get_first();

    if (dev == NULL)
        return TEEC_ERROR_GENERIC;

    struct tee_context_imp *ctx = NULL;
    int err = dev->ops->init_context(dev, &ctx);
    if (err)
        return TEE_ERROR_GENERIC;

    context->imp = (void *)ctx;

    return TEEC_SUCCESS;
}

TEEC_Result TEEC_OpenSession(
    TEEC_Context    *context,
    TEEC_Session    *session,
    const TEEC_UUID *destination,
    uint32_t        connectionMethod,
    void            *connectionData,
    TEEC_Operation  *operation,
    uint32_t        *returnOrigin) 
{
    struct tee_context_imp *ctx = (struct tee_context_imp *)context->imp;
    struct tee_device *tee_dev = ctx->tee_dev;

    struct tee_open_session_data data = {
        .class_login = connectionMethod,
    };
    memcpy(data.uuid, destination, sizeof(TEEC_UUID));

    struct tee_session_imp *session_imp = NULL;
    int err = tee_dev->ops->open_session(tee_dev, &data, &session_imp);
    if (err) {
        *returnOrigin = data.ret_origin;
        return data.ret;
    }

    *returnOrigin = data.ret_origin;
    session->imp = (void *)session_imp;

    return data.ret;
}

TEEC_Result TEEC_InvokeCommand(
    TEEC_Session     *session,
    uint32_t         commandID,
    TEEC_Operation   *operation,
    uint32_t         *returnOrigin)
{
    TEEC_Result final_res = TEEC_SUCCESS;
    struct tee_session_imp *session_imp = (struct tee_session_imp *)session->imp;
    struct tee_device *tee_dev = session_imp->tee_dev;

    struct tee_operation op = (struct tee_operation) {
        .started = operation->started,
        .paramTypes = operation->paramTypes,
    };

    int err = pack_tee_operation(tee_dev, operation, &op);
    if (err)
        return TEEC_ERROR_GENERIC;

    struct tee_invoke_cmd_data data = (struct tee_invoke_cmd_data) {
        .cmd_id = commandID,
        .operation = &op,
        .session_id = session_imp->session_id,
    };

    err = tee_dev->ops->invoke_cmd(tee_dev, &data);
    if (err) {
        *returnOrigin = data.ret_origin;
        final_res = data.ret;
        goto out_free_operation;
    }

    *returnOrigin = data.ret_origin;
    final_res = data.ret;

    unpack_tee_operation(tee_dev, &op, operation);

out_free_operation:
    tee_dev_free_operation(tee_dev, &op);

    return final_res;
}

void TEEC_CloseSession(TEEC_Session *session)
{

}

void TEEC_FinalizeContext(TEEC_Context *context)
{

}