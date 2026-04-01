LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_SRCS += \
    $(LOCAL_DIR)/mmc.c \
    $(LOCAL_DIR)/mmc_bdev.c

GLOBAL_DEFINES += \
    WITH_DEV_MMC=1

MODULE_DEPS += \
    lib/bio

include make/module.mk
