/*
 * Copyright (c) 2026 Mykola Hohsadze
 *
 * Use of this source code is governed by a MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT
 */

#include <sys/types.h>
#include <stdint.h>

#include <lk/err.h>
#include <lk/list.h>

#define TEE_UUID_LEN 16

#define TEE_SUCCESS			            0x00000000
#define TEE_ERROR_STORAGE_NOT_AVAILABLE	0xf0100003
#define TEE_ERROR_GENERIC		        0xffff0000
#define TEE_ERROR_EXCESS_DATA		    0xffff0004
#define TEE_ERROR_BAD_PARAMETERS	    0xffff0006
#define TEE_ERROR_ITEM_NOT_FOUND	    0xffff0008
#define TEE_ERROR_NOT_IMPLEMENTED	    0xffff0009
#define TEE_ERROR_NOT_SUPPORTED		    0xffff000a
#define TEE_ERROR_COMMUNICATION		    0xffff000e
#define TEE_ERROR_BUSY                  0xffff000d
#define TEE_ERROR_SECURITY		        0xffff000f
#define TEE_ERROR_SHORT_BUFFER		    0xffff0010
#define TEE_ERROR_OUT_OF_MEMORY		    0xffff000c
#define TEE_ERROR_OVERFLOW              0xffff300f
#define TEE_ERROR_TARGET_DEAD		    0xffff3024
#define TEE_ERROR_STORAGE_NO_SPACE      0xffff3041

#define TEE_PARAM_ATTR_TYPE_NONE            0
#define TEE_PARAM_ATTR_TYPE_VALUE_INPUT     1
#define TEE_PARAM_ATTR_TYPE_VALUE_OUTPUT    2
#define TEE_PARAM_ATTR_TYPE_VALUE_INOUT     3
#define TEE_PARAM_ATTR_TYPE_MEMREF_TEMP_INPUT    5
#define TEE_PARAM_ATTR_TYPE_MEMREF_TEMP_OUTPUT   6
#define TEE_PARAM_ATTR_TYPE_MEMREF_TEMP_INOUT    7

#define TEE_PARAM_TYPE_GET(p, i) (((p) >> (i * 4)) & 0xF)

struct tee_shm {
    uintptr_t addr;
    size_t size;
    void *priv;
};

struct tee_param_tmemref {
    void *buffer;
    size_t size;
    struct tee_shm *shm;
};

struct tee_param_value {
    uint64_t a;
    uint64_t b;
    uint64_t c;
};

union tee_param {
    struct tee_param_tmemref tmem;
    struct tee_param_value value;
};

struct tee_operation {
    uint32_t started;
    uint32_t paramTypes;
    union tee_param params[4];
};

struct tee_open_session_data {
    uint8_t uuid[TEE_UUID_LEN];
	uint32_t class_login;
    uint32_t session_id;
    uint32_t ret;
    uint32_t ret_origin;
};

struct tee_invoke_cmd_data {
    uint32_t cmd_id;
    uint32_t session_id;
    struct tee_operation *operation;
    uint32_t ret;
    uint32_t ret_origin;
};

struct tee_device;
struct tee_context_imp;
struct tee_session_imp;

struct tee_driver_ops {
    int (*init_context)(struct tee_device *dev, struct tee_context_imp **ctx);
    int (*open_session)(struct tee_device *dev,
                        struct tee_open_session_data *data,
                        struct tee_session_imp **session);
    int (*close_session)(struct tee_device *dev);
    int (*invoke_cmd)(struct tee_device *dev, struct tee_invoke_cmd_data *data);
    int (*shm_alloc)(struct tee_device *dev, size_t size, struct tee_shm **shm);
    int (*shm_free)(struct tee_device *dev, struct tee_shm *shm);
};

struct tee_device {
    struct list_node node;
    const char *name;
    const struct tee_driver_ops *ops;
    void *priv;
};

struct tee_context_imp {
    struct tee_device *tee_dev;
    void *priv;
};

struct tee_session_imp {
    struct tee_device *tee_dev;
    uint32_t session_id;
    void *priv;
};

status_t tee_dev_register(struct tee_device *dev);
struct tee_device *tee_dev_get_first(void);
struct tee_device *tee_dev_get_by_name(const char *name);
void tee_dev_free_operation(struct tee_device *dev, struct tee_operation *op);