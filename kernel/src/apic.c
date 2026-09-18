#include "apic.h"
#include "io.h"
#include "paging.h"
#include "spinlock.h"
#include <stdint.h>

#define IA32_APIC_BASE_MSR UINT32_C(0x1b)
#define APIC_BASE_ENABLE (UINT64_C(1) << 11)
#define APIC_BASE_X2APIC (UINT64_C(1) << 10)
#define APIC_BASE_ADDRESS_MASK UINT64_C(0x000ffffffffff000)

#define LAPIC_ID 0x020u
#define LAPIC_TPR 0x080u
#define LAPIC_EOI 0x0b0u
#define LAPIC_SVR 0x0f0u
#define LAPIC_ICR_LOW 0x300u
#define LAPIC_ICR_HIGH 0x310u
#define LAPIC_LVT_TIMER 0x320u
#define LAPIC_LVT_LINT0 0x350u
#define LAPIC_LVT_LINT1 0x360u
#define LAPIC_LVT_ERROR 0x370u

#define LAPIC_SVR_ENABLE (1u << 8)
#define LAPIC_LVT_MASKED (1u << 16)
#define LAPIC_ICR_DELIVERY_PENDING (1u << 12)

#define IOAPIC_REG_ID 0x00u
#define IOAPIC_REG_VERSION 0x01u
#define IOAPIC_REG_REDIR_BASE 0x10u
#define IOAPIC_MAX_RUNTIME ACPI_MAX_IOAPICS
#define APIC_WAIT_SPINS 1000000u

typedef struct {
    volatile uint32_t *base;
    uint32_t gsi_base;
    uint32_t redirection_count;
    spinlock_t lock;
} ioapic_runtime_t;

static volatile uint32_t *lapic_base;
static uint64_t lapic_physical;
static uint32_t lapic_id;
static ioapic_runtime_t ioapics[IOAPIC_MAX_RUNTIME];
static uint32_t ioapic_count;
static acpi_platform_info_t topology;
static spinlock_t lapic_lock = SPINLOCK_INIT;
static int initialised;

static uint32_t lapic_read(uint32_t offset) {
    return lapic_base[offset / sizeof(uint32_t)];
}

static void lapic_write(uint32_t offset, uint32_t value) {
    lapic_base[offset / sizeof(uint32_t)] = value;
    (void)lapic_base[LAPIC_ID / sizeof(uint32_t)];
}

static uint32_t ioapic_read_unlocked(
    ioapic_runtime_t *ioapic,
    uint8_t reg
) {
    ioapic->base[0] = reg;
    return ioapic->base[4];
}

static void ioapic_write_unlocked(
    ioapic_runtime_t *ioapic,
    uint8_t reg,
    uint32_t value
) {
    ioapic->base[0] = reg;
    ioapic->base[4] = value;
}

static ioapic_runtime_t *ioapic_for_gsi(uint32_t gsi) {
    for (uint32_t i = 0; i < ioapic_count; ++i) {
        uint64_t end =
            (uint64_t)ioapics[i].gsi_base + ioapics[i].redirection_count;
        if (gsi >= ioapics[i].gsi_base && (uint64_t)gsi < end) {
            return &ioapics[i];
        }
    }
    return 0;
}

static void disable_legacy_pic(void) {
    outb(0x21, 0xff);
    outb(0xa1, 0xff);

    /*
     * IMCR is present on classic MP-compatible chipsets. Machines without it
     * ignore these ports; on machines with it this routes external interrupts
     * away from the legacy PIC and toward the APIC fabric.
     */
    outb(0x22, 0x70);
    outb(0x23, 0x01);
}

static int wait_icr_idle(void) {
    for (uint32_t i = 0; i < APIC_WAIT_SPINS; ++i) {
        if ((lapic_read(LAPIC_ICR_LOW) &
             LAPIC_ICR_DELIVERY_PENDING) == 0) {
            return 1;
        }
        __asm__ volatile ("pause");
    }
    return 0;
}

static apic_status_t route_gsi(
    uint32_t gsi,
    uint64_t redirection
) {
    ioapic_runtime_t *ioapic = ioapic_for_gsi(gsi);
    if (!ioapic) return APIC_ROUTE_NOT_FOUND;

    uint32_t pin = gsi - ioapic->gsi_base;
    if (pin >= ioapic->redirection_count || pin > 119u) {
        return APIC_ROUTE_NOT_FOUND;
    }

    uint8_t low_reg = (uint8_t)(IOAPIC_REG_REDIR_BASE + pin * 2u);
    uint8_t high_reg = (uint8_t)(low_reg + 1u);

    uint64_t flags = spin_lock_irqsave(&ioapic->lock);
    ioapic_write_unlocked(
        ioapic, low_reg,
        (uint32_t)redirection | (1u << 16));
    ioapic_write_unlocked(
        ioapic, high_reg, (uint32_t)(redirection >> 32));
    ioapic_write_unlocked(
        ioapic, low_reg, (uint32_t)redirection);
    spin_unlock_irqrestore(&ioapic->lock, flags);
    return APIC_OK;
}

apic_status_t apic_init(
    const acpi_platform_info_t *platform,
    const cpu_features_t *cpu
) {
    if (!platform || !cpu) return APIC_BAD_ARGUMENT;
    if (!cpu->has_msr || !cpu->has_apic ||
        platform->local_apic_address == 0 ||
        platform->ioapic_count == 0) {
        return APIC_UNAVAILABLE;
    }
    if (platform->ioapic_count > IOAPIC_MAX_RUNTIME ||
        (platform->local_apic_address & 0xfffu) != 0) {
        return APIC_BAD_ARGUMENT;
    }

    uint64_t apic_base_msr = cpu_read_msr(IA32_APIC_BASE_MSR);
    if ((apic_base_msr & APIC_BASE_X2APIC) != 0) {
        return APIC_UNSUPPORTED_MODE;
    }

    uint64_t wanted_base =
        platform->local_apic_address & APIC_BASE_ADDRESS_MASK;
    apic_base_msr &= ~APIC_BASE_ADDRESS_MASK;
    apic_base_msr |= wanted_base | APIC_BASE_ENABLE;
    cpu_write_msr(IA32_APIC_BASE_MSR, apic_base_msr);

    lapic_base = (volatile uint32_t *)paging_map_mmio(
        platform->local_apic_address, 4096);
    if (!lapic_base) return APIC_MAP_FAILED;

    lapic_physical = platform->local_apic_address;
    lapic_id = lapic_read(LAPIC_ID) >> 24;

    ioapic_count = 0;
    for (uint32_t i = 0; i < platform->ioapic_count; ++i) {
        if ((platform->ioapics[i].physical_address & 0xfffu) != 0) {
            return APIC_BAD_IOAPIC;
        }

        volatile uint32_t *mapped =
            (volatile uint32_t *)paging_map_mmio(
                platform->ioapics[i].physical_address, 4096);
        if (!mapped) return APIC_MAP_FAILED;

        ioapic_runtime_t *runtime = &ioapics[ioapic_count];
        runtime->base = mapped;
        runtime->gsi_base = platform->ioapics[i].gsi_base;
        runtime->lock.value = 0;

        uint32_t version =
            ioapic_read_unlocked(runtime, IOAPIC_REG_VERSION);
        runtime->redirection_count =
            ((version >> 16) & 0xffu) + 1u;
        if (runtime->redirection_count == 0 ||
            runtime->redirection_count > 120u) {
            return APIC_BAD_IOAPIC;
        }

        for (uint32_t pin = 0;
             pin < runtime->redirection_count;
             ++pin) {
            uint8_t low =
                (uint8_t)(IOAPIC_REG_REDIR_BASE + pin * 2u);
            ioapic_write_unlocked(runtime, (uint8_t)(low + 1u), 0);
            ioapic_write_unlocked(runtime, low, (1u << 16));
        }
        ioapic_count++;
    }

    topology = *platform;
    disable_legacy_pic();

    lapic_write(LAPIC_TPR, 0);
    lapic_write(LAPIC_LVT_TIMER,
                lapic_read(LAPIC_LVT_TIMER) | LAPIC_LVT_MASKED);
    lapic_write(LAPIC_LVT_LINT0,
                lapic_read(LAPIC_LVT_LINT0) | LAPIC_LVT_MASKED);
    lapic_write(LAPIC_LVT_LINT1,
                lapic_read(LAPIC_LVT_LINT1) | LAPIC_LVT_MASKED);
    lapic_write(LAPIC_LVT_ERROR,
                lapic_read(LAPIC_LVT_ERROR) | LAPIC_LVT_MASKED);
    lapic_write(LAPIC_SVR, LAPIC_SVR_ENABLE | 0xffu);

    initialised = 1;
    return APIC_OK;
}

apic_status_t apic_route_isa_irq(
    uint8_t irq,
    uint8_t vector,
    int masked
) {
    if (!initialised) return APIC_UNAVAILABLE;

    uint32_t gsi;
    uint16_t flags;
    if (!acpi_resolve_isa_irq(
            &topology, irq, &gsi, &flags)) {
        return APIC_ROUTE_NOT_FOUND;
    }

    uint64_t redirection;
    if (!apic_build_redirection(
            vector, lapic_id, flags, masked, &redirection)) {
        return APIC_BAD_IRQ_FLAGS;
    }
    return route_gsi(gsi, redirection);
}

apic_status_t apic_set_isa_irq_mask(uint8_t irq, int masked) {
    if (!initialised) return APIC_UNAVAILABLE;

    uint32_t gsi;
    uint16_t flags;
    (void)flags;
    if (!acpi_resolve_isa_irq(
            &topology, irq, &gsi, &flags)) {
        return APIC_ROUTE_NOT_FOUND;
    }

    ioapic_runtime_t *ioapic = ioapic_for_gsi(gsi);
    if (!ioapic) return APIC_ROUTE_NOT_FOUND;
    uint32_t pin = gsi - ioapic->gsi_base;
    uint8_t low_reg =
        (uint8_t)(IOAPIC_REG_REDIR_BASE + pin * 2u);

    uint64_t irq_flags = spin_lock_irqsave(&ioapic->lock);
    uint32_t low = ioapic_read_unlocked(ioapic, low_reg);
    if (masked) low |= 1u << 16;
    else low &= ~(1u << 16);
    ioapic_write_unlocked(ioapic, low_reg, low);
    spin_unlock_irqrestore(&ioapic->lock, irq_flags);
    return APIC_OK;
}

void apic_eoi(void) {
    if (initialised && lapic_base) lapic_write(LAPIC_EOI, 0);
}

uint32_t apic_local_id(void) {
    return initialised ? lapic_id : UINT32_MAX;
}

apic_summary_t apic_summary(void) {
    apic_summary_t summary = {
        .local_apic_physical = lapic_physical,
        .local_apic_id = initialised ? lapic_id : UINT32_MAX,
        .ioapic_count = ioapic_count
    };
    return summary;
}

apic_status_t apic_send_ipi(
    uint32_t destination_apic_id,
    uint8_t vector
) {
    if (!initialised) return APIC_UNAVAILABLE;
    if (destination_apic_id > 0xffu ||
        vector < 32 || vector == 0xff) {
        return APIC_BAD_ARGUMENT;
    }

    uint64_t flags = spin_lock_irqsave(&lapic_lock);
    if (!wait_icr_idle()) {
        spin_unlock_irqrestore(&lapic_lock, flags);
        return APIC_DELIVERY_TIMEOUT;
    }

    lapic_write(LAPIC_ICR_HIGH, destination_apic_id << 24);
    lapic_write(LAPIC_ICR_LOW, vector);

    int complete = wait_icr_idle();
    spin_unlock_irqrestore(&lapic_lock, flags);
    return complete ? APIC_OK : APIC_DELIVERY_TIMEOUT;
}

const char *apic_status_string(apic_status_t status) {
    switch (status) {
        case APIC_OK: return "ok";
        case APIC_BAD_ARGUMENT: return "bad argument";
        case APIC_UNAVAILABLE: return "APIC unavailable";
        case APIC_UNSUPPORTED_MODE: return "unsupported APIC mode";
        case APIC_MAP_FAILED: return "APIC MMIO mapping failed";
        case APIC_BAD_IOAPIC: return "invalid IOAPIC";
        case APIC_BAD_IRQ_FLAGS: return "invalid ACPI IRQ flags";
        case APIC_ROUTE_NOT_FOUND: return "no IOAPIC route for GSI";
        case APIC_DELIVERY_TIMEOUT: return "APIC IPI delivery timeout";
        default: return "unknown APIC error";
    }
}
