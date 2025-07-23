/*
 * Copyright (c) 2025 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#include <stdint.h>

#include <lk/debug.h>
#include <lk/init.h>
#include <lk/list.h>
#include <lk/trace.h>

#include <lib/bio.h>

#include <dev/driver.h>
#include <dev/mmc.h>
#include <dev/mmc_bdev.h>
#include <dev/class/mmc.h>

#define LOCAL_TRACE 1

static struct mmc_device mmc;
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
    dprintf(INFO, "Manufacturer ID: 0x%02X\n", cid->mid);
    dprintf(INFO, "OEM/Application ID: %s\n", cid->oid);
    dprintf(INFO, "Product Name: %s\n", cid->pnm);
    dprintf(INFO, "Product Revision: %u.%u\n", cid->prv_major, cid->prv_minor);
    dprintf(INFO, "Product Serial Number: 0x%08X\n", cid->psn);
    dprintf(INFO, "Manufacturing Date: %04u-%02u\n", cid->mdt_y, cid->mdt_m);
    dprintf(INFO, "CRC7 Checksum: 0x%02X\n", cid->crc);
}

status_t mmc_go_idle_state(struct mmc_device *mmc_dev) {
    struct mmc_cmd cmd = (struct mmc_cmd) {
        .idx = MMC_CMD_GO_IDLE_STATE,
        .resp_type = MMC_RESP_NONE,
        .arg = 0,
    };

    status_t err = class_mmc_send_cmd(mmc_dev->dev, &cmd);
    if (err < 0) {
        LTRACEF("Failed to send command, cmd: %d\n", cmd.idx);
        return err;
    }

    return err;
}

status_t mmc_all_send_cid(struct mmc_device *mmc_dev) {
    struct mmc_cmd cmd = (struct mmc_cmd) {
        .idx = MMC_CMD_ALL_SEND_CID,
        .resp_type = MMC_RESP_R138,
        .arg = 0,
    };

    status_t err = class_mmc_send_cmd(mmc_dev->dev, &cmd);
    if (err < 0) {
        LTRACEF("Failed to send command, cmd: %d\n", cmd.idx);
        return err;
    }

    parse_cid(cmd.resp, &mmc_dev->cid);

    return err;
}

status_t mmc_send_op_cond(struct mmc_device *mmc_dev) {
    struct mmc_cmd cmd = (struct mmc_cmd) {
        .idx = SD_CMD_SEND_OP_COND,
        .resp_type = MMC_RESP_R48,
        .arg = 0x40300000,
    };

    status_t err = class_mmc_send_cmd(mmc_dev->dev, &cmd);
    if (err < 0) {
        LTRACEF("Failed to send command, cmd: %d\n", cmd.idx);
        return err;
    }
    if ((cmd.resp[0] & OCR_BUSY_MASK) == 0) {
        LTRACEF("Card is busy or host ommited voltage range\n");
        return err;
    }

    return err;
}

status_t mmc_send_csd(struct mmc_device *mmc_dev) {
    struct mmc_cmd cmd = (struct mmc_cmd) {
        .idx = MMC_CMD_SEND_CSD,
        .resp_type = MMC_RESP_R138,
        .arg = 0,
    };

    status_t err = class_mmc_send_cmd(mmc_dev->dev, &cmd);
    if (err < 0) {
        LTRACEF("Failed to send command, cmd: %d\n", cmd.idx);
        return err;
    }

    mmc_dev->csd.c_size = 0;
    mmc_dev->csd.read_blk_ln = 0;
    mmc_dev->csd.structure = 0;

    return err;
}

status_t mmc_read_single_blk(struct device *mmc_dev, uint32_t block) {
    struct mmc_cmd cmd = (struct mmc_cmd) {
        .idx = MMC_CMD_READ_SINGLE_BLK,
        .resp_type = MMC_RESP_R48,
        .arg = block,
    };

    status_t err = class_mmc_send_cmd(mmc_dev, &cmd);
    if (err < 0) {
        LTRACEF("Failed to send command, cmd: %d, reason: %d\n", cmd.idx, err);
        return err;
    }

    return err;
}

status_t mmc_read_multiple_blk(struct device *mmc_dev, uint32_t block) {
    struct mmc_cmd cmd = {
        .idx = MMC_CMD_READ_MULTIPLE_BLK,
        .resp_type = MMC_RESP_R48,
        .arg = block,
    };

    status_t err = class_mmc_send_cmd(mmc_dev, &cmd);
    if (err < 0) {
        LTRACEF("Failed to send command, cmd: %d, reason: %d\n", cmd.idx, err);
        return err;
    }

    return err;
}

status_t mmc_write_single_blk(struct device *mmc_dev, uint32_t block) {
    struct mmc_cmd cmd = (struct mmc_cmd) {
        .idx = MMC_CMD_WRITE_SINGLE_BLK,
        .resp_type = MMC_RESP_R48,
        .arg = block,
    };

    status_t err = class_mmc_send_cmd(mmc_dev, &cmd);
    if (err < 0) {
        LTRACEF("Failed to send command, cmd: %d, reason: %d\n", cmd.idx, err);
        return err;
    }

    return err;
}

status_t mmc_write_multiple_blk(struct device *mmc_dev, uint32_t block) {
    struct mmc_cmd cmd = (struct mmc_cmd) {
        .idx = MMC_CMD_WRITE_MULTIPLE_BLK,
        .resp_type = MMC_RESP_R48,
        .arg = block,
    };
    status_t err = class_mmc_send_cmd(mmc_dev, &cmd);
    if (err < 0) {
        LTRACEF("Failed to send command, cmd: %d, reason: %d\n", cmd.idx, err);
        return err;
    }

    return err;
}

status_t sd_set_app_cmd(struct mmc_device *mmc_dev) {
    struct mmc_cmd cmd = (struct mmc_cmd) {
        .idx = SD_APP_CMD,
        .resp_type = MMC_RESP_R48,
        .arg = 0,
    };

    status_t err = class_mmc_send_cmd(mmc_dev->dev, &cmd);
    if (err < 0) {
        LTRACEF("Failed to send command, cmd: %d\n", cmd.idx);
        return err;
    }
    if ((cmd.resp[0] & CARD_STATUS_ACMD) == 0) {
        LTRACEF("Failed to enable Application Command\n");
        return err;
    }

    return err;
}

static void mmc_init(uint level) {
    status_t err;
    struct device *dev;
    struct device_list_entry *entry;

    list_initialize(&mmc_devices);

    err = device_get_list_type("mmc", &mmc_devices);
    if (err < 0) {
        dprintf(INFO, "Failed to get list of MMC devices, reason: %d\n", err);
        return;
    }

    if (list_length(&mmc_devices) > 1) {
        dprintf(INFO, "Failed to init mmc devices, mmc driver supports only one SD card\n");
        return;
    }

    entry = list_peek_head_type(&mmc_devices, struct device_list_entry, node);
    dev = entry->dev;

    err = device_init(dev);
    if (err < 0)
        return;

    mmc.dev = dev;

    err = mmc_go_idle_state(&mmc);
    if (err < 0)
        return;

    err = sd_set_app_cmd(&mmc);
    if (err < 0)
        return;

    err = mmc_send_op_cond(&mmc);
    if (err < 0)
        return;

    err = mmc_all_send_cid(&mmc);
    if (err < 0)
        return;

    trace_cid(&mmc.cid);

    mmc_bdev_init(&mmc);
}

LK_INIT_HOOK(mmc, &mmc_init, LK_INIT_LEVEL_PLATFORM);
