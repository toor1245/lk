/*
 * Copyright (c) 2025 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/* MMC commands indexes */
#define MMC_CMD_GO_IDLE_STATE		(0)
#define MMC_CMD_SEND_OP_COND		(1)
#define MMC_CMD_ALL_SEND_CID		(2)
#define MMC_CMD_SET_RELATIVE_ADDR	(3)
#define MMC_CMD_SET_DSR			(4)
#define MMC_CMD_SWITCH			(6)
#define MMC_CMD_STOP_TRANSMISSION	(12)
#define MMC_CMD_SET_BLOCKLEN		(16)
#define MMC_CMD_READ_SINGLE_BLK		(17)
#define MMC_CMD_READ_MULTIPLE_BLK	(18)
#define MMC_CMD_WRITE_SINGLE_BLK	(24)
#define MMC_CMD_WRITE_MULTIPLE_BLK	(25)

/* SD card commands indexes */
#define SD_CMD_SEND_OP_COND	(41)
#define SD_APP_CMD		(55)

struct mmc_cid {
    uint8_t mid;	/* Manufacturer ID (8 bits) */
    char oid[3];	/* OEM/Application ID (2 ASCII chars + null terminator) */
    char pnm[6];	/* Product name (5 ASCII chars + null terminator) */
    uint8_t prv_major;	/* Product revision major (4 bits) */
    uint8_t prv_minor;	/* Product revision minor (4 bits) */
    uint32_t psn;	/* Product serial number */
    uint8_t mdt_m;	/* Manufacturing date month (4 bits) */
    uint16_t mdt_y;	/* Manufacturing date year (8 bits + offset from 2000) */
    uint8_t crc;
};
