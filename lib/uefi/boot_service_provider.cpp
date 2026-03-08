/*
 * Copyright (C) 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#include "boot_service_provider.h"

#include <lk/compiler.h>
#include <lk/list.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <platform.h>
#include <sys/types.h>
#include <uefi/boot_service.h>
#include <uefi/protocols/block_io_protocol.h>
#include <uefi/protocols/dt_fixup_protocol.h>
#include <uefi/protocols/gbl_efi_image_loading_protocol.h>
#include <uefi/protocols/gbl_efi_os_configuration_protocol.h>
#include <uefi/protocols/loaded_image_protocol.h>
#include <uefi/protocols/unicode_collation_protocol.h>
#include <uefi/types.h>
#include <stdarg.h>

#include "blockio2_protocols.h"
#include "blockio_protocols.h"
#include "events.h"
#include "io_stack.h"
#include "memory_protocols.h"
#include "uefi_platform.h"
#include "log.h"
#include "guid.h"

namespace {

typedef struct {
  struct list_node link;
  struct list_node protocols;
} EfiHandleInternal;

typedef struct {
    struct list_node link;
    EfiGuid guid;
    void* interface;
    struct list_node open_intfs;
} EfiProtocolInterface;

struct list_node handle_list = LIST_INITIAL_VALUE(handle_list);

EfiStatus unload(EfiHandle handle) { return EFI_STATUS_SUCCESS; }

EfiHandleInternal *search_handle_internal(EfiHandle handle) {
  EfiHandleInternal *h;
  
  if (handle == nullptr) {
    return nullptr;
  }
  
  list_for_every_entry(&handle_list, h, EfiHandleInternal, link) { 
    if (reinterpret_cast<EfiHandle>(h) == handle) {
      return h;
    }
  }

  return nullptr;
}

EfiStatus search_protocol(EfiHandleInternal *handle,
                          const EfiGuid *protocol,
                          EfiProtocolInterface **protocol_intf) {
  if (handle == nullptr) {
    EFI_LOG("(EFI_STATUS_INVALID_PARAMETER) internal handle is null\n");
    return EFI_STATUS_INVALID_PARAMETER;
  }

  if (protocol == nullptr) {
    EFI_LOG("(EFI_STATUS_INVALID_PARAMETER) protocol is null\n");
    return EFI_STATUS_INVALID_PARAMETER;
  }

  EfiProtocolInterface *i;
  list_for_every_entry(&handle->protocols, i, EfiProtocolInterface, link) {
    if (guid_eq(&i->guid, protocol)) {
      if (protocol_intf) {
        *protocol_intf = i;
      }
      return EFI_STATUS_SUCCESS;
    }
  }

  EFI_LOG("Protocol %s not found\n", guid_to_string(protocol));
  return EFI_STATUS_NOT_FOUND;
}

EfiStatus handle_protocol(EfiHandle handle,
                          const EfiGuid *protocol,
                          void **intf) {
  if (!handle || !protocol || !intf) {
    return EFI_STATUS_INVALID_PARAMETER;
  }

  EfiHandleInternal *h = search_handle_internal(handle);
  if (!h) {
    EFI_LOG("Invalid handle %p\n", handle);
    return EFI_STATUS_INVALID_PARAMETER;
  }

  EfiProtocolInterface *p = nullptr;
  EfiStatus status = search_protocol(h, protocol, &p);
  if (status != EFI_STATUS_SUCCESS) {
    EFI_LOG("unsupported protocol %s\n", guid_to_string(protocol));
    return EFI_STATUS_UNSUPPORTED;
  }

  *intf = p->interface;

  EFI_LOG("returning interface %p\n", *intf);
  return EFI_STATUS_SUCCESS;
}

EfiStatus register_protocol_notify(const EfiGuid *protocol, EfiEvent event,
                                   void **registration) {
  EFI_LOG("is unsupported\n");
  return EFI_STATUS_UNSUPPORTED;
}

EfiStatus locate_handle(EfiLocateHandleSearchType search_type,
                        const EfiGuid *protocol, const void *search_key,
                        size_t *buf_size, EfiHandle *buf) {
  if (search_type == EFI_LOCATE_HANDLE_SEARCH_TYPE_ALL_HANDLES) {
    return EFI_STATUS_UNSUPPORTED;
  } else if (search_type == EFI_LOCATE_HANDLE_SEARCH_TYPE_BY_REGISTER_NOTIFY) {
    return EFI_STATUS_UNSUPPORTED;
  } else if (search_type == EFI_LOCATE_HANDLE_SEARCH_TYPE_BY_PROTOCOL) {
    if (protocol == nullptr) {
      EFI_LOG("protocol must be provided for EFI_LOCATE_HANDLE_SEARCH_TYPE_BY_PROTOCOL\n");
      return EFI_STATUS_INVALID_PARAMETER;
    }

    size_t size = 0;
    EfiHandleInternal *h;
    list_for_every_entry(&handle_list, h, EfiHandleInternal, link) { 
      if (search_protocol(h, protocol, nullptr) == EFI_STATUS_SUCCESS) {
        size += sizeof(EfiHandle);
        EFI_LOG("Add to locate_handle buffer: %s\n", guid_to_string(protocol));
      }
    }

    if (size == 0) {
      EFI_LOG("No handles for protocol %s\n", guid_to_string(protocol));
      return EFI_STATUS_NOT_FOUND;
    }

    if (buf_size == nullptr) {
      return EFI_STATUS_INVALID_PARAMETER;
    }

    if (*buf_size < size) {
      *buf_size = size;
      return EFI_STATUS_BUFFER_TOO_SMALL;
    }

    *buf_size = size;

    if (buf == nullptr) {
      return EFI_STATUS_INVALID_PARAMETER;
    }

    list_for_every_entry(&handle_list, h, EfiHandleInternal, link) { 
      if (search_protocol(h, protocol, nullptr) == EFI_STATUS_SUCCESS) {
        *buf++ = h;
      }
    }

    return EFI_STATUS_SUCCESS;
  } else {
    EFI_LOG("Unknown search_type %d\n", search_type);
    return EFI_STATUS_INVALID_PARAMETER;
  }

  return EFI_STATUS_SUCCESS;
}

EfiStatus locate_protocol(const EfiGuid *protocol, void *registration,
                          void **intf) {
  if (protocol == nullptr) {
    return EFI_STATUS_INVALID_PARAMETER;
  }
  if (memcmp(protocol, &EFI_RNG_PROTOCOL_GUID, sizeof(*protocol)) == 0) {
    return EFI_STATUS_UNSUPPORTED;
  }
  if (memcmp(protocol, &EFI_TCG2_PROTOCOL_GUID, sizeof(*protocol)) == 0) {
    return EFI_STATUS_NOT_FOUND;
  }

  EFI_LOG("(%s) is unsupported\n", guid_to_string(protocol));
  return EFI_STATUS_UNSUPPORTED;
}

EfiStatus uninstall_multiple_protocol_interfaces(EfiHandle handle, ...) {
  EFI_LOG("is unsupported\n");
  return EFI_STATUS_UNSUPPORTED;
}
EfiStatus calculate_crc32(void *data, size_t len, uint32_t *crc32) {
  EFI_LOG("is unsupported\n");
  return EFI_STATUS_UNSUPPORTED;
}

EfiStatus uninstall_protocol_interface(EfiHandle handle,
                                       const EfiGuid *protocol, void *intf) {
  EFI_LOG("is unsupported\n");
  return EFI_STATUS_UNSUPPORTED;
}

EfiStatus load_image(bool boot_policy, EfiHandle parent_image_handle,
                     EfiDevicePathProtocol *path, void *src, size_t src_size,
                     EfiHandle *image_handle) {
  EFI_LOG("is unsupported\n");
  return EFI_STATUS_UNSUPPORTED;
}

EfiStatus locate_device_path(const EfiGuid *protocol,
                             EfiDevicePathProtocol **path, EfiHandle *device) {
  if (memcmp(protocol, &EFI_LOAD_FILE2_PROTOCOL_GUID,
             sizeof(EFI_LOAD_FILE2_PROTOCOL_GUID)) == 0) {
    return EFI_STATUS_NOT_FOUND;
  }
  EFI_LOG("is unsupported\n");
  return EFI_STATUS_UNSUPPORTED;
}

EfiStatus install_configuration_table(const EfiGuid *guid, void *table) {
  EFI_LOG("is unsupported\n");
  return EFI_STATUS_UNSUPPORTED;
}

void copy_mem(void *dest, const void *src, size_t len) {
  memcpy(dest, src, len);
}
void set_mem(void *buf, size_t len, uint8_t val) { memset(buf, val, len); }

EfiTpl raise_tpl(EfiTpl new_tpl) {
  EFI_LOG("is called %zu\n", new_tpl);
  return EFI_TPL_APPLICATION;
}

void restore_tpl(EfiTpl old_tpl) {
  EFI_LOG("is called %zu\n", old_tpl);
}

EfiStatus open_protocol(EfiHandle handle, const EfiGuid *protocol, const void **intf,
                        EfiHandle agent_handle, EfiHandle controller_handle,
                        EfiOpenProtocolAttributes attr) {
  EFI_LOG("(%s, handle=%p, agent_handle=%p, controller_handle=%p, attr=0x%x)\n",
          guid_to_string(protocol),
          handle,
          agent_handle,
          controller_handle,
          attr);

  if (guid_eq(protocol, LOADED_IMAGE_PROTOCOL_GUID)) {
    auto interface = reinterpret_cast<EfiLoadedImageProtocol *>(
        uefi_malloc(sizeof(EfiLoadedImageProtocol)));
    memset(interface, 0, sizeof(*interface));
    interface->parent_handle = handle;
    interface->image_base = handle;
    *intf = interface;
    return EFI_STATUS_SUCCESS;
  } else if (guid_eq(protocol, EFI_BLOCK_IO_PROTOCOL_GUID)) {
    return open_block_device(handle, intf);
  } else if (guid_eq(protocol, EFI_BLOCK_IO2_PROTOCOL_GUID)) {
    return open_async_block_device(handle, intf);
  } else if (guid_eq(protocol, EFI_DT_FIXUP_PROTOCOL_GUID)) {
    if (intf != nullptr) {
      EfiDtFixupProtocol *fixup = nullptr;
      allocate_pool(EFI_MEMORY_TYPE_BOOT_SERVICES_DATA, sizeof(EfiDtFixupProtocol),
                    reinterpret_cast<void **>(&fixup));
      if (fixup == nullptr) {
        return EFI_STATUS_OUT_OF_RESOURCES;
      }
      fixup->revision = EFI_DT_FIXUP_PROTOCOL_REVISION;
      fixup->fixup = efi_dt_fixup;
      *intf = reinterpret_cast<void *>(fixup);
    }
    return EFI_STATUS_SUCCESS;
  } else if (guid_eq(protocol, EFI_GBL_OS_CONFIGURATION_PROTOCOL_GUID)) {
    GblEfiOsConfigurationProtocol *config = nullptr;
    allocate_pool(EFI_MEMORY_TYPE_BOOT_SERVICES_DATA, sizeof(*config),
                  reinterpret_cast<void **>(&config));
    if (config == nullptr) {
      return EFI_STATUS_OUT_OF_RESOURCES;
    }
    config->revision = GBL_EFI_OS_CONFIGURATION_PROTOCOL_REVISION;
    config->fixup_bootconfig = fixup_bootconfig;
    config->select_device_trees = select_device_trees;
    config->select_fit_configuration = select_fit_configuration;
    *intf = reinterpret_cast<void *>(config);
    return EFI_STATUS_SUCCESS;
  } else if (guid_eq(protocol, EFI_TIMESTAMP_PROTOCOL_GUID)) {
    EfiTimestampProtocol *ts = reinterpret_cast<EfiTimestampProtocol *>(
        uefi_malloc(sizeof(EfiTimestampProtocol)));
    if (ts == nullptr) {
      return EFI_STATUS_OUT_OF_RESOURCES;
    }
    ts->get_timestamp = get_timestamp;
    ts->get_properties = get_timestamp_properties;
    *intf = reinterpret_cast<void *>(ts);
    return EFI_STATUS_SUCCESS;
  } else if (guid_eq(protocol, EFI_ERASE_BLOCK_PROTOCOL_GUID)) {
    return open_efi_erase_block_protocol(handle, intf);
  } else if (guid_eq(protocol, EFI_BOOT_MEMORY_PROTOCOL_GUID)) {
    *intf = open_boot_memory_protocol();
    if (*intf == nullptr) {
      return EFI_STATUS_OUT_OF_RESOURCES;
    }
    return EFI_STATUS_SUCCESS;
  }
  return EFI_STATUS_UNSUPPORTED;
}

EfiStatus close_protocol(EfiHandle handle, const EfiGuid *protocol,
                         EfiHandle agent_handle, EfiHandle controller_handle) {
  
  EFI_LOG("(%s, handle=%p, agent_handle=%p, controller_handle=%p)\n",
          guid_to_string(protocol),
          handle,
          agent_handle,
          controller_handle);

  if (guid_eq(protocol, LOADED_IMAGE_PROTOCOL_GUID)) {
    return EFI_STATUS_SUCCESS;
  } else if (guid_eq(protocol, EFI_DEVICE_PATH_PROTOCOL_GUID)) {
    return EFI_STATUS_SUCCESS;
  } else if (guid_eq(protocol, EFI_BLOCK_IO_PROTOCOL_GUID)) {
    return EFI_STATUS_SUCCESS;
  } else if (guid_eq(protocol, EFI_DT_FIXUP_PROTOCOL_GUID)) {
    return EFI_STATUS_SUCCESS;
  }
  return EFI_STATUS_UNSUPPORTED;
}

EfiStatus locate_handle_buffer(EfiLocateHandleSearchType search_type,
                               const EfiGuid *protocol, const void *search_key,
                               size_t *num_handles, EfiHandle **buf) {
  
  EFI_LOG("%s\n", guid_to_string(protocol));

  if (guid_eq(protocol, EFI_BLOCK_IO_PROTOCOL_GUID)) {
    if (search_type == EFI_LOCATE_HANDLE_SEARCH_TYPE_BY_PROTOCOL) {
      return list_block_devices(num_handles, buf);
    }
    return EFI_STATUS_UNSUPPORTED;
  } else if (guid_eq(protocol, EFI_TEXT_INPUT_PROTOCOL_GUID)) {
    return EFI_STATUS_NOT_FOUND;
  } else if (guid_eq(protocol, EFI_GBL_OS_CONFIGURATION_PROTOCOL_GUID)) {
    if (num_handles != nullptr) {
      *num_handles = 1;
    }
    if (buf != nullptr) {
      *buf = reinterpret_cast<EfiHandle *>(uefi_malloc(sizeof(buf)));
    }
    return EFI_STATUS_SUCCESS;
  } else if (guid_eq(protocol, EFI_DT_FIXUP_PROTOCOL_GUID)) {
    if (num_handles != nullptr) {
      *num_handles = 1;
    }
    if (buf != nullptr) {
      *buf = reinterpret_cast<EfiHandle *>(uefi_malloc(sizeof(buf)));
    }
    return EFI_STATUS_SUCCESS;
  } else if (guid_eq(protocol, EFI_TIMESTAMP_PROTOCOL_GUID)) {
    if (num_handles != nullptr) {
      *num_handles = 1;
    }
    if (buf != nullptr) {
      *buf = reinterpret_cast<EfiHandle *>(uefi_malloc(sizeof(buf)));
    }
    return EFI_STATUS_SUCCESS;
  } else if (guid_eq(protocol, EFI_BOOT_MEMORY_PROTOCOL_GUID)) {
    if (num_handles != nullptr) {
      *num_handles = 1;
    }
    if (buf != nullptr) {
      *buf = reinterpret_cast<EfiHandle*>(uefi_malloc(sizeof(buf)));
    }
    return EFI_STATUS_SUCCESS;
  }
  return EFI_STATUS_UNSUPPORTED;
}

EfiStatus stall(size_t microseconds) {
  uint64_t end_microseconds;

  end_microseconds = current_time_hires() + microseconds;
  while (current_time_hires() < end_microseconds) {
    thread_yield();
  }

  return EFI_STATUS_SUCCESS;
}

EfiStatus free_pages(EfiPhysicalAddr memory, size_t pages) {
  // NOLINTNEXTLINE(performance-no-int-to-ptr)
  return ::free_pages(reinterpret_cast<void *>(memory), pages);
}

EfiStatus create_handle(EfiHandle *handle) {
  EfiHandleInternal *h = reinterpret_cast<EfiHandleInternal *>(
    uefi_malloc(sizeof(EfiHandleInternal))
  );

  if (h == nullptr) {
    return EFI_STATUS_OUT_OF_RESOURCES;
  }

  list_initialize(&h->link);
  list_initialize(&h->protocols);
  list_add_tail(&handle_list, &h->link);

  *handle = reinterpret_cast<EfiHandle>(h);

  return EFI_STATUS_SUCCESS;
}

EfiStatus add_protocol(EfiHandle handle,
                       const EfiGuid *protocol,
                       void *intf) {
  EfiStatus status;

  if (handle == nullptr) {
    EFI_LOG("(EFI_STATUS_INVALID_PARAMETER), handle is null\n");
    return EFI_STATUS_INVALID_PARAMETER;
  }

  if (protocol == nullptr) {
    EFI_LOG("(EFI_STATUS_INVALID_PARAMETER), protocol is null\n");
    return EFI_STATUS_INVALID_PARAMETER;
  }

  if (intf == nullptr) {
    EFI_LOG("(EFI_STATUS_INVALID_PARAMETER), interface is null\n");
    return EFI_STATUS_INVALID_PARAMETER;
  }

  EfiHandleInternal *h = search_handle_internal(handle);
  if (h == nullptr) {
    EFI_LOG("(EFI_STATUS_INVALID_PARAMETER), failed to find internal handle\n");
    return EFI_STATUS_INVALID_PARAMETER;
  }

  status = search_protocol(h, protocol, nullptr);
  if (status != EFI_STATUS_NOT_FOUND) {
    return EFI_STATUS_INVALID_PARAMETER;  
  }

  EfiProtocolInterface *protocol_intf = reinterpret_cast<EfiProtocolInterface *>(
    uefi_malloc(sizeof(EfiProtocolInterface))
  );
  if (protocol_intf == nullptr) {
    return EFI_STATUS_OUT_OF_RESOURCES;
  }

  memcpy(&protocol_intf->guid, protocol, sizeof(EfiGuid));
  protocol_intf->interface = intf;
  list_initialize(&protocol_intf->open_intfs);
  list_add_tail(&h->protocols, &protocol_intf->link);

  EFI_LOG("%s\n", guid_to_string(&protocol_intf->guid));
  
  return EFI_STATUS_SUCCESS;
}

EfiStatus install_protocol_interface(EfiHandle* handle,
                                     const EfiGuid* protocol,
                                     EfiInterfaceType intf_type,
                                     void* intf) {
  EfiStatus status;

  if (!handle || !protocol || intf_type != EFI_INTERFACE_TYPE_EFI_NATIVE_INTERFACE) {
    return EFI_STATUS_INVALID_PARAMETER;
  }

  if (!*handle) {
    status = create_handle(handle);
    if (status != EFI_STATUS_SUCCESS) {
      return status;
    }
    EFI_LOG("(new handle=%p)\n", *handle);
  } else {
    EFI_LOG("(handle=%p)\n", *handle);
  }

  return add_protocol(*handle, protocol, intf);
}

EfiStatus install_multiple_protocol_interfaces_int(EfiHandle* handle,
                                                   va_list ap) {
  const EfiGuid *protocol;
  void *protocol_interface;
  EfiStatus ret = EFI_STATUS_SUCCESS;
  va_list argptr_copy;

  if (handle == nullptr)
    return EFI_STATUS_INVALID_PARAMETER;

  va_copy(argptr_copy, ap);

  for (;;) {
    protocol = va_arg(ap, EfiGuid*);
    if (!protocol) {
      break;
    }
    
    protocol_interface = va_arg(ap, void*);
    ret = install_protocol_interface(handle, protocol,
    			             EFI_INTERFACE_TYPE_EFI_NATIVE_INTERFACE,
    		                     protocol_interface);

    if (ret != EFI_STATUS_SUCCESS) {
      break;
    }
  }

  return ret;
} 

EfiStatus install_multiple_protocol_interfaces(EfiHandle* handle, ...) {
  va_list ap;

  va_start(ap, handle);
  EfiStatus status = install_multiple_protocol_interfaces_int(handle, ap);
  va_end(ap);

  return status;
}

} // namespace

void setup_boot_service_table(EfiBootService *service) {
  service->handle_protocol = handle_protocol;
  service->allocate_pool = allocate_pool;
  service->free_pool = free_pool;
  service->get_memory_map = get_physical_memory_map;
  service->register_protocol_notify = register_protocol_notify;
  service->locate_handle = locate_handle;
  service->locate_protocol = locate_protocol;
  service->allocate_pages = allocate_pages;
  service->free_pages = free_pages;
  service->uninstall_multiple_protocol_interfaces =
      uninstall_multiple_protocol_interfaces;
  service->calculate_crc32 = calculate_crc32;
  service->uninstall_protocol_interface = uninstall_protocol_interface;
  service->load_image = load_image;
  service->locate_device_path = locate_device_path;
  service->install_configuration_table = install_configuration_table;
  service->exit_boot_services = exit_boot_services;
  service->copy_mem = copy_mem;
  service->set_mem = set_mem;
  service->open_protocol = open_protocol;
  service->locate_handle_buffer = locate_handle_buffer;
  service->close_protocol = close_protocol;
  service->wait_for_event =
      switch_stack_wrapper<size_t, EfiEvent *, size_t *, wait_for_event>();
  service->signal_event = switch_stack_wrapper<EfiEvent, signal_event>();
  service->check_event = switch_stack_wrapper<EfiEvent, check_event>();
  service->create_event = create_event;
  service->close_event = close_event;
  service->stall = stall;
  service->raise_tpl = raise_tpl;
  service->restore_tpl = restore_tpl;
  service->set_watchdog_timer = set_watchdog_timer;
  service->install_protocol_interface = install_protocol_interface;
  service->install_multiple_protocol_interfaces = install_multiple_protocol_interfaces;
}
