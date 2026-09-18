#include "ahci.h"
#include "paging.h"
#include "pci.h"
#include "pmm.h"
#include <stddef.h>
#include <stdint.h>

#define AHCI_GHC_AE (1u << 31)
#define AHCI_CAP_S64A (1u << 31)
#define AHCI_PORT_CMD_ST (1u << 0)
#define AHCI_PORT_CMD_FRE (1u << 4)
#define AHCI_PORT_CMD_FR (1u << 14)
#define AHCI_PORT_CMD_CR (1u << 15)
#define AHCI_PORT_IS_TFES (1u << 30)
#define AHCI_TFD_BSY 0x80u
#define AHCI_TFD_DRQ 0x08u
#define AHCI_SSTS_DET_MASK 0x0fu
#define AHCI_SSTS_IPM_MASK 0x0f00u
#define AHCI_SSTS_DET_PRESENT 0x03u
#define AHCI_SSTS_IPM_ACTIVE 0x0100u
#define AHCI_SIG_ATA UINT32_C(0x00000101)
#define AHCI_FIS_REG_H2D 0x27u
#define ATA_CMD_IDENTIFY 0xecu
#define ATA_CMD_READ_DMA_EXT 0x25u
#define ATA_CMD_WRITE_DMA_EXT 0x35u
#define ATA_CMD_FLUSH_CACHE_EXT 0xeau
#define AHCI_COMMAND_TIMEOUT 2000000u
#define AHCI_SECTOR_SIZE 512u

typedef struct {
    volatile uint32_t clb;
    volatile uint32_t clbu;
    volatile uint32_t fb;
    volatile uint32_t fbu;
    volatile uint32_t is;
    volatile uint32_t ie;
    volatile uint32_t cmd;
    volatile uint32_t reserved0;
    volatile uint32_t tfd;
    volatile uint32_t sig;
    volatile uint32_t ssts;
    volatile uint32_t sctl;
    volatile uint32_t serr;
    volatile uint32_t sact;
    volatile uint32_t ci;
    volatile uint32_t sntf;
    volatile uint32_t fbs;
    volatile uint32_t devslp;
    volatile uint32_t reserved1[10];
    volatile uint32_t vendor[4];
} hba_port_t;

typedef struct {
    volatile uint32_t cap;
    volatile uint32_t ghc;
    volatile uint32_t is;
    volatile uint32_t pi;
    volatile uint32_t vs;
    volatile uint32_t ccc_ctl;
    volatile uint32_t ccc_pts;
    volatile uint32_t em_loc;
    volatile uint32_t em_ctl;
    volatile uint32_t cap2;
    volatile uint32_t bohc;
    uint8_t reserved[0xa0 - 0x2c];
    uint8_t vendor[0x100 - 0xa0];
    hba_port_t ports[32];
} hba_mem_t;

typedef struct {
    uint16_t flags;
    uint16_t prdtl;
    volatile uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t reserved[4];
} __attribute__((packed)) hba_cmd_header_t;

typedef struct {
    uint32_t dba;
    uint32_t dbau;
    uint32_t reserved;
    uint32_t dbc_i;
} __attribute__((packed)) hba_prdt_t;

typedef struct {
    uint8_t cfis[64];
    uint8_t acmd[16];
    uint8_t reserved[48];
    hba_prdt_t prdt[1];
} __attribute__((packed)) hba_cmd_table_t;

typedef struct {
    uint8_t fis_type;
    uint8_t pmport_c;
    uint8_t command;
    uint8_t featurel;
    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device;
    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t featureh;
    uint8_t countl;
    uint8_t counth;
    uint8_t icc;
    uint8_t control;
    uint8_t reserved[4];
} __attribute__((packed)) fis_reg_h2d_t;

typedef struct {
    const boot_context_t *boot;
    volatile hba_port_t *port;
    uint64_t command_list_phys;
    uint64_t fis_phys;
    uint64_t command_table_phys;
    uint64_t dma_phys;
    hba_cmd_header_t *command_list;
    hba_cmd_table_t *command_table;
    uint8_t *dma;
    uint64_t sectors;
    block_device_t block;
} ahci_disk_t;

_Static_assert(sizeof(hba_port_t) == 0x80, "AHCI port layout changed");
_Static_assert(sizeof(hba_cmd_header_t) == 32, "AHCI command header layout changed");
_Static_assert(sizeof(fis_reg_h2d_t) == 20, "AHCI register FIS layout changed");

static ahci_disk_t disks[AHCI_MAX_DISKS];
static uint32_t disk_count;

static void zero_bytes(void *pointer, size_t length) {
    volatile uint8_t *bytes = pointer;
    for (size_t i = 0; i < length; ++i) bytes[i] = 0;
}

static void *phys_pointer(const boot_context_t *boot, uint64_t physical) {
    if (!boot || physical >= boot->physical_memory_limit ||
        UINT64_MAX - boot->physical_memory_offset < physical) return 0;
    return (void *)(uintptr_t)(boot->physical_memory_offset + physical);
}

static int address_supported(uint32_t cap, uint64_t physical) {
    return (cap & AHCI_CAP_S64A) != 0 || (physical >> 32) == 0;
}

static int wait_clear(volatile uint32_t *register_address, uint32_t mask) {
    for (uint32_t i = 0; i < AHCI_COMMAND_TIMEOUT; ++i) {
        if ((*register_address & mask) == 0) return 1;
        __asm__ volatile ("pause");
    }
    return 0;
}

static int stop_port(volatile hba_port_t *port) {
    port->cmd &= ~AHCI_PORT_CMD_ST;
    port->cmd &= ~AHCI_PORT_CMD_FRE;
    return wait_clear(&port->cmd, AHCI_PORT_CMD_CR | AHCI_PORT_CMD_FR);
}

static void start_port(volatile hba_port_t *port) {
    port->cmd |= AHCI_PORT_CMD_FRE;
    port->cmd |= AHCI_PORT_CMD_ST;
}

static block_status_t issue_command(ahci_disk_t *disk, uint8_t command,
                                    uint64_t lba, int write, int has_data) {
    volatile hba_port_t *port = disk->port;
    if (!wait_clear(&port->tfd, AHCI_TFD_BSY | AHCI_TFD_DRQ)) return BLOCK_IO_ERROR;

    hba_cmd_header_t *header = &disk->command_list[0];
    zero_bytes(header, sizeof(*header));
    zero_bytes(disk->command_table, sizeof(*disk->command_table));

    header->flags = (uint16_t)(sizeof(fis_reg_h2d_t) / 4u);
    if (write) header->flags |= (uint16_t)(1u << 6);
    header->prdtl = has_data ? 1u : 0u;
    header->ctba = (uint32_t)disk->command_table_phys;
    header->ctbau = (uint32_t)(disk->command_table_phys >> 32);

    if (has_data) {
        disk->command_table->prdt[0].dba = (uint32_t)disk->dma_phys;
        disk->command_table->prdt[0].dbau = (uint32_t)(disk->dma_phys >> 32);
        disk->command_table->prdt[0].dbc_i = (AHCI_SECTOR_SIZE - 1u) | (1u << 31);
    }

    fis_reg_h2d_t *fis = (fis_reg_h2d_t *)disk->command_table->cfis;
    fis->fis_type = AHCI_FIS_REG_H2D;
    fis->pmport_c = 1u << 7;
    fis->command = command;
    if (command == ATA_CMD_READ_DMA_EXT || command == ATA_CMD_WRITE_DMA_EXT) {
        fis->device = 1u << 6;
        fis->lba0 = (uint8_t)lba;
        fis->lba1 = (uint8_t)(lba >> 8);
        fis->lba2 = (uint8_t)(lba >> 16);
        fis->lba3 = (uint8_t)(lba >> 24);
        fis->lba4 = (uint8_t)(lba >> 32);
        fis->lba5 = (uint8_t)(lba >> 40);
        fis->countl = 1;
        fis->counth = 0;
    }

    port->is = UINT32_MAX;
    __asm__ volatile ("" ::: "memory");
    port->ci |= 1u;

    for (uint32_t i = 0; i < AHCI_COMMAND_TIMEOUT; ++i) {
        if ((port->ci & 1u) == 0) {
            if (port->is & AHCI_PORT_IS_TFES) return BLOCK_IO_ERROR;
            return BLOCK_OK;
        }
        if (port->is & AHCI_PORT_IS_TFES) return BLOCK_IO_ERROR;
        __asm__ volatile ("pause");
    }
    return BLOCK_IO_ERROR;
}

static block_status_t disk_read(void *context, uint64_t lba, uint32_t count, void *buffer) {
    ahci_disk_t *disk = context;
    uint8_t *out = buffer;
    if (!disk || !buffer) return BLOCK_BAD_ARGUMENT;
    for (uint32_t sector = 0; sector < count; ++sector) {
        block_status_t status = issue_command(disk, ATA_CMD_READ_DMA_EXT, lba + sector, 0, 1);
        if (status != BLOCK_OK) return status;
        for (uint32_t i = 0; i < AHCI_SECTOR_SIZE; ++i) {
            out[(size_t)sector * AHCI_SECTOR_SIZE + i] = disk->dma[i];
        }
    }
    return BLOCK_OK;
}

static block_status_t disk_write(void *context, uint64_t lba, uint32_t count, const void *buffer) {
    ahci_disk_t *disk = context;
    const uint8_t *in = buffer;
    if (!disk || !buffer) return BLOCK_BAD_ARGUMENT;
    for (uint32_t sector = 0; sector < count; ++sector) {
        for (uint32_t i = 0; i < AHCI_SECTOR_SIZE; ++i) {
            disk->dma[i] = in[(size_t)sector * AHCI_SECTOR_SIZE + i];
        }
        __asm__ volatile ("" ::: "memory");
        block_status_t status = issue_command(disk, ATA_CMD_WRITE_DMA_EXT, lba + sector, 1, 1);
        if (status != BLOCK_OK) return status;
    }
    return BLOCK_OK;
}

static block_status_t disk_flush(void *context) {
    ahci_disk_t *disk = context;
    if (!disk) return BLOCK_BAD_ARGUMENT;
    return issue_command(disk, ATA_CMD_FLUSH_CACHE_EXT, 0, 0, 0);
}

static int allocate_dma_frame(const boot_context_t *boot, uint32_t cap,
                              uint64_t *phys_out, void **virt_out) {
    uint64_t physical = pmm_alloc_frame();
    if (physical == UINT64_MAX || !address_supported(cap, physical)) return 0;
    void *virtual_address = phys_pointer(boot, physical);
    if (!virtual_address) return 0;
    zero_bytes(virtual_address, PMM_PAGE_SIZE);
    *phys_out = physical;
    *virt_out = virtual_address;
    return 1;
}

static int identify_disk(ahci_disk_t *disk) {
    if (issue_command(disk, ATA_CMD_IDENTIFY, 0, 0, 1) != BLOCK_OK) return 0;
    const uint16_t *words = (const uint16_t *)disk->dma;
    uint64_t sectors;
    if (words[83] & (1u << 10)) {
        sectors = (uint64_t)words[100] |
                  ((uint64_t)words[101] << 16) |
                  ((uint64_t)words[102] << 32) |
                  ((uint64_t)words[103] << 48);
    } else {
        sectors = (uint64_t)words[60] | ((uint64_t)words[61] << 16);
    }
    if (sectors == 0) return 0;
    disk->sectors = sectors;
    return 1;
}

static int port_has_disk(volatile hba_port_t *port) {
    uint32_t ssts = port->ssts;
    if ((ssts & AHCI_SSTS_DET_MASK) != AHCI_SSTS_DET_PRESENT) return 0;
    if ((ssts & AHCI_SSTS_IPM_MASK) != AHCI_SSTS_IPM_ACTIVE) return 0;
    return port->sig == AHCI_SIG_ATA;
}

static int init_port(const boot_context_t *boot, uint32_t cap,
                     volatile hba_port_t *port, ahci_disk_t *disk, uint32_t index) {
    if (!stop_port(port)) return 0;
    zero_bytes(disk, sizeof(*disk));
    disk->boot = boot;
    disk->port = port;

    void *pointer;
    if (!allocate_dma_frame(boot, cap, &disk->command_list_phys, &pointer)) return 0;
    disk->command_list = pointer;
    if (!allocate_dma_frame(boot, cap, &disk->fis_phys, &pointer)) return 0;
    if (!allocate_dma_frame(boot, cap, &disk->command_table_phys, &pointer)) return 0;
    disk->command_table = pointer;
    if (!allocate_dma_frame(boot, cap, &disk->dma_phys, &pointer)) return 0;
    disk->dma = pointer;

    port->clb = (uint32_t)disk->command_list_phys;
    port->clbu = (uint32_t)(disk->command_list_phys >> 32);
    port->fb = (uint32_t)disk->fis_phys;
    port->fbu = (uint32_t)(disk->fis_phys >> 32);
    port->serr = UINT32_MAX;
    port->is = UINT32_MAX;
    start_port(port);

    if (!identify_disk(disk)) {
        stop_port(port);
        return 0;
    }

    zero_bytes(&disk->block, sizeof(disk->block));
    disk->block.name[0] = 's';
    disk->block.name[1] = 'a';
    disk->block.name[2] = 't';
    disk->block.name[3] = 'a';
    disk->block.name[4] = (char)('0' + index);
    disk->block.name[5] = '\0';
    disk->block.sector_size = AHCI_SECTOR_SIZE;
    disk->block.sector_count = disk->sectors;
    disk->block.writable = 1;
    disk->block.context = disk;
    disk->block.read = disk_read;
    disk->block.write = disk_write;
    disk->block.flush = disk_flush;
    return block_register(&disk->block, 0) == BLOCK_OK;
}

ahci_status_t ahci_probe(const boot_context_t *boot) {
    if (!boot) return AHCI_BAD_ARGUMENT;
    disk_count = 0;
    int controller_found = 0;

    for (uint32_t i = 0; i < pci_count(); ++i) {
        const pci_device_t *device = pci_get(i);
        if (!device || device->class_code != PCI_CLASS_MASS_STORAGE ||
            device->subclass != PCI_SUBCLASS_SATA || device->prog_if != PCI_PROGIF_AHCI) continue;
        controller_found = 1;

        pci_bar_t abar_bar;
        if (!pci_decode_bar(device, 5, &abar_bar) || abar_bar.io) continue;

        uint32_t command = pci_config_read32(device->bus, device->slot, device->function, 0x04);
        command |= (1u << 1) | (1u << 2);
        pci_config_write32(device->bus, device->slot, device->function, 0x04, command);

        void *mapped = 0;
        if (paging_map_mmio(abar_bar.base, sizeof(hba_mem_t), &mapped) != PAGING_OK || !mapped) {
            return AHCI_MMIO_ERROR;
        }
        volatile hba_mem_t *hba = mapped;
        hba->ghc |= AHCI_GHC_AE;
        uint32_t implemented = hba->pi;

        for (uint32_t port_index = 0; port_index < 32 && disk_count < AHCI_MAX_DISKS; ++port_index) {
            if (!(implemented & (1u << port_index))) continue;
            volatile hba_port_t *port = &hba->ports[port_index];
            if (!port_has_disk(port)) continue;
            if (init_port(boot, hba->cap, port, &disks[disk_count], disk_count)) disk_count++;
        }
    }

    if (!controller_found) return AHCI_NO_CONTROLLER;
    if (disk_count == 0) return AHCI_NO_DISK;
    return AHCI_OK;
}

uint32_t ahci_disk_count(void) {
    return disk_count;
}

const block_device_t *ahci_disk(uint32_t index) {
    return index < disk_count ? &disks[index].block : 0;
}

const char *ahci_status_string(ahci_status_t status) {
    switch (status) {
        case AHCI_OK: return "ok";
        case AHCI_BAD_ARGUMENT: return "bad argument";
        case AHCI_NO_CONTROLLER: return "no AHCI controller";
        case AHCI_NO_DISK: return "no SATA disk";
        case AHCI_NO_MEMORY: return "no DMA memory";
        case AHCI_MMIO_ERROR: return "AHCI MMIO mapping failed";
        case AHCI_TIMEOUT: return "AHCI timeout";
        case AHCI_IO_ERROR: return "AHCI I/O error";
        default: return "unknown AHCI error";
    }
}
