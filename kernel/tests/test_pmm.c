#include "pmm.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void expect(const char *name, int ok) { if (!ok) { fprintf(stderr, "FAIL: %s\n", name); failures++; } }

int main(void) {
    boot_context_t boot;
    memset(&boot, 0, sizeof(boot));
    boot.memory_map_count = 4;
    boot.memory_map[0] = (boot_memory_region_t){0, 0x100000, BOOT_MEMORY_RESERVED, 0};
    boot.memory_map[1] = (boot_memory_region_t){0x100000, 63u * 1024u * 1024u, BOOT_MEMORY_USABLE, 0};
    boot.memory_map[2] = (boot_memory_region_t){0x500000, 0x100000, BOOT_MEMORY_RESERVED, 0};
    boot.memory_map[3] = (boot_memory_region_t){0x4000000, 0x100000, BOOT_MEMORY_RESERVED, 0};
    boot.physical_memory_limit = UINT64_MAX;
    boot.kernel_phys_start = 0x200000;
    boot.kernel_phys_end = 0x300000;

    expect("init", pmm_init(&boot) == PMM_OK);
    pmm_stats_t initial = pmm_stats();
    expect("kernel and overlapping reserved removed", initial.total_frames == 15616u);

    uint64_t first = pmm_alloc_frame();
    expect("alloc returns frame", first != UINT64_MAX);
    expect("kernel skipped", first < boot.kernel_phys_start || first >= boot.kernel_phys_end);
    expect("free succeeds", pmm_free_frame(first) == PMM_OK);
    expect("double free detected", pmm_free_frame(first) == PMM_DOUBLE_FREE);
    expect("misaligned free rejected", pmm_free_frame(first + 1) == PMM_INVALID_FREE);
    expect("outside free rejected", pmm_free_frame(0) == PMM_INVALID_FREE);

    expect("4096-frame stress", pmm_self_test(4096));
    pmm_stats_t after = pmm_stats();
    expect("free count restored", after.free_frames == initial.free_frames);
    expect("no leaked frames", after.allocated_frames == 0);

    if (failures) return 1;
    puts("physical memory allocator tests passed");
    return 0;
}
