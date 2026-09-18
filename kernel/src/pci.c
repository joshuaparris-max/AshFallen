#include "pci.h"
#include "io.h"
#include <stdint.h>

#define PCI_CONFIG_ADDRESS 0xcf8u
#define PCI_CONFIG_DATA 0xcfcu

static pci_device_t inventory[PCI_MAX_DEVICES];
static uint32_t inventory_count;

uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset) {
    uint32_t address = UINT32_C(0x80000000) |
        ((uint32_t)bus << 16) |
        ((uint32_t)(slot & 0x1fu) << 11) |
        ((uint32_t)(function & 0x07u) << 8) |
        (offset & 0xfcu);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

void pci_config_write32(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset, uint32_t value) {
    uint32_t address = UINT32_C(0x80000000) |
        ((uint32_t)bus << 16) |
        ((uint32_t)(slot & 0x1fu) << 11) |
        ((uint32_t)(function & 0x07u) << 8) |
        (offset & 0xfcu);
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

static uint16_t vendor_at(uint8_t bus, uint8_t slot, uint8_t function) {
    return (uint16_t)(pci_config_read32(bus, slot, function, 0x00) & 0xffffu);
}

static void record_device(uint8_t bus, uint8_t slot, uint8_t function) {
    if (inventory_count >= PCI_MAX_DEVICES) return;
    uint32_t id = pci_config_read32(bus, slot, function, 0x00);
    if ((id & 0xffffu) == 0xffffu) return;

    pci_device_t *device = &inventory[inventory_count++];
    device->bus = bus;
    device->slot = slot;
    device->function = function;
    device->vendor_id = (uint16_t)(id & 0xffffu);
    device->device_id = (uint16_t)(id >> 16);

    uint32_t class_reg = pci_config_read32(bus, slot, function, 0x08);
    device->revision = (uint8_t)class_reg;
    device->prog_if = (uint8_t)(class_reg >> 8);
    device->subclass = (uint8_t)(class_reg >> 16);
    device->class_code = (uint8_t)(class_reg >> 24);

    uint32_t header = pci_config_read32(bus, slot, function, 0x0c);
    device->header_type = (uint8_t)(header >> 16);
    for (uint32_t i = 0; i < 6; ++i) {
        device->bars[i] = pci_config_read32(bus, slot, function, (uint8_t)(0x10u + i * 4u));
    }
}

void pci_scan(void) {
    inventory_count = 0;
    for (uint16_t bus = 0; bus < 256; ++bus) {
        for (uint8_t slot = 0; slot < 32; ++slot) {
            if (vendor_at((uint8_t)bus, slot, 0) == 0xffffu) continue;
            uint32_t header = pci_config_read32((uint8_t)bus, slot, 0, 0x0c);
            uint8_t functions = ((header >> 16) & 0x80u) ? 8u : 1u;
            for (uint8_t function = 0; function < functions; ++function) {
                if (vendor_at((uint8_t)bus, slot, function) != 0xffffu) {
                    record_device((uint8_t)bus, slot, function);
                }
            }
        }
    }
}

uint32_t pci_count(void) {
    return inventory_count;
}

const pci_device_t *pci_get(uint32_t index) {
    return index < inventory_count ? &inventory[index] : 0;
}

const pci_device_t *pci_find_class(uint8_t class_code, uint8_t subclass, uint8_t prog_if) {
    for (uint32_t i = 0; i < inventory_count; ++i) {
        const pci_device_t *device = &inventory[i];
        if (device->class_code == class_code && device->subclass == subclass &&
            device->prog_if == prog_if) return device;
    }
    return 0;
}

int pci_decode_bar(const pci_device_t *device, uint32_t index, pci_bar_t *bar_out) {
    if (!device || !bar_out || index >= 6) return 0;
    uint32_t low = device->bars[index];
    if (low == 0 || low == UINT32_MAX) return 0;
    bar_out->io = (low & 1u) != 0;
    bar_out->is_64 = 0;
    bar_out->prefetchable = 0;
    if (bar_out->io) {
        bar_out->base = low & ~UINT32_C(0x3);
        return bar_out->base != 0;
    }

    uint32_t type = (low >> 1) & 0x03u;
    bar_out->prefetchable = (low & 0x08u) != 0;
    uint64_t base = low & ~UINT32_C(0x0f);
    if (type == 0x02u) {
        if (index + 1u >= 6) return 0;
        bar_out->is_64 = 1;
        base |= (uint64_t)device->bars[index + 1u] << 32;
    } else if (type != 0x00u) {
        return 0;
    }
    bar_out->base = base;
    return base != 0;
}
