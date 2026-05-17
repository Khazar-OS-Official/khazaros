#include <drivers/power.h>
#include <drivers/vga.h>
#include <fs/fat32.h>
#include <fs/vfs.h>
#include <kernel/pe.h>
#include <kernel/pkg.h>
#include <kernel/syscall.h>
#include <libk/string.h>
#include <mm/kheap.h>
#include <net/arp.h>
#include <net/ethernet.h>
#include <net/ipv4.h>
#include <net/udp.h>

#define SHELL_BUFFER_SIZE 256
#define SHELL_HISTORY_MAX 16

static char input_buffer[SHELL_BUFFER_SIZE];
static int  buffer_index = 0;

// ── Komanda tarixi ──────────────────────────────────────────────────────────
static char history[SHELL_HISTORY_MAX][SHELL_BUFFER_SIZE];
static int  history_count = 0;   // Toplam saxlanılan sayı
static int  history_pos   = -1;  // -1 = aktif giriş, >=0 = tarix naviqasiyası

static void history_push(const char *cmd) {
  if (strlen(cmd) == 0) return;
  // Dublikati önlə
  if (history_count > 0) {
    if (strcmp(history[(history_count - 1) % SHELL_HISTORY_MAX], cmd) == 0)
      return;
  }
  int idx = history_count % SHELL_HISTORY_MAX;
  kstrncpy(history[idx], cmd, SHELL_BUFFER_SIZE - 1);
  history[idx][SHELL_BUFFER_SIZE - 1] = '\0';
  history_count++;
}

// history_get: offset 0 = ən son, 1 = ondan əvvəlki, ...
// returns NULL əgər offset keçərli deyilsə
static const char *history_get(int offset) {
  if (offset < 0 || offset >= history_count) return NULL;
  if (offset >= SHELL_HISTORY_MAX) return NULL;
  int total = history_count;
  int idx = (total - 1 - offset);
  if (idx < 0) return NULL;
  idx = idx % SHELL_HISTORY_MAX;
  return history[idx];
}

// ── Tab tamamlama ──────────────────────────────────────────────────────────
static const char *builtin_commands[] = {
  "help", "ls", "clear", "reboot", "shutdown", "about",
  "chmod", "pkg", "ping", "cat", "echo", "ifconfig",
  NULL
};

static void shell_tab_complete(void) {
  if (buffer_index == 0) return;

  // İlk söz (komanda) tap
  int word_start = 0;
  // Hələlik yalnız birinci sözü tamamla
  char prefix[32];
  int plen = 0;
  for (int i = 0; i < buffer_index && i < 31; i++) {
    if (input_buffer[i] == ' ') break;
    prefix[plen++] = input_buffer[i];
  }
  prefix[plen] = '\0';
  (void)word_start;

  if (plen == 0) return;

  // Uyğun komandaları sayıq
  int match_count = 0;
  const char *last_match = NULL;
  for (int i = 0; builtin_commands[i] != NULL; i++) {
    if (strncmp(builtin_commands[i], prefix, (uint32_t)plen) == 0) {
      match_count++;
      last_match = builtin_commands[i];
    }
  }

  if (match_count == 1 && last_match) {
    // Tək uyğunluq — tamamla
    int full_len = (int)strlen(last_match);
    // Buferdəki prefix-dən sonrasını tamamla
    for (int i = plen; i < full_len && buffer_index < SHELL_BUFFER_SIZE - 2; i++) {
      input_buffer[buffer_index++] = last_match[i];
      kprintf("%c", last_match[i]);
    }
    // Boşluq əlavə et
    if (buffer_index < SHELL_BUFFER_SIZE - 2) {
      input_buffer[buffer_index++] = ' ';
      kprintf(" ");
    }
  } else if (match_count > 1) {
    // Çoxlu uyğunluq — siyahı göstər
    kprintf("\n");
    for (int i = 0; builtin_commands[i] != NULL; i++) {
      if (strncmp(builtin_commands[i], prefix, (uint32_t)plen) == 0) {
        kprintf("  %s\n", builtin_commands[i]);
      }
    }
    // Prompt-u yenidən göstər
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    kprintf("khazar> ");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("%s", input_buffer);
  }
}

// ── Xətti silmək (sol tərəfdən tamamilə sil) ────────────────────────────────
static void shell_clear_line(void) {
  for (int i = 0; i < buffer_index; i++) {
    kprintf("\b");
  }
  buffer_index = 0;
  input_buffer[0] = '\0';
}

// ── ASCII Art Logo ──────────────────────────────────────────────────────────
static void shell_print_logo(void) {
  terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
  kprintf("  /$$   /$$ /$$                                                "
          "/$$$$$$   /$$$$$$ \n");
  kprintf(" | $$  /$$/| $$                                               /$$__ "
          " $$ /$$__  $$\n");
  kprintf(" | $$ /$$/ | $$$$$$$   /$$$$$$  /$$$$$$$$  /$$$$$$   /$$$$$$ | $$  "
          "\\ $$| $$  \\__/\n");
  kprintf(" | $$$$$/  | $$__  $$ |____  $$|____ /$$/ |____  $$ /$$__  $$| $$  "
          "| $$|  $$$$$$ \n");
  kprintf(" | $$  $$  | $$  \\ $$  /$$$$$$$   /$$$$/   /$$$$$$$| $$  \\__/| $$ "
          " | $$ \\____  $$\n");
  kprintf(" | $$\\  $$ | $$  | $$ /$$__  $$  /$$__/   /$$__  $$| $$      | $$  "
          "| $$ /$$  \\ $$\n");
  kprintf(" | $$ \\  $$| $$  | $$|  $$$$$$$ /$$$$$$$$|  $$$$$$$| $$      |  "
          "$$$$$$/|  $$$$$$/\n");
  kprintf(" |__/  \\__/|__/  |__/ \\_______/|________/ \\_______/|__/       "
          "\\______/  \\______/ \n");
  terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
  kprintf(
      "      Khazar OS [Version 0.2.0] - Khazar terminalina xos geldiniz!\n");
  kprintf("      Tip 'help' for commands. Use UP/DOWN for history, TAB to complete.\n\n");
}

static void shell_prompt(void) {
  terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
  kprintf("khazar> ");
  terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
}

// ── Komand işlənməsi ────────────────────────────────────────────────────────
static void shell_process_command(char *command) {
  if (strlen(command) == 0)
    return;

  history_push(command);
  history_pos = -1;

  if (strcmp(command, "help") == 0) {
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    kprintf("\n=== Khazar OS Shell Komandaları ===\n\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("  help              - Bu mesajı göster\n");
    kprintf("  ls                - Disk üzərindəki faylları siyahıla\n");
    kprintf("  clear             - Ekranı təmizlə\n");
    kprintf("  reboot            - Sistemi yenidən başlat\n");
    kprintf("  shutdown          - Sistemi bağla\n");
    kprintf("  about             - Khazar OS haqqında\n");
    kprintf("  chmod <mod> <yol> - Fayl icazələrini dəyiş\n");
    kprintf("  pkg list          - Quraşdırılmış paketlər\n");
    kprintf("  pkg install <ad>  - Lokal paket quraşdır\n");
    kprintf("  pkg update        - Uzaq paket siyahısını yenilə\n");
    kprintf("  pkg fetch <ad>    - Paketi serverdən yüklə\n");
    kprintf("  ping <ip>         - Standart ICMP ping göndər\n");
    kprintf("  ifconfig          - Şəbəkə parametrlərinə bax / dəyiş\n");
    kprintf("  sysinfo           - Sistem məlumatları\n");
    kprintf("  [komanda]         - Userland PE binary icra et\n\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK));
    kprintf("  [TAB] = tamamla  |  [UP/DOWN] = tarix  |  [BKSP] = sil\n\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));

  } else if (strcmp(command, "reboot") == 0) {
    kprintf("Yenidən başladılır...\n");
    power_reboot();
  } else if (strcmp(command, "shutdown") == 0) {
    kprintf("Söndürülür...\n");
    power_shutdown();
  } else if (strcmp(command, "clear") == 0) {
    terminal_clear();
    shell_print_logo();

  } else if (strcmp(command, "sysinfo") == 0) {
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    kprintf("\n=== Sistem Məlumatları ===\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("  OS:        Khazar OS v0.2.0\n");
    kprintf("  Arch:      x86 (32-bit Protected Mode)\n");
    kprintf("  Kernel:    Monolithic, Custom\n");
    kprintf("  FS:        FAT32 over ATA/LBA28 PIO\n");
    kprintf("  GUI:       VBE VESA Framebuffer @ 1024x768x32\n");
    if (ethernet_is_ready()) {
      const ethernet_device_t *eth = ethernet_get_device();
      if (eth && eth->pci_dev) {
        char dname[32] = "Unknown";
        if (eth->pci_dev->vendor_id == 0x10EC) {
          strcpy(dname, "Realtek RTL8136");
        } else if (eth->pci_dev->vendor_id == 0x8086) {
          strcpy(dname, "Intel E1000");
        }
        kprintf("  Network:   %s (%02X:%02X:%02X:%02X:%02X:%02X)\n",
                dname, eth->mac[0], eth->mac[1], eth->mac[2], eth->mac[3], eth->mac[4], eth->mac[5]);
      } else {
        kprintf("  Network:   None (Not bound)\n");
      }
    } else {
      kprintf("  Network:   None (Not active)\n");
    }
    kprintf("  Shell:     Built-in with history & tab-complete\n\n");

  } else if (strcmp(command, "about") == 0) {
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    kprintf("\n=== Khazar OS v0.2.0 ===\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    kprintf("  Professionally crafted 32-bit operating system.\n");
    kprintf("  Features:\n");
    kprintf("    - Custom bootloader (GRUB Multiboot)\n");
    kprintf("    - x86 Protected Mode with GDT/IDT/ISR\n");
    kprintf("    - Physical & Virtual Memory Manager\n");
    kprintf("    - Preemptive process scheduler\n");
    kprintf("    - VBE VESA GUI with Window Manager\n");
    kprintf("    - ATA disk driver (LBA28 PIO)\n");
    kprintf("    - FAT32 filesystem with VFS layer\n");
    kprintf("    - Network stack (E1000, ARP, IPv4, UDP)\n");
    kprintf("    - Package manager (.kzp format)\n");
    kprintf("    - PE executable loader\n\n");

  } else if (strcmp(command, "ls") == 0) {
    if (vfs_root) {
      fat32_ls();
    } else {
      kprintf("ls: Fayl sistemi mövcud deyil.\n");
    }

  } else if (strncmp(command, "chmod ", 6) == 0) {
    char *mode_str = command + 6;
    char *path = NULL;
    for (int i = 0; mode_str[i] != '\0'; i++) {
      if (mode_str[i] == ' ') {
        mode_str[i] = '\0';
        path = mode_str + i + 1;
        break;
      }
    }

    if (path) {
      uint32_t mode = 0;
      for (int i = 0; mode_str[i] != '\0'; i++) {
        if (mode_str[i] >= '0' && mode_str[i] <= '7') {
          mode = (mode << 3) | (uint32_t)(mode_str[i] - '0');
        }
      }

      int ret;
      __asm__ volatile("int $0x80"
                       : "=a"(ret)
                       : "a"(15), "b"((uint32_t)path), "c"(mode));

      if (ret == 0) {
        kprintf("chmod: %s icazəsi %o olaraq dəyişdirildi.\n", path, mode);
      } else {
        kprintf("chmod: %s üçün icazə dəyişdirilə bilmədi.\n", path);
      }
    } else {
      kprintf("İstifadə: chmod <octal_mod> <yol>\n");
    }

  } else if (strncmp(command, "pkg ", 4) == 0 || strcmp(command, "pkg") == 0) {
    char *subcmd = command + 4;
    if (strlen(command) <= 4) {
      kprintf("İstifadə: pkg [list|install|info|update|fetch] [ad]\n");
    } else if (strncmp(subcmd, "list", 4) == 0) {
      pkg_list();
    } else if (strncmp(subcmd, "install ", 8) == 0) {
      pkg_install(subcmd + 8);
    } else if (strncmp(subcmd, "info ", 5) == 0) {
      pkg_info(subcmd + 5);
    } else if (strcmp(subcmd, "update") == 0) {
      pkg_remote_list();
    } else if (strncmp(subcmd, "fetch ", 6) == 0) {
      const char *pkg_name = subcmd + 6;
      if (strlen(pkg_name) == 0) {
        kprintf("İstifadə: pkg fetch <ad>\n");
      } else {
        pkg_fetch(pkg_name);
      }
    } else {
      kprintf("pkg: Naməlum alt komanda. 'help' yazmağa cəhd edin.\n");
    }

  } else if (strncmp(command, "ping ", 5) == 0) {
    const char *ip_str = command + 5;
    uint32_t a = 0, b = 0, c = 0, d = 0;
    while (*ip_str >= '0' && *ip_str <= '9') a = a * 10 + (uint32_t)(*ip_str++ - '0');
    if (*ip_str == '.') ip_str++;
    while (*ip_str >= '0' && *ip_str <= '9') b = b * 10 + (uint32_t)(*ip_str++ - '0');
    if (*ip_str == '.') ip_str++;
    while (*ip_str >= '0' && *ip_str <= '9') c = c * 10 + (uint32_t)(*ip_str++ - '0');
    if (*ip_str == '.') ip_str++;
    while (*ip_str >= '0' && *ip_str <= '9') d = d * 10 + (uint32_t)(*ip_str++ - '0');

    uint32_t dst_ip = a | (b << 8) | (c << 16) | (d << 24);
    kprintf("ping: ARP həll edilir %d.%d.%d.%d...\n", a, b, c, d);

    arp_request(dst_ip);
    uint8_t mac_out[6];
    int got_arp = 0;
    for (uint32_t i = 0; i < 500000; i++) {
      if (arp_lookup(dst_ip, mac_out)) { got_arp = 1; break; }
      __asm__ volatile("int $0x20");
    }

    if (got_arp) {
      if (!icmp_send_request(dst_ip)) {
        kprintf("ping: ICMP göndərmə uğursuz oldu\n");
      }
    } else {
      kprintf("ping: %d.%d.%d.%d-dən ARP cavabı gəlmədi\n", a, b, c, d);
    }

  } else if (strncmp(command, "ifconfig", 8) == 0) {
    char *args = command + 8;
    while (*args == ' ') args++;

    if (*args == '\0') {
      kprintf("\n=== Şəbəkə Konfiqurasiyası ===\n");
      kprintf("  IP Ünvanı: %d.%d.%d.%d\n",
              (net_our_ip >> 0) & 0xFF, (net_our_ip >> 8) & 0xFF,
              (net_our_ip >> 16) & 0xFF, (net_our_ip >> 24) & 0xFF);
      kprintf("  Şlüz (GW): %d.%d.%d.%d\n",
              (net_gateway_ip >> 0) & 0xFF, (net_gateway_ip >> 8) & 0xFF,
              (net_gateway_ip >> 16) & 0xFF, (net_gateway_ip >> 24) & 0xFF);
      kprintf("  Mask (NM): %d.%d.%d.%d\n",
              (net_netmask >> 0) & 0xFF, (net_netmask >> 8) & 0xFF,
              (net_netmask >> 16) & 0xFF, (net_netmask >> 24) & 0xFF);
      kprintf("  Yayını  (BC): %d.%d.%d.%d\n\n",
              (net_bcast_ip >> 0) & 0xFF, (net_bcast_ip >> 8) & 0xFF,
              (net_bcast_ip >> 16) & 0xFF, (net_bcast_ip >> 24) & 0xFF);
    } else {
      char ip_str[32] = {0};
      char gw_str[32] = {0};
      char nm_str[32] = {0};

      int arg_idx = 0;
      char *p = args;
      while (*p != ' ' && *p != '\0' && arg_idx < 31) ip_str[arg_idx++] = *p++;
      ip_str[arg_idx] = '\0';

      while (*p == ' ') p++;
      arg_idx = 0;
      while (*p != ' ' && *p != '\0' && arg_idx < 31) gw_str[arg_idx++] = *p++;
      gw_str[arg_idx] = '\0';

      while (*p == ' ') p++;
      arg_idx = 0;
      while (*p != ' ' && *p != '\0' && arg_idx < 31) nm_str[arg_idx++] = *p++;
      nm_str[arg_idx] = '\0';

      if (ip_str[0] == '\0') {
        kprintf("İstifadə: ifconfig [<ip> <gw> <mask>]\n");
      } else {
        uint32_t a = 0, b = 0, c = 0, d = 0;
        char *s = ip_str;
        while (*s >= '0' && *s <= '9') a = a * 10 + (*s++ - '0');
        if (*s == '.') s++;
        while (*s >= '0' && *s <= '9') b = b * 10 + (*s++ - '0');
        if (*s == '.') s++;
        while (*s >= '0' && *s <= '9') c = c * 10 + (*s++ - '0');
        if (*s == '.') s++;
        while (*s >= '0' && *s <= '9') d = d * 10 + (*s++ - '0');
        net_our_ip = a | (b << 8) | (c << 16) | (d << 24);

        if (gw_str[0] != '\0') {
          a = b = c = d = 0;
          s = gw_str;
          while (*s >= '0' && *s <= '9') a = a * 10 + (*s++ - '0');
          if (*s == '.') s++;
          while (*s >= '0' && *s <= '9') b = b * 10 + (*s++ - '0');
          if (*s == '.') s++;
          while (*s >= '0' && *s <= '9') c = c * 10 + (*s++ - '0');
          if (*s == '.') s++;
          while (*s >= '0' && *s <= '9') d = d * 10 + (*s++ - '0');
          net_gateway_ip = a | (b << 8) | (c << 16) | (d << 24);
        }

        if (nm_str[0] != '\0') {
          a = b = c = d = 0;
          s = nm_str;
          while (*s >= '0' && *s <= '9') a = a * 10 + (*s++ - '0');
          if (*s == '.') s++;
          while (*s >= '0' && *s <= '9') b = b * 10 + (*s++ - '0');
          if (*s == '.') s++;
          while (*s >= '0' && *s <= '9') c = c * 10 + (*s++ - '0');
          if (*s == '.') s++;
          while (*s >= '0' && *s <= '9') d = d * 10 + (*s++ - '0');
          net_netmask = a | (b << 8) | (c << 16) | (d << 24);
        }

        net_bcast_ip = (net_our_ip & net_netmask) | (~net_netmask);

        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        kprintf("ifconfig: Şəbəkə konfiqurasiyası uğurla yeniləndi!\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
      }
    }

  } else {
    // External PE binary
    char exe_name[32];
    int i = 0;
    while (command[i] != ' ' && command[i] != '\0' && i < 27) {
      exe_name[i] = command[i];
      i++;
    }
    exe_name[i] = '\0';

    // Auto-append .exe
    if (strlen(exe_name) < 4 ||
        strcmp(exe_name + strlen(exe_name) - 4, ".exe") != 0) {
      kstrncpy(exe_name + strlen(exe_name), ".exe", 4);
      exe_name[strlen(exe_name) + 4] = '\0';
    }

    // Uppercase for FAT32
    for (int j = 0; exe_name[j] != '\0'; j++) {
      if (exe_name[j] >= 'a' && exe_name[j] <= 'z') {
        exe_name[j] = exe_name[j] - 'a' + 'A';
      }
    }

    char full_path[64];
    kstrncpy(full_path, "/BIN/", 5);
    kstrncpy(full_path + 5, exe_name, strlen(exe_name));
    full_path[5 + strlen(exe_name)] = '\0';

    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(11), "b"((uint32_t)full_path),
                       "c"((uint32_t)command));

    if (ret != 0) {
      kprintf("Naməlum komanda: '%s'. 'help' yazın.\n", command);
    }
  }
}

// ── Klavyə xüsusi düymələri üçün state ─────────────────────────────────────
// keyboard.c-dən bu iki funksiya mövcuddursa istifadə ediləcək
// Yoxsa sadə char ilə işləyəcək
extern uint8_t keyboard_get_special(void); // 0x48=UP, 0x50=DOWN, 0x0F=TAB

// ── Shell input (klavyedən çağırılır) ──────────────────────────────────────
void shell_input(char c) {
  // Check for special keys via scancode
  uint8_t special = keyboard_get_special();

  // TAB
  if (special == 0x0F || c == '\t') {
    shell_tab_complete();
    return;
  }

  // UP arrow (0x48)
  if (special == 0x48) {
    int next_pos = history_pos + 1;
    const char *entry = history_get(next_pos);
    if (entry) {
      shell_clear_line();
      history_pos = next_pos;
      kstrncpy(input_buffer, entry, SHELL_BUFFER_SIZE - 1);
      buffer_index = (int)strlen(input_buffer);
      kprintf("%s", input_buffer);
    }
    return;
  }

  // DOWN arrow (0x50)
  if (special == 0x50) {
    if (history_pos > 0) {
      history_pos--;
      const char *entry = history_get(history_pos);
      if (entry) {
        shell_clear_line();
        kstrncpy(input_buffer, entry, SHELL_BUFFER_SIZE - 1);
        buffer_index = (int)strlen(input_buffer);
        kprintf("%s", input_buffer);
      }
    } else if (history_pos == 0) {
      // Aktif giriş boş vəziyyətə qayıt
      shell_clear_line();
      history_pos = -1;
    }
    return;
  }

  // ENTER
  if (c == '\n') {
    kprintf("\n");
    input_buffer[buffer_index] = '\0';
    shell_process_command(input_buffer);
    buffer_index = 0;
    history_pos = -1;
    shell_prompt();

  // BACKSPACE
  } else if (c == '\b') {
    if (buffer_index > 0) {
      buffer_index--;
      kprintf("\b");
    }

  // Printable chars
  } else if (c >= 32 && c <= 126 && buffer_index < SHELL_BUFFER_SIZE - 1) {
    input_buffer[buffer_index++] = c;
    kprintf("%c", c);
  }
}

// ── Shell init ──────────────────────────────────────────────────────────────
void shell_init(void) {
  terminal_clear();
  buffer_index = 0;
  history_count = 0;
  history_pos = -1;
  shell_print_logo();
  shell_prompt();
}

// ── Shell update (əsas döngüdən çağırılır) ──────────────────────────────────
extern char tty_getc_raw(void);
#include <kernel/tty.h>
extern struct tty main_tty;

void shell_update(void) {
  char c = tty_getc();
  if (c != 0) {
    shell_input(c);
  }
}
