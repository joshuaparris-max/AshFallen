#ifndef JOSHOS_PAGING_H
#define JOSHOS_PAGING_H

#include "boot.h"
#include <stdint.h>

typedef enum {
    PAGING_OK = 0,
    PAGING_BAD_ARGUMENT,
    PAGING_NO_MEMORY,
    PAGING_UNSUPPORTED_LAYOUT,
    PAGING_MAPPING_CONFLICT,
    PAGING_WINDOW_EXHAUSTED
} paging_status_t;

typedef struct {
    uint64_t root_phys;
} paging_address_space_t;

typedef struct {
    uint64_t physical_address;
    uint8_t present;
    uint8_t writable;
    uint8_t executable;
    uint8_t huge;
    uint8_t cache_disabled;
} paging_mapping_t;

paging_status_t paging_init(const boot_context_t *boot, uint64_t *root_phys_out);
__attribute__((noreturn))
void paging_activate(uint64_t root_phys, void (*continuation)(void));

const paging_address_space_t *paging_kernel_address_space(void);
uint64_t paging_current_root(void);
int paging_query(uint64_t virtual_address, paging_mapping_t *mapping);
int paging_verify_kernel_layout(void);

void *paging_direct_pointer(uint64_t physical_address);
void *paging_map_mmio(uint64_t physical_address, uint64_t size);
void *paging_temp_map(uint64_t physical_address);
void paging_temp_unmap(void);

const char *paging_status_string(paging_status_t status);

#endif
