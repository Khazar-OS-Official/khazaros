#include <drivers/serial.h>
#include <arch/io.h>
#include <libk/string.h>

#define COM1_BASE 0x3F8

#define COM_DATA(b)            (b)
#define COM_INTERRUPT_ENABLE(b)(b + 1)
#define COM_INTERRUPT_ID(b)    (b + 2)
#define COM_LINE_CONTROL(b)    (b + 3)
#define COM_MODEM_CONTROL(b)   (b + 4)
#define COM_LINE_STATUS(b)     (b + 5)
#define COM_MODEM_STATUS(b)    (b + 6)
#define COM_SCRATCH(b)         (b + 7)

// 115200 baud
#define COM_DIVISOR_LOW  0x01
#define COM_DIVISOR_HIGH 0x00

static bool serial_ready = false;

// ── Init ────────────────────────────────────────────────────────────────────
void serial_init(void) {
  outb(COM_INTERRUPT_ENABLE(COM1_BASE), 0x00);  // Interrupt'ları devre dışı

  // Baud rate (DLAB=1)
  outb(COM_LINE_CONTROL(COM1_BASE), 0x80);
  outb(COM_DATA(COM1_BASE),           COM_DIVISOR_LOW);
  outb(COM_INTERRUPT_ENABLE(COM1_BASE), COM_DIVISOR_HIGH);

  // 8N1, DLAB=0
  outb(COM_LINE_CONTROL(COM1_BASE), 0x03);

  // FIFO on, clear, 14-byte threshold
  outb(COM_INTERRUPT_ID(COM1_BASE), 0xC7);

  // RTS/DSR
  outb(COM_MODEM_CONTROL(COM1_BASE), 0x0B);

  serial_ready = true;
}

// ── Char göndər ─────────────────────────────────────────────────────────────
static void serial_wait_tx(void) {
  int timeout = 0xFFFF;
  while (!(inb(COM_LINE_STATUS(COM1_BASE)) & 0x20) && timeout-- > 0);
}

void serial_write_char(char c) {
  if (!serial_ready) return;
  if (c == '\n') {
    serial_wait_tx();
    outb(COM_DATA(COM1_BASE), '\r');
  }
  serial_wait_tx();
  outb(COM_DATA(COM1_BASE), (uint8_t)c);
}

void serial_write_string(const char *str) {
  while (*str) serial_write_char(*str++);
}

// ── Tam serial_kprintf (%d, %u, %x, %s, %c) ─────────────────────────────────
static void serial_write_uint(uint32_t v, int base) {
  char buf[12];
  int  len = 0;
  if (v == 0) { serial_write_char('0'); return; }
  while (v > 0) {
    uint32_t rem = v % (uint32_t)base;
    buf[len++] = (char)(rem < 10 ? '0' + rem : 'a' + rem - 10);
    v /= (uint32_t)base;
  }
  for (int i = len - 1; i >= 0; i--) serial_write_char(buf[i]);
}

static void serial_write_int(int32_t v) {
  if (v < 0) { serial_write_char('-'); v = -v; }
  serial_write_uint((uint32_t)v, 10);
}

// Sadə variadic — sadəcə işarəçi ötürür
// Tam va_list dəstəyi üçün libk/stdarg.h-ı ehtiva edirik
#include <stdarg.h>
void serial_kprintf(const char *fmt, ...) {
  if (!serial_ready) return;

  va_list args;
  va_start(args, fmt);

  while (*fmt) {
    if (*fmt != '%') {
      serial_write_char(*fmt++);
      continue;
    }
    fmt++;  // '%' keçir
    switch (*fmt) {
      case 'd': serial_write_int(va_arg(args, int));           break;
      case 'u': serial_write_uint(va_arg(args, uint32_t), 10); break;
      case 'x': serial_write_string("0x");
                serial_write_uint(va_arg(args, uint32_t), 16); break;
      case 's': { const char *s = va_arg(args, const char *);
                  serial_write_string(s ? s : "(null)");       break; }
      case 'c': serial_write_char((char)va_arg(args, int));    break;
      case '%': serial_write_char('%');                         break;
      default:  serial_write_char('%');
                serial_write_char(*fmt);                        break;
    }
    fmt++;
  }

  va_end(args);
}
