#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/block.h"
#include "../src/partition.h"

typedef struct {
    uint8_t *bytes;
    uint64_t sectors;
    unsigned reads;
    unsigned writes;
} memory_disk_t;

static int memory_read(
    void *context,
    uint64_t lba,
    uint32_t count,
    void *buffer
) {
    memory_disk_t *disk = context;
    if (!disk || !buffer || lba >= disk->sectors ||
        (uint64_t)count > disk->sectors - lba) {
        return -1;
    }

    memcpy(
        buffer,
        disk->bytes + lba * JOSH_BLOCK_SECTOR_SIZE,
        (size_t)count * JOSH_BLOCK_SECTOR_SIZE);
    ++disk->reads;
    return 0;
}

static int memory_write(
    void *context,
    uint64_t lba,
    uint32_t count,
    const void *buffer
) {
    memory_disk_t *disk = context;
    if (!disk || !buffer || lba >= disk->sectors ||
        (uint64_t)count > disk->sectors - lba) {
        return -1;
    }

    memcpy(
        disk->bytes + lba * JOSH_BLOCK_SECTOR_SIZE,
        buffer,
        (size_t)count * JOSH_BLOCK_SECTOR_SIZE);
    ++disk->writes;
    return 0;
}

static void put32(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static void put64(uint8_t *p, uint64_t value) {
    put32(p, (uint32_t)value);
    put32(p + 4, (uint32_t)(value >> 32));
}

static uint32_t crc32_update(
    uint32_t crc,
    const uint8_t *data,
    size_t length
) {
    crc = ~crc;
    while (length--) {
        crc ^= *data++;
        for (unsigned bit = 0; bit < 8; ++bit) {
            uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

static memory_disk_t make_disk(uint64_t sectors) {
    memory_disk_t disk = {
        .bytes = calloc((size_t)sectors, JOSH_BLOCK_SECTOR_SIZE),
        .sectors = sectors
    };
    assert(disk.bytes);
    return disk;
}

typedef struct {
    josh_partition_t items[8];
    unsigned count;
} collected_partitions_t;

static void collect_partition(
    const josh_partition_t *partition,
    void *context
) {
    collected_partitions_t *collected = context;
    assert(collected->count < 8);
    collected->items[collected->count++] = *partition;
}

static void test_block_bounds_and_write(void) {
    memory_disk_t disk = make_disk(8);
    josh_block_device_t device = {
        .context = &disk,
        .sector_count = disk.sectors,
        .read = memory_read,
        .write = memory_write
    };

    uint8_t sector[JOSH_BLOCK_SECTOR_SIZE];
    memset(sector, 0xA5, sizeof(sector));

    assert(josh_block_write(&device, 7, 1, sector) == 0);
    memset(sector, 0, sizeof(sector));
    assert(josh_block_read(&device, 7, 1, sector) == 0);
    assert(sector[0] == 0xA5 && sector[511] == 0xA5);
    assert(josh_block_read(&device, 8, 1, sector) == -1);
    assert(josh_block_write(&device, 7, 2, sector) == -1);
    assert(josh_block_read(&device, UINT64_MAX, 1, sector) == -1);

    free(disk.bytes);
}

static void write_mbr_entry(
    uint8_t *mbr,
    unsigned index,
    uint8_t status,
    uint8_t type,
    uint32_t first,
    uint32_t count
) {
    uint8_t *entry = mbr + 446 + index * 16;
    entry[0] = status;
    entry[4] = type;
    put32(entry + 8, first);
    put32(entry + 12, count);
}

static void test_mbr(void) {
    memory_disk_t disk = make_disk(4096);
    uint8_t *mbr = disk.bytes;
    mbr[510] = 0x55;
    mbr[511] = 0xAA;
    write_mbr_entry(mbr, 0, 0x80, 0x0C, 2048, 512);
    write_mbr_entry(mbr, 1, 0x00, 0x83, 2560, 1024);

    josh_block_device_t device = {
        .context = &disk,
        .sector_count = disk.sectors,
        .read = memory_read,
        .write = memory_write
    };

    josh_partition_summary_t summary;
    collected_partitions_t collected = {0};
    assert(josh_partition_scan(
        &device, &summary, collect_partition, &collected) ==
        JOSH_PARTITION_OK);
    assert(summary.scheme == JOSH_PARTITION_SCHEME_MBR);
    assert(summary.partition_count == 2);
    assert(collected.count == 2);
    assert(collected.items[0].bootable == 1);
    assert(collected.items[0].first_lba == 2048);
    assert(collected.items[0].sector_count == 512);
    assert(collected.items[0].mbr_type == 0x0C);
    assert(collected.items[1].bootable == 0);
    assert(collected.items[1].mbr_type == 0x83);

    mbr[446] = 0x7F;
    assert(josh_partition_scan(&device, &summary, 0, 0) ==
           JOSH_PARTITION_CORRUPT);

    free(disk.bytes);
}

static void build_gpt(memory_disk_t *disk) {
    uint8_t *mbr = disk->bytes;
    mbr[510] = 0x55;
    mbr[511] = 0xAA;
    write_mbr_entry(
        mbr, 0, 0x00, 0xEE, 1, (uint32_t)(disk->sectors - 1));

    uint8_t *entries =
        disk->bytes + 2u * JOSH_BLOCK_SECTOR_SIZE;
    static const uint8_t basic_data_guid[16] = {
        0xA2, 0xA0, 0xD0, 0xEB,
        0xE5, 0xB9, 0x33, 0x44,
        0x87, 0xC0, 0x68, 0xB6,
        0xB7, 0x26, 0x99, 0xC7
    };

    memcpy(entries, basic_data_guid, 16);
    for (unsigned i = 0; i < 16; ++i) {
        entries[16 + i] = (uint8_t)(i + 1u);
    }
    put64(entries + 32, 64);
    put64(entries + 40, 127);

    uint8_t *entry1 = entries + 128;
    memcpy(entry1, basic_data_guid, 16);
    for (unsigned i = 0; i < 16; ++i) {
        entry1[16 + i] = (uint8_t)(0x80u + i);
    }
    put64(entry1 + 32, 128);
    put64(entry1 + 40, 255);

    uint32_t entries_crc = crc32_update(0, entries, 4u * 128u);

    uint8_t *header = disk->bytes + JOSH_BLOCK_SECTOR_SIZE;
    memcpy(header, "EFI PART", 8);
    put32(header + 8, 0x00010000u);
    put32(header + 12, 92);
    put64(header + 24, 1);
    put64(header + 32, disk->sectors - 1);
    put64(header + 40, 34);
    put64(header + 48, disk->sectors - 34);
    for (unsigned i = 0; i < 16; ++i) {
        header[56 + i] = (uint8_t)(0x40u + i);
    }
    put64(header + 72, 2);
    put32(header + 80, 4);
    put32(header + 84, 128);
    put32(header + 88, entries_crc);
    put32(header + 16, crc32_update(0, header, 92));
}

static void test_gpt(void) {
    memory_disk_t disk = make_disk(4096);
    build_gpt(&disk);

    josh_block_device_t device = {
        .context = &disk,
        .sector_count = disk.sectors,
        .read = memory_read,
        .write = memory_write
    };

    josh_partition_summary_t summary;
    collected_partitions_t collected = {0};
    assert(josh_partition_scan(
        &device, &summary, collect_partition, &collected) ==
        JOSH_PARTITION_OK);
    assert(summary.scheme == JOSH_PARTITION_SCHEME_GPT);
    assert(summary.partition_count == 2);
    assert(collected.count == 2);
    assert(collected.items[0].first_lba == 64);
    assert(collected.items[0].sector_count == 64);
    assert(collected.items[1].first_lba == 128);
    assert(collected.items[1].sector_count == 128);

    disk.bytes[JOSH_BLOCK_SECTOR_SIZE + 40] ^= 1u;
    assert(josh_partition_scan(&device, &summary, 0, 0) ==
           JOSH_PARTITION_CORRUPT);

    free(disk.bytes);
}

static void test_missing_and_out_of_range(void) {
    memory_disk_t disk = make_disk(128);
    josh_block_device_t device = {
        .context = &disk,
        .sector_count = disk.sectors,
        .read = memory_read,
        .write = memory_write
    };
    josh_partition_summary_t summary;

    assert(josh_partition_scan(&device, &summary, 0, 0) ==
           JOSH_PARTITION_NO_TABLE);

    disk.bytes[510] = 0x55;
    disk.bytes[511] = 0xAA;
    write_mbr_entry(disk.bytes, 0, 0x00, 0x83, 100, 40);
    assert(josh_partition_scan(&device, &summary, 0, 0) ==
           JOSH_PARTITION_CORRUPT);

    free(disk.bytes);
}

int main(void) {
    test_block_bounds_and_write();
    test_mbr();
    test_gpt();
    test_missing_and_out_of_range();
    puts("Block and partition tests passed");
    return 0;
}
