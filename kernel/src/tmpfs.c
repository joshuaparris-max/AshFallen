#include "tmpfs.h"
#include <stddef.h>
#include <stdint.h>

static void zero_bytes(void *pointer, size_t length) {
    uint8_t *bytes = (uint8_t *)pointer;
    for (size_t i = 0; i < length; ++i) bytes[i] = 0;
}

static size_t string_length(const char *text) {
    size_t n = 0;
    while (text && text[n]) n++;
    return n;
}

static int name_equal(const char *a, const char *b, size_t length) {
    for (size_t i = 0; i < length; ++i) if (a[i] != b[i]) return 0;
    return a[length] == '\0';
}

static int copy_name(char destination[VFS_NAME_MAX + 1u], const char *source, size_t length) {
    if (!source || length == 0 || length > VFS_NAME_MAX) return 0;
    for (size_t i = 0; i < length; ++i) destination[i] = source[i];
    destination[length] = '\0';
    return 1;
}

static int child_named(const tmpfs_t *fs, uint16_t parent, const char *name, size_t length) {
    for (uint16_t i = 1; i < TMPFS_MAX_NODES; ++i) {
        if (fs->nodes[i].used && fs->nodes[i].parent == parent &&
            name_equal(fs->nodes[i].name, name, length)) return (int)i;
    }
    return -1;
}

static int resolve_node(const tmpfs_t *fs, const char *path) {
    if (!fs || !path || path[0] != '/') return -1;
    if (path[1] == '\0') return 0;

    uint16_t current = 0;
    size_t pos = 1;
    while (path[pos]) {
        size_t start = pos;
        while (path[pos] && path[pos] != '/') pos++;
        size_t length = pos - start;
        if (length == 0 || length > VFS_NAME_MAX) return -1;
        int child = child_named(fs, current, path + start, length);
        if (child < 0) return -1;
        current = (uint16_t)child;
        if (path[pos] == '/') {
            if (fs->nodes[current].type != VFS_NODE_DIRECTORY) return -1;
            pos++;
            if (!path[pos]) return -1;
        }
    }
    return current;
}

static int resolve_parent(const tmpfs_t *fs, const char *path, uint16_t *parent_out,
                          const char **name_out, size_t *name_length_out) {
    if (!fs || !path || path[0] != '/' || path[1] == '\0') return 0;
    size_t length = string_length(path);
    if (length < 2 || path[length - 1] == '/') return 0;

    size_t slash = length - 1;
    while (slash > 0 && path[slash] != '/') slash--;
    size_t name_length = length - slash - 1u;
    if (name_length == 0 || name_length > VFS_NAME_MAX) return 0;

    if (slash == 0) {
        *parent_out = 0;
    } else {
        char parent_path[VFS_PATH_MAX + 1u];
        if (slash > VFS_PATH_MAX) return 0;
        for (size_t i = 0; i < slash; ++i) parent_path[i] = path[i];
        parent_path[slash] = '\0';
        int parent = resolve_node(fs, parent_path);
        if (parent < 0 || fs->nodes[parent].type != VFS_NODE_DIRECTORY) return 0;
        *parent_out = (uint16_t)parent;
    }
    *name_out = path + slash + 1u;
    *name_length_out = name_length;
    return 1;
}

static int allocate_node(tmpfs_t *fs) {
    for (uint16_t i = 1; i < TMPFS_MAX_NODES; ++i) if (!fs->nodes[i].used) return (int)i;
    return -1;
}

static vfs_status_t create_node(tmpfs_t *fs, const char *path, vfs_node_type_t type,
                                uint32_t permissions, int *index_out) {
    uint16_t parent;
    const char *name;
    size_t length;
    if (!resolve_parent(fs, path, &parent, &name, &length)) return VFS_BAD_ARGUMENT;
    if (child_named(fs, parent, name, length) >= 0) return VFS_EXISTS;
    int slot = allocate_node(fs);
    if (slot < 0) return VFS_NO_SPACE;

    tmpfs_node_t *node = &fs->nodes[slot];
    zero_bytes(node, sizeof(*node));
    node->used = 1;
    node->parent = parent;
    node->type = type;
    node->permissions = permissions & (VFS_PERM_READ | VFS_PERM_WRITE);
    if (!copy_name(node->name, name, length)) {
        node->used = 0;
        return VFS_BAD_ARGUMENT;
    }
    if (index_out) *index_out = slot;
    return VFS_OK;
}

static vfs_status_t op_mkdir(void *context, const char *path, uint32_t permissions) {
    tmpfs_t *fs = context;
    if (!fs || !path) return VFS_BAD_ARGUMENT;
    return create_node(fs, path, VFS_NODE_DIRECTORY, permissions, 0);
}

static vfs_status_t op_open(void *context, const char *path, uint32_t flags, uint32_t *handle_out) {
    tmpfs_t *fs = context;
    if (!fs || !path || !handle_out) return VFS_BAD_ARGUMENT;

    int node_index = resolve_node(fs, path);
    if (node_index < 0 && (flags & VFS_OPEN_CREATE)) {
        vfs_status_t status = create_node(fs, path, VFS_NODE_FILE,
                                         VFS_PERM_READ | VFS_PERM_WRITE, &node_index);
        if (status != VFS_OK) return status;
    }
    if (node_index < 0) return VFS_NOT_FOUND;
    tmpfs_node_t *node = &fs->nodes[node_index];
    if (node->type != VFS_NODE_FILE) return VFS_IS_DIRECTORY;
    if ((flags & VFS_OPEN_READ) && !(node->permissions & VFS_PERM_READ)) return VFS_PERMISSION;
    if ((flags & VFS_OPEN_WRITE) && !(node->permissions & VFS_PERM_WRITE)) return VFS_PERMISSION;

    uint32_t slot = TMPFS_MAX_OPEN;
    for (uint32_t i = 0; i < TMPFS_MAX_OPEN; ++i) if (!fs->open[i].used) { slot = i; break; }
    if (slot == TMPFS_MAX_OPEN) return VFS_NO_SPACE;

    if ((flags & VFS_OPEN_TRUNC) && (flags & VFS_OPEN_WRITE)) node->size = 0;
    fs->open[slot].used = 1;
    fs->open[slot].node = (uint16_t)node_index;
    fs->open[slot].flags = flags;
    fs->open[slot].offset = 0;
    *handle_out = slot;
    return VFS_OK;
}

static vfs_status_t op_close(void *context, uint32_t handle) {
    tmpfs_t *fs = context;
    if (!fs || handle >= TMPFS_MAX_OPEN || !fs->open[handle].used) return VFS_BAD_HANDLE;
    fs->open[handle].used = 0;
    return VFS_OK;
}

static vfs_status_t op_read(void *context, uint32_t handle, void *buffer,
                            size_t length, size_t *read_out) {
    tmpfs_t *fs = context;
    if (!fs || !buffer || !read_out || handle >= TMPFS_MAX_OPEN || !fs->open[handle].used) {
        return VFS_BAD_HANDLE;
    }
    tmpfs_open_t *open = &fs->open[handle];
    if (!(open->flags & VFS_OPEN_READ)) return VFS_PERMISSION;
    tmpfs_node_t *node = &fs->nodes[open->node];
    if (open->offset >= node->size) {
        *read_out = 0;
        return VFS_OK;
    }
    size_t available = (size_t)(node->size - open->offset);
    size_t take = length < available ? length : available;
    for (size_t i = 0; i < take; ++i) ((uint8_t *)buffer)[i] = node->data[open->offset + i];
    open->offset += take;
    *read_out = take;
    return VFS_OK;
}

static vfs_status_t op_write(void *context, uint32_t handle, const void *buffer,
                             size_t length, size_t *written_out) {
    tmpfs_t *fs = context;
    if (!fs || (!buffer && length != 0) || !written_out ||
        handle >= TMPFS_MAX_OPEN || !fs->open[handle].used) return VFS_BAD_HANDLE;
    tmpfs_open_t *open = &fs->open[handle];
    if (!(open->flags & VFS_OPEN_WRITE)) return VFS_PERMISSION;
    tmpfs_node_t *node = &fs->nodes[open->node];
    if (open->offset > TMPFS_FILE_CAPACITY || length > TMPFS_FILE_CAPACITY - open->offset) {
        return VFS_NO_SPACE;
    }
    for (size_t i = 0; i < length; ++i) node->data[open->offset + i] = ((const uint8_t *)buffer)[i];
    open->offset += length;
    if (open->offset > node->size) node->size = (uint32_t)open->offset;
    *written_out = length;
    return VFS_OK;
}

static vfs_status_t op_seek(void *context, uint32_t handle, uint64_t offset) {
    tmpfs_t *fs = context;
    if (!fs || handle >= TMPFS_MAX_OPEN || !fs->open[handle].used) return VFS_BAD_HANDLE;
    if (offset > TMPFS_FILE_CAPACITY) return VFS_BAD_ARGUMENT;
    fs->open[handle].offset = offset;
    return VFS_OK;
}

static vfs_status_t op_list(void *context, const char *path, uint32_t index, vfs_dirent_t *entry_out) {
    tmpfs_t *fs = context;
    if (!fs || !path || !entry_out) return VFS_BAD_ARGUMENT;
    int directory = resolve_node(fs, path);
    if (directory < 0) return VFS_NOT_FOUND;
    if (fs->nodes[directory].type != VFS_NODE_DIRECTORY) return VFS_NOT_DIRECTORY;

    uint32_t seen = 0;
    for (uint16_t i = 1; i < TMPFS_MAX_NODES; ++i) {
        if (!fs->nodes[i].used || fs->nodes[i].parent != (uint16_t)directory) continue;
        if (seen++ != index) continue;
        zero_bytes(entry_out, sizeof(*entry_out));
        size_t length = string_length(fs->nodes[i].name);
        for (size_t j = 0; j <= length; ++j) entry_out->name[j] = fs->nodes[i].name[j];
        entry_out->type = fs->nodes[i].type;
        entry_out->permissions = fs->nodes[i].permissions;
        entry_out->size = fs->nodes[i].size;
        return VFS_OK;
    }
    return VFS_NOT_FOUND;
}

static const vfs_fs_ops_t operations = {
    .mkdir = op_mkdir,
    .open = op_open,
    .close = op_close,
    .read = op_read,
    .write = op_write,
    .seek = op_seek,
    .list = op_list
};

void tmpfs_init(tmpfs_t *filesystem) {
    if (!filesystem) return;
    zero_bytes(filesystem, sizeof(*filesystem));
    filesystem->nodes[0].used = 1;
    filesystem->nodes[0].type = VFS_NODE_DIRECTORY;
    filesystem->nodes[0].permissions = VFS_PERM_READ | VFS_PERM_WRITE;
    filesystem->nodes[0].name[0] = '\0';
}

const vfs_fs_ops_t *tmpfs_ops(void) {
    return &operations;
}

vfs_status_t tmpfs_set_permissions(tmpfs_t *filesystem, const char *path, uint32_t permissions) {
    if (!filesystem || !path) return VFS_BAD_ARGUMENT;
    int node = resolve_node(filesystem, path);
    if (node < 0) return VFS_NOT_FOUND;
    filesystem->nodes[node].permissions = permissions & (VFS_PERM_READ | VFS_PERM_WRITE);
    return VFS_OK;
}
