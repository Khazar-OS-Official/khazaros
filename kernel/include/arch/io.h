#ifndef IO_H
#define IO_H

#include <libk/types.h>

// I/O Port Operations
// x86'da donanımla iletişim için IN/OUT instruction'ları kullanılır

// Byte (8-bit) yaz
static inline void outb(uint16_t port, uint8_t value) {
  __asm__ __volatile__("outb %0, %1" : : "a"(value), "Nd"(port));
}

// Byte (8-bit) oku
static inline uint8_t inb(uint16_t port) {
  uint8_t ret;
  __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

// Word (16-bit) yaz
static inline void outw(uint16_t port, uint16_t value) {
  __asm__ __volatile__("outw %0, %1" : : "a"(value), "Nd"(port));
}

// Word (16-bit) oku
static inline uint16_t inw(uint16_t port) {
  uint16_t ret;
  __asm__ __volatile__("inw %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

// Dword (32-bit) yaz
static inline void outl(uint16_t port, uint32_t value) {
  __asm__ __volatile__("outl %0, %1" : : "a"(value), "Nd"(port));
}

// Dword (32-bit) oku
static inline uint32_t inl(uint16_t port) {
  uint32_t ret;
  __asm__ __volatile__("inl %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

// I/O wait - Eski donanımlar için delay
static inline void io_wait(void) {
  // Port 0x80'e yazma - unused port, ~1μs delay
  outb(0x80, 0);
}

#endif // IO_H
