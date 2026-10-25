# main project for qemu-aarch64
MODULES += \
	app/shell \
	app/rpmb_cmd \
    lib/uefi

include project/virtual/test.mk
include project/virtual/fs.mk
include project/virtual/minip.mk
include project/target/qemu-virt-arm64.mk

USE_RUST ?= 0

ifeq ($(call TOBOOL,$(USE_RUST)),true)
$(info "Including rust support")
include project/virtual/rust.mk
endif

# Leave secondary CPUs for the OS to start.
GLOBAL_DEFINES += FDTWALK_LEAVE_SECONDARY_CPUS_OFF=1
