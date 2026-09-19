#include "pmm.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void expect(const char *name, int ok) { if (!ok) { fprintf(stderr, "FAIL: %s\n", name); failures++; } }

int main(void) {
    boot_context_t boot;
    memset(&boot, 0, sizeof(boot));
    boot.memory_map_count = 5;
    boot.memory_map[0] = (boot_memory_region_t){0, 0x100000, BOOT_MEMORY_RESERVED, 0};
    boot.memory_map[1] = (boot_memory_region_t){0x100000, 63u * 1024u * 1024u, BOOT_MEMORY_USABLE, 0};
    boot.memory_map[2] = (boot_memory_region_t){0x500000, 0x100000, BOOT_MEMORY_ACPI_RECLAIMABLE, 0};
    boot.memory_map[3] = (boot_memory_region_t){0x600000, 0x100000, BOOT_MEMORY_RESERVED, 0};
    boot.memory_map[4] = (boot_memory_region_t){0x700000, 0x100000, BOOT_MEMORY_ACPI_NVS, 0};
    boot.physical_memory_limit = UINT64_MAX;
    boot.kernel_phys_start = 0x200000;
    boot.kernel_phys_end = 0x300000;
    boot.framebuffer_phys_start = 0x400000;
    boot.framebuffer_phys_end = 0x410000;

    expect("init", pmm_init(&boot) == PMM_OK);
    pmm_stats_t initial = pmm_stats();
    expect("kernel framebuffer boot and ACPI ranges removed",
           initial.total_frames == 15088u);

    uint64_t frames[1024];
    for (unsigned i = 0; i < 1024; ++i) {
        frames[i] = pmm_alloc_frame();
        expect("alloc returns frame", frames[i] != UINT64_MAX);
        expect("kernel skipped",
               frames[i] < boot.kernel_phys_start || frames[i] >= boot.kernel_phys_end);
        expect("framebuffer skipped",
               frames[i] < boot.framebuffer_phys_start || frames[i] >= boot.framebuffer_phys_end);
        expect("ACPI reclaimable skipped",
               frames[i] < 0x500000 || frames[i] >= 0x600000);
        expect("boot-reserved skipped",
               frames[i] < 0x600000 || frames[i] >= 0x700000);
        expect("ACPI NVS skipped",
               frames[i] < 0x700000 || frames[i] >= 0x800000);
    }
    for (unsigned i = 0; i < 1024; ++i) {
        expect("free succeeds", pmm_free_frame(frames[i]) == PMM_OK);
    }

    uint64_t first = pmm_alloc_frame();
    expect("double-free setup allocation", first != UINT64_MAX);
    expect("free succeeds", pmm_free_frame(first) == PMM_OK);
    expect("double free detected", pmm_free_frame(first) == PMM_DOUBLE_FREE);
    expect("misaligned free rejected", pmm_free_frame(first + 1) == PMM_INVALID_FREE);
    expect("outside free rejected", pmm_free_frame(0) == PMM_INVALID_FREE);

    uint64_t before_run = pmm_stats().free_frames;
    uint64_t run = pmm_alloc_frames(16);
    expect("contiguous run allocated", run != UINT64_MAX);
    expect("contiguous run aligned", (run & (PMM_PAGE_SIZE - 1u)) == 0);
    expect("contiguous run accounting",
           pmm_stats().free_frames == before_run - 16u);
    expect("contiguous run free", pmm_free_frames(run, 16) == PMM_OK);
    expect("contiguous run restored",
           pmm_stats().free_frames == before_run);
    expect("contiguous run double free",
           pmm_free_frames(run, 16) == PMM_DOUBLE_FREE);

    expect("4096-frame stress", pmm_self_test(4096));
    pmm_stats_t after = pmm_stats();
    expect("free count restored", after.free_frames == initial.free_frames);
    expect("no leaked frames", after.allocated_frames == 0);
    for (int status = PMM_OK; status <= PMM_DOUBLE_FREE; ++status) {
        expect("PMM status string", pmm_status_string((pmm_status_t)status) != NULL);
    }
    expect("unknown PMM status string", pmm_status_string((pmm_status_t)999) != NULL);

    if (failures) return 1;
    puts("physical memory allocator tests passed");
    return 0;
}
