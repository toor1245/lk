/*
 * Copyright (c) 2025 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
#include "include/dev/mmc/pl180.h"

#include <string.h>
#include <stdbool.h>

#include <dev/driver.h>
#include <dev/mmc.h>
#include <dev/class/mmc.h>
#include <dev/uart.h>

#include <lk/err.h>
#include <lk/trace.h>
#include <lk/pow2.h>

#include <platform/time.h>

#include "mci.h"

#define FIFO_BURST_SIZE 8

#define LOCAL_TRACE 1

static inline void delay(lk_time_t delay) {
    lk_time_t start = current_time();

    while (start + delay > current_time());
}

static void trace_cmd_resp(struct mmc_cmd *cmd) {
    /* Print MCI RESP0 for R48 otherwise print all RESP MCI registers (R138) */
    uint32_t count = cmd->resp_type == MMC_RESP_R48 ? 1 : 4;

    for (uint32_t i = 0; i < count; i++) {
        LTRACEF("resp[%d]: 0x%x\n", i, cmd->resp[i]);
    }
}

static status_t pl180_init(struct device *dev) {
    const struct pl180_config *config = dev->config;
    uintptr_t base = config->base;

    write_mci_reg(base, MCI_PWR, 0xBF);

    uint32_t clk = (1 << 8); // Enable clock
    clk |= (0xC6 << 0);      // CLKDIV = 0xC6 (i.e. slow clock)
    write_mci_reg(base, MCI_CLK, clk);
    delay(300);

    return NO_ERROR;
}

static status_t pl180_send_cmd(struct device *dev, struct mmc_cmd *cmd) {
    const struct pl180_config *config = dev->config;
    uintptr_t base = config->base;
    uint32_t clr_mask = 0;
    uint32_t mci_cmd = 0;
    uint32_t host_status = 0;
    bool has_resp = (cmd->resp_type == MMC_RESP_R48) || (cmd->resp_type == MMC_RESP_R138);

    LTRACEF("cmd idx: 0x%x\n", cmd->idx);
    LTRACEF("cmd arg: 0x%x\n", cmd->arg);

    write_mci_reg(base, MCI_ARG, cmd->arg);
    delay(300);

    mci_cmd = (cmd->idx & 0xFF) | (1 << 10);
    if (cmd->resp_type == MMC_RESP_R48) {
        LTRACEF("cmd resp width: R48\n");
        mci_cmd |= (1 << 6);
    }
    if (cmd->resp_type == MMC_RESP_R138) {
        LTRACEF("cmd resp width: R138\n");
        mci_cmd |= (1 << 6) | (1 << 7);
    }

    write_mci_reg(base, MCI_CMD, mci_cmd);

    clr_mask = MCI_CLR_CMD_TIMEOUT | MCI_CLR_CMD_CRC_FAIL;
    clr_mask |= has_resp ? MCI_CLR_CMD_RESP_END : MCI_CLR_CMD_SENT;

    do {
        host_status = read_mci_reg(base, MCI_STAT) & clr_mask;
        LTRACEF("host_status: 0x%x\n", host_status);
    } while (!host_status);

    write_mci_reg(base, MCI_CLR, clr_mask);

    if (host_status & MCI_CLR_CMD_TIMEOUT) {
        return ERR_TIMED_OUT;
    }
    if (host_status & MCI_CLR_CMD_CRC_FAIL) {
        return ERR_CRC_FAIL;
    }

    if (has_resp) {
        cmd->resp[0] = read_mci_reg(base, MCI_RESP0);
        cmd->resp[1] = read_mci_reg(base, MCI_RESP1);
        cmd->resp[2] = read_mci_reg(base, MCI_RESP2);
        cmd->resp[3] = read_mci_reg(base, MCI_RESP3);

        trace_cmd_resp(cmd);
    }

    return NO_ERROR;
}

static status_t pl180_read_fifo(uintptr_t base, char *dst, uint64_t xfercount) {
    uint32_t status_err_mask = MCI_STAT_DATA_CRC_FAIL | MCI_STAT_DATA_TIME_OUT | MCI_STAT_RX_OVERRUN;
    char *tmp = dst;

    while (xfercount >= sizeof(uint32_t)) {
        uint32_t status = read_mci_reg(base, MCI_STAT);
        uint32_t status_err = status & status_err_mask;
        if (status_err)
            return ERR_IO;

        if (status & MCI_STAT_RX_DATA_AVLBL) {
            uint32_t reg = read_mci_reg(base, MCI_DFIFO);
            memcpy(tmp, &reg, sizeof(uint32_t));
            tmp += sizeof(uint32_t);
            xfercount -= sizeof(uint32_t);
        }
    }

    return NO_ERROR;
}

static status_t pl180_read(struct device *dev, struct mmc_xfer_info *info) {
    const struct pl180_config *config = dev->config;
    uint64_t xfercount = info->blkcount * info->blksize;
    status_t res = NO_ERROR;
    struct mmc_cmd cmd = { 0 };

    uint32_t shift = log2_uint(info->blksize);
    uint32_t dctrl_reg = (1 << 0) | (1 << 1); // ENABLE + Read
    dctrl_reg |= (shift << 4);
    write_mci_reg(config->base, MCI_DCTRL, dctrl_reg);

    cmd = (struct mmc_cmd) {
        .idx = MMC_CMD_READ_SINGLE_BLK,
        .resp_type = MMC_RESP_R48,
        .arg = 0,
    };
    res = class_mmc_send_cmd(dev, &cmd);
    if (res < 0) {
        LTRACEF("Failed to send command, cmd: %d, reason: %d\n", cmd.idx, res);
        return res;
    }

    return pl180_read_fifo(config->base, info->buffer, xfercount);
}

static status_t pl180_write_fifo(uintptr_t base, char *dst, uint64_t xfercount) {
    uint32_t status_err_mask = MCI_STAT_DATA_CRC_FAIL | MCI_STAT_DATA_TIME_OUT;
    uint32_t *tmp = (uint32_t *)dst;

    while (xfercount) {
        uint32_t status = read_mci_reg(base, MCI_STAT);
        uint32_t status_err = status & status_err_mask;
        if (status_err)
            return ERR_IO;

        if (!(status & MCI_STAT_TX_FIFO_HALF_EMPTY))
            continue;

        if (xfercount >= FIFO_BURST_SIZE * sizeof(uint32_t)) {
            for (uint32_t i = 0; i < FIFO_BURST_SIZE; i++) {
                write_mci_reg(base, MCI_DFIFO, tmp[i]);
            }
            tmp += FIFO_BURST_SIZE;
            xfercount -= FIFO_BURST_SIZE * sizeof(uint32_t);
        } else {
            while (xfercount >= sizeof(uint32_t)) {
                write_mci_reg(base, MCI_DFIFO, *tmp);
                tmp++;
                xfercount -= sizeof(uint32_t);
            }
        }
    }

    return NO_ERROR;
}

static status_t pl180_write(struct device *dev, struct mmc_xfer_info *info) {
    const struct pl180_config *config = dev->config;
    uint64_t xfercount = info->blkcount * info->blksize;
    status_t res = NO_ERROR;
    struct mmc_cmd cmd = { 0 };
    uint32_t shift = log2_uint(info->blksize);

    uint32_t dctrl_reg = (1 << 0); // ENABLE + Write
    dctrl_reg |= (shift << 4);
    write_mci_reg(config->base, MCI_DCTRL, dctrl_reg);

    cmd = (struct mmc_cmd) {
        .idx = MMC_CMD_WRITE_SINGLE_BLK,
        .resp_type = MMC_RESP_R48,
        .arg = 0,
    };
    res = class_mmc_send_cmd(dev, &cmd);
    if (res < 0) {
        LTRACEF("Failed to send command, cmd: %d, reason: %d\n", cmd.idx, res);
        return res;
    }

    return pl180_write_fifo(config->base, info->buffer, xfercount);
}

static struct mmc_ops pl180_ops = {
    .std = {
        .init = pl180_init,
    },
    .send_cmd = pl180_send_cmd,
    .read = pl180_read,
    .write = pl180_write,
};

DRIVER_EXPORT(mmc, &pl180_ops.std);
