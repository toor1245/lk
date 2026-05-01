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

#include <lk/debug.h>
#include <lk/init.h>
#include <lk/list.h>
#include <lk/trace.h>
#include <lk/pow2.h>
#include <lk/err.h>

#include <lib/bio.h>

#include <lib/bio.h>

#include <dev/mmc.h>
#include <dev/mmc_bdev.h>

#define LOCAL_TRACE 1

static struct mmc_device mmc;

static void parse_cid(const uint32_t resp[4], struct mmc_cid *cid) {
    cid->mid = (uint8_t)extract_bit_range128(resp, 127, 120);

    cid->oid[0] = (char)extract_bit_range128(resp, 111, 104);
    cid->oid[1] = '\0';

    cid->pnm[0] = (char)extract_bit_range128(resp, 103, 96);
    cid->pnm[1] = (char)extract_bit_range128(resp, 95, 88);
    cid->pnm[2] = (char)extract_bit_range128(resp, 87, 80);
    cid->pnm[3] = (char)extract_bit_range128(resp, 79, 72);
    cid->pnm[4] = (char)extract_bit_range128(resp, 71, 64);
    cid->pnm[5] = '\0';

    uint8_t prv = (uint8_t)extract_bit_range128(resp, 55, 48);
    cid->prv_major = (prv >> 4) & 0x0F;
    cid->prv_minor = prv & 0x0F;

    cid->psn = extract_bit_range128(resp, 47, 16);


    uint8_t mdt = (uint8_t)extract_bit_range128(resp, 15, 8);
    cid->mdt_m = ((mdt >> 4) & 0x0F);
    cid->mdt_y = 1997 + (mdt & 0x0F);

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
    case MMC_CSD_EXT:
        break;
    }
}

static void parse_ext_csd(char *buffer, struct mmc_ext_csd *ext_csd) {
    ext_csd->structure = extract_byte_range512(buffer, 194, 1);
    ext_csd->sec_count = extract_byte_range512(buffer, 212, 4);
}

status_t mmc_select_card(struct mmc_device *mmc_dev) {
    struct mmc_cmd cmd = {
        .idx = MMC_CMD_SELECT_CARD,
        .resp_type = MMC_RESP_R1B,
        .arg = 1 << 0x10,
        .has_data = false
    };

    status_t err = mmc_dev->host->ops->send_cmd(mmc_dev, &cmd);
    if (err < 0) {
        printf("SDHCI: Failed to select card (CMD7)\n");
        return err;
    }

    return NO_ERROR;
}

static status_t mmc_set_mem_caps(struct mmc_device *mmc_dev) {
    status_t err ;
    uint32_t resp[4] = { 0 };

    err = mmc_send_csd(mmc_dev, resp);
    if (err < 0)
        return err;

    parse_csd(resp, &mmc_dev->csd);

    struct mmc_csd *csd = &mmc_dev->csd;
    mmc_dev->blksize = 512;

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
        mmc_dev->blkcount = mmc_dev->mem_capacity / mmc_dev->blksize;

        break;
    case MMC_CSD_EXT:
        uint8_t *ext_csd_buf = malloc(mmc_dev->blksize);
        if (!ext_csd_buf) {
            return ERR_NO_MEMORY;
        }

        err = mmc_select_card(&mmc);
        if (err < 0)
            return err;

        err = mmc_send_ext_csd(mmc_dev, ext_csd_buf);
        if (err < 0) {
            free(ext_csd_buf);
            return err;
        }

        parse_ext_csd((char *)ext_csd_buf, &mmc_dev->ext_csd);
        mmc_dev->mem_capacity = mmc_dev->ext_csd.sec_count * mmc_dev->blksize;

        free(ext_csd_buf);
        break;
    }

    return NO_ERROR;
}

void trace_cmd_resp(struct mmc_cmd *cmd) {
    /* Print MCI RESP0 for R48 otherwise print all RESP MCI registers (R138) */
    uint32_t count = cmd->resp_type == MMC_RESP_R48 ? 1 : 4;

    for (uint32_t i = 0; i < count; i++) {
        printf("SDHCI: resp[%d]: 0x%x\n", i, cmd->resp[i]);
    }
}

void print_mmc_info(struct mmc_device *mmc_dev) {
    struct mmc_cid *cid = &mmc_dev->cid;
    struct mmc_csd *csd = &mmc_dev->csd;
    struct mmc_ext_csd *ext_csd = &mmc_dev->ext_csd;

    dprintf(INFO, "CID Register\n");
    dprintf(INFO, "\tManufacturer ID: 0x%02X\n", cid->mid);
    dprintf(INFO, "\tOEM/Application ID: %s\n", cid->oid);
    dprintf(INFO, "\tProduct Name: %s\n", cid->pnm);
    dprintf(INFO, "\tProduct Revision: %u.%u\n", cid->prv_major, cid->prv_minor);
    dprintf(INFO, "\tProduct Serial Number: 0x%08X\n", cid->psn);
    dprintf(INFO, "\tManufacturing Date: %04u-%02u\n", cid->mdt_y, cid->mdt_m);
    dprintf(INFO, "\tCRC7 Checksum: 0x%02X\n", cid->crc);

    dprintf(INFO, "\nCSD Register\n");
    dprintf(INFO, "\tCSD structure: %d\n", csd->structure);
    dprintf(INFO, "\tCSD C_SIZE: %d\n", csd->c_size);
    dprintf(INFO, "\tCSD C_SIZE_MULT: %d\n", csd->c_size_mult);
    dprintf(INFO, "\tCSD READ_BLK_LEN: %d\n", csd->read_blk_ln);

    dprintf(INFO, "\nExtended CSD register:\n");
    dprintf(INFO, "\tCSD structure version %x\n", ext_csd->structure);
    dprintf(INFO, "\tSector count %x\n", ext_csd->sec_count);

    dprintf(INFO, "\nMemory capacity: %lu\n", mmc_dev->mem_capacity);
}

status_t mmc_go_idle_state(struct mmc_device *mmc_dev) {
    struct mmc_cmd cmd = (struct mmc_cmd) {
        .idx = MMC_CMD_GO_IDLE_STATE,
        .resp_type = MMC_RESP_NONE,
        .arg = 0,
    };

    status_t err = mmc_dev->host->ops->send_cmd(mmc_dev, &cmd);
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
        .arg = 1 << 0x10
    };

    status_t err = mmc_dev->host->ops->send_cmd(mmc_dev, &cmd);
    if (err < 0) {
        LTRACEF("Failed to send command, cmd: %d\n", cmd.idx);
        return err;
    }

    parse_cid(cmd.resp, &mmc_dev->cid);

    return err;
}

status_t mmc_set_relative_addr(struct mmc_device *mmc_dev) {
    struct mmc_cmd cmd = (struct mmc_cmd) {
        .idx = MMC_CMD_SET_RELATIVE_ADDR,
        .resp_type = MMC_RESP_R48,
        .arg = 1 << 0x10
    };

    status_t err = mmc_dev->host->ops->send_cmd(mmc_dev, &cmd);
    if (err < 0) {
        LTRACEF("Failed to send command, cmd: %d\n", cmd.idx);
        return err;
    }

    return err;
}

status_t mmc_send_op_cond(struct mmc_device *mmc_dev) {
    struct mmc_cmd cmd = (struct mmc_cmd) {
        .idx = MMC_CMD_SEND_OP_COND,
        .resp_type = MMC_RESP_R48,
        .arg = 0x40300000,
    };

    status_t err = mmc_dev->host->ops->send_cmd(mmc_dev, &cmd);
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

status_t mmc_send_csd(struct mmc_device *mmc_dev, uint32_t *resp) {
    struct mmc_cmd cmd = (struct mmc_cmd) {
        .idx = MMC_CMD_SEND_CSD,
        .resp_type = MMC_RESP_R138,
        .arg = 1 << 0x10,
    };

    status_t err = mmc_dev->host->ops->send_cmd(mmc_dev, &cmd);
    if (err < 0) {
        LTRACEF("Failed to send command, cmd: %d\n", cmd.idx);
        return err;
    }

    memcpy(resp, cmd.resp, sizeof(uint32_t) * 4);

    return err;
}

status_t mmc_send_ext_csd(struct mmc_device *mmc_dev, uint8_t *buffer) {
    struct mmc_cmd cmd = (struct mmc_cmd) {
        .idx = MMC_CMD_SEND_EXT_CSD,
        .resp_type = MMC_RESP_R48,
        .arg = 0,
        .has_data = true,
    };

    status_t err = mmc_dev->host->ops->get_ext_csd(mmc_dev, buffer);
    if (err < 0) {
        LTRACEF("Failed to send command, cmd: %d\n", cmd.idx);
        return err;
    }

    return err;
}

status_t mmc_stop_transmission(struct mmc_device *mmc_dev) {
    struct mmc_cmd cmd = (struct mmc_cmd) {
        .idx = MMC_CMD_STOP_TRANSMISSION,
        .resp_type = MMC_RESP_R48,
        .arg = 0,
    };

    status_t err = mmc_dev->host->ops->send_cmd(mmc_dev, &cmd);
    if (err < 0) {
        LTRACEF("Failed to send command, cmd: %d, reason: %d\n", cmd.idx, err);
        return err;
    }

    return err;
}

status_t mmc_read_single_blk(struct mmc_device *mmc_dev, uint32_t block) {
    struct mmc_cmd cmd = (struct mmc_cmd) {
        .idx = MMC_CMD_READ_SINGLE_BLK,
        .resp_type = MMC_RESP_R48,
        .arg = block,
    };

    status_t err = mmc_dev->host->ops->send_cmd(mmc_dev, &cmd);
    if (err < 0) {
        LTRACEF("Failed to send command, cmd: %d, reason: %d\n", cmd.idx, err);
        return err;
    }

    return err;
}

status_t mmc_read_multiple_blk(struct mmc_device *mmc_dev, uint32_t block) {
    struct mmc_cmd cmd = {
        .idx = MMC_CMD_READ_MULTIPLE_BLK,
        .resp_type = MMC_RESP_R48,
        .arg = block,
    };

    status_t err = mmc_dev->host->ops->send_cmd(mmc_dev, &cmd);
    if (err < 0) {
        LTRACEF("Failed to send command, cmd: %d, reason: %d\n", cmd.idx, err);
        return err;
    }

    return err;
}

status_t mmc_write_single_blk(struct mmc_device *mmc_dev, uint32_t block) {
    struct mmc_cmd cmd = (struct mmc_cmd) {
        .idx = MMC_CMD_WRITE_SINGLE_BLK,
        .resp_type = MMC_RESP_R48,
        .arg = block,
    };

    status_t err = mmc_dev->host->ops->send_cmd(mmc_dev, &cmd);
    if (err < 0) {
        LTRACEF("Failed to send command, cmd: %d, reason: %d\n", cmd.idx, err);
        return err;
    }

    return err;
}

status_t mmc_write_multiple_blk(struct mmc_device *mmc_dev, uint32_t block) {
    struct mmc_cmd cmd = (struct mmc_cmd) {
        .idx = MMC_CMD_WRITE_MULTIPLE_BLK,
        .resp_type = MMC_RESP_R48,
        .arg = block,
    };
    status_t err = mmc_dev->host->ops->send_cmd(mmc_dev, &cmd);
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

    status_t err = mmc_dev->host->ops->send_cmd(mmc_dev, &cmd);
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

status_t mmc_init(struct mmc_host *host) {
    status_t err;

    mmc.host = host;
    mmc.name = "mmc";

    err = mmc.host->ops->init(&mmc);
    if (err < 0)
        return err;

    err = mmc_go_idle_state(&mmc);
    if (err < 0)
        return err;

    err = mmc_send_op_cond(&mmc);
    if (err < 0)
        return err;

    err = mmc_all_send_cid(&mmc);
    if (err < 0)
        return err;

    err = mmc_set_relative_addr(&mmc);
    if (err < 0)
        return err;

    mmc_set_mem_caps(&mmc); 
    print_mmc_info(&mmc);
    mmc_bdev_init(&mmc);

    return NO_ERROR;
}
