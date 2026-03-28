#include <tee_client_api.h>

#include <arch/defines.h>
#include <arm_ffa.h>
#include <arm_ffa_mem.h>
#include <string.h>
#include <stdlib.h>
#include <kernel/vm.h>

#include "tee_log.h"

#include <optee/optee_msg.h>
#include <optee_ffa.h>

uint8_t tx_buff[4096] __attribute__((aligned(4096)));
uint8_t rx_buff[4096] __attribute__((aligned(4096)));

TEEC_Result TEEC_InitializeContext(
    const char *name,
    TEEC_Context *context)
{
    TEEC_Context_Imp *ctx = (TEEC_Context_Imp *)context;

    ffa_args_t payload;

    payload.arg1 = (FFA_VERSION_MAJOR << 16) | FFA_VERSION_MINOR;
    ffa_version(&payload);

    if (payload.fid == FFA_ERROR_NOT_SUPPORTED || payload.fid == FFA_ERROR_SMC32) {
        TEE_LOG("ffa_version failed, FF-A Error %llx\n", payload.fid);
        return TEEC_ERROR_GENERIC;
    }
    TEE_LOG("FF-A version %llx\n", payload.fid);

    ffa_endpoint_id_t client_id = ffa_get_curr_endpoint_id();
    ctx->client_id = client_id;
    TEE_LOG("FF-A Current Endpoint ID %llx\n", client_id);

    memset(&payload, 0, sizeof(ffa_args_t));
    if (rxtx_map_64((uint64_t)&tx_buff[0], (uint64_t)&rx_buff[0], 1)) {
        return TEEC_ERROR_GENERIC;
    }

    memset(&payload, 0, sizeof(ffa_args_t));
    ffa_rx_release(&payload);

    const uint32_t null_uuid[4] = {0};
    payload.arg1 = null_uuid[0];
    payload.arg2 = null_uuid[1];
    payload.arg3 = null_uuid[2];
    payload.arg4 = null_uuid[3];
    payload.arg5 = FFA_PARTITION_INFO_FLAG_RETDESC;
    ffa_partition_info_get(&payload);

    /* Print all rx descriptors */
    uint64_t desc_count = (uint64_t)payload.arg2;
    ffa_partition_info_t *ret_info = (ffa_partition_info_t *)rx_buff;
    TEE_LOG("Partition Descriptor count: %d\n", (int)payload.arg2);
    
    for (int i = 0; i < desc_count; i++) {
        ffa_partition_info_t *part = &ret_info[i];

        TEE_LOG("+---------------- Partition Descriptor [%d] ----------------+\n", i);
        TEE_LOG("| ID            : 0x%08x                               |\n", part->id);
        TEE_LOG("| Exec Contexts : %-36u     |\n", part->exec_context);
        TEE_LOG("| Properties    : 0x%08x                               |\n", part->properties);
        TEE_LOG("| UUID          : %08x-%08x-%08x-%08x      |\n",
                 part->uuid[0], part->uuid[1], part->uuid[2], part->uuid[3]);
        TEE_LOG("+----------------------------------------------------------+\n");

        context->imp.tee_id = part->id;
    }

    if (payload.arg3 != sizeof(ffa_partition_info_t)) {
        TEE_LOG("Expected desc size %zu, got %lx\n", sizeof(ffa_partition_info_t), payload.arg2);
        ffa_rx_release(&payload);
        return TEEC_ERROR_GENERIC;
    }

    memset(&payload, 0, sizeof(ffa_args_t));
    payload.arg1 = ((uint32_t)context->imp.client_id << 16) | context->imp.tee_id;
    payload.arg2 = 0;
    payload.arg3 = OPTEE_FFA_EXCHANGE_CAPABILITIES;

    ffa_msg_send_direct_req_64(&payload);

    TEE_LOG("ffa_msg_send_direct_req fid=0x%lx, arg1=0x%lx, arg2=0x%lx\n", payload.fid, payload.arg1, payload.arg2);

    if (payload.fid != FFA_MSG_SEND_DIRECT_RESP_SMC64 || payload.arg3 != 0) {
        return TEEC_ERROR_COMMUNICATION;
    }

    memset(&payload, 0, sizeof(ffa_args_t));
    ffa_rxtx_unmap(&payload);

    memset(&payload, 0, sizeof(ffa_args_t));
    ffa_rx_release(&payload);

    return TEEC_SUCCESS;
}

uint8_t *pages = NULL;

TEEC_Result TEEC_OpenSession(
    TEEC_Context    *context,
    TEEC_Session    *session,
    const TEEC_UUID *destination,
    uint32_t        connectionMethod,
    void            *connectionData,
    TEEC_Operation  *operation,
    uint32_t        *returnOrigin) 
{
    mb_buf_t mb;
    uint64_t size = 0x1000;
    ffa_args_t payload;
    ffa_mem_region_flags_t flags = 0;
    ffa_mem_handle_t handle;
    struct ffa_mem_region_init mem_region_init;
    struct ffa_mem_region_constituent constituents[1];
    const uint32_t constituents_count = sizeof(constituents) / sizeof(struct ffa_mem_region_constituent);

    mb.recv = memalign(PAGE_SIZE, size);
    mb.send = memalign(PAGE_SIZE, size);

    if (mb.recv == NULL || mb.send == NULL) {
        TEE_LOG("Failed to allocate RxTx buffer\n");
        return TEEC_ERROR_OUT_OF_MEMORY;
    }

    uint32_t page_count = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    if (rxtx_map_64((uint64_t)mb.send, (uint64_t)mb.recv, page_count)){
        TEE_LOG("RxTx map failed\n");
        return TEEC_ERROR_OUT_OF_MEMORY;
    }

    pages = (uint8_t *)memalign(PAGE_SIZE, size);
    if (pages == NULL) {
        TEE_LOG("pages alloc failed\n");
    }

    constituents[0].address = (void *)vaddr_to_paddr((void *)pages);
    constituents[0].page_count = 1;

    memset(&mem_region_init, 0x0, sizeof(struct ffa_mem_region_init));

    mem_region_init.memory_region = mb.send;
    mem_region_init.sender = context->imp.client_id;
    mem_region_init.receiver = context->imp.tee_id;
    mem_region_init.tag = 0;
    mem_region_init.flags = flags;
    mem_region_init.data_access = FFA_DATA_ACCESS_RW;
    mem_region_init.instruction_access = FFA_INSTRUCTION_ACCESS_NOT_SPECIFIED;
    mem_region_init.type = FFA_MEM_NORMAL_MEM;
    mem_region_init.cacheability = FFA_MEM_CACHE_WRITE_BACK;

    mem_region_init.multi_share = false;
    mem_region_init.receiver_count = 1;
    mem_region_init.receivers[0].receiver_permissions.receiver = context->imp.tee_id;
    mem_region_init.receivers[0].receiver_permissions.permissions = FFA_DATA_ACCESS_RW;
    mem_region_init.receivers[0].receiver_permissions.flags = 0;
    mem_region_init.receivers[0].impdef.val[0] = 0;
    mem_region_init.receivers[0].impdef.val[1] = 0;

    ffa_memory_region_init(&mem_region_init, constituents, constituents_count);

    memset(&payload, 0, sizeof(ffa_args_t));
    payload.arg1 = mem_region_init.total_length;
    payload.arg2 = mem_region_init.fragment_length;

    TEE_LOG("FF-A mem share arg1=0x%lx arg2=0x%lx\n", payload.arg1, payload.arg2);

    ffa_mem_share_64(&payload);

    if (payload.fid == FFA_ERROR_SMC32) {
        TEE_LOG("FF-A Mem Share request failed err %x\n", payload.arg2);
        return TEEC_ERROR_GENERIC;
    }

    handle = payload.arg2;

    TEE_LOG("ffa_mem_share_64 0x%llx\n", payload.arg2);

    struct optee_msg_arg *arg = (struct optee_msg_arg *)pages;
    memset(arg, 0, PAGE_SIZE);
    arg->cmd = OPTEE_MSG_CMD_OPEN_SESSION;
    arg->num_params = 2;

    arg->params[0].attr = OPTEE_MSG_ATTR_TYPE_VALUE_INPUT | OPTEE_MSG_ATTR_META;
    arg->params[1].attr = OPTEE_MSG_ATTR_TYPE_VALUE_INPUT | OPTEE_MSG_ATTR_META;
    
    memcpy(&arg->params[0].u.value, destination, sizeof(TEEC_UUID));
    arg->params[1].u.value.c = connectionMethod;

    uint32_t cmd = OPTEE_FFA_YIELDING_CALL_WITH_ARG;
    uint32_t w4 = handle;
    uint32_t w5 = 0;
    uint32_t w6 = 0;

    while (true) {
        memset(&payload, 0, sizeof(ffa_args_t));
        payload.arg1 = ((uint32_t)context->imp.client_id << 16) | context->imp.tee_id;
        payload.arg3 = cmd;
        payload.arg4 = w4;
        payload.arg5 = w5;
        payload.arg6 = w6;

        ffa_msg_send_direct_req_64(&payload);

        if (payload.fid != FFA_MSG_SEND_DIRECT_RESP_SMC64) {
            TEE_LOG("Transport error: fid=0x%lx, arg1=0x%lx, arg2=0x%lx, arg3=0x%lx\n",
                payload.fid, payload.arg1, payload.arg2, payload.arg3);
            return TEEC_ERROR_COMMUNICATION;
        }

        if (payload.arg3 == TEEC_ERROR_BUSY) {
            if (cmd == OPTEE_FFA_YIELDING_CALL_RESUME) {
                TEE_LOG("OP-TEE busy on resume, aborting\n");
                return TEEC_ERROR_BUSY;
            }
            continue;
        } else if (payload.arg3 != TEEC_SUCCESS) {
            TEE_LOG("OP-TEE returned error: 0x%lx\n", payload.arg3);
            return TEEC_ERROR_GENERIC;
        }

        if (payload.arg4 == OPTEE_FFA_YIELDING_CALL_RETURN_DONE) { 
            break; 
        } else if (payload.arg4 == OPTEE_FFA_YIELDING_CALL_RETURN_RPC_CMD) { 
            TEE_LOG("OPTEE_FFA_YIELDING_CALL_RETURN_RPC_CMD is unsupported\n");
            return TEEC_ERROR_COMMUNICATION;
        } else if (payload.arg4 == OPTEE_FFA_YIELDING_CALL_RETURN_INTERRUPT) {
            TEE_LOG("OPTEE_FFA_YIELDING_CALL_RETURN_INTERRUPT, resumed\n");
            cmd = OPTEE_FFA_YIELDING_CALL_RESUME;
            w4 = 0;
            w5 = 0;
            w6 = 0;
        } else {
            TEE_LOG("OP-TEE unsupported yielding call return %d\n", payload.arg4);
        }
    }

    struct optee_msg_arg *arg_res = (struct optee_msg_arg *)pages;
    TEE_LOG("OP-TEE Internal Result: 0x%x\n", arg_res->ret);
    TEE_LOG("OP-TEE Internal Origin: 0x%x\n", arg_res->ret_origin);

    if (arg_res->ret == 0) {
        session->imp.session_id = arg_res->session;
        session->imp.client_id = context->imp.client_id;
        session->imp.tee_id = context->imp.tee_id;
        session->imp.handle = handle;
        session->imp.constituent = pages;
        TEE_LOG("Session successfully opened. ID: 0x%x\n", arg_res->session);
    }

    return arg_res->ret;
}

TEEC_Result TEEC_InvokeCommand(
    TEEC_Session     *session,
    uint32_t         commandID,
    TEEC_Operation   *operation,
    uint32_t         *returnOrigin)
{
    struct optee_msg_arg *arg = (struct optee_msg_arg *)session->imp.constituent;
    memset(arg, 0, PAGE_SIZE);
    arg->cmd = OPTEE_MSG_CMD_INVOKE_COMMAND;
    arg->func = commandID;
    arg->session = session->imp.session_id;
    arg->cancel_id = 0;

    if (operation != NULL) {
        arg->num_params = TEEC_CONFIG_PAYLOAD_REF_COUNT;
        for (size_t i = 0; i < TEEC_CONFIG_PAYLOAD_REF_COUNT; i++) {
            uint32_t param_type = TEEC_PARAM_TYPE_GET(operation->paramTypes, i);

            switch (param_type) {
                case TEEC_VALUE_INPUT:
                case TEEC_VALUE_OUTPUT:
                case TEEC_VALUE_INOUT:
                    arg->params[i].attr = OPTEE_MSG_ATTR_TYPE_VALUE_INPUT + (param_type - TEEC_VALUE_INPUT);
                    arg->params[i].u.value.a = operation->params[i].value.a;
                    arg->params[i].u.value.b = operation->params[i].value.b;
                    break;
                case TEEC_NONE:
                    arg->params[i].attr = OPTEE_MSG_ATTR_TYPE_NONE;
                    break;
                default:
                    TEE_LOG("InvokeCommand: Param type %d unsupported\n", param_type);
                    return TEEC_ERROR_NOT_IMPLEMENTED;
            }
        }
    }

    ffa_args_t payload;

    uint32_t cmd = OPTEE_FFA_YIELDING_CALL_WITH_ARG;
    uint32_t w4 = (uint32_t)(session->imp.handle);
    uint32_t w5 = 0;
    uint32_t w6 = 0;

    while (true) {
        memset(&payload, 0, sizeof(ffa_args_t));
        payload.arg1 = ((uint32_t)session->imp.client_id << 16) | session->imp.tee_id;
        payload.arg3 = cmd;
        payload.arg4 = w4;
        payload.arg5 = w5;
        payload.arg6 = w6;

        ffa_msg_send_direct_req_64(&payload);

        if (payload.fid != FFA_MSG_SEND_DIRECT_RESP_SMC64) {
            TEE_LOG("Transport error: fid=0x%lx, arg1=0x%lx, arg2=0x%lx, arg3=0x%lx\n",
                payload.fid, payload.arg1, payload.arg2, payload.arg3);
            return TEEC_ERROR_COMMUNICATION;
        }

        if (payload.arg3 == TEEC_ERROR_BUSY) {
            if (cmd == OPTEE_FFA_YIELDING_CALL_RESUME) {
                TEE_LOG("OP-TEE busy on resume, aborting\n");
                return TEEC_ERROR_BUSY;
            }
            continue;
        } else if (payload.arg3 != TEEC_SUCCESS) {
            TEE_LOG("OP-TEE returned error: 0x%lx\n", payload.arg3);
            return TEEC_ERROR_GENERIC;
        }

        if (payload.arg4 == OPTEE_FFA_YIELDING_CALL_RETURN_DONE) { 
            break;
        } else if (payload.arg4 == OPTEE_FFA_YIELDING_CALL_RETURN_RPC_CMD) { 
            TEE_LOG("OPTEE_FFA_YIELDING_CALL_RETURN_RPC_CMD is unsupported\n");
            return TEEC_ERROR_COMMUNICATION;
        } else if (payload.arg4 == OPTEE_FFA_YIELDING_CALL_RETURN_INTERRUPT) {
            TEE_LOG("OPTEE_FFA_YIELDING_CALL_RETURN_INTERRUPT, resumed\n");
            cmd = OPTEE_FFA_YIELDING_CALL_RESUME;
            w4 = 0;
            w5 = 0;
            w6 = 0;
        } else {
            TEE_LOG("OP-TEE unsupported yielding call return %d\n", payload.arg4);
        }
    }

    struct optee_msg_arg *arg_res = (struct optee_msg_arg *)session->imp.constituent;

    TEE_LOG("OP-TEE Internal Result: 0x%x\n", arg_res->ret);
    TEE_LOG("OP-TEE Internal Origin: 0x%x\n", arg_res->ret_origin);

    for (int i = 0; i < 4; i++) {
        TEE_LOG("OP-TEE Opetation params[%d].value.a: 0x%x\n", i, arg_res->params[i].u.value.a);
        TEE_LOG("OP-TEE Opetation params[%d].value.b: 0x%x\n", i, arg_res->params[i].u.value.b);
    }

    if (operation != NULL) {
        for (size_t i = 0; i < TEEC_CONFIG_PAYLOAD_REF_COUNT; i++) {
            uint32_t param_type = TEEC_PARAM_TYPE_GET(operation->paramTypes, i);
            if (param_type == TEEC_VALUE_OUTPUT || param_type == TEEC_VALUE_INOUT) {
                operation->params[i].value.a = arg_res->params[i].u.value.a;
                operation->params[i].value.b = arg_res->params[i].u.value.b;
            }
        }
    }

    if (arg_res->ret == 0) {
        TEE_LOG("Invoke Command successfully finished.\n");
    }

    return arg_res->ret;
}

void TEEC_CloseSession(TEEC_Session *session)
{

}

void TEEC_FinalizeContext(TEEC_Context *context)
{

}