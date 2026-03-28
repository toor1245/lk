#include <arm_ffa.h>
#include <arm_ffa_mem.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <kernel/vm.h>

static ffa_args_t ffa_smccc(uint64_t fid, uint64_t arg1, uint64_t arg2,
                            uint64_t arg3, uint64_t arg4, uint64_t arg5,
                            uint64_t arg6, uint64_t arg7, uint64_t arg8,
                            uint64_t arg9, uint64_t arg10, uint64_t arg11,
                            uint64_t arg12, uint64_t arg13, uint64_t arg14,
                            uint64_t arg15, uint64_t arg16, uint64_t arg17) {
    ffa_args_t args;

    args.fid = fid;
    args.arg1 = arg1;
    args.arg2 = arg2;
    args.arg3 = arg3;
    args.arg4 = arg4;
    args.arg5 = arg5;
    args.arg6 = arg6;
    args.arg7 = arg7;
    args.ext_args.arg8 = arg8;
    args.ext_args.arg9 = arg9;
    args.ext_args.arg10 = arg10;
    args.ext_args.arg11 = arg11;
    args.ext_args.arg12 = arg12;
    args.ext_args.arg13 = arg13;
    args.ext_args.arg14 = arg14;
    args.ext_args.arg15 = arg15;
    args.ext_args.arg16 = arg16;
    args.ext_args.arg17 = arg17;

    call_conduit(&args);

    return args;
}

void ffa_error(ffa_args_t *args) {
    *args = ffa_smccc(FFA_ERROR_SMC32, args->arg1, args->arg2, args->arg3,
                      args->arg4, args->arg5, args->arg6, args->arg7,
                      0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void ffa_success_smc32(ffa_args_t *args) {
    *args = ffa_smccc(FFA_SUCCESS_SMC32, (uint32_t)args->arg1,
                      (uint32_t)args->arg2, (uint32_t)args->arg3,
                      (uint32_t)args->arg4, (uint32_t)args->arg5,
                      (uint32_t)args->arg6, (uint32_t)args->arg7,
                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void ffa_success_smc64(ffa_args_t *args) {
    *args = ffa_smccc(FFA_SUCCESS_SMC64, args->arg1, args->arg2, args->arg3,
                      args->arg4, args->arg5, args->arg6, args->arg7,
                      0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void ffa_version(ffa_args_t *args) {
    *args = ffa_smccc(FFA_VERSION_SMC32, (uint32_t)args->arg1,
                      (uint32_t)args->arg2, (uint32_t)args->arg3,
                      (uint32_t)args->arg4, (uint32_t)args->arg5,
                      (uint32_t)args->arg6, (uint32_t)args->arg7,
                      0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void ffa_msg_send_direct_req2(ffa_args_t *args) {
    *args = ffa_smccc(FFA_MSG_SEND_DIRECT_REQ2_SMC64,
                      args->arg1, args->arg2,
                      args->arg3, args->arg4,
                      args->arg5, args->arg6,
                      args->arg7, args->ext_args.arg8,
                      args->ext_args.arg9, args->ext_args.arg10,
                      args->ext_args.arg11, args->ext_args.arg12,
                      args->ext_args.arg13, args->ext_args.arg14,
                      args->ext_args.arg15, args->ext_args.arg16,
                      args->ext_args.arg17);
}

void ffa_id_get(ffa_args_t *args) {
    *args = ffa_smccc(FFA_ID_GET_SMC32, (uint32_t)args->arg1,
                      (uint32_t)args->arg2, (uint32_t)args->arg3,
                      (uint32_t)args->arg4, (uint32_t)args->arg5,
                      (uint32_t)args->arg6, (uint32_t)args->arg7,
                      0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

static void ffa_msg_send_direct_req(ffa_args_t *args, bool smc64) {
    if (smc64) {
        *args = ffa_smccc(FFA_MSG_SEND_DIRECT_REQ_SMC64, args->arg1, args->arg2,
                          args->arg3, args->arg4, args->arg5, args->arg6,
                          args->arg7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    } else {
        *args = ffa_smccc(FFA_MSG_SEND_DIRECT_REQ_SMC32, args->arg1, args->arg2,
                          args->arg3, args->arg4, args->arg5, args->arg6,
                          args->arg7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }
}

void ffa_msg_send_direct_req_32(ffa_args_t *args) {
    ffa_msg_send_direct_req(args, false);
}

void ffa_msg_send_direct_req_64(ffa_args_t *args) {
    ffa_msg_send_direct_req(args, true);
}

static void ffa_msg_send_direct_resp(ffa_args_t *args, bool smc64) {
    if (smc64) {
        *args = ffa_smccc(FFA_MSG_SEND_DIRECT_RESP_SMC64, args->arg1, args->arg2,
                          args->arg3, args->arg4, args->arg5, args->arg6,
                          args->arg7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    } else {
        *args = ffa_smccc(FFA_MSG_SEND_DIRECT_RESP_SMC32, args->arg1, args->arg2,
                          args->arg3, args->arg4, args->arg5, args->arg6,
                          args->arg7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }
}

static void ffa_msg_send_direct_resp2(ffa_args_t *args) {
    *args = ffa_smccc(FFA_MSG_SEND_DIRECT_RESP2_SMC64, args->arg1, args->arg2,
                        args->arg3, args->arg4, args->arg5, args->arg6,
                        args->arg7, args->ext_args.arg8, args->ext_args.arg9, args->ext_args.arg10,
                        args->ext_args.arg11, args->ext_args.arg12, args->ext_args.arg13,
                        args->ext_args.arg14, args->ext_args.arg15, args->ext_args.arg16,
                        args->ext_args.arg17);
}

void ffa_msg_send_direct_resp2_64(ffa_args_t *args) {
    ffa_msg_send_direct_resp2(args);
}

void ffa_msg_send_direct_resp_32(ffa_args_t *args) {
    ffa_msg_send_direct_resp(args, false);
}

void ffa_msg_send_direct_resp_64(ffa_args_t *args) {
    ffa_msg_send_direct_resp(args, true);
}

void ffa_spm_id_get(ffa_args_t *args) {
    *args = ffa_smccc(FFA_SPM_ID_GET_SMC32, (uint32_t)args->arg1,
                          (uint32_t)args->arg2, (uint32_t)args->arg3,
                          (uint32_t)args->arg4, (uint32_t)args->arg5,
                          (uint32_t)args->arg6, (uint32_t)args->arg7,
                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

ffa_endpoint_id_t ffa_get_curr_endpoint_id(void) {
    ffa_args_t ret;
    ffa_endpoint_id_t id;

    ret = ffa_smccc(FFA_ID_GET_SMC32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    if (ret.fid == FFA_ERROR_SMC32) {
        printf("Error: FFA_ID_GET_32 failed");
    }

    id = ret.arg2 & 0xffff;
    return id;
}

void ffa_rx_release(ffa_args_t *args) {
    *args = ffa_smccc(FFA_RX_RELEASE_SMC32, (uint32_t)args->arg1,
                          (uint32_t)args->arg2, (uint32_t)args->arg3,
                          (uint32_t)args->arg4, (uint32_t)args->arg5,
                          (uint32_t)args->arg6, (uint32_t)args->arg7,
                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}


void ffa_rxtx_unmap(ffa_args_t *args) {
    *args = ffa_smccc(FFA_RXTX_UNMAP_SMC32, (uint32_t)args->arg1,
                          (uint32_t)args->arg2, (uint32_t)args->arg3,
                          (uint32_t)args->arg4, (uint32_t)args->arg5,
                          (uint32_t)args->arg6, (uint32_t)args->arg7,
                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

static void ffa_rxtx_map(ffa_args_t *args, bool smc64) {
    if (smc64) {
        *args = ffa_smccc(FFA_RXTX_MAP_SMC64, args->arg1, args->arg2, args->arg3,
                                args->arg4, args->arg5, args->arg6, args->arg7,
                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    } else {
        *args = ffa_smccc(FFA_RXTX_MAP_SMC32, (uint32_t)args->arg1,
                          (uint32_t)args->arg2, (uint32_t)args->arg3,
                          (uint32_t)args->arg4, (uint32_t)args->arg5,
                          (uint32_t)args->arg6, (uint32_t)args->arg7,
                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }
}

void ffa_rxtx_map_32(ffa_args_t *args) {
    ffa_rxtx_map(args, false);
}

void ffa_rxtx_map_64(ffa_args_t *args) {
    ffa_rxtx_map(args, true);
}

void ffa_msg_send(ffa_args_t *args) {
    *args = ffa_smccc(FFA_MSG_SEND_SMC32, (uint32_t)args->arg1,
                          (uint32_t)args->arg2, (uint32_t)args->arg3,
                          (uint32_t)args->arg4, (uint32_t)args->arg5,
                          (uint32_t)args->arg6, (uint32_t)args->arg7,
                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void ffa_msg_send2(ffa_args_t *args) {
    *args = ffa_smccc(FFA_MSG_SEND2_SMC32, (uint32_t)args->arg1,
                          (uint32_t)args->arg2, (uint32_t)args->arg3,
                          (uint32_t)args->arg4, (uint32_t)args->arg5,
                          (uint32_t)args->arg6, (uint32_t)args->arg7,
                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void ffa_partition_info_get_regs(ffa_args_t *args) {
    *args = ffa_smccc(FFA_PARTITION_INFO_GET_REGS_SMC64, args->arg1, args->arg2,
                        args->arg3, args->arg4, args->arg5, args->arg6,
                        args->arg7, args->ext_args.arg8, args->ext_args.arg9, args->ext_args.arg10,
                        args->ext_args.arg11, args->ext_args.arg12, args->ext_args.arg13,
                        args->ext_args.arg14, args->ext_args.arg15, args->ext_args.arg16,
                        args->ext_args.arg17);
}

void ffa_partition_info_get(ffa_args_t *args) {
    *args = ffa_smccc(FFA_PARTITION_INFO_GET_SMC32, (uint32_t)args->arg1,
                          (uint32_t)args->arg2, (uint32_t)args->arg3,
                          (uint32_t)args->arg4, (uint32_t)args->arg5,
                          (uint32_t)args->arg6, (uint32_t)args->arg7,
                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void ffa_features(ffa_args_t *args) {
    *args = ffa_smccc(FFA_FEATURES_SMC32, (uint32_t)args->arg1,
                          (uint32_t)args->arg2, (uint32_t)args->arg3,
                          (uint32_t)args->arg4, (uint32_t)args->arg5,
                          (uint32_t)args->arg6, (uint32_t)args->arg7,
                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void ffa_mem_reclaim(ffa_args_t *args) {
    *args = ffa_smccc(FFA_MEM_RECLAIM_SMC32, (uint32_t)args->arg1,
                          (uint32_t)args->arg2, (uint32_t)args->arg3,
                          (uint32_t)args->arg4, (uint32_t)args->arg5,
                          (uint32_t)args->arg6, (uint32_t)args->arg7,
                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void ffa_msg_wait(ffa_args_t *args) {
    *args = ffa_smccc(FFA_MSG_WAIT_SMC32, (uint32_t)args->arg1,
                          (uint32_t)args->arg2, (uint32_t)args->arg3,
                          (uint32_t)args->arg4, (uint32_t)args->arg5,
                          (uint32_t)args->arg6, (uint32_t)args->arg7,
                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void ffa_yield(ffa_args_t *args) {
    *args = ffa_smccc(FFA_YIELD_SMC32, (uint32_t)args->arg1,
                          (uint32_t)args->arg2, (uint32_t)args->arg3,
                          (uint32_t)args->arg4, (uint32_t)args->arg5,
                          (uint32_t)args->arg6, (uint32_t)args->arg7,
                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void ffa_run(ffa_args_t *args) {
    *args = ffa_smccc(FFA_RUN_SMC32, (uint32_t)args->arg1,
                          (uint32_t)args->arg2, (uint32_t)args->arg3,
                          (uint32_t)args->arg4, (uint32_t)args->arg5,
                          (uint32_t)args->arg6, (uint32_t)args->arg7,
                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void ffa_msg_poll(ffa_args_t *args) {
    *args = ffa_smccc(FFA_MSG_POLL_SMC32, (uint32_t)args->arg1,
                          (uint32_t)args->arg2, (uint32_t)args->arg3,
                          (uint32_t)args->arg4, (uint32_t)args->arg5,
                          (uint32_t)args->arg6, (uint32_t)args->arg7,
                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

static void ffa_mem_donate(ffa_args_t *args, bool smc64) {
    if (smc64) {
        *args = ffa_smccc(FFA_MEM_DONATE_SMC64, args->arg1, args->arg2, args->arg3,
                                args->arg4, args->arg5, args->arg6, args->arg7,
                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }
    else {
        *args = ffa_smccc(FFA_MEM_DONATE_SMC32, (uint32_t)args->arg1,
                          (uint32_t)args->arg2, (uint32_t)args->arg3,
                          (uint32_t)args->arg4, (uint32_t)args->arg5,
                          (uint32_t)args->arg6, (uint32_t)args->arg7,
                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }
}

void ffa_mem_donate_32(ffa_args_t *args) {
    ffa_mem_donate(args, false);
}

void ffa_mem_donate_64(ffa_args_t *args) {
    ffa_mem_donate(args, true);
}

static void ffa_mem_lend(ffa_args_t *args, bool smc64) {
    if (smc64) {
        *args = ffa_smccc(FFA_MEM_LEND_SMC64, args->arg1, args->arg2, args->arg3,
                                args->arg4, args->arg5, args->arg6, args->arg7,
                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }
    else {
        *args = ffa_smccc(FFA_MEM_LEND_SMC32, (uint32_t)args->arg1,
                          (uint32_t)args->arg2, (uint32_t)args->arg3,
                          (uint32_t)args->arg4, (uint32_t)args->arg5,
                          (uint32_t)args->arg6, (uint32_t)args->arg7,
                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }
}

void ffa_mem_lend_32(ffa_args_t *args) {
    ffa_mem_lend(args, false);
}

void ffa_mem_lend_64(ffa_args_t *args) {
    ffa_mem_lend(args, true);
}

static void ffa_mem_share(ffa_args_t *args, bool smc64)
{
    if (smc64) {
        *args = ffa_smccc(FFA_MEM_SHARE_SMC64, args->arg1, args->arg2, args->arg3,
                                args->arg4, args->arg5, args->arg6, args->arg7,
                                0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }
    else {
        *args = ffa_smccc(FFA_MEM_SHARE_SMC32, (uint32_t)args->arg1,
                          (uint32_t)args->arg2, (uint32_t)args->arg3,
                          (uint32_t)args->arg4, (uint32_t)args->arg5,
                          (uint32_t)args->arg6, (uint32_t)args->arg7,
                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }
}

void ffa_mem_share_32(ffa_args_t *args) {
    ffa_mem_share(args, false);
}

void ffa_mem_share_64(ffa_args_t *args) {
    ffa_mem_share(args, true);
}

static void ffa_mem_retrieve(ffa_args_t *args, bool smc64) {
    if (smc64) {
        *args = ffa_smccc(FFA_MEM_RETRIEVE_REQ_SMC64, args->arg1, args->arg2,
                          args->arg3, args->arg4, args->arg5, args->arg6,
                          args->arg7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }
    else {
        *args = ffa_smccc(FFA_MEM_RETRIEVE_REQ_SMC32, (uint32_t)args->arg1,
                          (uint32_t)args->arg2, (uint32_t)args->arg3,
                          (uint32_t)args->arg4, (uint32_t)args->arg5,
                          (uint32_t)args->arg6, (uint32_t)args->arg7,
                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }
}

void ffa_mem_retrieve_32(ffa_args_t *args) {
    ffa_mem_retrieve(args, false);
}

void ffa_mem_retrieve_64(ffa_args_t *args) {
    ffa_mem_retrieve(args, true);
}

uint32_t rxtx_map_64(uint64_t tx_buf, uint64_t rx_buf, uint32_t page_count) {
    ffa_args_t payload;

    memset(&payload, 0, sizeof(ffa_args_t));
    payload.arg1 = (uint64_t)vaddr_to_paddr((void *)tx_buf);
    payload.arg2 = (uint64_t)vaddr_to_paddr((void *)rx_buf);
    payload.arg3 = page_count;

    ffa_rxtx_map_64(&payload);

    if (payload.fid == FFA_ERROR_SMC32) {
        printf("RXTX_MAP failed err 0x%llx\n", payload.arg2);
        return 1;
    }

    return 0;
}

uint32_t rxtx_map_32(uint64_t tx_buf, uint64_t rx_buf, uint32_t page_count) {
    ffa_args_t payload;

    memset(&payload, 0, sizeof(ffa_args_t));
    payload.arg1 = (uint64_t)vaddr_to_paddr((void *)tx_buf);
    payload.arg2 = (uint64_t)vaddr_to_paddr((void *)rx_buf);
    payload.arg3 = page_count;

    ffa_rxtx_map_32(&payload);

    if (payload.fid == FFA_ERROR_SMC32) {
        printf("RXTX_MAP failed err 0x%llx\n", payload.arg2);
        return 1;
    }

    return 0;
}

uint32_t rxtx_unmap(ffa_endpoint_id_t id) {
    (void)id;

    ffa_args_t payload;

    memset(&payload, 0, sizeof(ffa_args_t));
    ffa_rxtx_unmap(&payload);

    if (payload.fid == FFA_ERROR_SMC32) {
        printf("RXTX_UNMAP failed err 0x%llx\n", payload.arg2);
        return 1;
    }
    return 0;
}

void ffa_mem_region_init_header(struct ffa_mem_region_init *mem_region_init,
                    ffa_mem_attributes_t attributes,
                    ffa_mem_handle_t handle,
                    ffa_mem_access_permissions_t permissions) {
    struct ffa_mem_region *memory_region = mem_region_init->memory_region;

    memory_region->sender = mem_region_init->sender;
    memory_region->attributes = attributes;

    memory_region->reserved[0] = 0;
    memory_region->reserved[1] = 0;
    memory_region->reserved[2] = 0;
    memory_region->receivers_offset = EP_MEM_ACCESS_DESC_ARR_OFFSET;
    memory_region->memory_access_desc_size = sizeof(struct ffa_mem_access);

    memory_region->flags = mem_region_init->flags;
    memory_region->handle = handle;
    memory_region->tag = mem_region_init->tag;

    if (!mem_region_init->multi_share) {
        memory_region->receiver_count = 1;
        memory_region->receivers[0].receiver_permissions.receiver = mem_region_init->receiver;
        memory_region->receivers[0].receiver_permissions.permissions = permissions;
        memory_region->receivers[0].receiver_permissions.flags = 0;
        memory_region->receivers[0].reserved_0 = 0;
        memory_region->receivers[0].impdef.val[0] = mem_region_init->impdef.val[0];
        memory_region->receivers[0].impdef.val[1] = mem_region_init->impdef.val[1];
    }
}

uint32_t ffa_memory_region_init(struct ffa_mem_region_init *mem_region_init,
                 const struct ffa_mem_region_constituent constituents[],
                 uint32_t constituent_count) {
    ffa_mem_access_permissions_t permissions = 0;
    ffa_mem_attributes_t attributes = 0;
    struct ffa_composite_mem_region *composite_memory_region;
    uint32_t fragment_max_constituents;
    uint32_t count_to_copy;
    uint32_t i;
    uint32_t constituents_offset;
    struct ffa_mem_region *memory_region = mem_region_init->memory_region;

    /* Check for Invalid combination of multi_share and receiver count */
    if (mem_region_init->multi_share ^ (mem_region_init->receiver_count>>1)) {
        printf("Invalid Combination receiver_count %x, multi_share %x\n",
            mem_region_init->receiver_count, mem_region_init->multi_share);
    }

    /* Set memory region's permissions. */
    ffa_set_data_access_attr(&permissions, mem_region_init->data_access);
    ffa_set_instruction_access_attr(&permissions, mem_region_init->instruction_access);

    /* Set memory region's page attributes. */
    ffa_set_mem_type_attr(&attributes, mem_region_init->type);
    ffa_set_mem_cacheability_attr(&attributes, mem_region_init->cacheability);
    ffa_set_mem_shareability_attr(&attributes, mem_region_init->shareability);

    ffa_mem_region_init_header(mem_region_init, attributes, 0, permissions);
    if (mem_region_init->multi_share) {
        memory_region->receiver_count = mem_region_init->receiver_count;
        for (i = 0; i < mem_region_init->receiver_count; i++)
        {
            memory_region->receivers[i].receiver_permissions.receiver =
                              mem_region_init->receivers[i].receiver_permissions.receiver;
            memory_region->receivers[i].receiver_permissions.permissions = permissions;
            memory_region->receivers[i].receiver_permissions.flags =
                              mem_region_init->receivers[i].receiver_permissions.flags;
            memory_region->receivers[i].reserved_0 = 0;

            memory_region->receivers[i].impdef.val[0] = mem_region_init->receivers[i].impdef.val[0];
            memory_region->receivers[i].impdef.val[1] = mem_region_init->receivers[i].impdef.val[1];
        }
    }
    /*
     * Note that `sizeof(struct_ffa_memory_region)` and `sizeof(struct
     * ffa_memory_access)` must both be multiples of 16 (as verified by the
     * asserts in `ffa_memory.c`, so it is guaranteed that the offset we
     * calculate here is aligned to a 64-bit boundary and so 64-bit values
     * can be copied without alignment faults.
     */
    if (mem_region_init->multi_share) {
        for (i = 0; i < mem_region_init->receiver_count; i++) {
            mem_region_init->memory_region->receivers[i].composite_memory_region_offset =
            (uint32_t)(sizeof(struct ffa_mem_region) +
            mem_region_init->memory_region->receiver_count *
                sizeof(struct ffa_mem_access));
        }
    }
    else {
        mem_region_init->memory_region->receivers[0].composite_memory_region_offset =
        (uint32_t)(sizeof(struct ffa_mem_region) + 1 * sizeof(struct ffa_mem_access));
    }

    composite_memory_region =
        ffa_mem_region_get_composite(mem_region_init->memory_region, 0);
    composite_memory_region->page_count = 0;
    composite_memory_region->constituent_count = constituent_count;
    composite_memory_region->reserved_0 = 0;

    constituents_offset =
        (uint32_t)(mem_region_init->memory_region->receivers[0].composite_memory_region_offset +
        sizeof(struct ffa_composite_mem_region));
    fragment_max_constituents =
        (uint32_t)((mem_region_init->memory_region_max_size - constituents_offset) /
        sizeof(struct ffa_mem_region_constituent));

    count_to_copy = constituent_count;
    if (count_to_copy > fragment_max_constituents) {
        count_to_copy = fragment_max_constituents;
    }

    for (i = 0; i < constituent_count; ++i) {
        if (i < count_to_copy) {
            composite_memory_region->constituents[i] = constituents[i];
        }
        composite_memory_region->page_count += constituents[i].page_count;
    }

    mem_region_init->total_length = 
        (uint32_t)(constituents_offset +
        composite_memory_region->constituent_count * sizeof(struct ffa_mem_region_constituent));

    mem_region_init->fragment_length = 
        (uint32_t)(constituents_offset + count_to_copy * sizeof(struct ffa_mem_region_constituent));

    return composite_memory_region->constituent_count - count_to_copy;
}