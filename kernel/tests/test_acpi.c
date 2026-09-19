#include "acpi.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MEMORY_BYTES 65536u
#define RSDP_ADDR 0x1000u
#define XSDT_ADDR 0x2000u
#define MADT_ADDR 0x3000u
#define RSDT_ADDR 0x4000u

static unsigned char memory[MEMORY_BYTES];
static int failures;

static void expect(const char *name, int ok) {
    if (!ok) {
        fprintf(stderr, "FAIL: %s\n", name);
        failures++;
    }
}

static void put16(unsigned char *p, uint16_t value) {
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
}

static void put32(unsigned char *p, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) p[i] = (unsigned char)(value >> (i * 8u));
}

static void put64(unsigned char *p, uint64_t value) {
    put32(p, (uint32_t)value);
    put32(p + 4, (uint32_t)(value >> 32));
}

static unsigned char checksum(const unsigned char *p, size_t length) {
    unsigned char sum = 0;
    for (size_t i = 0; i < length; ++i) sum = (unsigned char)(sum + p[i]);
    return sum;
}

static void fix_checksum(unsigned char *p, size_t length, size_t field) {
    p[field] = 0;
    p[field] = (unsigned char)(0u - checksum(p, length));
}

static void make_sdt_header(
    unsigned char *p,
    const char signature[4],
    uint32_t length
) {
    memset(p, 0, length);
    memcpy(p, signature, 4);
    put32(p + 4, length);
    p[8] = 1;
    memcpy(p + 10, "JOSHOS", 6);
    memcpy(p + 16, "JOSHTEST", 8);
    put32(p + 24, 1);
    memcpy(p + 28, "JOSH", 4);
    put32(p + 32, 1);
}

static void build_madt(void) {
    unsigned char *madt = memory + MADT_ADDR;
    const uint32_t length = 94;
    make_sdt_header(madt, "APIC", length);
    put32(madt + 36, 0xfee00000u);
    put32(madt + 40, 1u);

    unsigned offset = 44;
    unsigned char *e = madt + offset;
    e[0] = 0; e[1] = 8; e[2] = 0; e[3] = 0; put32(e + 4, 1); offset += 8;
    e = madt + offset;
    e[0] = 0; e[1] = 8; e[2] = 1; e[3] = 1; put32(e + 4, 1); offset += 8;
    e = madt + offset;
    e[0] = 1; e[1] = 12; e[2] = 2; e[3] = 0;
    put32(e + 4, 0xfec00000u); put32(e + 8, 0); offset += 12;
    e = madt + offset;
    e[0] = 2; e[1] = 10; e[2] = 0; e[3] = 0;
    put32(e + 4, 2); put16(e + 8, 0x000fu); offset += 10;
    e = madt + offset;
    e[0] = 5; e[1] = 12; put16(e + 2, 0);
    put64(e + 4, UINT64_C(0xfee00000)); offset += 12;

    expect("MADT length fixture", offset == length);
    fix_checksum(madt, length, 9);
}

static void build_xsdt(void) {
    unsigned char *xsdt = memory + XSDT_ADDR;
    make_sdt_header(xsdt, "XSDT", 44);
    put64(xsdt + 36, MADT_ADDR);
    fix_checksum(xsdt, 44, 9);
}

static void build_rsdt(void) {
    unsigned char *rsdt = memory + RSDT_ADDR;
    make_sdt_header(rsdt, "RSDT", 40);
    put32(rsdt + 36, MADT_ADDR);
    fix_checksum(rsdt, 40, 9);
}

static void build_rsdp(unsigned revision) {
    unsigned char *rsdp = memory + RSDP_ADDR;
    memset(rsdp, 0, 36);
    memcpy(rsdp, "RSD PTR ", 8);
    memcpy(rsdp + 9, "JOSHOS", 6);
    rsdp[15] = (unsigned char)revision;
    put32(rsdp + 16, RSDT_ADDR);

    if (revision >= 2) {
        put32(rsdp + 20, 36);
        put64(rsdp + 24, XSDT_ADDR);
    }

    fix_checksum(rsdp, 20, 8);
    if (revision >= 2) fix_checksum(rsdp, 36, 32);
}

static void build_fixture(unsigned revision) {
    memset(memory, 0, sizeof(memory));
    build_madt();
    build_xsdt();
    build_rsdt();
    build_rsdp(revision);
}

static int reader(
    uint64_t physical,
    void *destination,
    size_t length,
    void *context
) {
    (void)context;
    if (physical > MEMORY_BYTES ||
        length > MEMORY_BYTES - (size_t)physical) {
        return 0;
    }
    memcpy(destination, memory + (size_t)physical, length);
    return 1;
}

static void test_valid_xsdt(void) {
    build_fixture(2);
    acpi_platform_info_t info;
    expect("XSDT discover",
           acpi_discover(RSDP_ADDR, reader, 0, &info) == ACPI_OK);
    expect("CPU count", info.cpu_count == 2);
    expect("CPU APIC ids",
           info.cpus[0].apic_id == 0 && info.cpus[1].apic_id == 1);
    expect("LAPIC address", info.local_apic_address == UINT64_C(0xfee00000));
    expect("IOAPIC count", info.ioapic_count == 1);
    expect("IOAPIC address",
           info.ioapics[0].physical_address == UINT64_C(0xfec00000));

    uint32_t gsi;
    uint16_t flags;
    expect("resolve overridden IRQ0",
           acpi_resolve_isa_irq(&info, 0, &gsi, &flags) &&
           gsi == 2 && flags == 0x000f);
    expect("resolve default IRQ1",
           acpi_resolve_isa_irq(&info, 1, &gsi, &flags) &&
           gsi == 1 && flags == 0);
}

static void test_rsdt_fallback(void) {
    build_fixture(1);
    acpi_platform_info_t info;
    expect("RSDT discover",
           acpi_discover(RSDP_ADDR, reader, 0, &info) == ACPI_OK);
    expect("RSDT CPUs", info.cpu_count == 2);
}

static void test_bad_checksum(void) {
    build_fixture(2);
    memory[RSDP_ADDR + 10] ^= 1u;
    acpi_platform_info_t info;
    expect("RSDP checksum rejected",
           acpi_discover(RSDP_ADDR, reader, 0, &info) ==
               ACPI_BAD_CHECKSUM);
}

static void test_bad_entry_length(void) {
    build_fixture(2);
    unsigned char *madt = memory + MADT_ADDR;
    madt[45] = 1;
    fix_checksum(madt, 94, 9);
    acpi_platform_info_t info;
    expect("short MADT entry rejected",
           acpi_discover(RSDP_ADDR, reader, 0, &info) ==
               ACPI_BAD_LENGTH);
}

int main(void) {
    test_valid_xsdt();
    test_rsdt_fallback();
    test_bad_checksum();
    test_bad_entry_length();
    for (int status = ACPI_OK; status <= ACPI_TOO_MANY_ENTRIES; ++status) {
        expect("ACPI status string", acpi_status_string((acpi_status_t)status) != NULL);
    }
    expect("unknown ACPI status string", acpi_status_string((acpi_status_t)999) != NULL);

    if (failures) return 1;
    puts("ACPI MADT parser tests passed");
    return 0;
}
