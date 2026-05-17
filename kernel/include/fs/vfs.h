#ifndef VFS_H
#define VFS_H

#include <libk/types.h>

#define VFS_FILE 0x01
#define VFS_DIRECTORY 0x02
#define VFS_CHAR_DEVICE 0x03
#define VFS_BLOCK_DEVICE 0x04

// Permissions (POSIX-style)
#define S_IRUSR 00400 // Read, owner
#define S_IWUSR 00200 // Write, owner
#define S_IXUSR 00100 // Execute, owner
#define S_IRGRP 00040 // Read, group
#define S_IWGRP 00020 // Write, group
#define S_IXGRP 00010 // Execute, group
#define S_IROTH 00004 // Read, other
#define S_IWOTH 00002 // Write, other
#define S_IXOTH 00001 // Execute, other
#define S_IRWXU (S_IRUSR | S_IWUSR | S_IXUSR)
#define S_IRWXG (S_IRGRP | S_IWGRP | S_IXGRP)
#define S_IRWXO (S_IROTH | S_IWOTH | S_IXOTH)

struct vfs_node;

// VFS operations function pointers
typedef uint32_t (*vfs_read_func)(struct vfs_node *, uint32_t, uint32_t,
                                   uint8_t *);
typedef uint32_t (*vfs_write_func)(struct vfs_node *, uint32_t, uint32_t,
                                    uint8_t *);
typedef void (*vfs_open_func)(struct vfs_node *);
typedef void (*vfs_close_func)(struct vfs_node *);
typedef struct vfs_node *(*vfs_readdir_func)(struct vfs_node *, uint32_t);
typedef struct vfs_node *(*vfs_finddir_func)(struct vfs_node *, char *name);
typedef struct vfs_node *(*vfs_create_func)(struct vfs_node *, char *name);
typedef bool (*vfs_unlink_func)(struct vfs_node *, char *name);
typedef struct vfs_node *(*vfs_mkdir_func)(struct vfs_node *, char *name);

typedef struct vfs_node {
  char name[128];
  uint32_t mask;   // Permissions
  uint32_t uid;    // User ID
  uint32_t gid;    // Group ID
  uint32_t flags;  // Type (File, Dir, etc.)
  uint32_t inode;  // Filesystem-specific ID
  uint32_t length; // File size in bytes
  uint32_t impl;   // Implementation specific (e.g. cluster num)

  vfs_read_func read;
  vfs_write_func write;
  vfs_open_func open;
  vfs_close_func close;
  vfs_readdir_func readdir;
  vfs_finddir_func finddir;
  vfs_create_func create;
  vfs_unlink_func unlink;
  vfs_mkdir_func mkdir;

  struct vfs_node *ptr; // Used for mount points or symlinks
  void *device_data;    // Driver-specific data
} vfs_node_t;

// Standard I/O nodes
extern vfs_node_t *vfs_root;

// VFS functions
uint32_t vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size,
                  uint8_t *buffer);
uint32_t vfs_write(vfs_node_t *node, uint32_t offset, uint32_t size,
                   uint8_t *buffer);
void vfs_open(vfs_node_t *node);
void vfs_close(vfs_node_t *node);
struct vfs_node *vfs_readdir(vfs_node_t *node, uint32_t index);
struct vfs_node *vfs_finddir(vfs_node_t *node, char *name);
struct vfs_node *vfs_create(vfs_node_t *node, char *name);
bool vfs_unlink(vfs_node_t *node, char *name);
struct vfs_node *vfs_mkdir(vfs_node_t *node, char *name);
struct vfs_node *vfs_find_path(vfs_node_t *root, const char *path);

// Mount management
typedef struct {
  char path[128];
  vfs_node_t *root;
} vfs_mount_t;

#define MAX_MOUNTS 8
extern vfs_mount_t vfs_mounts[MAX_MOUNTS];

bool vfs_mount(const char *path, vfs_node_t *root_node);
vfs_node_t *vfs_check_mount(vfs_node_t *node);

#endif // VFS_H
