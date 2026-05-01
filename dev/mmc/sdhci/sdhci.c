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
#include <stdbool.h>

#include <lk/compiler.h>

#include <lk/err.h>
#include <platform/time.h>
#include <dev/mmc.h>
#include <dev/mmc/sdhci.h>
#include <kernel/vm.h>

#if WITH_DEV_BUS_PCI
#include <dev/bus/pci.h>
#endif

#include "sdhci_regs.h"

static inline void delay(lk_time_t delay) {
    lk_time_t start = current_time();

    while (start + delay > current_time());
}

static void sdhci_set_power(uintptr_t base, unsigned short power) {
    uint8_t pwr = SDHCI_POWER_ON | SDHCI_POWER_330; 
    sdhci_writeb(base, pwr, SDHCI_POWER_CONTROL);
}

static void sdhci_reset(uintptr_t base, uint8_t mask) {
    unsigned long timeout;
    timeout = 100;
    sdhci_writeb(base, mask, SDHCI_SOFTWARE_RESET);
    while (sdhci_readb(base, SDHCI_SOFTWARE_RESET) & mask) {
        if (timeout == 0) {
            printf("Reset %#x never completed\n", mask);
            return;
        }
        timeout--;
        delay(1000);
    }
}

static void sdhci_pio_read(uintptr_t base, char *dst, uint64_t blksize) {
    for (size_t i = 0; i < blksize; i += sizeof(uint32_t)) {
        char *offset = dst + i;
        *(uint32_t *)offset = sdhci_readl(base, SDHCI_REG_BUFFER);
    }
}

static void sdhci_pio_write(uintptr_t base, char *src, uint64_t blksize) {
    for (size_t i = 0; i < blksize; i += sizeof(uint32_t)) {
        char *offset = src + i;
        sdhci_writel(base, *(uint32_t *)offset, SDHCI_REG_BUFFER);
    }
}

static ssize_t sdhci_transfer_data(uintptr_t base, char *buf, uint64_t blksize,
        uint64_t blkcount, bool is_read)
{
    uint32_t stat;
    bool transfer_done = false;
    uint32_t buf_ready = SDHCI_INT_SPACE_AVAIL | SDHCI_INT_DATA_AVAIL;
	uint32_t mask = SDHCI_DATA_AVAILABLE | SDHCI_SPACE_AVAILABLE;
    uint32_t block = 0;
    char *tmp = buf;
    ssize_t bytes_trns = 0;

    do {
        stat = sdhci_readl(base, SDHCI_INT_STATUS);
        if (stat & SDHCI_INT_ERROR) {
            printf("Error detected in status(%#x)!\n", stat);
            return ERR_IO;
        }

        if (!transfer_done && (stat & buf_ready)) {
            if (!(sdhci_readl(base, SDHCI_REG_PRESENT_STATE) & mask)) {
                continue;
            }

            sdhci_writel(base, buf_ready, SDHCI_INT_STATUS);

            if (is_read)
                sdhci_pio_read(base, buf, blksize);
            else
                sdhci_pio_write(base, buf, blksize);

            tmp += blksize;
            bytes_trns += blksize;
            if (++block >= blkcount) {
                transfer_done = true;
                continue;
            }
        }
    } while(!(stat & SDHCI_INT_DATA_END));

    return bytes_trns;
}

static status_t sdhci_wait_present_state(uintptr_t base) {
    uint32_t time = 0;
    static uint32_t cmd_timeout = 100;
    uint32_t mask = SDHCI_CMD_INHIBIT | SDHCI_DATA_INHIBIT;

	while (sdhci_readl(base, SDHCI_REG_PRESENT_STATE) & mask) {
        if (time >= cmd_timeout) {
            printf("SDHCI: mmc busy timeout\n");
            return ERR_TIMED_OUT;
        }
        time++;
        delay(1000);
	}

    return NO_ERROR;
}

static status_t sdhci_wait_response(uintptr_t base, struct mmc_cmd *cmd) {
    uint32_t stat;
    lk_time_t start_time = current_time();
    
    do {
        stat = sdhci_readl(base, SDHCI_INT_STATUS);
        if (stat & SDHCI_INT_ERROR) {
            printf("SDHCI: CMD%d Error detected, INT_STATUS: 0x%08x\n", cmd->idx, stat);
            sdhci_writeb(base, SDHCI_RESET_CMD, SDHCI_SOFTWARE_RESET);
            return ERR_IO;
        }
        
        if (current_time() - start_time > 1000) {
            printf("SDHCI: CMD%d response timeout\n", cmd->idx);
            return ERR_TIMED_OUT;
        }
    } while ((stat & SDHCI_INT_RESPONSE) == 0);

    sdhci_writel(base, SDHCI_INT_RESPONSE, SDHCI_INT_STATUS);

    return NO_ERROR;
}

static void sdhci_read_resp(uintptr_t base, struct mmc_cmd *cmd) {
    if (cmd->resp_type == MMC_RESP_R48 || cmd->resp_type == MMC_RESP_R1B) {
        cmd->resp[0] = sdhci_readl(base, SDHCI_REG_RESP);
        return;
    }

    for (size_t i = 0; i < sizeof(uint32_t); i++) {
        cmd->resp[i] = sdhci_readl(base, SDHCI_REG_RESP + (3 - i) * 4) << 8;
        if (i != 3) {
            cmd->resp[i] |= sdhci_readb(base, SDHCI_REG_RESP + (3 - i) * 4 - 1);
        }
    }
}

static uint16_t sdhci_prepare_cmd(struct mmc_cmd *cmd) {
    uint32_t flags = 0;
    if (cmd->resp_type == MMC_RESP_NONE)
        flags = SDHCI_CMD_RESP_NONE;
    else if (cmd->resp_type == MMC_RESP_R138)
        flags = SDHCI_CMD_RESP_LONG | SDHCI_CMD_CRC;
    else if (cmd->resp_type == MMC_RESP_R1B)
        flags = SDHCI_CMD_RESP_SHORT_BUSY | SDHCI_CMD_INDEX | SDHCI_CMD_CRC;
    else
        flags = SDHCI_CMD_RESP_SHORT | SDHCI_CMD_INDEX | SDHCI_CMD_CRC;

    if (cmd->has_data) {
        flags |= SDHCI_CMD_DATA; 
    }

    return SDHCI_MAKE_CMD(cmd->idx, flags);
}

static status_t sdhci_send_cmd(struct mmc_device *dev, struct mmc_cmd *cmd) {
    struct mmc_host *m = dev->host;
    struct sdhci_host *shost = containerof(m, struct sdhci_host, mmc);
    uintptr_t base = shost->base;
    status_t err;
    
    printf("SDHCI @ 0x%lx: Sending CMD%d\n", shost->base, cmd->idx);

    err = sdhci_wait_present_state(base);
    if (err != NO_ERROR)
        return err;

    sdhci_writel(base, SDHCI_INT_ALL_MASK, SDHCI_INT_STATUS);

    uint16_t sdhci_cmd = sdhci_prepare_cmd(cmd);

    sdhci_writel(base, cmd->arg, SDHCI_REG_ARG);
    sdhci_writew(base, sdhci_cmd, SDHCI_REG_COMMAND);

    err = sdhci_wait_response(base, cmd);
    if (err != NO_ERROR)
        return err;

    if (cmd->resp_type != MMC_RESP_NONE) {
        sdhci_read_resp(base, cmd);
        trace_cmd_resp(cmd);
    }

    return NO_ERROR;
}

static status_t sdhci_get_ext_csd(struct mmc_device *mmc_dev, uint8_t *buf) {
    status_t err;
    struct mmc_host *m = mmc_dev->host;
    struct sdhci_host *shost = containerof(m, struct sdhci_host, mmc);
    uintptr_t base = shost->base;

    uint32_t blkcount = 1;

    sdhci_writeb(base, SDHCI_RESET_DATA, SDHCI_SOFTWARE_RESET);
    sdhci_writew(base, 512, SDHCI_REG_BLOCK_SIZE);
    sdhci_writew(base, blkcount, SDHCI_REG_BLOCK_COUNT);

    sdhci_writew(base, SDHCI_TRNS_READ, SDHCI_REG_TRANSFER_MODE);

    struct mmc_cmd cmd = {
        .idx = MMC_CMD_SEND_EXT_CSD,
        .resp_type = MMC_RESP_R48,
        .arg = 0,
        .has_data = true,
    };

    err = sdhci_send_cmd(mmc_dev, &cmd);
    if (err != NO_ERROR) {
        return err;
    }

    ssize_t bytes_read = sdhci_transfer_data(base, (char *)buf, 512, blkcount, true);
    if (bytes_read < 0) {
        return (status_t)bytes_read;
    }

    return NO_ERROR;
}

static status_t sdhci_init(struct mmc_device *dev) {
    printf("SDHCI: sdhci_init called\n");

    struct mmc_host *m = dev->host;
    struct sdhci_host *shost = containerof(m, struct sdhci_host, mmc);
    uintptr_t base = shost->base;

    sdhci_reset(base, SDHCI_RESET_ALL);

    sdhci_set_power(base, 3300);

    uint16_t clk = SDHCI_CLOCK_INT_EN;
    sdhci_writew(base, clk, SDHCI_CLOCK_CONTROL);

    lk_time_t start = current_time();
    while (!(sdhci_readw(base, SDHCI_CLOCK_CONTROL) & SDHCI_CLOCK_INT_STABLE)) {
        if (current_time() - start > 20) {
            printf("SDHCI: Internal clock never stabilized\n");
            return ERR_TIMED_OUT;
        }
        delay(1000);
    }

    clk |= SDHCI_CLOCK_CARD_EN;
    sdhci_writew(base, clk, SDHCI_CLOCK_CONTROL);

    uint32_t int_mask = SDHCI_INT_DATA_MASK | SDHCI_INT_CMD_MASK | SDHCI_INT_ERROR;
    
    sdhci_writel(base, 0xFFFFFFFF, SDHCI_INT_STATUS);
    
    sdhci_writel(base, int_mask, SDHCI_INT_ENABLE);

    sdhci_writel(base, 0x0, SDHCI_SIGNAL_ENABLE);

    return NO_ERROR;
}

static status_t sdhci_fini(struct mmc_device *dev) {
    printf("SDHCI: sdhci_fini called\n");
    return NO_ERROR;
}

ssize_t sdhci_read(struct mmc_device *mmc_dev, struct mmc_xfer_info *info) {
    status_t err;
    struct mmc_host *m = mmc_dev->host;
    struct sdhci_host *shost = containerof(m, struct sdhci_host, mmc);
    uintptr_t base = shost->base;

    assert(info->blksize == 512);
    assert(info->blkcount >= 1);

    printf("SDHCI: Read blocks, blkcount=0x%x, blksize=0x%x,blk=%x  buffer=%p\n",
	    info->blkcount, info->blksize, info->block, info->buffer);

    sdhci_writel(base, info->blksize, SDHCI_REG_BLOCK_SIZE);
    sdhci_writel(base, info->blkcount, SDHCI_REG_BLOCK_COUNT);

    uint32_t trns_mode = SDHCI_TRNS_READ;

    if (info->blkcount == 1) {
        sdhci_writel(base, trns_mode, SDHCI_REG_TRANSFER_MODE);
        err = mmc_read_single_blk(mmc_dev, info->block);
    } else {
        trns_mode |= SDHCI_TRNS_MULTI | SDHCI_TRNS_BLK_CNT_EN;
        sdhci_writel(base, trns_mode, SDHCI_REG_TRANSFER_MODE);
	    err = mmc_read_multiple_blk(mmc_dev, info->block);
    }

    if (err < 0)
        return err;

    ssize_t res = sdhci_transfer_data(base, info->buffer, info->blksize, info->blkcount, true);
    if (res < 0) {
        return res;
    }

    if (info->blkcount > 1) {
        status_t stop_err = mmc_stop_transmission(mmc_dev);
        if (stop_err < 0) {
            printf("SDHCI: Failed to stop multi-block read, reason: %d\n", stop_err);
            return stop_err;
        }
    }

    return res;
}

ssize_t sdhci_write(struct mmc_device *mmc_dev, struct mmc_xfer_info *info) {
    status_t err;
    struct mmc_host *m = mmc_dev->host;
    struct sdhci_host *shost = containerof(m, struct sdhci_host, mmc);
    uintptr_t base = shost->base;

    assert(info->blksize == 512);
    assert(info->blkcount >= 1);

    printf("SDHCI: Read blocks, blkcount=0x%x, blksize=0x%x,blk=%x  buffer=%p\n",
	    info->blkcount, info->blksize, info->block, info->buffer);

    sdhci_writel(base, info->blksize, SDHCI_REG_BLOCK_SIZE);
    sdhci_writel(base, info->blkcount, SDHCI_REG_BLOCK_COUNT);

    uint32_t trns_mode = 0;

    if (info->blkcount == 1) {
        sdhci_writel(base, trns_mode, SDHCI_REG_TRANSFER_MODE);
        err = mmc_write_single_blk(mmc_dev, info->block);
    } else {
        trns_mode |= SDHCI_TRNS_MULTI | SDHCI_TRNS_BLK_CNT_EN;
        sdhci_writel(base, trns_mode, SDHCI_REG_TRANSFER_MODE);
	    err = mmc_write_multiple_blk(mmc_dev, info->block);
    }

    if (err < 0)
        return err;

    ssize_t res = sdhci_transfer_data(base, info->buffer, info->blksize, info->blkcount, false);
    if (res < 0) {
        return res;
    }

    if (info->blkcount > 1) {
        status_t stop_err = mmc_stop_transmission(mmc_dev);
        if (stop_err < 0) {
            printf("SDHCI: Failed to stop multi-block read, reason: %d\n", stop_err);
            return stop_err;
        }
    }

    return res;
}

static const struct mmc_ops sdhci_ops = {
    .init = sdhci_init,
    .fini = sdhci_fini,
    .read = sdhci_read,
    .write = sdhci_write,
    .send_cmd = sdhci_send_cmd,
    .get_ext_csd = sdhci_get_ext_csd,
};

static status_t mmc_sdhci_pci_create(struct sdhci_host **out) {
    status_t err;
    pci_location_t sdhci_loc;
    pci_bar_t bars[6];

    err = pci_bus_mgr_find_device_by_class(&sdhci_loc, 0x08, 0x05, 0x01, 0x00);
    if (err != NO_ERROR) {
        printf("SDHCI: Controller not found on PCI bus\n");
        return err;
    }

    err = pci_bus_mgr_enable_device(sdhci_loc);
    if (err != NO_ERROR) {
        printf("SDHCI: Failed to enable device\n");
        return err;
    }

    err = pci_bus_mgr_read_bars(sdhci_loc, bars);
    if (err != NO_ERROR || !bars[0].valid) {
        printf("SDHCI: Failed to read BAR0\n");
        return ERR_NOT_FOUND;
    }

    uint64_t phys_addr = bars[0].addr;
    size_t size = bars[0].size;

    void *vaddr;
    err = vmm_alloc_physical(vmm_get_kernel_aspace(), 
                             "sdhci_regs", 
                             ROUNDUP(size, PAGE_SIZE), 
                             &vaddr, 
                             PAGE_SIZE_SHIFT, 
                             phys_addr, 
                             0, 
                             ARCH_MMU_FLAG_UNCACHED_DEVICE);

    if (err != NO_ERROR) {
        printf("SDHCI: Failed to map memory\n");
        return err;
    }

    uintptr_t sdhci_base = (uintptr_t)vaddr;
    printf("SDHCI: Successfully mapped registers to 0x%lx\n", sdhci_base);

    *out = calloc(1, sizeof(struct sdhci_host));
    (*out)->base = sdhci_base;
    (*out)->mmc.ops = &sdhci_ops;
    return NO_ERROR;
}

status_t mmc_sdhci_create(struct mmc_host **out_host) {
    struct sdhci_host *shost = NULL;
    status_t err;

#if WITH_DEV_BUS_PCI
    err = mmc_sdhci_pci_create(&shost);
    if (err == NO_ERROR) {
        *out_host = &shost->mmc;
        return NO_ERROR;
    }
#endif

    printf("SDHCI: is unsupported\n");
    return ERR_NOT_SUPPORTED;
}
