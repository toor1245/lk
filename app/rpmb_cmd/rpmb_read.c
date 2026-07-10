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
#include <stddef.h>
#include <endian.h>

#include <dev/rpmb.h>

#include "rpmb_hmac.h"

int rpmb_cmd_read(int argc, const console_cmd_args *argv) {
    struct rpmb_dev *rpmb_dev = rpmb_dev_get(0);
    if (!rpmb_dev) {
        printf("RPMB device is not found\n");
        return -1;
    }

    struct rpmb_frame req = {0};
    struct rpmb_frame resp = {0};

    req.req_resp = BE16(RPMB_REQ_AUTH_READ);
    req.block_count = BE16(1);
    req.address = BE16(0x0000);
    memset(req.nonce, 0xA5, RPMB_NONCE_SIZE);

    status_t err = rpmb_dev->ops->route_frames(rpmb_dev, &req, sizeof(struct rpmb_frame),
                                               &resp, sizeof(struct rpmb_frame));
    if (err < 0) {
        printf("RPMB read request failed, reason: %d\n", err);
        return -1;
    }

    uint16_t result = BE16(resp.result);
    if (result != RPMB_RESULT_OK) {
        printf("RPMB read request failed, result: %d\n", result);
        return -1;
    }

    if (memcmp(req.nonce, resp.nonce, RPMB_NONCE_SIZE) != 0) {
        printf("RPMB read response nonce mismatch, possible replay\n");
        return -1;
    }

    uint8_t mac[RPMB_KEY_MAC_SIZE];
    rpmb_hmac(rpmb_test_key, resp.data,
              sizeof(struct rpmb_frame) - offsetof(struct rpmb_frame, data), mac);

    if (memcmp(mac, resp.key_mac, RPMB_KEY_MAC_SIZE) != 0) {
        printf("RPMB read response MAC verification failed\n");
        return -1;
    }

    resp.data[RPMB_DATA_SIZE - 1] = '\0';
    printf("RPMB read OK, MAC verified, data: '%s'\n", resp.data);

    return 0;
}
