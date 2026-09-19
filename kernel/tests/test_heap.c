#include "heap.h"
#include <stdint.h>
#include <stdio.h>

#define TEST_POOL_BYTES (256u * 1024u)

static unsigned char pool[TEST_POOL_BYTES + 32u];
static size_t pool_used;
static int failures;

static void expect(const char *name, int ok) {
    if (!ok) {
        fprintf(stderr, "FAIL: %s\n", name);
        failures++;
    }
}

static void *test_grow(size_t minimum, size_t *actual, void *context) {
    (void)context;
    size_t chunk = minimum < 16384u ? 16384u : minimum;
    chunk = (chunk + 15u) & ~(size_t)15u;
    if (pool_used + chunk > TEST_POOL_BYTES) return 0;
    void *result = pool + pool_used;
    pool_used += chunk;
    *actual = chunk;
    return result;
}

int main(void) {
    pool_used = 0;
    expect("init", heap_init(test_grow, 0) == HEAP_OK);

    unsigned char *a = (unsigned char *)kmalloc(24);
    unsigned char *b = (unsigned char *)kzalloc(1000);
    unsigned char *c = (unsigned char *)kmalloc(20000);
    expect("allocations", a && b && c);
    expect("alignment",
           ((uintptr_t)a & 15u) == 0 &&
           ((uintptr_t)b & 15u) == 0 &&
           ((uintptr_t)c & 15u) == 0);

    int zero = 1;
    for (unsigned i = 0; i < 1000; ++i) if (b[i] != 0) zero = 0;
    expect("zeroed allocation", zero);

    heap_stats_t live = heap_stats();
    expect("active accounting", live.active_allocations == 3);
    expect("payload accounting", live.allocated_payload_bytes == 21024u);
    expect("growth happened", live.region_count >= 2);

    expect("free middle", kfree(b) == HEAP_OK);
    expect("free first", kfree(a) == HEAP_OK);
    expect("free large", kfree(c) == HEAP_OK);
    expect("double free", kfree(c) == HEAP_DOUBLE_FREE);

    heap_stats_t empty = heap_stats();
    expect("no active allocations", empty.active_allocations == 0);
    expect("no payload leak", empty.allocated_payload_bytes == 0);

    unsigned char *guard = (unsigned char *)kmalloc(32);
    expect("guard allocation", guard != 0);
    guard[32] ^= 1u;
    expect("canary detects overflow", kfree(guard) == HEAP_CORRUPT);

    pool_used = 0;
    expect("reset", heap_init(test_grow, 0) == HEAP_OK);
    expect("self test", heap_self_test());
    for (int status = HEAP_OK; status <= HEAP_CORRUPT; ++status) {
        expect("heap status string", heap_status_string((heap_status_t)status) != NULL);
    }
    expect("unknown heap status string", heap_status_string((heap_status_t)999) != NULL);

    if (failures) return 1;
    puts("heap allocator tests passed");
    return 0;
}
