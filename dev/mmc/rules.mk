LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_SRCS += \
	$(LOCAL_DIR)/mmc_core.c \
	$(LOCAL_DIR)/mmc_bdev.c \
	$(LOCAL_DIR)/mmc_init.c

MODULE_DEPS += \
	lib/bio \
	dev/rpmb \

include make/module.mk
