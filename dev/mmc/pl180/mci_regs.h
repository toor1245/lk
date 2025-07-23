/*
 * Copyright (c) 2025 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */
#pragma once

/* PL180 MCI registers */
enum pl180_regs {
    MCI_PWR     = 0x000,
    MCI_CLK     = 0x004,
    MCI_ARG     = 0x008,
    MCI_CMD     = 0x00C,
    MCI_RESPCMD = 0x010,
    MCI_RESP0   = 0x014,
    MCI_RESP1   = 0x018,
    MCI_RESP2   = 0x01C,
    MCI_RESP3   = 0x020,
    MCI_DTIMER  = 0x024,
    MCI_DLEN    = 0x028,
    MCI_DCTRL   = 0x02C,
    MCI_DCNT    = 0x030,
    MCI_STAT    = 0x034,
    MCI_CLR     = 0x038,
    MCI_MASK0   = 0x03C,
    MCI_MASK1   = 0x040,
    MCI_DFIFO   = 0x080,
};

/* Bit assignment of the MCI registers */
#define MCI_STAT_CMD_CRC_FAIL       (1 << 0)
#define MCI_STAT_DATA_CRC_FAIL      (1 << 1)
#define MCI_STAT_CMD_TIME_OUT       (1 << 2)
#define MCI_STAT_DATA_TIME_OUT      (1 << 3)
#define MCI_STAT_TX_UNDERRUN        (1 << 4)
#define MCI_STAT_RX_OVERRUN         (1 << 5)
#define MCI_STAT_CMD_RESP_END       (1 << 6)
#define MCI_STAT_CMD_SENT           (1 << 7)
#define MCI_STAT_DATA_END           (1 << 8)
#define MCI_STAT_START_BIT_ERR      (1 << 9)
#define MCI_STAT_DATA_BLOCK_END     (1 << 10)
#define MCI_STAT_CMD_ACTIVE         (1 << 11)
#define MCI_STAT_TX_ACTIVE          (1 << 12)
#define MCI_STAT_RX_ACTIVE          (1 << 13)
#define MCI_STAT_TX_FIFO_HALF_EMPTY (1 << 14)
#define MCI_STAT_RX_FIFO_HALF_FULL  (1 << 15)
#define MCI_STAT_TX_FIFO_FULL       (1 << 16)
#define MCI_STAT_RX_FIFO_FULL       (1 << 17)
#define MCI_STAT_TX_FIFO_EMPTY      (1 << 18)
#define MCI_STAT_RX_FIFO_EMPTY      (1 << 19)
#define MCI_STAT_TX_DATA_AVLBL      (1 << 20)
#define MCI_STAT_RX_DATA_AVLBL      (1 << 21)

#define MCI_CLR_CMD_CRC_FAIL  (1 << 0)
#define MCI_CLR_DATA_CRC_FAIL (1 << 1)
#define MCI_CLR_CMD_TIMEOUT   (1 << 2)
#define MCI_CLR_DATA_TIMEOUT  (1 << 3)
#define MCI_CLR_CMD_RESP_END  (1 << 6)
#define MCI_CLR_CMD_SENT      (1 << 7)
