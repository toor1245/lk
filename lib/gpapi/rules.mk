LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_SRCS += \
	$(LOCAL_DIR)/tee_client_api.c \

# 1 - OP_TEE
GPAPI_TEE ?= 1

# 1 - FF-A
GPAPI_TRANSPORT ?= 1

ifeq ($(GPAPI_TEE),1)
  GLOBAL_DEFINES += GPAPI_OPTEE=1
endif

ifeq ($(GPAPI_TRANSPORT),1)
  MODULE_DEPS += lib/arm_ffa
endif

include make/module.mk
