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
#include <lk/bits.h>

#include <platform/time.h>
#include <dev/mmc.h>
#include <dev/mmc/sdhci.h>
#include <kernel/vm.h>

#if WITH_DEV_BUS_PCI
#include <dev/bus/pci.h>
#endif

#include "sdhci_regs.h"

#define SDHCI_MAKE_BLKSZ(dma, blksz) (((dma & 0x7) << 12) | (blksz & 0xFFF))

static inline void delay(lk_time_t delay) {
    lk_time_t start = current_time();

    while (start + delay > current_time());
}

static void sdhci_dump_present_state(uintptr_t base) {
    uint32_t pstate = sdhci_readl(base, SDHCI_REG_PRESENT_STATE);
    printf("SDHCI: PRESENT_STATE: 0x%08x\n", pstate);
    printf("\tCommand Inhibit (CMD) [0]: %lld\n", extract_bit_range(pstate, 0, 0));
    printf("\tCommand Inhibit (DAT) [1]: %lld\n", extract_bit_range(pstate, 1, 1));
    printf("\tDAT Line Active [2]: %lld\n", extract_bit_range(pstate, 2, 2));
    printf("\tRe-Tuning Request (UHS-I Only) [3]: %lld\n", extract_bit_range(pstate, 3, 3));
    printf("\tLine Signal Level (Embedded only) [7:4]: %lld\n", extract_bit_range(pstate, 7, 4));
    printf("\tWrite Transfer Active [8]: %lld\n", extract_bit_range(pstate, 8, 8));
    printf("\tRead Transfer Active [9]: %lld\n", extract_bit_range(pstate, 9, 9));
    printf("\tBuf Write Enable [10]: %lld\n", extract_bit_range(pstate, 10, 10));
    printf("\tBuf Read Enable [11]: %lld\n", extract_bit_range(pstate, 11, 11));
    printf("\tCard Inserted [16]: %lld\n", extract_bit_range(pstate, 16, 16));
    printf("\tCard State Stable [17]: %lld\n", extract_bit_range(pstate, 17, 17));
    printf("\tCard Detect Pin Level [18]: %lld\n", extract_bit_range(pstate, 18, 18));
    printf("\tWrite Protect Switch Level [19]: %lld\n", extract_bit_range(pstate, 19, 19));
    printf("\tDAT[3:0] Line Signal Level [23:20]: %lld\n", extract_bit_range(pstate, 23, 20));
    printf("\tCMD Line Signal Level [24]: %lld\n", extract_bit_range(pstate, 24, 24));
    printf("\tHost Regulator Voltage Stable [25]: %lld\n", extract_bit_range(pstate, 25, 25));
    printf("\tCommand Not Issued by Error [27]: %lld\n", extract_bit_range(pstate, 27, 27));
    printf("\tSub Command Status [28]: %lld\n", extract_bit_range(pstate, 28, 28));
}

static void sdhci_dump_normal_interrupt_status(uintptr_t base) {
    uint16_t int_status = sdhci_readw(base, SDHCI_REG_NORMAL_INT_STATUS); 
    printf("SDHCI: NORMAL_INT_STATUS: 0x%08x\n", int_status);
    printf("\tCommand Complete [0]: %lld\n", extract_bit_range(int_status, 0, 0));
    printf("\tTransfer Complete [1]: %lld\n", extract_bit_range(int_status, 1, 1));
    printf("\tBlock Gap Event [2]: %lld\n", extract_bit_range(int_status, 2, 2));
    printf("\tDMA Complete [3]: %lld\n", extract_bit_range(int_status, 3, 3));
    printf("\tBuffer Write Ready [4]: %lld\n", extract_bit_range(int_status, 4, 4));
    printf("\tBuffer Read Ready [5]: %lld\n", extract_bit_range(int_status, 5, 5));
    printf("\tCard Insertion [6]: %lld\n", extract_bit_range(int_status, 6, 6));
    printf("\tCard Removal[7]: %lld\n", extract_bit_range(int_status, 7, 7));
    printf("\tCard Interrupt [8]: %lld\n", extract_bit_range(int_status, 8, 8));
    printf("\tINT_A [9]: %lld\n", extract_bit_range(int_status, 9, 9));
    printf("\tINT_B [10]: %lld\n", extract_bit_range(int_status, 10, 10));
    printf("\tINT_C [11]: %lld\n", extract_bit_range(int_status, 11, 11));
    printf("\tError Interrupt [15]: %lld\n", extract_bit_range(int_status, 15, 15));
}

static void sdhci_dump_error_interrupt_status(uintptr_t base) {
    uint16_t int_status = sdhci_readw(base, SDHCI_REG_ERROR_INT_STATUS);
    printf("SDHCI: ERROR_INT_STATUS: 0x%08x\n", int_status);
    printf("\tCommand Timeout Error [0]: %lld\n", extract_bit_range(int_status, 0, 0));
    printf("\tCommand CRC Error [1]: %lld\n", extract_bit_range(int_status, 1, 1));
    printf("\tCommand End Bit Error [2]: %lld\n", extract_bit_range(int_status, 2, 2));
    printf("\tCommand Index Error [3]: %lld\n", extract_bit_range(int_status, 3, 3));
    printf("\tData Timeout Error [4]: %lld\n", extract_bit_range(int_status, 4, 4));
    printf("\tData Crc Error [5]: %lld\n", extract_bit_range(int_status, 5, 5));
    printf("\tData End Bit Error [6]: %lld\n", extract_bit_range(int_status, 6, 6));
    printf("\tCurrent Limit Error [7]: %lld\n", extract_bit_range(int_status, 7, 7));
    printf("\tAuto CMD Error [8]: %lld\n", extract_bit_range(int_status, 8, 8));
    printf("\tADMA Error [9]: %lld\n", extract_bit_range(int_status, 9, 9));
    printf("\tResponse Error [11]: %lld\n", extract_bit_range(int_status, 11, 11));
    printf("\tVendor Specific Status Error [15:12]: %lld\n", extract_bit_range(int_status, 15, 12));
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

static status_t sdhci_transfer_data(uintptr_t base, char *buf, uint64_t blksize,
                                  uint64_t blkcount, bool is_read) {
    uint32_t stat, rdy, mask;
    uint64_t block = 0;
    bool transfer_done = false;
    char *tmp = buf;
    ssize_t bytes_trns = 0;
    lk_time_t start = current_time();

    rdy = is_read ? SDHCI_INT_DATA_AVAIL : SDHCI_INT_SPACE_AVAIL;
    mask = is_read ? SDHCI_DATA_AVAILABLE : SDHCI_SPACE_AVAILABLE;

    do {
        stat = sdhci_readw(base, SDHCI_REG_NORMAL_INT_STATUS);
        if (stat & SDHCI_INT_ERROR) {
            printf("SDHCI: Data error detected! Stat: 0x%08x\n", stat);
            return ERR_IO;
        }

        if (!transfer_done && (stat & rdy)) {
            if (!(sdhci_readw(base, SDHCI_REG_PRESENT_STATE) & mask))
                continue;

            sdhci_writew(base, rdy, SDHCI_REG_NORMAL_INT_STATUS);

            if (is_read)
                sdhci_pio_read(base, tmp, blksize);
            else
                sdhci_pio_write(base, tmp, blksize);

            tmp += blksize;
            bytes_trns += blksize;
            if (++block >= blkcount) {
                transfer_done = true;
                continue;
            }
        }

        if (current_time() - start > 5000) {
            printf("SDHCI: Transfer timeout! Stat: 0x%08x, Present: 0x%08x\n", 
                   stat, sdhci_readw(base, SDHCI_REG_PRESENT_STATE));
            sdhci_dump_present_state(base);
            return ERR_TIMED_OUT;
        }

    } while (!(stat & SDHCI_INT_DATA_END));

    sdhci_writew(base, SDHCI_INT_DATA_END, SDHCI_REG_NORMAL_INT_STATUS);

    return (status_t)bytes_trns;
}

static status_t sdhci_wait_present_state(uintptr_t base, uint32_t mask) {
    uint32_t time = 0;
    static uint32_t cmd_timeout = 100;

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
        stat = sdhci_readw(base, SDHCI_REG_NORMAL_INT_STATUS);
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

    sdhci_writew(base, SDHCI_INT_RESPONSE, SDHCI_REG_NORMAL_INT_STATUS);

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

static status_t sdhci_send_cmd(struct mmc_device *dev, struct mmc_cmd *cmd) {
    struct mmc_host *m = dev->host;
    struct sdhci_host *shost = containerof(m, struct sdhci_host, mmc);
    uintptr_t base = shost->base;
    uint32_t flags, mode = 0;
    status_t err;

    uint32_t mask = SDHCI_CMD_INHIBIT;
    if (cmd->data || cmd->resp_type == MMC_RESP_R1B)
        mask |= SDHCI_DATA_INHIBIT;

    if (cmd->idx == MMC_CMD_STOP_TRANSMISSION)
        mask &= ~SDHCI_DATA_INHIBIT;

    err = sdhci_wait_present_state(base, mask);
    if (err != NO_ERROR)
        return err;

    sdhci_writew(base, SDHCI_INT_ALL_MASK, SDHCI_REG_NORMAL_INT_STATUS);

    mask = SDHCI_INT_RESPONSE;
    if (cmd->resp_type == MMC_RESP_NONE) {
        flags = SDHCI_CMD_RESP_NONE;
    } else if (cmd->resp_type == MMC_RESP_R138) {
        flags = SDHCI_CMD_RESP_LONG | SDHCI_CMD_CRC;
    } else if (cmd->resp_type == MMC_RESP_R1B) {
        flags = SDHCI_CMD_RESP_SHORT_BUSY | SDHCI_CMD_CRC | SDHCI_CMD_INDEX;
        mask |= SDHCI_INT_DATA_END;
    } else {
        flags = SDHCI_CMD_RESP_SHORT | SDHCI_CMD_CRC | SDHCI_CMD_INDEX;
    }

    if (cmd->data) {
        sdhci_writew(base, SDHCI_MAKE_BLKSZ(7, dev->blksize), SDHCI_REG_BLOCK_SIZE);
        sdhci_writew(base, cmd->data->blkcount, SDHCI_REG_BLOCK_COUNT);

        sdhci_writeb(base, 0xe, SDHCI_REG_TIMEOUT_CONTROL);

        if (cmd->data->flags & MMC_DATA_READ)
            mode |= SDHCI_TRNS_READ;

        if (cmd->data->blkcount > 1)
            mode |= SDHCI_TRNS_MULTI | SDHCI_TRNS_BLK_CNT_EN | SDHCI_TRNS_ACMD12;

        sdhci_writew(base, mode, SDHCI_REG_TRANSFER_MODE);

        flags |= SDHCI_CMD_DATA;
    }

    sdhci_writel(base, cmd->arg, SDHCI_REG_ARG);
    sdhci_writew(base, SDHCI_MAKE_CMD(cmd->idx, flags), SDHCI_REG_COMMAND);

    lk_time_t start = current_time();
    uint32_t stat;
    do {
        stat = sdhci_readw(base, SDHCI_REG_NORMAL_INT_STATUS);
        if (stat & SDHCI_INT_ERROR) {
            printf("SDHCI: CMD%d INT Error detected\n", cmd->idx);
            sdhci_dump_present_state(base);
            sdhci_dump_normal_interrupt_status(base);
            sdhci_dump_error_interrupt_status(base);

            sdhci_reset(base, SDHCI_RESET_CMD);
            sdhci_reset(base, SDHCI_RESET_DATA);

            return ERR_IO;
        }
        if (current_time() - start > 1000) {
            printf("SDHCI: CMD%d Timeout\n", cmd->idx);
            sdhci_dump_present_state(base);
            sdhci_dump_normal_interrupt_status(base);
            sdhci_dump_error_interrupt_status(base);
            return ERR_TIMED_OUT;
        }
    } while ((stat & mask) != mask);

    if (cmd->resp_type != MMC_RESP_NONE) {
        sdhci_read_resp(base, cmd);
    }

    if (cmd->data) {
        bool is_read = (cmd->data->flags & MMC_DATA_READ) != 0;

        err = sdhci_transfer_data(base, cmd->data->buffer, dev->blksize, 
                                  cmd->data->blkcount, is_read);
        if (err < 0)
            return err;
    }


    sdhci_writew(base, mask, SDHCI_REG_NORMAL_INT_STATUS);

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

    sdhci_writel(base, 0xFFFFFFFF, SDHCI_INT_ENABLE);
    sdhci_writel(base, 0xFFFFFFFF, SDHCI_SIGNAL_ENABLE);

    return NO_ERROR;
}

static status_t sdhci_fini(struct mmc_device *dev) {
    printf("SDHCI: sdhci_fini called\n");
    return NO_ERROR;
}

static const struct mmc_ops sdhci_ops = {
    .init = sdhci_init,
    .fini = sdhci_fini,
    .send_cmd = sdhci_send_cmd,
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
