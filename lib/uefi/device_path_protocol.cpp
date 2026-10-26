/*
 * Copyright (C) 2025 The Android Open Source Project
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

#include "device_path_protocol.h"

#include <lib/minip.h>
#include <stdio.h>
#include <string.h>

#include "memory_protocols.h"
#include "uefi_platform.h"

namespace {

constexpr uint8_t kMessagingDevicePathType = 0x03;
constexpr uint8_t kMessagingMacAddressSubType = 0x0B;
constexpr uint8_t kEndDevicePathType = 0x7F;
constexpr uint8_t kEndEntireSubType = 0xFF;
constexpr uint8_t kIfTypeEthernet = 1;

struct __attribute__((packed)) MacDevicePath {
  EfiDevicePathProtocol header;
  uint8_t mac[32];
  uint8_t if_type;
  EfiDevicePathProtocol end;
};

static_assert(sizeof(MacDevicePath) == 41, "unexpected device path layout");

MacDevicePath device_path;
bool device_path_initialized = false;

void set_node_length(EfiDevicePathProtocol *node, uint16_t length) {
  node->length[0] = length & 0xff;
  node->length[1] = (length >> 8) & 0xff;
}

uint16_t node_length(const EfiDevicePathProtocol *node) {
  return static_cast<uint16_t>(node->length[0] |
                               (static_cast<uint16_t>(node->length[1]) << 8));
}

bool is_end_node(const EfiDevicePathProtocol *node) {
  return node->type == kEndDevicePathType;
}

size_t node_to_ascii(const EfiDevicePathProtocol *node, char *out,
                     size_t out_size) {
  if (node->type == kMessagingDevicePathType &&
      node->sub_type == kMessagingMacAddressSubType &&
      node_length(node) >= sizeof(MacDevicePath::header) + 7) {
    auto *mac_node = reinterpret_cast<const MacDevicePath *>(node);
    return snprintf(out, out_size, "MAC(%02x%02x%02x%02x%02x%02x,0x%x)",
                    mac_node->mac[0], mac_node->mac[1], mac_node->mac[2],
                    mac_node->mac[3], mac_node->mac[4], mac_node->mac[5],
                    mac_node->if_type);
  }

  return snprintf(out, out_size, "Path(%u,%u)", node->type, node->sub_type);
}

EfiChar16 *ascii_to_pool_char16(const char *text, size_t len) {
  auto *out = static_cast<EfiChar16 *>(uefi_malloc((len + 1) * sizeof(EfiChar16)));
  if (out == nullptr) {
    return nullptr;
  }
  for (size_t i = 0; i < len; i++) {
    out[i] = static_cast<unsigned char>(text[i]);
  }
  out[len] = 0;
  return out;
}

EfiChar16 *convert_device_node_to_text(EfiDevicePathProtocol *device_node,
                                       bool display_only,
                                       bool allow_shortcuts) {
  if (device_node == nullptr) {
    return nullptr;
  }

  char buffer[128];
  int written = node_to_ascii(device_node, buffer, sizeof(buffer));
  if (written < 0) {
    return nullptr;
  }

  return ascii_to_pool_char16(buffer, static_cast<size_t>(written));
}

EfiChar16 *convert_device_path_to_text(EfiDevicePathProtocol *path,
                                       bool display_only,
                                       bool allow_shortcuts) {
  if (path == nullptr) {
    return nullptr;
  }

  char buffer[256];
  size_t used = 0;

  for (const EfiDevicePathProtocol *node = path; !is_end_node(node);) {
    const uint16_t len = node_length(node);
    if (len < sizeof(EfiDevicePathProtocol)) {
      break;
    }

    if (used != 0 && used < sizeof(buffer) - 1) {
      buffer[used++] = '/';
    }

    int written = node_to_ascii(node, buffer + used, sizeof(buffer) - used);
    if (written < 0) {
      break;
    }
    used += static_cast<size_t>(written);
    if (used >= sizeof(buffer) - 1) {
      used = sizeof(buffer) - 1;
      break;
    }

    node = reinterpret_cast<const EfiDevicePathProtocol *>(
        reinterpret_cast<const uint8_t *>(node) + len);
  }

  buffer[used] = '\0';
  return ascii_to_pool_char16(buffer, used);
}

EfiDevicePathToTextProtocol device_path_to_text = {
    .convert_device_node_to_text = convert_device_node_to_text,
    .convert_device_path_to_text = convert_device_path_to_text,
};

}  // namespace

EfiDevicePathProtocol *open_device_path_protocol() {
  if (!device_path_initialized) {
    memset(&device_path, 0, sizeof(device_path));

    device_path.header.type = kMessagingDevicePathType;
    device_path.header.sub_type = kMessagingMacAddressSubType;
    set_node_length(&device_path.header,
                    sizeof(device_path.header) + sizeof(device_path.mac) +
                        sizeof(device_path.if_type));
    minip_get_macaddr(device_path.mac);
    device_path.if_type = kIfTypeEthernet;

    device_path.end.type = kEndDevicePathType;
    device_path.end.sub_type = kEndEntireSubType;
    set_node_length(&device_path.end, sizeof(device_path.end));

    device_path_initialized = true;
  }

  return &device_path.header;
}

EfiDevicePathToTextProtocol *open_device_path_to_text_protocol() {
  return &device_path_to_text;
}
