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

int rpmb_cmd_write(int argc, const console_cmd_args *argv) {
    struct rpmb_dev *rpmb_dev = rpmb_dev_get(0);
    if (!rpmb_dev) {
        printf("RPMB device is not found\n");
        return -1;
    }

    struct rpmb_frame counter_resp = {0};

    status_t err = rpmb_read_write_counter(rpmb_dev, &counter_resp);
    if (err < 0) {
        printf("RPMB read counter request failed, reason: %d\n", err);
        return -1;
    }

    if (BE16(counter_resp.result) != RPMB_RESULT_OK) {
        printf("RPMB write aborted, could not read write counter, result: %d\n",
               BE16(counter_resp.result));
        return -1;
    }

    struct rpmb_frame req = {0};
    struct rpmb_frame resp = {0};

    req.req_resp = BE16(RPMB_REQ_AUTH_WRITE);
    req.block_count = BE16(1);
    req.address = BE16(0x0000);

    req.write_counter = counter_resp.write_counter;
    strncpy((char *)req.data, "hello rpmb", RPMB_DATA_SIZE);

    rpmb_hmac(rpmb_test_key, req.data,
              sizeof(struct rpmb_frame) - offsetof(struct rpmb_frame, data), req.key_mac);

    err = rpmb_dev->ops->route_frames(rpmb_dev, &req, sizeof(struct rpmb_frame),
                                      &resp, sizeof(struct rpmb_frame));
    if (err < 0) {
        printf("RPMB write request failed, reason: %d\n", err);
        return -1;
    }

    uint16_t result = BE16(resp.result);
    if (result != RPMB_RESULT_OK) {
        printf("RPMB write request failed, result: %d\n", result);
        return -1;
    }

    printf("RPMB write OK\n");

    return 0;
}
