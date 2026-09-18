#ifndef JOSHOS_PAGING_H
#define JOSHOS_PAGING_H

#include "boot.h"
#include <stdint.h>

typedef enum {
    PAGING_OK = 0,
    PAGING_BAD_ARGUMENT,
    PAGING_NO_MEMORY,
    PAGING_UNSUPPORTED_LAYOUT,
    PAGING_MAP_CONFLICT,
    PAGING_SELF_TEST_FAILED
} paging_status_t;

paging_status_t paging_init(boot_context_t *boot);
uint64_t paging_root_phys(void);
void *paging_phys_to_virt(uint64_t physical_address);
const char *paging_status_string(paging_status_t status);

#endif
