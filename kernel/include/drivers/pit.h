#ifndef PIT_H
#define PIT_H

#include <libk/types.h>

// PIT (8253/8254) timer – IRQ0 / INT 32
void pit_init(uint32_t freq_hz);

// Monotonik tick sayacı (PIT interrupt-ları ilə artır)
uint64_t pit_get_ticks(void);

// Təxmini uptime (ms)
uint64_t pit_get_uptime_ms(void);

// Interrupt-lar AÇIQ olmalıdır. HLT ilə “sleep”.
void pit_sleep_ms(uint32_t ms);

#endif // PIT_H

