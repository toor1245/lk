#include "ta_avb_client.h"

#include <lk/trace.h>
#include <ta_avb.h>

namespace {

TEEC_Context avb_ctx = {};
TEEC_Session avb_sess = {};
bool avb_ctx_initialized = false;
bool avb_session_open = false;

}  // namespace

EfiStatus teec_to_efi_status(TEEC_Result res) {
  switch (res) {
    case TEEC_SUCCESS:
      return EFI_STATUS_SUCCESS;
    case TEEC_ERROR_ITEM_NOT_FOUND:
      return EFI_STATUS_NOT_FOUND;
    case TEEC_ERROR_SHORT_BUFFER:
      return EFI_STATUS_BUFFER_TOO_SMALL;
    case TEEC_ERROR_OUT_OF_MEMORY:
      return EFI_STATUS_OUT_OF_RESOURCES;
    case TEEC_ERROR_BAD_PARAMETERS:
      return EFI_STATUS_INVALID_PARAMETER;
    case TEEC_ERROR_ACCESS_DENIED:
    case TEEC_ERROR_SECURITY:
      return EFI_STATUS_ACCESS_DENIED;
    case TEEC_ERROR_NOT_IMPLEMENTED:
    case TEEC_ERROR_NOT_SUPPORTED:
      return EFI_STATUS_UNSUPPORTED;
    default:
      return EFI_STATUS_DEVICE_ERROR;
  }
}

TaAvbSession::TaAvbSession() {
  if (avb_session_open) {
    return;
  }

  if (!avb_ctx_initialized) {
    TEEC_Result res = TEEC_InitializeContext(nullptr, &avb_ctx);
    if (res != TEEC_SUCCESS) {
      TRACEF("TEEC_InitializeContext failed 0x%x\n", res);
      return;
    }
    avb_ctx_initialized = true;
  }

  const TEEC_UUID uuid = TA_AVB_UUID;
  uint32_t err_origin = 0;
  TEEC_Result res = TEEC_OpenSession(&avb_ctx, &avb_sess, &uuid, TEEC_LOGIN_PUBLIC,
                                     nullptr, nullptr, &err_origin);
  if (res != TEEC_SUCCESS) {
    TRACEF("TEEC_OpenSession failed 0x%x origin 0x%x\n", res, err_origin);
    return;
  }
  avb_session_open = true;
}

bool TaAvbSession::valid() const { return avb_session_open; }

TEEC_Result TaAvbSession::invoke(uint32_t cmd, TEEC_Operation* op) {
  uint32_t err_origin = 0;
  TEEC_Result res = TEEC_InvokeCommand(&avb_sess, cmd, op, &err_origin);
  if (res != TEEC_SUCCESS) {
    TRACEF("TEEC_InvokeCommand(%u) failed 0x%x origin 0x%x\n", cmd, res, err_origin);
    if (res == TEEC_ERROR_COMMUNICATION || res == TEEC_ERROR_TARGET_DEAD) {
      TEEC_CloseSession(&avb_sess);
      avb_session_open = false;
    }
  }
  return res;
}

EfiStatus ta_avb_lock_boot_state() {
  TaAvbSession session;
  if (!session.valid()) {
    return EFI_STATUS_DEVICE_ERROR;
  }

  TEEC_Operation op = {};
  op.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE, TEEC_NONE, TEEC_NONE);
  return teec_to_efi_status(session.invoke(TA_AVB_CMD_LOCK_BOOT_STATE, &op));
}

void ta_avb_release() {
  if (avb_session_open) {
    TEEC_CloseSession(&avb_sess);
    avb_session_open = false;
  }
  if (avb_ctx_initialized) {
    TEEC_FinalizeContext(&avb_ctx);
    avb_ctx_initialized = false;
  }
}
