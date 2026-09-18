#ifndef JOSHOS_STORAGE_SERVICES_H
#define JOSHOS_STORAGE_SERVICES_H

#include "vfs.h"
#include <stddef.h>

vfs_status_t storage_services_init(void);
vfs_status_t log_service_append(const char *text);
vfs_status_t log_service_read(void *buffer, size_t capacity, size_t *read_out);
vfs_status_t settings_service_set(const char *key, const char *value);
vfs_status_t settings_service_get(const char *key, void *buffer, size_t capacity, size_t *read_out);
vfs_status_t device_service_publish(const char *name, const char *value);
vfs_status_t device_service_read(const char *name, void *buffer, size_t capacity, size_t *read_out);

#endif
