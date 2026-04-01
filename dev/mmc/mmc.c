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
#include <lk/pow2.h>

#include <lib/bio.h>

#include <dev/driver.h>
#include <dev/mmc.h>
#include <dev/mmc_bdev.h>
#include <dev/class/mmc.h>

#define LOCAL_TRACE 1

static struct mmc_device mmc;
static struct list_node mmc_devices;

static void parse_cid(const uint32_t resp[4], struct mmc_cid *cid) {
    cid->mid = (uint8_t)extract_bit_range128(resp, 127, 120);

    cid->oid[0] = (char)extract_bit_range128(resp, 119, 112);
    cid->oid[1] = (char)extract_bit_range128(resp, 111, 104);
    cid->oid[2] = '\0';

    cid->pnm[0] = (char)extract_bit_range128(resp, 103, 96);
    cid->pnm[1] = (char)extract_bit_range128(resp, 95, 88);
    cid->pnm[2] = (char)extract_bit_range128(resp, 87, 80);
    cid->pnm[3] = (char)extract_bit_range128(resp, 79, 72);
    cid->pnm[4] = (char)extract_bit_range128(resp, 71, 64);
    cid->pnm[5] = '\0';

    uint8_t prv = (uint8_t)extract_bit_range128(resp, 63, 56);
    cid->prv_major = (prv >> 4) & 0x0F;
    cid->prv_minor = prv & 0x0F;

    cid->psn = extract_bit_range128(resp, 55, 24);

    uint16_t mdt = (uint16_t)extract_bit_range128(resp, 19, 8);
    cid->mdt_y = 2000 + ((mdt >> 4) & 0xFF);  // year = 2000 + high 8 bits
    cid->mdt_m = mdt & 0x0F;                  // month = low 4 bits

    cid->crc = (uint8_t)extract_bit_range128(resp, 7, 1);
}

static void parse_csd(const uint32_t resp[4], struct mmc_csd *csd) {
    csd->structure = extract_bit_range128(resp, 127, 126);
    csd->read_blk_ln = extract_bit_range128(resp, 83, 80);
    csd->c_size_mult = extract_bit_range128(resp, 49, 47);

    switch(csd->structure) {
        case MMC_CSD_V1:
	    csd->c_size = extract_bit_range128(resp, 73, 62);
	    break;
	case MMC_CSD_V2:
	    csd->c_size = extract_bit_range128(resp, 69, 48);
	    break;
	case MMC_CSD_V3:
	    csd->c_size = extract_bit_range128(resp, 75, 48);
	    break;
    }
}

static void mmc_set_mem_caps(struct mmc_device *mmc_dev) {
    struct mmc_csd *csd = &mmc_dev->csd;

    switch(csd->structure) {
        case MMC_CSD_V1: {
            size_t mult = valpow2(csd->c_size_mult + 2);
            size_t blocknr = ((size_t)csd->c_size + 1) * mult;
            size_t block_len = valpow2(csd->read_blk_ln);
            mmc_dev->mem_capacity = blocknr * block_len;
	    mmc_dev->blksize = block_len;
	    mmc_dev->blkcount = mmc_dev->mem_capacity / mmc_dev->blksize;
	    break;
	}
	case MMC_CSD_V2:
	case MMC_CSD_V3:
            mmc_dev->mem_capacity = ((size_t)csd->c_size + 1) * 512 * 1024;
	    mmc_dev->blksize = 512;
	    mmc_dev->blkcount = mmc_dev->mem_capacity / mmc_dev->blksize;
	    break;
    }
}

void print_mmc_info(struct mmc_device *mmc_dev) {
    struct mmc_cid *cid = &mmc_dev->cid;
    struct mmc_csd *csd = &mmc_dev->csd;

    dprintf(INFO, "\tManufacturer ID: 0x%02X\n", cid->mid);
    dprintf(INFO, "\tOEM/Application ID: %s\n", cid->oid);
    dprintf(INFO, "\tProduct Name: %s\n", cid->pnm);
    dprintf(INFO, "\tProduct Revision: %u.%u\n", cid->prv_major, cid->prv_minor);
    dprintf(INFO, "\tProduct Serial Number: 0x%08X\n", cid->psn);
    dprintf(INFO, "\tManufacturing Date: %04u-%02u\n", cid->mdt_y, cid->mdt_m);
    dprintf(INFO, "\tCRC7 Checksum: 0x%02X\n", cid->crc);

    dprintf(INFO, "\tCSD structure: %d\n", csd->structure);
    dprintf(INFO, "\tCSD C_SIZE: %d\n", csd->c_size);
    dprintf(INFO, "\tCSD C_SIZE_MULT: %d\n", csd->c_size_mult);
    dprintf(INFO, "\tCSD READ_BLK_LEN: %d\n", csd->read_blk_ln);
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

    parse_csd(cmd.resp, &mmc_dev->csd);
    mmc_set_mem_caps(mmc_dev); 
    dprintf(INFO, "SD memory capacity: %lu\n", mmc_dev->mem_capacity);

    return err;
}

status_t mmc_stop_transmission(struct device *dev) {
    struct mmc_cmd cmd = (struct mmc_cmd) {
        .idx = MMC_CMD_STOP_TRANSMISSION,
        .resp_type = MMC_RESP_R48,
        .arg = 0,
    };

    status_t err = class_mmc_send_cmd(dev, &cmd);
    if (err < 0) {
        LTRACEF("Failed to send command, cmd: %d, reason: %d\n", cmd.idx, err);
        return err;
    }

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

    err = mmc_send_csd(&mmc);
    if (err < 0)
        return;

    print_mmc_info(&mmc);
    mmc_bdev_init(&mmc);
}

LK_INIT_HOOK(mmc, &mmc_init, LK_INIT_LEVEL_PLATFORM);
