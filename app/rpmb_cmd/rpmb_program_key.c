/*
 * Copyright (c) 2026 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#include "rpmb_cmd.h"

#include <string.h>
#include <stdio.h>
#include <endian.h>

#include <dev/rpmb.h>

static const uint8_t rpmb_test_key[RPMB_KEY_MAC_SIZE] = {
	0xD3, 0xEB, 0x3E, 0xC3, 0x6E, 0x33, 0x4C, 0x9F,
	0x98, 0x8C, 0xE2, 0xC0, 0xB8, 0x59, 0x54, 0x61,
	0x0D, 0x2B, 0xCF, 0x86, 0x64, 0x84, 0x4D, 0xF2,
	0xAB, 0x56, 0xE6, 0xC6, 0x1B, 0xB7, 0x01, 0xE4
};

int rpmb_cmd_program_key(int argc, const console_cmd_args *argv) {
    struct rpmb_dev *rpmb_dev = rpmb_dev_get(0);
    if (!rpmb_dev) {
        printf("RPMB device is not found\n");
        return -1;
    }

    struct rpmb_frame req = {0};
    struct rpmb_frame resp = {0};

    req.req_resp = BE16(RPMB_REQ_PROGRAM_KEY);
    req.block_count = BE16(1);
    memcpy(req.key_mac, rpmb_test_key, RPMB_KEY_MAC_SIZE);

    rpmb_dev->ops->route_frames(rpmb_dev, &req, sizeof(struct rpmb_frame), &resp, sizeof(struct rpmb_frame));

    printf("RPMB Resp Result: %d\n", BE16(resp.result));

    return 0;
}
