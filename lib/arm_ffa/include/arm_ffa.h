/*
 * Copyright (c) 2026 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
#pragma once

#include <stdint.h>

#define FFA_VERSION_MAJOR 0x1
#define FFA_VERSION_MINOR 0x2

#define FFA_GET_MAJOR(x) ((x >> 16) & 0x7fff)
#define FFA_GET_MINOR(x) (x & 0xffff)

/* FF-A Error Codes, Arm FF-A specification 12.2 FFA_ERROR */
#define FFA_ERROR_NOT_SUPPORTED         0xffffffff //-1
#define FFA_ERROR_INVALID_PARAMETERS    0xfffffffe //-2
#define FFA_ERROR_NO_MEMORY             0xfffffffd //-3
#define FFA_ERROR_BUSY                  0xfffffffc //-4
#define FFA_ERROR_INTERRUPTED           0xfffffffb //-5
#define FFA_ERROR_DENIED                0xfffffffa //-6
#define FFA_ERROR_RETRY                 0xfffffff9 //-7
#define FFA_ERROR_ABORTED               0xfffffff8 //-8
#define FFA_ERROR_NODATA                0xfffffff7 //-9
#define FFA_ERROR_NOT_READY             0xfffffff6 //-10

#define FFA_FID_LIST(X) \
    /* FF-A specification */                                \
    X(FFA_ERROR_SMC32,                          0x84000060) \
    X(FFA_SUCCESS_SMC32,                        0x84000061) \
    X(FFA_INTERRUPT_SMC32,                      0x84000062) \
    X(FFA_VERSION_SMC32,                        0x84000063) \
    X(FFA_FEATURES_SMC32,                       0x84000064) \
    X(FFA_RX_RELEASE_SMC32,                     0x84000065) \
    X(FFA_RXTX_MAP_SMC32,                       0x84000066) \
    X(FFA_RXTX_UNMAP_SMC32,                     0x84000067) \
    X(FFA_PARTITION_INFO_GET_SMC32,             0x84000068) \
    X(FFA_ID_GET_SMC32,                         0x84000069) \
    X(FFA_MSG_POLL_SMC32,                       0x8400006A) \
    X(FFA_MSG_WAIT_SMC32,                       0x8400006B) \
    X(FFA_YIELD_SMC32,                          0x8400006C) \
    X(FFA_RUN_SMC32,                            0x8400006D) \
    X(FFA_MSG_SEND_SMC32,                       0x8400006E) \
    X(FFA_MSG_SEND_DIRECT_REQ_SMC32,            0x8400006F) \
    X(FFA_MSG_SEND_DIRECT_RESP_SMC32,           0x84000070) \
    /* FF-A Memory Management Protocol */                   \
    X(FFA_MEM_DONATE_SMC32,                     0x84000071) \
    X(FFA_MEM_LEND_SMC32,                       0x84000072) \
    X(FFA_MEM_SHARE_SMC32,                      0x84000073) \
    X(FFA_MEM_RETRIEVE_REQ_SMC32,               0x84000074) \
    X(FFA_MEM_RETRIEVE_RESP_SMC32,              0x84000075) \
    X(FFA_MEM_RELINQUISH_SMC32,                 0x84000076) \
    X(FFA_MEM_RECLAIM_SMC32,                    0x84000077) \
    X(FFA_MEM_OP_PAUSE_SMC32,                   0x84000078) \
    X(FFA_MEM_OP_RESUME_SMC32,                  0x84000079) \
    X(FFA_MEM_FRAG_RX_SMC32,                    0x8400007A) \
    X(FFA_MEM_FRAG_TX_SMC32,                    0x8400007B) \
    /* FF-A specification */                                \
    X(FFA_NORMAL_WORLD_RESUME_SMC32,            0x8400007C) \
    X(FFA_NOTIFICATION_BITMAP_CREATE_SMC32,     0x8400007D) \
    X(FFA_NOTIFICATION_BITMAP_DESTROY_SMC32,    0x8400007E) \
    X(FFA_NOTIFICATION_BIND_SMC32,              0x8400007F) \
    X(FFA_NOTIFICATION_UNBIND_SMC32,            0x84000080) \
    X(FFA_NOTIFICATION_SET_SMC32,               0x84000081) \
    X(FFA_NOTIFICATION_GET_SMC32,               0x84000082) \
    X(FFA_NOTIFICATION_INFO_GET_SMC32,          0x84000083) \
    X(FFA_RX_ACQUIRE_SMC32,                     0x84000084) \
    X(FFA_SPM_ID_GET_SMC32,                     0x84000085) \
    X(FFA_MSG_SEND2_SMC32,                      0x84000086) \
    X(FFA_SECONDARY_EP_REGISTER_SMC32,          0x84000087) \
    X(FFA_MEM_PERM_GET_SMC32,                   0x84000088) \
    X(FFA_MEM_PERM_SET_SMC32,                   0x84000089) \
    X(FFA_CONSOLE_LOG_SMC32,                    0x8400008A) \
    /* SMC64 */\
    X(FFA_SUCCESS_SMC64,                        0xC4000061) \
    X(FFA_RXTX_MAP_SMC64,                       0xC4000066) \
    X(FFA_MSG_SEND_DIRECT_REQ_SMC64,            0xC400006F) \
    X(FFA_MSG_SEND_DIRECT_RESP_SMC64,           0xC4000070) \
    X(FFA_MEM_DONATE_SMC64,                     0xC4000071) \
    X(FFA_MEM_LEND_SMC64,                       0xC4000072) \
    X(FFA_MEM_SHARE_SMC64,                      0xC4000073) \
    X(FFA_MEM_RETRIEVE_REQ_SMC64,               0xC4000074) \
    X(FFA_SECONDARY_EP_REGISTER_SMC64,          0xC4000087) \
    X(FFA_NOTIFICATION_INFO_GET_SMC64,          0xC4000083) \
    X(FFA_MEM_PERM_GET_SMC64,                   0xC4000088) \
    X(FFA_MEM_PERM_SET_SMC64,                   0xC4000089) \
    X(FFA_CONSOLE_LOG_SMC64,                    0xC400008A) \
    X(FFA_MSG_SEND_DIRECT_REQ2_SMC64,           0xC400008D) \
    X(FFA_MSG_SEND_DIRECT_RESP2_SMC64,          0xC400008E) \
    X(FFA_PARTITION_INFO_GET_REGS_SMC64,        0xC400008B)

#define FFA_FID_UPPER_MASK        0xFFFF0000
#define FFA_FID_LOWER_MASK        0x0000FFFF

#define FFA_FID_ABI32_PREFIX      0x84000000
#define FFA_FID_ABI64_PREFIX      0xC4000000

/* FF-A Function Enum*/
typedef enum {
#define X(name, val) name = val,
    FFA_FID_LIST(X)
#undef X
} ffa_func_id_t;

static const ffa_func_id_t ffa_supported_fids[] = {
#define X(name, val) name,
    FFA_FID_LIST(X)
#undef X
};

#define FFA_SUPPORTED_FID_COUNT \
    (sizeof(ffa_supported_fids) / sizeof(ffa_supported_fids[0]))

#define SENDER_ID(x)    (x >> 16) & 0xffff
#define RECEIVER_ID(x)  (x & 0xffff)

/* 16-bit ID of FF-A component */
typedef uint16_t ffa_endpoint_id_t;

typedef struct {
    uint64_t fid;
    uint64_t arg1;
    uint64_t arg2;
    uint64_t arg3;
    uint64_t arg4;
    uint64_t arg5;
    uint64_t arg6;
    uint64_t arg7;
    struct {
        uint64_t arg8;
        uint64_t arg9;
        uint64_t arg10;
        uint64_t arg11;
        uint64_t arg12;
        uint64_t arg13;
        uint64_t arg14;
        uint64_t arg15;
        uint64_t arg16;
        uint64_t arg17;
    } ext_args;
} ffa_args_t;

typedef struct {
    void *recv;
    void *send;
} mb_buf_t;

typedef struct {
    /** The ID of the VM the information is about */
    ffa_endpoint_id_t id;
    /** The number of execution contexts implemented by the partition */
    uint16_t exec_context;
    /** The Partition's properties, e.g. supported messaging methods */
    uint32_t properties;
	uint32_t uuid[4];
} ffa_partition_info_t;

#define FFA_PARTITION_INFO_FLAG_RETCOUNT       0x00000001
#define FFA_PARTITION_INFO_FLAG_RETDESC        0x00000000

/* Conduit function to perform the actual SMC call */
void call_conduit(ffa_args_t *args);

/* Version & basic FF-A functions */
void ffa_version(ffa_args_t *args);
void ffa_error(ffa_args_t *args);
void ffa_success_smc32(ffa_args_t *args);
void ffa_success_smc64(ffa_args_t *args);
void ffa_features(ffa_args_t *args);

/* Message passing functions */
void ffa_msg_send_direct_req_32(ffa_args_t *args);
void ffa_msg_send_direct_req_64(ffa_args_t *args);
void ffa_msg_send_direct_resp_32(ffa_args_t *args);
void ffa_msg_send_direct_resp_64(ffa_args_t *args);
void ffa_msg_send_direct_req2(ffa_args_t *args);
void ffa_msg_send_direct_resp2_64(ffa_args_t *args);
void ffa_msg_send(ffa_args_t *args);
void ffa_msg_send2(ffa_args_t *args);
void ffa_msg_wait(ffa_args_t *args);
void ffa_msg_poll(ffa_args_t *args);
void ffa_yield(ffa_args_t *args);

/* Endpoint & partition info */
void ffa_id_get(ffa_args_t *args);
ffa_endpoint_id_t ffa_get_curr_endpoint_id(void);
void ffa_spm_id_get(ffa_args_t *args);
void ffa_partition_info_get(ffa_args_t *args);
void ffa_partition_info_get_regs(ffa_args_t *args);

/* RX/TX buffer management */
void ffa_rxtx_map_32(ffa_args_t *args);
void ffa_rxtx_map_64(ffa_args_t *args);
void ffa_rxtx_unmap(ffa_args_t *args);
void ffa_rx_release(ffa_args_t *args);

/* Memory sharing / management */
void ffa_mem_donate_32(ffa_args_t *args);
void ffa_mem_donate_64(ffa_args_t *args);
void ffa_mem_lend_32(ffa_args_t *args);
void ffa_mem_lend_64(ffa_args_t *args);
void ffa_mem_share_32(ffa_args_t *args);
void ffa_mem_share_64(ffa_args_t *args);
void ffa_mem_retrieve_32(ffa_args_t *args);
void ffa_mem_retrieve_64(ffa_args_t *args);
void ffa_mem_reclaim(ffa_args_t *args);

/* Run / execute function */
void ffa_run(ffa_args_t *args);

/* RX/TX map helpers returning status */
uint32_t rxtx_map_32(uint64_t tx_buf, uint64_t rx_buf, uint32_t page_count);
uint32_t rxtx_map_64(uint64_t tx_buf, uint64_t rx_buf, uint32_t page_count);
uint32_t rxtx_unmap(ffa_endpoint_id_t id);