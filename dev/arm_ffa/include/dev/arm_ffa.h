/*
 * Copyright (c) 2026 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#pragma once

#include <arm_ffa.h>
#include <arm_ffa_mem.h>
#include <stdint.h>
#include <stddef.h>

#include <lk/err.h>

struct ffa_device {
    uint32_t version;
    ffa_endpoint_id_t endpoint_id;
    void *rx_buffer;
    void *tx_buffer;
    ffa_partition_info_t *partitions;
    size_t desc_count;
};

struct ffa_device *arm_ffa_get_device(void);
status_t arm_ffa_find_partition_by_uuid(struct ffa_device *ffa_dev,
                                        const uint32_t uuid[4],
                                        ffa_partition_info_t *partition);

status_t arm_ffa_shm_alloc(struct ffa_device *dev,
                           ffa_endpoint_id_t receiver_id, ffa_mem_handle_t *handle,
                           size_t size, void **shm);
status_t arm_ffa_shm_free(struct ffa_device *dev, ffa_mem_handle_t handle);
