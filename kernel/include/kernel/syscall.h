#ifndef SYSCALL_H
#define SYSCALL_H

#include <arch/isr.h>
#include <libk/types.h>

// Syscall Numaraları (x86_32 standardına yakın)
#define SYS_EXIT 1
#define SYS_READ 3
#define SYS_WRITE 4
#define SYS_OPEN 5
#define SYS_CLOSE 6
#define SYS_EXEC 11
#define SYS_GETDENTS 78
#define SYS_REBOOT 88
#define SYS_SHUTDOWN 89
#define SYS_CHMOD 15

// Fonksiyonlar
void syscall_init(void);
void syscall_handler(registers_t *regs);

#endif // SYSCALL_H
