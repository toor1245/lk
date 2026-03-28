#pragma once

#include "arm_ffa.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define FFA_MAX_MULTI_ENDPOINT_SUPPORT 2
#define EP_MEM_ACCESS_DESC_ARR_OFFSET 0x30U

typedef uint64_t ffa_mem_handle_t;

typedef uint8_t ffa_mem_access_permissions_t;

typedef uint8_t ffa_mem_receiver_flags_t;

typedef uint8_t ffa_mem_attributes_t;

/** Flags to control the behaviour of a memory sharing transaction. */
typedef uint32_t ffa_mem_region_flags_t;

enum ffa_data_access {
    FFA_DATA_ACCESS_NOT_SPECIFIED,
    FFA_DATA_ACCESS_RO,
    FFA_DATA_ACCESS_RW,
    FFA_DATA_ACCESS_RESERVED,
};

enum ffa_instruction_access {
    FFA_INSTRUCTION_ACCESS_NOT_SPECIFIED,
    FFA_INSTRUCTION_ACCESS_NX,
    FFA_INSTRUCTION_ACCESS_X,
    FFA_INSTRUCTION_ACCESS_RESERVED,
};

enum ffa_mem_type {
    FFA_MEM_NOT_SPECIFIED_MEM,
    FFA_MEM_DEVICE_MEM,
    FFA_MEM_NORMAL_MEM,
};

enum ffa_mem_cacheability {
    FFA_MEM_CACHE_RESERVED = 0x0,
    FFA_MEM_CACHE_NON_CACHEABLE = 0x1,
    FFA_MEM_CACHE_RESERVED_1 = 0x2,
    FFA_MEM_CACHE_WRITE_BACK = 0x3,
    FFA_MEM_DEV_NGNRNE = 0x0,
    FFA_MEM_DEV_NGNRE = 0x1,
    FFA_MEM_DEV_NGRE = 0x2,
    FFA_MEM_DEV_GRE = 0x3,
};

enum ffa_mem_shareability {
    FFA_MEM_SHARE_NON_SHAREABLE,
    FFA_MEM_SHARE_RESERVED,
    FFA_MEM_OUTER_SHAREABLE,
    FFA_MEM_INNER_SHAREABLE,
};

enum ffa_mem_security {
    FFA_MEM_SECURITY_UNSPECIFIED = 0,
    FFA_MEM_SECURITY_SECURE = 0,
    FFA_MEM_SECURITY_NON_SECURE,
};

#define FFA_DATA_ACCESS_OFFSET (0x0U)
#define FFA_DATA_ACCESS_MASK ((0x3U) << FFA_DATA_ACCESS_OFFSET)

#define FFA_INSTRUCTION_ACCESS_OFFSET (0x2U)
#define FFA_INSTRUCTION_ACCESS_MASK ((0x3U) << FFA_INSTRUCTION_ACCESS_OFFSET)

#define FFA_MEM_TYPE_OFFSET (0x4U)
#define FFA_MEM_TYPE_MASK ((0x3U) << FFA_MEM_TYPE_OFFSET)

#define FFA_MEM_SECURITY_OFFSET (0x6U)
#define FFA_MEM_SECURITY_MASK ((0x1U) << FFA_MEM_SECURITY_OFFSET)

#define FFA_MEM_CACHEABILITY_OFFSET (0x2U)
#define FFA_MEM_CACHEABILITY_MASK ((0x3U) << FFA_MEM_CACHEABILITY_OFFSET)

#define FFA_MEM_SHAREABILITY_OFFSET (0x0U)
#define FFA_MEM_SHAREABILITY_MASK ((0x3U) << FFA_MEM_SHAREABILITY_OFFSET)

#define ATTR_FUNCTION_SET(name, container_type, offset, mask)                   \
    static inline void ffa_set_##name##_attr(container_type * attr,             \
                         const enum ffa_##name perm)                            \
    {                                                                           \
        *attr = (uint8_t)((*attr & ~(mask)) | ((perm << offset) & mask));       \
    }

#define ATTR_FUNCTION_GET(name, container_type, offset, mask)       \
    static inline enum ffa_##name ffa_get_##name##_attr(            \
        container_type attr)                                        \
    {                                                               \
        return (enum ffa_##name)((attr & mask) >> offset);          \
    }

ATTR_FUNCTION_SET(data_access, ffa_mem_access_permissions_t,
          FFA_DATA_ACCESS_OFFSET, FFA_DATA_ACCESS_MASK)
ATTR_FUNCTION_GET(data_access, ffa_mem_access_permissions_t,
          FFA_DATA_ACCESS_OFFSET, FFA_DATA_ACCESS_MASK)

ATTR_FUNCTION_SET(instruction_access, ffa_mem_access_permissions_t,
          FFA_INSTRUCTION_ACCESS_OFFSET, FFA_INSTRUCTION_ACCESS_MASK)
ATTR_FUNCTION_GET(instruction_access, ffa_mem_access_permissions_t,
          FFA_INSTRUCTION_ACCESS_OFFSET, FFA_INSTRUCTION_ACCESS_MASK)

ATTR_FUNCTION_SET(mem_type, ffa_mem_attributes_t, FFA_MEM_TYPE_OFFSET,
          FFA_MEM_TYPE_MASK)
ATTR_FUNCTION_GET(mem_type, ffa_mem_attributes_t, FFA_MEM_TYPE_OFFSET,
          FFA_MEM_TYPE_MASK)

ATTR_FUNCTION_SET(mem_security, ffa_mem_attributes_t,
          FFA_MEM_SECURITY_OFFSET, FFA_MEM_SECURITY_MASK)
ATTR_FUNCTION_GET(mem_security, ffa_mem_attributes_t,
          FFA_MEM_SECURITY_OFFSET, FFA_MEM_SECURITY_MASK)

ATTR_FUNCTION_SET(mem_cacheability, ffa_mem_attributes_t,
          FFA_MEM_CACHEABILITY_OFFSET, FFA_MEM_CACHEABILITY_MASK)
ATTR_FUNCTION_GET(mem_cacheability, ffa_mem_attributes_t,
          FFA_MEM_CACHEABILITY_OFFSET, FFA_MEM_CACHEABILITY_MASK)

ATTR_FUNCTION_SET(mem_shareability, ffa_mem_attributes_t,
          FFA_MEM_SHAREABILITY_OFFSET, FFA_MEM_SHAREABILITY_MASK)
ATTR_FUNCTION_GET(mem_shareability, ffa_mem_attributes_t,
          FFA_MEM_SHAREABILITY_OFFSET, FFA_MEM_SHAREABILITY_MASK)

/**
  * Struct to store the impdef value seen in Table 11.16 of the
  * FF-A v1.2 ALP0 specification "Endpoint memory access descriptor".
  */
struct ffa_mem_access_impdef {
    uint64_t val[2];
};

struct ffa_mem_region_attributes {
    /** The ID of the VM to which the memory is being given or shared. */
    ffa_endpoint_id_t receiver;
    /**
     * The permissions with which the memory region should be mapped in the
     * receiver's page table.
     */
    ffa_mem_access_permissions_t permissions;
    /**
     * Flags used during FFA_MEM_RETRIEVE_REQ and FFA_MEM_RETRIEVE_RESP
     * for memory regions with multiple borrowers.
     */
    ffa_mem_receiver_flags_t flags;
};

/**
 * This corresponds to table "Endpoint memory access descriptor" of the FFA 1.0
 * specification.
 */
struct ffa_mem_access {
    struct ffa_mem_region_attributes receiver_permissions;
    /**
     * Offset in bytes from the start of the outer `ffa_memory_region` to
     * an `ffa_composite_memory_region` struct.
     */
    uint32_t composite_memory_region_offset;
    struct ffa_mem_access_impdef impdef;
    uint64_t reserved_0;
};

struct ffa_mem_region_constituent {
    /**
     * The base IPA of the constituent memory region, aligned to 4 kiB page
     * size granularity.
     */
    void *address;
    /** The number of 4 kiB pages in the constituent memory region. */
    uint32_t page_count;
    /** Reserved field, must be 0. */
    uint32_t reserved;
};

struct ffa_composite_mem_region {
    /**
     * The total number of 4 kiB pages included in this memory region. This
     * must be equal to the sum of page counts specified in each
     * `ffa_memory_region_constituent`.
     */
    uint32_t page_count;
    /**
     * The number of constituents (`ffa_memory_region_constituent`)
     * included in this memory region range.
     */
    uint32_t constituent_count;
    /** Reserved field, must be 0. */
    uint64_t reserved_0;
    /** An array of `constituent_count` memory region constituents. */
    struct ffa_mem_region_constituent constituents[];
};

/**
  * Information about a set of pages which are being shared. This corresponds to
  * table 10.20 of the FF-A v1.1 EAC0 specification, "Lend, donate or share
  * memory transaction descriptor". Note that it is also used for retrieve
  * requests and responses.
  */
struct ffa_mem_region {
    /**
      * The ID of the VM which originally sent the memory region, i.e. the
      * owner.
      */
    ffa_endpoint_id_t sender;
    ffa_mem_attributes_t attributes;

    /** Flags to control behaviour of the transaction. */
    ffa_mem_region_flags_t flags;
    ffa_mem_handle_t handle;

    /**
      * An implementation defined value associated with the receiver and the
      * memory region.
      */
    uint64_t tag;

    /* Size of the memory access descriptor. */
    uint32_t memory_access_desc_size;

    /**
      * The number of `ffa_memory_access` entries included in this
      * transaction.
      */
    uint32_t receiver_count;

    /**
      * Offset of the 'receivers' field, which relates to the memory access
      * descriptors.
      */
    uint32_t receivers_offset;

    /** Reserved field (12 bytes) must be 0. */
    uint32_t reserved[3];

    /**
      * An array of `receiver_count` endpoint memory access descriptors.
      * Each one specifies a memory region offset, an endpoint and the
      * attributes with which this memory region should be mapped in that
      * endpoint's page table.
      */
    struct ffa_mem_access receivers[];
};

struct ffa_mem_region_init {
    struct ffa_mem_region *memory_region;
    size_t memory_region_max_size;
    ffa_endpoint_id_t sender;
    ffa_endpoint_id_t receiver;
    uint64_t tag;
    ffa_mem_region_flags_t flags;
    struct ffa_mem_access_impdef impdef;
    enum ffa_data_access data_access;
    enum ffa_instruction_access instruction_access;
    enum ffa_mem_type type;
    enum ffa_mem_cacheability cacheability;
    enum ffa_mem_shareability shareability;
    uint32_t total_length;
    uint32_t fragment_length;
    bool multi_share;
    uint32_t receiver_count;
    struct ffa_mem_access receivers[FFA_MAX_MULTI_ENDPOINT_SUPPORT];
};

void ffa_mem_region_init_header(struct ffa_mem_region_init *mem_region_init,
                    ffa_mem_attributes_t attributes,
                    ffa_mem_handle_t handle,
                    ffa_mem_access_permissions_t permissions);

uint32_t ffa_memory_region_init(struct ffa_mem_region_init *mem_region_init,
                    const struct ffa_mem_region_constituent constituents[],
                    uint32_t constituent_count);

static inline struct ffa_composite_mem_region *
ffa_mem_region_get_composite(struct ffa_mem_region *memory_region,
                uint32_t receiver_index) {
    uint32_t offset = memory_region->receivers[receiver_index].composite_memory_region_offset;

    if (offset == 0) {
        return NULL;
    }

    return (struct ffa_composite_mem_region *)(void *)((uint8_t *)memory_region + offset);
}