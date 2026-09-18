#include "apic.h"

int apic_build_redirection(
    uint8_t vector,
    uint32_t destination_apic_id,
    uint16_t acpi_flags,
    int masked,
    uint64_t *entry_out
) {
    if (!entry_out || vector < 32 || vector == 0xff ||
        destination_apic_id > 0xffu) {
        return 0;
    }

    uint16_t polarity = acpi_flags & 0x3u;
    uint16_t trigger = (acpi_flags >> 2) & 0x3u;
    if (polarity == 2u || trigger == 2u) return 0;

    int active_low = polarity == 3u;
    int level = trigger == 3u;

    uint64_t entry = vector;
    if (active_low) entry |= UINT64_C(1) << 13;
    if (level) entry |= UINT64_C(1) << 15;
    if (masked) entry |= UINT64_C(1) << 16;
    entry |= (uint64_t)destination_apic_id << 56;
    *entry_out = entry;
    return 1;
}
