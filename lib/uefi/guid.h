#ifndef __LIB_UEFI_GUID_H_
#define __LIB_UEFI_GUID_H_

#include <string.h>
#include <stdio.h>
#include <uefi/types.h>
#include "boot_service_provider.h"

bool guid_eq(const EfiGuid *a, const EfiGuid *b) {
  return memcmp(a, b, sizeof(*a)) == 0;
}

bool guid_eq(const EfiGuid *a, const EfiGuid &b) {
  return memcmp(a, &b, sizeof(*a)) == 0;
}

static const char *guid_to_string(const EfiGuid *g) {
  if (guid_eq(g, LOADED_IMAGE_PROTOCOL_GUID))
    return "LOADED_IMAGE_PROTOCOL";
  if (guid_eq(g, EFI_DEVICE_PATH_PROTOCOL_GUID))
    return "EFI_DEVICE_PATH_PROTOCOL";
  if (guid_eq(g, EFI_BLOCK_IO_PROTOCOL_GUID))
    return "EFI_BLOCK_IO_PROTOCOL";
  if (guid_eq(g, EFI_BLOCK_IO2_PROTOCOL_GUID))
    return "EFI_BLOCK_IO2_PROTOCOL";
  if (guid_eq(g, EFI_DT_FIXUP_PROTOCOL_GUID))
    return "EFI_DT_FIXUP_PROTOCOL";
  if (guid_eq(g, EFI_RNG_PROTOCOL_GUID))
    return "EFI_RNG_PROTOCOL";
  if (guid_eq(g, EFI_TCG2_PROTOCOL_GUID))
    return "EFI_TCG2_PROTOCOL";
  if (guid_eq(g, EFI_LOAD_FILE2_PROTOCOL_GUID))
    return "EFI_LOAD_FILE2_PROTOCOL";
  if (guid_eq(g, EFI_TEXT_INPUT_PROTOCOL_GUID))
    return "EFI_TEXT_INPUT_PROTOCOL";
  if (guid_eq(g, EFI_GBL_VENDOR_MEDIA_DEVICE_PATH_GUID))
    return "EFI_GBL_VENDOR_MEDIA_DEVICE_PATH";
  if (guid_eq(g, LINUX_EFI_LOADED_IMAGE_FIXED_GUID))
    return "LINUX_EFI_LOADED_IMAGE_FIXED";
  if (guid_eq(g, EFI_GBL_OS_CONFIGURATION_PROTOCOL_GUID))
    return "EFI_GBL_OS_CONFIGURATION_PROTOCOL";
  if (guid_eq(g, EFI_GBL_EFI_IMAGE_LOADING_PROTOCOL_GUID))
    return "EFI_GBL_EFI_IMAGE_LOADING_PROTOCOL";
  if (guid_eq(g, EFI_GBL_EFI_BOOT_CONTROL_PROTOCOL_GUID))
    return "EFI_GBL_EFI_BOOT_CONTROL_PROTOCOL";
  if (guid_eq(g, EFI_GBL_EFI_FASTBOOT_PROTOCOL_GUID))
    return "EFI_GBL_EFI_FASTBOOT_PROTOCOL";
  if (guid_eq(g, EFI_GBL_EFI_FASTBOOT_TRANSPORT_PROTOCOL_GUID))
    return "EFI_GBL_EFI_FASTBOOT_TRANSPORT_PROTOCOL";
  if (guid_eq(g, EFI_TIMESTAMP_PROTOCOL_GUID))
    return "EFI_TIMESTAMP_PROTOCOL";
  if (guid_eq(g, EFI_BOOT_MEMORY_PROTOCOL_GUID))
    return "EFI_BOOT_MEMORY_PROTOCOL";
  if (guid_eq(g, EFI_ERASE_BLOCK_PROTOCOL_GUID))
    return "EFI_ERASE_BLOCK_PROTOCOL";
  if (guid_eq(g, EFI_UNICODE_COLLATION_PROTOCOL_GUID))
    return "EFI_UNICODE_COLLATION_PROTOCOL";
  if (guid_eq(g, EFI_UNICODE_COLLATION_PROTOCOL2_GUID))
    return "EFI_UNICODE_COLLATION_PROTOCOL2";

  static char buf[48];

  snprintf(buf,
           sizeof(buf),
           "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
           g->data1,
           g->data2,
           g->data3,
           g->data4[0], g->data4[1],
           g->data4[2], g->data4[3],
           g->data4[4], g->data4[5],
           g->data4[6], g->data4[7]);

  return buf;
}

#endif