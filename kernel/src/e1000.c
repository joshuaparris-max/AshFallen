#include "e1000.h"
#include "net.h"
#include "paging.h"
#include "pci.h"
#include "pmm.h"
#include <stddef.h>
#include <stdint.h>

#define E1000_VENDOR_INTEL 0x8086u
#define E1000_DEVICE_82540EM 0x100eu

#define E1000_MMIO_SIZE 0x20000u

#define REG_CTRL   0x0000u
#define REG_STATUS 0x0008u
#define REG_ICR    0x00c0u
#define REG_IMC    0x00d8u
#define REG_RCTL   0x0100u
#define REG_TCTL   0x0400u
#define REG_TIPG   0x0410u
#define REG_RDBAL  0x2800u
#define REG_RDBAH  0x2804u
#define REG_RDLEN  0x2808u
#define REG_RDH    0x2810u
#define REG_RDT    0x2818u
#define REG_TDBAL  0x3800u
#define REG_TDBAH  0x3804u
#define REG_TDLEN  0x3808u
#define REG_TDH    0x3810u
#define REG_TDT    0x3818u
#define REG_MTA    0x5200u
#define REG_RAL0   0x5400u
#define REG_RAH0   0x5404u

#define CTRL_SLU (1u << 6)
#define CTRL_RST (1u << 26)

#define STATUS_LU (1u << 1)

#define RCTL_EN    (1u << 1)
#define RCTL_BAM   (1u << 15)
#define RCTL_SECRC (1u << 26)

#define TCTL_EN   (1u << 1)
#define TCTL_PSP  (1u << 3)
#define TCTL_RTLC (1u << 24)

#define TX_CMD_EOP  0x01u
#define TX_CMD_IFCS 0x02u
#define TX_CMD_RS   0x08u
#define TX_STATUS_DD 0x01u

#define RX_STATUS_DD  0x01u
#define RX_STATUS_EOP 0x02u

#define RX_COUNT 32u
#define TX_COUNT 16u
#define DMA_BUFFER_SIZE 2048u
#define RESET_SPINS 1000000u
#define TX_SPINS 1000000u

typedef struct __attribute__((packed)) {
    uint64_t address;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
} rx_descriptor_t;

typedef struct __attribute__((packed)) {
    uint64_t address;
    uint16_t length;
    uint8_t checksum_offset;
    uint8_t command;
    uint8_t status;
    uint8_t checksum_start;
    uint16_t special;
} tx_descriptor_t;

typedef struct {
    volatile uint32_t *mmio;
    volatile rx_descriptor_t *rx_ring;
    volatile tx_descriptor_t *tx_ring;
    uint64_t rx_ring_phys;
    uint64_t tx_ring_phys;
    uint64_t rx_buffer_phys[RX_COUNT];
    uint64_t tx_buffer_phys[TX_COUNT];
    uint8_t *rx_buffer[RX_COUNT];
    uint8_t *tx_buffer[TX_COUNT];
    uint8_t mac[6];
    uint32_t rx_next;
    uint32_t tx_next;
    uint32_t net_device_id;
    int ready;
} e1000_state_t;

static e1000_state_t state;

_Static_assert(sizeof(rx_descriptor_t) == 16, "e1000 RX descriptor size");
_Static_assert(sizeof(tx_descriptor_t) == 16, "e1000 TX descriptor size");

static void zero_bytes(void *memory, size_t length) {
    uint8_t *p = (uint8_t *)memory;
    while (length--) *p++ = 0;
}

static void copy_bytes(void *destination, const void *source, size_t length) {
    uint8_t *d = (uint8_t *)destination;
    const uint8_t *s = (const uint8_t *)source;
    while (length--) *d++ = *s++;
}

static uint32_t reg_read(uint32_t offset) {
    return state.mmio[offset / sizeof(uint32_t)];
}

static void reg_write(uint32_t offset, uint32_t value) {
    state.mmio[offset / sizeof(uint32_t)] = value;
    (void)state.mmio[REG_STATUS / sizeof(uint32_t)];
}

static uint64_t allocate_dma_page(void **virtual_out) {
    uint64_t physical = pmm_alloc_frame();
    if (physical == UINT64_MAX) return UINT64_MAX;
    void *pointer = paging_direct_pointer(physical);
    if (!pointer) {
        (void)pmm_free_frame(physical);
        return UINT64_MAX;
    }
    zero_bytes(pointer, PMM_PAGE_SIZE);
    *virtual_out = pointer;
    return physical;
}

static int mac_valid(const uint8_t mac[6]) {
    unsigned nonzero = 0;
    unsigned all_ff = 1;
    for (unsigned i = 0; i < 6; ++i) {
        nonzero |= mac[i];
        if (mac[i] != 0xffu) all_ff = 0;
    }
    return nonzero != 0 && !all_ff && (mac[0] & 1u) == 0;
}

static net_status_t e1000_transmit(void *context,
                                   const uint8_t *frame,
                                   size_t length) {
    (void)context;
    if (!state.ready || !frame || length == 0 || length > DMA_BUFFER_SIZE) {
        return NET_BAD_ARGUMENT;
    }

    uint32_t index = state.tx_next;
    volatile tx_descriptor_t *descriptor = &state.tx_ring[index];

    for (uint32_t spin = 0;
         spin < TX_SPINS && (descriptor->status & TX_STATUS_DD) == 0;
         ++spin) {
        __asm__ volatile ("pause");
    }
    if ((descriptor->status & TX_STATUS_DD) == 0) return NET_DRIVER_ERROR;

    copy_bytes(state.tx_buffer[index], frame, length);
    descriptor->length = (uint16_t)length;
    descriptor->checksum_offset = 0;
    descriptor->command = TX_CMD_EOP | TX_CMD_IFCS | TX_CMD_RS;
    descriptor->status = 0;
    descriptor->checksum_start = 0;
    descriptor->special = 0;

    __sync_synchronize();
    state.tx_next = (index + 1u) % TX_COUNT;
    reg_write(REG_TDT, state.tx_next);

    for (uint32_t spin = 0;
         spin < TX_SPINS && (descriptor->status & TX_STATUS_DD) == 0;
         ++spin) {
        __asm__ volatile ("pause");
    }
    return (descriptor->status & TX_STATUS_DD) ? NET_OK : NET_DRIVER_ERROR;
}

static e1000_status_t setup_rings(void) {
    void *ring_virtual = 0;
    state.rx_ring_phys = allocate_dma_page(&ring_virtual);
    if (state.rx_ring_phys == UINT64_MAX) return E1000_NO_MEMORY;
    state.rx_ring = (volatile rx_descriptor_t *)ring_virtual;

    ring_virtual = 0;
    state.tx_ring_phys = allocate_dma_page(&ring_virtual);
    if (state.tx_ring_phys == UINT64_MAX) return E1000_NO_MEMORY;
    state.tx_ring = (volatile tx_descriptor_t *)ring_virtual;

    for (uint32_t i = 0; i < RX_COUNT; ++i) {
        void *buffer = 0;
        state.rx_buffer_phys[i] = allocate_dma_page(&buffer);
        if (state.rx_buffer_phys[i] == UINT64_MAX) return E1000_NO_MEMORY;
        state.rx_buffer[i] = (uint8_t *)buffer;
        state.rx_ring[i].address = state.rx_buffer_phys[i];
        state.rx_ring[i].status = 0;
    }

    for (uint32_t i = 0; i < TX_COUNT; ++i) {
        void *buffer = 0;
        state.tx_buffer_phys[i] = allocate_dma_page(&buffer);
        if (state.tx_buffer_phys[i] == UINT64_MAX) return E1000_NO_MEMORY;
        state.tx_buffer[i] = (uint8_t *)buffer;
        state.tx_ring[i].address = state.tx_buffer_phys[i];
        state.tx_ring[i].status = TX_STATUS_DD;
    }

    reg_write(REG_RDBAL, (uint32_t)state.rx_ring_phys);
    reg_write(REG_RDBAH, (uint32_t)(state.rx_ring_phys >> 32));
    reg_write(REG_RDLEN, RX_COUNT * sizeof(rx_descriptor_t));
    reg_write(REG_RDH, 0);
    reg_write(REG_RDT, RX_COUNT - 1u);

    reg_write(REG_TDBAL, (uint32_t)state.tx_ring_phys);
    reg_write(REG_TDBAH, (uint32_t)(state.tx_ring_phys >> 32));
    reg_write(REG_TDLEN, TX_COUNT * sizeof(tx_descriptor_t));
    reg_write(REG_TDH, 0);
    reg_write(REG_TDT, 0);

    state.rx_next = 0;
    state.tx_next = 0;

    reg_write(REG_TIPG, 0x0060200au);
    reg_write(REG_TCTL,
              TCTL_EN | TCTL_PSP | TCTL_RTLC |
              (15u << 4) | (64u << 12));
    reg_write(REG_RCTL, RCTL_EN | RCTL_BAM | RCTL_SECRC);
    return E1000_OK;
}

e1000_status_t e1000_init(uint32_t *net_device_id_out) {
    if (!net_device_id_out) return E1000_PCI_ERROR;
    zero_bytes(&state, sizeof(state));

    const pci_device_t *device =
        pci_find_device(E1000_VENDOR_INTEL, E1000_DEVICE_82540EM);
    if (!device) return E1000_NOT_FOUND;

    if (pci_enable_memory_busmaster(device) != PCI_OK) return E1000_PCI_ERROR;

    pci_bar_t bar;
    if (pci_decode_bar(device, 0, &bar) != PCI_OK || bar.is_io || bar.base == 0) {
        return E1000_PCI_ERROR;
    }

    state.mmio = (volatile uint32_t *)paging_map_mmio(bar.base, E1000_MMIO_SIZE);
    if (!state.mmio) return E1000_MMIO_ERROR;

    reg_write(REG_IMC, 0xffffffffu);
    (void)reg_read(REG_ICR);

    reg_write(REG_CTRL, reg_read(REG_CTRL) | CTRL_RST);
    uint32_t spin;
    for (spin = 0; spin < RESET_SPINS; ++spin) {
        if ((reg_read(REG_CTRL) & CTRL_RST) == 0) break;
        __asm__ volatile ("pause");
    }
    if (spin == RESET_SPINS) return E1000_RESET_TIMEOUT;

    reg_write(REG_IMC, 0xffffffffu);
    (void)reg_read(REG_ICR);
    reg_write(REG_CTRL, reg_read(REG_CTRL) | CTRL_SLU);

    uint32_t ral = reg_read(REG_RAL0);
    uint32_t rah = reg_read(REG_RAH0);
    state.mac[0] = (uint8_t)ral;
    state.mac[1] = (uint8_t)(ral >> 8);
    state.mac[2] = (uint8_t)(ral >> 16);
    state.mac[3] = (uint8_t)(ral >> 24);
    state.mac[4] = (uint8_t)rah;
    state.mac[5] = (uint8_t)(rah >> 8);
    if (!mac_valid(state.mac)) return E1000_BAD_MAC;

    reg_write(REG_RAL0, ral);
    reg_write(REG_RAH0, (rah & 0xffffu) | (1u << 31));

    for (uint32_t i = 0; i < 128u; ++i) {
        reg_write(REG_MTA + i * 4u, 0);
    }

    e1000_status_t ring_status = setup_rings();
    if (ring_status != E1000_OK) return ring_status;

    net_device_config_t config;
    zero_bytes(&config, sizeof(config));
    const char name[] = "e1000";
    copy_bytes(config.name, name, sizeof(name));
    copy_bytes(config.mac, state.mac, sizeof(state.mac));
    config.mtu = 1500;
    config.context = &state;
    config.transmit = e1000_transmit;

    if (net_register_device(&config, &state.net_device_id) != NET_OK) {
        return E1000_NET_REGISTER_ERROR;
    }

    state.ready = 1;
    *net_device_id_out = state.net_device_id;
    return E1000_OK;
}

void e1000_poll(void) {
    if (!state.ready) return;

    for (;;) {
        volatile rx_descriptor_t *descriptor = &state.rx_ring[state.rx_next];
        if ((descriptor->status & RX_STATUS_DD) == 0) break;

        __sync_synchronize();
        uint16_t length = descriptor->length;
        uint8_t status = descriptor->status;
        uint8_t errors = descriptor->errors;

        if ((status & RX_STATUS_EOP) != 0 && errors == 0 &&
            length >= 14u && length <= DMA_BUFFER_SIZE) {
            (void)net_receive(
                state.net_device_id, state.rx_buffer[state.rx_next], length);
        }

        descriptor->status = 0;
        descriptor->errors = 0;
        __sync_synchronize();
        reg_write(REG_RDT, state.rx_next);
        state.rx_next = (state.rx_next + 1u) % RX_COUNT;
    }
}

int e1000_link_up(void) {
    return state.ready && (reg_read(REG_STATUS) & STATUS_LU) != 0;
}

const uint8_t *e1000_mac_address(void) {
    return state.ready ? state.mac : 0;
}

const char *e1000_status_string(e1000_status_t status) {
    switch (status) {
        case E1000_OK: return "ok";
        case E1000_NOT_FOUND: return "82540EM not found";
        case E1000_PCI_ERROR: return "PCI setup failed";
        case E1000_MMIO_ERROR: return "MMIO mapping failed";
        case E1000_RESET_TIMEOUT: return "reset timed out";
        case E1000_NO_MEMORY: return "DMA memory unavailable";
        case E1000_BAD_MAC: return "invalid MAC address";
        case E1000_NET_REGISTER_ERROR: return "network registration failed";
        case E1000_TX_TIMEOUT: return "transmit timed out";
        default: return "unknown e1000 error";
    }
}
