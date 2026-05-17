#include <stdio.h>
#include <syscall.h>

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: touch <file>\n");
    return 1;
  }
  int ret = syscall1(SYS_CREATE, (uint32_t)argv[1]);
  if (ret < 0) {
    printf("touch: cannot touch '%s'\n", argv[1]);
    return 1;
  }
  return 0;
}
