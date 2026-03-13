#!/bin/bash

LK=build-qemu-virt-arm64-test/lk.elf
UEFI=/home/mhohsadze/Projects/edk2/Build/MdeModule/DEBUG_GCC/AARCH64/MyEfiApp/MyEfiApp/DEBUG/MyEfiApp.efi

qemu-system-aarch64 \
    -S \
    -s \
    -cpu max,pauth-impdef=on \
    -smp 4 \
    -m 2048M \
    -machine virt \
    -kernel $LK \
    -drive "driver=raw,file=$UEFI,if=none,id=uefi" \
    -device virtio-blk-device,drive=uefi \
    -nographic
