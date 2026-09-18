#include "partition.h"
#include <stddef.h>
#include <stdint.h>

#define PARTITION_MAX_SECTOR 4096u
#define GPT_ENTRY_MAX_SIZE 512u
#define GPT_ENTRY_MAX_COUNT 128u
#define GPT_TABLE_MAX_BYTES (GPT_ENTRY_MAX_SIZE * GPT_ENTRY_MAX_COUNT)

static uint8_t sector_buffer[PARTITION_MAX_SECTOR];
static uint8_t gpt_entries[GPT_TABLE_MAX_BYTES];

static uint16_t le16(const uint8_t *p) {
    return (uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8);
}

static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint64_t le64(const uint8_t *p) {
    return (uint64_t)le32(p) | ((uint64_t)le32(p + 4) << 32);
}

static int add_overflows_u64(uint64_t a, uint64_t b) {
    return UINT64_MAX - a < b;
}

static void zero_bytes(void *pointer, size_t length) {
    uint8_t *bytes = (uint8_t *)pointer;
    for (size_t i = 0; i < length; ++i) bytes[i] = 0;
}

static void copy16(uint8_t destination[16], const uint8_t *source) {
    for (uint32_t i = 0; i < 16; ++i) destination[i] = source[i];
}

static int guid_is_zero(const uint8_t guid[16]) {
    uint8_t combined = 0;
    for (uint32_t i = 0; i < 16; ++i) combined |= guid[i];
    return combined == 0;
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t length) {
    crc = ~crc;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (uint32_t bit = 0; bit < 8; ++bit) {
            uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
            crc = (crc >> 1) ^ (UINT32_C(0xedb88320) & mask);
        }
    }
    return ~crc;
}

static partition_status_t read_sector(const block_device_t *device, uint64_t lba, uint8_t *buffer) {
    return block_read(device, lba, 1, buffer) == BLOCK_OK
        ? PARTITION_OK : PARTITION_IO_ERROR;
}

static partition_status_t parse_gpt(const block_device_t *device, partition_table_t *table) {
    if (read_sector(device, 1, sector_buffer) != PARTITION_OK) return PARTITION_IO_ERROR;
    static const uint8_t signature[8] = {'E','F','I',' ','P','A','R','T'};
    for (uint32_t i = 0; i < 8; ++i) if (sector_buffer[i] != signature[i]) return PARTITION_BAD_GPT;

    uint32_t header_size = le32(sector_buffer + 12);
    uint32_t stored_crc = le32(sector_buffer + 16);
    if (header_size < 92u || header_size > device->sector_size || header_size > PARTITION_MAX_SECTOR) {
        return PARTITION_BAD_GPT;
    }

    uint8_t header_copy[PARTITION_MAX_SECTOR];
    for (uint32_t i = 0; i < header_size; ++i) header_copy[i] = sector_buffer[i];
    header_copy[16] = header_copy[17] = header_copy[18] = header_copy[19] = 0;
    if (crc32_update(0, header_copy, header_size) != stored_crc) return PARTITION_BAD_GPT;

    uint64_t current_lba = le64(sector_buffer + 24);
    uint64_t first_usable = le64(sector_buffer + 40);
    uint64_t last_usable = le64(sector_buffer + 48);
    uint64_t entries_lba = le64(sector_buffer + 72);
    uint32_t entry_count = le32(sector_buffer + 80);
    uint32_t entry_size = le32(sector_buffer + 84);
    uint32_t entries_crc = le32(sector_buffer + 88);

    if (current_lba != 1 || first_usable > last_usable ||
        entry_count == 0 || entry_count > GPT_ENTRY_MAX_COUNT ||
        entry_size < 128u || entry_size > GPT_ENTRY_MAX_SIZE || (entry_size & 7u) != 0) {
        return PARTITION_BAD_GPT;
    }

    uint64_t table_bytes64 = (uint64_t)entry_count * entry_size;
    if (table_bytes64 > GPT_TABLE_MAX_BYTES) return PARTITION_BAD_GPT;
    uint32_t table_bytes = (uint32_t)table_bytes64;
    uint32_t sectors = (table_bytes + device->sector_size - 1u) / device->sector_size;
    if (entries_lba >= device->sector_count || sectors == 0 ||
        (uint64_t)sectors > device->sector_count - entries_lba) return PARTITION_BAD_GPT;

    uint32_t copied = 0;
    for (uint32_t s = 0; s < sectors; ++s) {
        if (read_sector(device, entries_lba + s, sector_buffer) != PARTITION_OK) return PARTITION_IO_ERROR;
        uint32_t remaining = table_bytes - copied;
        uint32_t take = remaining < device->sector_size ? remaining : device->sector_size;
        for (uint32_t i = 0; i < take; ++i) gpt_entries[copied + i] = sector_buffer[i];
        copied += take;
    }
    if (crc32_update(0, gpt_entries, table_bytes) != entries_crc) return PARTITION_BAD_GPT;

    table->scheme = PARTITION_SCHEME_GPT;
    table->count = 0;
    for (uint32_t i = 0; i < entry_count; ++i) {
        const uint8_t *entry = gpt_entries + (uint64_t)i * entry_size;
        if (guid_is_zero(entry)) continue;
        uint64_t first = le64(entry + 32);
        uint64_t last = le64(entry + 40);
        if (first > last || first < first_usable || last > last_usable || last >= device->sector_count) {
            return PARTITION_BAD_GPT;
        }
        if (table->count >= PARTITION_MAX) return PARTITION_TOO_MANY;
        partition_t *out = &table->entries[table->count++];
        zero_bytes(out, sizeof(*out));
        out->first_lba = first;
        out->sector_count = last - first + 1u;
        copy16(out->type_guid, entry);
        copy16(out->unique_guid, entry + 16);
    }
    return PARTITION_OK;
}

partition_status_t partition_scan(const block_device_t *device, partition_table_t *table) {
    if (!device || !table || device->sector_size < 512u ||
        device->sector_size > PARTITION_MAX_SECTOR || device->sector_count < 2u) {
        return PARTITION_BAD_ARGUMENT;
    }
    zero_bytes(table, sizeof(*table));
    if (read_sector(device, 0, sector_buffer) != PARTITION_OK) return PARTITION_IO_ERROR;
    if (le16(sector_buffer + 510) != UINT16_C(0xaa55)) return PARTITION_BAD_SIGNATURE;

    int protective = 0;
    for (uint32_t i = 0; i < 4; ++i) {
        const uint8_t *entry = sector_buffer + 446u + i * 16u;
        uint8_t type = entry[4];
        if (type == 0) continue;
        if (type == 0xee) protective = 1;
    }
    if (protective) return parse_gpt(device, table);

    table->scheme = PARTITION_SCHEME_MBR;
    for (uint32_t i = 0; i < 4; ++i) {
        const uint8_t *entry = sector_buffer + 446u + i * 16u;
        uint8_t type = entry[4];
        uint32_t first = le32(entry + 8);
        uint32_t count = le32(entry + 12);
        if (type == 0 || count == 0) continue;
        if ((uint64_t)first >= device->sector_count ||
            (uint64_t)count > device->sector_count - first) return PARTITION_BAD_SIGNATURE;
        if (table->count >= PARTITION_MAX) return PARTITION_TOO_MANY;
        partition_t *out = &table->entries[table->count++];
        zero_bytes(out, sizeof(*out));
        out->first_lba = first;
        out->sector_count = count;
        out->mbr_type = type;
        out->bootable = entry[0] == 0x80u;
    }
    return PARTITION_OK;
}

const char *partition_status_string(partition_status_t status) {
    switch (status) {
        case PARTITION_OK: return "ok";
        case PARTITION_BAD_ARGUMENT: return "bad argument";
        case PARTITION_IO_ERROR: return "I/O error";
        case PARTITION_BAD_SIGNATURE: return "bad MBR signature";
        case PARTITION_BAD_GPT: return "invalid GPT";
        case PARTITION_TOO_MANY: return "too many partitions";
        default: return "unknown partition error";
    }
}
