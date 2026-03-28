#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t client_id;
    uint16_t tee_id;
} TEEC_Context_Imp;

typedef struct {
    uint32_t session_id;
    uint16_t tee_id;
    uint16_t client_id;
    uint64_t handle;
    void *constituent;
} TEEC_Session_Imp;

typedef struct {
    uint64_t ffa_mem_handle;
    bool is_allocated;
} TEEC_SharedMemory_Imp;

typedef struct {
    uint32_t cancel_id;
    TEEC_Session_Imp *session;
    bool cancel_requested;
} TEEC_Operation_Imp;