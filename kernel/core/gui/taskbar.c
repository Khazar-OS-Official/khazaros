#include <kernel/taskbar.h>
#include <drivers/gfx.h>
#include <drivers/mouse.h>
#include <drivers/rtc.h>
#include <drivers/vbe.h>
#include <drivers/vga.h>
#include <kernel/wm.h>
#include <drivers/power.h>
#include <libk/string.h>

static int taskbar_y;
static bool start_button_hover = false;
static bool start_button_pressed = false;
static bool start_menu_active = false;
static int start_menu_h = 180;

// Start menusu layout konstantlari (piksel olaraq)
#define START_MENU_X 5
#define START_MENU_WIDTH 220
#define START_MENU_PADDING 10
#define START_MENU_HEADER_HEIGHT 26
#define START_MENU_ITEM_HEIGHT 22
#define START_MENU_ITEM_GAP 4

static int calc_start_menu_height(int item_count) {
  if (item_count <= 0)
    return START_MENU_PADDING * 2 + START_MENU_HEADER_HEIGHT;

  int item_block =
      item_count * (START_MENU_ITEM_HEIGHT + START_MENU_ITEM_GAP) -
      START_MENU_ITEM_GAP; // Son elementden sonra bosluq yoxdur
  return START_MENU_PADDING * 2 + START_MENU_HEADER_HEIGHT + item_block;
}

// Shutdown action
static void start_action_shutdown(void) {
  // Ekranda shutdown mesaji goster
  struct vbe_info *vbe = vbe_get_info();
  gfx_draw_rect(vbe->width / 2 - 150, vbe->height / 2 - 50, 300, 100,
                0x00202020);
  gfx_draw_rect(vbe->width / 2 - 150, vbe->height / 2 - 50, 300, 1, 0x00808080);
  gfx_draw_rect(vbe->width / 2 - 150, vbe->height / 2 - 50, 1, 100, 0x00808080);
  gfx_puts(vbe->width / 2 - 100, vbe->height / 2 - 30, "Sistem Kapatiliyor...",
           COLOR_WHITE);
  gfx_puts(vbe->width / 2 - 80, vbe->height / 2, "Khazar OS", 0x0087CEEB);
  gfx_swap_buffers();

  // Faktik power-off cəhdi (VM-lər və mümkün olduqda real PC).
  power_shutdown();
}

/* Start Menü Yapısı */
typedef struct {
  char label[32];
  void (*action)(void);
} start_item_t;

static start_item_t start_items[] = {{"Terminal", wm_open_terminal},
                                     {"Dosyalar", wm_open_files},
                                     {"Ayarlar", wm_open_settings},
                                     {"Notepad", wm_open_notepad},
                                     {"Kapat", start_action_shutdown}};
static int start_item_count = 5;

/* Saat string'i (HH:MM AM/PM) */
static void format_time(char *buf, size_t bufsz) {
  struct rtc_time t;
  rtc_read(&t);
  (void)bufsz;
  if (t.hour < 12) {
    int h = (t.hour == 0) ? 12 : t.hour;
    buf[0] = (h / 10) + '0';
    buf[1] = (h % 10) + '0';
    buf[2] = ':';
    buf[3] = (t.minute / 10) + '0';
    buf[4] = (t.minute % 10) + '0';
    buf[5] = ' ';
    buf[6] = 'A';
    buf[7] = 'M';
    buf[8] = '\0';
  } else {
    int h = (t.hour == 12) ? 12 : (t.hour - 12);
    buf[0] = (h / 10) + '0';
    buf[1] = (h % 10) + '0';
    buf[2] = ':';
    buf[3] = (t.minute / 10) + '0';
    buf[4] = (t.minute % 10) + '0';
    buf[5] = ' ';
    buf[6] = 'P';
    buf[7] = 'M';
    buf[8] = '\0';
  }
}


void taskbar_init(void) {
  struct vbe_info *vbe = vbe_get_info();
  taskbar_y = vbe->height - TASKBAR_HEIGHT;
  kprintf("Taskbar: Initialized at y=%d\n", taskbar_y);
}

void taskbar_update(void) {
  struct mouse_state *ms = mouse_get_state();
  static bool last_left_click = false;
  bool left_click = ms->buttons & 1;
  bool just_pressed = left_click && !last_left_click;

  // Menyu olcusu (click testleri ucun)
  start_menu_h = calc_start_menu_height(start_item_count);
  int menu_y = taskbar_y - start_menu_h - 5;

  // Start butonu kontrolü
  if (ms->y >= taskbar_y && ms->y < taskbar_y + TASKBAR_HEIGHT && ms->x >= 0 &&
      ms->x < TASKBAR_START_BTN_WIDTH) {
    start_button_hover = true;
    if (just_pressed) {
      start_menu_active = !start_menu_active;
    }
    start_button_pressed = left_click;
  } else {
    start_button_hover = false;
    start_button_pressed = false;
    // Menü dışına tıklandığında kapat (basitçe)
    if (just_pressed && start_menu_active) {
      if (!(ms->x >= START_MENU_X &&
            ms->x < START_MENU_X + START_MENU_WIDTH &&
            ms->y >= menu_y && ms->y < menu_y + start_menu_h)) {
        start_menu_active = false;
      }
    }

    // Menü elemanı tıklama (Menü aktifse)
    if (start_menu_active && just_pressed) {
      int items_start_y =
          menu_y + START_MENU_PADDING + START_MENU_HEADER_HEIGHT;

      if (ms->x >= START_MENU_X + START_MENU_PADDING &&
          ms->x < START_MENU_X + START_MENU_WIDTH - START_MENU_PADDING &&
          ms->y >= items_start_y &&
          ms->y < items_start_y + start_menu_h) {
        for (int i = 0; i < start_item_count; i++) {
          int item_y = items_start_y +
                       i * (START_MENU_ITEM_HEIGHT + START_MENU_ITEM_GAP);
          if (ms->y >= item_y &&
              ms->y < item_y + START_MENU_ITEM_HEIGHT) {
            if (start_items[i].action) {
              start_items[i].action();
            }
            start_menu_active = false;
            break;
          }
        }
      }
    }

  }

  // Click durumunu her frame güncelle
  last_left_click = left_click;

  // Window buttons in taskbar
  if (just_pressed && ms->y >= taskbar_y) {
      int bx = 110; 
      window_t *curr = windows_get_head(); 
      while (curr) {
          if (ms->x >= bx && ms->x < bx + 150) {
              if (curr->is_minimized) {
                  curr->is_visible = true;
                  curr->is_minimized = false;
                  wm_bring_to_front(curr);
              } else {
                  // If it's already front, minimize it. Else bring to front.
                  if (curr == windows_get_head()) {
                      curr->is_visible = false;
                      curr->is_minimized = true;
                  } else {
                      wm_bring_to_front(curr);
                  }
              }
              break;
          }
          bx += 160;
          curr = curr->next;
      }
  }
}

static void draw_start_menu(void) {
  if (!start_menu_active) {
    return;
  }

  struct mouse_state *ms = mouse_get_state();
  start_menu_h = calc_start_menu_height(start_item_count);
  int menu_y = taskbar_y - start_menu_h - 5; // 5px gap from taskbar

  // Arxa fon (gradientli panel)
  uint32_t c1 = 0x0016213e;
  uint32_t c2 = 0x000f3460;
  gfx_draw_gradient_rect(START_MENU_X, menu_y, START_MENU_WIDTH, start_menu_h,
                         c1, c2, true);

  // Kölgə
  gfx_draw_rect(START_MENU_X + START_MENU_WIDTH,
                menu_y + 4, 4, start_menu_h, 0x00202020);
  gfx_draw_rect(START_MENU_X + 4,
                menu_y + start_menu_h, START_MENU_WIDTH, 4, 0x00202020);

  // Header
  int header_x = START_MENU_X + START_MENU_PADDING;
  int header_y = menu_y + START_MENU_PADDING;
  gfx_puts(header_x, header_y, "KHAZAR OS", 0x0087CEEB);

  // Menu elemanlari
  int items_start_y =
      header_y + START_MENU_HEADER_HEIGHT; // Bir az bosluq saxla
  for (int i = 0; i < start_item_count; i++) {
    int item_y = items_start_y +
                 i * (START_MENU_ITEM_HEIGHT + START_MENU_ITEM_GAP);
    int item_x = START_MENU_X + START_MENU_PADDING;
    int item_w = START_MENU_WIDTH - 2 * START_MENU_PADDING;

    bool hover = (ms->x >= item_x && ms->x < item_x + item_w &&
                  ms->y >= item_y && ms->y < item_y + START_MENU_ITEM_HEIGHT);
    uint32_t bg = hover ? 0x000f3460 : 0x001a1a2e;

    gfx_draw_rounded_rect(item_x, item_y, item_w, START_MENU_ITEM_HEIGHT, 4,
                          bg);
    gfx_puts(item_x + 8, item_y + 6, start_items[i].label, COLOR_WHITE);
  }
}

static void draw_wave_icon(int x, int y) {
  // Wave/Hazar İkonu simülasyonu (3 katmanlı dalga)
  // Üst (Beyaz)
  gfx_draw_rect(x, y + 4, 16, 4, COLOR_WHITE);
  // Orta (Açık Mavi)
  gfx_draw_rect(x, y + 8, 16, 4, 0x0087CEEB);
  // Alt (Turkuaz)
  gfx_draw_rect(x, y + 12, 16, 4, 0x0040E0D0);
}

void taskbar_draw(void) {
  struct vbe_info *vbe = vbe_get_info();

  // Ana taskbar arka planı
  gfx_draw_rect(0, taskbar_y, vbe->width, TASKBAR_HEIGHT, TASKBAR_COLOR);
  gfx_draw_rect(0, taskbar_y, vbe->width, 1, 0x00404040); // Üst çizgi

  // Start butonu
  uint32_t start_bg = start_button_pressed
                          ? 0x00151515
                          : (start_button_hover ? 0x00404040 : 0x00333333);
  gfx_draw_rect(2, taskbar_y + 2, TASKBAR_START_BTN_WIDTH - 4,
                TASKBAR_HEIGHT - 4, start_bg);

  // Wave İkonu
  draw_wave_icon(8, taskbar_y + 10);

  gfx_puts(28, taskbar_y + 15, "Start", COLOR_WHITE);

  // Saat Paneli
  int clock_x = vbe->width - 110;
  char time_buf[16];
  format_time(time_buf, sizeof(time_buf));
  gfx_puts(clock_x + 5, taskbar_y + 15, time_buf, COLOR_WHITE);

  // Window Buttons
  int bx = 110;
  window_t *curr = windows_get_head();
  while (curr) {
      uint32_t btn_bg = (curr == windows_get_head()) ? 0x004A78C8 : 0x00333333;
      if (curr->is_minimized) btn_bg = 0x00222222;
      
      gfx_draw_rect(bx, taskbar_y + 4, 150, TASKBAR_HEIGHT - 8, btn_bg);
      gfx_draw_rect(bx, taskbar_y + 4, 150, 1, 0x00555555);
      
      char title_trim[16];
      kstrncpy(title_trim, curr->title, 15);
      gfx_puts(bx + 10, taskbar_y + 12, title_trim, COLOR_WHITE);
      
      bx += 160;
      curr = curr->next;
  }

  // Menü (En üstte çizilmeli)
  draw_start_menu();
}
