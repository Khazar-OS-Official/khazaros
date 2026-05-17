#include <drivers/pit.h>
#include <arch/idt.h>
#include <arch/io.h>
#include <arch/isr.h>

#define PIT_BASE_HZ 1193182
#define PIT_COMMAND 0x43
#define PIT_CHANNEL0 0x40

static volatile uint64_t pit_ticks = 0;
static volatile uint64_t pit_uptime_ms = 0;
static uint32_t pit_freq = 0;
static uint32_t pit_ms_base = 0;     // floor(1000/freq)
static uint32_t pit_ms_rem = 0;      // 1000%freq
static uint32_t pit_ms_rem_acc = 0;  // rem accumulator (0..freq-1)

static void pit_irq0_handler(registers_t *regs) {
  (void)regs;
  pit_ticks++;

  // Uptime hesabı: 64-bit bölmə istifadə etmədən (libgcc yoxdur).
  // Hər tick: +floor(1000/freq) ms, üstəlik qalıqları yığaraq dəqiqləşdir.
  pit_uptime_ms += pit_ms_base;
  pit_ms_rem_acc += pit_ms_rem;
  if (pit_ms_rem_acc >= pit_freq) {
    pit_uptime_ms += 1;
    pit_ms_rem_acc -= pit_freq;
  }
}

void pit_init(uint32_t freq_hz) {
  if (freq_hz == 0)
    freq_hz = 60;
  pit_freq = freq_hz;
  pit_ms_base = 1000 / pit_freq;
  pit_ms_rem = 1000 % pit_freq;
  pit_ms_rem_acc = 0;

  uint32_t divisor = PIT_BASE_HZ / freq_hz;
  if (divisor == 0)
    divisor = 1;
  if (divisor > 0xFFFF)
    divisor = 0xFFFF;

  // Mode 3 (square wave), lobyte/hibyte, channel 0
  outb(PIT_COMMAND, 0x36);
  outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
  outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

  // IRQ0 handler (INT 32)
  register_interrupt_handler(32, pit_irq0_handler);

  // PIC'te IRQ0 maskesini aç (bit0)
  uint8_t mask = inb(PIC1_DATA);
  mask &= ~(1 << 0);
  outb(PIC1_DATA, mask);
}

uint64_t pit_get_ticks(void) { return pit_ticks; }

uint64_t pit_get_uptime_ms(void) {
  return pit_uptime_ms;
}

void pit_sleep_ms(uint32_t ms) {
  if (pit_freq == 0 || ms == 0)
    return;

  // target_ticks = ceil(ms*freq/1000) – 64-bit bölmə olmadan
  uint32_t seconds = ms / 1000;
  uint32_t rem_ms = ms % 1000;
  uint32_t t1 = seconds * pit_freq;
  uint32_t t2 = (rem_ms * pit_freq + 999) / 1000; // ceil
  uint64_t target_ticks = (uint64_t)t1 + (uint64_t)t2;
  if (target_ticks == 0)
    target_ticks = 1;

  uint64_t start = pit_ticks;
  uint64_t end = start + target_ticks;
  while (pit_ticks < end) {
    __asm__ __volatile__("hlt");
  }
}

