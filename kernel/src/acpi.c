#include "acpi.h"
#include <stdint.h>

#define ACPI_SDT_HEADER_SIZE 36u
#define ACPI_MADT_HEADER_SIZE 44u
#define ACPI_MAX_TABLE_BYTES (1024u * 1024u)

static uint16_t read_le16(const unsigned char *p) {
    return (uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8);
}

static uint32_t read_le32(const unsigned char *p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint64_t read_le64(const unsigned char *p) {
    return (uint64_t)read_le32(p) |
           ((uint64_t)read_le32(p + 4) << 32);
}

static int bytes_equal(
    const unsigned char *left,
    const char *right,
    size_t length
) {
    for (size_t i = 0; i < length; ++i) {
        if (left[i] != (unsigned char)right[i]) return 0;
    }
    return 1;
}

static int read_exact(
    acpi_reader_fn reader,
    void *context,
    uint64_t physical,
    void *destination,
    size_t length
) {
    if (!reader || !destination) return 0;
    if (length != 0 && UINT64_MAX - physical < length - 1u) return 0;
    return reader(physical, destination, length, context);
}

static acpi_status_t checksum_region(
    acpi_reader_fn reader,
    void *context,
    uint64_t physical,
    uint32_t length
) {
    unsigned char buffer[256];
    uint32_t offset = 0;
    unsigned char sum = 0;

    while (offset < length) {
        uint32_t remaining = length - offset;
        uint32_t chunk = remaining < sizeof(buffer)
            ? remaining : (uint32_t)sizeof(buffer);
        if (!read_exact(
                reader, context, physical + offset, buffer, chunk)) {
            return ACPI_READ_ERROR;
        }
        for (uint32_t i = 0; i < chunk; ++i) {
            sum = (unsigned char)(sum + buffer[i]);
        }
        offset += chunk;
    }
    return sum == 0 ? ACPI_OK : ACPI_BAD_CHECKSUM;
}

static acpi_status_t read_sdt_header(
    acpi_reader_fn reader,
    void *context,
    uint64_t physical,
    unsigned char header[ACPI_SDT_HEADER_SIZE]
) {
    if (!read_exact(
            reader, context, physical, header, ACPI_SDT_HEADER_SIZE)) {
        return ACPI_READ_ERROR;
    }
    uint32_t length = read_le32(header + 4);
    if (length < ACPI_SDT_HEADER_SIZE || length > ACPI_MAX_TABLE_BYTES) {
        return ACPI_BAD_LENGTH;
    }
    return checksum_region(reader, context, physical, length);
}

static acpi_status_t append_cpu(
    acpi_platform_info_t *info,
    uint32_t processor_id,
    uint32_t apic_id,
    uint32_t flags,
    uint8_t x2apic
) {
    if ((flags & 3u) == 0) return ACPI_OK;
    if (info->cpu_count >= ACPI_MAX_CPUS) return ACPI_TOO_MANY_ENTRIES;

    acpi_cpu_info_t *cpu = &info->cpus[info->cpu_count++];
    cpu->acpi_processor_id = processor_id;
    cpu->apic_id = apic_id;
    cpu->flags = flags;
    cpu->x2apic = x2apic;
    return ACPI_OK;
}

static acpi_status_t parse_madt_entry(
    uint8_t type,
    const unsigned char *entry,
    uint8_t entry_length,
    acpi_platform_info_t *info
) {
    if (type == 0 && entry_length >= 8) {
        return append_cpu(
            info, entry[2], entry[3], read_le32(entry + 4), 0);
    }
    if (type == 1 && entry_length >= 12) {
        if (info->ioapic_count >= ACPI_MAX_IOAPICS) return ACPI_TOO_MANY_ENTRIES;
        acpi_ioapic_info_t *io = &info->ioapics[info->ioapic_count++];
        io->id = entry[2];
        io->physical_address = read_le32(entry + 4);
        io->gsi_base = read_le32(entry + 8);
        return ACPI_OK;
    }
    if (type == 2 && entry_length >= 10) {
        if (entry[2] != 0) return ACPI_OK;
        if (info->override_count >= ACPI_MAX_ISO) return ACPI_TOO_MANY_ENTRIES;
        acpi_irq_override_t *override = &info->overrides[info->override_count++];
        override->source_irq = entry[3];
        override->gsi = read_le32(entry + 4);
        override->flags = read_le16(entry + 8);
        return ACPI_OK;
    }
    if (type == 5 && entry_length >= 12) {
        info->local_apic_address = read_le64(entry + 4);
        return ACPI_OK;
    }
    if (type == 9 && entry_length >= 16) {
        return append_cpu(
            info, read_le32(entry + 12), read_le32(entry + 4),
            read_le32(entry + 8), 1);
    }
    return ACPI_OK;
}

static acpi_status_t parse_madt(
    uint64_t physical,
    acpi_reader_fn reader,
    void *context,
    acpi_platform_info_t *info
) {
    unsigned char header[ACPI_MADT_HEADER_SIZE];
    if (!read_exact(reader, context, physical, header, sizeof(header))) {
        return ACPI_READ_ERROR;
    }
    if (!bytes_equal(header, "APIC", 4)) return ACPI_BAD_SIGNATURE;

    uint32_t length = read_le32(header + 4);
    if (length < ACPI_MADT_HEADER_SIZE || length > ACPI_MAX_TABLE_BYTES) {
        return ACPI_BAD_LENGTH;
    }

    acpi_status_t status = checksum_region(reader, context, physical, length);
    if (status != ACPI_OK) return status;

    info->local_apic_address = read_le32(header + 36);
    info->madt_flags = read_le32(header + 40);

    uint32_t offset = ACPI_MADT_HEADER_SIZE;
    while (offset < length) {
        unsigned char entry_header[2];
        if (!read_exact(
                reader, context, physical + offset, entry_header, 2)) {
            return ACPI_READ_ERROR;
        }

        uint8_t type = entry_header[0];
        uint8_t entry_length = entry_header[1];
        if (entry_length < 2 || entry_length > length - offset) {
            return ACPI_BAD_LENGTH;
        }

        unsigned char entry[32];
        if (entry_length > sizeof(entry)) {
            offset += entry_length;
            continue;
        }
        if (!read_exact(
                reader, context, physical + offset, entry, entry_length)) {
            return ACPI_READ_ERROR;
        }

        status = parse_madt_entry(type, entry, entry_length, info);
        if (status != ACPI_OK) return status;

        offset += entry_length;
    }

    return ACPI_OK;
}

static acpi_status_t find_madt_in_root(
    uint64_t root_physical,
    uint32_t entry_size,
    const char *expected_signature,
    acpi_reader_fn reader,
    void *context,
    acpi_platform_info_t *info
) {
    unsigned char header[ACPI_SDT_HEADER_SIZE];
    acpi_status_t status =
        read_sdt_header(reader, context, root_physical, header);
    if (status != ACPI_OK) return status;
    if (!bytes_equal(header, expected_signature, 4)) {
        return ACPI_BAD_SIGNATURE;
    }

    uint32_t length = read_le32(header + 4);
    uint32_t payload = length - ACPI_SDT_HEADER_SIZE;
    if (entry_size == 0 || (payload % entry_size) != 0) {
        return ACPI_BAD_LENGTH;
    }

    uint32_t entries = payload / entry_size;
    for (uint32_t i = 0; i < entries; ++i) {
        unsigned char address_bytes[8] = {0};
        uint64_t address_position =
            root_physical + ACPI_SDT_HEADER_SIZE +
            (uint64_t)i * entry_size;
        if (!read_exact(
                reader, context, address_position,
                address_bytes, entry_size)) {
            return ACPI_READ_ERROR;
        }

        uint64_t table_physical = entry_size == 8
            ? read_le64(address_bytes)
            : read_le32(address_bytes);
        if (table_physical == 0) continue;

        unsigned char table_header[ACPI_SDT_HEADER_SIZE];
        if (!read_exact(
                reader, context, table_physical,
                table_header, sizeof(table_header))) {
            return ACPI_READ_ERROR;
        }
        if (!bytes_equal(table_header, "APIC", 4)) continue;

        return parse_madt(
            table_physical, reader, context, info);
    }

    return ACPI_NOT_FOUND;
}

acpi_status_t acpi_discover(
    uint64_t rsdp_physical,
    acpi_reader_fn reader,
    void *reader_context,
    acpi_platform_info_t *info
) {
    if (!rsdp_physical || !reader || !info) return ACPI_BAD_ARGUMENT;

    for (size_t i = 0; i < sizeof(*info); ++i) {
        ((unsigned char *)info)[i] = 0;
    }

    unsigned char rsdp[36];
    if (!read_exact(
            reader, reader_context, rsdp_physical, rsdp, 20)) {
        return ACPI_READ_ERROR;
    }
    if (!bytes_equal(rsdp, "RSD PTR ", 8)) return ACPI_BAD_SIGNATURE;

    acpi_status_t status =
        checksum_region(reader, reader_context, rsdp_physical, 20);
    if (status != ACPI_OK) return status;

    uint8_t revision = rsdp[15];
    if (revision >= 2) {
        if (!read_exact(
                reader, reader_context, rsdp_physical, rsdp, sizeof(rsdp))) {
            return ACPI_READ_ERROR;
        }
        uint32_t length = read_le32(rsdp + 20);
        if (length < sizeof(rsdp) || length > 4096u) return ACPI_BAD_LENGTH;
        status = checksum_region(
            reader, reader_context, rsdp_physical, length);
        if (status != ACPI_OK) return status;

        uint64_t xsdt = read_le64(rsdp + 24);
        if (xsdt != 0) {
            return find_madt_in_root(
                xsdt, 8, "XSDT", reader, reader_context, info);
        }
    }

    uint32_t rsdt = read_le32(rsdp + 16);
    if (rsdt == 0) return ACPI_NOT_FOUND;
    return find_madt_in_root(
        rsdt, 4, "RSDT", reader, reader_context, info);
}

int acpi_resolve_isa_irq(
    const acpi_platform_info_t *info,
    uint8_t irq,
    uint32_t *gsi_out,
    uint16_t *flags_out
) {
    if (!info || !gsi_out || !flags_out) return 0;

    *gsi_out = irq;
    *flags_out = 0;
    for (uint32_t i = 0; i < info->override_count; ++i) {
        if (info->overrides[i].source_irq != irq) continue;
        *gsi_out = info->overrides[i].gsi;
        *flags_out = info->overrides[i].flags;
        return 1;
    }
    return 1;
}

const char *acpi_status_string(acpi_status_t status) {
    switch (status) {
        case ACPI_OK: return "ok";
        case ACPI_BAD_ARGUMENT: return "bad argument";
        case ACPI_READ_ERROR: return "physical read failed";
        case ACPI_BAD_SIGNATURE: return "bad ACPI signature";
        case ACPI_BAD_CHECKSUM: return "bad ACPI checksum";
        case ACPI_BAD_LENGTH: return "bad ACPI table length";
        case ACPI_NOT_FOUND: return "required ACPI table not found";
        case ACPI_TOO_MANY_ENTRIES: return "ACPI topology exceeds kernel limit";
        default: return "unknown ACPI error";
    }
}
