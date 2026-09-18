#ifndef JOSHOS_VFS_H
#define JOSHOS_VFS_H

#include <stddef.h>
#include <stdint.h>

#define VFS_PATH_MAX 127u
#define VFS_NAME_MAX 31u
#define VFS_MAX_MOUNTS 8u
#define VFS_MAX_HANDLES 16u

#define VFS_PERM_READ  0x01u
#define VFS_PERM_WRITE 0x02u

#define VFS_OPEN_READ   0x01u
#define VFS_OPEN_WRITE  0x02u
#define VFS_OPEN_CREATE 0x04u
#define VFS_OPEN_TRUNC  0x08u

typedef enum {
    VFS_OK = 0,
    VFS_BAD_ARGUMENT,
    VFS_NOT_FOUND,
    VFS_EXISTS,
    VFS_NOT_DIRECTORY,
    VFS_IS_DIRECTORY,
    VFS_PERMISSION,
    VFS_NO_SPACE,
    VFS_BAD_HANDLE,
    VFS_IO_ERROR
} vfs_status_t;

typedef enum {
    VFS_NODE_FILE = 1,
    VFS_NODE_DIRECTORY = 2
} vfs_node_type_t;

typedef struct {
    char name[VFS_NAME_MAX + 1u];
    vfs_node_type_t type;
    uint32_t permissions;
    uint64_t size;
} vfs_dirent_t;

typedef struct vfs_fs_ops {
    vfs_status_t (*mkdir)(void *context, const char *path, uint32_t permissions);
    vfs_status_t (*open)(void *context, const char *path, uint32_t flags, uint32_t *backend_handle);
    vfs_status_t (*close)(void *context, uint32_t backend_handle);
    vfs_status_t (*read)(void *context, uint32_t backend_handle, void *buffer, size_t length, size_t *read_out);
    vfs_status_t (*write)(void *context, uint32_t backend_handle, const void *buffer, size_t length, size_t *written_out);
    vfs_status_t (*seek)(void *context, uint32_t backend_handle, uint64_t offset);
    vfs_status_t (*list)(void *context, const char *path, uint32_t index, vfs_dirent_t *entry_out);
} vfs_fs_ops_t;

void vfs_init(void);
vfs_status_t vfs_mount(const char *path, void *context, const vfs_fs_ops_t *ops);
vfs_status_t vfs_mkdir(const char *path, uint32_t permissions);
vfs_status_t vfs_open(const char *path, uint32_t flags, uint32_t *handle_out);
vfs_status_t vfs_close(uint32_t handle);
vfs_status_t vfs_read(uint32_t handle, void *buffer, size_t length, size_t *read_out);
vfs_status_t vfs_write(uint32_t handle, const void *buffer, size_t length, size_t *written_out);
vfs_status_t vfs_seek(uint32_t handle, uint64_t offset);
vfs_status_t vfs_list(const char *path, uint32_t index, vfs_dirent_t *entry_out);
const char *vfs_status_string(vfs_status_t status);

#endif
