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

#include <platform/time.h>

#include "mci.h"

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
    } while(!host_status);

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

static struct mmc_ops pl180_ops = {
    .std = {
        .init = pl180_init,
    },
    .send_cmd = pl180_send_cmd,
};

DRIVER_EXPORT(mmc, &pl180_ops.std);
