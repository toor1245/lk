/*
 * Copyright (c) 2025 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
#include <app/tests.h>
#include <lk/err.h>
#include <lk/debug.h>

#define _LOGF(fmt, args...) \
    printf("[%s:%d] " fmt, __PRETTY_FUNCTION__, __LINE__, ##args)
#define LOGF(x...) _LOGF(x)

#if WITH_DEV_MMC
#include <lib/fs.h>
#include <lib/bio.h>

#define MMC_MOUNT_POINT  "/mmc"
#define MMC_BDEV_NAME     "mmc"

#define BUF_SIZE 1024

static int mmc_read_test(void) {
    status_t status;
    ssize_t readbytes;
    filehandle *handle;
    char buf[BUF_SIZE];

    status = fs_mount(MMC_MOUNT_POINT, "ext2", MMC_BDEV_NAME);
    if (status != NO_ERROR) {
        printf("Mount failed: %d\n", status);
        return status;
    }

    status = fs_open_file(MMC_MOUNT_POINT "/test.txt", &handle);
    if (status != NO_ERROR) {
        printf("Open failed: %d\n", status);
        fs_unmount(MMC_MOUNT_POINT);
        return status;
    }

    readbytes = fs_read_file(handle, buf, 0, BUF_SIZE);
    if (readbytes < 0) {
        LOGF("failed to read the target file: %ld\n", readbytes);
        return status;
    }

    hexdump8(buf, BUF_SIZE);

    status = fs_close_file(handle);
    if (status != NO_ERROR) {
        LOGF("failed to close the target file: %d\n", status);
        return status;
    }

    status = fs_unmount(MMC_MOUNT_POINT);
    if (status != NO_ERROR) {
        LOGF("failed to unmount mmc on mount point (%s): %d\n",
             MMC_MOUNT_POINT, status);
        return status;
    }

    return NO_ERROR;
}

int mmc_tests(int argc, const console_cmd_args *argv) {
    status_t status;

    status = mmc_read_test();

    return status;
}
#else
int mmc_tests(int argc, const console_cmd_args *argv) {
    LOGF("platform didn't have dev/mmc supported\n");
    return ERR_NOT_SUPPORTED;
}
#endif
