/*
 * Copyright (c) 2026 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#include <dev/tee.h>
#include <dev/arm_ffa.h>

#include <arch/defines.h>

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include <optee_msg.h>
#include <optee_ffa.h>
#include <optee_rpc_cmd.h>

#include <dev/rpmb.h>

#include <lk/err.h>
#include <lk/init.h>
#include <lk/list.h>

#define OPTEE_FFA_UUID_0 0xe0786148
#define OPTEE_FFA_UUID_1 0xe311f8e7
#define OPTEE_FFA_UUID_2 0x02005ebc
#define OPTEE_FFA_UUID_3 0x1bc5d5a5

/* e0786148-e311f8e7-02005ebc-1bc5d5a5 */
static const uint32_t optee_uuid[4] = {
    OPTEE_FFA_UUID_0,
    OPTEE_FFA_UUID_1,
    OPTEE_FFA_UUID_2,
    OPTEE_FFA_UUID_3,
};

struct optee_ffa_info {
    ffa_endpoint_id_t vm_id;
    ffa_endpoint_id_t sp_id;
    struct rpmb_dev *rpmb_dev;
    uint32_t rpmb_probe_next_id;
    struct list_node shm_list;
};

struct optee_ffa_call_data {
    uint32_t cmd;
    uint32_t w4;
    uint32_t w5;
    uint32_t w6;
};

struct optee_ffa_shm {
    struct tee_shm *shm;
    struct list_node node;
    ffa_mem_handle_t handle;
};

static struct tee_shm *optee_ffa_find_shm(struct optee_ffa_info *optee_ffa, ffa_mem_handle_t handle) {
    struct optee_ffa_shm *entry;
    list_for_every_entry(&optee_ffa->shm_list, entry, struct optee_ffa_shm, node) {
        if (entry->handle == handle) {
            return entry->shm;
        }
    }

    return NULL;
}

/*
 * Resolve an OPTEE_MSG fmem parameter received from secure world to a
 * local buffer address, or NULL if the reference is unknown or out of
 * bounds. The buffer size is given by mp->u.fmem.size.
 */
static void *optee_ffa_fmem_buffer(struct optee_ffa_info *optee_ffa,
                                   const struct optee_msg_param *mp) {
    if (mp->u.fmem.global_id == OPTEE_MSG_FMEM_INVALID_GLOBAL_ID)
        return NULL;

    struct tee_shm *shm = optee_ffa_find_shm(optee_ffa, mp->u.fmem.global_id);
    if (shm == NULL) {
        printf("OP-TEE FF-A: unknown shm global id 0x%llx\n",
            (unsigned long long)mp->u.fmem.global_id);
        return NULL;
    }

    uint64_t offs = mp->u.fmem.offs_low | ((uint64_t)mp->u.fmem.offs_high << 32);

    /* Local FF-A shares are page aligned, so internal_offs is always 0 */
    if (mp->u.fmem.internal_offs != 0 ||
        offs > shm->size || mp->u.fmem.size > shm->size - offs)
        return NULL;

    return (void *)(shm->addr + offs);
}

static status_t optee_ffa_xchg_caps(ffa_endpoint_id_t current_id,
                                    ffa_endpoint_id_t sp_id) {
    ffa_args_t payload;

    memset(&payload, 0, sizeof(ffa_args_t));
    payload.arg1 = (current_id << 16) | sp_id;
    payload.arg2 = 0;
    payload.arg3 = OPTEE_FFA_EXCHANGE_CAPABILITIES;

    ffa_msg_send_direct_req_64(&payload);

    if (payload.fid != FFA_MSG_SEND_DIRECT_RESP_SMC64 || payload.arg3 != 0) {
        return ERR_GENERIC;
    }

    printf("OP-TEE FF-A Caps: %lld\n", payload.arg4);

    return NO_ERROR;
}

static status_t optee_ffa_init_context(struct tee_device *dev, struct tee_context_imp **ctx) {
    if (dev == NULL || ctx == NULL)
        return ERR_INVALID_ARGS;

    struct tee_context_imp *contex = calloc(1, sizeof(struct tee_context_imp));
    if (contex == NULL)
        return ERR_NO_MEMORY;
    
    contex->tee_dev = dev;
    *ctx = contex;

    return NO_ERROR;
}

static status_t optee_ffa_init(struct tee_device *dev) {
    struct optee_ffa_info *optee_ffa = (struct optee_ffa_info*) dev->priv;
    struct ffa_device *ffa_dev = arm_ffa_get_device();

    ffa_partition_info_t partition;
    status_t err = arm_ffa_find_partition_by_uuid(ffa_dev, optee_uuid, &partition);
    if (err != NO_ERROR) {
        printf("Failed to find partition by optee uuid, err=%d\n", err);
        return err;
    }

    err = optee_ffa_xchg_caps(ffa_dev->endpoint_id, partition.id);
    if (err != NO_ERROR) {
        printf("Failed to exchange OP-TEE FF-A caps, err=%d\n", err);
        return err;
    }

    optee_ffa->sp_id = partition.id;
    optee_ffa->vm_id = ffa_dev->endpoint_id;

    list_initialize(&optee_ffa->shm_list);

    return NO_ERROR;
}

static void optee_ffa_dump_optee_msg_arg(struct optee_msg_arg *arg) {
    if (!arg) {
        printf("[OP-TEE Msg Dump] ERROR: arg pointer is NULL\n");
        return;
    }

    printf("\n================ [OP-TEE MSG ARG DUMP] ================\n");
    printf("  Primary Command ID (cmd) : %u\n", arg->cmd);
    printf("  Return Status Code (ret) : 0x%08X\n", arg->ret);
    printf("  Return Origin            : %u\n", arg->ret_origin);
    printf("  Session ID               : 0x%08X\n", arg->session);
    printf("  Number of Parameters     : %u\n", arg->num_params);
    printf("-------------------------------------------------------\n");

    for (uint32_t i = 0; i < arg->num_params; i++) {
        uint64_t attr = arg->params[i].attr;
        uint32_t type = attr & 0xFF;

        printf("  Param [%u] - Raw Attr: 0x%08llX (Base Type: 0x%02X)\n", i, (unsigned long long)attr, type);

        switch (type) {
            case OPTEE_MSG_ATTR_TYPE_NONE:
                printf("    [TYPE] NONE\n");
                break;

            case OPTEE_MSG_ATTR_TYPE_VALUE_INPUT:
            case OPTEE_MSG_ATTR_TYPE_VALUE_OUTPUT:
            case OPTEE_MSG_ATTR_TYPE_VALUE_INOUT:
                printf("    [TYPE] VALUE\n");
                printf("    Value.a : 0x%016llX\n", (unsigned long long)arg->params[i].u.value.a);
                printf("    Value.b : 0x%016llX\n", (unsigned long long)arg->params[i].u.value.b);
                printf("    Value.c : 0x%016llX\n", (unsigned long long)arg->params[i].u.value.c);
                break;

            case OPTEE_MSG_ATTR_TYPE_FMEM_INPUT:
            case OPTEE_MSG_ATTR_TYPE_FMEM_OUTPUT:
            case OPTEE_MSG_ATTR_TYPE_FMEM_INOUT:
                printf("    [TYPE] FMEM\n");
                printf("    global_id     : 0x%016llX\n", (unsigned long long)arg->params[i].u.fmem.global_id);
                printf("    offs          : 0x%llX\n",
                    (unsigned long long)arg->params[i].u.fmem.offs_low |
                    ((unsigned long long)arg->params[i].u.fmem.offs_high << 32));
                printf("    internal_offs : 0x%X\n", arg->params[i].u.fmem.internal_offs);
                printf("    size          : %llu bytes\n", (unsigned long long)arg->params[i].u.fmem.size);
                break;

            case OPTEE_MSG_ATTR_TYPE_TMEM_INPUT:
            case OPTEE_MSG_ATTR_TYPE_TMEM_OUTPUT:
            case OPTEE_MSG_ATTR_TYPE_TMEM_INOUT:
                printf("    [TYPE] TMEM\n");
                printf("    shm_ref : 0x%016llX\n", (unsigned long long)arg->params[i].u.tmem.shm_ref);
                printf("    buf_ptr : 0x%016llX\n", (unsigned long long)arg->params[i].u.tmem.buf_ptr);
                printf("    size    : %llu bytes\n", (unsigned long long)arg->params[i].u.tmem.size);
                break;

            default:
                printf("    [TYPE] OTHER / UNKNOWN\n");
                printf("    Raw Octets[0-7]  : 0x%016llX\n", *(unsigned long long*)&arg->params[i].u.octets[0]);
                printf("    Raw Octets[8-15] : 0x%016llX\n", *(unsigned long long*)&arg->params[i].u.octets[8]);
                break;
        }
        printf("-------------------------------------------------------\n");
    }
    printf("=======================================================\n\n");
}

static void handle_rpc_shm_alloc(struct tee_device *dev, struct optee_msg_arg *arg) {
    printf("OPTEE_RPC_CMD_SHM_ALLOC called\n");

    if (arg->num_params != 1 ||
        arg->params[0].attr != OPTEE_MSG_ATTR_TYPE_VALUE_INPUT) {
        printf("Invalid parameters for OPTEE_RPC_CMD_SHM_ALLOC\n");
        arg->ret = TEE_ERROR_BAD_PARAMETERS;
        return;
    }

    if (arg->params[0].u.value.a != OPTEE_RPC_SHM_TYPE_KERNEL) {
        printf("Unsupported shm type for OPTEE_RPC_CMD_SHM_ALLOC: %llu\n",
            (unsigned long long)arg->params[0].u.value.a);
        arg->ret = TEE_ERROR_BAD_PARAMETERS;
        return;
    }

    uint64_t size = arg->params[0].u.value.b;

    struct tee_shm *shm = NULL;
    if (dev->ops->shm_alloc(dev, ROUNDUP(size, PAGE_SIZE), &shm) != TEE_SUCCESS) {
        arg->ret = TEE_ERROR_OUT_OF_MEMORY;
        return;
    }

    struct optee_ffa_shm *ffa_shm = (struct optee_ffa_shm *)shm->priv;

    memset(&arg->params[0], 0, sizeof(arg->params[0]));
    arg->params[0].attr = OPTEE_MSG_ATTR_TYPE_FMEM_OUTPUT;
    arg->params[0].u.fmem.size = size;
    arg->params[0].u.fmem.internal_offs = 0;
    arg->params[0].u.fmem.global_id = ffa_shm->handle;

    arg->ret = TEE_SUCCESS;
}

static void handle_rpc_shm_free(struct tee_device *dev, struct optee_msg_arg *arg) {
    struct optee_ffa_info *optee_ffa = (struct optee_ffa_info*) dev->priv;

    printf("OPTEE_RPC_CMD_SHM_FREE called\n");

    if (arg->num_params != 1 ||
        arg->params[0].attr != OPTEE_MSG_ATTR_TYPE_VALUE_INPUT) {
        printf("Invalid parameters for OPTEE_RPC_CMD_SHM_FREE\n");
        arg->ret = TEE_ERROR_BAD_PARAMETERS;
        return;
    }

    struct tee_shm *shm = optee_ffa_find_shm(optee_ffa, arg->params[0].u.value.b);
    if (shm == NULL) {
        printf("Unknown shm global id for OPTEE_RPC_CMD_SHM_FREE: 0x%llx\n",
            (unsigned long long)arg->params[0].u.value.b);
        arg->ret = TEE_ERROR_ITEM_NOT_FOUND;
        return;
    }

    dev->ops->shm_free(dev, shm);

    arg->ret = TEE_SUCCESS;
}

static void handle_rpc_rpmb_reset(struct tee_device *dev, struct optee_msg_arg *arg) {
    struct optee_ffa_info *optee_ffa = (struct optee_ffa_info*) dev->priv;

    printf("OPTEE_RPC_CMD_RPMB_PROBE_RESET called\n");

    if (arg->num_params != 1) {
        printf("Invalid number of parameters for OPTEE_RPC_CMD_RPMB_PROBE_RESET: %u\n",
            arg->num_params);

        arg->ret = TEE_ERROR_BAD_PARAMETERS;
        return;
    }

    if (arg->params[0].attr != OPTEE_MSG_ATTR_TYPE_VALUE_OUTPUT) {
        printf("Invalid parameter type for OPTEE_RPC_CMD_RPMB_PROBE_RESET: 0x%llX\n",
            (uint64_t)arg->params[0].attr);

        arg->ret = TEE_ERROR_BAD_PARAMETERS;
        return;
    }

    optee_ffa->rpmb_dev = NULL;
    optee_ffa->rpmb_probe_next_id = 0;

    arg->params[0].u.value.a = OPTEE_RPC_SHM_TYPE_KERNEL;
    arg->params[0].u.value.b = 0;
    arg->params[0].u.value.c = 0;

    arg->ret = TEE_SUCCESS;
}

static void handle_rpc_rpmb_next(struct tee_device *dev, struct optee_msg_arg *arg) {
    struct optee_ffa_info *optee_ffa = (struct optee_ffa_info*) dev->priv;

    printf("OPTEE_RPC_CMD_RPMB_PROBE_NEXT called\n");

    if (arg->num_params != 2 ||
        arg->params[0].attr != OPTEE_MSG_ATTR_TYPE_VALUE_OUTPUT ||
        arg->params[1].attr != OPTEE_MSG_ATTR_TYPE_FMEM_OUTPUT) {
        printf("Invalid parameters for OPTEE_RPC_CMD_RPMB_PROBE_NEXT\n");
        arg->ret = TEE_ERROR_BAD_PARAMETERS;
        return;
    }

    /* memref[1] is the output buffer for the raw device ID (eMMC CID) */
    uint8_t *dev_id_buf = optee_ffa_fmem_buffer(optee_ffa, &arg->params[1]);
    if (dev_id_buf == NULL) {
        printf("Invalid device ID buffer for OPTEE_RPC_CMD_RPMB_PROBE_NEXT\n");
        arg->ret = TEE_ERROR_BAD_PARAMETERS;
        return;
    }

    struct rpmb_dev *rpmb_dev = rpmb_dev_get(optee_ffa->rpmb_probe_next_id);
    if (rpmb_dev == NULL) {
        printf("No more RPMB devices to probe\n");
        arg->ret = TEE_ERROR_ITEM_NOT_FOUND;
        return;
    }
    optee_ffa->rpmb_probe_next_id++;

    if (rpmb_dev->type != RPMB_TYPE_EMMC) {
        printf("OP-TEE FF-A driver supports only eMMC RPMB devices\n");
        arg->ret = TEE_ERROR_NOT_SUPPORTED;
        return;
    }

    if (arg->params[1].u.fmem.size < rpmb_dev->dev_id_len) {
        printf("Device ID buffer too small for OPTEE_RPC_CMD_RPMB_PROBE_NEXT: %llu < %u\n",
            (unsigned long long)arg->params[1].u.fmem.size, rpmb_dev->dev_id_len);
        arg->ret = TEE_ERROR_BAD_PARAMETERS;
        return;
    }

    memcpy(dev_id_buf, rpmb_dev->dev_id, rpmb_dev->dev_id_len);
    arg->params[1].u.fmem.size = rpmb_dev->dev_id_len;

    optee_ffa->rpmb_dev = rpmb_dev;

    arg->params[0].u.value.a = OPTEE_RPC_RPMB_EMMC;
    arg->params[0].u.value.b = rpmb_dev->capacity;
    arg->params[0].u.value.c = rpmb_dev->rel_wr_count;

    arg->ret = TEE_SUCCESS;
}

static void handle_rpc_rpmb_frames(struct tee_device *dev, struct optee_msg_arg *arg) {
    struct optee_ffa_info *optee_ffa = (struct optee_ffa_info*) dev->priv;

    printf("OPTEE_RPC_CMD_RPMB_FRAMES called\n");

    struct rpmb_dev *rpmb_dev = optee_ffa->rpmb_dev;
    if (rpmb_dev == NULL) {
        printf("No RPMB device selected, probe first\n");
        arg->ret = TEE_ERROR_ITEM_NOT_FOUND;
        return;
    }

    if (arg->num_params != 2 ||
        arg->params[0].attr != OPTEE_MSG_ATTR_TYPE_FMEM_INPUT ||
        arg->params[1].attr != OPTEE_MSG_ATTR_TYPE_FMEM_OUTPUT) {
        printf("Invalid parameters for OPTEE_RPC_CMD_RPMB_FRAMES\n");
        arg->ret = TEE_ERROR_BAD_PARAMETERS;
        return;
    }

    void *req = optee_ffa_fmem_buffer(optee_ffa, &arg->params[0]);
    void *resp = optee_ffa_fmem_buffer(optee_ffa, &arg->params[1]);
    if (req == NULL || resp == NULL) {
        printf("Invalid frame buffers for OPTEE_RPC_CMD_RPMB_FRAMES\n");
        arg->ret = TEE_ERROR_BAD_PARAMETERS;
        return;
    }

    status_t err = rpmb_route_frames(rpmb_dev,
                                     req, (uint32_t)arg->params[0].u.fmem.size,
                                     resp, (uint32_t)arg->params[1].u.fmem.size);
    if (err != NO_ERROR) {
        printf("Failed to route RPMB frames, err=%d\n", err);
        arg->ret = TEE_ERROR_GENERIC;
        return;
    }

    arg->ret = TEE_SUCCESS;
}

static void optee_ffa_handle_rpc(struct tee_device *dev, struct optee_msg_arg *arg) {
    optee_ffa_dump_optee_msg_arg(arg);

    switch (arg->cmd) {
        case OPTEE_RPC_CMD_SHM_ALLOC:
            handle_rpc_shm_alloc(dev, arg);
            break;

        case OPTEE_RPC_CMD_SHM_FREE:
            handle_rpc_shm_free(dev, arg);
            break;

        case OPTEE_RPC_CMD_RPMB_PROBE_RESET:
            handle_rpc_rpmb_reset(dev, arg);
            break;

        case OPTEE_RPC_CMD_RPMB_PROBE_NEXT:
            handle_rpc_rpmb_next(dev, arg);
            break;

        case OPTEE_RPC_CMD_RPMB_FRAMES:
            handle_rpc_rpmb_frames(dev, arg);
            break;

        default:
            printf("Unhandled RPC command: %d\n", arg->cmd);
            arg->ret = TEE_ERROR_NOT_SUPPORTED;
            break;
    }
}

static int optee_ffa_yielding_call_with_arg(struct tee_device *dev,
                                            struct optee_ffa_call_data *data,
                                            struct optee_msg_arg *arg) {
    struct optee_ffa_info *optee_ffa = (struct optee_ffa_info*) dev->priv;

    ffa_args_t payload;
    uint32_t current_cmd = data->cmd;
    uint32_t w4 = data->w4;
    uint32_t w5 = data->w5;
    uint32_t w6 = data->w6;

    memset(&payload, 0, sizeof(ffa_args_t));

    while (true) {
        memset(&payload, 0, sizeof(ffa_args_t));
        payload.arg1 = ((uint32_t)optee_ffa->vm_id << 16) | optee_ffa->sp_id;
        payload.arg3 = current_cmd;
        payload.arg4 = w4;
        payload.arg5 = w5;
        payload.arg6 = w6;

        ffa_msg_send_direct_req_64(&payload);

        if (payload.fid != FFA_MSG_SEND_DIRECT_RESP_SMC64) {
            printf("Transport error: fid=0x%llx, arg1=0x%llx, arg2=0x%llx, arg3=0x%llx\n",
                payload.fid, payload.arg1, payload.arg2, payload.arg3);
            return TEE_ERROR_GENERIC;
        }

        if (payload.arg3 == TEE_ERROR_BUSY) {
            if (current_cmd == OPTEE_FFA_YIELDING_CALL_RESUME) {
                printf("OP-TEE FF-A: busy on resume, aborting\n");
                return TEE_ERROR_BUSY;
            }

            current_cmd = data->cmd;
            w4 = data->w4;
            w5 = data->w5;
            w6 = data->w6;
            continue;
        } else if (payload.arg3 != TEE_SUCCESS) {
            printf("OP-TEE FF-A: returned error: 0x%llx\n", payload.arg3);
            return TEE_ERROR_GENERIC;
        }

        if (payload.arg4 == OPTEE_FFA_YIELDING_CALL_RETURN_DONE) {
            break;
        }

        switch (payload.arg4) {
            case OPTEE_FFA_YIELDING_CALL_RETURN_RPC_CMD:
                printf("OPTEE_FFA_YIELDING_CALL_RETURN_RPC_CMD started\n");

                size_t rpc_arg_offs = OPTEE_MSG_GET_ARG_SIZE(arg->num_params);
                struct optee_msg_arg *rpc_arg = (struct optee_msg_arg *)((uint8_t *)arg + rpc_arg_offs);

                optee_ffa_handle_rpc(dev, rpc_arg);

                current_cmd = OPTEE_FFA_YIELDING_CALL_RESUME;
                w4 = 0;
                w5 = 0;
                w6 = 0;
                break;

            case OPTEE_FFA_YIELDING_CALL_RETURN_INTERRUPT:
                current_cmd = OPTEE_FFA_YIELDING_CALL_RESUME;
                w4 = 0;
                w5 = 0;
                w6 = 0;
                break;

            default:
                printf("FF-A: OP-TEE returned unhandled action: %lld\n", payload.arg4);
                return TEE_ERROR_COMMUNICATION;
        }
    }

    return TEE_SUCCESS;
}

static int optee_ffa_pack_params(struct optee_msg_param *msg_params, 
                                 const struct tee_operation *op) {
    if (!op)
        return TEE_SUCCESS;

    for (size_t i = 0; i < 4; i++) {
        uint32_t param_type = TEE_PARAM_TYPE_GET(op->paramTypes, i);
        struct optee_msg_param *mp = &msg_params[i];

        switch (param_type) {
            case TEE_PARAM_ATTR_TYPE_NONE:
                mp->attr = OPTEE_MSG_ATTR_TYPE_NONE;
                break;

            case TEE_PARAM_ATTR_TYPE_VALUE_INPUT:
            case TEE_PARAM_ATTR_TYPE_VALUE_OUTPUT:
            case TEE_PARAM_ATTR_TYPE_VALUE_INOUT:
                mp->attr = OPTEE_MSG_ATTR_TYPE_VALUE_INPUT + (param_type - TEE_PARAM_ATTR_TYPE_VALUE_INPUT);
                mp->u.value.a = op->params[i].value.a;
                mp->u.value.b = op->params[i].value.b;
                mp->u.value.c = 0;
                break;

            case TEE_PARAM_ATTR_TYPE_MEMREF_TEMP_INPUT:
            case TEE_PARAM_ATTR_TYPE_MEMREF_TEMP_OUTPUT:
            case TEE_PARAM_ATTR_TYPE_MEMREF_TEMP_INOUT:
                struct optee_ffa_shm *ffa_shm = (struct optee_ffa_shm *)op->params[i].tmem.shm->priv;
                
                mp->attr = OPTEE_MSG_ATTR_TYPE_FMEM_INPUT + (param_type - TEE_PARAM_ATTR_TYPE_MEMREF_TEMP_INPUT);

                if (op->params[i].tmem.shm) {
                    mp->u.fmem.internal_offs = 0;
                    mp->u.fmem.size = op->params[i].tmem.size;
                    mp->u.fmem.global_id = ffa_shm->handle;
                } else {
                    mp->u.fmem.internal_offs = 0;
                    mp->u.fmem.size = 0;
                    mp->u.fmem.global_id = 0;
                }
                break;

            default:
                printf("InvokeCommand: Param type %d unsupported\n", param_type);
                return TEE_ERROR_NOT_IMPLEMENTED;
        }
    }

    return TEE_SUCCESS;
}

static void optee_ffa_unpack_params(struct tee_operation *op, 
                                    const struct optee_msg_param *msg_params) {
    if (!op)
        return;

    for (size_t i = 0; i < 4; i++) {
        uint32_t param_type = TEE_PARAM_TYPE_GET(op->paramTypes, i);
        const struct optee_msg_param *mp = &msg_params[i];

        switch (param_type) {
            case TEE_PARAM_ATTR_TYPE_VALUE_OUTPUT:
            case TEE_PARAM_ATTR_TYPE_VALUE_INOUT:
                op->params[i].value.a = mp->u.value.a;
                op->params[i].value.b = mp->u.value.b;
                break;

            case TEE_PARAM_ATTR_TYPE_MEMREF_TEMP_OUTPUT:
            case TEE_PARAM_ATTR_TYPE_MEMREF_TEMP_INOUT:
                op->params[i].tmem.size = mp->u.tmem.size;
                break;

            default:
                break;
        }
    }
}

static int optee_ffa_open_session(struct tee_device *dev,
                                  struct tee_open_session_data *data,
                                  struct tee_session_imp **session) {
    struct tee_shm *shm = NULL;
    int err = dev->ops->shm_alloc(dev, PAGE_SIZE, &shm);
    if (err)
        return err;

    struct optee_ffa_shm *ffa_shm = (struct optee_ffa_shm *)shm->priv;

    struct optee_msg_arg *arg = (struct optee_msg_arg *)shm->addr;
    memset(arg, 0, PAGE_SIZE);

    arg->cmd = OPTEE_MSG_CMD_OPEN_SESSION;
    arg->num_params = 2;
    arg->params[0].attr = OPTEE_MSG_ATTR_TYPE_VALUE_INPUT | OPTEE_MSG_ATTR_META;
    arg->params[1].attr = OPTEE_MSG_ATTR_TYPE_VALUE_INPUT | OPTEE_MSG_ATTR_META;

    memcpy(&arg->params[0].u.value, data->uuid, sizeof(data->uuid));
    arg->params[1].u.value.c = data->class_login;

    struct optee_ffa_call_data call_data = {
        .cmd = OPTEE_FFA_YIELDING_CALL_WITH_ARG,
        .w4  = (uint32_t)ffa_shm->handle,
        .w5  = 0,
        .w6  = 0,
    };

    err = optee_ffa_yielding_call_with_arg(dev, &call_data, arg);
    if (err != TEE_SUCCESS)
        goto out;

    printf("OP-TEE Internal Result: 0x%x\n", arg->ret);
    printf("OP-TEE Internal Origin: 0x%x\n", arg->ret_origin);

    data->session_id = arg->session;
    data->ret_origin = arg->ret_origin;
    data->ret = arg->ret;

    struct tee_session_imp *s = calloc(1, sizeof(struct tee_session_imp));
    if (s == NULL) {
        err = ERR_NO_MEMORY;
        goto out;
    }

    s->session_id = data->session_id;
    s->tee_dev = dev;
    *session = s;

    err = NO_ERROR;

out:
    (void)dev->ops->shm_free(dev, shm);

    return err;
}

int optee_ffa_invoke_cmd(struct tee_device *dev, struct tee_invoke_cmd_data *data) {
    struct tee_shm *shm = NULL;
    int err = dev->ops->shm_alloc(dev, PAGE_SIZE, &shm);
    if (err)
        return err;

    struct optee_ffa_shm *ffa_shm = (struct optee_ffa_shm *)shm->priv;

    struct optee_msg_arg *arg = (struct optee_msg_arg *)shm->addr;
    memset(arg, 0, PAGE_SIZE);

    arg->cmd = OPTEE_MSG_CMD_INVOKE_COMMAND;
    arg->func = data->cmd_id;
    arg->session = data->session_id;
    arg->cancel_id = 0;

    if (data->operation != NULL) {
        arg->num_params = 4;
        optee_ffa_pack_params(arg->params, data->operation);
    }

    struct optee_ffa_call_data call_data = {
        .cmd = OPTEE_FFA_YIELDING_CALL_WITH_ARG,
        .w4 = (uint32_t)(ffa_shm->handle),
        .w5 = 0,
        .w6 = 0,
    };

    err = optee_ffa_yielding_call_with_arg(dev, &call_data, arg);
    if (err != TEE_SUCCESS)
        goto out;

    printf("OP-TEE Internal Result: 0x%x\n", arg->ret);
    printf("OP-TEE Internal Origin: 0x%x\n", arg->ret_origin);

    if (data->operation != NULL) {
        optee_ffa_unpack_params(data->operation, arg->params);
    }

    data->ret_origin = arg->ret_origin;
    data->ret = arg->ret;
    
out:
    (void)dev->ops->shm_free(dev, shm);

    return err;
}

static int optee_ffa_shm_alloc(struct tee_device *dev, size_t size, struct tee_shm **out_shm) {
    struct optee_ffa_info *optee_ffa = (struct optee_ffa_info *)dev->priv;
    struct ffa_device *ffa_dev = arm_ffa_get_device();
    int err = 0;

    struct tee_shm *shm = calloc(1, sizeof(struct tee_shm));
    if (shm == NULL)
        return TEE_ERROR_OUT_OF_MEMORY;

    void *buffer = NULL;
    ffa_mem_handle_t handle;
    err = arm_ffa_shm_alloc(ffa_dev, optee_ffa->sp_id, &handle, size, &buffer);
    if (err != NO_ERROR)
        goto out_free_shm;

    struct optee_ffa_shm *ffa_shm = calloc(1, sizeof(struct optee_ffa_shm));
    if (ffa_shm == NULL) {
        err = TEE_ERROR_OUT_OF_MEMORY;
        goto out_free_ffa_shm;
    }

    ffa_shm->handle = handle;
    ffa_shm->shm = shm;

    list_add_tail(&optee_ffa->shm_list, &ffa_shm->node);

    shm->addr = (uintptr_t)buffer;
    shm->priv = ffa_shm;
    shm->size = size;
    *out_shm = shm;
    
    return TEE_SUCCESS;

out_free_ffa_shm:
    arm_ffa_shm_free(ffa_dev, handle);

out_free_shm:
    free(shm);

    return err;
}

static int optee_ffa_shm_free(struct tee_device *dev, struct tee_shm *shm) {
    if (dev == NULL || shm == NULL)
        return TEE_ERROR_BAD_PARAMETERS;

    if (shm->priv == NULL)
        return TEE_ERROR_GENERIC;

    struct ffa_device *ffa_dev = arm_ffa_get_device();
    struct optee_ffa_shm *ffa_shm = (struct optee_ffa_shm *)shm->priv;

    if (list_in_list(&ffa_shm->node))
        list_delete(&ffa_shm->node);

    status_t err = arm_ffa_shm_free(ffa_dev, ffa_shm->handle);
    if (err)
        return TEE_ERROR_GENERIC;

    free(ffa_shm);
    free(shm);

    return TEE_SUCCESS;
}

static const struct tee_driver_ops optee_ffa_ops = {
    .init_context = optee_ffa_init_context,
    .open_session = optee_ffa_open_session,
    .invoke_cmd = optee_ffa_invoke_cmd,
    .shm_alloc = optee_ffa_shm_alloc,
    .shm_free = optee_ffa_shm_free,
};

static struct optee_ffa_info optee_ffa_priv;

static struct tee_device optee_ffa_dev = {
    .name = "optee_ffa",
    .ops = &optee_ffa_ops,
    .priv = &optee_ffa_priv,
};

static void optee_ffa_init_hook(uint level) {
    printf("OP-TEE FF-A: Initializing at level 0x%x\n", level);

    status_t err = optee_ffa_init(&optee_ffa_dev);
    if (err != NO_ERROR) {
        panic("OP-TEE FF-A failed to initialize, err=%d\n", err);
        return;
    }

    err = tee_dev_register(&optee_ffa_dev);
    if (err != NO_ERROR) {
        panic("OP-TEE FF-A failed to register, err=%d\n", err);
        return;
    }

    printf("OP-TEE FF-A: successfully initialized\n");
}

LK_INIT_HOOK(optee_ffa, &optee_ffa_init_hook, LK_INIT_LEVEL_PLATFORM + 1);
