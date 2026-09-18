#ifndef JOSHOS_SPINLOCK_H
#define JOSHOS_SPINLOCK_H

#include <stdint.h>

typedef struct {
    volatile uint32_t value;
} spinlock_t;

#define SPINLOCK_INIT {0}

static inline uint32_t spin_exchange(
    volatile uint32_t *pointer,
    uint32_t value
) {
    __asm__ volatile (
        "xchgl %0, %1"
        : "+r"(value), "+m"(*pointer)
        :
        : "memory"
    );
    return value;
}

static inline void spin_lock(spinlock_t *lock) {
    while (spin_exchange(&lock->value, 1u) != 0) {
        while (lock->value != 0) {
            __asm__ volatile ("pause");
        }
    }
    __asm__ volatile ("" ::: "memory");
}

static inline int spin_try_lock(spinlock_t *lock) {
    if (spin_exchange(&lock->value, 1u) != 0) return 0;
    __asm__ volatile ("" ::: "memory");
    return 1;
}

static inline void spin_unlock(spinlock_t *lock) {
    __asm__ volatile ("" ::: "memory");
    lock->value = 0;
}

static inline uint64_t spin_lock_irqsave(spinlock_t *lock) {
    uint64_t flags;
    __asm__ volatile (
        "pushfq\n\t"
        "popq %0\n\t"
        "cli"
        : "=r"(flags)
        :
        : "memory"
    );
    spin_lock(lock);
    return flags;
}

static inline void spin_unlock_irqrestore(
    spinlock_t *lock,
    uint64_t flags
) {
    spin_unlock(lock);
    if ((flags & (UINT64_C(1) << 9)) != 0) {
        __asm__ volatile ("sti" ::: "memory");
    }
}

#endif
