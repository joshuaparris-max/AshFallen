#ifndef JOSHOS_PCI_H
#define JOSHOS_PCI_H

#include <stdint.h>

#define PCI_MAX_DEVICES 256u

typedef enum {
    PCI_OK = 0,
    PCI_BAD_ARGUMENT,
    PCI_NOT_FOUND,
    PCI_TABLE_FULL,
    PCI_UNSUPPORTED_BAR
} pci_status_t;

typedef struct {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t revision;
    uint8_t header_type;
    uint8_t interrupt_line;
    uint8_t interrupt_pin;
    uint32_t bars[6];
} pci_device_t;

typedef struct {
    uint64_t base;
    uint8_t is_io;
    uint8_t is_64bit;
    uint8_t prefetchable;
} pci_bar_t;

uint32_t pci_config_address(uint8_t bus, uint8_t device,
                            uint8_t function, uint8_t offset);
uint32_t pci_config_read32(uint8_t bus, uint8_t device,
                           uint8_t function, uint8_t offset);
uint16_t pci_config_read16(uint8_t bus, uint8_t device,
                           uint8_t function, uint8_t offset);
uint8_t pci_config_read8(uint8_t bus, uint8_t device,
                         uint8_t function, uint8_t offset);
void pci_config_write16(uint8_t bus, uint8_t device,
                        uint8_t function, uint8_t offset, uint16_t value);

void pci_init(void);
uint32_t pci_device_count(void);
const pci_device_t *pci_device_at(uint32_t index);
const pci_device_t *pci_find_device(uint16_t vendor_id, uint16_t device_id);
const pci_device_t *pci_find_class(uint8_t class_code, uint8_t subclass,
                                   uint32_t start_index);
pci_status_t pci_decode_bar(const pci_device_t *device, uint8_t index,
                            pci_bar_t *bar);
pci_status_t pci_enable_memory_busmaster(const pci_device_t *device);
const char *pci_status_string(pci_status_t status);

#endif
