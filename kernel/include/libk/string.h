#ifndef STRING_H
#define STRING_H

#include <libk/types.h>

// String length
static inline size_t strlen(const char *str) {
  size_t len = 0;
  while (str[len]) {
    len++;
  }
  return len;
}

// Memory set
static inline void *memset(void *dest, int val, size_t count) {
  uint32_t *d32 = (uint32_t *)dest;
  uint8_t v = (uint8_t)val;
  uint32_t v32 = v | (v << 8) | (v << 16) | (v << 24);
  size_t i = 0;

  // Set 4 bytes at a time
  size_t dwords = count / 4;
  for (; i < dwords; i++) {
    d32[i] = v32;
  }

  // Set remainder
  uint8_t *d8 = (uint8_t *)dest;
  for (i = i * 4; i < count; i++) {
    d8[i] = v;
  }
  return dest;
}

// Memory copy
static inline void *memcpy(void *dest, const void *src, size_t count) {
  uint32_t *d32 = (uint32_t *)dest;
  const uint32_t *s32 = (const uint32_t *)src;
  size_t i = 0;

  // Copy 4 bytes at a time
  size_t dwords = count / 4;
  for (; i < dwords; i++) {
    d32[i] = s32[i];
  }

  // Copy remainder
  uint8_t *d8 = (uint8_t *)dest;
  const uint8_t *s8 = (const uint8_t *)src;
  for (i = i * 4; i < count; i++) {
    d8[i] = s8[i];
  }
  return dest;
}

// Memory move (overlapping safe)
static inline void *memmove(void *dest, const void *src, size_t count) {
  uint8_t *d = (uint8_t *)dest;
  const uint8_t *s = (const uint8_t *)src;

  if (d < s) {
    for (size_t i = 0; i < count; i++) {
      d[i] = s[i];
    }
  } else {
    for (size_t i = count; i > 0; i--) {
      d[i - 1] = s[i - 1];
    }
  }
  return dest;
}

// String compare
static inline int strcmp(const char *s1, const char *s2) {
  while (*s1 && (*s1 == *s2)) {
    s1++;
    s2++;
  }
  return *(const uint8_t *)s1 - *(const uint8_t *)s2;
}

// String compare (limited)
static inline int strncmp(const char *s1, const char *s2, size_t n) {
  while (n-- && *s1 && (*s1 == *s2)) {
    s1++;
    s2++;
  }
  return n == (size_t)-1 ? 0 : *(const uint8_t *)s1 - *(const uint8_t *)s2;
}

// Memory compare
static inline int memcmp(const void *s1, const void *s2, size_t n) {
  const uint8_t *p1 = (const uint8_t *)s1;
  const uint8_t *p2 = (const uint8_t *)s2;
  for (size_t i = 0; i < n; i++) {
    if (p1[i] != p2[i]) {
      return p1[i] - p2[i];
    }
  }
  return 0;
}

// String copy
static inline char *strcpy(char *dest, const char *src) {
  char *d = dest;
  while ((*d++ = *src++))
    ;
  return dest;
}

// String search
static inline char *strstr(const char *haystack, const char *needle) {
  if (!*needle)
    return (char *)haystack;

  for (; *haystack; haystack++) {
    const char *h = haystack;
    const char *n = needle;

    while (*h && *n && (*h == *n)) {
      h++;
      n++;
    }

    if (!*n)
      return (char *)haystack;
  }

  return NULL;
}

// String copy (limited)
static inline char *kstrncpy(char *dest, const char *src, size_t n) {
  size_t i;
  for (i = 0; i < n && src[i] != '\0'; i++) {
    dest[i] = src[i];
  }
  for (; i < n; i++) {
    dest[i] = '\0';
  }
  return dest;
}

#endif // STRING_H
