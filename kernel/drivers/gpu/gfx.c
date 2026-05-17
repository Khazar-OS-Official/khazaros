#include <drivers/gfx.h>
#include <libk/font.h>
#include <mm/kheap.h>
#include <libk/string.h>
#include <drivers/vbe.h>
#include <drivers/tga.h>

static struct vbe_info *vbe;
static uint32_t *backbuffer = NULL;
static uint32_t *frontbuffer = NULL; // Ekrandakı mövcud pikselləri izləmək üçün
static uint32_t buffer_size = 0;

void gfx_init(void) {
  vbe = vbe_get_info();
  if (!vbe->lfb)
    return;

  // Double buffering için backbuffer ve frontbuffer ayır
  buffer_size = vbe->width * vbe->height * sizeof(uint32_t);
  backbuffer = (uint32_t *)kmalloc(buffer_size);
  frontbuffer = (uint32_t *)kmalloc(buffer_size);

  if (backbuffer) {
    memset(backbuffer, 0, buffer_size);
  }
  if (frontbuffer) {
    memset(frontbuffer, 0, buffer_size);
  }

  // Debug: Ekranin acildigini kanitlamak ucun maviye boya ve swap et
  gfx_clear(0x000A1628); // Dark blue
  gfx_swap_buffers();
}

void gfx_put_pixel(int x, int y, uint32_t color) {
  if (!backbuffer || x < 0 || x >= (int)vbe->width || y < 0 || y >= (int)vbe->height)
    return;

  // Backbuffera çiz
  backbuffer[y * vbe->width + x] = color;
}

void gfx_put_pixel_alpha(int x, int y, uint32_t color, uint8_t alpha) {
  if (!backbuffer || x < 0 || x >= (int)vbe->width || y < 0 || y >= (int)vbe->height)
    return;

  if (alpha == 255) {
    backbuffer[y * vbe->width + x] = color;
    return;
  }
  if (alpha == 0) return;

  uint32_t bg = backbuffer[y * vbe->width + x];
  
  uint32_t rb = (((color & 0x00FF00FF) * alpha) + ((bg & 0x00FF00FF) * (255 - alpha))) >> 8;
  uint32_t g  = (((color & 0x0000FF00) * alpha) + ((bg & 0x0000FF00) * (255 - alpha))) >> 8;
  
  backbuffer[y * vbe->width + x] = (rb & 0x00FF00FF) | (g & 0x0000FF00);
}

void gfx_draw_shadow(int x, int y, int w, int h, int r) {
  // Basit drop shadow (kenarlarda alpha gradyanı)
  for (int j = -r; j < h + r; j++) {
    for (int i = -r; i < w + r; i++) {
      if (i >= 0 && i < w && j >= 0 && j < h) continue; // İç kısmını boş bırak (performans)
      
      int dx = (i < 0) ? -i : (i >= w ? i - w + 1 : 0);
      int dy = (j < 0) ? -j : (j >= h ? j - h + 1 : 0);
      int dist = dx > dy ? dx : dy; // Chebyshev distance as approximation
      
      if (dist <= r) {
        uint8_t alpha = (uint8_t)(100 * (r - dist) / r); // Max 100 alpha shadow
        gfx_put_pixel_alpha(x + i + 4, y + j + 4, 0x000000, alpha); // 4px offset shadow
      }
    }
  }
}

uint32_t gfx_get_pixel(int x, int y) {
  if (x < 0 || x >= (int)vbe->width || y < 0 || y >= (int)vbe->height)
    return 0;
  return backbuffer[y * vbe->width + x];
}

void gfx_save_rect(uint32_t *dest, int x, int y, int w, int h) {
  if (!dest)
    return;
  for (int row = 0; row < h; row++) {
    int sy = y + row;
    if (sy < 0 || sy >= (int)vbe->height)
      continue;
    for (int col = 0; col < w; col++) {
      int sx = x + col;
      if (sx >= 0 && sx < (int)vbe->width)
        dest[row * w + col] = backbuffer[sy * vbe->width + sx];
      else
        dest[row * w + col] = 0;
    }
  }
}

void gfx_restore_rect(const uint32_t *src, int x, int y, int w, int h) {
  if (!src)
    return;
  for (int row = 0; row < h; row++) {
    int dy = y + row;
    if (dy < 0 || dy >= (int)vbe->height)
      continue;
    for (int col = 0; col < w; col++) {
      int dx = x + col;
      if (dx >= 0 && dx < (int)vbe->width)
        backbuffer[dy * vbe->width + dx] = src[row * w + col];
    }
  }
}

void gfx_clear(uint32_t color) {
  if (!backbuffer) return;
  // Tüm bufferı tek seferde boya
  for (uint32_t i = 0; i < vbe->width * vbe->height; i++) {
    backbuffer[i] = color;
  }
}

void gfx_draw_rect(int x, int y, int w, int h, uint32_t color) {
  for (int i = 0; i < h; i++) {
    for (int j = 0; j < w; j++) {
      gfx_put_pixel(x + j, y + i, color);
    }
  }
}

void gfx_draw_rounded_rect(int x, int y, int w, int h, int r, uint32_t color) {
  if (r <= 0) {
    gfx_draw_rect(x, y, w, h, color);
    return;
  }

  if (r > w / 2)
    r = w / 2;
  if (r > h / 2)
    r = h / 2;

  // Draw 5 rectangular areas (center cross)
  gfx_draw_rect(x + r, y, w - 2 * r, r, color);         // Top
  gfx_draw_rect(x + r, y + h - r, w - 2 * r, r, color); // Bottom
  gfx_draw_rect(x, y + r, w, h - 2 * r, color);         // Center horizontal

  // Draw 4 corners with solid coverage inward
  for (int j = 0; j < r; j++) {
    for (int i = 0; i < r; i++) {
      int dist_sq = (i - r) * (i - r) + (j - r) * (j - r);
      if (dist_sq <= r * r) {
        gfx_put_pixel(x + i, y + j, color);                 // Top-left
        gfx_put_pixel(x + w - 1 - i, y + j, color);         // Top-right
        gfx_put_pixel(x + i, y + h - 1 - j, color);         // Bottom-left
        gfx_put_pixel(x + w - 1 - i, y + h - 1 - j, color); // Bottom-right
      }
    }
  }

  // Cover the inner corner boxes (rectangles between the arcs and center cross)
  // These are basically j in [0, r] but i just draws what's left.
  // Actually, the center cross and loops above should be enough if implemented
  // tightly.
}

void gfx_draw_gradient_rect(int x, int y, int w, int h, uint32_t color1,
                            uint32_t color2, bool vertical) {
  uint32_t r1 = (color1 >> 16) & 0xFF;
  uint32_t g1 = (color1 >> 8) & 0xFF;
  uint32_t b1 = color1 & 0xFF;

  uint32_t r2 = (color2 >> 16) & 0xFF;
  uint32_t g2 = (color2 >> 8) & 0xFF;
  uint32_t b2 = color2 & 0xFF;

  if (vertical) {
    for (int j = 0; j < h; j++) {
      uint32_t r = r1 + (r2 - r1) * j / h;
      uint32_t g = g1 + (g2 - g1) * j / h;
      uint32_t b = b1 + (b2 - b1) * j / h;
      uint32_t color = (r << 16) | (g << 8) | b;
      for (int i = 0; i < w; i++) {
        gfx_put_pixel(x + i, y + j, color);
      }
    }
  } else {
    for (int i = 0; i < w; i++) {
      uint32_t r = r1 + (r2 - r1) * i / w;
      uint32_t g = g1 + (g2 - g1) * i / w;
      uint32_t b = b1 + (b2 - b1) * i / w;
      uint32_t color = (r << 16) | (g << 8) | b;
      for (int j = 0; j < h; j++) {
        gfx_put_pixel(x + i, y + j, color);
      }
    }
  }
}

void gfx_draw_line(int x1, int y1, int x2, int y2, uint32_t color) {
  int dx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
  int dy = (y2 > y1) ? (y2 - y1) : (y1 - y2);
  int sx = (x1 < x2) ? 1 : -1;
  int sy = (y1 < y2) ? 1 : -1;
  int err = dx - dy;

  while (1) {
    gfx_put_pixel(x1, y1, color);
    if (x1 == x2 && y1 == y2)
      break;
    int e2 = 2 * err;
    if (e2 > -dy) {
      err -= dy;
      x1 += sx;
    }
    if (e2 < dx) {
      err += dx;
      y1 += sy;
    }
  }
}

void gfx_put_char(int x, int y, char c, uint32_t color) {
  if ((unsigned char)c < 32 || (unsigned char)c > 127)
    return;

  uint8_t *glyph = font_basic_8x8[c - 32];
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      if (glyph[i] & (0x80 >> j)) {
        gfx_put_pixel(x + j, y + i, color);
      }
    }
  }
}

void gfx_puts(int x, int y, const char *str, uint32_t color) {
  int cur_x = x;
  while (*str) {
    if (*str == '\n') {
      y += 8;
      cur_x = x;
    } else {
      gfx_put_char(cur_x, y, *str, color);
      cur_x += 8;
    }
    str++;
  }
}

void gfx_swap_buffers(void) {
  if (!backbuffer || !frontbuffer || !vbe->lfb)
    return;

  // Sadece dəyişən pikselləri VRAM-a kopyala (Damage rect optimization)
  // VRAM-a (PCI bus) yazmaq çox yavaşdır, amma RAM (frontbuffer) oxumaq sürətlidir.
  uint32_t total_pixels = vbe->width * vbe->height;
  
  if (vbe->pitch == vbe->width * sizeof(uint32_t)) {
    // Sürətli ardıcıl kopyalama
    for (uint32_t i = 0; i < total_pixels; i++) {
      if (backbuffer[i] != frontbuffer[i]) {
        vbe->lfb[i] = backbuffer[i];
        frontbuffer[i] = backbuffer[i];
      }
    }
  } else {
    // Pitch offset ilə kopyalama
    uint32_t *fb_dest = (uint32_t *)vbe->lfb;
    uint32_t lfb_pitch_words = vbe->pitch / sizeof(uint32_t);
    uint32_t i = 0;

    for (uint32_t y = 0; y < vbe->height; y++) {
      for (uint32_t x = 0; x < vbe->width; x++, i++) {
        if (backbuffer[i] != frontbuffer[i]) {
          fb_dest[y * lfb_pitch_words + x] = backbuffer[i];
          frontbuffer[i] = backbuffer[i];
        }
      }
    }
  }
}

void gfx_draw_tga(int x, int y, uint8_t *data) {
    if (!data) return;
    tga_header_t *header = (tga_header_t *)data;
    if (header->image_type != 2 && header->image_type != 10) return; 

    uint8_t *pixels = data + sizeof(tga_header_t) + header->id_length;
    int bpp = header->bits_per_pixel / 8;
    
    for (int j = 0; j < header->height; j++) {
        for (int i = 0; i < header->width; i++) {
            int rj = (header->image_descriptor & 0x20) ? j : (header->height - 1 - j);
            uint8_t *p = pixels + (j * header->width + i) * bpp;
            uint32_t color = 0;
            if (bpp == 3) {
                color = (p[2] << 16) | (p[1] << 8) | p[0];
                gfx_put_pixel(x + i, y + rj, color);
            } else if (bpp == 4) {
                color = (p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0];
                gfx_put_pixel_alpha(x + i, y + rj, color & 0xFFFFFF, p[3]);
            }
        }
    }
}
