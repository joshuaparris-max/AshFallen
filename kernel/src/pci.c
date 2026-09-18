#include "pci.h"

#ifndef JOSHOS_HOST_TEST
#include "io.h"
#include "serial.h"
#endif

#define PCI_CONFIG_ADDRESS 0xCF8u
#define PCI_CONFIG_DATA    0xCFCu

#define PCI_CLASS_MASS_STORAGE 0x01u
#define PCI_SUBCLASS_IDE       0x01u
#define PCI_SUBCLASS_RAID      0x04u
#define PCI_SUBCLASS_SATA      0x06u
#define PCI_SUBCLASS_NVM       0x08u
#define PCI_PROGIF_AHCI        0x01u
#define PCI_PROGIF_NVME        0x02u

#ifndef JOSHOS_HOST_TEST
static pci_device_t first_ahci;
static pci_device_t first_nvme;
static int have_ahci;
static int have_nvme;
#endif

static uint16_t vendor_id(uint32_t value) {
    return (uint16_t)(value & 0xFFFFu);
}

static void fill_device(
    const pci_config_access_t *access,
    uint8_t bus,
    uint8_t device,
    uint8_t function,
    uint32_t id,
    pci_device_t *out
) {
    uint32_t class_info = access->read32(
        access->context, bus, device, function, 0x08u);
    uint32_t header_info = access->read32(
        access->context, bus, device, function, 0x0Cu);

    out->bus = bus;
    out->device = device;
    out->function = function;
    out->vendor_id = vendor_id(id);
    out->device_id = (uint16_t)(id >> 16);
    out->class_code = (uint8_t)(class_info >> 24);
    out->subclass = (uint8_t)(class_info >> 16);
    out->prog_if = (uint8_t)(class_info >> 8);
    out->header_type = (uint8_t)(header_info >> 16);

    for (uint8_t bar = 0; bar < 6u; ++bar) {
        out->bars[bar] = access->read32(
            access->context,
            bus,
            device,
            function,
            (uint8_t)(0x10u + bar * 4u));
    }
}

pci_storage_kind_t pci_classify_storage(const pci_device_t *device) {
    if (!device || device->class_code != PCI_CLASS_MASS_STORAGE) {
        return PCI_STORAGE_NONE;
    }

    if (device->subclass == PCI_SUBCLASS_SATA &&
        device->prog_if == PCI_PROGIF_AHCI) {
        return PCI_STORAGE_AHCI;
    }

    if (device->subclass == PCI_SUBCLASS_NVM &&
        device->prog_if == PCI_PROGIF_NVME) {
        return PCI_STORAGE_NVME;
    }

    if (device->subclass == PCI_SUBCLASS_IDE) return PCI_STORAGE_IDE;
    if (device->subclass == PCI_SUBCLASS_RAID) return PCI_STORAGE_RAID;
    return PCI_STORAGE_OTHER;
}

int pci_scan(
    const pci_config_access_t *access,
    pci_scan_summary_t *summary,
    pci_device_visitor_fn visitor,
    void *visitor_context
) {
    if (!access || !access->read32 || !summary) return -1;

    *summary = (pci_scan_summary_t){0};

    for (uint16_t bus = 0; bus < 256u; ++bus) {
        for (uint8_t device = 0; device < 32u; ++device) {
            uint32_t function0_id = access->read32(
                access->context, (uint8_t)bus, device, 0, 0x00u);
            if (vendor_id(function0_id) == 0xFFFFu) continue;

            uint32_t header_info = access->read32(
                access->context, (uint8_t)bus, device, 0, 0x0Cu);
            uint8_t header_type = (uint8_t)(header_info >> 16);
            uint8_t function_count = (header_type & 0x80u) ? 8u : 1u;

            for (uint8_t function = 0; function < function_count; ++function) {
                uint32_t id = function == 0
                    ? function0_id
                    : access->read32(
                        access->context, (uint8_t)bus, device, function, 0x00u);

                if (vendor_id(id) == 0xFFFFu) continue;

                pci_device_t found;
                fill_device(
                    access, (uint8_t)bus, device, function, id, &found);

                ++summary->device_count;

                pci_storage_kind_t storage = pci_classify_storage(&found);
                if (storage != PCI_STORAGE_NONE) {
                    ++summary->mass_storage_count;
                    if (storage == PCI_STORAGE_AHCI) ++summary->ahci_count;
                    if (storage == PCI_STORAGE_NVME) ++summary->nvme_count;
                }

                if (visitor) visitor(&found, visitor_context);
            }
        }
    }

    return 0;
}

#ifndef JOSHOS_HOST_TEST
static uint32_t pci_address(
    uint8_t bus,
    uint8_t device,
    uint8_t function,
    uint8_t offset
) {
    return 0x80000000u |
        ((uint32_t)bus << 16) |
        ((uint32_t)device << 11) |
        ((uint32_t)function << 8) |
        ((uint32_t)offset & 0xFCu);
}

static uint32_t pci_x86_read32(
    void *context,
    uint8_t bus,
    uint8_t device,
    uint8_t function,
    uint8_t offset
) {
    (void)context;
    outl(PCI_CONFIG_ADDRESS, pci_address(bus, device, function, offset));
    return inl(PCI_CONFIG_DATA);
}

static void pci_x86_write32(
    uint8_t bus,
    uint8_t device,
    uint8_t function,
    uint8_t offset,
    uint32_t value
) {
    outl(PCI_CONFIG_ADDRESS, pci_address(bus, device, function, offset));
    outl(PCI_CONFIG_DATA, value);
}

static void report_storage_device(const pci_device_t *device, void *context) {
    (void)context;

    switch (pci_classify_storage(device)) {
        case PCI_STORAGE_AHCI:
            if (!have_ahci) {
                first_ahci = *device;
                have_ahci = 1;
            }
            serial_write("JOSHOS_STORAGE_AHCI_FOUND\n");
            break;
        case PCI_STORAGE_NVME:
            if (!have_nvme) {
                first_nvme = *device;
                have_nvme = 1;
            }
            serial_write("JOSHOS_STORAGE_NVME_FOUND\n");
            break;
        case PCI_STORAGE_IDE:
            serial_write("JOSHOS_STORAGE_IDE_FOUND\n");
            break;
        case PCI_STORAGE_RAID:
            serial_write("JOSHOS_STORAGE_RAID_FOUND\n");
            break;
        case PCI_STORAGE_OTHER:
            serial_write("JOSHOS_STORAGE_OTHER_FOUND\n");
            break;
        case PCI_STORAGE_NONE:
        default:
            break;
    }
}

int pci_init(pci_scan_summary_t *summary) {
    pci_config_access_t access = {
        .context = 0,
        .read32 = pci_x86_read32
    };

    have_ahci = 0;
    have_nvme = 0;
    serial_write("JOSHOS_PCI_SCAN_BEGIN\n");

    if (pci_scan(&access, summary, report_storage_device, 0) != 0) {
        serial_write("JOSHOS_PCI_SCAN_ERROR\n");
        return -1;
    }

    serial_write("JOSHOS_PCI_SCAN_OK\n");
    return 0;
}

int pci_get_first_storage(pci_storage_kind_t kind, pci_device_t *device) {
    if (!device) return -1;
    if (kind == PCI_STORAGE_AHCI && have_ahci) {
        *device = first_ahci;
        return 0;
    }
    if (kind == PCI_STORAGE_NVME && have_nvme) {
        *device = first_nvme;
        return 0;
    }
    return -1;
}

int pci_enable_memory_bus_master(const pci_device_t *device) {
    if (!device) return -1;

    uint32_t command_status = pci_x86_read32(
        0, device->bus, device->device, device->function, 0x04u);
    uint32_t command = (command_status & 0xFFFFu) | 0x0006u;

    pci_x86_write32(
        device->bus, device->device, device->function, 0x04u, command);

    uint32_t verified = pci_x86_read32(
        0, device->bus, device->device, device->function, 0x04u);
    return ((verified & 0x0006u) == 0x0006u) ? 0 : -1;
}
#else
int pci_init(pci_scan_summary_t *summary) {
    (void)summary;
    return -1;
}

int pci_get_first_storage(pci_storage_kind_t kind, pci_device_t *device) {
    (void)kind;
    (void)device;
    return -1;
}

int pci_enable_memory_bus_master(const pci_device_t *device) {
    (void)device;
    return -1;
}
#endif
