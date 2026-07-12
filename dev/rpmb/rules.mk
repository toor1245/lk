LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_SRCS += \
	$(LOCAL_DIR)/rpmb_core.c \
	$(LOCAL_DIR)/rpmb_trace.c \

include make/module.mk
