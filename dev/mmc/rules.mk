LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_SRCS += \
	$(LOCAL_DIR)/mmc.c

MODULE_DEPS += \
	lib/bio

include make/module.mk
