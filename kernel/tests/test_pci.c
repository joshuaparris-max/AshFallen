#include "pci.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;

static void expect(const char *name, int condition) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", name);
        failures++;
    }
}

int main(void) {
    pci_device_t device;
    memset(&device, 0, sizeof(device));
    pci_bar_t bar;

    device.bars[0] = UINT32_C(0xfebf1000);
    expect("32-bit memory BAR",
           pci_decode_bar(&device, 0, &bar) &&
           !bar.io && !bar.is_64 && bar.base == UINT64_C(0xfebf1000));

    device.bars[1] = UINT32_C(0x00004004);
    device.bars[2] = UINT32_C(0x00000001);
    expect("64-bit memory BAR",
           pci_decode_bar(&device, 1, &bar) &&
           !bar.io && bar.is_64 && bar.base == UINT64_C(0x0000000100004000));

    device.bars[3] = UINT32_C(0x0000c001);
    expect("I/O BAR",
           pci_decode_bar(&device, 3, &bar) &&
           bar.io && bar.base == UINT64_C(0x0000c000));

    device.bars[4] = UINT32_C(0x00001002);
    expect("reserved memory BAR type rejected", !pci_decode_bar(&device, 4, &bar));

    device.bars[5] = 0;
    expect("zero BAR rejected", !pci_decode_bar(&device, 5, &bar));

    if (failures) return 1;
    puts("PCI BAR decoding tests passed");
    return 0;
}
