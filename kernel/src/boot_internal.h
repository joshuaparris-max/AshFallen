#ifndef JOSHOS_BOOT_INTERNAL_H
#define JOSHOS_BOOT_INTERNAL_H

#include "boot.h"
#include "josh_boot_protocol.h"

boot_status_t boot_limine_context_init(boot_context_t *context);
boot_status_t boot_josh_context_init(boot_context_t *context, const JoshBootInfo *info);

#endif
