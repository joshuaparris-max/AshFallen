#include "ahci.h"

#include <stdint.h>
#include "pmm.h"
#include "paging.h"

#define HBA_CAP             0x00u
#define HBA_GHC             0x04u
#define HBA_PI              0x0Cu
#define HBA_PORT_BASE       0x100u
#define HBA_PORT_STRIDE     0x80u

#define PORT_CLB            0x00u
#define PORT_CLBU           0x04u
#define PORT_FB             0x08u
#define PORT_FBU            0x0Cu
#define PORT_IS             0x10u
#define PORT_CMD            0x18u
#define PORT_TFD            0x20u
#define PORT_SIG            0x24u
#define PORT_SSTS           0x28u
#define PORT_SERR           0x30u
#define PORT_SACT           0x34u
#define PORT_CI             0x38u

#define GHC_AE              (1u << 31)
#define CAP_S64A            (1u << 31)

#define CMD_ST              (1u << 0)
#define CMD_FRE             (1u << 4)
#define CMD_FR              (1u << 14)
#define CMD_CR              (1u << 15)

#define TFD_ERR             (1u << 0)
#define TFD_DRQ             (1u << 3)
#define TFD_BSY             (1u << 7)
#define PORT_IS_TFES        (1u << 30)

#define SATA_SIG_ATA        0x00000101u
#define SATA_DET_PRESENT    0x03u
#define SATA_IPM_ACTIVE     0x01u

#define ATA_CMD_READ_DMA_EXT   0x25u
#define ATA_CMD_WRITE_DMA_EXT  0x35u
#define ATA_CMD_IDENTIFY       0xECu
#define ATA_CMD_FLUSH_EXT      0xEAu

#define FIS_TYPE_REG_H2D       0x27u
#define FIS_COMMAND            0x80u
#define ATA_DEVICE_LBA         0x40u

#define AHCI_CMD_CFL_DWORDS    5u
#define AHCI_CMD_WRITE         (1u << 6)
#define AHCI_PRDT_IOC          (1u << 31)

#define AHCI_WAIT_SPINS        10000000u

typedef struct {
    uint16_t flags;
    uint16_t prdt_length;
    uint32_t prd_bytes;
    uint32_t command_table_base;
    uint32_t command_table_base_upper;
    uint32_t reserved[4];
} __attribute__((packed)) ahci_command_header_t;

typedef struct {
    uint32_t data_base;
    uint32_t data_base_upper;
    uint32_t reserved;
    uint32_t byte_count_and_flags;
} __attribute__((packed)) ahci_prdt_entry_t;

typedef struct {
    uint8_t command_fis[64];
    uint8_t atapi_command[16];
    uint8_t reserved[48];
    ahci_prdt_entry_t prdt[1];
} __attribute__((packed)) ahci_command_table_t;

_Static_assert(sizeof(ahci_command_header_t) == 32, "AHCI command header layout");
_Static_assert(sizeof(ahci_prdt_entry_t) == 16, "AHCI PRDT layout");

static void zero_bytes(void *buffer, uint32_t length) {
    uint8_t *bytes = buffer;
    while (length--) *bytes++ = 0;
}

static void copy_bytes(void *destination, const void *source, uint32_t length) {
    uint8_t *dst = destination;
    const uint8_t *src = source;
    while (length--) *dst++ = *src++;
}

static uint16_t read_le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static void cpu_pause(void) {
    __asm__ volatile ("pause" ::: "memory");
}

static void *phys_ptr(const boot_context_t *boot, uint64_t physical) {
    (void)boot;
    return paging_direct_pointer(physical);
}

static volatile uint32_t *hba_reg(
    const ahci_device_t *device,
    uint32_t offset
) {
    return (volatile uint32_t *)(void *)(device->abar + offset);
}

static volatile uint32_t *port_reg(
    const ahci_device_t *device,
    uint32_t offset
) {
    uint32_t base =
        HBA_PORT_BASE + (uint32_t)device->port_index * HBA_PORT_STRIDE;
    return hba_reg(device, base + offset);
}

static int address_supported(
    const ahci_device_t *device,
    uint64_t physical
) {
    uint32_t cap = *hba_reg(device, HBA_CAP);
    return (physical >> 32) == 0 || (cap & CAP_S64A) != 0;
}

static int wait_command_bits_clear(
    ahci_device_t *device,
    uint32_t mask
) {
    for (uint32_t spin = 0; spin < AHCI_WAIT_SPINS; ++spin) {
        if ((*port_reg(device, PORT_CMD) & mask) == 0) return 1;
        cpu_pause();
    }
    return 0;
}

static ahci_status_t stop_engine(ahci_device_t *device) {
    volatile uint32_t *cmd = port_reg(device, PORT_CMD);

    *cmd &= ~CMD_ST;
    if (!wait_command_bits_clear(device, CMD_CR)) {
        return AHCI_ENGINE_TIMEOUT;
    }

    *cmd &= ~CMD_FRE;
    if (!wait_command_bits_clear(device, CMD_FR)) {
        return AHCI_ENGINE_TIMEOUT;
    }

    return AHCI_OK;
}

static void start_engine(ahci_device_t *device) {
    volatile uint32_t *cmd = port_reg(device, PORT_CMD);
    *cmd |= CMD_FRE;
    *cmd |= CMD_ST;
}

static int sata_port_present(
    ahci_device_t *device,
    uint8_t port
) {
    device->port_index = port;

    uint32_t ssts = *port_reg(device, PORT_SSTS);
    uint32_t det = ssts & 0x0Fu;
    uint32_t ipm = (ssts >> 8) & 0x0Fu;

    if (det != SATA_DET_PRESENT || ipm != SATA_IPM_ACTIVE) return 0;
    return *port_reg(device, PORT_SIG) == SATA_SIG_ATA;
}

static ahci_status_t allocate_dma(ahci_device_t *device) {
    uint64_t *frames[] = {
        &device->command_list_phys,
        &device->received_fis_phys,
        &device->command_table_phys,
        &device->transfer_buffer_phys
    };

    for (uint32_t i = 0; i < 4u; ++i) {
        *frames[i] = pmm_alloc_frame();
        if (*frames[i] == UINT64_MAX) return AHCI_NO_MEMORY;
        if (!address_supported(device, *frames[i])) {
            return AHCI_DMA_ADDRESS_ERROR;
        }

        void *pointer = phys_ptr(device->boot, *frames[i]);
        if (!pointer) return AHCI_DMA_ADDRESS_ERROR;
        zero_bytes(pointer, (uint32_t)PMM_PAGE_SIZE);
    }

    return AHCI_OK;
}

static ahci_command_header_t *command_headers(ahci_device_t *device) {
    return (ahci_command_header_t *)phys_ptr(
        device->boot, device->command_list_phys);
}

static ahci_command_table_t *command_table(ahci_device_t *device) {
    return (ahci_command_table_t *)phys_ptr(
        device->boot, device->command_table_phys);
}

static uint8_t *transfer_buffer(ahci_device_t *device) {
    return (uint8_t *)phys_ptr(
        device->boot, device->transfer_buffer_phys);
}

static ahci_status_t configure_port(ahci_device_t *device) {
    ahci_status_t status = stop_engine(device);
    if (status != AHCI_OK) return status;

    *port_reg(device, PORT_CLB) = (uint32_t)device->command_list_phys;
    *port_reg(device, PORT_CLBU) =
        (uint32_t)(device->command_list_phys >> 32);
    *port_reg(device, PORT_FB) = (uint32_t)device->received_fis_phys;
    *port_reg(device, PORT_FBU) =
        (uint32_t)(device->received_fis_phys >> 32);

    *port_reg(device, PORT_SERR) = UINT32_MAX;
    *port_reg(device, PORT_IS) = UINT32_MAX;

    start_engine(device);
    return AHCI_OK;
}

static int wait_device_ready(ahci_device_t *device) {
    for (uint32_t spin = 0; spin < AHCI_WAIT_SPINS; ++spin) {
        uint32_t tfd = *port_reg(device, PORT_TFD);
        if ((tfd & (TFD_BSY | TFD_DRQ)) == 0) return 1;
        cpu_pause();
    }
    return 0;
}

static ahci_status_t issue_command(
    ahci_device_t *device,
    uint8_t ata_command,
    uint64_t lba,
    int write,
    int data_transfer
) {
    if (!wait_device_ready(device)) return AHCI_DEVICE_TIMEOUT;

    if ((*port_reg(device, PORT_SACT) & 1u) != 0 ||
        (*port_reg(device, PORT_CI) & 1u) != 0) {
        return AHCI_DEVICE_TIMEOUT;
    }

    ahci_command_header_t *headers = command_headers(device);
    ahci_command_table_t *table = command_table(device);
    if (!headers || !table) return AHCI_DMA_ADDRESS_ERROR;

    ahci_command_header_t *header = &headers[0];
    zero_bytes(header, sizeof(*header));
    zero_bytes(table, sizeof(*table));

    header->flags = AHCI_CMD_CFL_DWORDS |
        (write ? AHCI_CMD_WRITE : 0u);
    header->prdt_length = data_transfer ? 1u : 0u;
    header->command_table_base = (uint32_t)device->command_table_phys;
    header->command_table_base_upper =
        (uint32_t)(device->command_table_phys >> 32);

    if (data_transfer) {
        table->prdt[0].data_base =
            (uint32_t)device->transfer_buffer_phys;
        table->prdt[0].data_base_upper =
            (uint32_t)(device->transfer_buffer_phys >> 32);
        table->prdt[0].byte_count_and_flags =
            (JOSH_BLOCK_SECTOR_SIZE - 1u) | AHCI_PRDT_IOC;
    }

    uint8_t *fis = table->command_fis;
    fis[0] = FIS_TYPE_REG_H2D;
    fis[1] = FIS_COMMAND;
    fis[2] = ata_command;
    fis[4] = (uint8_t)lba;
    fis[5] = (uint8_t)(lba >> 8);
    fis[6] = (uint8_t)(lba >> 16);
    fis[7] = ATA_DEVICE_LBA;
    fis[8] = (uint8_t)(lba >> 24);
    fis[9] = (uint8_t)(lba >> 32);
    fis[10] = (uint8_t)(lba >> 40);
    if (data_transfer && ata_command != ATA_CMD_IDENTIFY) {
        fis[12] = 1;
        fis[13] = 0;
    }

    __asm__ volatile ("" ::: "memory");
    *port_reg(device, PORT_IS) = UINT32_MAX;
    *port_reg(device, PORT_CI) = 1u;

    for (uint32_t spin = 0; spin < AHCI_WAIT_SPINS; ++spin) {
        uint32_t interrupt_status = *port_reg(device, PORT_IS);
        if ((interrupt_status & PORT_IS_TFES) != 0) {
            return AHCI_IO_ERROR;
        }

        if ((*port_reg(device, PORT_CI) & 1u) == 0) {
            __asm__ volatile ("" ::: "memory");
            if ((*port_reg(device, PORT_TFD) & TFD_ERR) != 0) {
                return AHCI_IO_ERROR;
            }
            return AHCI_OK;
        }

        cpu_pause();
    }

    return AHCI_DEVICE_TIMEOUT;
}

static ahci_status_t identify_device(ahci_device_t *device) {
    uint8_t *buffer = transfer_buffer(device);
    if (!buffer) return AHCI_DMA_ADDRESS_ERROR;
    zero_bytes(buffer, JOSH_BLOCK_SECTOR_SIZE);

    ahci_status_t status = issue_command(
        device, ATA_CMD_IDENTIFY, 0, 0, 1);
    if (status != AHCI_OK) return status;

    uint16_t capabilities83 = read_le16(buffer + 83u * 2u);
    if ((capabilities83 & (1u << 10)) == 0) {
        return AHCI_UNSUPPORTED_DEVICE;
    }

    uint64_t sectors = 0;
    sectors |= (uint64_t)read_le16(buffer + 100u * 2u);
    sectors |= (uint64_t)read_le16(buffer + 101u * 2u) << 16;
    sectors |= (uint64_t)read_le16(buffer + 102u * 2u) << 32;
    sectors |= (uint64_t)read_le16(buffer + 103u * 2u) << 48;

    if (sectors == 0) return AHCI_IO_ERROR;
    device->sector_count = sectors;
    return AHCI_OK;
}

ahci_status_t ahci_init(
    const pci_device_t *controller,
    const boot_context_t *boot,
    ahci_device_t *device
) {
    if (!controller || !boot || !device ||
        pci_classify_storage(controller) != PCI_STORAGE_AHCI) {
        return AHCI_NO_CONTROLLER;
    }

    uint32_t bar = controller->bars[5];
    if ((bar & 1u) != 0 ||
        ((bar >> 1) & 3u) != 0 ||
        (bar & ~0x0Fu) == 0) {
        return AHCI_BAD_BAR;
    }

    if (pci_enable_memory_bus_master(controller) != 0) {
        return AHCI_BAD_BAR;
    }

    *device = (ahci_device_t){0};
    device->boot = boot;

    uint64_t abar_phys = (uint64_t)(bar & ~0x0Fu);
    void *abar = paging_map_mmio(abar_phys, UINT64_C(0x2000));
    if (!abar) return AHCI_MMIO_UNAVAILABLE;
    device->abar = (volatile uint8_t *)abar;

    *hba_reg(device, HBA_GHC) |= GHC_AE;

    ahci_status_t status = allocate_dma(device);
    if (status != AHCI_OK) return status;

    uint32_t ports = *hba_reg(device, HBA_PI);
    int found = 0;
    for (uint8_t port = 0; port < 32u; ++port) {
        if ((ports & (1u << port)) == 0) continue;
        if (sata_port_present(device, port)) {
            found = 1;
            break;
        }
    }

    if (!found) return AHCI_NO_SATA_DEVICE;

    status = configure_port(device);
    if (status != AHCI_OK) return status;

    return identify_device(device);
}

ahci_status_t ahci_read_sector(
    ahci_device_t *device,
    uint64_t lba,
    void *buffer
) {
    if (!device || !buffer || lba >= device->sector_count ||
        lba >= (UINT64_C(1) << 48)) {
        return AHCI_IO_ERROR;
    }

    ahci_status_t status = issue_command(
        device, ATA_CMD_READ_DMA_EXT, lba, 0, 1);
    if (status != AHCI_OK) return status;

    uint8_t *dma = transfer_buffer(device);
    if (!dma) return AHCI_DMA_ADDRESS_ERROR;
    copy_bytes(buffer, dma, JOSH_BLOCK_SECTOR_SIZE);
    return AHCI_OK;
}

ahci_status_t ahci_write_sector(
    ahci_device_t *device,
    uint64_t lba,
    const void *buffer
) {
    if (!device || !buffer || lba >= device->sector_count ||
        lba >= (UINT64_C(1) << 48)) {
        return AHCI_IO_ERROR;
    }

    uint8_t *dma = transfer_buffer(device);
    if (!dma) return AHCI_DMA_ADDRESS_ERROR;
    copy_bytes(dma, buffer, JOSH_BLOCK_SECTOR_SIZE);

    ahci_status_t status = issue_command(
        device, ATA_CMD_WRITE_DMA_EXT, lba, 1, 1);
    if (status != AHCI_OK) return status;

    return issue_command(device, ATA_CMD_FLUSH_EXT, 0, 0, 0);
}

static int block_read_callback(
    void *context,
    uint64_t lba,
    uint32_t sector_count,
    void *buffer
) {
    ahci_device_t *device = context;
    uint8_t *bytes = buffer;

    for (uint32_t i = 0; i < sector_count; ++i) {
        if (ahci_read_sector(
                device,
                lba + i,
                bytes + (uint64_t)i * JOSH_BLOCK_SECTOR_SIZE) != AHCI_OK) {
            return -1;
        }
    }
    return 0;
}

static int block_write_callback(
    void *context,
    uint64_t lba,
    uint32_t sector_count,
    const void *buffer
) {
    ahci_device_t *device = context;
    const uint8_t *bytes = buffer;

    for (uint32_t i = 0; i < sector_count; ++i) {
        if (ahci_write_sector(
                device,
                lba + i,
                bytes + (uint64_t)i * JOSH_BLOCK_SECTOR_SIZE) != AHCI_OK) {
            return -1;
        }
    }
    return 0;
}

int ahci_make_block_device(
    ahci_device_t *ahci,
    josh_block_device_t *block
) {
    if (!ahci || !block || ahci->sector_count == 0) return -1;

    block->context = ahci;
    block->sector_count = ahci->sector_count;
    block->read = block_read_callback;
    block->write = block_write_callback;
    return 0;
}

const char *ahci_status_string(ahci_status_t status) {
    switch (status) {
        case AHCI_OK: return "ok";
        case AHCI_NO_CONTROLLER: return "not an AHCI controller";
        case AHCI_BAD_BAR: return "invalid or disabled AHCI BAR";
        case AHCI_MMIO_UNAVAILABLE: return "AHCI MMIO unavailable";
        case AHCI_NO_SATA_DEVICE: return "no SATA disk present";
        case AHCI_NO_MEMORY: return "no DMA memory";
        case AHCI_DMA_ADDRESS_ERROR: return "DMA address unavailable";
        case AHCI_ENGINE_TIMEOUT: return "AHCI engine timeout";
        case AHCI_DEVICE_TIMEOUT: return "ATA device timeout";
        case AHCI_IO_ERROR: return "AHCI I/O error";
        case AHCI_UNSUPPORTED_DEVICE: return "ATA device lacks LBA48";
        default: return "unknown AHCI error";
    }
}
