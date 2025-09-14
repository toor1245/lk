/*
 * Copyright (c) 2025 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
#pragma once

#include <stdint.h>
#include <lk/compiler.h>
#include <dev/driver.h>

enum mmc_resp {
    MMC_RESP_NONE,
    MMC_RESP_R48,
    MMC_RESP_R138,
};

struct mmc_cmd {
    uint32_t idx;
    uint32_t arg;
    uint32_t resp[4];
    enum mmc_resp resp_type;
};

/* MMC/SD data transfer info */
struct mmc_xfer_info {
    char *buffer;
    uint32_t blkcount;
    uint32_t blksize;
    uint32_t block;
};

/* MMC/SD interface */
struct mmc_ops {
    struct driver_ops std;
 
    status_t (*send_cmd)(struct device *dev, struct mmc_cmd *cmd);
    ssize_t (*read)(struct device *dev, struct mmc_xfer_info *info);
    ssize_t (*write)(struct device *dev, struct mmc_xfer_info *info);
};

__BEGIN_CDECLS

status_t class_mmc_send_cmd(struct device *dev, struct mmc_cmd *cmd);
ssize_t class_mmc_read(struct device *dev, struct mmc_xfer_info *info);
ssize_t class_mmc_write(struct device *dev, struct mmc_xfer_info *info);

__END_CDECLS
