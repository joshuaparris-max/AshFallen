#ifndef JOSHOS_AHCI_H
#define JOSHOS_AHCI_H

#include <stdint.h>
#include "block.h"
#include "boot.h"
#include "pci.h"

typedef enum {
    AHCI_OK = 0,
    AHCI_NO_CONTROLLER,
    AHCI_BAD_BAR,
    AHCI_MMIO_UNAVAILABLE,
    AHCI_NO_SATA_DEVICE,
    AHCI_NO_MEMORY,
    AHCI_DMA_ADDRESS_ERROR,
    AHCI_ENGINE_TIMEOUT,
    AHCI_DEVICE_TIMEOUT,
    AHCI_IO_ERROR,
    AHCI_UNSUPPORTED_DEVICE
} ahci_status_t;

typedef struct {
    volatile uint8_t *abar;
    const boot_context_t *boot;
    uint64_t sector_count;
    uint64_t command_list_phys;
    uint64_t received_fis_phys;
    uint64_t command_table_phys;
    uint64_t transfer_buffer_phys;
    uint8_t port_index;
} ahci_device_t;

ahci_status_t ahci_init(
    const pci_device_t *controller,
    const boot_context_t *boot,
    ahci_device_t *device
);

ahci_status_t ahci_read_sector(
    ahci_device_t *device,
    uint64_t lba,
    void *buffer
);

ahci_status_t ahci_write_sector(
    ahci_device_t *device,
    uint64_t lba,
    const void *buffer
);

int ahci_make_block_device(
    ahci_device_t *ahci,
    josh_block_device_t *block
);

const char *ahci_status_string(ahci_status_t status);

#endif
