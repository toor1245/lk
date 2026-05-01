/*
 * Copyright (c) 2026 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#pragma once

#include <dev/mmc/sdhci.h>
#include <stdint.h>

#define BIT(nr) (1UL << (nr))

#define SDHCI_REG_BLOCK_COUNT   0x000
#define SDHCI_REG_BLOCK_SIZE    0x004
#define SDHCI_REG_ARG           0x008


#define SDHCI_REG_TRANSFER_MODE 0x00C

#define SDHCI_TRNS_DMA		BIT(0)
#define SDHCI_TRNS_BLK_CNT_EN	BIT(1)
#define SDHCI_TRNS_ACMD12	BIT(2)
#define SDHCI_TRNS_READ	BIT(4)
#define SDHCI_TRNS_MULTI	BIT(5)


/* SDHCI Command */
#define SDHCI_REG_COMMAND       0x00E

/* SDHCI Command fields */
#define  SDHCI_CMD_RESP_MASK	0x03
#define  SDHCI_CMD_CRC		0x08
#define  SDHCI_CMD_INDEX	0x10
#define  SDHCI_CMD_DATA		0x20
#define  SDHCI_CMD_ABORTCMD	0xC0

/* SDHCI Response */
#define SDHCI_REG_RESP              0x010

/* SDHCI Response Type */
#define  SDHCI_CMD_RESP_NONE        0x00
#define  SDHCI_CMD_RESP_LONG        0x01
#define  SDHCI_CMD_RESP_SHORT       0x02
#define  SDHCI_CMD_RESP_SHORT_BUSY  0x03

#define SDHCI_REG_BUFFER        0x020

#define SDHCI_REG_PRESENT_STATE 0x024

#define SDHCI_CMD_INHIBIT           (1 << 0)
#define SDHCI_DATA_INHIBIT          (1 << 1)
#define SDHCI_DAT_ACTIVE            (1 << 2)
#define SDHCI_DOING_WRITE           (1 << 8)
#define SDHCI_DOING_READ            (1 << 9)
#define SDHCI_SPACE_AVAILABLE       (1 << 10)
#define SDHCI_DATA_AVAILABLE        (1 << 11)
#define SDHCI_CARD_PRESENT          (1 << 16)
#define SDHCI_CARD_STATE_STABLE	    (1 << 17)
#define SDHCI_CARD_DETECT_PIN_LEVEL	(1 << 18)
#define SDHCI_WRITE_PROTECT         (1 << 19)

#define SDHCI_INT_STATUS	0x30
#define SDHCI_INT_ENABLE	0x34
#define SDHCI_SIGNAL_ENABLE	0x38
#define SDHCI_INT_RESPONSE	BIT(0)
#define SDHCI_INT_DATA_END	BIT(1)
#define SDHCI_INT_DMA_END	BIT(3)
#define SDHCI_INT_SPACE_AVAIL	BIT(4)
#define SDHCI_INT_DATA_AVAIL	BIT(5)
#define SDHCI_INT_CARD_INSERT	BIT(6)
#define SDHCI_INT_CARD_REMOVE	BIT(7)
#define SDHCI_INT_CARD_INT	BIT(8)
#define SDHCI_INT_ERROR	BIT(15)
#define SDHCI_INT_TIMEOUT	BIT(16)
#define SDHCI_INT_CRC		BIT(17)
#define SDHCI_INT_END_BIT	BIT(18)
#define SDHCI_INT_INDEX	BIT(19)
#define SDHCI_INT_DATA_TIMEOUT	BIT(20)
#define SDHCI_INT_DATA_CRC	BIT(21)
#define SDHCI_INT_DATA_END_BIT	BIT(22)
#define SDHCI_INT_BUS_POWER	BIT(23)
#define SDHCI_INT_ACMD12ERR	BIT(24)
#define SDHCI_INT_ADMA_ERROR	BIT(25)

#define SDHCI_INT_NORMAL_MASK	0x00007FFF
#define SDHCI_INT_ERROR_MASK	0xFFFF8000

#define SDHCI_INT_CMD_MASK	(SDHCI_INT_RESPONSE | SDHCI_INT_TIMEOUT | \
		SDHCI_INT_CRC | SDHCI_INT_END_BIT | SDHCI_INT_INDEX)
#define SDHCI_INT_DATA_MASK	(SDHCI_INT_DATA_END | SDHCI_INT_DMA_END | \
		SDHCI_INT_SPACE_AVAIL | SDHCI_INT_DATA_AVAIL | \
		SDHCI_INT_DATA_TIMEOUT | SDHCI_INT_DATA_CRC | \
		SDHCI_INT_DATA_END_BIT | SDHCI_INT_ADMA_ERROR)
#define SDHCI_INT_ALL_MASK	((unsigned int)-1)

#define SDHCI_SOFTWARE_RESET	0x2F
#define  SDHCI_RESET_ALL	0x01
#define  SDHCI_RESET_CMD	0x02
#define  SDHCI_RESET_DATA	0x04

#define SDHCI_POWER_CONTROL	0x29
#define  SDHCI_POWER_ON		0x01
#define  SDHCI_POWER_180	0x0A
#define  SDHCI_POWER_300	0x0C
#define  SDHCI_POWER_330	0x0E

#define SDHCI_CLOCK_CONTROL	0x2C
#define  SDHCI_DIVIDER_SHIFT	8
#define  SDHCI_DIVIDER_HI_SHIFT	6
#define  SDHCI_DIV_MASK	0xFF
#define  SDHCI_DIV_MASK_LEN	8
#define  SDHCI_DIV_HI_MASK	0x300
#define  SDHCI_PROG_CLOCK_MODE  BIT(5)
#define  SDHCI_CLOCK_CARD_EN	BIT(2)
#define  SDHCI_CLOCK_INT_STABLE	BIT(1)
#define  SDHCI_CLOCK_INT_EN	BIT(0)

#define SDHCI_MAKE_CMD(c, f) (((c & 0xff) << 8) | (f & 0xff))

static inline uint32_t sdhci_readl(uintptr_t base, uint32_t reg) {
    return *(volatile uint32_t *)(base + reg);
}

static inline uint16_t sdhci_readw(uintptr_t base, uint32_t reg) {
    return *(volatile uint16_t *)(base + reg);
}

static inline uint8_t sdhci_readb(uintptr_t base, uint8_t reg) {
    return *(volatile uint8_t *)(base + reg);
}

static inline void sdhci_writel(uintptr_t base, uint32_t val, uint32_t reg) {
    *(volatile uint32_t *)(base + reg) = val;
}

static inline void sdhci_writew(uintptr_t base, uint16_t val, uint32_t reg) {
    *(volatile uint16_t *)(base + reg) = val;
}

static inline void sdhci_writeb(uintptr_t base, uint8_t val, uint32_t reg) {
    *(volatile uint8_t *)(base + reg) = val;
}