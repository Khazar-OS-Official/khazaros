#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>

int main(int argc, char **argv) {
  if (argc < 3) {
    printf("Usage: chmod <mode> <file>\n");
    return 1;
  }
  // Basit decimal parsing (octal desteklenmiyor su an)
  int mode = atoi(argv[1]);
  int ret = syscall2(SYS_CHMOD, (uint32_t)argv[2], (uint32_t)mode);
  if (ret < 0) {
    printf("chmod: cannot access '%s'\n", argv[2]);
    return 1;
  }
  return 0;
}
