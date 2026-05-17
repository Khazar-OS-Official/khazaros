#include <drivers/keyboard.h>
#include <arch/idt.h>
#include <arch/io.h>
#include <arch/isr.h>
#include <kernel/shell.h>
#include <kernel/tty.h>
#include <drivers/vga.h>

#include <kernel/wm.h>
#include <kernel/events.h>

// Scancode to ASCII table (US-QWERTY)
static const char scancode_ascii[] = {
    0,    27,  '1', '2', '3',  '4', '5', '6', '7',  '8', /* 9 */
    '9',  '0', '-', '=', '\b',                           /* Backspace */
    '\t',                                                /* Tab */
    'q',  'w', 'e', 'r',                                 /* 19 */
    't',  'y', 'u', 'i', 'o',  'p', '[', ']', '\n',      /* Enter key */
    0,                                                   /* 29   - Control */
    'a',  's', 'd', 'f', 'g',  'h', 'j', 'k', 'l',  ';', /* 39 */
    '\'', '`', 0,                                        /* Left shift */
    '\\', 'z', 'x', 'c', 'v',  'b', 'n',                 /* 49 */
    'm',  ',', '.', '/', 0,                              /* Right shift */
    '*',  0,                                             /* Alt */
    ' ',                                                 /* Space bar */
    0,                                                   /* Caps lock */
    0,                                                   /* 59 - F1 key ... > */
    0,    0,   0,   0,   0,    0,   0,   0,   0,         /* < ... F10 */
    0,                                                   /* 69 - Num lock*/
    0,                                                   /* Scroll Lock */
    0,                                                   /* Home key */
    0,                                                   /* Up Arrow    (0x48) */
    0,                                                   /* Page Up */
    '-',  0,                                             /* Left Arrow */
    0,    0,                                             /* Right Arrow */
    '+',  0,                                             /* 79 - End key*/
    0,                                                   /* Down Arrow  (0x50) */
    0,                                                   /* Page Down */
    0,                                                   /* Insert Key */
    0,                                                   /* Delete Key */
    0,    0,   0,   0,                                   /* F11 Key */
    0,                                                   /* F12 Key */
    0, /* All other keys are undefined */
};

// ── Xüsusi düymə scancodeları ──────────────────────────────────────────────
#define SC_UP_ARROW    0x48
#define SC_DOWN_ARROW  0x50
#define SC_LEFT_ARROW  0x4B
#define SC_RIGHT_ARROW 0x4D
#define SC_TAB         0x0F

// ── Dövlət dəyişənləri ────────────────────────────────────────────────────
static bool shift_pressed   = false;
static bool caps_lock_on    = false;
static bool e0_prefix       = false;   // Extended scancode prefix

// Son xüsusi scancode (shell_input-in oxuması üçün)
static volatile uint8_t last_special_scancode = 0;

// ── Shift + shift düymə xəritəsi ─────────────────────────────────────────
static char shift_char(char c) {
  switch (c) {
    case '1': return '!';
    case '2': return '@';
    case '3': return '#';
    case '4': return '$';
    case '5': return '%';
    case '6': return '^';
    case '7': return '&';
    case '8': return '*';
    case '9': return '(';
    case '0': return ')';
    case '-': return '_';
    case '=': return '+';
    case '[': return '{';
    case ']': return '}';
    case '\\': return '|';
    case ';': return ':';
    case '\'': return '"';
    case ',': return '<';
    case '.': return '>';
    case '/': return '?';
    case '`': return '~';
    default:
      if (c >= 'a' && c <= 'z') return (char)(c - 32);
      return c;
  }
}

// ── Klaviatura interrupt handler ──────────────────────────────────────────
static void keyboard_callback(struct registers *regs) {
  (void)regs;
  uint8_t scancode = inb(KEYBOARD_DATA_PORT);

  // Extended prefix (E0 xüsusi düymələr üçün)
  if (scancode == 0xE0) {
    e0_prefix = true;
    return;
  }

  // Key release
  if (scancode & 0x80) {
    uint8_t key = scancode & 0x7F;
    if (key == 0x2A || key == 0x36) {  // Shift buraxıldı
      shift_pressed = false;
    }
    e0_prefix = false;
    return;
  }

  // Extended key (ox düymələri E0 prefiksi ilə gəlir)
  if (e0_prefix) {
    e0_prefix = false;
    // UP (0x48), DOWN (0x50), LEFT (0x4B), RIGHT (0x4D)
    if (scancode == SC_UP_ARROW || scancode == SC_DOWN_ARROW ||
        scancode == SC_LEFT_ARROW || scancode == SC_RIGHT_ARROW) {
      last_special_scancode = scancode;
      // Shell-ə virtual char olaraq göndər — shell_input içindəki
      // keyboard_get_special() ilə oxunar
      tty_putc('\x01'); // SOH — xüsusi düymə marker
    }
    return;
  }

  // Normal tuş
  if (scancode == 0x3A) {  // Caps Lock toggle
    caps_lock_on = !caps_lock_on;
    return;
  }
  if (scancode == 0x2A || scancode == 0x36) {  // Shift basıldı
    shift_pressed = true;
    return;
  }

  // TAB düyməsi
  if (scancode == SC_TAB) {
    last_special_scancode = SC_TAB;
    tty_putc('\t');
    return;
  }

  // ASCII çevirmə
  if (scancode < sizeof(scancode_ascii)) {
    char c = scancode_ascii[scancode];
    if (c != 0) {
      bool do_shift = shift_pressed;
      if (c >= 'a' && c <= 'z' && caps_lock_on) do_shift = !do_shift;
      if (do_shift) c = shift_char(c);
      
      event_t ev = {EVENT_KEY_DOWN, c, 0, 0};
      events_push(ev);
    }
  }
}

// ── Xüsusi scancode oxuma (shell.c tərəfindən çağırılır) ─────────────────
// Son xüsusi scancode-u qaytar və sıfırla
uint8_t keyboard_get_special(void) {
  uint8_t sc = last_special_scancode;
  last_special_scancode = 0;
  return sc;
}

// ── Klaviatura başlatma ──────────────────────────────────────────────────
void keyboard_init(void) {
  /* I8042 output buffer-ini temizle (real HW-da timeout olmadan doner!) */
  int flush_timeout = 100000;
  while ((inb(0x64) & 1) && flush_timeout-- > 0) {
    inb(0x60);
  }

  // IRQ1 (INT 33) handler qeyd et
  register_interrupt_handler(33, keyboard_callback);

  // PIC'den IRQ1-i ac
  uint8_t mask = inb(PIC1_DATA);
  mask &= ~(1 << 1);
  outb(PIC1_DATA, mask);
}
