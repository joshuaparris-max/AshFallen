#include "heap.h"
#include "paging.h"
#include "pmm.h"
#include <stddef.h>
#include <stdint.h>

#define HEAP_BASE UINT64_C(0xffffc10000000000)
#define HEAP_MAX_BYTES UINT64_C(0x04000000)
#define HEAP_INITIAL_PAGES 16u
#define HEAP_GROW_PAGES 16u
#define HEAP_ALIGNMENT 16u
#define HEAP_MAGIC UINT32_C(0x4a484541)
#define HEAP_FLAG_FREE UINT32_C(1)
#define HEAP_SELF_TEST_ALLOCS 512u

typedef struct heap_block {
    uint64_t size;
    struct heap_block *prev;
    struct heap_block *next;
    uint32_t magic;
    uint32_t flags;
    uint64_t reserved0;
    uint64_t reserved1;
} heap_block_t;

_Static_assert(sizeof(heap_block_t) == 48, "heap header must stay 16-byte aligned");

static heap_block_t *head;
static heap_block_t *tail;
static uint64_t heap_end;
static uint64_t mapped_bytes;
static uint64_t allocated_bytes;
static uint64_t allocation_count;
static int initialised;

static uint64_t align16(uint64_t value) {
    return (value + (HEAP_ALIGNMENT - 1u)) & ~(uint64_t)(HEAP_ALIGNMENT - 1u);
}

static uint64_t pages_for(uint64_t bytes) {
    return (bytes + PMM_PAGE_SIZE - 1u) / PMM_PAGE_SIZE;
}

static void zero_bytes(void *pointer, uint64_t length) {
    uint8_t *p = (uint8_t *)pointer;
    while (length--) *p++ = 0;
}

static heap_status_t map_pages(uint64_t start, uint64_t pages) {
    if (pages == 0) return HEAP_BAD_ARGUMENT;
    if (pages > HEAP_MAX_BYTES / PMM_PAGE_SIZE) return HEAP_OUT_OF_MEMORY;

    for (uint64_t i = 0; i < pages; ++i) {
        uint64_t virtual_address = start + i * PMM_PAGE_SIZE;
        uint64_t physical = pmm_alloc_frame();
        if (physical == UINT64_MAX) return HEAP_OUT_OF_MEMORY;

        if (paging_map_page(virtual_address, physical) != PAGING_OK) {
            (void)pmm_free_frame(physical);
            return HEAP_MAP_FAILED;
        }
        zero_bytes((void *)(uintptr_t)virtual_address, PMM_PAGE_SIZE);
    }
    return HEAP_OK;
}

static heap_status_t grow(uint64_t minimum_payload) {
    uint64_t need = align16(minimum_payload) + sizeof(heap_block_t);
    uint64_t pages = pages_for(need);
    if (pages < HEAP_GROW_PAGES) pages = HEAP_GROW_PAGES;

    uint64_t bytes = pages * PMM_PAGE_SIZE;
    if (mapped_bytes > HEAP_MAX_BYTES || bytes > HEAP_MAX_BYTES - mapped_bytes) {
        return HEAP_OUT_OF_MEMORY;
    }
    if (heap_end > UINT64_MAX - bytes) return HEAP_OUT_OF_MEMORY;

    uint64_t old_end = heap_end;
    heap_status_t status = map_pages(old_end, pages);
    if (status != HEAP_OK) return status;

    heap_end += bytes;
    mapped_bytes += bytes;

    if (!head) {
        head = (heap_block_t *)(uintptr_t)old_end;
        head->size = bytes - sizeof(heap_block_t);
        head->prev = 0;
        head->next = 0;
        head->magic = HEAP_MAGIC;
        head->flags = HEAP_FLAG_FREE;
        head->reserved0 = 0;
        head->reserved1 = 0;
        tail = head;
        return HEAP_OK;
    }

    if ((tail->flags & HEAP_FLAG_FREE) != 0) {
        tail->size += bytes;
        return HEAP_OK;
    }

    heap_block_t *block = (heap_block_t *)(uintptr_t)old_end;
    block->size = bytes - sizeof(heap_block_t);
    block->prev = tail;
    block->next = 0;
    block->magic = HEAP_MAGIC;
    block->flags = HEAP_FLAG_FREE;
    block->reserved0 = 0;
    block->reserved1 = 0;
    tail->next = block;
    tail = block;
    return HEAP_OK;
}

static heap_block_t *find_free(uint64_t size) {
    for (heap_block_t *block = head; block; block = block->next) {
        if (block->magic != HEAP_MAGIC) return 0;
        if ((block->flags & HEAP_FLAG_FREE) != 0 && block->size >= size) {
            return block;
        }
    }
    return 0;
}

static void split_block(heap_block_t *block, uint64_t size) {
    if (!block) return;
    if (block->size < size + sizeof(heap_block_t) + HEAP_ALIGNMENT) return;

    uint8_t *payload = (uint8_t *)(block + 1);
    heap_block_t *remainder =
        (heap_block_t *)(void *)(payload + size);

    remainder->size = block->size - size - sizeof(heap_block_t);
    remainder->prev = block;
    remainder->next = block->next;
    remainder->magic = HEAP_MAGIC;
    remainder->flags = HEAP_FLAG_FREE;
    remainder->reserved0 = 0;
    remainder->reserved1 = 0;

    if (block->next) block->next->prev = remainder;
    else tail = remainder;

    block->next = remainder;
    block->size = size;
}

static int adjacent(const heap_block_t *left, const heap_block_t *right) {
    if (!left || !right) return 0;
    const uint8_t *expected =
        (const uint8_t *)(left + 1) + left->size;
    return expected == (const uint8_t *)right;
}

static heap_status_t coalesce(heap_block_t *block) {
    if (!block || block->magic != HEAP_MAGIC) return HEAP_CORRUPT;

    if (block->next &&
        block->next->magic == HEAP_MAGIC &&
        (block->next->flags & HEAP_FLAG_FREE) != 0 &&
        adjacent(block, block->next)) {
        heap_block_t *next = block->next;
        block->size += sizeof(heap_block_t) + next->size;
        block->next = next->next;
        if (next->next) next->next->prev = block;
        else tail = block;
        next->magic = 0;
    }

    if (block->prev &&
        block->prev->magic == HEAP_MAGIC &&
        (block->prev->flags & HEAP_FLAG_FREE) != 0 &&
        adjacent(block->prev, block)) {
        heap_block_t *prev = block->prev;
        prev->size += sizeof(heap_block_t) + block->size;
        prev->next = block->next;
        if (block->next) block->next->prev = prev;
        else tail = prev;
        block->magic = 0;
    }

    return HEAP_OK;
}

heap_status_t heap_init(void) {
    head = 0;
    tail = 0;
    heap_end = HEAP_BASE;
    mapped_bytes = 0;
    allocated_bytes = 0;
    allocation_count = 0;
    initialised = 0;

    heap_status_t status =
        grow((uint64_t)HEAP_INITIAL_PAGES * PMM_PAGE_SIZE -
             sizeof(heap_block_t));
    if (status != HEAP_OK) return status;

    initialised = 1;
    return HEAP_OK;
}

void *kmalloc(size_t requested) {
    if (!initialised || requested == 0) return 0;
    uint64_t size = align16((uint64_t)requested);
    if (size < requested) return 0;

    heap_block_t *block = find_free(size);
    if (!block) {
        if (grow(size) != HEAP_OK) return 0;
        block = find_free(size);
        if (!block) return 0;
    }

    split_block(block, size);
    block->flags &= ~HEAP_FLAG_FREE;
    allocated_bytes += block->size;
    allocation_count++;
    return (void *)(block + 1);
}

void *kcalloc(size_t count, size_t size) {
    if (count == 0 || size == 0 || count > SIZE_MAX / size) return 0;
    size_t total = count * size;
    void *memory = kmalloc(total);
    if (!memory) return 0;
    zero_bytes(memory, total);
    return memory;
}

heap_status_t kfree(void *pointer) {
    if (!initialised) return HEAP_NOT_INITIALISED;
    if (!pointer) return HEAP_BAD_ARGUMENT;

    uint64_t address = (uint64_t)(uintptr_t)pointer;
    if (address < HEAP_BASE + sizeof(heap_block_t) || address >= heap_end) {
        return HEAP_INVALID_FREE;
    }

    heap_block_t *block = ((heap_block_t *)pointer) - 1;
    if (block->magic != HEAP_MAGIC) return HEAP_INVALID_FREE;
    if ((block->flags & HEAP_FLAG_FREE) != 0) return HEAP_DOUBLE_FREE;

    block->flags |= HEAP_FLAG_FREE;
    if (allocated_bytes < block->size || allocation_count == 0) {
        return HEAP_CORRUPT;
    }
    allocated_bytes -= block->size;
    allocation_count--;
    return coalesce(block);
}

heap_stats_t heap_stats(void) {
    heap_stats_t stats = {
        .mapped_bytes = mapped_bytes,
        .allocated_bytes = allocated_bytes,
        .free_bytes = 0,
        .allocation_count = allocation_count
    };

    for (heap_block_t *block = head; block; block = block->next) {
        if (block->magic != HEAP_MAGIC) break;
        if ((block->flags & HEAP_FLAG_FREE) != 0) {
            stats.free_bytes += block->size;
        }
    }
    return stats;
}

int heap_self_test(void) {
    static void *pointers[HEAP_SELF_TEST_ALLOCS];

    for (uint32_t i = 0; i < HEAP_SELF_TEST_ALLOCS; ++i) {
        size_t size = (size_t)((i * 37u) % 1536u) + 1u;
        pointers[i] = kmalloc(size);
        if (!pointers[i]) return 0;

        uint8_t *bytes = (uint8_t *)pointers[i];
        bytes[0] = (uint8_t)i;
        bytes[size - 1u] = (uint8_t)(i ^ 0x5au);
    }

    for (uint32_t i = 0; i < HEAP_SELF_TEST_ALLOCS; i += 2) {
        if (kfree(pointers[i]) != HEAP_OK) return 0;
        pointers[i] = 0;
    }

    void *zeroed = kcalloc(257u, 3u);
    if (!zeroed) return 0;
    uint8_t *z = (uint8_t *)zeroed;
    for (uint32_t i = 0; i < 771u; ++i) {
        if (z[i] != 0) return 0;
    }
    if (kfree(zeroed) != HEAP_OK) return 0;

    for (uint32_t i = 1; i < HEAP_SELF_TEST_ALLOCS; i += 2) {
        size_t size = (size_t)((i * 37u) % 1536u) + 1u;
        uint8_t *bytes = (uint8_t *)pointers[i];
        if (bytes[0] != (uint8_t)i ||
            bytes[size - 1u] != (uint8_t)(i ^ 0x5au)) {
            return 0;
        }
        if (kfree(pointers[i]) != HEAP_OK) return 0;
        pointers[i] = 0;
    }

    heap_stats_t stats = heap_stats();
    return stats.allocated_bytes == 0 && stats.allocation_count == 0;
}

const char *heap_status_string(heap_status_t status) {
    switch (status) {
        case HEAP_OK: return "ok";
        case HEAP_NOT_INITIALISED: return "not initialised";
        case HEAP_BAD_ARGUMENT: return "bad argument";
        case HEAP_OUT_OF_MEMORY: return "out of memory";
        case HEAP_MAP_FAILED: return "page mapping failed";
        case HEAP_INVALID_FREE: return "invalid free";
        case HEAP_DOUBLE_FREE: return "double free";
        case HEAP_CORRUPT: return "heap corrupt";
        default: return "unknown heap error";
    }
}
