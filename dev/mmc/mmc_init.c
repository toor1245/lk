/*
 * Copyright (c) 2026 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#include <sys/types.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <lk/init.h>
#include <lk/err.h>

#include <dev/mmc.h>
#include <dev/mmc_bdev.h>
#include <dev/mmc/sdhci.h>

static void mmc_init_hook(uint level) {
    printf("MMC: Initializing framework at level 0x%x\n", level);

    struct mmc_host *host = NULL;
    if (mmc_sdhci_create(&host) != NO_ERROR) {
        panic("failed to initialize SDHCI\n");
    }

    struct mmc_device *mmc_dev = NULL;
    status_t err = mmc_init(host, &mmc_dev);
    if (err != NO_ERROR) {
        panic("failed to initialize MMC device, err=%d\n", err);
    }

    mmc_bdev_init(mmc_dev);

    printf("MMC: successfully initialized\n");
}

LK_INIT_HOOK(mmc, &mmc_init_hook, LK_INIT_LEVEL_PLATFORM);
