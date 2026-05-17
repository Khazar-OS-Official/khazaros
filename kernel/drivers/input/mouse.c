#include <drivers/mouse.h>
#include <drivers/gfx.h>
#include <arch/io.h>
#include <drivers/vbe.h>
#include <drivers/vga.h>
#include <kernel/events.h>

#define MOUSE_PORT 0x60
#define MOUSE_STATUS 0x64
#define CURSOR_W 16
#define CURSOR_H 16
#define CURSOR_SIZE (CURSOR_W * CURSOR_H)
#define CURSOR_TRANSPARENT 0x00FF00FF /* Magenta = şeffaf */

static struct mouse_state state;
static uint8_t mouse_cycle = 0;
static int8_t mouse_byte[3];

/* İmleç altındaki pikselleri yedekle (Dirty Rect / Mouse Restoring) */
static uint32_t save_buffer[CURSOR_SIZE];
static int last_cursor_x = -1;
static int last_cursor_y = -1;

/* 16x16 Windows-style ok imleci bitmap: 0x00000000=siyah (kenarlık),
 * 0x00FFFFFF=beyaz (dolgu), 0x00FF00FF=şeffaf */
static const uint32_t cursor_arrow[CURSOR_SIZE] = {
    /* Satır 0 */ 0x00000000,
    0x00000000,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    /* Satır 1 */ 0x00000000,
    0x00FFFFFF,
    0x00000000,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    /* Satır 2 */ 0x00000000,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00000000,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    /* Satır 3 */ 0x00000000,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00000000,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    /* Satır 4 */ 0x00000000,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00000000,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    /* Satır 5 */ 0x00000000,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00000000,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    /* Satır 6 */ 0x00000000,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00000000,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    /* Satır 7 */ 0x00000000,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00000000,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    /* Satır 8 */ 0x00000000,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00000000,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    /* Satır 9 */ 0x00000000,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    /* Satır 10 */ 0x00000000,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00000000,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00000000,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    /* Satır 11 */ 0x00000000,
    0x00FFFFFF,
    0x00000000,
    0x00FF00FF,
    0x00000000,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00000000,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    /* Satır 12 */ 0x00000000,
    0x00000000,
    0x00FF00FF,
    0x00FF00FF,
    0x00000000,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00000000,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    /* Satır 13 */ 0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00000000,
    0x00FFFFFF,
    0x00FFFFFF,
    0x00000000,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    /* Satır 14 */ 0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00000000,
    0x00FFFFFF,
    0x00000000,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    /* Satır 15 */ 0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00000000,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
    0x00FF00FF,
};

static void mouse_wait(uint8_t type) {
  uint32_t timeout = 100000;
  if (type == 0) {
    while (timeout--) {
      if ((inb(MOUSE_STATUS) & 1) == 1)
        return;
      io_wait();
    }
  } else {
    while (timeout--) {
      if ((inb(MOUSE_STATUS) & 2) == 0)
        return;
      io_wait();
    }
  }
}

static void mouse_write(uint8_t write) {
  mouse_wait(1);
  outb(MOUSE_STATUS, 0xD4);
  mouse_wait(1);
  outb(MOUSE_PORT, write);
}

static uint8_t mouse_read(void) {
  mouse_wait(0);
  return inb(MOUSE_PORT);
}

static void mouse_handler(struct registers *regs) {
  (void)regs;

  uint8_t b = inb(MOUSE_PORT);

  // Sync check: First byte of packet MUST have bit 3 set!
  // If not, it's a stray ACK (0xFA) or out-of-sync byte. Discard it.
  if (mouse_cycle == 0 && !(b & 0x08)) {
      return;
  }

  mouse_byte[mouse_cycle++] = b;
  if (mouse_cycle == 3) {
    mouse_cycle = 0;
    int x_move = (int8_t)mouse_byte[1];
    int y_move = (int8_t)mouse_byte[2];
    if (mouse_byte[0] & 0x10)
      x_move |= 0xFFFFFF00;
    if (mouse_byte[0] & 0x20)
      y_move |= 0xFFFFFF00;
    state.x += x_move;
    state.y -= y_move;
    struct vbe_info *vbe = vbe_get_info();
    if (state.x < 0)
      state.x = 0;
    if (state.y < 0)
      state.y = 0;
    if (state.x >= (int)vbe->width)
      state.x = vbe->width - 1;
    if (state.y >= (int)vbe->height)
      state.y = vbe->height - 1;
    
    uint8_t old_buttons = state.buttons;
    state.buttons = mouse_byte[0] & 0x07;

    // Push events for state changes
    if ((state.buttons & 1) && !(old_buttons & 1)) events_push((event_t){EVENT_MOUSE_CLICK, 1, state.x, state.y});
    if (!(state.buttons & 1) && (old_buttons & 1)) events_push((event_t){EVENT_MOUSE_RELEASE, 1, state.x, state.y});
    if ((state.buttons & 2) && !(old_buttons & 2)) events_push((event_t){EVENT_MOUSE_CLICK, 2, state.x, state.y});
    if (!(state.buttons & 2) && (old_buttons & 2)) events_push((event_t){EVENT_MOUSE_RELEASE, 2, state.x, state.y});
    
    events_push((event_t){EVENT_MOUSE_MOVE, state.x, state.y, 0});
  }
}

void mouse_init(void) {
  kprintf("PS/2 Mouse: Initializing...\n");
  state.x = 512;
  state.y = 384;
  state.buttons = 0;
  last_cursor_x = -1;
  last_cursor_y = -1;

  mouse_wait(1);
  outb(MOUSE_STATUS, 0xA8);
  mouse_wait(1);
  outb(MOUSE_STATUS, 0x20);
  mouse_wait(0);
  uint8_t status = inb(MOUSE_PORT) | 2;
  mouse_wait(1);
  outb(MOUSE_STATUS, 0x60);
  mouse_wait(1);
  outb(MOUSE_PORT, status);
  mouse_write(0xF6);
  mouse_read();
  mouse_write(0xF4);
  mouse_read();

  uint8_t mask_slave = inb(0xA1);
  mask_slave &= ~(1 << 4);
  outb(0xA1, mask_slave);
  uint8_t mask_master = inb(0x21);
  mask_master &= ~(1 << 2);
  outb(0x21, mask_master);

  register_interrupt_handler(44, mouse_handler);
  kprintf("PS/2 Mouse: OK\n");
}

struct mouse_state *mouse_get_state(void) { return &state; }

/* Sprite rendering: bitmap'ten imleci çiz, alpha (şeffaf = magenta) atla */
static void cursor_draw_sprite(int x, int y) {
  struct vbe_info *vbe = vbe_get_info();
  for (int row = 0; row < CURSOR_H; row++) {
    int dy = y + row;
    if (dy < 0 || dy >= (int)vbe->height)
      continue;
    for (int col = 0; col < CURSOR_W; col++) {
      int dx = x + col;
      if (dx < 0 || dx >= (int)vbe->width)
        continue;
      uint32_t px = cursor_arrow[row * CURSOR_W + col];
      if (px == CURSOR_TRANSPARENT)
        continue;
      
      // If it's a shadow pixel (e.g. 0x010101), we use lower alpha
      if (px == 0x00000000)
          gfx_put_pixel(dx, dy, px);
      else
          gfx_put_pixel_alpha(dx, dy, px & 0xFFFFFF, 255);
    }
  }
}

void mouse_draw_cursor(void) {
  int x = state.x;
  int y = state.y;

  /* Adım 1: Eski konumdaki pikselleri geri yükle (Mouse Restoring / Dirty Rect)
   */
  if (last_cursor_x >= 0 && last_cursor_y >= 0) {
    gfx_restore_rect(save_buffer, last_cursor_x, last_cursor_y, CURSOR_W,
                     CURSOR_H);
  }

  /* Adım 2: Yeni konumdaki pikselleri yedekle */
  gfx_save_rect(save_buffer, x, y, CURSOR_W, CURSOR_H);

  /* Adım 3: İmleci yeni konumda çiz (sprite + alpha) */
  cursor_draw_sprite(x, y);

  last_cursor_x = x;
  last_cursor_y = y;
}

void mouse_invalidate_cursor(void) {
  last_cursor_x = -1;
  last_cursor_y = -1;
}
