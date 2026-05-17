#include <fs/devfs.h>
#include <mm/kheap.h>
#include <libk/string.h>
#include <net/ethernet.h>

#define MAX_DEVICES 32

static vfs_node_t *devfs_root = NULL;
static vfs_node_t *devices[MAX_DEVICES];
static int num_devices = 0;

static uint32_t netinfo_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node;
    if (offset > 0) return 0;
    
    char tmp[128] = "None";
    if (ethernet_is_ready()) {
        const ethernet_device_t *eth = ethernet_get_device();
        if (eth && eth->pci_dev) {
            char dname[32] = "Unknown";
            if (eth->pci_dev->vendor_id == 0x10EC) {
                strcpy(dname, "Realtek RTL8136");
            } else if (eth->pci_dev->vendor_id == 0x8086) {
                strcpy(dname, "Intel E1000");
            }
            extern void ksnprintf(char *str, size_t n, const char *format, ...);
            ksnprintf(tmp, sizeof(tmp), "%s (%02X:%02X:%02X:%02X:%02X:%02X)",
                      dname, eth->mac[0], eth->mac[1], eth->mac[2], eth->mac[3], eth->mac[4], eth->mac[5]);
        }
    }
    
    uint32_t len = strlen(tmp);
    if (size < len) len = size;
    memcpy(buffer, tmp, len);
    return len;
}

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

    // Register /dev/netinfo Char Device
    vfs_node_t *netinfo_node = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    if (netinfo_node) {
        memset(netinfo_node, 0, sizeof(vfs_node_t));
        kstrncpy(netinfo_node->name, "netinfo", 127);
        netinfo_node->flags = VFS_CHAR_DEVICE;
        netinfo_node->read = netinfo_read;
        devfs_register_device("netinfo", netinfo_node);
    }
}

void devfs_register_device(const char *name, vfs_node_t *device_node) {
    if (num_devices >= MAX_DEVICES) return;
    kstrncpy(device_node->name, name, 127);
    devices[num_devices++] = device_node;
}
