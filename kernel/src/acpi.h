#ifndef JOSHOS_ACPI_H
#define JOSHOS_ACPI_H

#include "boot.h"
#include <stddef.h>
#include <stdint.h>

#define ACPI_MAX_CPUS 64u
#define ACPI_MAX_IOAPICS 8u
#define ACPI_MAX_ISO 16u

typedef enum {
    ACPI_OK = 0,
    ACPI_BAD_ARGUMENT,
    ACPI_READ_ERROR,
    ACPI_BAD_SIGNATURE,
    ACPI_BAD_CHECKSUM,
    ACPI_BAD_LENGTH,
    ACPI_NOT_FOUND,
    ACPI_TOO_MANY_ENTRIES
} acpi_status_t;

typedef int (*acpi_reader_fn)(
    uint64_t physical_address,
    void *destination,
    size_t length,
    void *context
);

typedef struct {
    uint32_t acpi_processor_id;
    uint32_t apic_id;
    uint32_t flags;
    uint8_t x2apic;
} acpi_cpu_info_t;

typedef struct {
    uint8_t id;
    uint64_t physical_address;
    uint32_t gsi_base;
} acpi_ioapic_info_t;

typedef struct {
    uint8_t source_irq;
    uint32_t gsi;
    uint16_t flags;
} acpi_irq_override_t;

typedef struct {
    uint64_t local_apic_address;
    uint32_t madt_flags;
    acpi_cpu_info_t cpus[ACPI_MAX_CPUS];
    uint32_t cpu_count;
    acpi_ioapic_info_t ioapics[ACPI_MAX_IOAPICS];
    uint32_t ioapic_count;
    acpi_irq_override_t overrides[ACPI_MAX_ISO];
    uint32_t override_count;
} acpi_platform_info_t;

acpi_status_t acpi_discover(
    uint64_t rsdp_physical,
    acpi_reader_fn reader,
    void *reader_context,
    acpi_platform_info_t *info
);

acpi_status_t acpi_kernel_discover(
    const boot_context_t *boot,
    acpi_platform_info_t *info
);

int acpi_resolve_isa_irq(
    const acpi_platform_info_t *info,
    uint8_t irq,
    uint32_t *gsi_out,
    uint16_t *flags_out
);

const char *acpi_status_string(acpi_status_t status);

#endif
