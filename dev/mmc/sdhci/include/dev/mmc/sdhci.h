/*
 * Copyright (c) 2026 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#pragma once

#include <stdint.h>
#include <dev/mmc.h>
#include <dev/mmc/sdhci.h>
#include <sys/types.h>

struct sdhci_host {
    unsigned long base;
    struct mmc_host mmc;
};

status_t mmc_sdhci_create(struct mmc_host **out_host);
