/*
 * Copyright (c) 2025 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#include <lk/err.h>
#include <dev/class/mmc.h>

status_t class_mmc_send_cmd(struct device *dev, struct mmc_cmd *cmd) {
    struct mmc_ops *ops = device_get_driver_ops(dev, struct mmc_ops, std);
    if (!ops)
        return ERR_NOT_CONFIGURED;

    if (ops->send_cmd)
        return ops->send_cmd(dev, cmd);
    else
        return ERR_NOT_SUPPORTED;
}

status_t class_mmc_read(struct device *dev, struct mmc_xfer_info *info) {
    struct mmc_ops *ops = device_get_driver_ops(dev, struct mmc_ops, std);
    if (!ops)
        return ERR_NOT_CONFIGURED;

    if (ops->read)
        return ops->read(dev, info);
    else
        return ERR_NOT_SUPPORTED;
}

status_t class_mmc_write(struct device *dev, struct mmc_xfer_info *info) {
    struct mmc_ops *ops = device_get_driver_ops(dev, struct mmc_ops, std);
    if (!ops)
        return ERR_NOT_CONFIGURED;

    if (ops->read)
        return ops->write(dev, info);
    else
        return ERR_NOT_SUPPORTED;
}
