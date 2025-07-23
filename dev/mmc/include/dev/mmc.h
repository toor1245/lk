/*
 * Copyright (c) 2025 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#pragma once

#include <sys/types.h>
#include <lib/bio.h>
#include <dev/driver.h>

/* SD/MMC commands indexes */
#define MMC_CMD_GO_IDLE_STATE		(0)
#define MMC_CMD_SEND_OP_COND		(1)
#define MMC_CMD_ALL_SEND_CID		(2)
#define MMC_CMD_SET_RELATIVE_ADDR	(3)
#define MMC_CMD_SET_DSR			(4)
#define MMC_CMD_SWITCH			(6)
#define MMC_CMD_SEND_CSD		(9)
#define MMC_CMD_STOP_TRANSMISSION	(12)
#define MMC_CMD_SET_BLOCKLEN		(16)
#define MMC_CMD_READ_SINGLE_BLK		(17)
#define MMC_CMD_READ_MULTIPLE_BLK	(18)
#define MMC_CMD_WRITE_SINGLE_BLK	(24)
#define MMC_CMD_WRITE_MULTIPLE_BLK	(25)

/* SD card commands indexes */
#define SD_CMD_SEND_OP_COND	(41)
#define SD_APP_CMD		(55)

#define CARD_STATUS_ACMD    (1 << 5)

/* Card registers mask */
#define OCR_VOLTAGE_MASK    (0x00FFFF80)
#define OCR_ACCESS_MODE_MASK    (0x60000000)
#define OCR_BUSY_MASK       (0x80000000)

struct mmc_cid {
    uint8_t mid; /* Manufacturer ID (8 bits) */
    char oid[3]; /* OEM/Application ID (2 ASCII chars + null terminator) */
    char pnm[6]; /* Product name (5 ASCII chars + null terminator) */
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
};

struct mmc_csd {
    enum mmc_csd_structure structure; /* CSD structure, Readable, [127:126] */
    uint16_t c_size; /* Device size, Readable, [73:62] */
    uint8_t read_blk_ln; /* Max read data block length, Readable, [83:80] */
};

struct mmc_device {
    struct device *dev;
    bdev_t bdev;
    uint64_t capacity;
    uint64_t blksize;
    bnum_t blkcount;
    struct mmc_cid cid;
    struct mmc_csd csd;
};

/* MMC/SD card commands */
status_t mmc_go_idle_state(struct mmc_device *mmc_dev);
status_t mmc_all_send_cid(struct mmc_device *mmc_dev);
status_t mmc_send_op_cond(struct mmc_device *mmc_dev);
status_t mmc_send_csd(struct mmc_device *mmc_dev);
status_t mmc_read_single_blk(struct device *mmc_dev, uint32_t block);
status_t mmc_read_multiple_blk(struct device *mmc_dev, uint32_t block);
status_t mmc_write_single_blk(struct device *mmc_dev, uint32_t block);
status_t mmc_write_multiple_blk(struct device *mmc_dev, uint32_t block);

/* SD card specific commands */
status_t sd_set_app_cmd(struct mmc_device *mmc_dev);
