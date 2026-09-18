#include "heap.h"
#include <stdint.h>

#define HEAP_ALIGNMENT ((size_t)16)
#define HEAP_CANARY_BYTES ((size_t)8)
#define HEAP_MIN_SPLIT ((size_t)32)
#define HEAP_ALLOC_MAGIC UINT64_C(0x4a4f5348414c4c43)
#define HEAP_FREE_MAGIC  UINT64_C(0x4a4f534846524545)
#define HEAP_CANARY_SEED UINT64_C(0xd19f4e7a63b285c1)

typedef struct heap_block {
    uint64_t magic;
    size_t capacity;
    size_t requested;
    struct heap_block *next;
    struct heap_block *prev;
    uint32_t is_free;
    uint32_t reserved;
} heap_block_t;

_Static_assert((sizeof(heap_block_t) & (HEAP_ALIGNMENT - 1u)) == 0,
               "heap block header must preserve payload alignment");

static heap_block_t *blocks;
static heap_grow_fn grow_callback;
static void *grow_context;
static uint64_t total_region_bytes;
static uint64_t allocated_payload_bytes;
static uint64_t active_allocations;
static uint64_t total_allocations;
static uint64_t allocation_failures;
static uint32_t region_count;
static int initialised;

static uintptr_t align_up_ptr(uintptr_t value, size_t alignment) {
    uintptr_t mask = (uintptr_t)alignment - 1u;
    return (value + mask) & ~mask;
}

static int align_up_size(size_t value, size_t alignment, size_t *out) {
    if (!out || alignment == 0 || (alignment & (alignment - 1u)) != 0) return 0;
    size_t mask = alignment - 1u;
    if ((value & mask) == 0) {
        *out = value;
        return 1;
    }
    size_t add = alignment - (value & mask);
    if (SIZE_MAX - value < add) return 0;
    *out = value + add;
    return 1;
}

static unsigned char *payload(heap_block_t *block) {
    return (unsigned char *)(block + 1);
}

static uint64_t block_canary(const heap_block_t *block) {
    return HEAP_CANARY_SEED ^
           (uint64_t)(uintptr_t)block ^
           (uint64_t)block->requested;
}

static void write_u64(unsigned char *destination, uint64_t value) {
    for (unsigned i = 0; i < 8; ++i) {
        destination[i] = (unsigned char)(value >> (i * 8u));
    }
}

static uint64_t read_u64(const unsigned char *source) {
    uint64_t value = 0;
    for (unsigned i = 0; i < 8; ++i) {
        value |= (uint64_t)source[i] << (i * 8u);
    }
    return value;
}

static void fill_bytes(unsigned char *destination, size_t length, unsigned char value) {
    while (length--) *destination++ = value;
}

static int blocks_adjacent(const heap_block_t *left, const heap_block_t *right) {
    const unsigned char *end =
        (const unsigned char *)(left + 1) + left->capacity;
    return end == (const unsigned char *)right;
}

static void merge_with_next(heap_block_t *block) {
    heap_block_t *next = block ? block->next : 0;
    if (!block || !next || !block->is_free || !next->is_free ||
        !blocks_adjacent(block, next)) {
        return;
    }

    block->capacity += sizeof(heap_block_t) + next->capacity;
    block->next = next->next;
    if (block->next) block->next->prev = block;
    block->magic = HEAP_FREE_MAGIC;
    block->requested = 0;
}

static void insert_region(heap_block_t *block) {
    if (!blocks) {
        blocks = block;
        return;
    }

    heap_block_t *current = blocks;
    heap_block_t *previous = 0;
    while (current &&
           (uintptr_t)current < (uintptr_t)block) {
        previous = current;
        current = current->next;
    }

    block->prev = previous;
    block->next = current;
    if (previous) previous->next = block;
    else blocks = block;
    if (current) current->prev = block;

    if (block->prev) {
        merge_with_next(block->prev);
        if (block->prev->next != block) block = block->prev;
    }
    merge_with_next(block);
}

static int add_region(void *memory, size_t bytes) {
    if (!memory || bytes <= sizeof(heap_block_t) + HEAP_MIN_SPLIT) return 0;

    uintptr_t raw = (uintptr_t)memory;
    uintptr_t aligned = align_up_ptr(raw, HEAP_ALIGNMENT);
    size_t skipped = (size_t)(aligned - raw);
    if (skipped >= bytes) return 0;
    bytes -= skipped;
    bytes &= ~(HEAP_ALIGNMENT - 1u);
    if (bytes <= sizeof(heap_block_t) + HEAP_MIN_SPLIT) return 0;

    heap_block_t *block = (heap_block_t *)aligned;
    block->magic = HEAP_FREE_MAGIC;
    block->capacity = bytes - sizeof(*block);
    block->requested = 0;
    block->next = 0;
    block->prev = 0;
    block->is_free = 1;
    block->reserved = 0;

    insert_region(block);
    total_region_bytes += bytes;
    region_count++;
    return 1;
}

static int grow_heap(size_t minimum_payload) {
    if (!grow_callback) return 0;

    size_t needed;
    if (SIZE_MAX - minimum_payload < sizeof(heap_block_t) + HEAP_CANARY_BYTES) {
        return 0;
    }
    needed = sizeof(heap_block_t) + minimum_payload + HEAP_CANARY_BYTES;

    size_t actual = 0;
    void *memory = grow_callback(needed, &actual, grow_context);
    return memory && add_region(memory, actual);
}

static heap_block_t *find_block(size_t capacity_needed) {
    for (heap_block_t *block = blocks; block; block = block->next) {
        if (block->is_free && block->capacity >= capacity_needed) return block;
    }
    return 0;
}

static void split_block(heap_block_t *block, size_t used_capacity) {
    if (!block || block->capacity <= used_capacity) return;

    size_t remaining = block->capacity - used_capacity;
    if (remaining < sizeof(heap_block_t) + HEAP_MIN_SPLIT) return;

    unsigned char *new_address = payload(block) + used_capacity;
    heap_block_t *tail = (heap_block_t *)new_address;
    tail->magic = HEAP_FREE_MAGIC;
    tail->capacity = remaining - sizeof(*tail);
    tail->requested = 0;
    tail->is_free = 1;
    tail->reserved = 0;
    tail->prev = block;
    tail->next = block->next;
    if (tail->next) tail->next->prev = tail;
    block->next = tail;
    block->capacity = used_capacity;
}

heap_status_t heap_init(heap_grow_fn grow, void *context) {
    if (!grow) return HEAP_BAD_ARGUMENT;

    blocks = 0;
    grow_callback = grow;
    grow_context = context;
    total_region_bytes = 0;
    allocated_payload_bytes = 0;
    active_allocations = 0;
    total_allocations = 0;
    allocation_failures = 0;
    region_count = 0;
    initialised = 1;
    return HEAP_OK;
}

void *kmalloc(size_t size) {
    if (!initialised || size == 0) {
        allocation_failures++;
        return 0;
    }

    if (SIZE_MAX - size < HEAP_CANARY_BYTES) {
        allocation_failures++;
        return 0;
    }

    size_t capacity_needed;
    if (!align_up_size(size + HEAP_CANARY_BYTES, HEAP_ALIGNMENT, &capacity_needed)) {
        allocation_failures++;
        return 0;
    }

    heap_block_t *block = find_block(capacity_needed);
    if (!block) {
        if (!grow_heap(capacity_needed)) {
            allocation_failures++;
            return 0;
        }
        block = find_block(capacity_needed);
        if (!block) {
            allocation_failures++;
            return 0;
        }
    }

    split_block(block, capacity_needed);
    block->magic = HEAP_ALLOC_MAGIC;
    block->requested = size;
    block->is_free = 0;

    unsigned char *result = payload(block);
    fill_bytes(result, block->capacity, 0xcd);
    write_u64(result + size, block_canary(block));

    allocated_payload_bytes += size;
    active_allocations++;
    total_allocations++;
    return result;
}

void *kzalloc(size_t size) {
    unsigned char *memory = (unsigned char *)kmalloc(size);
    if (!memory) return 0;
    fill_bytes(memory, size, 0);
    return memory;
}

heap_status_t kfree(void *pointer) {
    if (!initialised) return HEAP_NOT_INITIALISED;
    if (!pointer) return HEAP_OK;

    if (((uintptr_t)pointer & (HEAP_ALIGNMENT - 1u)) != 0) {
        return HEAP_INVALID_POINTER;
    }

    heap_block_t *block = ((heap_block_t *)pointer) - 1;
    if (block->magic == HEAP_FREE_MAGIC && block->is_free) {
        return HEAP_DOUBLE_FREE;
    }
    if (block->magic != HEAP_ALLOC_MAGIC || block->is_free ||
        block->requested == 0 ||
        block->requested + HEAP_CANARY_BYTES > block->capacity) {
        return HEAP_INVALID_POINTER;
    }

    uint64_t expected = block_canary(block);
    if (read_u64(payload(block) + block->requested) != expected) {
        return HEAP_CORRUPT;
    }

    allocated_payload_bytes -= block->requested;
    active_allocations--;
    fill_bytes(payload(block), block->capacity, 0xdd);
    block->magic = HEAP_FREE_MAGIC;
    block->requested = 0;
    block->is_free = 1;

    if (block->prev && block->prev->is_free &&
        blocks_adjacent(block->prev, block)) {
        block = block->prev;
        merge_with_next(block);
    }
    merge_with_next(block);
    return HEAP_OK;
}

heap_stats_t heap_stats(void) {
    heap_stats_t stats = {
        .total_region_bytes = total_region_bytes,
        .allocated_payload_bytes = allocated_payload_bytes,
        .active_allocations = active_allocations,
        .total_allocations = total_allocations,
        .allocation_failures = allocation_failures,
        .region_count = region_count
    };

    for (heap_block_t *block = blocks; block; block = block->next) {
        if (block->is_free) stats.free_payload_bytes += block->capacity;
    }
    return stats;
}

int heap_self_test(void) {
    heap_stats_t before = heap_stats();

    unsigned char *small = (unsigned char *)kmalloc(37);
    unsigned char *zeroed = (unsigned char *)kzalloc(257);
    unsigned char *large = (unsigned char *)kmalloc(8193);
    if (!small || !zeroed || !large) return 0;

    for (size_t i = 0; i < 37; ++i) small[i] = (unsigned char)(i ^ 0x5a);
    for (size_t i = 0; i < 257; ++i) if (zeroed[i] != 0) return 0;
    for (size_t i = 0; i < 8193; ++i) large[i] = (unsigned char)(i & 0xffu);

    if (kfree(zeroed) != HEAP_OK ||
        kfree(small) != HEAP_OK ||
        kfree(large) != HEAP_OK) {
        return 0;
    }

    heap_stats_t after = heap_stats();
    return after.active_allocations == before.active_allocations &&
           after.allocated_payload_bytes == before.allocated_payload_bytes;
}

const char *heap_status_string(heap_status_t status) {
    switch (status) {
        case HEAP_OK: return "ok";
        case HEAP_BAD_ARGUMENT: return "bad argument";
        case HEAP_NOT_INITIALISED: return "heap not initialised";
        case HEAP_OUT_OF_MEMORY: return "out of memory";
        case HEAP_INVALID_POINTER: return "invalid pointer";
        case HEAP_DOUBLE_FREE: return "double free";
        case HEAP_CORRUPT: return "heap canary corrupt";
        default: return "unknown heap error";
    }
}
