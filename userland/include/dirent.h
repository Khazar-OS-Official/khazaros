#ifndef KHAZAR_LIBC_DIRENT_H
#define KHAZAR_LIBC_DIRENT_H

#include <stdint.h>

#define MAX_NAME_LENGTH 128

typedef struct {
  uint32_t d_ino;
  uint32_t d_off; // File size
  uint16_t d_reclen;
  uint16_t d_mode;      // File permissions (mask)
  unsigned char d_type; // VFS_FILE or VFS_DIRECTORY flag
  char d_name[MAX_NAME_LENGTH];
} dirent_t;

// Minimal DIR structure for userland
typedef struct {
  int fd;
  int index;
  dirent_t current;
} DIR;

DIR *opendir(const char *name);
dirent_t *readdir(DIR *dirp);
int closedir(DIR *dirp);

#endif
