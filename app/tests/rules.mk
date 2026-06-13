LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_SRCS := \
    $(LOCAL_DIR)/cache_tests.c \
    $(LOCAL_DIR)/clock_tests.c \
    $(LOCAL_DIR)/fibo.c \
    $(LOCAL_DIR)/mem_tests.c \
    $(LOCAL_DIR)/port_tests.c \
    $(LOCAL_DIR)/tests.c \
    $(LOCAL_DIR)/thread_tests.c \
    $(LOCAL_DIR)/ta_test.c \
    $(LOCAL_DIR)/mmc_tests.c \
    $(LOCAL_DIR)/gpt_tests.c \
    $(LOCAL_DIR)/avb_test.c \

MODULE_FLOAT_SRCS := \
    $(LOCAL_DIR)/benchmarks.c \
    $(LOCAL_DIR)/float.c \
    $(LOCAL_DIR)/float_instructions.S \

MODULE_DEPS += \
    lib/cbuf \
    lib/libm \
    lib/gpapi \
    lib/fs \
    lib/fs/ext2 \
    lib/fs/fat \
    lib/gpt \

MODULE_COMPILEFLAGS += -fno-builtin

include make/module.mk
