#ifndef KHAZAR_LIBC_STRING_H
#define KHAZAR_LIBC_STRING_H

#include <stddef.h>

size_t strlen(const char *str);
char *strcpy(char *dest, const char *src);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);

void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);

#endif // KHAZAR_LIBC_STRING_H
