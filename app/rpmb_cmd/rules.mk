LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_SRCS += \
	$(LOCAL_DIR)/rpmb_cmd.c \
	$(LOCAL_DIR)/rpmb_program_key.c \
	$(LOCAL_DIR)/rpmb_read_counter.c \
	$(LOCAL_DIR)/rpmb_read.c \
	$(LOCAL_DIR)/rpmb_write.c \
	$(LOCAL_DIR)/rpmb_hmac.c \

MODULE_DEPS += \
	dev/mmc/sdhci \
	lib/mincrypt

include make/module.mk
