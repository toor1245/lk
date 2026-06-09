/*
 * Copyright (c) 2026 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#include <dev/tee.h>

#include <string.h>
#include <stdio.h>

#include <lk/err.h>

static struct list_node tee_devices = LIST_INITIAL_VALUE(tee_devices);

status_t tee_dev_register(struct tee_device *dev) {
    if (!dev || !dev->ops || !dev->name)
        return ERR_INVALID_ARGS;

    list_add_tail(&tee_devices, &dev->node);

    printf("TEE device registered %s\n", dev->name);

    return NO_ERROR;
}

struct tee_device *tee_dev_get_by_name(const char *name) {
    struct tee_device *dev;

    list_for_every_entry(&tee_devices, dev, struct tee_device, node) {
        if (strcmp(dev->name, name) == 0) {
            return dev;
        }
    }

    return NULL;
}

struct tee_device *tee_dev_get_first(void) {
    if (list_is_empty(&tee_devices)) {
        return NULL;
    }

    return containerof(tee_devices.next, struct tee_device, node);
}

void tee_dev_free_operation(struct tee_device *dev, struct tee_operation *op) {
    for (int i = 0; i < 4; i++) {
        uint32_t ptype = TEE_PARAM_TYPE_GET(op->paramTypes, i);
        if ((ptype == TEE_PARAM_ATTR_TYPE_MEMREF_TEMP_INOUT ||
             ptype == TEE_PARAM_ATTR_TYPE_MEMREF_TEMP_OUTPUT ||
             ptype == TEE_PARAM_ATTR_TYPE_MEMREF_TEMP_INPUT) && op->params[i].tmem.shm) {
            dev->ops->shm_free(dev, op->params[i].tmem.shm);
            op->params[i].tmem.shm = NULL;
        }
    }
}