#include "uefi_platform.h"

#include <lk/trace.h>
#include <stdio.h>
#include <string.h>
#include <uefi/protocols/gbl_efi_os_configuration_protocol.h>
#include <uefi/types.h>

namespace {

bool select_first_dtb(size_t num_device_trees,
                      GblEfiVerifiedDeviceTree* device_trees,
                      GblEfiDeviceTreeType type) {
  for (size_t i = 0; i < num_device_trees; i++) {
    if (device_trees[i].metadata.type == type) {
      device_trees[i].selected = true;
      return true;
    }
  }
  return false;
}

}  // namespace

// Selects which device trees and overlays to use from those loaded by GBL.
EfiStatus select_device_trees(struct GblEfiOsConfigurationProtocol* self,
                                     size_t num_device_trees,
                                     GblEfiVerifiedDeviceTree* device_trees) {
  printf("%s(%p, %p %lu)\n", __FUNCTION__, self, device_trees,
         num_device_trees);
  if (num_device_trees > 0) {
    if (device_trees == nullptr) {
      return EFI_STATUS_INVALID_PARAMETER;
    }
    for (size_t i = 0; i < num_device_trees; i++) {
      device_trees[i].selected = false;
    }
    if (!select_first_dtb(num_device_trees, device_trees,
                          GBL_EFI_DEVICE_TREE_TYPE_DEVICE_TREE)) {
      return EFI_STATUS_UNSUPPORTED;
    }
    select_first_dtb(num_device_trees, device_trees,
                     GBL_EFI_DEVICE_TREE_TYPE_OVERLAY);
  }
  return EFI_STATUS_SUCCESS;
}

EfiStatus select_fit_configuration(
    struct GblEfiOsConfigurationProtocol* self, size_t fit_size, const uint8_t* fit,
    size_t metadata_size, const uint8_t* metadata, size_t* selected_configuration_offset) {
  printf("%s(%p, %lu, %p, %lu, %p, %p)\n", __FUNCTION__, self, fit_size, fit, metadata_size,
         metadata, selected_configuration_offset);
  return EFI_STATUS_UNSUPPORTED;
}

// Generates fixups for the bootconfig built by GBL.
EfiStatus fixup_bootconfig(struct GblEfiOsConfigurationProtocol* self,
                                  size_t bootconfig_size,
                                  const uint8_t* bootconfig,
                                  size_t* fixup_buffer_size, uint8_t* fixup) {
  printf("%s(%p, %s, %lu, %lu)\n", __FUNCTION__, self,
         reinterpret_cast<const char*>(bootconfig), bootconfig_size,
         *fixup_buffer_size);
  constexpr auto&& to_add =
      "\nandroidboot.boot_devices=a003a00.virtio_mmio";
  const auto final_len = sizeof(to_add);
  if (final_len > *fixup_buffer_size) {
    *fixup_buffer_size = final_len;
    return EFI_STATUS_OUT_OF_RESOURCES;
  }
  *fixup_buffer_size = final_len;
  memcpy(fixup, to_add, final_len);

  return EFI_STATUS_SUCCESS;
}

// Generates fixups for the kernel command line built by GBL.
EfiStatus fixup_kernel_commandline(struct GblEfiOsConfigurationProtocol* self,
                                   size_t commandline_size,
                                   const EfiChar8* commandline,
                                   size_t* fixup_buffer_size, EfiChar8* fixup) {
  printf("%s(%p, %s, %lu, %lu)\n", __FUNCTION__, self,
         reinterpret_cast<const char*>(commandline), commandline_size,
         *fixup_buffer_size);
  constexpr auto&& to_add = "console=ttyAMA0 earlycon=pl011,mmio32,0x9000000";
  const auto final_len = sizeof(to_add) - 1;
  if (final_len > *fixup_buffer_size) {
    *fixup_buffer_size = final_len;
    return EFI_STATUS_BUFFER_TOO_SMALL;
  }
  *fixup_buffer_size = final_len;
  memcpy(fixup, to_add, final_len);

  return EFI_STATUS_SUCCESS;
}

GblEfiOsConfigurationProtocol* open_gbl_os_configuration_protocol() {
  static GblEfiOsConfigurationProtocol protocol = {
    .revision = GBL_EFI_OS_CONFIGURATION_PROTOCOL_REVISION,
    .fixup_bootconfig = fixup_bootconfig,
    .select_device_trees = select_device_trees,
    .select_fit_configuration = select_fit_configuration,
    .fixup_kernel_commandline = fixup_kernel_commandline,
  };
  return &protocol;
}
