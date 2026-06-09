#pragma once

#include <stdint.h>
#include <stddef.h>

typedef uint32_t TEEC_Result;

#define TEEC_ORIGIN_API                      0x00000001
#define TEEC_ORIGIN_COMMS                    0x00000002
#define TEEC_ORIGIN_TEE                      0x00000003
#define TEEC_ORIGIN_TRUSTED_APP              0x00000004

#define TEEC_MEM_INPUT                       0x00000001
#define TEEC_MEM_OUTPUT                      0x00000002

#define TEEC_NONE                     0x0
#define TEEC_VALUE_INPUT              0x1
#define TEEC_VALUE_OUTPUT             0x2
#define TEEC_VALUE_INOUT              0x3
#define TEEC_MEMREF_TEMP_INPUT        0x5
#define TEEC_MEMREF_TEMP_OUTPUT       0x6
#define TEEC_MEMREF_TEMP_INOUT        0x7
#define TEEC_MEMREF_WHOLE             0xC
#define TEEC_MEMREF_PARTIAL_INPUT     0xD
#define TEEC_MEMREF_PARTIAL_OUTPUT    0xE
#define TEEC_MEMREF_PARTIAL_INOUT     0xF

#define TEEC_LOGIN_PUBLIC                    0x00000000
#define TEEC_LOGIN_USER                      0x00000001
#define TEEC_LOGIN_GROUP                     0x00000002
#define TEEC_LOGIN_APPLICATION               0x00000004
#define TEEC_LOGIN_USER_APPLICATION          0x00000005
#define TEEC_LOGIN_GROUP_APPLICATION         0x00000006

#define TEEC_CONFIG_PAYLOAD_REF_COUNT 4

#define TEEC_PARAM_TYPE_GET(p, i) (((p) >> (i * 4)) & 0xF)

#define TEEC_PARAM_TYPES(p0, p1, p2, p3) \
	((p0) | ((p1) << 4) | ((p2) << 8) | ((p3) << 12))

#define TEEC_SUCCESS                      ((TEEC_Result)0x00000000)

/**
 * Generic error code : Generic error
 **/
#define TEEC_ERROR_GENERIC                ((TEEC_Result)0xFFFF0000)

/**
 * Generic error code : The underlying security system denies the access to the
 * object
 **/
#define TEEC_ERROR_ACCESS_DENIED          ((TEEC_Result)0xFFFF0001)

/**
 * Generic error code : The pending operation is cancelled.
 **/
#define TEEC_ERROR_CANCEL                 ((TEEC_Result)0xFFFF0002)

/**
 * Generic error code : The underlying system detects a conflict
 **/
#define TEEC_ERROR_ACCESS_CONFLICT        ((TEEC_Result)0xFFFF0003)

/**
 * Generic error code : Too much data for the operation or some data remain
 * unprocessed by the operation.
 **/
#define TEEC_ERROR_EXCESS_DATA            ((TEEC_Result)0xFFFF0004)

/**
 * Generic error code : Error of data format
 **/
#define TEEC_ERROR_BAD_FORMAT             ((TEEC_Result)0xFFFF0005)

/**
 * Generic error code : The specified parameters are invalid
 **/
#define TEEC_ERROR_BAD_PARAMETERS         ((TEEC_Result)0xFFFF0006)

/**
 * Generic error code : Illegal state for the operation.
 **/
#define TEEC_ERROR_BAD_STATE              ((TEEC_Result)0xFFFF0007)

/**
 * Generic error code : The item is not found
 **/
#define TEEC_ERROR_ITEM_NOT_FOUND         ((TEEC_Result)0xFFFF0008)

/**
 * Generic error code : The specified operation is not implemented
 **/
#define TEEC_ERROR_NOT_IMPLEMENTED        ((TEEC_Result)0xFFFF0009)

/**
 * Generic error code : The specified operation is not supported
 **/
#define TEEC_ERROR_NOT_SUPPORTED          ((TEEC_Result)0xFFFF000A)

/**
 * Generic error code : Insufficient data is available for the operation.
 **/
#define TEEC_ERROR_NO_DATA                ((TEEC_Result)0xFFFF000B)

/**
 * Generic error code : Not enough memory to perform the operation
 **/
#define TEEC_ERROR_OUT_OF_MEMORY          ((TEEC_Result)0xFFFF000C)

/**
 * Generic error code : The service is currently unable to handle the request;
 * try later
 **/
#define TEEC_ERROR_BUSY                   ((TEEC_Result)0xFFFF000D)

/**
 * Generic communication error
 **/
#define TEEC_ERROR_COMMUNICATION          ((TEEC_Result)0xFFFF000E)

/**
 * Generic error code : security violation
 **/
#define TEEC_ERROR_SECURITY               ((TEEC_Result)0xFFFF000F)

/**
 * Generic error code : the buffer is too short
 **/
#define TEEC_ERROR_SHORT_BUFFER           ((TEEC_Result)0xFFFF0010)

/**
 * Error of communication: The target of the connection is dead
 **/
#define TEEC_ERROR_TARGET_DEAD            ((TEEC_Result)0xFFFF3024)

typedef struct {
    uint32_t timeLow;
    uint16_t timeMid;
    uint16_t timeHiAndVersion;
    uint8_t clockSeqAndNode[8];
} TEEC_UUID;

typedef struct {
    void *imp;
} TEEC_Context;

typedef struct {
    void *imp;
} TEEC_Session;

typedef struct {
    void* buffer;
    size_t size;
    uint32_t flags;
    void *imp;
} TEEC_SharedMemory;

typedef struct {
    void* buffer;
    size_t size;
} TEEC_TempMemoryReference;

typedef struct {
    TEEC_SharedMemory* parent;
    size_t size;
    size_t offset;
} TEEC_RegisteredMemoryReference;

typedef struct {
    uint32_t a;
    uint32_t b;
} TEEC_Value;

typedef union {
    TEEC_TempMemoryReference tmpref;
    TEEC_RegisteredMemoryReference memref;
    TEEC_Value value;
} TEEC_Parameter;

typedef struct {
    uint32_t started;
    uint32_t paramTypes;
    TEEC_Parameter params[4];
    void *imp;
} TEEC_Operation;

TEEC_Result TEEC_InitializeContext(
    const char   *name,
    TEEC_Context *context);

void TEEC_FinalizeContext(
    TEEC_Context *context);

TEEC_Result TEEC_RegisterSharedMemory(
    TEEC_Context      *context,
    TEEC_SharedMemory *sharedMem);

TEEC_Result TEEC_AllocateSharedMemory(
    TEEC_Context      *context,
    TEEC_SharedMemory *sharedMem);

void TEEC_ReleaseSharedMemory(
    TEEC_SharedMemory *sharedMem);

TEEC_Result TEEC_OpenSession(
    TEEC_Context    *context,
    TEEC_Session    *session,
    const TEEC_UUID *destination,
    uint32_t        connectionMethod,
    void            *connectionData,
    TEEC_Operation  *operation,
    uint32_t        *returnOrigin);

void TEEC_CloseSession(
    TEEC_Session *session);

TEEC_Result TEEC_InvokeCommand(
    TEEC_Session     *session,
    uint32_t         commandID,
    TEEC_Operation   *operation,
    uint32_t         *returnOrigin);

void TEEC_RequestCancellation(
    TEEC_Operation *operation);