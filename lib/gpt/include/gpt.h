#pragma once

#include <stdint.h>
#include <lib/bio.h>

#define MBR_PARTITION_RECORD_COUNT 4

struct mbr_partition_record {
    uint8_t boot_indicator;
    uint8_t start_head;
    uint8_t start_sector;
    uint8_t start_track;
    uint8_t os_indicator;
    uint8_t ending_head;
    uint8_t ending_sector;
    uint8_t ending_track;
    uint8_t starting_lba[4];
    uint8_t size_in_lba[4];
} __attribute__((packed));

#define MBR_SIGNATURE 0xAA55

struct master_boot_record {
    uint8_t boot_code[440];
    uint8_t unique_mbr_signature[4];
    uint8_t unknown[2];
    struct mbr_partition_record partition[MBR_PARTITION_RECORD_COUNT];
    uint16_t signature;
} __attribute__((packed));


#define GPT_HEADER_SIGNATURE 0x5452415020494645
#define GPT_MINIMUM_HEADER_SIZE 92

struct gpt_header {
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t reserved;
    uint64_t current_lba;
    uint64_t backup_lba;
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;
    uint8_t disk_guid[16];
    uint64_t partition_entry_lba;
    uint32_t num_partition_entries;
    uint32_t size_of_partition_entry;
    uint32_t partition_entry_array_crc32;
} __attribute__((packed));

#define GPT_PARTITION_ENTRY_NAME_SIZE 36

struct gpt_partition_entry {
  uint8_t partition_type_guid[16];
  uint8_t unique_partition_guid[16];
  uint64_t starting_lba;
  uint64_t ending_lba;
  uint64_t attributes;
  uint16_t partition_name[GPT_PARTITION_ENTRY_NAME_SIZE];
} __attribute__((packed));

int gpt_probe(bdev_t *dev);