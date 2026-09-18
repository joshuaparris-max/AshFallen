#include "vfs.h"

typedef struct {
    int used;
    char path[VFS_PATH_MAX + 1u];
    void *context;
    const vfs_fs_ops_t *ops;
} mount_t;

typedef struct {
    int used;
    uint32_t mount_index;
    uint32_t backend_handle;
} handle_t;

static mount_t mounts[VFS_MAX_MOUNTS];
static handle_t handles[VFS_MAX_HANDLES];

static size_t string_length(const char *text) {
    size_t n = 0;
    while (text && text[n]) n++;
    return n;
}

static int copy_path(char destination[VFS_PATH_MAX + 1u], const char *source) {
    size_t n = string_length(source);
    if (n == 0 || n > VFS_PATH_MAX || source[0] != '/') return 0;
    for (size_t i = 0; i <= n; ++i) destination[i] = source[i];
    return 1;
}

static int mount_matches(const char *mount_path, const char *path) {
    if (mount_path[0] == '/' && mount_path[1] == '\0') return path[0] == '/';
    size_t length = string_length(mount_path);
    for (size_t i = 0; i < length; ++i) if (mount_path[i] != path[i]) return 0;
    return path[length] == '\0' || path[length] == '/';
}

static int resolve_mount(const char *path, uint32_t *index_out, const char **relative_out) {
    size_t best = 0;
    int found = -1;
    for (uint32_t i = 0; i < VFS_MAX_MOUNTS; ++i) {
        if (!mounts[i].used || !mount_matches(mounts[i].path, path)) continue;
        size_t length = string_length(mounts[i].path);
        if (found < 0 || length > best) {
            found = (int)i;
            best = length;
        }
    }
    if (found < 0) return 0;
    *index_out = (uint32_t)found;
    const char *relative = path;
    if (!(mounts[found].path[0] == '/' && mounts[found].path[1] == '\0')) relative += best;
    if (*relative == '\0') relative = "/";
    *relative_out = relative;
    return 1;
}

void vfs_init(void) {
    for (uint32_t i = 0; i < VFS_MAX_MOUNTS; ++i) mounts[i].used = 0;
    for (uint32_t i = 0; i < VFS_MAX_HANDLES; ++i) handles[i].used = 0;
}

vfs_status_t vfs_mount(const char *path, void *context, const vfs_fs_ops_t *ops) {
    if (!path || !ops || !ops->open || !ops->close || !ops->read || !ops->write ||
        !ops->seek || !ops->mkdir || !ops->list) return VFS_BAD_ARGUMENT;
    for (uint32_t i = 0; i < VFS_MAX_MOUNTS; ++i) {
        if (mounts[i].used) continue;
        if (!copy_path(mounts[i].path, path)) return VFS_BAD_ARGUMENT;
        mounts[i].context = context;
        mounts[i].ops = ops;
        mounts[i].used = 1;
        return VFS_OK;
    }
    return VFS_NO_SPACE;
}

vfs_status_t vfs_mkdir(const char *path, uint32_t permissions) {
    uint32_t mount;
    const char *relative;
    if (!path || !resolve_mount(path, &mount, &relative)) return VFS_NOT_FOUND;
    return mounts[mount].ops->mkdir(mounts[mount].context, relative, permissions);
}

vfs_status_t vfs_open(const char *path, uint32_t flags, uint32_t *handle_out) {
    if (!path || !handle_out || (flags & (VFS_OPEN_READ | VFS_OPEN_WRITE)) == 0) return VFS_BAD_ARGUMENT;
    uint32_t mount;
    const char *relative;
    if (!resolve_mount(path, &mount, &relative)) return VFS_NOT_FOUND;

    uint32_t slot = VFS_MAX_HANDLES;
    for (uint32_t i = 0; i < VFS_MAX_HANDLES; ++i) if (!handles[i].used) { slot = i; break; }
    if (slot == VFS_MAX_HANDLES) return VFS_NO_SPACE;

    uint32_t backend = 0;
    vfs_status_t status = mounts[mount].ops->open(mounts[mount].context, relative, flags, &backend);
    if (status != VFS_OK) return status;
    handles[slot].used = 1;
    handles[slot].mount_index = mount;
    handles[slot].backend_handle = backend;
    *handle_out = slot;
    return VFS_OK;
}

vfs_status_t vfs_close(uint32_t handle) {
    if (handle >= VFS_MAX_HANDLES || !handles[handle].used) return VFS_BAD_HANDLE;
    handle_t current = handles[handle];
    vfs_status_t status = mounts[current.mount_index].ops->close(
        mounts[current.mount_index].context, current.backend_handle);
    handles[handle].used = 0;
    return status;
}

vfs_status_t vfs_read(uint32_t handle, void *buffer, size_t length, size_t *read_out) {
    if (handle >= VFS_MAX_HANDLES || !handles[handle].used) return VFS_BAD_HANDLE;
    handle_t *current = &handles[handle];
    return mounts[current->mount_index].ops->read(
        mounts[current->mount_index].context, current->backend_handle, buffer, length, read_out);
}

vfs_status_t vfs_write(uint32_t handle, const void *buffer, size_t length, size_t *written_out) {
    if (handle >= VFS_MAX_HANDLES || !handles[handle].used) return VFS_BAD_HANDLE;
    handle_t *current = &handles[handle];
    return mounts[current->mount_index].ops->write(
        mounts[current->mount_index].context, current->backend_handle, buffer, length, written_out);
}

vfs_status_t vfs_seek(uint32_t handle, uint64_t offset) {
    if (handle >= VFS_MAX_HANDLES || !handles[handle].used) return VFS_BAD_HANDLE;
    handle_t *current = &handles[handle];
    return mounts[current->mount_index].ops->seek(
        mounts[current->mount_index].context, current->backend_handle, offset);
}

vfs_status_t vfs_list(const char *path, uint32_t index, vfs_dirent_t *entry_out) {
    uint32_t mount;
    const char *relative;
    if (!path || !entry_out || !resolve_mount(path, &mount, &relative)) return VFS_NOT_FOUND;
    return mounts[mount].ops->list(mounts[mount].context, relative, index, entry_out);
}

const char *vfs_status_string(vfs_status_t status) {
    switch (status) {
        case VFS_OK: return "ok";
        case VFS_BAD_ARGUMENT: return "bad argument";
        case VFS_NOT_FOUND: return "not found";
        case VFS_EXISTS: return "exists";
        case VFS_NOT_DIRECTORY: return "not a directory";
        case VFS_IS_DIRECTORY: return "is a directory";
        case VFS_PERMISSION: return "permission denied";
        case VFS_NO_SPACE: return "no space";
        case VFS_BAD_HANDLE: return "bad handle";
        case VFS_IO_ERROR: return "I/O error";
        default: return "unknown VFS error";
    }
}
