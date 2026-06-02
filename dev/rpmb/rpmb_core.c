/*
 * Copyright (c) 2026 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#include <sys/types.h>
#include <stdint.h>
#include <stdlib.h>

#include <lk/err.h>
#include <lk/trace.h>

#include <dev/rpmb.h>

#define LOCAL_TRACE 0

static struct list_node rpmb_device_list = LIST_INITIAL_VALUE(rpmb_device_list);
static uint32_t rpmb_id = 0;

status_t rpmb_dev_register(struct rpmb_dev *dev) {
    if (!dev || !dev->ops || !dev->ops->route_frames)
        return ERR_INVALID_ARGS;

    dev->id = rpmb_id++;

    list_add_tail(&rpmb_device_list, &dev->node);

    LTRACEF("RPMB device registered (Type: %d), ID: %u\n", dev->type, dev->id);
    return NO_ERROR;
}

status_t rpmb_dev_unregister(struct rpmb_dev *dev) {
    if (!dev)
        return ERR_INVALID_ARGS;

    list_delete(&dev->node);

    return NO_ERROR;
}

struct rpmb_dev *rpmb_dev_get(uint32_t id) {
    struct rpmb_dev *dev;

    list_for_every_entry(&rpmb_device_list, dev, struct rpmb_dev, node) {
        if (dev->id == id)
            return dev;
    }

    return NULL;
}

status_t rpmb_route_frames(struct rpmb_dev *dev, const uint8_t *req,
                           uint32_t req_len, uint8_t *resp, uint32_t resp_len) {

    if (!dev || !dev->ops || !dev->ops->route_frames)
        return ERR_INVALID_ARGS;

    if (!req || req_len == 0 || (req_len % 512 != 0))
        return ERR_INVALID_ARGS;

    if (resp && (resp_len % 512 != 0))
        return ERR_INVALID_ARGS;

    return dev->ops->route_frames(dev, req, req_len, resp, resp_len);
}
