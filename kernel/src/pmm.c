#include "pmm.h"
#include <stdint.h>

#define PMM_MIN_PHYSICAL UINT64_C(0x00100000)
#define PMM_MAX_RANGES 4096u
#define PMM_SELF_TEST_MAX 4096u
#define PMM_INVALID_FRAME UINT64_MAX

typedef struct {
    uint64_t start;
    uint64_t end;
} pmm_range_t;

static pmm_range_t free_ranges[PMM_MAX_RANGES];
static pmm_range_t managed_ranges[PMM_MAX_RANGES];
static uint32_t free_range_count;
static uint32_t managed_range_count;
static uint64_t total_frames;
static uint64_t free_frames;
static int initialised;

static uint64_t align_down(uint64_t value) {
    return value & ~(PMM_PAGE_SIZE - 1u);
}

static int align_up(uint64_t value, uint64_t *out) {
    if (!out) return 0;
    uint64_t mask = PMM_PAGE_SIZE - 1u;
    if ((value & mask) == 0) {
        *out = value;
        return 1;
    }
    uint64_t add = PMM_PAGE_SIZE - (value & mask);
    if (UINT64_MAX - value < add) return 0;
    *out = value + add;
    return 1;
}

static void erase_range(uint32_t index) {
    for (uint32_t i = index + 1; i < free_range_count; ++i) {
        free_ranges[i - 1] = free_ranges[i];
    }
    free_range_count--;
}

static pmm_status_t insert_free_range(uint64_t start, uint64_t end) {
    if (start >= end) return PMM_OK;
    uint32_t pos = 0;
    while (pos < free_range_count && free_ranges[pos].start < start) pos++;
    if (free_range_count >= PMM_MAX_RANGES) return PMM_RANGE_CAPACITY;

    for (uint32_t i = free_range_count; i > pos; --i) free_ranges[i] = free_ranges[i - 1];
    free_ranges[pos] = (pmm_range_t){start, end};
    free_range_count++;

    if (pos > 0 && free_ranges[pos - 1].end >= free_ranges[pos].start) {
        if (free_ranges[pos].end > free_ranges[pos - 1].end) free_ranges[pos - 1].end = free_ranges[pos].end;
        erase_range(pos);
        pos--;
    }
    while (pos + 1 < free_range_count && free_ranges[pos].end >= free_ranges[pos + 1].start) {
        if (free_ranges[pos + 1].end > free_ranges[pos].end) free_ranges[pos].end = free_ranges[pos + 1].end;
        erase_range(pos + 1);
    }
    return PMM_OK;
}

static pmm_status_t remove_interval(uint64_t start, uint64_t end) {
    if (start >= end) return PMM_OK;
    for (uint32_t i = 0; i < free_range_count;) {
        pmm_range_t r = free_ranges[i];
        if (end <= r.start || start >= r.end) { i++; continue; }

        if (start <= r.start && end >= r.end) { erase_range(i); continue; }
        if (start <= r.start) { free_ranges[i].start = end < r.end ? end : r.end; i++; continue; }
        if (end >= r.end) { free_ranges[i].end = start; i++; continue; }

        if (free_range_count >= PMM_MAX_RANGES) return PMM_RANGE_CAPACITY;
        for (uint32_t j = free_range_count; j > i + 1; --j) free_ranges[j] = free_ranges[j - 1];
        free_ranges[i].end = start;
        free_ranges[i + 1] = (pmm_range_t){end, r.end};
        free_range_count++;
        i += 2;
    }
    return PMM_OK;
}

static int contains_frame(const pmm_range_t *r, uint64_t frame) {
    return frame >= r->start && UINT64_MAX - frame >= PMM_PAGE_SIZE &&
           frame + PMM_PAGE_SIZE <= r->end;
}

static int frame_is_managed(uint64_t frame) {
    for (uint32_t i = 0; i < managed_range_count; ++i) if (contains_frame(&managed_ranges[i], frame)) return 1;
    return 0;
}

static int frame_is_free(uint64_t frame) {
    for (uint32_t i = 0; i < free_range_count; ++i) if (contains_frame(&free_ranges[i], frame)) return 1;
    return 0;
}

pmm_status_t pmm_init(const boot_context_t *boot) {
    initialised = 0;
    free_range_count = managed_range_count = 0;
    total_frames = free_frames = 0;

    if (!boot || boot->memory_map_count == 0 ||
        boot->memory_map_count > BOOT_MEMORY_MAX_ENTRIES ||
        boot->physical_memory_limit <= PMM_MIN_PHYSICAL) return PMM_BAD_ARGUMENT;

    for (uint32_t i = 0; i < boot->memory_map_count; ++i) {
        const boot_memory_region_t *region = &boot->memory_map[i];
        if (region->type != BOOT_MEMORY_USABLE || region->length == 0) continue;
        if (UINT64_MAX - region->base < region->length) return PMM_BAD_ARGUMENT;

        uint64_t start = region->base < PMM_MIN_PHYSICAL ? PMM_MIN_PHYSICAL : region->base;
        uint64_t end = region->base + region->length;
        if (end > boot->physical_memory_limit) end = boot->physical_memory_limit;
        if (!align_up(start, &start)) continue;
        end = align_down(end);
        if (start >= end) continue;

        pmm_status_t status = insert_free_range(start, end);
        if (status != PMM_OK) return status;
    }

    if (free_range_count == 0) return PMM_NO_MEMORY;

    /* Reserved/ACPI/framebuffer entries win over overlapping usable entries. */
    for (uint32_t i = 0; i < boot->memory_map_count; ++i) {
        const boot_memory_region_t *region = &boot->memory_map[i];
        if (region->type == BOOT_MEMORY_USABLE || region->length == 0) continue;
        if (UINT64_MAX - region->base < region->length) return PMM_BAD_ARGUMENT;

        uint64_t start = align_down(region->base);
        uint64_t end;
        if (!align_up(region->base + region->length, &end)) end = align_down(UINT64_MAX);
        pmm_status_t status = remove_interval(start, end);
        if (status != PMM_OK) return status;
    }

    if (boot->kernel_phys_start < boot->kernel_phys_end) {
        uint64_t start = align_down(boot->kernel_phys_start);
        uint64_t end;
        if (!align_up(boot->kernel_phys_end, &end)) return PMM_BAD_ARGUMENT;
        pmm_status_t status = remove_interval(start, end);
        if (status != PMM_OK) return status;
    }

    if (boot->framebuffer_phys_start < boot->framebuffer_phys_end) {
        uint64_t start = align_down(boot->framebuffer_phys_start);
        uint64_t end;
        if (!align_up(boot->framebuffer_phys_end, &end)) return PMM_BAD_ARGUMENT;
        pmm_status_t status = remove_interval(start, end);
        if (status != PMM_OK) return status;
    }

    if (free_range_count == 0) return PMM_NO_MEMORY;

    managed_range_count = free_range_count;
    for (uint32_t i = 0; i < free_range_count; ++i) {
        managed_ranges[i] = free_ranges[i];
        total_frames += (free_ranges[i].end - free_ranges[i].start) / PMM_PAGE_SIZE;
    }
    free_frames = total_frames;
    initialised = 1;
    return PMM_OK;
}

uint64_t pmm_alloc_frames(uint32_t frame_count) {
    if (!initialised || frame_count == 0 ||
        frame_count > free_frames ||
        (uint64_t)frame_count > UINT64_MAX / PMM_PAGE_SIZE) {
        return PMM_INVALID_FRAME;
    }

    uint64_t bytes = (uint64_t)frame_count * PMM_PAGE_SIZE;
    for (uint32_t i = 0; i < free_range_count; ++i) {
        uint64_t available = free_ranges[i].end - free_ranges[i].start;
        if (available < bytes) continue;

        uint64_t frame = free_ranges[i].start;
        free_ranges[i].start += bytes;
        if (free_ranges[i].start == free_ranges[i].end) erase_range(i);
        free_frames -= frame_count;
        return frame;
    }
    return PMM_INVALID_FRAME;
}

uint64_t pmm_alloc_frame(void) {
    return pmm_alloc_frames(1);
}

pmm_status_t pmm_free_frames(uint64_t frame, uint32_t frame_count) {
    if (!initialised || frame_count == 0 ||
        (frame & (PMM_PAGE_SIZE - 1u)) != 0 ||
        (uint64_t)frame_count > UINT64_MAX / PMM_PAGE_SIZE) {
        return PMM_INVALID_FREE;
    }

    uint64_t bytes = (uint64_t)frame_count * PMM_PAGE_SIZE;
    if (UINT64_MAX - frame < bytes) return PMM_INVALID_FREE;
    uint64_t end = frame + bytes;

    for (uint64_t current = frame; current < end; current += PMM_PAGE_SIZE) {
        if (!frame_is_managed(current)) return PMM_INVALID_FREE;
        if (frame_is_free(current)) return PMM_DOUBLE_FREE;
    }

    pmm_status_t status = insert_free_range(frame, end);
    if (status != PMM_OK) return status;
    free_frames += frame_count;
    return PMM_OK;
}

pmm_status_t pmm_free_frame(uint64_t frame) {
    return pmm_free_frames(frame, 1);
}

pmm_stats_t pmm_stats(void) {
    return (pmm_stats_t){
        .total_frames = total_frames,
        .free_frames = free_frames,
        .allocated_frames = total_frames - free_frames,
        .free_range_count = free_range_count
    };
}

int pmm_self_test(uint32_t count) {
    static uint64_t frames[PMM_SELF_TEST_MAX];
    if (!initialised || count == 0 || count > PMM_SELF_TEST_MAX || free_frames < count) return 0;

    uint64_t before = free_frames;
    for (uint32_t i = 0; i < count; ++i) {
        frames[i] = pmm_alloc_frame();
        if (frames[i] == PMM_INVALID_FRAME) return 0;
    }
    for (uint32_t i = 0; i < count; i += 2) if (pmm_free_frame(frames[i]) != PMM_OK) return 0;
    for (uint32_t i = 1; i < count; i += 2) if (pmm_free_frame(frames[i]) != PMM_OK) return 0;
    return free_frames == before;
}

const char *pmm_status_string(pmm_status_t status) {
    switch (status) {
        case PMM_OK: return "ok";
        case PMM_BAD_ARGUMENT: return "bad argument";
        case PMM_NO_MEMORY: return "no usable memory";
        case PMM_RANGE_CAPACITY: return "range capacity exhausted";
        case PMM_INVALID_FREE: return "invalid free";
        case PMM_DOUBLE_FREE: return "double free";
        default: return "unknown PMM error";
    }
}
