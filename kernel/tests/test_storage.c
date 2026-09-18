#include "block.h"
#include "partition.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SECTOR_SIZE 512u
#define SECTORS 128u

static uint8_t disk[SECTOR_SIZE * SECTORS];
static int failures;
static int flushes;

static void expect(const char *name, int condition) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", name);
        failures++;
    }
}

static block_status_t mem_read(void *context, uint64_t lba, uint32_t count, void *buffer) {
    uint8_t *bytes = context;
    memcpy(buffer, bytes + lba * SECTOR_SIZE, (size_t)count * SECTOR_SIZE);
    return BLOCK_OK;
}

static block_status_t mem_write(void *context, uint64_t lba, uint32_t count, const void *buffer) {
    uint8_t *bytes = context;
    memcpy(bytes + lba * SECTOR_SIZE, buffer, (size_t)count * SECTOR_SIZE);
    return BLOCK_OK;
}

static block_status_t mem_flush(void *context) {
    (void)context;
    flushes++;
    return BLOCK_OK;
}

static void put16(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void put32(uint8_t *p, uint32_t value) {
    for (uint32_t i = 0; i < 4; ++i) p[i] = (uint8_t)(value >> (i * 8));
}

static void put64(uint8_t *p, uint64_t value) {
    put32(p, (uint32_t)value);
    put32(p + 4, (uint32_t)(value >> 32));
}

static uint32_t crc32_calc(const uint8_t *data, size_t length) {
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (uint32_t bit = 0; bit < 8; ++bit) {
            uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
            crc = (crc >> 1) ^ (UINT32_C(0xedb88320) & mask);
        }
    }
    return ~crc;
}

static block_device_t make_device(int writable) {
    block_device_t device = {
        .name = "mem0",
        .sector_size = SECTOR_SIZE,
        .sector_count = SECTORS,
        .writable = writable,
        .context = disk,
        .read = mem_read,
        .write = writable ? mem_write : 0,
        .flush = mem_flush
    };
    return device;
}

static void make_mbr(void) {
    memset(disk, 0, sizeof(disk));
    uint8_t *mbr = disk;
    uint8_t *entry = mbr + 446;
    entry[0] = 0x80;
    entry[4] = 0x0c;
    put32(entry + 8, 4);
    put32(entry + 12, 32);
    put16(mbr + 510, 0xaa55);
}

static void make_gpt(void) {
    memset(disk, 0, sizeof(disk));
    uint8_t *mbr = disk;
    uint8_t *protective = mbr + 446;
    protective[4] = 0xee;
    put32(protective + 8, 1);
    put32(protective + 12, SECTORS - 1);
    put16(mbr + 510, 0xaa55);

    uint8_t *entries = disk + 2 * SECTOR_SIZE;
    for (uint32_t i = 0; i < 16; ++i) {
        entries[i] = (uint8_t)(i + 1);
        entries[16 + i] = (uint8_t)(0xa0 + i);
    }
    put64(entries + 32, 40);
    put64(entries + 40, 79);

    uint8_t *header = disk + SECTOR_SIZE;
    memcpy(header, "EFI PART", 8);
    put32(header + 8, 0x00010000);
    put32(header + 12, 92);
    put64(header + 24, 1);
    put64(header + 32, SECTORS - 1);
    put64(header + 40, 34);
    put64(header + 48, SECTORS - 34);
    put64(header + 72, 2);
    put32(header + 80, 4);
    put32(header + 84, 128);
    put32(header + 88, crc32_calc(entries, 4u * 128u));
    put32(header + 16, 0);
    put32(header + 16, crc32_calc(header, 92));
}

int main(void) {
    block_registry_reset();
    block_device_t device = make_device(1);
    uint32_t index = UINT32_MAX;
    expect("block register", block_register(&device, &index) == BLOCK_OK && index == 0);
    expect("block registry count", block_count() == 1 && block_get(0) != 0);

    uint8_t sector[SECTOR_SIZE];
    memset(sector, 0x5a, sizeof(sector));
    expect("block write", block_write(block_get(0), 3, 1, sector) == BLOCK_OK);
    memset(sector, 0, sizeof(sector));
    expect("block read", block_read(block_get(0), 3, 1, sector) == BLOCK_OK && sector[0] == 0x5a);
    expect("block bounds", block_read(block_get(0), SECTORS, 1, sector) == BLOCK_OUT_OF_RANGE);
    expect("block flush", block_flush(block_get(0)) == BLOCK_OK && flushes == 1);

    block_device_t ro = make_device(0);
    expect("read-only write rejected", block_write(&ro, 0, 1, sector) == BLOCK_READ_ONLY);

    make_mbr();
    partition_table_t table;
    expect("MBR parse", partition_scan(&device, &table) == PARTITION_OK);
    expect("MBR scheme", table.scheme == PARTITION_SCHEME_MBR);
    expect("MBR partition", table.count == 1 && table.entries[0].first_lba == 4 &&
           table.entries[0].sector_count == 32 && table.entries[0].mbr_type == 0x0c &&
           table.entries[0].bootable == 1);

    make_gpt();
    expect("GPT parse", partition_scan(&device, &table) == PARTITION_OK);
    expect("GPT scheme", table.scheme == PARTITION_SCHEME_GPT);
    expect("GPT partition", table.count == 1 && table.entries[0].first_lba == 40 &&
           table.entries[0].sector_count == 40);

    disk[SECTOR_SIZE + 20] ^= 1;
    expect("GPT header CRC rejected", partition_scan(&device, &table) == PARTITION_BAD_GPT);

    make_gpt();
    disk[2 * SECTOR_SIZE + 64] ^= 1;
    expect("GPT entries CRC rejected", partition_scan(&device, &table) == PARTITION_BAD_GPT);

    if (failures) return 1;
    puts("Block and partition tests passed");
    return 0;
}
