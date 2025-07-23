/*
 * Copyright (c) 2025 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#include <string.h>

#include <lk/trace.h>

#include <lib/bio.h>

#include <dev/mmc.h>
#include <dev/class/mmc.h>

#define LOCAL_TRACE 1

ssize_t mmc_bdev_read_block(struct bdev *bdev, void *buf, bnum_t block, uint count) {
    struct mmc_device *dev = containerof(bdev, struct mmc_device, bdev);
    struct mmc_xfer_info info = (struct mmc_xfer_info) {
        .buffer = buf,
        .blkcount = count,
        .blksize = dev->blksize,
	.block = block,
    };

    return class_mmc_read(dev->dev, &info);
}

ssize_t mmc_bdev_write_block(struct bdev *bdev, const void *buf, bnum_t block, uint count) {
    struct mmc_device *dev = containerof(bdev, struct mmc_device, bdev);
    struct mmc_xfer_info info = (struct mmc_xfer_info) {
        .buffer = buf,
        .blkcount = count,
        .blksize = dev->blksize,
	.block = block,
    };

    return class_mmc_write(dev->dev, &info);
}

status_t mmc_bdev_init(struct mmc_device *mmc_dev) {
    mmc_dev->blksize = 512;
    mmc_dev->blkcount = 4096;
    mmc_dev->capacity = mmc_dev->blksize * mmc_dev->blkcount; // 16MB

    bio_initialize_bdev(&mmc_dev->bdev, "mmc",
                        mmc_dev->blksize, mmc_dev->blkcount,
                        0, NULL, BIO_FLAGS_NONE);

    /* override block device hooks */
    mmc_dev->bdev.read_block = &mmc_bdev_read_block;
    mmc_dev->bdev.write_block = &mmc_bdev_write_block;

    bio_register_device(&mmc_dev->bdev);

    return 0;
}
