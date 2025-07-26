/*
 * Copyright (c) 2012 Corey Tabaka
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#include <dev/driver.h>
#include <assert.h>
#include <lk/err.h>
#include <lk/trace.h>
#include <lk/list.h>
#include <stdlib.h>
#include <string.h>

/* static list of devices constructed with DEVICE_INSTANCE macros */
extern struct device __start_devices __WEAK;
extern struct device __stop_devices __WEAK;

status_t device_init_all(void) {
    status_t res = NO_ERROR;

    for (struct device *dev = &__start_devices; dev != &__stop_devices; dev++) {
        if (dev->flags & DEVICE_FLAG_AUTOINIT) {
            status_t code = device_init(dev);

            if (code < 0) {
                TRACEF("Driver init failed for driver \"%s\", device \"%s\", reason %d\n",
                       dev->driver->type, dev->name, code);

                res = code;
            }
        }
    }

    return res;
}

status_t device_fini_all(void) {
    status_t res = NO_ERROR;

    for (struct device *dev = &__start_devices; dev != &__stop_devices; dev++) {
        status_t code = device_fini(dev);

        if (code < 0) {
            TRACEF("Driver fini failed for driver \"%s\", device \"%s\", reason %d\n",
                   dev->driver->type, dev->name, code);

            res = code;
        }
    }

    return res;
}

status_t device_init(struct device *dev) {
    if (!dev)
        return ERR_INVALID_ARGS;

    DEBUG_ASSERT(dev->driver);

    const struct driver_ops *ops = dev->driver->ops;

    if (ops && ops->init) {
        dprintf(INFO, "dev: initializing device %s:%s\n", dev->driver->type, dev->name);
        status_t err = ops->init(dev);
        if (err < 0) {
            dev->device_state = DEVICE_INITIALIZED_FAILED;
        } else {
            dev->device_state = DEVICE_INITIALIZED;
        }
        return err;
    } else {
        return ERR_NOT_SUPPORTED;
    }
}

status_t device_fini(struct device *dev) {
    if (!dev)
        return ERR_INVALID_ARGS;

    DEBUG_ASSERT(dev->driver);

    const struct driver_ops *ops = dev->driver->ops;

    if (ops && ops->fini)
        return ops->fini(dev);
    else
        return ERR_NOT_SUPPORTED;
}

status_t device_suspend(struct device *dev) {
    if (!dev)
        return ERR_NOT_SUPPORTED;

    DEBUG_ASSERT(dev->driver);

    const struct driver_ops *ops = dev->driver->ops;

    if (ops && ops->suspend)
        return ops->suspend(dev);
    else
        return ERR_NOT_SUPPORTED;
}

status_t device_resume(struct device *dev) {
    if (!dev)
        return ERR_NOT_SUPPORTED;

    DEBUG_ASSERT(dev->driver);

    const struct driver_ops *ops = dev->driver->ops;

    if (ops && ops->resume)
        return ops->resume(dev);
    else
        return ERR_NOT_SUPPORTED;
}

status_t device_get_list_type(const char *type, struct list_node *out_list) {
    struct device_list_entry *entry;

    if (!type || !out_list) {
        return ERR_INVALID_ARGS;
    }

     for (struct device *dev = &__start_devices; dev != &__stop_devices; dev++) {
        if (strcmp(dev->driver->type, type) != 0)
            continue;

        entry = malloc(sizeof(struct device_list_entry));
        if (!entry) {
            device_list_destroy(out_list);
            return ERR_NO_MEMORY;
        }

        entry->dev = dev;
        list_add_tail(out_list, &entry->node);
    }

    return NO_ERROR;
}

void device_list_destroy(struct list_node *list) {
    struct device_list_entry *entry;
    struct list_node *node, *tmp;

    list_for_every_safe(list, node, tmp) {
        entry = containerof(node, struct device_list_entry, node);
        list_delete(&entry->node);
        free(entry);
    }
}
