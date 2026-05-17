#include <kernel/panic.h>
#include <kernel/klog.h>
#include <drivers/vga.h>
#include <drivers/gfx.h>
#include <drivers/vbe.h>
#include <drivers/serial.h>
#include <libk/string.h>
#include <proc/process.h>

/* Son cagiran subsystem etiketi */
static char panic_last_subsystem[32] = "unknown";

void panic_set_subsystem(const char *name) {
  if (!name) return;
  int i = 0;
  while (name[i] && i < 31) { panic_last_subsystem[i] = name[i]; i++; }
  panic_last_subsystem[i] = '\0';
}

// ── Yardimci: tam ekran Blue Screen ciz ──────────────────────────────────────
static void panic_draw_gui(const char *message, const char *file, uint32_t line) {
  struct vbe_info *vbe = vbe_get_info();
  if (!vbe || !vbe->lfb) return;

  int W = (int)vbe->width;
  int H = (int)vbe->height;

  gfx_clear(0x000A1628);
  gfx_draw_gradient_rect(0, 0, W, 8, 0x00C0392B, 0x00922B21, false);
  gfx_draw_gradient_rect(0, H - 8, W, 8, 0x00922B21, 0x00C0392B, false);

  int panel_x = W / 2 - 340;
  int panel_y = H / 2 - 180;
  int panel_w = 680;
  int panel_h = 360;

  gfx_draw_rect(panel_x + 6, panel_y + 6, panel_w, panel_h, 0x00000000);
  gfx_draw_gradient_rect(panel_x, panel_y, panel_w, panel_h, 0x00101C2E, 0x000A1220, true);
  gfx_draw_rect(panel_x,               panel_y,               panel_w, 2, 0x00E74C3C);
  gfx_draw_rect(panel_x,               panel_y + panel_h - 2, panel_w, 2, 0x00E74C3C);
  gfx_draw_rect(panel_x,               panel_y,               2, panel_h, 0x00E74C3C);
  gfx_draw_rect(panel_x + panel_w - 2, panel_y,               2, panel_h, 0x00E74C3C);

  // Sad face (pixel art)
  int face_x = panel_x + 30, face_y = panel_y + 30, ps = 3;
  for (int r = -5; r <= 5; r++) {
    for (int c = -5; c <= 5; c++) {
      int d2 = r*r + c*c;
      if (d2 <= 25 && d2 >= 20) gfx_draw_rect(face_x+(c+6)*ps, face_y+(r+6)*ps, ps, ps, 0x00FFDD57);
      if (d2 < 20)              gfx_draw_rect(face_x+(c+6)*ps, face_y+(r+6)*ps, ps, ps, 0x00FFD000);
    }
  }
  gfx_draw_rect(face_x+3*ps, face_y+3*ps, ps*2, ps*2, 0x00202020);
  gfx_draw_rect(face_x+7*ps, face_y+3*ps, ps*2, ps*2, 0x00202020);
  gfx_draw_rect(face_x+3*ps, face_y+8*ps, ps*6, ps,   0x00202020);
  gfx_draw_rect(face_x+2*ps, face_y+7*ps, ps,   ps,   0x00202020);
  gfx_draw_rect(face_x+9*ps, face_y+7*ps, ps,   ps,   0x00202020);

  // "KERNEL PANIC" baslik (3x scaled)
  int title_x = panel_x + 130, title_y = panel_y + 25;
  const char *title = "KERNEL PANIC";
  for (int i = 0; title[i]; i++)
    for (int dy = 0; dy < 3; dy++)
      for (int dx = 0; dx < 3; dx++)
        gfx_put_char(title_x + i*24 + dx*8, title_y + dy*8, title[i],
                     (dy == 0 && dx == 0) ? 0x00E74C3C : 0x00C0392B);

  int msg_y = panel_y + 95;
  gfx_draw_rect(panel_x + 20, msg_y - 5, panel_w - 40, 1, 0x00E74C3C);
  gfx_puts(panel_x + 20, msg_y + 8,  "Error:",  0x00E74C3C);
  gfx_puts(panel_x + 75, msg_y + 8,  message,  0x00FFFFFF);

  char loc_buf[128];
  ksnprintf(loc_buf, sizeof(loc_buf), "File: %s  Line: %d", file, (int)line);
  gfx_puts(panel_x + 20, msg_y + 26, loc_buf, 0x00AAAAAA);

  char sub_buf[64];
  ksnprintf(sub_buf, sizeof(sub_buf), "Last subsystem: %s", panic_last_subsystem);
  gfx_puts(panel_x + 20, msg_y + 42, sub_buf, 0x00FFDD57);

  gfx_draw_rect(panel_x + 20, msg_y + 58, panel_w - 40, 1, 0x00334466);

  int info_y = msg_y + 68;
  gfx_puts(panel_x + 20, info_y,       "The system has encountered a fatal error", 0x00CCCCCC);
  gfx_puts(panel_x + 20, info_y + 14,  "and cannot continue running.",             0x00CCCCCC);
  gfx_puts(panel_x + 20, info_y + 36,  "Suggested actions:",                       0x00FFDD57);
  gfx_puts(panel_x + 20, info_y + 52,  "  1. Note the error message above",        0x00AAAAAA);
  gfx_puts(panel_x + 20, info_y + 66,  "  2. Check serial output (COM1) for debug log", 0x00AAAAAA);
  gfx_puts(panel_x + 20, info_y + 80,  "  3. Reboot the system",                   0x00AAAAAA);

  gfx_draw_rect(panel_x + 20, panel_y + panel_h - 35, panel_w - 40, 1, 0x00334466);
  gfx_puts(panel_x + 20, panel_y + panel_h - 25,
           "System halted. Press any key to reboot (if supported).", 0x00E74C3C);
  gfx_puts(8, 10, "Khazar OS v0.2.0", 0x00AAAAAA);
  gfx_puts(W - 80, 10, "PANIC", 0x00E74C3C);

  gfx_swap_buffers();
}

// ── Kernel panic — ana fonksiyon ──────────────────────────────────────────
void kernel_panic(const char *message, const char *file, uint32_t line) {
  __asm__ __volatile__("cli");

  /* Serial'e tam rapor */
  LOG_ERROR("panic", "*** KERNEL PANIC ***");
  LOG_ERROR("panic", "Message : %s", message);
  LOG_ERROR("panic", "File    : %s", file);
  LOG_ERROR("panic", "Line    : %d", (int)line);
  LOG_ERROR("panic", "Subsys  : %s", panic_last_subsystem);

  /* Aktif proses bilgisi */
  extern thread_t *scheduler_get_current_thread(void);
  thread_t *t = scheduler_get_current_thread();
  if (t && t->process) {
    char proc_buf[64];
    ksnprintf(proc_buf, sizeof(proc_buf), "Process : %s (PID %d, TID %d)",
              t->process->name, (int)t->process->pid, (int)t->tid);
    LOG_ERROR("panic", "%s", proc_buf);
  } else {
    LOG_ERROR("panic", "Process : <kernel / no active process>");
  }

  panic_draw_gui(message, file, line);

  terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLUE));
  terminal_clear();

  kprintf("\n");
  kprintf("====================================================================\n");
  kprintf("                        !!! KERNEL PANIC !!!                        \n");
  kprintf("====================================================================\n");
  kprintf("\n");
  kprintf("  Message   : %s\n", message);
  kprintf("  File      : %s\n", file);
  kprintf("  Line      : %d\n", line);
  kprintf("  Subsystem : %s\n", panic_last_subsystem);

  if (t && t->process)
    kprintf("  Process   : %s  PID=%d  TID=%d\n",
            t->process->name, t->process->pid, t->tid);
  else
    kprintf("  Process   : <kernel / no active process>\n");

  kprintf("\n");
  kprintf("  System halted. Cannot continue.\n");
  kprintf("====================================================================\n");

  while (1) { __asm__ __volatile__("hlt"); }
}
