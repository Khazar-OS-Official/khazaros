#ifndef KHAZAR_LIBC_STDIO_H
#define KHAZAR_LIBC_STDIO_H

#include <stddef.h>
#include <stdint.h>


int puts(const char *s);
int printf(const char *format, ...);

int fopen(const char *name);
int fclose(int fd);
int fread(int fd, void *buffer, uint32_t size);
int fwrite(int fd, const void *buffer, uint32_t size);

#endif // KHAZAR_LIBC_STDIO_H
