/*
 * Copyright (c) 2026 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 *
 * RPMB frame tracer: decodes and dumps the frames of every RPMB
 * exchange routed through rpmb_route_frames().
 */

#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <endian.h>

#include <lk/debug.h>

#include <dev/rpmb.h>

/* Set to 0 to silence the per-exchange frame dumps */
#define RPMB_TRACE_FRAMES 1

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
#if RPMB_TRACE_FRAMES
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

        if (rpmb_buf_is_zero(frame->data, RPMB_DATA_SIZE)) {
            printf("  data      : (%u bytes, all zeros)\n", RPMB_DATA_SIZE);
        } else {
            printf("  data      :\n");
            hexdump8(frame->data, RPMB_DATA_SIZE);
        }
        printf("-----------------------------------------------------------\n");
    }
#endif
}
