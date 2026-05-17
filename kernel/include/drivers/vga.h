#ifndef VGA_H
#define VGA_H

#include <libk/types.h>

// VGA text mode sabitleri
#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xC00B8000

// VGA renk kodlari (4-bit)
enum vga_color {
  VGA_COLOR_BLACK        = 0,
  VGA_COLOR_BLUE         = 1,
  VGA_COLOR_GREEN        = 2,
  VGA_COLOR_CYAN         = 3,
  VGA_COLOR_RED          = 4,
  VGA_COLOR_MAGENTA      = 5,
  VGA_COLOR_BROWN        = 6,
  VGA_COLOR_LIGHT_GREY   = 7,
  VGA_COLOR_DARK_GREY    = 8,
  VGA_COLOR_LIGHT_BLUE   = 9,
  VGA_COLOR_LIGHT_GREEN  = 10,
  VGA_COLOR_LIGHT_CYAN   = 11,
  VGA_COLOR_LIGHT_RED    = 12,
  VGA_COLOR_LIGHT_MAGENTA= 13,
  VGA_COLOR_LIGHT_BROWN  = 14,
  VGA_COLOR_WHITE        = 15,
};

// VGA attribute byte olustur (background << 4 | foreground)
static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
  return fg | bg << 4;
}

// VGA entry olustur (karakter + attribute)
static inline uint16_t vga_entry(unsigned char c, uint8_t color) {
  return (uint16_t)c | (uint16_t)color << 8;
}

// ── Virtual Terminal ───────────────────────────────────────────────────────
typedef struct virtual_terminal {
  size_t   row;
  size_t   column;
  uint8_t  color;
  uint16_t buffer[VGA_HEIGHT * VGA_WIDTH];
} virtual_terminal_t;

// ── Temel Terminal Fonksiyonlari ──────────────────────────────────────────
void terminal_initialize(void);
void terminal_setcolor(uint8_t color);
void terminal_putchar(char c);
void terminal_write(const char *data, size_t size);
void terminal_writestring(const char *data);
void terminal_clear(void);
void terminal_scroll(void);
void update_cursor(uint8_t x, uint8_t y);

// ── Virtual Terminal API ──────────────────────────────────────────────────
void               terminal_init_virtual(virtual_terminal_t *term);
void               terminal_set_active(virtual_terminal_t *term);
void               terminal_flush_to_hardware(void);   // aktif VT -> donanim
virtual_terminal_t *terminal_get_active(void);
virtual_terminal_t *terminal_get_default(void);
uint16_t           *terminal_get_buffer(void);
size_t              terminal_get_row(void);
size_t              terminal_get_column(void);

// ── Format Yazicilar ──────────────────────────────────────────────────────
void kprintf(const char *format, ...);
void ksprintf(char *str, const char *format, ...);
void ksnprintf(char *str, size_t n, const char *format, ...);  /* boyut sinirli */

#endif // VGA_H
