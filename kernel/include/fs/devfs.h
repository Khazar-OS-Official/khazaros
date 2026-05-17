#ifndef DEVFS_H
#define DEVFS_H

#include <fs/vfs.h>

void devfs_init(void);
void devfs_register_device(const char *name, vfs_node_t *device_node);

#endif // DEVFS_H
