/*
 * Copyright (c) 2026 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#pragma once

#include <sys/types.h>
#include <lib/bio.h>
#include <dev/mmc.h>

ssize_t mmc_bdev_read_block(struct bdev *bdev, void *buf, bnum_t block, uint count);
ssize_t mmc_bdev_write_block(struct bdev *bdev, const void *buf, bnum_t block, uint count);
void mmc_bdev_init(struct mmc_device *mmc_dev);
