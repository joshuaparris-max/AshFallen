#ifndef JOSHOS_TMPFS_H
#define JOSHOS_TMPFS_H

#include "vfs.h"
#include <stdint.h>

#define TMPFS_MAX_NODES 32u
#define TMPFS_MAX_OPEN 16u
#define TMPFS_FILE_CAPACITY 2048u

typedef struct {
    int used;
    uint16_t parent;
    vfs_node_type_t type;
    uint32_t permissions;
    uint32_t size;
    char name[VFS_NAME_MAX + 1u];
    uint8_t data[TMPFS_FILE_CAPACITY];
} tmpfs_node_t;

typedef struct {
    int used;
    uint16_t node;
    uint32_t flags;
    uint64_t offset;
} tmpfs_open_t;

typedef struct {
    tmpfs_node_t nodes[TMPFS_MAX_NODES];
    tmpfs_open_t open[TMPFS_MAX_OPEN];
} tmpfs_t;

void tmpfs_init(tmpfs_t *filesystem);
const vfs_fs_ops_t *tmpfs_ops(void);
vfs_status_t tmpfs_set_permissions(tmpfs_t *filesystem, const char *path, uint32_t permissions);

#endif
