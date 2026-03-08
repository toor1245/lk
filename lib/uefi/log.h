#ifndef __LIB_UEFI_LOG_H_
#define __LIB_UEFI_LOG_H_

#define EFI_LOG(fmt, ...) \
    printf("[UEFI FW] %s: " fmt, __FUNCTION__, ##__VA_ARGS__)

#endif // __LIB_UEFI_LOG_H_