/*
 * Copyright (c) 2026 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#pragma once

#include <sys/types.h>
#include <lib/bio.h>
#include <stdint.h>

#include <lk/list.h>

/* SD/MMC commands indexes */
#define MMC_CMD_GO_IDLE_STATE		(0)
#define MMC_CMD_SEND_OP_COND		(1)
#define MMC_CMD_ALL_SEND_CID		(2)
#define MMC_CMD_SET_RELATIVE_ADDR	(3)
#define MMC_CMD_SET_DSR			(4)
#define MMC_CMD_SWITCH			(6)
#define MMC_CMD_SELECT_CARD     (7)
#define MMC_CMD_SEND_EXT_CSD	(8)
#define MMC_CMD_SEND_CSD		(9)
#define MMC_CMD_STOP_TRANSMISSION	(12)
#define MMC_CMD_SEND_STATUS         (13)
#define MMC_CMD_SET_BLOCKLEN		(16)
#define MMC_CMD_READ_SINGLE_BLK		(17)
#define MMC_CMD_READ_MULTIPLE_BLK	(18)
#define MMC_CMD_SET_BLOCK_COUNT		(23)
#define MMC_CMD_WRITE_SINGLE_BLK	(24)
#define MMC_CMD_WRITE_MULTIPLE_BLK	(25)

/* SD card commands indexes */
#define SD_CMD_SEND_OP_COND	(41)
#define SD_APP_CMD		(55)

#define CARD_STATUS_ACMD                (1 << 5)
#define CARD_STATUS_READY_FOR_DATA      (1 << 8)
#define MMC_CURRENT_STATE()

/* Card registers mask */
#define OCR_VOLTAGE_MASK    (0x00FFFF80)
#define OCR_ACCESS_MODE_MASK    (0x60000000)
#define OCR_BUSY_MASK       (0x80000000)

#define MMC_EXT_CSD_PARTITION_CONFIG 179

#define EXT_CSD_SET_CMD	        (0 << 24)
#define EXT_CSD_SET_BITS        (1 << 24)
#define EXT_CSD_CLR_BITS        (2 << 24)
#define EXT_CSD_WRITE_BYTES		(3 << 24)
#define EXT_CSD_CMD(x)          (((x) & 0xff) << 16)
#define EXT_CSD_VALUE(x)        (((x) & 0xff) << 8)

#define EXT_CSD_PART_CONFIG_ACC_MASK 0x07
#define EXT_CSD_PART_CONFIG_ACC_USER 0x00
#define EXT_CSD_PART_CONFIG_ACC_RPMB 0x03

struct mmc_cid {
    uint8_t mid; /* Manufacturer ID (8 bits) */
    char oid[3]; /* OEM/Application ID (2 ASCII chars + null terminator) */
    char pnm[6]; /* Product name (6 ASCII chars) */
    uint8_t prv_major; /* Product revision major (4 bits) */
    uint8_t prv_minor; /* Product revision minor (4 bits) */
    uint32_t psn; /* Product serial number */
    uint8_t mdt_m; /* Manufacturing date month (4 bits) */
    uint16_t mdt_y; /* Manufacturing date year (8 bits + offset from 2000) */
    uint8_t crc; /* CRC checksum (7 bits) */
};

enum mmc_csd_structure {
    MMC_CSD_V1 = 0, /* CSD Version 1.0, Standard Capacity */
    MMC_CSD_V2 = 1, /* CSD Version 2.0, High Capacity and Extended Capacity */
    MMC_CSD_V3 = 2, /* CSD Version 3.0, Ultra Capacity (SDUC) */
    MMC_CSD_EXT = 3, /* CSD Structure is located in CSD_EXT */
};

struct mmc_csd {
    enum mmc_csd_structure structure; /* CSD structure, Readable, [127:126] */
    uint8_t read_blk_ln; /* Max read data block length, Readable, [83:80] */
    uint32_t c_size; /* Device size, Readable, [73:62] */
    uint8_t c_size_mult; /* Device size multiplier, Readable, [49:47] */
};

struct mmc_ext_csd {
    enum mmc_csd_structure structure;
    uint32_t sec_count;
    uint8_t part_config;
};

struct mmc_device {
    struct list_node list_node;
    struct mmc_host *host;
    bdev_t bdev;
    const char *name;
    struct mmc_cid cid;
    struct mmc_csd csd;
    struct mmc_ext_csd ext_csd;
    uint64_t blksize;
    bnum_t blkcount;
    size_t mem_capacity;
    uint16_t rca;
};

enum mmc_resp {
    MMC_RESP_NONE,
    MMC_RESP_R48,
    MMC_RESP_R138,
    MMC_RESP_R1B,
};

#define MMC_DATA_READ  (1 << 0)

/* MMC/SD data transfer info */
struct mmc_data {
    char *buffer;
    uint64_t blkcount;
    uint64_t block;
    uint32_t flags;
};

struct mmc_cmd {
    uint32_t idx;
    uint32_t arg;
    uint32_t resp[4];
    enum mmc_resp resp_type;
    struct mmc_data *data;
};

struct mmc_host;

struct mmc_ops {
    status_t (*init)(struct mmc_device *mmc_dev);
    status_t (*fini)(struct mmc_device *mmc_dev);
    status_t (*send_cmd)(struct mmc_device *mmc_dev, struct mmc_cmd *cmd);
    ssize_t (*get_max_block_count)(struct mmc_device *mmc_dev);
};

/* MMC/SD interface */
struct mmc_host {
    const struct mmc_ops *ops;
    void *priv;
};

/* MMC/SD API */
status_t mmc_init(struct mmc_host *host, struct mmc_device **out_dev);
ssize_t mmc_read_blocks(struct mmc_device *mmc_dev, char *buffer,
                        uint64_t block, uint64_t blkcount);
ssize_t mmc_write_blocks(struct mmc_device *mmc_dev, const char *buffer,
                         uint64_t block, uint64_t blkcount);

void trace_cmd_resp(struct mmc_cmd *cmd);
