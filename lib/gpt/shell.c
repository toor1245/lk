/*
 * Copyright (c) 2026 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#include <lk/err.h>
#include <lk/console_cmd.h>

#include <lib/bio.h>
#include <gpt.h>

static int cmd_gpt(int argc, const console_cmd_args *argv) {
    if (argc != 2) {
        printf("not enough arguments\n");
        printf("usage: %s <device>\n", argv[0].str);
        printf("  probe <device> for a GPT partition table and publish its\n");
        printf("  partitions as subdevices (e.g. mmc0 -> mmc0p1, mmc0p2, ...)\n");
        return ERR_INVALID_ARGS;
    }

    const char *device = argv[1].str;
    if (device[0] == '\0') {
        printf("error: device name must not be empty\n");
        return ERR_INVALID_ARGS;
    }

    bdev_t *dev = bio_open(device);
    if (!dev) {
        printf("error: device '%s' not found\n", device);
        return ERR_NOT_FOUND;
    }

    status_t err = gpt_probe(dev);
    bio_close(dev);

    if (err != NO_ERROR) {
        printf("no GPT partition table found on '%s'\n", device);
        return err;
    }

    return NO_ERROR;
}

STATIC_COMMAND_START
STATIC_COMMAND("gpt", "probe a GPT disk and publish its partitions", &cmd_gpt)
STATIC_COMMAND_END(gpt);
