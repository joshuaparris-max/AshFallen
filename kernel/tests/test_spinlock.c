#include "spinlock.h"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#define THREADS 4
#define ITERATIONS 50000

static spinlock_t lock = SPINLOCK_INIT;
static uint64_t counter;
static int failures;

static void expect(const char *name, int ok) {
    if (!ok) {
        fprintf(stderr, "FAIL: %s\n", name);
        failures++;
    }
}

static void *worker(void *argument) {
    (void)argument;
    for (unsigned i = 0; i < ITERATIONS; ++i) {
        spin_lock(&lock);
        counter++;
        spin_unlock(&lock);
    }
    return 0;
}

int main(void) {
    expect("try lock succeeds", spin_try_lock(&lock));
    expect("try lock excludes", !spin_try_lock(&lock));
    spin_unlock(&lock);

    pthread_t threads[THREADS];
    for (unsigned i = 0; i < THREADS; ++i) {
        expect("pthread create",
               pthread_create(&threads[i], 0, worker, 0) == 0);
    }
    for (unsigned i = 0; i < THREADS; ++i) {
        expect("pthread join", pthread_join(threads[i], 0) == 0);
    }

    expect("contended counter exact",
           counter == (uint64_t)THREADS * ITERATIONS);

    if (failures) return 1;
    puts("spinlock contention tests passed");
    return 0;
}
