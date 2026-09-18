#include "storage_services.h"
#include "tmpfs.h"
#include "vfs.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;

static void expect(const char *name, int condition) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", name);
        failures++;
    }
}

int main(void) {
    tmpfs_t root;
    tmpfs_t data;
    tmpfs_init(&root);
    tmpfs_init(&data);
    vfs_init();

    expect("mount root", vfs_mount("/", &root, tmpfs_ops()) == VFS_OK);
    expect("mkdir data", vfs_mkdir("/data", VFS_PERM_READ | VFS_PERM_WRITE) == VFS_OK);
    expect("mount data", vfs_mount("/data", &data, tmpfs_ops()) == VFS_OK);

    uint32_t handle;
    expect("create file", vfs_open("/data/hello.txt",
           VFS_OPEN_READ | VFS_OPEN_WRITE | VFS_OPEN_CREATE, &handle) == VFS_OK);
    const char message[] = "hello Josh";
    size_t written = 0;
    expect("write file", vfs_write(handle, message, sizeof(message) - 1u, &written) == VFS_OK &&
           written == sizeof(message) - 1u);
    expect("seek file", vfs_seek(handle, 0) == VFS_OK);
    char buffer[64] = {0};
    size_t read = 0;
    expect("read file", vfs_read(handle, buffer, sizeof(buffer), &read) == VFS_OK &&
           read == sizeof(message) - 1u && memcmp(buffer, message, read) == 0);
    expect("close file", vfs_close(handle) == VFS_OK);

    vfs_dirent_t entry;
    expect("list mounted directory", vfs_list("/data", 0, &entry) == VFS_OK &&
           strcmp(entry.name, "hello.txt") == 0 && entry.type == VFS_NODE_FILE);

    expect("set read-only", tmpfs_set_permissions(&data, "/hello.txt", VFS_PERM_READ) == VFS_OK);
    expect("deny write", vfs_open("/data/hello.txt", VFS_OPEN_WRITE, &handle) == VFS_PERMISSION);
    expect("allow read", vfs_open("/data/hello.txt", VFS_OPEN_READ, &handle) == VFS_OK);
    expect("close read", vfs_close(handle) == VFS_OK);

    expect("services init", storage_services_init() == VFS_OK);
    expect("log append 1", log_service_append("boot\n") == VFS_OK);
    expect("log append 2", log_service_append("storage\n") == VFS_OK);
    memset(buffer, 0, sizeof(buffer));
    expect("log read", log_service_read(buffer, sizeof(buffer), &read) == VFS_OK &&
           read == 13 && memcmp(buffer, "boot\nstorage\n", 13) == 0);

    expect("settings set", settings_service_set("theme", "dark") == VFS_OK);
    memset(buffer, 0, sizeof(buffer));
    expect("settings get", settings_service_get("theme", buffer, sizeof(buffer), &read) == VFS_OK &&
           read == 4 && memcmp(buffer, "dark", 4) == 0);
    expect("settings reject path injection", settings_service_set("../bad", "x") == VFS_BAD_ARGUMENT);

    expect("device publish", device_service_publish("sata0", "ready") == VFS_OK);
    memset(buffer, 0, sizeof(buffer));
    expect("device read", device_service_read("sata0", buffer, sizeof(buffer), &read) == VFS_OK &&
           read == 5 && memcmp(buffer, "ready", 5) == 0);

    if (failures) return 1;
    puts("VFS, tmpfs, and storage service tests passed");
    return 0;
}
