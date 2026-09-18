#include "partition.h"

#include <stddef.h>
#include <stdint.h>

#define MBR_SIGNATURE_OFFSET 510u
#define MBR_ENTRY_OFFSET     446u
#define MBR_ENTRY_SIZE       16u
#define MBR_ENTRY_COUNT      4u
#define MBR_PROTECTIVE_GPT   0xEEu
#define GPT_HEADER_LBA       1u
#define GPT_HEADER_MIN_SIZE  92u
#define GPT_MAX_ENTRIES      4096u
#define GPT_MIN_ENTRY_SIZE   128u

static const uint8_t gpt_signature[8] = {
    'E', 'F', 'I', ' ', 'P', 'A', 'R', 'T'
};

static uint32_t read_le32(const uint8_t *p) {
    return (uint32_t)p[0]
        | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16)
        | ((uint32_t)p[3] << 24);
}

static uint64_t read_le64(const uint8_t *p) {
    return (uint64_t)read_le32(p)
        | ((uint64_t)read_le32(p + 4) << 32);
}

static void bytes_copy(uint8_t *dst, const uint8_t *src, size_t length) {
    while (length--) *dst++ = *src++;
}

static int bytes_equal(const uint8_t *left, const uint8_t *right, size_t length) {
    while (length--) {
        if (*left++ != *right++) return 0;
    }
    return 1;
}

static int bytes_zero(const uint8_t *data, size_t length) {
    while (length--) {
        if (*data++ != 0) return 0;
    }
    return 1;
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t length) {
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

static int range_valid(
    const josh_block_device_t *device,
    uint64_t first_lba,
    uint64_t sector_count
) {
    if (!device || sector_count == 0 || first_lba >= device->sector_count) {
        return 0;
    }
    return sector_count <= device->sector_count - first_lba;
}

static void emit_partition(
    josh_partition_summary_t *summary,
    const josh_partition_t *partition,
    josh_partition_visitor_fn visitor,
    void *visitor_context
) {
    ++summary->partition_count;
    if (visitor) visitor(partition, visitor_context);
}

static josh_partition_status_t scan_gpt(
    const josh_block_device_t *device,
    josh_partition_summary_t *summary,
    josh_partition_visitor_fn visitor,
    void *visitor_context
) {
    uint8_t header[JOSH_BLOCK_SECTOR_SIZE];
    if (josh_block_read(device, GPT_HEADER_LBA, 1, header) != 0) {
        return JOSH_PARTITION_IO_ERROR;
    }

    if (!bytes_equal(header, gpt_signature, sizeof(gpt_signature))) {
        return JOSH_PARTITION_CORRUPT;
    }

    uint32_t header_size = read_le32(header + 12);
    uint32_t expected_header_crc = read_le32(header + 16);
    uint64_t current_lba = read_le64(header + 24);
    uint64_t first_usable = read_le64(header + 40);
    uint64_t last_usable = read_le64(header + 48);
    uint64_t entries_lba = read_le64(header + 72);
    uint32_t entry_count = read_le32(header + 80);
    uint32_t entry_size = read_le32(header + 84);
    uint32_t expected_entries_crc = read_le32(header + 88);

    if (header_size < GPT_HEADER_MIN_SIZE ||
        header_size > JOSH_BLOCK_SECTOR_SIZE ||
        current_lba != GPT_HEADER_LBA ||
        first_usable > last_usable ||
        last_usable >= device->sector_count) {
        return JOSH_PARTITION_CORRUPT;
    }

    if (entry_count == 0 || entry_count > GPT_MAX_ENTRIES ||
        entry_size < GPT_MIN_ENTRY_SIZE ||
        entry_size > JOSH_BLOCK_SECTOR_SIZE ||
        (JOSH_BLOCK_SECTOR_SIZE % entry_size) != 0u) {
        return JOSH_PARTITION_UNSUPPORTED;
    }

    uint8_t header_copy[JOSH_BLOCK_SECTOR_SIZE];
    bytes_copy(header_copy, header, header_size);
    header_copy[16] = 0;
    header_copy[17] = 0;
    header_copy[18] = 0;
    header_copy[19] = 0;
    if (crc32_update(0, header_copy, header_size) != expected_header_crc) {
        return JOSH_PARTITION_CORRUPT;
    }

    uint64_t entry_bytes = (uint64_t)entry_count * entry_size;
    uint64_t entry_sectors =
        (entry_bytes + JOSH_BLOCK_SECTOR_SIZE - 1u) / JOSH_BLOCK_SECTOR_SIZE;
    if (!range_valid(device, entries_lba, entry_sectors)) {
        return JOSH_PARTITION_CORRUPT;
    }

    uint8_t sector[JOSH_BLOCK_SECTOR_SIZE];
    uint64_t bytes_remaining = entry_bytes;
    uint32_t entries_crc = 0;
    for (uint64_t i = 0; i < entry_sectors; ++i) {
        if (josh_block_read(device, entries_lba + i, 1, sector) != 0) {
            return JOSH_PARTITION_IO_ERROR;
        }

        size_t chunk = bytes_remaining > JOSH_BLOCK_SECTOR_SIZE
            ? JOSH_BLOCK_SECTOR_SIZE
            : (size_t)bytes_remaining;
        entries_crc = crc32_update(entries_crc, sector, chunk);
        bytes_remaining -= chunk;
    }

    if (entries_crc != expected_entries_crc) {
        return JOSH_PARTITION_CORRUPT;
    }

    summary->scheme = JOSH_PARTITION_SCHEME_GPT;

    uint32_t entries_per_sector = JOSH_BLOCK_SECTOR_SIZE / entry_size;
    for (uint32_t index = 0; index < entry_count; ++index) {
        uint64_t sector_lba = entries_lba + (index / entries_per_sector);
        uint32_t offset = (index % entries_per_sector) * entry_size;

        if (josh_block_read(device, sector_lba, 1, sector) != 0) {
            return JOSH_PARTITION_IO_ERROR;
        }

        const uint8_t *entry = sector + offset;
        if (bytes_zero(entry, 16)) continue;

        uint64_t first = read_le64(entry + 32);
        uint64_t last = read_le64(entry + 40);
        if (first > last ||
            first < first_usable ||
            last > last_usable ||
            !range_valid(device, first, last - first + 1u)) {
            return JOSH_PARTITION_CORRUPT;
        }

        josh_partition_t partition = {0};
        partition.scheme = JOSH_PARTITION_SCHEME_GPT;
        partition.index = index;
        partition.first_lba = first;
        partition.sector_count = last - first + 1u;
        bytes_copy(partition.gpt_type_guid, entry, 16);
        bytes_copy(partition.gpt_unique_guid, entry + 16, 16);

        emit_partition(summary, &partition, visitor, visitor_context);
    }

    return summary->partition_count
        ? JOSH_PARTITION_OK
        : JOSH_PARTITION_NO_PARTITIONS;
}

josh_partition_status_t josh_partition_scan(
    const josh_block_device_t *device,
    josh_partition_summary_t *summary,
    josh_partition_visitor_fn visitor,
    void *visitor_context
) {
    if (!device || !summary || !device->read || device->sector_count < 2) {
        return JOSH_PARTITION_NO_TABLE;
    }

    summary->scheme = JOSH_PARTITION_SCHEME_NONE;
    summary->partition_count = 0;

    uint8_t mbr[JOSH_BLOCK_SECTOR_SIZE];
    if (josh_block_read(device, 0, 1, mbr) != 0) {
        return JOSH_PARTITION_IO_ERROR;
    }

    if (mbr[MBR_SIGNATURE_OFFSET] != 0x55 ||
        mbr[MBR_SIGNATURE_OFFSET + 1] != 0xAA) {
        return JOSH_PARTITION_NO_TABLE;
    }

    for (uint32_t index = 0; index < MBR_ENTRY_COUNT; ++index) {
        const uint8_t *entry =
            mbr + MBR_ENTRY_OFFSET + index * MBR_ENTRY_SIZE;
        if (entry[4] == MBR_PROTECTIVE_GPT) {
            return scan_gpt(device, summary, visitor, visitor_context);
        }
    }

    summary->scheme = JOSH_PARTITION_SCHEME_MBR;

    for (uint32_t index = 0; index < MBR_ENTRY_COUNT; ++index) {
        const uint8_t *entry =
            mbr + MBR_ENTRY_OFFSET + index * MBR_ENTRY_SIZE;
        uint8_t status = entry[0];
        uint8_t type = entry[4];
        uint32_t first = read_le32(entry + 8);
        uint32_t count = read_le32(entry + 12);

        if (status != 0x00 && status != 0x80) {
            return JOSH_PARTITION_CORRUPT;
        }

        if (type == 0 || count == 0) continue;

        if (!range_valid(device, first, count)) {
            return JOSH_PARTITION_CORRUPT;
        }

        josh_partition_t partition = {0};
        partition.scheme = JOSH_PARTITION_SCHEME_MBR;
        partition.index = index;
        partition.first_lba = first;
        partition.sector_count = count;
        partition.bootable = status == 0x80;
        partition.mbr_type = type;

        emit_partition(summary, &partition, visitor, visitor_context);
    }

    return summary->partition_count
        ? JOSH_PARTITION_OK
        : JOSH_PARTITION_NO_PARTITIONS;
}

const char *josh_partition_status_string(josh_partition_status_t status) {
    switch (status) {
        case JOSH_PARTITION_OK: return "ok";
        case JOSH_PARTITION_NO_TABLE: return "no partition table";
        case JOSH_PARTITION_NO_PARTITIONS: return "no partitions";
        case JOSH_PARTITION_IO_ERROR: return "block I/O error";
        case JOSH_PARTITION_CORRUPT: return "corrupt partition table";
        case JOSH_PARTITION_UNSUPPORTED: return "unsupported partition layout";
        default: return "unknown partition error";
    }
}
