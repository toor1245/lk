/*
 * Copyright (c) 2026 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#include <dev/arm_ffa.h>
#include <arm_ffa.h>
#include <arm_ffa_mem.h>

#include <sys/types.h>
#include <arch/defines.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <lk/init.h>
#include <lk/debug.h>

#include <kernel/vm.h>

#define FFA_VERSION_MAJOR 0x1
#define FFA_VERSION_MINOR 0x2
#define FFA_SUPPORTED_VERSION ((FFA_VERSION_MAJOR << 16) | FFA_VERSION_MINOR)

#define UUID_SIZE (sizeof(uint32_t) * 4)

static struct ffa_device ffa_dev;

static status_t arm_ffa_version(struct ffa_device *dev) {
    ffa_args_t payload;
    memset(&payload, 0, sizeof(ffa_args_t));
    payload.arg1 = FFA_SUPPORTED_VERSION;

    ffa_version(&payload);

    if (payload.fid == FFA_ERROR_NOT_SUPPORTED) {
        printf("FFA_VERSION not supported\n");
        return ERR_NOT_SUPPORTED;
    }

    if (payload.fid == FFA_ERROR_SMC32) {
        printf("FFA_VERSION failed, FF-A Error %llx\n", payload.fid);
        return ERR_GENERIC;
    }

    uint32_t version = payload.fid;
    uint32_t major = FFA_GET_MAJOR(version);
    uint32_t minor = FFA_GET_MINOR(version);

    printf("FF-A version found %u.%u\n", major, minor);

    dev->version = version;

    return NO_ERROR;
}

static status_t arm_ffa_get_id(ffa_endpoint_id_t *endpoint_id) {
    ffa_args_t payload;
    memset(&payload, 0, sizeof(ffa_args_t));

    ffa_id_get(&payload);

    if (payload.fid == FFA_ERROR_SMC32) {
        printf("Error: FFA_ID_GET_32 failed\n");
        return ERR_GENERIC;
    }

    *endpoint_id = payload.arg2 & 0xffff;

    printf("FF-A Current Endpoint ID %x\n", *endpoint_id);

    return NO_ERROR;
}

static status_t arm_ffa_map_rxtx(struct ffa_device *dev, void *rx_buffer, void *tx_buffer,
                                 uint32_t page_count) {
    ffa_args_t payload;

    memset(&payload, 0, sizeof(ffa_args_t));
    payload.arg1 = (uint64_t)vaddr_to_paddr(tx_buffer);
    payload.arg2 = (uint64_t)vaddr_to_paddr(rx_buffer);
    payload.arg3 = page_count;

    ffa_rxtx_map_64(&payload);
    if (payload.fid == FFA_ERROR_SMC32) {
        printf("RXTX_MAP failed err 0x%llx\n", payload.arg2);
        return ERR_GENERIC;
    }

    dev->rx_buffer = rx_buffer;
    dev->tx_buffer = tx_buffer;

    return NO_ERROR;
}

static status_t arm_ffa_unmap_rxtx(struct ffa_device *dev) {
    ffa_args_t payload;

    memset(&payload, 0, sizeof(ffa_args_t));
    ffa_rxtx_unmap(&payload);

    if (payload.fid == FFA_ERROR_SMC32) {
        printf("RXTX_UNMAP failed err 0x%llx\n", payload.arg2);
        return ERR_GENERIC;
    }

    dev->rx_buffer = NULL;
    dev->tx_buffer = NULL;

    return NO_ERROR;
}

static status_t arm_ffa_get_partition_info(struct ffa_device *dev,
                                           ffa_partition_info_t **partitions,
                                           size_t *desc_count) {
    ffa_args_t payload;
    const uint32_t null_uuid[4] = {0};

    memset(&payload, 0, sizeof(ffa_args_t));
    payload.arg1 = null_uuid[0];
    payload.arg2 = null_uuid[1];
    payload.arg3 = null_uuid[2];
    payload.arg4 = null_uuid[3];
    payload.arg5 = FFA_PARTITION_INFO_FLAG_RETDESC;
    ffa_partition_info_get(&payload);

    if (payload.fid == FFA_ERROR_SMC32) {
        printf("FFA_PARTITION_INFO_GET failed, FF-A Error %llx\n", payload.fid);
        return ERR_GENERIC;
    }

    if (payload.arg3 != sizeof(ffa_partition_info_t)) {
        printf("Expected desc size %zu, got %llx\n", sizeof(ffa_partition_info_t), payload.arg2);
        return ERR_GENERIC;
    }

    *partitions = (ffa_partition_info_t *)dev->rx_buffer;
    *desc_count = (uint64_t)payload.arg2;

    return NO_ERROR;
}

static void arm_ffa_dump_partition_info(struct ffa_device *dev) {
    ffa_partition_info_t *partitions = dev->partitions;
    size_t desc_count = dev->desc_count;

    printf("FF-A Partition Descriptor count: %ld\n", desc_count);

    for (size_t i = 0; i < desc_count; i++) {
        ffa_partition_info_t *part = &partitions[i];

        printf("+---------------- Partition Descriptor [%ld] ----------------+\n", i);
        printf("| ID            : 0x%08x                               |\n", part->id);
        printf("| Exec Contexts : %-36u     |\n", part->exec_context);
        printf("| Properties    : 0x%08x                               |\n", part->properties);
        printf("| UUID          : %08x-%08x-%08x-%08x      |\n",
                 part->uuid[0], part->uuid[1], part->uuid[2], part->uuid[3]);
        printf("+----------------------------------------------------------+\n");
    }
}

static status_t arm_ffa_rx_release(struct ffa_device *dev) {
    ffa_args_t payload;

    memset(&payload, 0, sizeof(ffa_args_t));
    ffa_rx_release(&payload);

    if (payload.fid == FFA_ERROR_SMC32) {
        printf("Failed to release RX buffer state, FF-A Error %llx\n", payload.fid);
        return ERR_GENERIC;
    }

    return NO_ERROR;
}

struct ffa_device *arm_ffa_get_device(void) {
    return &ffa_dev;
}

status_t arm_ffa_find_partition_by_uuid(struct ffa_device *dev,
                                        const uint32_t uuid[4],
                                        ffa_partition_info_t *out) {
    if (uuid == NULL || out == NULL)
        return ERR_INVALID_ARGS;

    if (dev == NULL || dev->partitions == NULL)
        return ERR_GENERIC;

    for (size_t i = 0; i < dev->desc_count; i++) {
        if (memcmp(dev->partitions[i].uuid, uuid, UUID_SIZE) == 0) {
            *out = dev->partitions[i];
            return NO_ERROR;
        }
    }

    return ERR_NOT_FOUND;
}

status_t arm_ffa_shm_alloc(struct ffa_device *dev,
                           ffa_endpoint_id_t receiver_id, ffa_mem_handle_t *handle,
                           size_t size, void **shm) {
    ffa_args_t payload;
    status_t err = NO_ERROR;
    void *buffer = NULL;

    if (size == 0)
        return ERR_INVALID_ARGS;

    buffer = memalign(PAGE_SIZE, size);
    if (buffer == NULL)
        return ERR_NO_MEMORY;

    uint32_t page_count = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    struct ffa_mem_region_constituent constituents[1];
    uint32_t constituents_count = sizeof(constituents) / sizeof(struct ffa_mem_region_constituent);
    constituents[0].address = (void *)vaddr_to_paddr((void *)buffer);
    constituents[0].page_count = page_count;

    struct ffa_mem_region_init mem_region_init;
    memset(&mem_region_init, 0x0, sizeof(struct ffa_mem_region_init));

    mem_region_init.memory_region = dev->tx_buffer;
    mem_region_init.sender = dev->endpoint_id;
    mem_region_init.receiver = receiver_id;
    mem_region_init.tag = 0;
    mem_region_init.flags = 0;
    mem_region_init.data_access = FFA_DATA_ACCESS_RW;
    mem_region_init.instruction_access = FFA_INSTRUCTION_ACCESS_NOT_SPECIFIED;
    mem_region_init.type = FFA_MEM_NORMAL_MEM;
    mem_region_init.cacheability = FFA_MEM_CACHE_WRITE_BACK;
    mem_region_init.multi_share = false;
    mem_region_init.receiver_count = 1;

    mem_region_init.receivers[0].receiver_permissions.receiver = receiver_id;
    mem_region_init.receivers[0].receiver_permissions.permissions = FFA_DATA_ACCESS_RW;
    mem_region_init.receivers[0].receiver_permissions.flags = 0;
    mem_region_init.receivers[0].impdef.val[0] = 0;
    mem_region_init.receivers[0].impdef.val[1] = 0;

    ffa_memory_region_init(&mem_region_init, constituents, constituents_count);

    memset(&payload, 0, sizeof(ffa_args_t));
    payload.arg1 = mem_region_init.total_length;
    payload.arg2 = mem_region_init.fragment_length;

    ffa_mem_share_64(&payload);

    if (payload.fid == FFA_ERROR_SMC32) {
        err = ERR_GENERIC;
        goto out_free_buffer;
    }

    *handle = payload.arg2;
    *shm = buffer;

    return NO_ERROR;

out_free_buffer:
    free(buffer);

    return err;
}

status_t arm_ffa_shm_free(struct ffa_device *dev, ffa_mem_handle_t handle) {
    ffa_args_t payload;
    memset(&payload, 0, sizeof(ffa_args_t));

    payload.arg1 = (uint32_t)(handle & 0xFFFFFFFF);
    payload.arg2 = (uint32_t)(handle >> 32);
    payload.arg3 = 0;

    ffa_mem_reclaim(&payload);

    if (payload.fid == FFA_ERROR_SMC32)
        return ERR_GENERIC;

    return NO_ERROR;
}

static status_t arm_ffa_init(struct ffa_device *dev) {
    status_t err;
    ffa_endpoint_id_t endpoint_id;
    ffa_partition_info_t *partitions;
    size_t desc_count;
    void *rx_buffer = NULL;
    void *tx_buffer = NULL;

    err = arm_ffa_version(dev);
    if (err != NO_ERROR)
        return err;

    err = arm_ffa_get_id(&endpoint_id);
    if (err != NO_ERROR)
        return err;

    dev->endpoint_id = endpoint_id;

    rx_buffer = memalign(PAGE_SIZE, PAGE_SIZE);
    if (rx_buffer == NULL) {
        printf("Failed to allocate RX buffer\n");
        return ERR_NO_MEMORY;
    }

    tx_buffer = memalign(PAGE_SIZE, PAGE_SIZE);
    if (tx_buffer == NULL) {
        printf("Failed to allocate TX buffer\n");
        err = ERR_NO_MEMORY;
        goto out_free_rx;
    }

    err = arm_ffa_map_rxtx(dev, rx_buffer, tx_buffer, 1);
    if (err != NO_ERROR)
        goto out_free_tx;

    err = arm_ffa_get_partition_info(dev, &partitions, &desc_count);
    if (err != NO_ERROR)
        goto out_unmap_rxtx;

    dev->partitions = partitions;
    dev->desc_count = desc_count;

    arm_ffa_dump_partition_info(dev);

    err = arm_ffa_rx_release(dev);
    if (err != NO_ERROR) {
        printf("Failed to release RX buffer state\n");
        goto out_unmap_rxtx;
    }

    return NO_ERROR;

out_unmap_rxtx:
    (void)arm_ffa_rx_release(dev);
    (void)arm_ffa_unmap_rxtx(dev);

out_free_tx:
    free(tx_buffer);

out_free_rx:
    free(rx_buffer);

    return err;
}

static void arm_ffa_init_hook(uint level) {
    printf("FF-A: Initializing framework at level 0x%x\n", level);

    status_t err = arm_ffa_init(&ffa_dev);
    if (err != NO_ERROR) {
        panic("FF-A initialization failed, cannot proceed boot, err=%d.\n", err);
        return;
    }

    printf("FF-A: Framework successfully initialized\n");
}

LK_INIT_HOOK(arm_ffa, &arm_ffa_init_hook, LK_INIT_LEVEL_PLATFORM);
