LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_SRCS += $(LOCAL_DIR)/pl180.c

MODULE_DEPS += \
	dev/mmc

include make/module.mk
