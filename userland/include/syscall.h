#ifndef KHAZAR_LIBC_SYSCALL_H
#define KHAZAR_LIBC_SYSCALL_H

#include <stdint.h>

#define SYS_EXIT 1
#define SYS_READ 3
#define SYS_WRITE 4
#define SYS_OPEN 5
#define SYS_CLOSE 6
#define SYS_EXEC 11
#define SYS_GETCMDLINE 12
#define SYS_GETDENTS 78
#define SYS_REBOOT 88
#define SYS_SHUTDOWN 89

#define SYS_UNLINK 16
#define SYS_MKDIR 17
#define SYS_CREATE 18
#define SYS_PS 19
#define SYS_CHMOD 15

#define SYS_WAITPID 7
#define SYS_KILL 37

static inline int syscall0(int num) {
  int ret;
  __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num));
  return ret;
}

static inline int syscall1(int num, uint32_t a1) {
  int ret;
  __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(a1));
  return ret;
}

static inline int syscall2(int num, uint32_t a1, uint32_t a2) {
  int ret;
  __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(a1), "c"(a2));
  return ret;
}

static inline int syscall3(int num, uint32_t a1, uint32_t a2, uint32_t a3) {
  int ret;
  __asm__ volatile("int $0x80"
                   : "=a"(ret)
                   : "a"(num), "b"(a1), "c"(a2), "d"(a3));
  return ret;
}

#endif // KHAZAR_LIBC_SYSCALL_H
