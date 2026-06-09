LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_SRCS += $(LOCAL_DIR)/optee_ffa.c

MODULE_DEPS += \
	dev/tee \
	dev/arm_ffa \

include make/module.mk
