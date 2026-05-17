#ifndef WM_H
#define WM_H

#include <libk/types.h>

// Pencere Tipleri
typedef enum {
  WINDOW_TYPE_MAIN,
  WINDOW_TYPE_TERMINAL,
  WINDOW_TYPE_FILES,
  WINDOW_TYPE_SETTINGS,
  WINDOW_TYPE_ABOUT,
  WINDOW_TYPE_TASKMAN,
  WINDOW_TYPE_NOTEPAD
} window_type_t;

// Pencere Yapısı
typedef struct window {
  int x, y;
  int w, h;
  char title[64];
  bool is_dragging;
  bool is_resizing;
  int drag_off_x;
  int drag_off_y;
  bool is_visible;
  bool is_minimized;
  bool is_maximized;
  int last_x, last_y, last_w, last_h; // Restore için
  window_type_t type;
  void *data;
  struct window *next;
  struct window *prev;
} window_t;

// Menü Yapısı
typedef struct {
  char label[32];
  void (*action)(void);
} menu_item_t;

typedef struct {
  int x, y;
  int w, h;
  int item_count;
  menu_item_t items[8];
  bool is_visible;
} menu_t;

void wm_init(void);
void wm_update(void);
void wm_draw(void);
void wm_show_menu(int x, int y);
void wm_handle_key(char c);

// Window management functions
void wm_open_terminal(void);
void wm_open_files(void);
void wm_open_settings(void);
void wm_open_about(void);
void wm_open_taskman(void);
void wm_open_notepad(void);
void wm_refresh_desktop(void);
window_t *windows_get_head(void);
void wm_bring_to_front(window_t *win);

#endif
