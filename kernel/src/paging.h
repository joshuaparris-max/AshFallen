#ifndef JOSHOS_PAGING_H
#define JOSHOS_PAGING_H

#include "boot.h"
#include <stdint.h>

typedef enum {
    PAGING_OK = 0,
    PAGING_BAD_ARGUMENT,
    PAGING_NO_MEMORY,
    PAGING_UNSUPPORTED_LAYOUT,
    PAGING_MAPPING_CONFLICT
} paging_status_t;

paging_status_t paging_init(const boot_context_t *boot, uint64_t *root_phys_out);
__attribute__((noreturn))
void paging_activate(uint64_t root_phys, void (*continuation)(void));
const char *paging_status_string(paging_status_t status);

#endif
