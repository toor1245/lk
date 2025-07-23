/*
 * Copyright (c) 2025 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
#include "include/dev/mmc/pl180.h"

#include <dev/driver.h>
#include <dev/mmc.h>
#include <dev/class/mmc.h>
#include <string.h>
#include <stdbool.h>
#include <dev/uart.h>
#include <lk/err.h>
#include <lk/reg.h>
#include <lk/trace.h>
#include <platform/time.h>

#include "mci_regs.h"

#define LOCAL_TRACE 1 

/*
#define CARD_STATUS_ACMD	(1 << 5)

// Card registers mask
#define OCR_VOLTAGE_MASK	(0x00FFFF80)
#define OCR_ACCESS_MODE_MASK	(0x60000000)
#define OCR_BUSY_MASK		(0x80000000)

#define OCR_ACCESS_BYTE		(0x0)
#define OCR_ACCESS_SECTOR	(0x2)

#define RESP_TYPE_NONE 0
#define RESP_TYPE_SHORT 1
#define RESP_TYPE_LONG 2
*/

//static struct pl180_config config;

static inline void delay(lk_time_t delay) {
    lk_time_t start = current_time();

    while (start + delay > current_time());
}

static inline void write_mci_reg(uintptr_t base, size_t offset, uint32_t val) {
    mmio_write32((uint32_t *)(base + offset), val);
}

static inline uint32_t read_mci_reg(uintptr_t base, size_t offset) {
    return mmio_read32((uint32_t *)(base + offset));
}

static void trace_cmd_resp(struct mmc_cmd *cmd) {
    /* Print MCI RESP0 for R48 otherwise print all RESP MCI registers (R138) */
    uint32_t count = cmd->resp_type == MMC_RESP_R48 ? 1 : 4;

    LTRACEF("cmd index: %d\n", cmd->idx);

    for (uint32_t i = 0; i < count; i++) {
        LTRACEF("resp[%d]: 0x%x\n", i, cmd->resp[i]);
    }
}

/*
static uint64_t extract_bits(uint64_t reg, uint64_t msb, uint64_t lsb) {
  const uint64_t bits = msb - lsb + 1ULL;
  const uint64_t mask = (1ULL << bits) - 1ULL;
  return (reg >> lsb) & mask;
}

static void parse_cid(uint32_t resp[4], struct mmc_cid *cid) {
    cid->mid = (resp[0] >> 24) & 0xFF;

    cid->oid[0] = (char)((resp[0] >> 16) & 0xFF);
    cid->oid[1] = (char)((resp[0] >> 8) & 0xFF);
    cid->oid[2] = '\0';

    cid->pnm[0] = (char)(resp[0] & 0xFF);
    cid->pnm[1] = (char)((resp[1] >> 24) & 0xFF);
    cid->pnm[2] = (char)((resp[1] >> 16) & 0xFF);
    cid->pnm[3] = (char)((resp[1] >> 8) & 0xFF);
    cid->pnm[4] = (char)(resp[1] & 0xFF);
    cid->pnm[5] = '\0';

    uint8_t prv = (resp[2] >> 24) & 0xFF;
    cid->prv_major = (prv >> 4) & 0x0F;
    cid->prv_minor = prv & 0x0F;

    cid->psn = ((resp[3] >> 24) & 0xFF) << 24 | (resp[2] & 0xFFFFFF);

    uint16_t mdt = (resp[3] >> 8) & 0x0FFF;
    cid->mdt_y = 2000 + ((mdt >> 4) & 0xFF);
    cid->mdt_m = mdt & 0x0F;			
    cid->crc = (resp[3] >> 1) & 0x7F;
}

void trace_cid(const struct mmc_cid *cid) {
    LTRACEF("Manufacturer ID: 0x%02X\n", cid->mid);
    LTRACEF("OEM/Application ID: %s\n", cid->oid);
    LTRACEF("Product Name: %s\n", cid->pnm);
    LTRACEF("Product Revision: %u.%u\n", cid->prv_major, cid->prv_minor);
    LTRACEF("Product Serial Number: 0x%08X\n", cid->psn);
    LTRACEF("Manufacturing Date: %04u-%02u\n", cid->mdt_y, cid->mdt_m);
    LTRACEF("CRC7 Checksum: 0x%02X\n", cid->crc);
}

int send_cmd(uint32_t cmd_idx, uint32_t arg, int resp_type, uint32_t *resp) {
    uintptr_t base = config.base;
    uint32_t clr_mask = 0;
    uint32_t cmd = 0;
    uint32_t host_status = 0;
    bool has_resp = false;

    cmd = (cmd_idx & 0xFF) | (1 << 10);
    if (resp_type == RESP_TYPE_SHORT) {
	has_resp = true;
        cmd |= (1 << 6);
    }
    if (resp_type == RESP_TYPE_LONG) {
	has_resp = true;
        cmd |= (1 << 6);
        cmd |= (1 << 7);
    }


    LTRACEF("arg: 0x%x\n", arg); 
    write_mci_reg(base, MCI_ARG, arg);
    delay(300);
    write_mci_reg(base, MCI_CMD, cmd);

    clr_mask = MCI_CLR_CMD_TIMEOUT | MCI_CLR_CMD_CRC_FAIL;
    clr_mask |= has_resp ? MCI_CLR_CMD_RESP_END : MCI_CLR_CMD_SENT;

    do {
        host_status = read_mci_reg(base, MCI_STAT) & clr_mask;
	LTRACEF("host_status: 0x%x\n", host_status);	
    } while(!host_status);

    write_mci_reg(base, MCI_CLR, clr_mask);
   
    if (host_status & MCI_CLR_CMD_TIMEOUT) {
        return -1;    
    }
    if (host_status & MCI_CLR_CMD_CRC_FAIL) {
        return -1;
    }

    if (has_resp) {
        resp[0] = read_mci_reg(base, MCI_RESP0);
        resp[1] = read_mci_reg(base, MCI_RESP1);
        resp[2] = read_mci_reg(base, MCI_RESP2);
        resp[3] = read_mci_reg(base, MCI_RESP3);

	trace_cmd_resp(cmd_idx, resp, 4);
    }

    return 0;
}

#define BLKSIZE_MASK 0x000000F0

static int pl180_read(uint32_t *dst, uint32_t blkcount, uint32_t blksize) {
    uintptr_t base = config.base;
    uint64_t xfercount = blkcount * blksize;
    uint32_t *tempbuff = dst;
    int res = 0;
    uint32_t resp[4] = { 0 };

    // log2 block size
    uint32_t shift = 0;
    for (int s = blksize; s > 1; s >>= 1) shift++;

    uint32_t dctrl_reg = (1 << 0) | (1 << 1); // ENABLE + Read
    dctrl_reg |= (shift << 4);               // block size

    write_mci_reg(base, MCI_DCNT, blkcount * blksize);  // MUST be before DCTRL
    write_mci_reg(base, MCI_DCTRL, dctrl_reg);          // start transfer

    res = send_cmd(MMC_CMD_READ_SINGLE_BLK, 0, RESP_TYPE_SHORT, resp);
    if (res != 0) {
	LTRACEF("Failed to send command, cmd: %d\n", MMC_CMD_READ_SINGLE_BLK);
	return -1;
    }

    // Wait for RX_ACTIVE
    while (!(read_mci_reg(base, MCI_STAT) & MCI_STAT_RX_ACTIVE));

    // FIFO read
    while (xfercount >= sizeof(uint32_t)) {
        uint32_t status = read_mci_reg(base, MCI_STAT);
        uint32_t status_err = status & (MCI_STAT_DATA_CRC_FAIL | MCI_STAT_DATA_TIME_OUT | MCI_STAT_RX_OVERRUN);

        if (status_err)
            return -1;

        if (status & MCI_STAT_RX_DATA_AVLBL) {
            *tempbuff = read_mci_reg(base, MCI_DFIFO);
            LTRACEF("read fifo value: 0x%08x\n", *tempbuff);
            tempbuff++;
            xfercount -= sizeof(uint32_t);
        }
    }

    return 0;
}

void pl180_init2(struct pl180_config *cfg) {
    config = *cfg;
    struct mmc_cid cid = { 0 };
    uintptr_t base = config.base;
    uint32_t resp[4] = { 0 };
    uint32_t arg = 0;
    int res = 0;

    // power up
    write_mci_reg(base, MCI_PWR, 0xBF);

    uint32_t clk = (1 << 8); // Enable clock
    clk |= (0xC6 << 0);      // CLKDIV = 0xC6 (i.e. slow clock)
    write_mci_reg(base, MCI_CLK, clk);
    delay(300);

    LTRACEF("pl180_init started\n");

    send_cmd(MMC_CMD_GO_IDLE_STATE, RESP_TYPE_NONE, false, NULL);

    send_cmd(SD_APP_CMD, 0, true, resp);
    if ((resp[0] & CARD_STATUS_ACMD) == 0) {
	LTRACEF("Failed to enable Application Command\n");
    }

    arg = OCR_VOLTAGE_MASK | OCR_ACCESS_MODE_MASK;

    send_cmd(SD_CMD_SEND_OP_COND, arg, RESP_TYPE_SHORT, resp);
    if ((resp[0] & OCR_BUSY_MASK) == 0) {
	LTRACEF("Card is busy or host ommited voltage range\n");
	return;
    }

    res = send_cmd(MMC_CMD_ALL_SEND_CID, 0, RESP_TYPE_LONG, resp);
    if (res != 0) {
	LTRACEF("Failed to send command, cmd: %d\n", MMC_CMD_ALL_SEND_CID);
	return;
    }

    parse_cid(resp, &cid);
    trace_cid(&cid);

    res = send_cmd(MMC_CMD_SET_RELATIVE_ADDR, 10 << 16, RESP_TYPE_SHORT, resp);
    if (res != 0) {
	LTRACEF("Failed to send command, cmd: %d\n", MMC_CMD_ALL_SEND_CID);
	return;
    }

    uint32_t rca = extract_bits(resp[0], 31, 16);
    uint32_t card_status = extract_bits(resp[0], 15, 0);
    uint32_t r1b = resp[0];   
 
    LTRACEF("RCA: %d\n", rca);
    LTRACEF("Card status: %d\n", card_status);

    memset(resp, 0, sizeof(uint32_t) * 4);
    res = send_cmd(7, rca << 16, RESP_TYPE_SHORT, resp);
    if (res != 0) {
	LTRACEF("Failed to send command, cmd: %d\n", 7);
	return;
    }

    uint32_t buffer[128] = { 0 };
    pl180_read(buffer, 1, 512);
    char *str = (char *)buffer;
    str[511] = '\0';

    LTRACEF("str: %s\n", str);
}
*/

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

    LTRACEF("cmd arg: 0x%x\n", cmd->arg); 
    write_mci_reg(base, MCI_ARG, cmd->arg);
    delay(300);

    mci_cmd = (cmd->idx & 0xFF) | (1 << 10);
    if (cmd->resp_type == MMC_RESP_R48) {
        LTRACEF("cmd resp width: R48");
        mci_cmd |= (1 << 6);
    }
    if (cmd->resp_type == MMC_RESP_R138) {
        LTRACEF("cmd resp width: R138");
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
