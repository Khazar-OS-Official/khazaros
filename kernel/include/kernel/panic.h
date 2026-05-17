#ifndef PANIC_H
#define PANIC_H

#include <libk/types.h>

// Kernel panic - sistem cokmesi
void kernel_panic(const char *message, const char *file, uint32_t line);

// Son aktif subsystem'i kaydet (panic raporunda gosterilir)
// Her buyuk init adiminin basinda cagir: panic_set_subsystem("ahci");
void panic_set_subsystem(const char *name);

// Macro - dosya ve satır bilgisi ile panic
#define PANIC(msg) kernel_panic(msg, __FILE__, __LINE__)

// Assert macro
#define ASSERT(condition)                                                      \
  if (!(condition)) {                                                          \
    kernel_panic("Assertion failed: " #condition, __FILE__, __LINE__);         \
  }

#endif // PANIC_H
