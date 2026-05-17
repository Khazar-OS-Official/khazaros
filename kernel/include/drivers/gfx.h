#ifndef GFX_H
#define GFX_H

#include <libk/types.h>

// Renk yardımcıları (ARGB/RGB)
#define COLOR_BLACK 0x00000000
#define COLOR_WHITE 0x00FFFFFF
#define COLOR_RED 0x00FF0000
#define COLOR_GREEN 0x0000FF00
#define COLOR_BLUE 0x000000FF
#define COLOR_GRAY 0x00808080
#define COLOR_WINDOW 0x00C0C0C0

void gfx_init(void);
void gfx_put_pixel(int x, int y, uint32_t color);
void gfx_put_pixel_alpha(int x, int y, uint32_t color, uint8_t alpha);
uint32_t gfx_get_pixel(int x, int y);
void gfx_save_rect(uint32_t *dest, int x, int y, int w, int h);
void gfx_restore_rect(const uint32_t *src, int x, int y, int w, int h);
void gfx_draw_rect(int x, int y, int w, int h, uint32_t color);
void gfx_draw_rounded_rect(int x, int y, int w, int h, int radius,
                           uint32_t color);
void gfx_draw_gradient_rect(int x, int y, int w, int h, uint32_t color1,
                            uint32_t color2, bool vertical);
void gfx_draw_shadow(int x, int y, int w, int h, int radius);
void gfx_draw_line(int x1, int y1, int x2, int y2, uint32_t color);
void gfx_put_char(int x, int y, char c, uint32_t color);
void gfx_puts(int x, int y, const char *str, uint32_t color);
void gfx_clear(uint32_t color);
void gfx_swap_buffers(void);

#endif
