/*
 * Copyright (c) 2026 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#pragma once

#include <sys/types.h>
#include <stdint.h>
#include <stddef.h>

#include <lk/list.h>

/* RPMB Request Types */
#define RPMB_REQ_PROGRAM_KEY                (0x0001)
#define RPMB_REQ_READ_WRITE_COUNTER         (0x0002)
#define RPMB_REQ_AUTH_WRITE                 (0x0003)
#define RPMB_REQ_AUTH_READ                  (0x0004)
#define RPMB_REQ_RESULT_READ                (0x0005)
#define RPMB_REQ_AUTH_DEVICE_CONFIG_READ    (0x0006)
#define RPMB_REQ_AUTH_DEVICE_CONFIG_WRITE   (0x0007)

/* RPMB Response Types */
#define RPMB_RESP_PROGRAM_KEY               (0x0100)
#define RPMB_RESP_READ_WRITE_COUNTER        (0x0200)
#define RPMB_RESP_AUTH_WRITE                (0x0300)
#define RPMB_RESP_AUTH_READ                 (0x0400)
#define RPMB_RESP_AUTH_DEVICE_CONFIG_READ   (0x0600)
#define RPMB_RESP_AUTH_DEVICE_CONFIG_WRITE  (0x0700)

struct rpmb_frame {
    uint8_t stuff_bytes[128];
    uint8_t key_mac[32];
    uint8_t data[256];
    uint8_t nonce[16];
    uint32_t write_counter;
    uint16_t address;
    uint16_t block_count;
    uint16_t result;
    uint16_t req_resp;
} __attribute__((packed));

#define RPMB_FRAME_SIZE sizeof(struct rpmb_frame)

enum rpmb_type {
    RPMB_TYPE_EMMC,
};

struct rpmb_dev;

struct rpmb_ops {
    status_t (*route_frames)(struct rpmb_dev *dev, const void *req, 
                             uint32_t req_len, void *resp, uint32_t resp_len);
};

struct rpmb_dev {
    struct list_node node;
    uint32_t id;
    enum rpmb_type type;
    uint16_t capacity_mult;
    const struct rpmb_ops *ops;
    void *priv;
};

status_t rpmb_dev_register(struct rpmb_dev *dev);
status_t rpmb_dev_unregister(struct rpmb_dev *dev);
struct rpmb_dev *rpmb_dev_get(uint32_t id);

status_t rpmb_route_frames(struct rpmb_dev *dev, const uint8_t *req,
                           uint32_t req_len, uint8_t *resp, uint32_t resp_len);
