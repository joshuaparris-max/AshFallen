#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../src/pci.h"

static void test_config_address(void) {
    assert(pci_config_address(0, 0, 0, 0) == 0x80000000u);
    assert(pci_config_address(2, 3, 1, 0x14) ==
           (0x80000000u | (2u << 16) | (3u << 11) | (1u << 8) | 0x14u));
    assert((pci_config_address(255, 31, 7, 0xff) & 3u) == 0);
}

static void test_memory_bar(void) {
    pci_device_t device = {0};
    pci_bar_t bar;

    device.bars[0] = 0xfebc0000u;
    assert(pci_decode_bar(&device, 0, &bar) == PCI_OK);
    assert(!bar.is_io);
    assert(!bar.is_64bit);
    assert(bar.base == 0xfebc0000u);

    device.bars[1] = 0x0000c001u;
    assert(pci_decode_bar(&device, 1, &bar) == PCI_OK);
    assert(bar.is_io);
    assert(bar.base == 0x0000c000u);

    device.bars[2] = 0x34567004u;
    device.bars[3] = 0x00000012u;
    assert(pci_decode_bar(&device, 2, &bar) == PCI_OK);
    assert(bar.is_64bit);
    assert(bar.base == UINT64_C(0x0000001234567000));
}

static void test_invalid_bars(void) {
    pci_device_t device = {0};
    pci_bar_t bar;
    assert(pci_decode_bar(NULL, 0, &bar) == PCI_BAD_ARGUMENT);
    assert(pci_decode_bar(&device, 6, &bar) == PCI_BAD_ARGUMENT);
    assert(pci_decode_bar(&device, 0, &bar) == PCI_NOT_FOUND);
    device.bars[0] = 0xffffffffu;
    assert(pci_decode_bar(&device, 0, &bar) == PCI_NOT_FOUND);
    device.bars[5] = 0x4u;
    assert(pci_decode_bar(&device, 5, &bar) == PCI_UNSUPPORTED_BAR);
}

int main(void) {
    test_config_address();
    test_memory_bar();
    test_invalid_bars();
    puts("PCI helper tests passed");
    return 0;
}
