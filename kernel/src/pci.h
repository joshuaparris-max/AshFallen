#ifndef JOSHOS_PCI_H
#define JOSHOS_PCI_H

#include <stdint.h>

#define PCI_MAX_DEVICES 64u
#define PCI_CLASS_MASS_STORAGE 0x01u
#define PCI_SUBCLASS_SATA 0x06u
#define PCI_PROGIF_AHCI 0x01u
#define PCI_SUBCLASS_NVM 0x08u
#define PCI_PROGIF_NVME 0x02u

typedef struct {
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t revision;
    uint8_t header_type;
    uint32_t bars[6];
} pci_device_t;

typedef struct {
    uint64_t base;
    int io;
    int is_64;
    int prefetchable;
} pci_bar_t;

void pci_scan(void);
uint32_t pci_count(void);
const pci_device_t *pci_get(uint32_t index);
const pci_device_t *pci_find_class(uint8_t class_code, uint8_t subclass, uint8_t prog_if);
uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset);
void pci_config_write32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset, uint32_t value);
int pci_decode_bar(const pci_device_t *device, uint32_t index, pci_bar_t *bar_out);

#endif
