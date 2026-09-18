#ifndef JOSHOS_PCI_H
#define JOSHOS_PCI_H

#include <stdint.h>

typedef struct {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t header_type;
} pci_device_t;

typedef enum {
    PCI_STORAGE_NONE = 0,
    PCI_STORAGE_IDE,
    PCI_STORAGE_AHCI,
    PCI_STORAGE_NVME,
    PCI_STORAGE_RAID,
    PCI_STORAGE_OTHER
} pci_storage_kind_t;

typedef uint32_t (*pci_config_read32_fn)(
    void *context,
    uint8_t bus,
    uint8_t device,
    uint8_t function,
    uint8_t offset
);

typedef struct {
    void *context;
    pci_config_read32_fn read32;
} pci_config_access_t;

typedef struct {
    uint32_t device_count;
    uint32_t mass_storage_count;
    uint32_t ahci_count;
    uint32_t nvme_count;
} pci_scan_summary_t;

typedef void (*pci_device_visitor_fn)(const pci_device_t *device, void *context);

pci_storage_kind_t pci_classify_storage(const pci_device_t *device);

int pci_scan(
    const pci_config_access_t *access,
    pci_scan_summary_t *summary,
    pci_device_visitor_fn visitor,
    void *visitor_context
);

int pci_init(pci_scan_summary_t *summary);

#endif
