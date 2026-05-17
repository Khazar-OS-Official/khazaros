#include <stdlib.h>
#include <syscall.h>

// Evades the undefined reference to `__main` inherently added by MinGW gcc
void __main(void) {}

void exit(int status) {
  syscall1(SYS_EXIT, (uint32_t)status);
  while (1)
    ; // Should not reach here
}

int atoi(const char *str) {
  int res = 0;
  int sign = 1;
  int i = 0;

  if (str[0] == '-') {
    sign = -1;
    i++;
  } else if (str[0] == '+') {
    i++;
  }

  for (; str[i] != '\0'; ++i) {
    if (str[i] >= '0' && str[i] <= '9') {
      res = res * 10 + str[i] - '0';
    } else {
      break;
    }
  }

  return sign * res;
}

// Basic heap for userland (64KB static buffer)
static uint8_t user_heap[64 * 1024];
static uint32_t heap_offset = 0;

void *malloc(size_t size) {
  if (size == 0 || heap_offset + size > sizeof(user_heap)) {
    return NULL;
  }
  void *ptr = &user_heap[heap_offset];
  heap_offset += size;
  return ptr;
}

void free(void *ptr) {
  // Very basic: do nothing. (Not a real allocator yet)
  (void)ptr;
}

int waitpid(int pid, int *status) {
  return syscall2(SYS_WAITPID, (uint32_t)pid, (uint32_t)status);
}

int kill(int pid, int sig) {
  return syscall2(SYS_KILL, (uint32_t)pid, (uint32_t)sig);
}
