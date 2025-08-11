/*
 * Copyright (c) 2025 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#include <stdint.h>
#include <string.h>

#include <lk/init.h>
#include <lk/list.h>
#include <lk/trace.h>

#include <dev/driver.h>
#include <dev/mmc.h>
#include <dev/class/mmc.h>

#define LOCAL_TRACE 1

#define CARD_STATUS_ACMD    (1 << 5)

/* Card registers mask */
#define OCR_VOLTAGE_MASK    (0x00FFFF80)
#define OCR_ACCESS_MODE_MASK    (0x60000000)
#define OCR_BUSY_MASK       (0x80000000)

static struct list_node mmc_devices;

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

    /* year offset from 2000 */
    cid->mdt_y = 2000 + ((mdt >> 4) & 0xFF);
    /* month 1-12 */
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

static void mmc_init(uint level) {
    status_t err;
    struct device *dev = NULL;
    struct device_list_entry *entry;
    struct mmc_cmd cmd = { 0 };
    struct mmc_cid cid = { 0 };

    list_initialize(&mmc_devices);

    err = device_get_list_type("mmc", &mmc_devices);
    if (err < 0) {
        LTRACEF("Failed to get list of MMC devices, reason: %d\n", err);
        return;
    }

    if (list_length(&mmc_devices) > 1) {
        LTRACEF("Failed to init mmc devices, mmc driver supports only one MMC/SD card\n");
        return;
    }

    entry = list_peek_head_type(&mmc_devices, struct device_list_entry, node);
    dev = entry->dev;

    err = device_init(dev);
    if (err < 0)
        return;

    cmd = (struct mmc_cmd) {
        .idx = MMC_CMD_GO_IDLE_STATE,
        .resp_type = MMC_RESP_NONE,
        .arg = 0,
    };
    err = class_mmc_send_cmd(dev, &cmd);
    if (err < 0) {
        LTRACEF("Failed to send command, cmd: %d\n", cmd.idx);
        return;
    }

    cmd = (struct mmc_cmd) {
        .idx = SD_APP_CMD,
        .resp_type = MMC_RESP_R48,
        .arg = 0,
    };
    err = class_mmc_send_cmd(dev, &cmd);
    if (err < 0) {
        LTRACEF("Failed to send command, cmd: %d\n", cmd.idx);
        return;
    }
    if ((cmd.resp[0] & CARD_STATUS_ACMD) == 0) {
        LTRACEF("Failed to enable Application Command\n");
        return;
    }

    cmd = (struct mmc_cmd) {
        .idx = SD_CMD_SEND_OP_COND,
        .resp_type = MMC_RESP_R48,
        .arg = 0,
    };
    err = class_mmc_send_cmd(dev, &cmd);
    if (err < 0) {
        LTRACEF("Failed to send command, cmd: %d\n", cmd.idx);
        return;
    }
    if ((cmd.resp[0] & OCR_BUSY_MASK) == 0) {
        LTRACEF("Card is busy or host ommited voltage range\n");
        return;
    }

    cmd = (struct mmc_cmd) {
        .idx = MMC_CMD_ALL_SEND_CID,
        .resp_type = MMC_RESP_R138,
        .arg = 0,
    };
    err = class_mmc_send_cmd(dev, &cmd);
    if (err < 0) {
        LTRACEF("Failed to send command, cmd: %d\n", cmd.idx);
        return;
    }

    parse_cid(cmd.resp, &cid);
    trace_cid(&cid);

    char dst[512] = { 0 };
    struct mmc_xfer_info info = (struct mmc_xfer_info) {
        .buffer = dst,
        .blkcount = 512,
        .blksize = 1,
    };
    class_mmc_read(dev, &info);
    dst[511] = '\0';
    LTRACEF("Before write: %s\n", dst);

    char src[512] = "New write text!!";
    struct mmc_xfer_info write_info = (struct mmc_xfer_info) {
        .buffer = src,
        .blkcount = 512,
        .blksize = 1,
    };
    class_mmc_write(dev, &write_info);

    memset(dst, 0, 512);
    struct mmc_xfer_info rinfo = (struct mmc_xfer_info) {
        .buffer = dst,
        .blkcount = 512,
        .blksize = 1,
    };
    class_mmc_read(dev, &rinfo);
    dst[511] = '\0';
    LTRACEF("After write: %s\n", dst);
}

LK_INIT_HOOK(mmc, &mmc_init, LK_INIT_LEVEL_PLATFORM);
