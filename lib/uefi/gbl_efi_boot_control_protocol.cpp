#include "uefi_platform.h"

#include <lk/err.h>
#include <lk/trace.h>
#include <stdio.h>
#include <string.h>
#include <uefi/protocols/gbl_efi_boot_control_protocol.h>
#include <uefi/types.h>

#include <lib/bio.h>
#include <lib/cksum.h>
#include "log.h"
#include "ta_avb_client.h"

#define LOCAL_TRACE 0

constexpr size_t BOOTLOADER_MESSAGE_OFFSET_IN_MISC = 0;
constexpr size_t VENDOR_SPACE_OFFSET_IN_MISC = 2 * 1024;
constexpr size_t WIPE_PACKAGE_OFFSET_IN_MISC = 16 * 1024;
constexpr size_t SYSTEM_SPACE_OFFSET_IN_MISC = 32 * 1024;
constexpr size_t SYSTEM_SPACE_SIZE_IN_MISC = 32 * 1024;

struct misc_virtual_ab_message {
  uint8_t version;
  uint32_t magic;
  uint8_t merge_status;
  uint8_t source_slot;
  uint8_t reserved[57];
} __attribute__((packed));

struct slot_metadata {
  // Slot priority with 15 meaning highest priority, 1 lowest
  // priority and 0 the slot is unbootable.
  uint8_t priority : 4;
  // Number of times left attempting to boot this slot.
  uint8_t tries_remaining : 3;
  // 1 if this slot has booted successfully, 0 otherwise.
  uint8_t successful_boot : 1;
  // 1 if this slot is corrupted from a dm-verity corruption, 0
  // otherwise.
  uint8_t verity_corrupted : 1;
  // Reserved for further use.
  uint8_t reserved : 7;
} __attribute__((packed));

/*
 * https://android.googlesource.com/platform/hardware/interfaces/+/main/boot/1.1/default/boot_control/include/private/boot_control_definition.h
 */
struct bootloader_control {
  // NUL terminated active slot suffix.
  char slot_suffix[4];
  // Bootloader Control AB magic number (see BOOT_CTRL_MAGIC).
  uint32_t magic;
  // Version of struct being used (see BOOT_CTRL_VERSION).
  uint8_t version;
  // Number of slots being managed.
  uint8_t nb_slot : 3;
  // Number of times left attempting to boot recovery.
  uint8_t recovery_tries_remaining : 3;
  // Status of any pending snapshot merge of dynamic partitions.
  uint8_t merge_status : 3;
  // Ensure 4-bytes alignment for slot_info field.
  uint8_t reserved0[1];
  // Per-slot information.  Up to 4 slots.
  struct slot_metadata slot_info[4];
  // Reserved for further use.
  uint8_t reserved1[8];
  // CRC32 of all 28 bytes preceding this field (little endian
  // format).
  uint32_t crc32_le;
} __attribute__((packed));

static_assert(sizeof(struct bootloader_control) == 32, "bootloader_control must be 32 bytes");

/*
 * Bootloader Control Block, at the start of misc.
 * https://android.googlesource.com/platform/bootable/recovery/+/main/bootloader_message/include/bootloader_message/bootloader_message.h
 */
struct bootloader_message {
  char command[32];
  char status[32];
  char recovery[768];
  char stage[32];
  char reserved[1184];
};

static_assert(sizeof(struct bootloader_message) == 2048, "bootloader_message must be 2048 bytes");

#define BCB_COMMAND_RECOVERY   "boot-recovery"

#define BOOT_CTRL_MAGIC   0x42414342 /* Bootloader Control AB */
#define BOOT_CTRL_VERSION 1

#define BOOT_CTRL_DEFAULT_PRIORITY 7
#define BOOT_CTRL_DEFAULT_TRIES 7

#define BOOT_CTRL_ACTIVE_PRIORITY 15
#define BOOT_CTRL_ACTIVE_TRIES 6

#define MAX_NUM_SLOTS 2

static_assert(MAX_NUM_SLOTS <= 4, "slot_info only holds 4 entries");

static char slot_char_from_index(uint8_t index) { return (char)('a' + index); }

static uint32_t bootctrl_crc32(const struct bootloader_control *bootctrl) {
  return (uint32_t)crc32(0, (const unsigned char *)bootctrl,
                         sizeof(*bootctrl) - sizeof(bootctrl->crc32_le));
}

static void bootctrl_dump(const char *tag, const struct bootloader_control *bootctrl) {
  if (!LOCAL_TRACE) {
    return;
  }

  char suffix[sizeof(bootctrl->slot_suffix) + 1];
  for (size_t i = 0; i < sizeof(bootctrl->slot_suffix); i++) {
    char c = bootctrl->slot_suffix[i];
    suffix[i] = (c >= 0x20 && c < 0x7f) ? c : '\0';
  }
  suffix[sizeof(bootctrl->slot_suffix)] = '\0';

  printf("\n[UEFI FW] ===== bootloader_control (%s) =====\n", tag);
  printf("\t- Slot Suffix:      \"%s\"\n", suffix);
  printf("\t- Magic:            0x%08x%s\n", bootctrl->magic,
         bootctrl->magic == BOOT_CTRL_MAGIC ? "" : "  (BAD)");
  printf("\t- Version:          %u\n", (unsigned)bootctrl->version);
  printf("\t- Slot Count:       %u\n", (unsigned)bootctrl->nb_slot);
  printf("\t- Recovery Tries:   %u\n", (unsigned)bootctrl->recovery_tries_remaining);
  printf("\t- Merge Status:     %u\n", (unsigned)bootctrl->merge_status);
  printf("\t- CRC32:            0x%08x%s\n", bootctrl->crc32_le,
         bootctrl->crc32_le == bootctrl_crc32(bootctrl) ? "" : "  (BAD)");

  for (size_t i = 0; i < countof(bootctrl->slot_info); i++) {
    const struct slot_metadata *slot = &bootctrl->slot_info[i];
    printf("\n  Slot[%zu] Metadata:%s\n", i,
           i < bootctrl->nb_slot ? "" : "  (beyond slot count)");
    printf("\t- Suffix:           _%c\n", slot_char_from_index((uint8_t)i));
    printf("\t- Priority:         %u\n", (unsigned)slot->priority);
    printf("\t- Tries Remaining:  %u\n", (unsigned)slot->tries_remaining);
    printf("\t- Successful Boot:  %u\n", (unsigned)slot->successful_boot);
    printf("\t- Verity Corrupted: %u\n", (unsigned)slot->verity_corrupted);
  }
  printf("\n");
}

static void bootctrl_init_default(struct bootloader_control *bootctrl) {
  memset(bootctrl, 0, sizeof(*bootctrl));
  bootctrl->magic = BOOT_CTRL_MAGIC;
  bootctrl->version = BOOT_CTRL_VERSION;
  bootctrl->nb_slot = MAX_NUM_SLOTS;
  bootctrl->recovery_tries_remaining = 0;

  for (uint8_t i = 0; i < MAX_NUM_SLOTS; i++) {
    bootctrl->slot_info[i].priority = BOOT_CTRL_DEFAULT_PRIORITY;
    bootctrl->slot_info[i].tries_remaining = BOOT_CTRL_DEFAULT_TRIES;
    bootctrl->slot_info[i].successful_boot = 0;
  }

  snprintf(bootctrl->slot_suffix, sizeof(bootctrl->slot_suffix), "_%c",
           slot_char_from_index(0));
  bootctrl->crc32_le = bootctrl_crc32(bootctrl);
}

static bdev_t *bootctrl_load(struct bootloader_control *bootctrl) {
  bdev_t *misc = bio_open_by_label("misc");
  if (!misc) {
    return nullptr;
  }

  ssize_t res = bio_read(misc, bootctrl, VENDOR_SPACE_OFFSET_IN_MISC, sizeof(*bootctrl));
  if (res != (ssize_t)sizeof(*bootctrl)) {
    bio_close(misc);
    return nullptr;
  }

  bootctrl_dump("before", bootctrl);

  if (bootctrl->magic == BOOT_CTRL_MAGIC && bootctrl->crc32_le == bootctrl_crc32(bootctrl) &&
      bootctrl->nb_slot >= 1 && bootctrl->nb_slot <= MAX_NUM_SLOTS) {
    return misc;
  }

  bootctrl_init_default(bootctrl);
  bootctrl_dump("before (reinitialized)", bootctrl);
  res = bio_write(misc, bootctrl, VENDOR_SPACE_OFFSET_IN_MISC, sizeof(*bootctrl));
  if (res != (ssize_t)sizeof(*bootctrl)) {
    bio_close(misc);
    return nullptr;
  }

  return misc;
}

static EfiStatus bootctrl_store(bdev_t *misc, struct bootloader_control *bootctrl) {
  bootctrl->crc32_le = bootctrl_crc32(bootctrl);
  bootctrl_dump("after", bootctrl);
  ssize_t res = bio_write(misc, bootctrl, VENDOR_SPACE_OFFSET_IN_MISC, sizeof(*bootctrl));
  if (res != (ssize_t)sizeof(*bootctrl)) {
    return EFI_STATUS_DEVICE_ERROR;
  }
  return EFI_STATUS_SUCCESS;
}

/* The command field need not be NUL terminated, so bound every comparison. */
static bool bcb_command_is(const struct bootloader_message *bcb, const char *cmd) {
  return strncmp(bcb->command, cmd, sizeof(bcb->command)) == 0;
}

static status_t bcb_read(bdev_t *misc, struct bootloader_message *bcb) {
  ssize_t res = bio_read(misc, bcb, BOOTLOADER_MESSAGE_OFFSET_IN_MISC, sizeof(*bcb));
  return res == (ssize_t)sizeof(*bcb) ? NO_ERROR : ERR_IO;
}

static void bcb_dump_field(const char *name, const char *field, size_t len) {
  printf("\t- %-17s", name);
  if (len == 0 || field[0] == '\0') {
    printf("(empty)\n");
    return;
  }
  printf("\"");
  for (size_t i = 0; i < len && field[i] != '\0'; i++) {
    char c = field[i];
    if (c == '\n') {
      printf("\"\n\t  %-17s\"", "");
    } else {
      printf("%c", (c >= 0x20 && c < 0x7f) ? c : '.');
    }
  }
  printf("\"\n");
}

static void bcb_dump(const char *tag, const struct bootloader_message *bcb) {
  if (!LOCAL_TRACE) {
    return;
  }

  printf("\n[UEFI FW] ===== bootloader_message / BCB (%s) =====\n", tag);
  bcb_dump_field("Command:", bcb->command, sizeof(bcb->command));
  bcb_dump_field("Status:", bcb->status, sizeof(bcb->status));
  bcb_dump_field("Recovery:", bcb->recovery, sizeof(bcb->recovery));
  bcb_dump_field("Stage:", bcb->stage, sizeof(bcb->stage));
  printf("\n");
}

static status_t bcb_write(bdev_t *misc, const struct bootloader_message *bcb) {
  ssize_t res = bio_write(misc, bcb, BOOTLOADER_MESSAGE_OFFSET_IN_MISC, sizeof(*bcb));
  return res == (ssize_t)sizeof(*bcb) ? NO_ERROR : ERR_IO;
}

static bool bootctrl_recovery_requested(bdev_t *misc) {
  struct bootloader_message bcb;
  return bcb_read(misc, &bcb) == NO_ERROR && bcb_command_is(&bcb, BCB_COMMAND_RECOVERY);
}

static bool g_no_bootable_slot = false;

static int g_boot_slot = -1;

static EfiStatus bootctrl_request_recovery(bdev_t *misc) {
  struct bootloader_message bcb;
  if (bcb_read(misc, &bcb) != NO_ERROR) {
    return EFI_STATUS_DEVICE_ERROR;
  }

  if (bcb.command[0] != '\0') {
    return EFI_STATUS_SUCCESS;
  }

  memset(bcb.command, 0, sizeof(bcb.command));
  strlcpy(bcb.command, BCB_COMMAND_RECOVERY, sizeof(bcb.command));
  if (bcb_write(misc, &bcb) != NO_ERROR) {
    return EFI_STATUS_DEVICE_ERROR;
  }
  bcb_dump("after recovery request", &bcb);
  return EFI_STATUS_SUCCESS;
}

static bool bootctrl_slot_bootable(const struct slot_metadata *slot) {
  return slot->priority != 0 && slot->tries_remaining != 0;
}

static int bootctrl_valid_slot(const struct bootloader_control *bootctrl) {
  int active = -1;
  uint8_t best_priority = 0;
  for (uint8_t i = 0; i < bootctrl->nb_slot; i++) {
    const struct slot_metadata *slot = &bootctrl->slot_info[i];
    if (!bootctrl_slot_bootable(slot)) {
      continue;
    }
    if (slot->priority > best_priority) {
      best_priority = slot->priority;
      active = i;
    }
  }
  return active;
}

static uint8_t bootctrl_active_slot(const struct bootloader_control *bootctrl) {
  uint8_t active = 0;
  char c = bootctrl->slot_suffix[1];
  if (bootctrl->slot_suffix[0] == '_' && c >= 'a' && c < 'a' + bootctrl->nb_slot) {
    active = (uint8_t)(c - 'a');
  }

  uint8_t best_priority = bootctrl->slot_info[active].priority;
  for (uint8_t i = 0; i < bootctrl->nb_slot; i++) {
    if (bootctrl->slot_info[i].priority > best_priority) {
      best_priority = bootctrl->slot_info[i].priority;
      active = i;
    }
  }
  return active;
}

static void boot_control_dump(const char *tag) {
  if (!LOCAL_TRACE) {
    return;
  }

  struct bootloader_control bootctrl;
  bdev_t *misc = bootctrl_load(&bootctrl);
  if (!misc) {
    TRACEF("%s: no usable bootloader_control in misc\n", tag);
    return;
  }
  bio_close(misc);

  bootctrl_dump(tag, &bootctrl);
}

EfiStatus boot_control_get_slot_count(struct GblEfiBootControlProtocol* self,
                                      uint8_t* slot_count) {
  if (self == nullptr || slot_count == nullptr) {
    return EFI_STATUS_INVALID_PARAMETER;
  }

  struct bootloader_control bootctrl;
  bdev_t *misc = bootctrl_load(&bootctrl);
  if (!misc) {
    return EFI_STATUS_NOT_FOUND;
  }
  bio_close(misc);

  *slot_count = bootctrl.nb_slot;
  LTRACEF("slot_count=%u\n", (unsigned)*slot_count);
  return EFI_STATUS_SUCCESS;
}

EfiStatus boot_control_get_slot_info(struct GblEfiBootControlProtocol* self,
                                     uint8_t index, GblEfiSlotInfo* info) {
  if (info == nullptr) {
    return EFI_STATUS_INVALID_PARAMETER;
  }

  struct bootloader_control bootctrl;
  bdev_t *misc = bootctrl_load(&bootctrl);
  if (!misc) {
    return EFI_STATUS_NOT_FOUND;
  }
  bio_close(misc);

  if (index >= bootctrl.nb_slot) {
    LTRACEF("index %u beyond nb_slot %u\n", (unsigned)index, (unsigned)bootctrl.nb_slot);
    return EFI_STATUS_NOT_FOUND;
  }

  const struct slot_metadata *slot = &bootctrl.slot_info[index];

  info->suffix = (uint32_t)slot_char_from_index(index);
  info->priority = slot->priority;
  info->remaining_tries = slot->tries_remaining;
  info->successful = slot->successful_boot;
  if (slot->verity_corrupted) {
    info->unbootable_reason = GBL_EFI_UNBOOTABLE_REASON_VERIFICATION_FAILURE;
  } else if (!slot->successful_boot && slot->tries_remaining == 0) {
    info->unbootable_reason = GBL_EFI_UNBOOTABLE_REASON_NO_MORE_TRIES;
  } else {
    info->unbootable_reason = GBL_EFI_UNBOOTABLE_REASON_UNKNOWN_REASON;
  }

  return EFI_STATUS_SUCCESS;
}

EfiStatus boot_control_get_current_slot(struct GblEfiBootControlProtocol* self,
                                        GblEfiSlotInfo* info) {
  struct bootloader_control bootctrl;
  bdev_t *misc = bootctrl_load(&bootctrl);
  if (!misc) {
    return EFI_STATUS_NOT_FOUND;
  }

  int active = g_boot_slot >= 0 ? g_boot_slot : bootctrl_valid_slot(&bootctrl);
  if (active < 0 && (g_no_bootable_slot || bootctrl_recovery_requested(misc))) {
    active = bootctrl_active_slot(&bootctrl);
    LTRACEF("no bootable slot, reporting active slot %d for recovery\n", active);
  }
  bio_close(misc);

  if (active < 0) {
    LTRACEF("no bootable slot\n");
    return EFI_STATUS_NOT_FOUND;
  }
  LTRACEF("active slot %d '%c'\n", active, slot_char_from_index((uint8_t)active));

  return boot_control_get_slot_info(self, (uint8_t)active, info);
}

EfiStatus boot_control_set_active_slot(struct GblEfiBootControlProtocol* self,
                                       uint8_t index) {
  LTRACEF("index=%u\n", (unsigned)index);

  if (self == nullptr) {
    return EFI_STATUS_INVALID_PARAMETER;
  }

  if (index >= MAX_NUM_SLOTS) {
    return EFI_STATUS_INVALID_PARAMETER;
  }

  struct bootloader_control bootctrl;
  bdev_t *misc = bootctrl_load(&bootctrl);
  if (!misc) {
    return EFI_STATUS_DEVICE_ERROR;
  }

  if (index >= bootctrl.nb_slot) {
    bio_close(misc);
    return EFI_STATUS_INVALID_PARAMETER;
  }

  for (uint8_t i = 0; i < bootctrl.nb_slot; i++) {
    if (i != index && bootctrl.slot_info[i].priority >= BOOT_CTRL_ACTIVE_PRIORITY) {
      bootctrl.slot_info[i].priority = BOOT_CTRL_ACTIVE_PRIORITY - 1;
    }
  }

  bootctrl.slot_info[index].priority = BOOT_CTRL_ACTIVE_PRIORITY;
  bootctrl.slot_info[index].tries_remaining = BOOT_CTRL_ACTIVE_TRIES;
  bootctrl.slot_info[index].successful_boot = 0;
  bootctrl.slot_info[index].verity_corrupted = 0;

  snprintf(bootctrl.slot_suffix, sizeof(bootctrl.slot_suffix), "_%c",
           slot_char_from_index(index));

  EfiStatus status = bootctrl_store(misc, &bootctrl);
  bio_close(misc);

  return status;
}

EfiStatus boot_control_get_one_shot_boot_mode(struct GblEfiBootControlProtocol* self,
                                              GblEfiOneShotBootMode* mode) {
  return EFI_STATUS_UNSUPPORTED;
}

EfiStatus boot_control_handle_loaded_os(struct GblEfiBootControlProtocol* self,
                                        const GblEfiLoadedOs* os) {
  EFI_LOG("kernel_size=%zu, ramdisk_size=%zu, device_tree_size=%zu\n",
          os->kernel_size, os->ramdisk_size, os->device_tree_size);

  EfiStatus status = ta_avb_lock_boot_state();
  if (status != EFI_STATUS_SUCCESS) {
    EFI_LOG("Failed to lock AVB boot state: %d\n", static_cast<int>(status));
    return EFI_STATUS_DEVICE_ERROR;
  }

  return EFI_STATUS_SUCCESS;
}

EfiStatus bootctrl_mark_boot_attempt() {
  struct bootloader_control bootctrl;
  bdev_t *misc = bootctrl_load(&bootctrl);
  if (!misc) {
    return EFI_STATUS_DEVICE_ERROR;
  }

  struct bootloader_message bcb;
  if (bcb_read(misc, &bcb) == NO_ERROR) {
    bcb_dump("before GBL", &bcb);
  }

  if (bootctrl_recovery_requested(misc)) {
    EFI_LOG("Recovery requested, not spending a boot attempt\n");
    bio_close(misc);
    return EFI_STATUS_SUCCESS;
  }

  int active = bootctrl_valid_slot(&bootctrl);
  if (active < 0) {
    EFI_LOG("No valid slot remaining, requesting recovery\n");
    g_no_bootable_slot = true;
    EfiStatus status = bootctrl_request_recovery(misc);
    bio_close(misc);
    return status == EFI_STATUS_SUCCESS ? EFI_STATUS_NOT_FOUND : status;
  }

  struct slot_metadata *slot = &bootctrl.slot_info[active];
  g_boot_slot = active;

  if (slot->successful_boot) {
    bio_close(misc);
    return EFI_STATUS_SUCCESS;
  }

  slot->tries_remaining--;
  EFI_LOG("Booting slot %c, %u tries remaining\n",
          slot_char_from_index((uint8_t)active), (unsigned)slot->tries_remaining);
  EfiStatus status = bootctrl_store(misc, &bootctrl);
  bio_close(misc);
  return status;
}

GblEfiBootControlProtocol* open_gbl_efi_boot_control_protocol() {
  boot_control_dump("open");

  static GblEfiBootControlProtocol protocol = {
    .revision = GBL_EFI_BOOT_CONTROL_PROTOCOL_REVISION,
    .get_slot_count = boot_control_get_slot_count,
    .get_slot_info = boot_control_get_slot_info,
    .get_current_slot = boot_control_get_current_slot,
    .set_active_slot = boot_control_set_active_slot,
    .get_one_shot_boot_mode = boot_control_get_one_shot_boot_mode,
    .handle_loaded_os = boot_control_handle_loaded_os,
  };
  return &protocol;
}
