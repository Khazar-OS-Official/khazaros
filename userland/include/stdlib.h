#ifndef KHAZAR_LIBC_STDLIB_H
#define KHAZAR_LIBC_STDLIB_H
#include <stddef.h>

void exit(int status);
int atoi(const char *str);
void *malloc(size_t size);
void free(void *ptr);

int waitpid(int pid, int *status);
int kill(int pid, int sig);

#endif // KHAZAR_LIBC_STDLIB_H
