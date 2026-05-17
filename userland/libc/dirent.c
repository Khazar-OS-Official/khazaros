#include <dirent.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>


DIR *opendir(const char *name) {
  int fd = syscall1(SYS_OPEN, (uint32_t)name);
  if (fd < 0)
    return NULL;

  DIR *dirp = (DIR *)malloc(sizeof(DIR));
  if (!dirp) {
    syscall1(SYS_CLOSE, fd);
    return NULL;
  }

  dirp->fd = fd;
  dirp->index = 0;
  return dirp;
}

dirent_t *readdir(DIR *dirp) {
  if (!dirp)
    return NULL;

  // Call GETDENTS with: FD, return buffer, and current index
  int ret =
      syscall3(SYS_GETDENTS, dirp->fd, (uint32_t)&dirp->current, dirp->index);
  if (ret == 1) {
    dirp->index++;
    return &dirp->current;
  }

  return NULL; // End of directory or error
}

int closedir(DIR *dirp) {
  if (!dirp)
    return -1;
  int ret = syscall1(SYS_CLOSE, dirp->fd);
  free(dirp);
  return ret;
}
