#include <fs/vfs.h>
#include <libk/types.h>
#include <libk/string.h>

vfs_node_t *vfs_root = 0;

uint32_t vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size,
                  uint8_t *buffer) {
  if (!node)
    return 0;
  if (node->read != 0) {
    return node->read(node, offset, size, buffer);
  }
  return 0;
}

uint32_t vfs_write(vfs_node_t *node, uint32_t offset, uint32_t size,
                   uint8_t *buffer) {
  if (!node)
    return 0;
  if (node->write != 0) {
    return node->write(node, offset, size, buffer);
  }
  return 0;
}

void vfs_open(vfs_node_t *node) {
  if (!node)
    return;
  if (node->open != 0) {
    node->open(node);
  }
}

void vfs_close(vfs_node_t *node) {
  if (!node)
    return;
  if (node->close != 0) {
    node->close(node);
  }
}

struct vfs_node *vfs_readdir(vfs_node_t *node, uint32_t index) {
  if (!node)
    return 0;
  if ((node->flags & 0x07) == VFS_DIRECTORY && node->readdir != 0) {
    return node->readdir(node, index);
  }
  return 0;
}

struct vfs_node *vfs_finddir(vfs_node_t *node, char *name) {
  if (!node)
    return 0;
  if ((node->flags & 0x07) == VFS_DIRECTORY && node->finddir != 0) {
    return node->finddir(node, name);
  }
  return 0;
}

struct vfs_node *vfs_create(vfs_node_t *node, char *name) {
  if (!node)
    return 0;
  if ((node->flags & 0x07) == VFS_DIRECTORY && node->create != 0) {
    return node->create(node, name);
  }
  return 0;
}

bool vfs_unlink(vfs_node_t *node, char *name) {
  if (!node)
    return false;
  if ((node->flags & 0x07) == VFS_DIRECTORY && node->unlink != 0) {
    return node->unlink(node, name);
  }
  return false;
}

struct vfs_node *vfs_mkdir(vfs_node_t *node, char *name) {
  if (!node)
    return 0;
  if ((node->flags & 0x07) == VFS_DIRECTORY && node->mkdir != 0) {
    return node->mkdir(node, name);
  }
  return 0;
}

vfs_mount_t vfs_mounts[MAX_MOUNTS];

bool vfs_mount(const char *path, vfs_node_t *root_node) {
  for (int i = 0; i < MAX_MOUNTS; i++) {
    if (vfs_mounts[i].path[0] == '\0') {
      kstrncpy(vfs_mounts[i].path, path, 127);
      vfs_mounts[i].root = root_node;

      // Also mark the target node in the parent FS if possible
      vfs_node_t *target = vfs_find_path(vfs_root, path);
      if (target) {
        target->ptr = root_node;
      }
      return true;
    }
  }
  return false;
}

vfs_node_t *vfs_check_mount(vfs_node_t *node) {
  if (!node)
    return NULL;
  if (node->ptr)
    return node->ptr; // If ptr is set, we follow it to the mounted FS
  return node;
}

// Path traversal (recursive helper)
vfs_node_t *vfs_find_path(vfs_node_t *root, const char *path) {
  if (!path || path[0] == '\0')
    return vfs_check_mount(root);

  // Handle absolute path starting with /
  if (path[0] == '/') {
    path++;
    if (path[0] == '\0')
      return vfs_check_mount(vfs_root);
    root = vfs_root;
  }

  if (!root)
    return NULL;

  char component[128];
  int i = 0;
  while (path[i] != '/' && path[i] != '\0' && i < 127) {
    component[i] = path[i];
    i++;
  }
  component[i] = '\0';

  vfs_node_t *next = vfs_finddir(vfs_check_mount(root), component);
  if (!next)
    return NULL;

  if (path[i] == '/') {
    // There is more to traverse
    return vfs_find_path(next, path + i + 1);
  }

  return vfs_check_mount(next);
}
