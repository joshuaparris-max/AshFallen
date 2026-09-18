#ifndef JOSHOS_APIC_H
#define JOSHOS_APIC_H

#include "acpi.h"
#include "cpu.h"
#include <stdint.h>

typedef enum {
    APIC_OK = 0,
    APIC_BAD_ARGUMENT,
    APIC_UNAVAILABLE,
    APIC_UNSUPPORTED_MODE,
    APIC_MAP_FAILED,
    APIC_BAD_IOAPIC,
    APIC_BAD_IRQ_FLAGS,
    APIC_ROUTE_NOT_FOUND,
    APIC_DELIVERY_TIMEOUT
} apic_status_t;

typedef struct {
    uint64_t local_apic_physical;
    uint32_t local_apic_id;
    uint32_t ioapic_count;
} apic_summary_t;

int apic_build_redirection(
    uint8_t vector,
    uint32_t destination_apic_id,
    uint16_t acpi_flags,
    int masked,
    uint64_t *entry_out
);

apic_status_t apic_init(
    const acpi_platform_info_t *platform,
    const cpu_features_t *cpu
);
apic_status_t apic_route_isa_irq(
    uint8_t irq,
    uint8_t vector,
    int masked
);
apic_status_t apic_set_isa_irq_mask(uint8_t irq, int masked);
void apic_eoi(void);
uint32_t apic_local_id(void);
apic_summary_t apic_summary(void);
apic_status_t apic_send_ipi(uint32_t destination_apic_id, uint8_t vector);
const char *apic_status_string(apic_status_t status);

#endif
