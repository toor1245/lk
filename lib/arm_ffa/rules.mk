LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_SRCS += \
	$(LOCAL_DIR)/asm_smc.S \
	$(LOCAL_DIR)/arm_ffa.c \

include make/module.mk
