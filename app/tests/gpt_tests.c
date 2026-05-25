/*
 * Copyright (c) 2025 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#include <lk/err.h>
#include <lk/debug.h>
#include <lk/console_cmd.h>
#include <gpt.h>

#define _LOGF(fmt, args...) \
    printf("[%s:%d] " fmt, __PRETTY_FUNCTION__, __LINE__, ##args)
#define LOGF(x...) _LOGF(x)

#if WITH_DEV_MMC
#include <lib/fs.h>
#include <lib/bio.h>

#define MMC_MOUNT_POINT  "/mmc"
#define MMC_BDEV_NAME     "mmc0"

#define BUF_SIZE 1024

static int gpt_read_mbr_test(void) {
    status_t status;
    ssize_t readbytes;
    filehandle *handle;
    char buf[BUF_SIZE];

    bdev_t *dev = bio_open(MMC_BDEV_NAME);
    if (!dev) {
        LOGF("Failed to open MMC device\n");
        return ERR_NOT_FOUND;
    }

    if (gpt_probe(dev) != 0) {
        LOGF("Failed to probe GPT on MMC device\n");
        bio_close(dev);
        return ERR_IO;
    }

    return NO_ERROR;
}

int gpt_tests(int argc, const console_cmd_args *argv) {
    status_t status;

    status = gpt_read_mbr_test();

    return status;
}
#else
int gpt_tests(int argc, const console_cmd_args *argv) {
    LOGF("platform didn't have dev/mmc supported\n");
    return ERR_NOT_SUPPORTED;
}
#endif