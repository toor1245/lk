#pragma once

#include <stdint.h>
#include <tee_client_api.h>
#include <uefi/types.h>

EfiStatus teec_to_efi_status(TEEC_Result res);

class TaAvbSession {
 public:
  TaAvbSession();
  ~TaAvbSession() = default;

  TaAvbSession(const TaAvbSession&) = delete;
  TaAvbSession& operator=(const TaAvbSession&) = delete;

  bool valid() const;
  TEEC_Result invoke(uint32_t cmd, TEEC_Operation* op);
};

EfiStatus ta_avb_lock_boot_state();

void ta_avb_release();
