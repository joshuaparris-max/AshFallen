#ifndef JOSHOS_AHCI_H
#define JOSHOS_AHCI_H

#include "boot.h"
#include "block.h"
#include <stdint.h>

#define AHCI_MAX_DISKS 4u

typedef enum {
    AHCI_OK = 0,
    AHCI_BAD_ARGUMENT,
    AHCI_NO_CONTROLLER,
    AHCI_NO_DISK,
    AHCI_NO_MEMORY,
    AHCI_MMIO_ERROR,
    AHCI_TIMEOUT,
    AHCI_IO_ERROR
} ahci_status_t;

ahci_status_t ahci_probe(const boot_context_t *boot);
uint32_t ahci_disk_count(void);
const block_device_t *ahci_disk(uint32_t index);
const char *ahci_status_string(ahci_status_t status);

#endif
