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
#include <lk/list.h>
#include <lk/bits.h>

#include <lib/bio.h>

#include <dev/mmc.h>
#include <dev/mmc_bdev.h>
#include <dev/rpmb.h>

#define LOCAL_TRACE 1

static struct list_node mmc_devices = LIST_INITIAL_VALUE(mmc_devices);
static uint32_t dev_num = 0;
static uint32_t dev_rca = 1;

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

static void print_mmc_info(struct mmc_device *mmc_dev) {
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

status_t mmc_send_csd(struct mmc_device *mmc_dev, uint32_t *resp) {
    struct mmc_cmd cmd = (struct mmc_cmd) {
        .idx = MMC_CMD_SEND_CSD,
        .resp_type = MMC_RESP_R138,
        .arg = mmc_dev->rca << 0x10,
        .data = NULL
    };

    status_t err = mmc_dev->host->ops->send_cmd(mmc_dev, &cmd);
    if (err < 0) {
        LTRACEF("Failed to send command, cmd: %d\n", cmd.idx);
        return err;
    }

    memcpy(resp, cmd.resp, sizeof(uint32_t) * 4);

    return err;
}

static status_t mmc_send_ext_csd(struct mmc_device *mmc_dev, char *buf) {
    struct mmc_data data = (struct mmc_data) {
        .buffer = buf,
        .block = 0,
        .blkcount = 1,
        .flags = MMC_DATA_READ,
    };

    struct mmc_cmd cmd = (struct mmc_cmd) {
        .idx = MMC_CMD_SEND_EXT_CSD,
        .resp_type = MMC_RESP_R48,
        .arg = 0,
        .data = &data,
    };

    status_t err = mmc_dev->host->ops->send_cmd(mmc_dev, &cmd);
    if (err < 0) {
        LTRACEF("Failed to send command, cmd: %d\n", cmd.idx);
        return err;
    }

    return NO_ERROR;
}

static status_t mmc_select_card(struct mmc_device *mmc_dev) {
    struct mmc_cmd cmd = {
        .idx = MMC_CMD_SELECT_CARD,
        .resp_type = MMC_RESP_R1B,
        .arg = mmc_dev->rca << 0x10,
        .data = NULL,
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
        char *ext_csd_buf = malloc(mmc_dev->blksize);
        if (!ext_csd_buf) {
            return ERR_NO_MEMORY;
        }

        err = mmc_select_card(mmc_dev);
        if (err < 0)
            return err;

        err = mmc_send_ext_csd(mmc_dev, ext_csd_buf);
        if (err < 0) {
            free(ext_csd_buf);
            return err;
        }

        parse_ext_csd((char *)ext_csd_buf, &mmc_dev->ext_csd);
        mmc_dev->mem_capacity = mmc_dev->ext_csd.sec_count * mmc_dev->blksize;
        mmc_dev->blkcount = mmc_dev->mem_capacity / mmc_dev->blksize;

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

static status_t mmc_go_idle_state(struct mmc_device *mmc_dev) {
    struct mmc_cmd cmd = (struct mmc_cmd) {
        .idx = MMC_CMD_GO_IDLE_STATE,
        .resp_type = MMC_RESP_NONE,
        .arg = 0,
        .data = NULL,
    };

    status_t err = mmc_dev->host->ops->send_cmd(mmc_dev, &cmd);
    if (err < 0) {
        LTRACEF("Failed to send command, cmd: %d\n", cmd.idx);
        return err;
    }

    return err;
}

static status_t mmc_all_send_cid(struct mmc_device *mmc_dev) {
    struct mmc_cmd cmd = (struct mmc_cmd) {
        .idx = MMC_CMD_ALL_SEND_CID,
        .resp_type = MMC_RESP_R138,
        .arg = mmc_dev->rca << 0x10,
        .data = NULL
    };

    status_t err = mmc_dev->host->ops->send_cmd(mmc_dev, &cmd);
    if (err < 0) {
        LTRACEF("Failed to send command, cmd: %d\n", cmd.idx);
        return err;
    }

    parse_cid(cmd.resp, &mmc_dev->cid);

    return err;
}

static status_t mmc_set_relative_addr(struct mmc_device *mmc_dev) {
    struct mmc_cmd cmd = (struct mmc_cmd) {
        .idx = MMC_CMD_SET_RELATIVE_ADDR,
        .resp_type = MMC_RESP_R48,
        .arg = mmc_dev->rca << 0x10,
        .data = NULL
    };

    status_t err = mmc_dev->host->ops->send_cmd(mmc_dev, &cmd);
    if (err < 0) {
        LTRACEF("Failed to send command, cmd: %d\n", cmd.idx);
        return err;
    }

    return err;
}

static status_t mmc_send_op_cond(struct mmc_device *mmc_dev) {
    struct mmc_cmd cmd = (struct mmc_cmd) {
        .idx = MMC_CMD_SEND_OP_COND,
        .resp_type = MMC_RESP_R48,
        .arg = 0x40300000,
        .data = NULL
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

static status_t mmc_set_block_count(struct mmc_device *mmc_dev, uint32_t block_count,
                                    bool is_rel_write) {
    struct mmc_cmd cmd = (struct mmc_cmd) {
        .idx = MMC_CMD_SET_BLOCK_COUNT,
        .resp_type = MMC_RESP_R48,
        .arg = block_count,
    };

    if (is_rel_write)
        cmd.arg |= 1 << 31;

    status_t err = mmc_dev->host->ops->send_cmd(mmc_dev, &cmd);
    if (err < 0) {
        LTRACEF("Failed to send command, cmd: %d, reason: %d\n", cmd.idx, err);
        return err;
    }

    return err;
}

static int mmc_get_state(struct mmc_device *mmc_dev) {
    struct mmc_cmd cmd;

    do {
        memset(&cmd, 0, sizeof(struct mmc_cmd));
        cmd.idx = MMC_CMD_SEND_STATUS;
        cmd.arg = mmc_dev->rca << 0x10;
        cmd.resp_type = MMC_RESP_R48;

        status_t err = mmc_dev->host->ops->send_cmd(mmc_dev, &cmd);
        if (err < 0) {
            LTRACEF("Failed to send command, cmd: %d\n", cmd.idx);
            return err;
        }
    } while ((cmd.resp[0] & CARD_STATUS_READY_FOR_DATA) == 0);

    /* Get Card status CURRENT_STATE */
    return extract_bit_range(cmd.resp[0], 12, 9);
}

static status_t mmc_set_ext_csd(struct mmc_device *mmc_dev, uint8_t index, uint8_t value) {
    struct mmc_cmd cmd = (struct mmc_cmd) {
        .idx = MMC_CMD_SWITCH,
        .resp_type = MMC_RESP_R48,
        .arg = EXT_CSD_WRITE_BYTES | EXT_CSD_CMD(index) | EXT_CSD_VALUE(value) | 1,
    };

    status_t err = mmc_dev->host->ops->send_cmd(mmc_dev, &cmd);
    if (err < 0) {
        LTRACEF("Failed to send command, cmd: %d\n", cmd.idx);
    }

    return err;
}

static status_t mmc_rpmb_enable(struct mmc_device *mmc_dev) {
    uint8_t current_cfg = mmc_dev->ext_csd.part_config;
    uint8_t new_cfg = (current_cfg & ~EXT_CSD_PART_CONFIG_ACC_MASK) | EXT_CSD_PART_CONFIG_ACC_RPMB;
    
    return mmc_set_ext_csd(mmc_dev, MMC_EXT_CSD_PARTITION_CONFIG, new_cfg);
}

static status_t mmc_rpmb_disable(struct mmc_device *mmc_dev) {
    uint8_t current_cfg = mmc_dev->ext_csd.part_config;
    uint8_t new_cfg = (current_cfg & ~EXT_CSD_PART_CONFIG_ACC_MASK) | EXT_CSD_PART_CONFIG_ACC_USER;
    
    return mmc_set_ext_csd(mmc_dev, MMC_EXT_CSD_PARTITION_CONFIG, new_cfg);
}

static status_t mmc_rpmb_write_multi_blk(struct mmc_device *mmc_dev,
                                         struct rpmb_frame *frame, uint32_t blkcount) {
    struct mmc_data data = (struct mmc_data) {
        .buffer = (char *)frame,
        .blkcount = blkcount,
    };

    struct mmc_cmd cmd = (struct mmc_cmd) {
        .idx = MMC_CMD_WRITE_MULTIPLE_BLK,
        .resp_type = MMC_RESP_R48,
        .data = &data,
    };

    status_t err = mmc_dev->host->ops->send_cmd(mmc_dev, &cmd);
    if (err < 0) {
        LTRACEF("Failed to send command, cmd: %d, reason: %d\n", cmd.idx, err);
        return err;
    }

    return err;
}

static status_t mmc_rpmb_read_multi_block(struct mmc_device *mmc_dev,
                                          struct rpmb_frame *frame, uint32_t blkcount) {
    struct mmc_data data = (struct mmc_data) {
        .buffer = (char *)frame,
        .blkcount = blkcount,
        .flags = MMC_DATA_READ,
    };

    struct mmc_cmd cmd = (struct mmc_cmd) {
        .idx = MMC_CMD_READ_MULTIPLE_BLK,
        .resp_type = MMC_RESP_R48,
        .data = &data,
    };

    status_t err = mmc_dev->host->ops->send_cmd(mmc_dev, &cmd);
    if (err < 0) {
        LTRACEF("Failed to send command, cmd: %d, reason: %d\n", cmd.idx, err);
        return err;
    }

    return err;
}

static status_t mmc_rpmb_route_write_req(struct mmc_device *mmc_dev, struct rpmb_frame *req,
                                         uint16_t req_cnt, struct rpmb_frame *resp,
                                         uint16_t resp_cnt) {
    status_t err = mmc_set_block_count(mmc_dev, req->block_count, true);
    if (err < 0)
        return err;

    err = mmc_rpmb_write_multi_blk(mmc_dev, req, req->block_count);
    if (err < 0)
        return err;

    err = mmc_set_block_count(mmc_dev, 1, false);
    if (err < 0)
        return err;

    memset(resp, 0, sizeof(struct rpmb_frame));
    resp->req_resp = BE16(RPMB_REQ_RESULT_READ);

    err = mmc_rpmb_write_multi_blk(mmc_dev, resp, 1);
    if (err < 0)
        return err;

    err = mmc_set_block_count(mmc_dev, 1, false);
    if (err < 0)
        return err;

    return mmc_rpmb_read_multi_block(mmc_dev, resp, 1);
}

static status_t mmc_rpmb_route_read_req(struct mmc_device *mmc_dev, struct rpmb_frame *req,
                                        uint16_t req_cnt, struct rpmb_frame *resp,
                                        uint16_t resp_cnt) {
    status_t err = mmc_set_block_count(mmc_dev, 1, false);
    if (err < 0)
        return err;

    err = mmc_rpmb_write_multi_blk(mmc_dev, req, 1);
    if (err < 0)
        return err;

    err = mmc_set_block_count(mmc_dev, 1, false);
    if (err < 0)
        return err;

    return mmc_rpmb_read_multi_block(mmc_dev, resp, resp_cnt);
}

static status_t mmc_rpmb_route_frames(struct rpmb_dev *rdev, const void *req, 
                                      uint32_t req_len, void *resp, uint32_t resp_len) {
    status_t err;
    struct mmc_device *mmc_dev = (struct mmc_device *)rdev->priv;

    err = mmc_rpmb_enable(mmc_dev);
    if (err < 0) {
        printf("Failed to enable RPMB partition, reason: %d\n", err);
        return err;
    }

    struct rpmb_frame *req_frame = (struct rpmb_frame *)req;

    if (req_len < RPMB_FRAME_SIZE) {
        printf("Invalid request length: %u\n", req_len);
        return ERR_INVALID_ARGS;
    }

    status_t route_err = NO_ERROR;

    uint16_t req_type = BE16(req_frame->req_resp);
    switch (req_type)
    {
        case RPMB_REQ_PROGRAM_KEY:
            printf("RPMB: Program key request\n");

            route_err = mmc_rpmb_route_write_req(mmc_dev, req_frame, req_len, resp, resp_len);
            break;

        case RPMB_REQ_READ_WRITE_COUNTER:
            printf("RPMB: Read write counter request\n");

            route_err = mmc_rpmb_route_write_req(mmc_dev, req_frame, req_len, resp, resp_len);
            break;

        case RPMB_REQ_AUTH_WRITE:
            printf("RPMB: Authenticated write request\n");

            route_err = mmc_rpmb_route_write_req(mmc_dev, req_frame, req_len, resp, resp_len);
            break;

        case RPMB_REQ_AUTH_READ:
            printf("RPMB: Authenticated read request\n");

            route_err = mmc_rpmb_route_read_req(mmc_dev, req_frame, req_len, resp, resp_len);
            break;

        default:
            printf("RPMB: Unsupported request type: 0x%04X\n", req_type);
            route_err = ERR_INVALID_ARGS;
    }

    err = mmc_rpmb_disable(mmc_dev);
    if (err < 0) {
        printf("Failed to disable RPMB partition, reason: %d\n", err);
        return err;
    }

    if (route_err < 0) {
        printf("Failed to route RPMB request, reason: %d\n", route_err);
    }
    
    return NO_ERROR;
}

ssize_t mmc_read_blocks(struct mmc_device *mmc_dev, char *buffer,
                        uint64_t block, uint64_t blkcount) {
    status_t err;
    uint64_t blocks_remaining = blkcount;
    uint64_t current_block = block;
    char *current_buf = buffer;

    ssize_t max_blocks = mmc_dev->host->ops->get_max_block_count(mmc_dev);
    if (max_blocks < 0)
        return max_blocks;

    while (blocks_remaining > 0) {
        uint32_t chunk = (blocks_remaining > max_blocks) ?
                         max_blocks : blocks_remaining;

        err = mmc_set_block_count(mmc_dev, chunk, false);
        if (err < 0)
            return err;

        struct mmc_data data = (struct mmc_data){
            .buffer = current_buf,
            .block = current_block,
            .blkcount = chunk,
            .flags = MMC_DATA_READ,
        };

        struct mmc_cmd cmd = (struct mmc_cmd) {
            .idx = chunk > 1 ? MMC_CMD_READ_MULTIPLE_BLK : MMC_CMD_READ_SINGLE_BLK,
            .resp_type = MMC_RESP_R48,
            .arg = current_block,
            .data = &data,
        };

        err = mmc_dev->host->ops->send_cmd(mmc_dev, &cmd);
        if (err < 0) {
            LTRACEF("Failed to send command, cmd: %d, reason: %d\n", cmd.idx, err);
            return err;
        }

        blocks_remaining -= chunk;
        current_block += chunk;
        current_buf += (chunk * mmc_dev->blksize);
    }

    return blkcount * mmc_dev->blksize;
}

ssize_t mmc_write_blocks(struct mmc_device *mmc_dev, const char *buffer,
                         uint64_t block, uint64_t blkcount) {
    status_t err = mmc_set_block_count(mmc_dev, blkcount, false);
    if (err < 0)
        return err;

    struct mmc_data data = (struct mmc_data){
        .buffer = (char *)buffer,
        .block = block,
        .blkcount = blkcount,
        .flags = 0,
    };

    struct mmc_cmd cmd = (struct mmc_cmd) {
        .idx = blkcount > 1 ? MMC_CMD_WRITE_MULTIPLE_BLK : MMC_CMD_WRITE_SINGLE_BLK,
        .resp_type = MMC_RESP_R48,
        .arg = block,
        .data = &data,
    };

    err = mmc_dev->host->ops->send_cmd(mmc_dev, &cmd);
    if (err < 0) {
        LTRACEF("Failed to send command, cmd: %d, reason: %d\n", cmd.idx, err);
        return err;
    }

    return blkcount * mmc_dev->blksize;
}

static const struct rpmb_ops rpmb_ops = {
    .route_frames = mmc_rpmb_route_frames,
};

status_t mmc_init(struct mmc_host *host, struct mmc_device **out_dev) {
    status_t err;

    struct mmc_device *mmc_dev = calloc(1, sizeof(struct mmc_device));
    if (!mmc_dev)
        return ERR_NO_MEMORY;

    mmc_dev->host = host;
    mmc_dev->rca = dev_rca;

    char dev_blockname[32];
    err = snprintf(dev_blockname, sizeof(dev_blockname), "mmc%u", dev_num);
    if (err < 0)
        return err;

    mmc_dev->name = strdup(dev_blockname);
    if (!mmc_dev->name)
        return ERR_NO_MEMORY;

    err = mmc_dev->host->ops->init(mmc_dev);
    if (err < 0)
        return err;

    err = mmc_go_idle_state(mmc_dev);
    if (err < 0)
        return err;

    err = mmc_send_op_cond(mmc_dev);
    if (err < 0)
        return err;

    err = mmc_all_send_cid(mmc_dev);
    if (err < 0)
        return err;

    err = mmc_set_relative_addr(mmc_dev);
    if (err < 0)
        return err;

    mmc_set_mem_caps(mmc_dev); 
    print_mmc_info(mmc_dev);

    struct rpmb_dev *rpmb = malloc(sizeof(struct rpmb_dev));
    if (!rpmb) {
        return ERR_NO_MEMORY;
    }

    rpmb->type = RPMB_TYPE_EMMC;
    rpmb->priv = mmc_dev;
    rpmb->ops = &rpmb_ops;

    err = rpmb_dev_register(rpmb);
    if (err < 0) {
        free(rpmb);
        return err;
    }

    *out_dev = mmc_dev;

    list_add_tail(&mmc_devices, &mmc_dev->list_node);
    dev_num++;
    dev_rca++;

    return NO_ERROR;
}
