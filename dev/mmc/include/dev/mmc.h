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
};

struct mmc_device {
    struct mmc_host *host;
    bdev_t bdev;
    const char *name;
    struct mmc_cid cid;
    struct mmc_csd csd;
    struct mmc_ext_csd ext_csd;
    uint64_t blksize;
    bnum_t blkcount;
    size_t mem_capacity;
};

enum mmc_resp {
    MMC_RESP_NONE,
    MMC_RESP_R48,
    MMC_RESP_R138,
    MMC_RESP_R1B,
};

struct mmc_cmd {
    uint32_t idx;
    uint32_t arg;
    uint32_t resp[4];
    enum mmc_resp resp_type;
    bool has_data;
};

/* MMC/SD data transfer info */
struct mmc_xfer_info {
    char *buffer;
    uint32_t blkcount;
    uint32_t blksize;
    uint32_t block;
};

struct mmc_host;

struct mmc_ops {
    status_t (*init)(struct mmc_device *mmc_dev);
    status_t (*fini)(struct mmc_device *mmc_dev);
    status_t (*send_cmd)(struct mmc_device *mmc_dev, struct mmc_cmd *cmd);
    ssize_t (*read)(struct mmc_device *mmc_dev, struct mmc_xfer_info *info);
    ssize_t (*write)(struct mmc_device *mmc_dev, struct mmc_xfer_info *info);
    status_t (*get_ext_csd)(struct mmc_device *dev, uint8_t *buf);
};

/* MMC/SD interface */
struct mmc_host {
    const struct mmc_ops *ops;
    void *priv;
};

status_t mmc_init(struct mmc_host *host);

/* MMC/SD card commands */
status_t mmc_go_idle_state(struct mmc_device *mmc_dev);
status_t mmc_all_send_cid(struct mmc_device *mmc_dev);
status_t mmc_send_op_cond(struct mmc_device *mmc_dev);
status_t mmc_send_ext_csd(struct mmc_device *mmc_dev, uint8_t *buffer);
status_t mmc_send_csd(struct mmc_device *mmc_dev, uint32_t *resp);
status_t mmc_read_single_blk(struct mmc_device *mmc_dev, uint32_t block);
status_t mmc_read_multiple_blk(struct mmc_device *mmc_dev, uint32_t block);
status_t mmc_write_single_blk(struct mmc_device *mmc_dev, uint32_t block);
status_t mmc_write_multiple_blk(struct mmc_device *mmc_dev, uint32_t block);
status_t mmc_stop_transmission(struct mmc_device *mmc_dev);

/* SD card specific commands */
status_t sd_set_app_cmd(struct mmc_device *mmc_dev);

/**
 * Extract a bit range from a 128-bit MMC/SD register (CID/CSD/other)
 * represented as 4x32-bit words.
 *
 * resp[0] = bits 127..96 (MSB word)
 * resp[1] = bits 95..64
 * resp[2] = bits 63..32
 * resp[3] = bits 31..0  (LSB word)
 *
 * @param resp  Pointer to 4x32-bit words (big-endian style)
 * @param msb   Most significant bit of field (0..127)
 * @param lsb   Least significant bit of field (0..127)
 * @return      Extracted value as uint32_t (max 32 bits)
 */
static inline uint32_t extract_bit_range128(const uint32_t resp[4],
    uint32_t msb, uint32_t lsb)
{
    assert(msb < 128);
    assert(lsb < 128);
    assert(msb >= lsb);

    // Width of the field
    uint32_t width = msb - lsb + 1;
    assert(width <= 32);

    // Word indices (resp[0] = bits 127..96)
    uint32_t w_msb = 3 - (msb >> 5);
    uint32_t w_lsb = 3 - (lsb >> 5);

    // Bit positions in their words
    uint32_t b_msb = msb & 31;
    uint32_t b_lsb = lsb & 31;

    uint32_t result;

    if (w_msb == w_lsb) {
        // Field fits in a single word
        result = (resp[w_msb] >> (b_lsb)) & ((1U << width) - 1U);
    } else {
        // Field spans two words
        uint32_t upper = resp[w_msb] & ((1U << (b_msb + 1U)) - 1U);
        uint32_t lower = resp[w_lsb] >> b_lsb;
        result = (upper << (width - (b_msb + 1U))) | lower;
    }

    return result;
}

static inline uint64_t extract_byte_range512(char *buffer, uint32_t offset, uint32_t len)
{
    assert(offset + len <= 512);
    assert(len > 0 && len <= 8);

    uint64_t result = 0;

    for (uint32_t i = 0; i < len; i++) {
        result |= ((uint64_t)buffer[offset + i] << (8 * i));
    }

    return result;
}

void trace_cmd_resp(struct mmc_cmd *cmd);
