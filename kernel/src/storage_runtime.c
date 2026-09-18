#include "storage_runtime.h"
#include "ahci.h"
#include "block.h"
#include "partition.h"
#include "pci.h"
#include "serial.h"
#include "storage_services.h"
#include <stddef.h>
#include <stdint.h>

#define STORAGE_SECTOR_MAX 4096u

static uint8_t sector_buffer[STORAGE_SECTOR_MAX];

static int bytes_equal(const uint8_t *a, const uint8_t *b, size_t length) {
    for (size_t i = 0; i < length; ++i) if (a[i] != b[i]) return 0;
    return 1;
}

#ifdef JOSHOS_STORAGE_TEST
static int test_partition_disk(const block_device_t *device) {
    static const uint8_t marker[] = "JOSHOS-STORAGE-READ-OK";
    static const uint8_t write_pattern[] = "JOSHOS-STORAGE-WRITE-OK";

    if (!device || device->sector_size < sizeof(marker) - 1u ||
        device->sector_size > STORAGE_SECTOR_MAX) return 0;

    partition_table_t table;
    if (partition_scan(device, &table) != PARTITION_OK || table.count == 0) return 0;

    for (uint32_t p = 0; p < table.count; ++p) {
        const partition_t *part = &table.entries[p];
        if (part->sector_count < 2) continue;
        if (block_read(device, part->first_lba, 1, sector_buffer) != BLOCK_OK) continue;
        if (!bytes_equal(sector_buffer, marker, sizeof(marker) - 1u)) continue;

        serial_write("JOSHOS_STORAGE_PARTITION_OK\n");
        serial_write("JOSHOS_STORAGE_READ_OK\n");

        for (uint32_t i = 0; i < device->sector_size; ++i) sector_buffer[i] = 0;
        for (size_t i = 0; i < sizeof(write_pattern) - 1u; ++i) sector_buffer[i] = write_pattern[i];
        if (block_write(device, part->first_lba + 1u, 1, sector_buffer) != BLOCK_OK) return 0;
        if (block_flush(device) != BLOCK_OK) return 0;
        serial_write("JOSHOS_STORAGE_FLUSH_OK\n");

        for (uint32_t i = 0; i < device->sector_size; ++i) sector_buffer[i] = 0;
        if (block_read(device, part->first_lba + 1u, 1, sector_buffer) != BLOCK_OK) return 0;
        if (!bytes_equal(sector_buffer, write_pattern, sizeof(write_pattern) - 1u)) return 0;
        serial_write("JOSHOS_STORAGE_WRITE_OK\n");
        return 1;
    }
    return 0;
}
#endif

int storage_runtime_init(const boot_context_t *boot) {
    if (!boot) return 0;

    block_registry_reset();
    if (storage_services_init() != VFS_OK) {
        serial_write("JOSHOS_ERROR_STORAGE_SERVICES\n");
        return 0;
    }
    serial_write("JOSHOS_STORAGE_SERVICES_OK\n");

    pci_scan();
    if (pci_count() == 0) {
        serial_write("JOSHOS_ERROR_PCI_EMPTY\n");
        return 0;
    }
    serial_write("JOSHOS_PCI_OK\n");

    const pci_device_t *nvme = pci_find_class(
        PCI_CLASS_MASS_STORAGE, PCI_SUBCLASS_NVM, PCI_PROGIF_NVME);
    if (nvme) serial_write("JOSHOS_NVME_FOUND\n");

    ahci_status_t ahci = ahci_probe(boot);
    if (ahci == AHCI_NO_CONTROLLER || ahci == AHCI_NO_DISK) {
#ifdef JOSHOS_STORAGE_TEST
        serial_write("JOSHOS_ERROR_AHCI_TEST_DISK\n");
        return 0;
#else
        serial_write("JOSHOS_AHCI_NO_DISK\n");
        return 1;
#endif
    }
    if (ahci != AHCI_OK) {
        serial_write("JOSHOS_ERROR_AHCI_INIT\n");
        serial_write(ahci_status_string(ahci));
        serial_write("\n");
        return 0;
    }
    serial_write("JOSHOS_AHCI_OK\n");

    for (uint32_t i = 0; i < block_count(); ++i) {
        const block_device_t *device = block_get(i);
        partition_table_t table;
        if (device && partition_scan(device, &table) == PARTITION_OK) {
            serial_write("JOSHOS_PARTITION_SCAN_OK\n");
            break;
        }
    }

#ifdef JOSHOS_STORAGE_TEST
    for (uint32_t i = 0; i < block_count(); ++i) {
        if (test_partition_disk(block_get(i))) {
            serial_write("JOSHOS_STORAGE_TEST_OK\n");
            return 1;
        }
    }
    serial_write("JOSHOS_ERROR_STORAGE_TEST\n");
    return 0;
#else
    (void)log_service_append("storage runtime initialised\n");
    return 1;
#endif
}
