#include <gpt.h>

#include <stdlib.h>
#include <string.h>

#include <lk/err.h>
#include <lk/trace.h>
#include <lk/debug.h>

#include <lib/bio.h>
#include <lib/cksum.h>

static status_t gpt_read_pmbr(bdev_t *dev, struct master_boot_record *mbr) {
    if (!mbr) {
        return ERR_NO_MEMORY;
    }

    ssize_t err = bio_read(dev, mbr, 0, dev->block_size);
    if (err < 0) {
        return err;
    }

    if (mbr->signature != MBR_SIGNATURE) {
        return ERR_GENERIC;
    }

    return 0;
}

static status_t gpt_read_header(bdev_t *dev, struct gpt_header *header) {
    uint32_t lba1 = 1 * dev->block_size;

    ssize_t err = bio_read(dev, header, lba1, dev->block_size);
    if (err < 0) {
        return err;
    }

    if (header->signature != GPT_HEADER_SIGNATURE) {
        printf("Invalid GPT header signature: 0x%lx\n", header->signature);
        return ERR_GENERIC;
    }

    return NO_ERROR;
}

static status_t gpt_check_header_crc32(bdev_t *dev, struct gpt_header *header) {
    if (header->header_size < GPT_MINIMUM_HEADER_SIZE || header->header_size > dev->block_size) {
        return ERR_INVALID_ARGS; 
    }

    uint32_t original_crc32 = header->header_crc32;
    header->header_crc32 = 0;

    uint32_t computed_crc32 = crc32(0, (const uint8_t *)header, header->header_size);
    header->header_crc32 = original_crc32;

    if (computed_crc32 != original_crc32) {
        printf("GPT header CRC32 mismatch: computed 0x%08x, expected 0x%08x\n", computed_crc32, original_crc32);
        return ERR_BAD_STATE;
    }

    return NO_ERROR;
}

static struct gpt_partition_entry *gpt_get_entry(
    const void *entry_array,
    uint32_t index,
    const struct gpt_header *header)
{
    if (!entry_array || !header) {
        return NULL;
    }

    if (index >= header->num_partition_entries) {
        return NULL;
    }

    if (header->size_of_partition_entry < sizeof(struct gpt_partition_entry)) {
        return NULL;
    }

    size_t byte_offset = (size_t)index * header->size_of_partition_entry;

    return (struct gpt_partition_entry *)((uint8_t *)entry_array + byte_offset);
}

static ssize_t gpt_read_partition_entry_array(bdev_t *dev, struct gpt_header *header, char *entry_array) {
    uint32_t lba = header->partition_entry_lba * dev->block_size;
    size_t read_size = header->num_partition_entries * header->size_of_partition_entry;

    ssize_t err = bio_read(dev, entry_array, lba, read_size);
    if (err < 0) {
        return err;
    }

    return NO_ERROR;
}

static void gpt_print_pmbr(struct master_boot_record *mbr) {
    printf("Master Boot Record:\n");
    printf("MBR signature: %04x\n", mbr->signature);

    for (size_t i = 0; i < MBR_PARTITION_RECORD_COUNT; i++) {
        printf("Protective MBR Partition Record[%ld]\n", i);
        printf("\tBoot Indicator: 0x%02x\n", mbr->partition[i].boot_indicator);
        printf("\tOS Indicator: 0x%02x\n", mbr->partition[i].os_indicator);
        printf("\tStarting LBA: 0x%02x%02x%02x%02x\n",
            mbr->partition[i].starting_lba[3],
            mbr->partition[i].starting_lba[2],
            mbr->partition[i].starting_lba[1],
            mbr->partition[i].starting_lba[0]);
        printf("\tSize in LBA: 0x%02x%02x%02x%02x\n",
            mbr->partition[i].size_in_lba[3],
            mbr->partition[i].size_in_lba[2],
            mbr->partition[i].size_in_lba[1],
            mbr->partition[i].size_in_lba[0]);
    }
}

static void gpt_print_header(struct gpt_header *header) {
    printf("GPT Header:\n");
    printf("\tSignature: 0x%lx\n", header->signature);
    printf("\tRevision: 0x%08x\n", header->revision);
    printf("\tHeader Size: %u\n", header->header_size);
    printf("\tHeader CRC32: 0x%08x\n", header->header_crc32);
    printf("\tCurrent LBA: 0x%lx\n", header->current_lba);
    printf("\tBackup LBA: 0x%lx\n", header->backup_lba);
    printf("\tFirst Usable LBA: 0x%lx\n", header->first_usable_lba);
    printf("\tLast Usable LBA: 0x%lx\n", header->last_usable_lba);
    printf("\tDisk GUID: %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
        header->disk_guid[0], header->disk_guid[1], header->disk_guid[2], header->disk_guid[3],
        header->disk_guid[4], header->disk_guid[5],
        header->disk_guid[6], header->disk_guid[7],
        header->disk_guid[8], header->disk_guid[9],
        header->disk_guid[10], header->disk_guid[11],
        header->disk_guid[12], header->disk_guid[13],
        header->disk_guid[14], header->disk_guid[15]);
    printf("\tPartition Entry LBA: 0x%lx\n", header->partition_entry_lba);
    printf("\tNumber of Partition Entries: %u\n", header->num_partition_entries);
    printf("\tSize of Partition Entry: %u\n", header->size_of_partition_entry);
    printf("\tPartition Entry Array CRC32: 0x%08x\n", header->partition_entry_array_crc32);
}

static void gpt_print_partition_entry(struct gpt_header *header, char *entry_array) {
    uint8_t empty_guid[16] = {0};

    printf("GPT Partition Entries:\n");
    for (size_t i = 0; i < header->num_partition_entries; i++) {
        struct gpt_partition_entry *entry = gpt_get_entry(entry_array, i, header);
        if (entry == NULL) {
            break;
        }

        if (memcmp(entry, &empty_guid, sizeof(uint8_t) * 16) == 0) {
            continue;
        }

        printf("\tPartition Entry[%ld]:\n", i);
        printf("\t\tPartition Type GUID: %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
            entry->partition_type_guid[0], entry->partition_type_guid[1], entry->partition_type_guid[2], entry->partition_type_guid[3],
            entry->partition_type_guid[4], entry->partition_type_guid[5],
            entry->partition_type_guid[6], entry->partition_type_guid[7],
            entry->partition_type_guid[8], entry->partition_type_guid[9],
            entry->partition_type_guid[10], entry->partition_type_guid[11],
            entry->partition_type_guid[12], entry->partition_type_guid[13],
            entry->partition_type_guid[14], entry->partition_type_guid[15]);
        printf("\t\tUnique Partition GUID: %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
            entry->unique_partition_guid[0], entry->unique_partition_guid[1], entry->unique_partition_guid[2], entry->unique_partition_guid[3],
            entry->unique_partition_guid[4], entry->unique_partition_guid[5],
            entry->unique_partition_guid[6], entry->unique_partition_guid[7],
            entry->unique_partition_guid[8], entry->unique_partition_guid[9],
            entry->unique_partition_guid[10], entry->unique_partition_guid[11],
            entry->unique_partition_guid[12], entry->unique_partition_guid[13],
            entry->unique_partition_guid[14], entry->unique_partition_guid[15]);
        printf("\t\tStarting LBA: 0x%lx\n", entry->starting_lba);
        printf("\t\tEnding LBA: 0x%lx\n", entry->ending_lba);
        printf("\t\tAttributes: 0x%016lx\n", entry->attributes);
        printf("\t\tPartition Name: ");

        uint8_t *entry_bytes = (uint8_t *)entry;
        for (size_t j = 56; j < 128; j += sizeof(uint16_t)) {
            uint16_t wchar = *((uint16_t *)(entry_bytes + j));
            if (wchar == 0)
                break;

            printf("%c", (char)wchar);
        }
        printf("\n");
    }
}

int gpt_probe(bdev_t *dev) {
    status_t err = NO_ERROR;
    struct master_boot_record *mbr = NULL;
    struct gpt_header *gpt_header = NULL;
    char *entry_array = NULL;

    mbr = (struct master_boot_record *)malloc(dev->block_size);
    if (!mbr) {
        err = ERR_NO_MEMORY;
        goto out;
    }

    err = gpt_read_pmbr(dev, mbr);
    if (err < 0) {
        printf("Failed to read Protective MBR\n");
        goto out;
    }

    gpt_print_pmbr(mbr);

    gpt_header = (struct gpt_header *)malloc(dev->block_size);
    if (!gpt_header) {
        err = ERR_NO_MEMORY;
        goto out;
    }

    err = gpt_read_header(dev, gpt_header);
    if (err < 0) {
        printf("Failed to read GPT header\n");
        goto out;
    }

    err = gpt_check_header_crc32(dev, gpt_header);
    if (err < 0) {
        printf("GPT header CRC32 check failed\n");
        goto out;
    }

    printf("GPT header CRC32 check passed\n");

    gpt_print_header(gpt_header);

    entry_array = (char *)malloc(gpt_header->num_partition_entries * gpt_header->size_of_partition_entry);
    if (!entry_array) {
        err = ERR_NO_MEMORY;
        goto out;
    }

    err = gpt_read_partition_entry_array(dev, gpt_header, entry_array);
    if (err < 0) {
        printf("Failed to read GPT partition entry array\n");
        goto out;
    }

    gpt_print_partition_entry(gpt_header, entry_array);

    uint8_t empty_guid[16] = {0};
    char blockdev_name[GPT_PARTITION_ENTRY_NAME_SIZE + 16];
    for (size_t i = 0; i < gpt_header->num_partition_entries; i++) {
        struct gpt_partition_entry *entry = gpt_get_entry(entry_array, i, gpt_header);
        if (entry == NULL)
            break;

        if (memcmp(entry, &empty_guid, sizeof(uint8_t) * 16) == 0)
            continue;

        snprintf(blockdev_name, sizeof(blockdev_name), "%sp%d", dev->name, i + 1);

        bnum_t block_count = entry->ending_lba - entry->starting_lba + 1;
        bnum_t startblock = entry->starting_lba;
        
        printf("Publishing subdevice '%s' for partition %ld: start block 0x%lx, block count 0x%lx\n",
               blockdev_name, i, startblock, block_count);
        status_t err = bio_publish_subdevice(dev->name, blockdev_name, startblock, block_count);
        if (err != NO_ERROR)
            printf("Failed to create subdevice: %s\n", blockdev_name);
        else
            printf("Created subdevice: %s\n",blockdev_name);
    }

out:
    free(mbr);
    free(gpt_header);
    free(entry_array);

    return err;
}
