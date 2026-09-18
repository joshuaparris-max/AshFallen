#include "acpi.h"
#include "paging.h"
#include <stdint.h>

#define PAGE_SIZE UINT64_C(4096)

static int kernel_acpi_read(
    uint64_t physical,
    void *destination,
    size_t length,
    void *context
) {
    (void)context;
    unsigned char *out = (unsigned char *)destination;

    while (length != 0) {
        uint64_t page_offset = physical & (PAGE_SIZE - 1u);
        size_t chunk = (size_t)(PAGE_SIZE - page_offset);
        if (chunk > length) chunk = length;

        const unsigned char *source =
            (const unsigned char *)paging_temp_map(physical);
        if (!source) return 0;
        for (size_t i = 0; i < chunk; ++i) out[i] = source[i];
        paging_temp_unmap();

        if (UINT64_MAX - physical < chunk) return 0;
        physical += chunk;
        out += chunk;
        length -= chunk;
    }
    return 1;
}

acpi_status_t acpi_kernel_discover(
    const boot_context_t *boot,
    acpi_platform_info_t *info
) {
    if (!boot || !info || boot->rsdp_phys == 0) return ACPI_BAD_ARGUMENT;
    return acpi_discover(
        boot->rsdp_phys, kernel_acpi_read, 0, info);
}
