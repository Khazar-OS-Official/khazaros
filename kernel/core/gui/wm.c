#include <kernel/wm.h>
#include <net/ethernet.h>
#include <drivers/gfx.h>
#include <kernel/htmlui.h>
#include <drivers/mouse.h>
#include <drivers/pci.h>
#include <drivers/vbe.h>
#include <libk/string.h>
#include <fs/vfs.h>
#include <mm/kheap.h>
#include <proc/process.h>
#include <drivers/vga.h>
#include <kernel/tty.h>
#include <mm/pmm.h>
#include <drivers/pci.h>
#include <kernel/events.h>

/* 
 * Khazar OS - Tam Funksional Dinamik WM
 */

static window_t *windows_head = NULL; 
static window_t *windows_tail = NULL; 
static menu_t   desktop_menu;
static window_t *active_window = NULL;

typedef struct {
    char text[2048];
    int cursor;
} notepad_data_t;

typedef struct {
    virtual_terminal_t vt;
} terminal_data_t;

#define DESKTOP_MENU_PADDING     5
#define DESKTOP_MENU_ITEM_HEIGHT 22

// ── Desktop İkonları ───────────────────────────────────────────────────────
typedef struct {
  int  x, y;
  char label[20];
  void (*action)(void);
  bool hovered;
} desktop_icon_t;

static desktop_icon_t desktop_icons[6];
static int desktop_icon_count = 0;

static void desktop_icon_add(int x, int y, const char *label, void (*action)(void)) {
  if (desktop_icon_count >= 6) return;
  desktop_icon_t *ic = &desktop_icons[desktop_icon_count++];
  ic->x = x; ic->y = y;
  kstrncpy(ic->label, label, 19);
  ic->action = action; ic->hovered = false;
}

static void draw_icon_terminal(int x, int y) {
  gfx_draw_rect(x, y, 48, 48, 0x00222222);
  gfx_draw_rect(x, y, 48, 2, 0x0000CC88);
  gfx_puts(x + 4, y + 6,  "> _",  0x0000FF88);
}

static void draw_icon_files(int x, int y) {
  gfx_draw_rect(x, y + 8, 48, 36, 0x00D4A017);
  gfx_draw_rect(x, y + 8, 20, 8,  0x00B8860B);
  gfx_draw_rect(x + 4, y + 14, 40, 26, 0x00FFD700);
}

static void draw_icon_notepad(int x, int y) {
  gfx_draw_rect(x, y, 48, 48, 0x00EEEEEE);
  gfx_draw_rect(x + 5, y + 8, 38, 2, 0x00333333);
  gfx_draw_rect(x + 5, y + 16, 38, 2, 0x00333333);
  gfx_draw_rect(x + 5, y + 24, 25, 2, 0x00333333);
}

static void draw_desktop_icon(desktop_icon_t *ic) {
  int x = ic->x, y = ic->y;
  if (ic->hovered) gfx_draw_rect(x - 4, y - 4, 56, 70, 0x33FFFFFF);
  gfx_draw_rect(x, y, 48, 48, 0x00404040);
  if      (strcmp(ic->label, "Terminal") == 0) draw_icon_terminal(x, y);
  else if (strcmp(ic->label, "Fayllar")  == 0) draw_icon_files(x, y);
  else if (strcmp(ic->label, "Notepad")  == 0) draw_icon_notepad(x, y);
  else gfx_draw_rect(x + 10, y + 10, 28, 28, 0x00AAAAAA);
  int label_x = x + (48 - (int)strlen(ic->label) * 8) / 2;
  gfx_puts(label_x, y + 52, ic->label, ic->hovered ? 0x00FFFFFF : 0x00DDDDDD);
}

// ── WM Fonksiyonları ──────────────────────────────────────────────────────

static window_t *wm_alloc_window(void) {
    window_t *win = (window_t *)kmalloc(sizeof(window_t));
    if (!win) return NULL;
    memset(win, 0, sizeof(window_t));
    win->next = windows_head;
    if (windows_head) windows_head->prev = win;
    windows_head = win;
    if (!windows_tail) windows_tail = win;
    return win;
}

void wm_bring_to_front(window_t *win) {
    if (!win || win == windows_head) return;
    if (win->prev) win->prev->next = win->next;
    if (win->next) win->next->prev = win->prev;
    if (win == windows_tail) windows_tail = win->prev;
    win->next = windows_head; win->prev = NULL;
    if (windows_head) windows_head->prev = win;
    windows_head = win;
}

static void wm_activate_terminal(window_t *win) {
    if (win && win->type == WINDOW_TYPE_TERMINAL && win->data) {
        terminal_data_t *d = (terminal_data_t *)win->data;
        terminal_set_active(&d->vt);
    } else {
        terminal_set_active(terminal_get_default());
    }
}

void wm_close_window(window_t *win) {
    if (!win) return;
    bool closing_active_terminal =
        win->type == WINDOW_TYPE_TERMINAL &&
        win->data &&
        terminal_get_active() == &((terminal_data_t *)win->data)->vt;
    if (win->data && (win->type == WINDOW_TYPE_NOTEPAD || win->type == WINDOW_TYPE_TERMINAL)) kfree(win->data);
    if (win->prev) win->prev->next = win->next;
    if (win->next) win->next->prev = win->prev;
    if (win == windows_head) windows_head = win->next;
    if (win == windows_tail) windows_tail = win->prev;
    kfree(win);
    if (closing_active_terminal) wm_activate_terminal(windows_head);
}

void wm_maximize_window(window_t *win) {
    if (!win) return;
    struct vbe_info *vbe = vbe_get_info();
    if (win->is_maximized) {
        win->x = win->last_x; win->y = win->last_y;
        win->w = win->last_w; win->h = win->last_h;
        win->is_maximized = false;
    } else {
        win->last_x = win->x; win->last_y = win->y;
        win->last_w = win->w; win->last_h = win->h;
        win->x = 0; win->y = 0;
        win->w = vbe->width; win->h = vbe->height - 40; // 40px for taskbar
        win->is_maximized = true;
    }
}

void wm_minimize_window(window_t *win) {
    if (!win) return;
    win->is_visible = false;
    win->is_minimized = true;
    // Taskbar handles restoration
}

void wm_open_terminal(void) {
  window_t *w = wm_alloc_window(); if (!w) return;
  w->x = 60; w->y = 60; w->w = 660; w->h = 420;
  w->is_visible = true; w->type = WINDOW_TYPE_TERMINAL;
  kstrncpy(w->title, "Khazar Terminal", 63);
  terminal_data_t *d = kmalloc(sizeof(terminal_data_t));
  if (!d) { wm_close_window(w); return; }
  terminal_init_virtual(&d->vt);  // Yeni terminal her zaman temiz başlar
  w->data = d;
  wm_activate_terminal(w);
}

void wm_open_notepad(void) {
  window_t *w = wm_alloc_window(); if (!w) return;
  w->x = 200; w->y = 100; w->w = 500; w->h = 400;
  w->is_visible = true; w->type = WINDOW_TYPE_NOTEPAD;
  kstrncpy(w->title, "Khazar Notepad", 63);
  notepad_data_t *d = kmalloc(sizeof(notepad_data_t));
  memset(d, 0, sizeof(notepad_data_t)); w->data = d;
}

void wm_open_files(void) {
  window_t *w = wm_alloc_window(); if (!w) return;
  w->x = 150; w->y = 130; w->w = 460; w->h = 380;
  w->is_visible = true; w->type = WINDOW_TYPE_FILES;
  kstrncpy(w->title, "Fayllar", 63);
}

void wm_open_settings(void) {
  window_t *w = wm_alloc_window(); if (!w) return;
  w->x = 220; w->y = 160; w->w = 420; w->h = 320;
  w->is_visible = true; w->type = WINDOW_TYPE_SETTINGS;
  kstrncpy(w->title, "Ayarlar", 63);
}

void wm_open_about(void) {
  window_t *w = wm_alloc_window(); if (!w) return;
  w->x = 260; w->y = 210; w->w = 420; w->h = 240;
  w->is_visible = true; w->type = WINDOW_TYPE_ABOUT;
  kstrncpy(w->title, "Hakkinda", 63);
}

void wm_refresh_desktop(void) { }

void wm_init(void) {
  windows_head = NULL; windows_tail = NULL; desktop_icon_count = 0;
  desktop_icon_add(20,  50, "Terminal", wm_open_terminal);
  desktop_icon_add(20, 140, "Fayllar",  wm_open_files);
  desktop_icon_add(20, 230, "Notepad",  wm_open_notepad);
  desktop_icon_add(20, 320, "Hakkinda", wm_open_about);

  desktop_menu.is_visible = false;
  desktop_menu.w = 160; desktop_menu.item_count = 4;
  kstrncpy(desktop_menu.items[0].label, "Terminal Ac", 31);
  desktop_menu.items[0].action = wm_open_terminal;
  kstrncpy(desktop_menu.items[1].label, "Notepad", 31);
  desktop_menu.items[1].action = wm_open_notepad;
  kstrncpy(desktop_menu.items[2].label, "Fayllar", 31);
  desktop_menu.items[2].action = wm_open_files;
  kstrncpy(desktop_menu.items[3].label, "Hakkinda", 31);
  desktop_menu.items[3].action = wm_open_about;
  desktop_menu.h = desktop_menu.item_count * DESKTOP_MENU_ITEM_HEIGHT + 10;
}

void wm_handle_key(char c) {
    if (!windows_head) { tty_putc(c); return; }
    if (windows_head->type == WINDOW_TYPE_TERMINAL) {
        wm_activate_terminal(windows_head);
        tty_putc(c);
    }
    else if (windows_head->type == WINDOW_TYPE_NOTEPAD) {
        notepad_data_t *d = (notepad_data_t *)windows_head->data;
        if (c == '\b') { if (d->cursor > 0) d->text[--d->cursor] = '\0'; }
        else if (d->cursor < 2047) { d->text[d->cursor++] = c; d->text[d->cursor] = '\0'; }
    }
}

void wm_update(void) {
  event_t ev;
  while (events_pop(&ev)) {
    if (ev.type == EVENT_KEY_DOWN) {
        wm_handle_key((char)ev.data1);
    }
    else if (ev.type == EVENT_MOUSE_CLICK) {
        int mx = ev.data2; int my = ev.data3;
        bool proc = false;
        if (desktop_menu.is_visible) {
            if (mx >= desktop_menu.x && mx < desktop_menu.x + desktop_menu.w && my >= desktop_menu.y && my < desktop_menu.y + desktop_menu.h) {
                int idx = (my - desktop_menu.y - 5) / 22;
                if (idx >= 0 && idx < desktop_menu.item_count) desktop_menu.items[idx].action();
                proc = true;
            }
            desktop_menu.is_visible = false;
        }
        
        if (ev.data1 == 2) { // Right click
            desktop_menu.x = mx; desktop_menu.y = my; desktop_menu.is_visible = true;
            proc = true;
        }

        window_t *curr = windows_head;
        while (curr && !proc) {
            if (!curr->is_visible) { curr = curr->next; continue; }
            
            // Buttons
            if (mx >= curr->x + curr->w - 22 && my >= curr->y + 4 && mx <= curr->x + curr->w - 4 && my <= curr->y + 22) { wm_close_window(curr); proc = true; break; }
            if (mx >= curr->x + curr->w - 42 && my >= curr->y + 4 && mx <= curr->x + curr->w - 24 && my <= curr->y + 22) { wm_maximize_window(curr); proc = true; break; }
            if (mx >= curr->x + curr->w - 62 && my >= curr->y + 4 && mx <= curr->x + curr->w - 44 && my <= curr->y + 22) { wm_minimize_window(curr); proc = true; break; }

            // Resize area
            if (mx >= curr->x + curr->w - 16 && mx <= curr->x + curr->w && my >= curr->y + curr->h - 16 && my <= curr->y + curr->h) {
                wm_bring_to_front(curr);
                if (!curr->is_maximized) { curr->is_resizing = true; active_window = curr; }
                proc = true; break;
            }
            
            // Title bar
            if (mx >= curr->x && mx <= curr->x + curr->w && my >= curr->y && my <= curr->y + 25) {
                wm_bring_to_front(curr); wm_activate_terminal(curr);
                if (!curr->is_maximized) {
                    curr->is_dragging = true; curr->drag_off_x = mx - curr->x; curr->drag_off_y = my - curr->y;
                    active_window = curr;
                }
                proc = true; break;
            }
            
            // Focus
            if (mx >= curr->x && mx <= curr->x + curr->w && my >= curr->y && my <= curr->y + curr->h) {
                wm_bring_to_front(curr); wm_activate_terminal(curr);
                proc = true; break;
            }
            curr = curr->next;
        }
        
        if (!proc) {
            for (int i = 0; i < desktop_icon_count; i++) {
                if (mx >= desktop_icons[i].x && mx < desktop_icons[i].x+48 && my >= desktop_icons[i].y && my < desktop_icons[i].y+48) {
                    desktop_icons[i].action();
                    break;
                }
            }
        }
    }
    else if (ev.type == EVENT_MOUSE_RELEASE) {
        if (active_window) {
            active_window->is_dragging = false;
            active_window->is_resizing = false;
        }
        active_window = NULL;
    }
  }

  // Update dragging/resizing continuously using mouse state for smoothness
  struct mouse_state *ms = mouse_get_state();
  if (active_window) {
      if (active_window->is_dragging) {
          active_window->x = ms->x - active_window->drag_off_x;
          active_window->y = ms->y - active_window->drag_off_y;
      } else if (active_window->is_resizing) {
          active_window->w = ms->x - active_window->x;
          active_window->h = ms->y - active_window->y;
          if (active_window->w < 100) active_window->w = 100;
          if (active_window->h < 60) active_window->h = 60;
      }
  }
  
  for (int i = 0; i < desktop_icon_count; i++)
    desktop_icons[i].hovered = (ms->x >= desktop_icons[i].x && ms->x < desktop_icons[i].x+48 && ms->y >= desktop_icons[i].y && ms->y < desktop_icons[i].y+48);
}

static void wm_draw_window_content(window_t *win) {
  int cy = win->y + 26; int ch = win->h - 26;
  if (win->type == WINDOW_TYPE_TERMINAL) {
    gfx_draw_rect(win->x + 2, cy, win->w - 4, ch - 2, 0x00111111);
    terminal_data_t *td = (terminal_data_t *)win->data;
    uint16_t *vga = td ? td->vt.buffer : terminal_get_buffer();
    if (vga) {
      for (int r = 0; r < VGA_HEIGHT; r++) for (int c = 0; c < VGA_WIDTH; c++) {
        uint16_t e = vga[r * VGA_WIDTH + c]; char ch = (char)(e & 0xFF);
        if (ch >= 32 && ch <= 126) gfx_put_char(win->x + 8 + c * 8, cy + 4 + r * 12, ch, 0xFFFFFFFF);
      }
    }
  } else if (win->type == WINDOW_TYPE_NOTEPAD) {
    gfx_draw_rect(win->x + 2, cy, win->w - 4, ch - 2, 0x00FFFFFF);
    notepad_data_t *d = (notepad_data_t *)win->data;
    if (d) { gfx_puts(win->x + 10, cy + 10, d->text, 0x00333333); gfx_draw_rect(win->x + 10 + (int)strlen(d->text) * 8, cy + 10, 2, 12, 0x00000000); }
  } else if (win->type == WINDOW_TYPE_SETTINGS) {
    gfx_draw_rect(win->x + 2, cy, win->w - 4, ch - 2, 0x00F0F0F0);
    char buf[64];
    
    // RAM Info
    size_t total = pmm_get_block_count() * 4 / 1024; // MB
    size_t free = pmm_get_free_block_count() * 4 / 1024; // MB
    ksprintf(buf, "RAM: %d MB / %d MB", total - free, total);
    gfx_puts(win->x + 20, cy + 20, buf, 0x00333333);
    gfx_draw_rect(win->x + 20, cy + 35, 300, 10, 0x00CCCCCC);
    if (total > 0) gfx_draw_rect(win->x + 20, cy + 35, 300 * (total - free) / total, 10, 0x003A78C8);

    // CPU Info
    gfx_puts(win->x + 20, cy + 60, "CPU: x86 Generic (32-bit)", 0x00333333);

    // PCI Devices
    gfx_puts(win->x + 20, cy + 90, "Hardware:", 0x00333333);
    for (uint32_t i = 0; i < pci_get_device_count() && i < 10; i++) {
        const pci_device_t *d = pci_get_device(i);
        ksprintf(buf, "- Bus %d, Dev %d (Ven: %x, Dev: %x, Cls: %x)", d->bus, d->dev, d->vendor_id, d->device_id, d->class_code);
        gfx_puts(win->x + 30, cy + 110 + i * 15, buf, 0x00666666);
    }
  } else if (win->type == WINDOW_TYPE_FILES) {
    gfx_draw_rect(win->x + 2, cy, win->w - 4, ch - 2, 0x00F8F8F8);
    gfx_puts(win->x + 10, cy + 10, "Fayllar Siyahisi:", 0x00222222);
    if (vfs_root && vfs_root->readdir) {
      for (int i = 0; i < 8; i++) {
        vfs_node_t *n = vfs_root->readdir(vfs_root, i);
        if (!n) break;
        gfx_puts(win->x + 15, cy + 35 + i * 20, n->name, 0x00444444); kfree(n);
      }
    }
  } else if (win->type == WINDOW_TYPE_ABOUT) {
    gfx_draw_rect(win->x + 2, cy, win->w - 4, ch - 2, 0x001A2A3A);
    gfx_puts(win->x + 20, cy + 20, "Khazar OS v0.2.0", 0x004A90D9);
    gfx_puts(win->x + 20, cy + 50, "Dinamik Pencere & Yaddas Sistemi", 0x00AAAAAA);
  }
}

void wm_draw(void) {
  for (int i = 0; i < desktop_icon_count; i++) draw_desktop_icon(&desktop_icons[i]);
  window_t *curr = windows_tail; while (curr) {
    if (curr->is_visible) {
      // 1. Shadow
      if (!curr->is_maximized) gfx_draw_shadow(curr->x, curr->y, curr->w, curr->h, 8);

      // 2. Window Frame
      gfx_draw_rect(curr->x, curr->y, curr->w, curr->h, 0xF2F2F2);
      
      // 3. Title Bar
      uint32_t c = (curr == windows_head) ? 0x3A78C8 : 0x424242;
      gfx_draw_rect(curr->x + 1, curr->y + 1, curr->w - 2, 24, c);
      gfx_puts(curr->x + 10, curr->y + 8, curr->title, 0xFFFFFF);
      
      // 4. Buttons (Close, Max, Min)
      // Close (Red)
      gfx_draw_rect(curr->x + curr->w - 22, curr->y + 4, 18, 18, 0xCC2222);
      gfx_puts(curr->x + curr->w - 17, curr->y + 8, "X", 0xFFFFFF);
      
      // Maximize (Green)
      gfx_draw_rect(curr->x + curr->w - 42, curr->y + 4, 18, 18, 0x22CC22);
      gfx_puts(curr->x + curr->w - 37, curr->y + 8, "M", 0xFFFFFF);
      
      // Minimize (Yellow)
      gfx_draw_rect(curr->x + curr->w - 62, curr->y + 4, 18, 18, 0xCCCC22);
      gfx_puts(curr->x + curr->w - 57, curr->y + 8, "_", 0xFFFFFF);

      // 5. Resize Handle (Bottom-right)
      if (!curr->is_maximized) {
          gfx_draw_rect(curr->x + curr->w - 10, curr->y + curr->h - 2, 8, 1, 0x888888);
          gfx_draw_rect(curr->x + curr->w - 6, curr->y + curr->h - 6, 4, 1, 0x888888);
          gfx_draw_rect(curr->x + curr->w - 2, curr->y + curr->h - 10, 1, 8, 0x888888);
      }
      
      wm_draw_window_content(curr);
    }
    curr = curr->prev;
  }
  if (desktop_menu.is_visible) {
    gfx_draw_rect(desktop_menu.x, desktop_menu.y, desktop_menu.w, desktop_menu.h, 0xEEEEEE);
    for (int i = 0; i < desktop_menu.item_count; i++) gfx_puts(desktop_menu.x + 5, desktop_menu.y + 5 + i * 22, desktop_menu.items[i].label, 0x333333);
  }
}
void wm_show_menu(int x, int y) { desktop_menu.x = x; desktop_menu.y = y; desktop_menu.is_visible = true; }
window_t *windows_get_head(void) { return windows_head; }
