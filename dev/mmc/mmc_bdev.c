/*
 * Copyright (c) 2026 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#include <sys/types.h>
#include <lk/err.h>
#include <string.h>

#include <lk/trace.h>

#include <lib/bio.h>
#include <stdlib.h>

#include <dev/mmc.h>

ssize_t mmc_bdev_read_block(struct bdev *bdev, void *buf, bnum_t block, uint count) {
    struct mmc_device *dev = containerof(bdev, struct mmc_device, bdev);

    struct mmc_xfer_info info = (struct mmc_xfer_info) {
        .buffer = buf,
        .blkcount = count,
        .blksize = dev->blksize,
        .block = block,
    };

    return dev->host->ops->read(dev, &info);
}

ssize_t mmc_bdev_write_block(struct bdev *bdev, const void *buf, bnum_t block, uint count) {
    struct mmc_device *dev = containerof(bdev, struct mmc_device, bdev);

    struct mmc_xfer_info info = (struct mmc_xfer_info) {
        .buffer = buf,
        .blkcount = count,
        .blksize = dev->blksize,
        .block = block,
    };

    return dev->host->ops->write(dev, &info);
}

void mmc_bdev_init(struct mmc_device *mmc_dev) {
    bio_initialize_bdev(&mmc_dev->bdev, mmc_dev->name,
                        mmc_dev->blksize, mmc_dev->blkcount,
                        0, NULL, BIO_FLAGS_NONE);

    /* override block device hooks */
    mmc_dev->bdev.read_block = &mmc_bdev_read_block;
    mmc_dev->bdev.write_block = &mmc_bdev_write_block;

    bio_register_device(&mmc_dev->bdev);
}
