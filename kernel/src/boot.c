#include "boot_internal.h"

boot_status_t boot_context_init(boot_context_t *context,
                                uint64_t loader_magic1,
                                uint64_t loader_magic2,
                                const void *loader_payload) {
    if (loader_magic1 == JOSH_LOADER_MAGIC1 && loader_magic2 == JOSH_LOADER_MAGIC2) {
        return boot_josh_context_init(context, (const JoshBootInfo *)loader_payload);
    }
    return boot_limine_context_init(context);
}
