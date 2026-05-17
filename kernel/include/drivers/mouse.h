#ifndef MOUSE_H
#define MOUSE_H

#include <arch/isr.h>
#include <libk/types.h>


// Mouse Durum Yapısı
struct mouse_state {
  int x;
  int y;
  uint8_t buttons; // Bit 0: Sol, Bit 1: Sağ, Bit 2: Orta
};

void mouse_init(void);
struct mouse_state *mouse_get_state(void);
void mouse_draw_cursor(void);
void mouse_invalidate_cursor(void);

#endif
