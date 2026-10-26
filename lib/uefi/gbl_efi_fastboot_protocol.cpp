#include "uefi_platform.h"

#include <lk/trace.h>
#include <stdio.h>
#include <string.h>
#include <uefi/protocols/gbl_efi_fastboot_protocol.h>
#include <uefi/types.h>

#include "log.h"

#define LOCAL_TRACE 1

#define FB_VAR_VERSION "version"
#define FB_VAR_VALUE_MAX 64

#define FB_CHECK_PTRS(...)                                        \
    do {                                                          \
        const void* _p[] = {__VA_ARGS__};                         \
        for (size_t _i = 0; _i < countof(_p); _i++)               \
            if (_p[_i] == nullptr) {                              \
                EFI_LOG("null arg %zu in %s\n", _i, __func__);    \
                return EFI_STATUS_INVALID_PARAMETER;              \
            }                                                     \
    } while (0)

#define FB_NEED_ARGS(argc, n)                                              \
    do {                                                                   \
        if ((argc) != (n)) {                                               \
            EFI_LOG("%s: expected %d args, got %zu\n", __func__, (n), (argc)); \
            return EFI_STATUS_UNSUPPORTED;                                 \
        }                                                                  \
    } while (0)

struct fastboot_cmd {
    const char *name;
    EfiStatus (*handler)(struct GblEfiFastbootProtocol* self,
                         size_t num_args,
                         const EfiChar8* const* args,
                         size_t* buffer_size,
                         EfiChar8* buffer);
};

EfiStatus fastboot_get_version(struct GblEfiFastbootProtocol* self,
                                          size_t num_args,
                                          const EfiChar8* const* args,
                                          size_t* buffer_size,
                                          EfiChar8* buffer) {
    FB_CHECK_PTRS(self, buffer, buffer_size);
    FB_NEED_ARGS(num_args, 1);

    static const char kVersion[] = "0.4";
    const size_t len = sizeof(kVersion) - 1;

    if (*buffer_size < len) {
        *buffer_size = len;
        return EFI_STATUS_BUFFER_TOO_SMALL;
    }

    memcpy(buffer, kVersion, len);
    *buffer_size = len;

    return EFI_STATUS_SUCCESS;
}

static struct fastboot_cmd cmds[] = {
    {FB_VAR_VERSION, fastboot_get_version},
};

EfiStatus fastboot_get_var(struct GblEfiFastbootProtocol* self,
                           /* in */ size_t num_args,
                           /* in */ const EfiChar8* const* args,
                           /* in-out */ size_t* buffer_size,
                           /* out */ EfiChar8* buffer) {

    EFI_LOG("fastboot_get_var: num_args=%zu, args[0]=%s\n", num_args,
             (num_args > 0) ? reinterpret_cast<const char*>(args[0]) : "null");

    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        if (strcmp(reinterpret_cast<const char*>(args[0]), cmds[i].name) == 0) {
            if (cmds[i].handler == nullptr) {
                return EFI_STATUS_UNSUPPORTED;
            }

            return cmds[i].handler(self, num_args, args, buffer_size, buffer);
        }
    }

    return EFI_STATUS_UNSUPPORTED;
}

EfiStatus fastboot_get_var_all(struct GblEfiFastbootProtocol* self,
                               void* context,
                               GetVarAllCallback cb) {
    FB_CHECK_PTRS(self);

    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        if (cmds[i].handler == nullptr) {
            continue;
        }

        const EfiChar8* args[] = {
            reinterpret_cast<const EfiChar8*>(cmds[i].name)};
        EfiChar8 value[FB_VAR_VALUE_MAX];
        size_t value_size = sizeof(value) - 1;

        EfiStatus status =
            cmds[i].handler(self, countof(args), args, &value_size, value);

        if (status != EFI_STATUS_SUCCESS || value_size >= sizeof(value)) {
            EFI_LOG("fastboot_get_var_all: skipping %s, status 0x%zx\n",
                    cmds[i].name, static_cast<size_t>(status));
            continue;
        }
        value[value_size] = '\0';

        cb(context, countof(args), args, value);
    }

    return EFI_STATUS_SUCCESS;
}

static EfiStatus fastboot_send(FastbootMessageSender sender, void* context,
                               GblEfiFastbootMessageType type, const char* msg) {
    return sender(context, type, strlen(msg), reinterpret_cast<const EfiChar8*>(msg));
}

EfiStatus fastboot_command_exec(struct GblEfiFastbootProtocol* self,
                                size_t num_args,
                                const EfiChar8* const* args,
                                size_t download_buffer_size,
                                size_t download_buffer_used_size,
                                uint8_t* download_buffer,
                                GblEfiFastbootCommandExecResult* implementation,
                                FastbootMessageSender sender,
                                void* context) {
    FB_CHECK_PTRS(self, args, implementation);
    if (sender == nullptr || num_args == 0) {
        return EFI_STATUS_INVALID_PARAMETER;
    }

    const char* cmd = reinterpret_cast<const char*>(args[0]);
    EFI_LOG("fastboot_command_exec: %s\n", cmd);

    if (strcmp(cmd, "oem hello") == 0) {
        fastboot_send(sender, context, GBL_EFI_FASTBOOT_MESSAGE_TYPE_INFO,
                      "hello from Little Kernel");
        fastboot_send(sender, context, GBL_EFI_FASTBOOT_MESSAGE_TYPE_OKAY, "");
        *implementation = GBL_EFI_FASTBOOT_COMMAND_EXEC_RESULT_CUSTOM_IMPL;
        return EFI_STATUS_SUCCESS;
    }

    *implementation = GBL_EFI_FASTBOOT_COMMAND_EXEC_RESULT_DEFAULT_IMPL;
    return EFI_STATUS_SUCCESS;
}

GblEfiFastbootProtocol* open_gbl_efi_fastboot_protocol() {
  static GblEfiFastbootProtocol protocol = {
      .revision = GBL_EFI_FASTBOOT_PROTOCOL_REVISION,
      .get_var = fastboot_get_var,
      .get_var_all = fastboot_get_var_all,
      .command_exec = fastboot_command_exec,
  };
  return &protocol;
}
