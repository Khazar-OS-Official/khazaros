#include <stdio.h>
#include <syscall.h>

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  char buf[1024];
  buf[0] = '\0';
  int ret = syscall2(SYS_PS, (uint32_t)buf, sizeof(buf));
  if (ret < 0) {
    printf("ps: failed to read process list\n");
    return 1;
  }
  printf("  PID CMD\n");
  printf("%s", buf);
  return 0;
}
