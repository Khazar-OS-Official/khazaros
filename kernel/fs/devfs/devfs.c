#include <fs/devfs.h>
#include <mm/kheap.h>
#include <libk/string.h>

#define MAX_DEVICES 32

static vfs_node_t *devfs_root = NULL;
static vfs_node_t *devices[MAX_DEVICES];
static int num_devices = 0;

static vfs_node_t *devfs_readdir(vfs_node_t *node, uint32_t index) {
    (void)node;
    if (index >= (uint32_t)num_devices) return NULL;
    return devices[index];
}

static vfs_node_t *devfs_finddir(vfs_node_t *node, char *name) {
    (void)node;
    for (int i = 0; i < num_devices; i++) {
        if (strncmp(devices[i]->name, name, 127) == 0) return devices[i];
    }
    return NULL;
}

void devfs_init(void) {
    devfs_root = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    memset(devfs_root, 0, sizeof(vfs_node_t));
    kstrncpy(devfs_root->name, "dev", 127);
    devfs_root->flags = VFS_DIRECTORY;
    devfs_root->readdir = devfs_readdir;
    devfs_root->finddir = devfs_finddir;

    vfs_node_t *dev_dir = vfs_find_path(vfs_root, "/dev");
    if (!dev_dir) {
        dev_dir = vfs_mkdir(vfs_root, "dev");
    }
    
    if (dev_dir) {
        vfs_mount("/dev", devfs_root);
    }
}

void devfs_register_device(const char *name, vfs_node_t *device_node) {
    if (num_devices >= MAX_DEVICES) return;
    kstrncpy(device_node->name, name, 127);
    devices[num_devices++] = device_node;
}
