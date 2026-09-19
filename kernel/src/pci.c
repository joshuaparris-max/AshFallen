#include "pci.h"
#include "io.h"

#define PCI_CONFIG_ADDRESS 0xCF8u
#define PCI_CONFIG_DATA    0xCFCu

#define PCI_VENDOR_NONE 0xffffu
#define PCI_HEADER_MULTIFUNCTION 0x80u
#define PCI_HEADER_TYPE_MASK 0x7fu
#define PCI_HEADER_NORMAL 0x00u

#define PCI_COMMAND_IO         0x0001u
#define PCI_COMMAND_MEMORY     0x0002u
#define PCI_COMMAND_BUS_MASTER 0x0004u

static pci_device_t devices[PCI_MAX_DEVICES];
static uint32_t device_count;

uint32_t pci_config_address(uint8_t bus, uint8_t device,
                            uint8_t function, uint8_t offset) {
    return UINT32_C(0x80000000)
        | ((uint32_t)bus << 16)
        | ((uint32_t)(device & 0x1fu) << 11)
        | ((uint32_t)(function & 0x07u) << 8)
        | ((uint32_t)offset & 0xfcu);
}

uint32_t pci_config_read32(uint8_t bus, uint8_t device,
                           uint8_t function, uint8_t offset) {
    outl(PCI_CONFIG_ADDRESS, pci_config_address(bus, device, function, offset));
    return inl(PCI_CONFIG_DATA);
}

uint16_t pci_config_read16(uint8_t bus, uint8_t device,
                           uint8_t function, uint8_t offset) {
    uint32_t value = pci_config_read32(bus, device, function, offset);
    uint32_t shift = (uint32_t)(offset & 2u) * 8u;
    return (uint16_t)(value >> shift);
}

uint8_t pci_config_read8(uint8_t bus, uint8_t device,
                         uint8_t function, uint8_t offset) {
    uint32_t value = pci_config_read32(bus, device, function, offset);
    uint32_t shift = (uint32_t)(offset & 3u) * 8u;
    return (uint8_t)(value >> shift);
}

void pci_config_write16(uint8_t bus, uint8_t device,
                        uint8_t function, uint8_t offset, uint16_t value) {
    uint8_t aligned = (uint8_t)(offset & 0xfcu);
    uint32_t current = pci_config_read32(bus, device, function, aligned);
    uint32_t shift = (uint32_t)(offset & 2u) * 8u;
    current &= ~(UINT32_C(0xffff) << shift);
    current |= (uint32_t)value << shift;
    outl(PCI_CONFIG_ADDRESS, pci_config_address(bus, device, function, aligned));
    outl(PCI_CONFIG_DATA, current);
}

static void read_bars(uint8_t bus, uint8_t slot, uint8_t function,
                      pci_device_t *out) {
    for (uint8_t i = 0; i < 6; ++i) out->bars[i] = 0;
    if ((out->header_type & PCI_HEADER_TYPE_MASK) != PCI_HEADER_NORMAL) return;
    for (uint8_t i = 0; i < 6; ++i) {
        out->bars[i] = pci_config_read32(
            bus, slot, function, (uint8_t)(0x10u + i * 4u));
    }
}

static int read_function(uint8_t bus, uint8_t slot, uint8_t function,
                         pci_device_t *out) {
    uint32_t id = pci_config_read32(bus, slot, function, 0x00);
    uint16_t vendor = (uint16_t)id;
    if (vendor == PCI_VENDOR_NONE) return 0;

    uint32_t class_revision = pci_config_read32(bus, slot, function, 0x08);
    uint32_t header = pci_config_read32(bus, slot, function, 0x0c);
    uint32_t interrupt = pci_config_read32(bus, slot, function, 0x3c);

    out->bus = bus;
    out->device = slot;
    out->function = function;
    out->vendor_id = vendor;
    out->device_id = (uint16_t)(id >> 16);
    out->revision = (uint8_t)class_revision;
    out->prog_if = (uint8_t)(class_revision >> 8);
    out->subclass = (uint8_t)(class_revision >> 16);
    out->class_code = (uint8_t)(class_revision >> 24);
    out->header_type = (uint8_t)(header >> 16);
    out->interrupt_line = (uint8_t)interrupt;
    out->interrupt_pin = (uint8_t)(interrupt >> 8);
    read_bars(bus, slot, function, out);
    return 1;
}

static void append_device(const pci_device_t *device) {
    if (device_count < PCI_MAX_DEVICES) devices[device_count++] = *device;
}

static void scan_extra_functions(uint8_t bus, uint8_t slot) {
    for (uint8_t function = 1; function < 8u; ++function) {
        pci_device_t found;
        if (read_function(bus, slot, function, &found)) append_device(&found);
    }
}

static void scan_slot(uint8_t bus, uint8_t slot) {
    pci_device_t first;
    if (!read_function(bus, slot, 0, &first)) return;
    append_device(&first);
    if ((first.header_type & PCI_HEADER_MULTIFUNCTION) != 0) {
        scan_extra_functions(bus, slot);
    }
}

void pci_init(void) {
    device_count = 0;
    for (uint16_t bus = 0; bus < 256u; ++bus) {
        for (uint8_t slot = 0; slot < 32u; ++slot) {
            scan_slot((uint8_t)bus, slot);
        }
    }
}

uint32_t pci_device_count(void) {
    return device_count;
}

const pci_device_t *pci_device_at(uint32_t index) {
    if (index >= device_count) return 0;
    return &devices[index];
}

const pci_device_t *pci_find_device(uint16_t vendor_id, uint16_t device_id) {
    for (uint32_t i = 0; i < device_count; ++i) {
        if (devices[i].vendor_id == vendor_id && devices[i].device_id == device_id) {
            return &devices[i];
        }
    }
    return 0;
}

const pci_device_t *pci_find_class(uint8_t class_code, uint8_t subclass,
                                   uint32_t start_index) {
    for (uint32_t i = start_index; i < device_count; ++i) {
        if (devices[i].class_code == class_code && devices[i].subclass == subclass) {
            return &devices[i];
        }
    }
    return 0;
}

pci_status_t pci_decode_bar(const pci_device_t *device, uint8_t index,
                            pci_bar_t *bar) {
    if (!device || !bar || index >= 6u) return PCI_BAD_ARGUMENT;
    uint32_t low = device->bars[index];
    if (low == 0 || low == UINT32_MAX) return PCI_NOT_FOUND;

    bar->base = 0;
    bar->is_io = 0;
    bar->is_64bit = 0;
    bar->prefetchable = 0;

    if (low & 1u) {
        bar->is_io = 1;
        bar->base = low & ~UINT32_C(3);
        return PCI_OK;
    }

    uint8_t type = (uint8_t)((low >> 1) & 3u);
    bar->prefetchable = (uint8_t)((low >> 3) & 1u);
    if (type == 0u) {
        bar->base = low & ~UINT32_C(0x0f);
        return PCI_OK;
    }
    if (type == 2u) {
        if (index >= 5u) return PCI_UNSUPPORTED_BAR;
        bar->is_64bit = 1;
        bar->base = (uint64_t)(low & ~UINT32_C(0x0f))
                  | ((uint64_t)device->bars[index + 1u] << 32);
        return PCI_OK;
    }
    return PCI_UNSUPPORTED_BAR;
}

pci_status_t pci_enable_memory_busmaster(const pci_device_t *device) {
    if (!device) return PCI_BAD_ARGUMENT;
    uint16_t command = pci_config_read16(
        device->bus, device->device, device->function, 0x04);
    command |= PCI_COMMAND_MEMORY | PCI_COMMAND_BUS_MASTER;
    pci_config_write16(
        device->bus, device->device, device->function, 0x04, command);
    uint16_t verify = pci_config_read16(
        device->bus, device->device, device->function, 0x04);
    if ((verify & (PCI_COMMAND_MEMORY | PCI_COMMAND_BUS_MASTER))
        != (PCI_COMMAND_MEMORY | PCI_COMMAND_BUS_MASTER)) {
        return PCI_NOT_FOUND;
    }
    return PCI_OK;
}

const char *pci_status_string(pci_status_t status) {
    switch (status) {
        case PCI_OK: return "ok";
        case PCI_BAD_ARGUMENT: return "invalid argument";
        case PCI_NOT_FOUND: return "PCI resource not found";
        case PCI_TABLE_FULL: return "PCI device table full";
        case PCI_UNSUPPORTED_BAR: return "unsupported PCI BAR";
        default: return "unknown PCI error";
    }
}
