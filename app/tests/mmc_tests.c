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

#include <lib/fs.h>
#include <lib/bio.h>

#define MMC_MOUNT_POINT  "/mmc"
#define MMC_BDEV_NAME     "mmc"

#define BUF_SIZE          1024

int mmc_tests(int argc, const console_cmd_args *argv) {
    status_t status;
    filehandle *handle;
    ssize_t readbytes;
    char buf[BUF_SIZE];

    fs_dump_list();

    status = fs_mount(MMC_MOUNT_POINT, "ext2", MMC_BDEV_NAME);
    if (status != NO_ERROR) {
      LOGF("failed to mount mmc bdev (%s) onto mount point (%s): %d\n",
           MMC_BDEV_NAME, MMC_MOUNT_POINT, status);
      return status;
    }

    LOGF("Mounted\n");

    status = fs_open_file("/mmc/test.txt", &handle);
    if (status != NO_ERROR) {
        LOGF("failed to open the target file: %d\n", status);
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
