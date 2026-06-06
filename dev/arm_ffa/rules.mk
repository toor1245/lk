LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_SRCS += \
	$(LOCAL_DIR)/arm_ffa.c \

MODULE_DEPS += \
	lib/arm_ffa \

include make/module.mk
