/*
 * Copyright (c) 2026 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 *
 * RPMB frame tracer: decodes the frames of every RPMB exchange routed
 * through rpmb_route_frames(), as one line per exchange or as a full dump.
 * Select the level at runtime with "rpmb trace off|line|full".
 */

#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <endian.h>

#include <lk/console_cmd.h>
#include <lk/debug.h>
#include <lk/err.h>

#include <dev/rpmb.h>

enum rpmb_trace_level {
    RPMB_TRACE_OFF,
    RPMB_TRACE_LINE,
    RPMB_TRACE_FULL,
};

static enum rpmb_trace_level rpmb_trace_level = RPMB_TRACE_OFF;

/* Bit 7 of the result signals that the write counter has expired */
#define RPMB_RESULT_COUNTER_EXPIRED (0x0080)

/* Minimum printable run length worth extracting from a data block */
#define RPMB_STRING_MIN 4

static const char *rpmb_req_resp_str(uint16_t req_resp) {
    switch (req_resp) {
        case RPMB_REQ_PROGRAM_KEY:               return "REQ_PROGRAM_KEY";
        case RPMB_REQ_READ_WRITE_COUNTER:        return "REQ_READ_WRITE_COUNTER";
        case RPMB_REQ_AUTH_WRITE:                return "REQ_AUTH_WRITE";
        case RPMB_REQ_AUTH_READ:                 return "REQ_AUTH_READ";
        case RPMB_REQ_RESULT_READ:               return "REQ_RESULT_READ";
        case RPMB_REQ_AUTH_DEVICE_CONFIG_READ:   return "REQ_AUTH_DEVICE_CONFIG_READ";
        case RPMB_REQ_AUTH_DEVICE_CONFIG_WRITE:  return "REQ_AUTH_DEVICE_CONFIG_WRITE";
        case RPMB_RESP_PROGRAM_KEY:              return "RESP_PROGRAM_KEY";
        case RPMB_RESP_READ_WRITE_COUNTER:       return "RESP_READ_WRITE_COUNTER";
        case RPMB_RESP_AUTH_WRITE:               return "RESP_AUTH_WRITE";
        case RPMB_RESP_AUTH_READ:                return "RESP_AUTH_READ";
        case RPMB_RESP_AUTH_DEVICE_CONFIG_READ:  return "RESP_AUTH_DEVICE_CONFIG_READ";
        case RPMB_RESP_AUTH_DEVICE_CONFIG_WRITE: return "RESP_AUTH_DEVICE_CONFIG_WRITE";
        default:                                 return "UNKNOWN";
    }
}

static const char *rpmb_result_str(uint16_t result) {
    switch (result & ~RPMB_RESULT_COUNTER_EXPIRED) {
        case RPMB_RESULT_OK:                      return "OK";
        case RPMB_RESULT_GENERAL_FAILURE:         return "GENERAL_FAILURE";
        case RPMB_RESULT_AUTH_FAILURE:            return "AUTH_FAILURE";
        case RPMB_RESULT_COUNTER_FAILURE:         return "COUNTER_FAILURE";
        case RPMB_RESULT_ADDRESS_FAILURE:         return "ADDRESS_FAILURE";
        case RPMB_RESULT_WRITE_FAILURE:           return "WRITE_FAILURE";
        case RPMB_RESULT_READ_FAILURE:            return "READ_FAILURE";
        case RPMB_RESULT_AUTH_KEY_NOT_PROGRAMMED: return "AUTH_KEY_NOT_PROGRAMMED";
        default:                                  return "UNKNOWN";
    }
}

static void rpmb_dump_hex_field(const char *name, const uint8_t *buf, size_t len) {
    printf("  %-10s: ", name);
    for (size_t i = 0; i < len; i++)
        printf("%02x", buf[i]);
    printf("\n");
}

static bool rpmb_buf_is_zero(const uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (buf[i] != 0)
            return false;
    }

    return true;
}

void rpmb_dump_frames(const char *label, const void *frames, uint32_t len) {
    const struct rpmb_frame *frame = (const struct rpmb_frame *)frames;
    uint32_t cnt = len / RPMB_FRAME_SIZE;

    printf("\n============ [RPMB %s: %u frame(s), %u bytes] ============\n",
        label, cnt, len);

    for (uint32_t i = 0; i < cnt; i++, frame++) {
        uint16_t req_resp = BE16(frame->req_resp);
        uint16_t result = BE16(frame->result);

        printf("Frame [%u]\n", i);
        printf("  req_resp  : 0x%04x (%s)\n", req_resp, rpmb_req_resp_str(req_resp));
        printf("  result    : 0x%04x (%s%s)\n", result, rpmb_result_str(result),
            (result & RPMB_RESULT_COUNTER_EXPIRED) ? ", counter expired" : "");
        printf("  address   : 0x%04x\n", BE16(frame->address));
        printf("  blk count : %u\n", BE16(frame->block_count));
        printf("  wr counter: %u\n", BE32(frame->write_counter));

        rpmb_dump_hex_field("nonce", frame->nonce, RPMB_NONCE_SIZE);
        rpmb_dump_hex_field("key_mac", frame->key_mac, RPMB_KEY_MAC_SIZE);

        printf("-----------------------------------------------------------\n");
    }
}

static void rpmb_trace_result(const struct rpmb_frame *resp, status_t err) {
    if (err != NO_ERROR) {
        printf(" error=%d\n", err);
        return;
    }
    if (!resp) {
        printf("\n");
        return;
    }
    uint16_t result = BE16(resp->result);
    printf(" result=%s%s\n", rpmb_result_str(result),
        (result & RPMB_RESULT_COUNTER_EXPIRED) ? " counter_expired=1" : "");
}

static void rpmb_trace_line(const void *req, uint32_t req_len,
                            const void *resp, uint32_t resp_len, status_t err) {
    const struct rpmb_frame *rq = (const struct rpmb_frame *)req;
    const struct rpmb_frame *rs = (resp && resp_len >= RPMB_FRAME_SIZE)
        ? (const struct rpmb_frame *)resp : NULL;
    /* the result of a multi-frame read is reported in the last frame */
    const struct rpmb_frame *rs_last = rs ? rs + (resp_len / RPMB_FRAME_SIZE - 1) : NULL;

    switch (BE16(rq->req_resp)) {
        case RPMB_REQ_PROGRAM_KEY:
            printf("RPMB op=PROGRAM_KEY");
            rpmb_trace_result(rs, err);
            break;
        case RPMB_REQ_READ_WRITE_COUNTER:
            printf("RPMB op=READ_COUNTER nonce=%02x%02x%02x%02x counter=%u",
                rq->nonce[0], rq->nonce[1], rq->nonce[2], rq->nonce[3],
                rs ? BE32(rs->write_counter) : 0);
            rpmb_trace_result(rs, err);
            break;
        case RPMB_REQ_AUTH_WRITE:
            printf("RPMB op=AUTH_WRITE addr=0x%04x blocks=%u counter=%u->%u",
                BE16(rq->address), (unsigned)(req_len / RPMB_FRAME_SIZE), BE32(rq->write_counter),
                rs ? BE32(rs->write_counter) : 0);
            rpmb_trace_result(rs, err);
            break;
        case RPMB_REQ_AUTH_READ:
            printf("RPMB op=AUTH_READ addr=0x%04x blocks=%u nonce=%02x%02x%02x%02x",
                BE16(rq->address), (unsigned)(resp_len / RPMB_FRAME_SIZE),
                rq->nonce[0], rq->nonce[1], rq->nonce[2], rq->nonce[3]);
            rpmb_trace_result(rs_last, err);
            break;
        default:
            printf("RPMB op=%s", rpmb_req_resp_str(BE16(rq->req_resp)));
            rpmb_trace_result(rs, err);
            break;
    }
}

void rpmb_trace_exchange(const void *req, uint32_t req_len,
                         const void *resp, uint32_t resp_len, status_t err) {
    switch (rpmb_trace_level) {
        case RPMB_TRACE_LINE:
            rpmb_trace_line(req, req_len, resp, resp_len, err);
            break;
        case RPMB_TRACE_FULL:
            rpmb_dump_frames("request", req, req_len);
            if (err == NO_ERROR && resp)
                rpmb_dump_frames("response", resp, resp_len);
            break;
        case RPMB_TRACE_OFF:
            break;
    }
}

static int cmd_rpmb(int argc, const console_cmd_args *argv) {
    static const char *const names[] = { "off", "line", "full" };

    if (argc == 3 && !strcmp(argv[1].str, "trace")) {
        for (size_t i = 0; i < countof(names); i++) {
            if (!strcmp(argv[2].str, names[i])) {
                rpmb_trace_level = (enum rpmb_trace_level)i;
                return 0;
            }
        }
    }
    if (argc == 2 && !strcmp(argv[1].str, "trace")) {
        printf("rpmb trace: %s\n", names[rpmb_trace_level]);
        return 0;
    }

    printf("usage: %s trace [off|line|full]\n", argv[0].str);
    return ERR_INVALID_ARGS;
}

STATIC_COMMAND_START
STATIC_COMMAND("rpmb", "RPMB frame trace", &cmd_rpmb)
STATIC_COMMAND_END(rpmb);
