#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../src/pci.h"

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
} fake_pci_device_t;

typedef struct {
    const fake_pci_device_t *devices;
    unsigned count;
} fake_pci_bus_t;

static uint32_t fake_read32(
    void *context,
    uint8_t bus,
    uint8_t device,
    uint8_t function,
    uint8_t offset
) {
    const fake_pci_bus_t *fake = context;

    for (unsigned i = 0; i < fake->count; ++i) {
        const fake_pci_device_t *entry = &fake->devices[i];
        if (entry->bus != bus ||
            entry->device != device ||
            entry->function != function) {
            continue;
        }

        switch (offset & 0xFCu) {
            case 0x00u:
                return ((uint32_t)entry->device_id << 16) |
                       entry->vendor_id;
            case 0x08u:
                return ((uint32_t)entry->class_code << 24) |
                       ((uint32_t)entry->subclass << 16) |
                       ((uint32_t)entry->prog_if << 8);
            case 0x0Cu:
                return (uint32_t)entry->header_type << 16;
            default:
                return 0;
        }
    }

    return 0xFFFFFFFFu;
}

typedef struct {
    unsigned visited;
    unsigned ahci;
    unsigned nvme;
    unsigned ide;
} visit_counts_t;

static void visit(const pci_device_t *device, void *context) {
    visit_counts_t *counts = context;
    ++counts->visited;

    switch (pci_classify_storage(device)) {
        case PCI_STORAGE_AHCI: ++counts->ahci; break;
        case PCI_STORAGE_NVME: ++counts->nvme; break;
        case PCI_STORAGE_IDE: ++counts->ide; break;
        default: break;
    }
}

static void test_classification(void) {
    pci_device_t device = {0};

    device.class_code = 0x02;
    assert(pci_classify_storage(&device) == PCI_STORAGE_NONE);

    device.class_code = 0x01;
    device.subclass = 0x06;
    device.prog_if = 0x01;
    assert(pci_classify_storage(&device) == PCI_STORAGE_AHCI);

    device.subclass = 0x08;
    device.prog_if = 0x02;
    assert(pci_classify_storage(&device) == PCI_STORAGE_NVME);

    device.subclass = 0x01;
    device.prog_if = 0x8A;
    assert(pci_classify_storage(&device) == PCI_STORAGE_IDE);

    device.subclass = 0x04;
    device.prog_if = 0x00;
    assert(pci_classify_storage(&device) == PCI_STORAGE_RAID);

    device.subclass = 0x80;
    assert(pci_classify_storage(&device) == PCI_STORAGE_OTHER);
}

static void test_scan(void) {
    static const fake_pci_device_t devices[] = {
        {
            .bus = 0, .device = 1, .function = 0,
            .vendor_id = 0x8086, .device_id = 0x2922,
            .class_code = 0x01, .subclass = 0x06, .prog_if = 0x01
        },
        {
            .bus = 0, .device = 2, .function = 0,
            .vendor_id = 0x1B36, .device_id = 0x0010,
            .class_code = 0x01, .subclass = 0x08, .prog_if = 0x02
        },
        {
            .bus = 0, .device = 3, .function = 0,
            .vendor_id = 0x1234, .device_id = 0x1111,
            .class_code = 0x02, .subclass = 0x00, .prog_if = 0x00,
            .header_type = 0x80
        },
        {
            .bus = 0, .device = 3, .function = 1,
            .vendor_id = 0x1234, .device_id = 0x2222,
            .class_code = 0x01, .subclass = 0x01, .prog_if = 0x8A
        }
    };

    fake_pci_bus_t fake = {
        .devices = devices,
        .count = sizeof(devices) / sizeof(devices[0])
    };

    pci_config_access_t access = {
        .context = &fake,
        .read32 = fake_read32
    };

    pci_scan_summary_t summary;
    visit_counts_t visits = {0};

    assert(pci_scan(&access, &summary, visit, &visits) == 0);
    assert(summary.device_count == 4);
    assert(summary.mass_storage_count == 3);
    assert(summary.ahci_count == 1);
    assert(summary.nvme_count == 1);
    assert(visits.visited == 4);
    assert(visits.ahci == 1);
    assert(visits.nvme == 1);
    assert(visits.ide == 1);
}

int main(void) {
    test_classification();
    test_scan();
    puts("PCI discovery tests passed");
    return 0;
}
