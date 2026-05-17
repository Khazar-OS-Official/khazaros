#include <drivers/vga.h>
#include <arch/io.h>
#include <libk/string.h>
#include <libk/types.h>

// Terminal state
static virtual_terminal_t default_terminal;
static virtual_terminal_t *active_terminal = &default_terminal;

// Donanim adresi yerine RAM uzerinde bir buffer tutuyoruz.
static uint16_t *terminal_buffer = default_terminal.buffer;

// Global erisim icin (wm.c okuyabilsin)
uint16_t *terminal_get_buffer(void) { return active_terminal->buffer; }

virtual_terminal_t *terminal_get_active(void) { return active_terminal; }
virtual_terminal_t *terminal_get_default(void) { return &default_terminal; }

size_t terminal_get_row(void) { return active_terminal->row; }
size_t terminal_get_column(void) { return active_terminal->column; }

void terminal_init_virtual(virtual_terminal_t *term) {
  if (!term) return;
  term->row = 0;
  term->column = 0;
  term->color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  for (size_t y = 0; y < VGA_HEIGHT; y++) {
    for (size_t x = 0; x < VGA_WIDTH; x++) {
      const size_t index = y * VGA_WIDTH + x;
      term->buffer[index] = vga_entry(' ', term->color);
    }
  }
}

void terminal_flush_to_hardware(void) {
  /* Aktif terminalin tam buffer'ini VGA donanim ekranina kopyala */
  for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
    ((uint16_t*)0xC00B8000)[i] = active_terminal->buffer[i];
  update_cursor((uint8_t)active_terminal->column, (uint8_t)active_terminal->row);
}

void terminal_set_active(virtual_terminal_t *term) {
  active_terminal = term ? term : &default_terminal;
  terminal_buffer = active_terminal->buffer;
  terminal_flush_to_hardware(); /* Pencere one getirilince icerik aninda gorsunsun */
}

// Terminal'i baslat - ekrani temizle
void terminal_initialize(void) {
  terminal_init_virtual(&default_terminal);
  terminal_set_active(&default_terminal);

  for (size_t y = 0; y < VGA_HEIGHT; y++) {
    for (size_t x = 0; x < VGA_WIDTH; x++) {
      const size_t index = y * VGA_WIDTH + x;
      terminal_buffer[index] = default_terminal.buffer[index];
      ((uint16_t*)0xC00B8000)[index] = terminal_buffer[index];
    }
  }

  update_cursor(0, 0);
}

// Terminal rengini ayarla
void terminal_setcolor(uint8_t color) { active_terminal->color = color; }

// Ekrani temizle
void terminal_clear(void) {
  for (size_t y = 0; y < VGA_HEIGHT; y++) {
    for (size_t x = 0; x < VGA_WIDTH; x++) {
      const size_t index = y * VGA_WIDTH + x;
      terminal_buffer[index] = vga_entry(' ', active_terminal->color);
      ((uint16_t*)0xC00B8000)[index] = terminal_buffer[index];
    }
  }
  active_terminal->row = 0;
  active_terminal->column = 0;
  update_cursor(0, 0);
}

// Ekrani bir satir yukari kaydira
void terminal_scroll(void) {
  bool is_active = (terminal_buffer == active_terminal->buffer);
  for (size_t y = 0; y < VGA_HEIGHT - 1; y++) {
    for (size_t x = 0; x < VGA_WIDTH; x++) {
      size_t src_index = (y + 1) * VGA_WIDTH + x;
      size_t dst_index = y * VGA_WIDTH + x;
      terminal_buffer[dst_index] = terminal_buffer[src_index];
      if (is_active)
        ((uint16_t*)0xC00B8000)[dst_index] = terminal_buffer[dst_index];
    }
  }

  for (size_t x = 0; x < VGA_WIDTH; x++) {
    size_t index = (VGA_HEIGHT - 1) * VGA_WIDTH + x;
    terminal_buffer[index] = vga_entry(' ', active_terminal->color);
    if (is_active)
      ((uint16_t*)0xC00B8000)[index] = terminal_buffer[index];
  }
}

// Hardware cursor pozisyonunu guncelle
void update_cursor(uint8_t x, uint8_t y) {
  uint16_t pos = y * VGA_WIDTH + x;

  outb(0x3D4, 0x0F);
  outb(0x3D5, (uint8_t)(pos & 0xFF));

  outb(0x3D4, 0x0E);
  outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

// Belirli pozisyona karakter yaz
static void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) {
  const size_t index = y * VGA_WIDTH + x;
  terminal_buffer[index] = vga_entry(c, color);
  // Yalnizca aktif terminalin buffer'ina yaziliyorsa donanimi guncelle
  if (terminal_buffer == active_terminal->buffer)
    ((uint16_t*)0xC00B8000)[index] = terminal_buffer[index];
}

// Tek karakter yaz (newline destegi ile)
void terminal_putchar(char c) {
  // Backspace karakteri
  if (c == '\b') {
    if (active_terminal->column > 0) {
      active_terminal->column--;
    } else if (active_terminal->row > 0) {
      active_terminal->row--;
      active_terminal->column = VGA_WIDTH - 1;
    }
    terminal_putentryat(' ', active_terminal->color, active_terminal->column,
                        active_terminal->row);
    update_cursor((uint8_t)active_terminal->column, (uint8_t)active_terminal->row);
    return;
  }

  // Newline karakteri
  if (c == '\n') {
    active_terminal->column = 0;
    if (++active_terminal->row == VGA_HEIGHT) {
      terminal_scroll();
      active_terminal->row = VGA_HEIGHT - 1;
    }
    update_cursor((uint8_t)active_terminal->column, (uint8_t)active_terminal->row);
    return;
  }

  // Tab karakteri (4 space)
  if (c == '\t') {
    active_terminal->column = (active_terminal->column + 4) & ~(4 - 1);
    if (active_terminal->column >= VGA_WIDTH) {
      active_terminal->column = 0;
      if (++active_terminal->row == VGA_HEIGHT) {
        terminal_scroll();
        active_terminal->row = VGA_HEIGHT - 1;
      }
    }
    update_cursor((uint8_t)active_terminal->column, (uint8_t)active_terminal->row);
    return;
  }

  // Normal karakter
  terminal_putentryat(c, active_terminal->color, active_terminal->column,
                      active_terminal->row);

  if (++active_terminal->column == VGA_WIDTH) {
    active_terminal->column = 0;
    if (++active_terminal->row == VGA_HEIGHT) {
      terminal_scroll();
      active_terminal->row = VGA_HEIGHT - 1;
    }
  }

  update_cursor((uint8_t)active_terminal->column, (uint8_t)active_terminal->row);

  // Debug: Seri porta da gonder
  extern void serial_write_char(char c);
  serial_write_char(c);
}

// Belirli uzunlukta veri yaz
void terminal_write(const char *data, size_t size) {
  for (size_t i = 0; i < size; i++) {
    terminal_putchar(data[i]);
  }
}

// String yaz (null-terminated)
void terminal_writestring(const char *data) {
  size_t len = strlen(data);
  terminal_write(data, len);
}

// Helper: Integer to string (decimal)
static void itoa(int num, char *str) {
  int i = 0;
  bool is_negative = false;

  if (num == 0) {
    str[i++] = '0';
    str[i] = '\0';
    return;
  }

  if (num < 0) {
    is_negative = true;
    num = -num;
  }

  while (num != 0) {
    int rem = num % 10;
    str[i++] = rem + '0';
    num = num / 10;
  }

  if (is_negative) {
    str[i++] = '-';
  }

  str[i] = '\0';

  int start = 0;
  int end = i - 1;
  while (start < end) {
    char temp = str[start];
    str[start] = str[end];
    str[end] = temp;
    start++;
    end--;
  }
}

// Helper: Integer to hex string (Full 32-bit - 8 chars)
static void itoh(uint32_t num, char *str) {
  const char hex_chars[] = "0123456789ABCDEF";
  for (int i = 7; i >= 0; i--) {
    str[i] = hex_chars[num & 0xF];
    num >>= 4;
  }
  str[8] = '\0';
}

// Core formatting engine (shared by kprintf, ksprintf, ksnprintf)
static void kvprintf(void (*putc_func)(char, void *), void *extra,
                     const char *format, __builtin_va_list args) {
  for (size_t i = 0; format[i] != '\0'; i++) {
    if (format[i] == '%' && format[i + 1] != '\0') {
      i++;
      while (format[i] >= '0' && format[i] <= '9')
        i++;
      switch (format[i]) {
      case 'd': {
        int num = __builtin_va_arg(args, int);
        char buffer[32];
        itoa(num, buffer);
        for (int j = 0; buffer[j]; j++)
          putc_func(buffer[j], extra);
        break;
      }
      case 'x': {
        uint32_t num = __builtin_va_arg(args, uint32_t);
        char buffer[32];
        itoh(num, buffer);
        for (int j = 0; buffer[j]; j++)
          putc_func(buffer[j], extra);
        break;
      }
      case 'l': {
        if (format[i + 1] == 'l' && format[i + 2] == 'x') {
          i += 2;
          uint64_t num = __builtin_va_arg(args, uint64_t);
          char buffer[32];
          const char hex_chars[] = "0123456789ABCDEF";
          int pos = 15;
          buffer[16] = '\0';
          for (int j = 0; j < 16; j++) {
            buffer[pos--] = hex_chars[num & 0xF];
            num >>= 4;
          }
          for (int j = 0; buffer[j]; j++)
            putc_func(buffer[j], extra);
          break;
        }
        break; // Unsupported %l formats
      }
      case 's': {
        const char *str = __builtin_va_arg(args, const char *);
        if (!str) str = "(null)";
        for (int j = 0; str[j]; j++)
          putc_func(str[j], extra);
        break;
      }
      case 'c': {
        char ch = (char)__builtin_va_arg(args, int);
        putc_func(ch, extra);
        break;
      }
      case '%':
        putc_func('%', extra);
        break;
      default:
        putc_func('%', extra);
        putc_func(format[i], extra);
        break;
      }
    } else {
      putc_func(format[i], extra);
    }
  }
}

static void kprintf_putc(char c, void *extra) {
  (void)extra;
  terminal_putchar(c);
}

void kprintf(const char *format, ...) {
  __builtin_va_list args;
  __builtin_va_start(args, format);
  kvprintf(kprintf_putc, NULL, format, args);
  __builtin_va_end(args);
}

static void ksprintf_putc(char c, void *extra) {
  char **buf_ptr = (char **)extra;
  **buf_ptr = c;
  (*buf_ptr)++;
}

void ksprintf(char *str, const char *format, ...) {
  __builtin_va_list args;
  __builtin_va_start(args, format);
  char *buf_ptr = str;
  kvprintf(ksprintf_putc, &buf_ptr, format, args);
  *buf_ptr = '\0';
  __builtin_va_end(args);
}

/* ksnprintf: boyut sinirli guvenli format yazici — her zaman null-terminate eder */
typedef struct { char *p; size_t remaining; } ksnprintf_ctx_t;

static void ksnprintf_putc(char c, void *extra) {
  ksnprintf_ctx_t *ctx = (ksnprintf_ctx_t *)extra;
  if (ctx->remaining > 1) {
    *ctx->p++ = c;
    ctx->remaining--;
  }
}

void ksnprintf(char *str, size_t n, const char *format, ...) {
  if (!str || n == 0) return;
  __builtin_va_list args;
  __builtin_va_start(args, format);
  ksnprintf_ctx_t ctx = { str, n };
  kvprintf(ksnprintf_putc, &ctx, format, args);
  *ctx.p = '\0';
  __builtin_va_end(args);
}
