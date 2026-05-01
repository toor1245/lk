LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_SRCS += $(LOCAL_DIR)/sdhci.c

MODULE_DEPS += \
	dev/mmc

include make/module.mk
