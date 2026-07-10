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

status_t rpmb_read_write_counter(struct rpmb_dev *rpmb_dev, struct rpmb_frame *resp) {
    struct rpmb_frame req = {0};

    req.req_resp = BE16(RPMB_REQ_READ_WRITE_COUNTER);

    return rpmb_dev->ops->route_frames(rpmb_dev, &req, sizeof(struct rpmb_frame),
                                       resp, sizeof(struct rpmb_frame));
}

int rpmb_cmd_read_counter(int argc, const console_cmd_args *argv) {
    struct rpmb_dev *rpmb_dev = rpmb_dev_get(0);
    if (!rpmb_dev) {
        printf("RPMB device is not found\n");
        return -1;
    }

    struct rpmb_frame resp = {0};

    status_t err = rpmb_read_write_counter(rpmb_dev, &resp);
    if (err < 0) {
        printf("RPMB read counter request failed, reason: %d\n", err);
        return -1;
    }

    uint16_t result = BE16(resp.result);

    switch (result) {
    case RPMB_RESULT_AUTH_KEY_NOT_PROGRAMMED:
        printf("RPMB key is NOT programmed yet\n");
        break;
    case RPMB_RESULT_OK:
        printf("RPMB key is already programmed, write counter: %u\n", BE32(resp.write_counter));
        break;
    default:
        printf("RPMB Resp Result: %d\n", result);
        break;
    }

    return 0;
}
