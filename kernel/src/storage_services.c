#include "storage_services.h"
#include "tmpfs.h"
#include <stddef.h>
#include <stdint.h>

static tmpfs_t root_fs;
static tmpfs_t var_fs;
static uint64_t log_offset;
static int initialised;

static size_t string_length(const char *text) {
    size_t n = 0;
    while (text && text[n]) n++;
    return n;
}

static int safe_name(const char *name) {
    if (!name || !name[0]) return 0;
    for (size_t i = 0; name[i]; ++i) {
        char c = name[i];
        int ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                 (c >= '0' && c <= '9') || c == '-' || c == '_';
        if (!ok || i >= VFS_NAME_MAX) return 0;
    }
    return 1;
}

static int make_path(char path[VFS_PATH_MAX + 1u], const char *prefix, const char *name) {
    if (!safe_name(name)) return 0;
    size_t prefix_len = string_length(prefix);
    size_t name_len = string_length(name);
    if (prefix_len + name_len > VFS_PATH_MAX) return 0;
    for (size_t i = 0; i < prefix_len; ++i) path[i] = prefix[i];
    for (size_t i = 0; i < name_len; ++i) path[prefix_len + i] = name[i];
    path[prefix_len + name_len] = '\0';
    return 1;
}

static vfs_status_t write_value(const char *path, const char *value) {
    if (!path || !value) return VFS_BAD_ARGUMENT;
    uint32_t handle;
    vfs_status_t status = vfs_open(path, VFS_OPEN_WRITE | VFS_OPEN_CREATE | VFS_OPEN_TRUNC, &handle);
    if (status != VFS_OK) return status;
    size_t written = 0;
    status = vfs_write(handle, value, string_length(value), &written);
    vfs_status_t close_status = vfs_close(handle);
    if (status != VFS_OK) return status;
    if (close_status != VFS_OK) return close_status;
    return written == string_length(value) ? VFS_OK : VFS_IO_ERROR;
}

static vfs_status_t read_value(const char *path, void *buffer, size_t capacity, size_t *read_out) {
    if (!path || !buffer || !read_out) return VFS_BAD_ARGUMENT;
    uint32_t handle;
    vfs_status_t status = vfs_open(path, VFS_OPEN_READ, &handle);
    if (status != VFS_OK) return status;
    status = vfs_read(handle, buffer, capacity, read_out);
    vfs_status_t close_status = vfs_close(handle);
    if (status != VFS_OK) return status;
    return close_status;
}

vfs_status_t storage_services_init(void) {
    vfs_init();
    tmpfs_init(&root_fs);
    tmpfs_init(&var_fs);
    log_offset = 0;

    vfs_status_t status = vfs_mount("/", &root_fs, tmpfs_ops());
    if (status != VFS_OK) return status;
    status = vfs_mkdir("/var", VFS_PERM_READ | VFS_PERM_WRITE);
    if (status != VFS_OK) return status;
    status = vfs_mkdir("/settings", VFS_PERM_READ | VFS_PERM_WRITE);
    if (status != VFS_OK) return status;
    status = vfs_mkdir("/devices", VFS_PERM_READ | VFS_PERM_WRITE);
    if (status != VFS_OK) return status;
    status = vfs_mount("/var", &var_fs, tmpfs_ops());
    if (status != VFS_OK) return status;
    initialised = 1;
    return VFS_OK;
}

vfs_status_t log_service_append(const char *text) {
    if (!initialised || !text) return VFS_BAD_ARGUMENT;
    uint32_t handle;
    vfs_status_t status = vfs_open("/var/kernel.log", VFS_OPEN_WRITE | VFS_OPEN_CREATE, &handle);
    if (status != VFS_OK) return status;
    status = vfs_seek(handle, log_offset);
    if (status == VFS_OK) {
        size_t written = 0;
        status = vfs_write(handle, text, string_length(text), &written);
        if (status == VFS_OK) log_offset += written;
    }
    vfs_status_t close_status = vfs_close(handle);
    return status != VFS_OK ? status : close_status;
}

vfs_status_t log_service_read(void *buffer, size_t capacity, size_t *read_out) {
    if (!initialised) return VFS_BAD_ARGUMENT;
    return read_value("/var/kernel.log", buffer, capacity, read_out);
}

vfs_status_t settings_service_set(const char *key, const char *value) {
    if (!initialised) return VFS_BAD_ARGUMENT;
    char path[VFS_PATH_MAX + 1u];
    if (!make_path(path, "/settings/", key)) return VFS_BAD_ARGUMENT;
    return write_value(path, value);
}

vfs_status_t settings_service_get(const char *key, void *buffer, size_t capacity, size_t *read_out) {
    if (!initialised) return VFS_BAD_ARGUMENT;
    char path[VFS_PATH_MAX + 1u];
    if (!make_path(path, "/settings/", key)) return VFS_BAD_ARGUMENT;
    return read_value(path, buffer, capacity, read_out);
}

vfs_status_t device_service_publish(const char *name, const char *value) {
    if (!initialised) return VFS_BAD_ARGUMENT;
    char path[VFS_PATH_MAX + 1u];
    if (!make_path(path, "/devices/", name)) return VFS_BAD_ARGUMENT;
    return write_value(path, value);
}

vfs_status_t device_service_read(const char *name, void *buffer, size_t capacity, size_t *read_out) {
    if (!initialised) return VFS_BAD_ARGUMENT;
    char path[VFS_PATH_MAX + 1u];
    if (!make_path(path, "/devices/", name)) return VFS_BAD_ARGUMENT;
    return read_value(path, buffer, capacity, read_out);
}
