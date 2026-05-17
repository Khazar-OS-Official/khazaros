#include <drivers/power.h>
#include <arch/io.h>
#include <drivers/gfx.h>
#include <drivers/vbe.h>

// QEMU/Bochs/VirtualBox kimi VM-lər üçün ümumi ACPI poweroff portları.
// Real PC-də ACPI tam dəstəyi yoxdur, amma bir çox hypervisor bu
// "magic" portlara cavab verir.

static void power_try_acpi(void) {
  // Bochs: outw(0xB004, 0x2000) → poweroff
  outw(0xB004, 0x2000);

  // QEMU (yeni): ACPI PM1a_CNT portu (0x604) + S5
  outw(0x604, 0x2000);

  // Bəzi VirtualBox konfiqurasiyaları üçün alternativ porta da yaz.
  outw(0x4004, 0x3400);
}

void power_shutdown(void) {
  // 1) Ekranda "subsystem stop" animasiyası göstər (təxminən 1–2 saniyə).
  struct vbe_info *vbe = vbe_get_info();

  int cx = vbe->width / 2 - 140;
  int cy = vbe->height / 2 + 20;

  gfx_puts(cx, cy, "[*] Stopping disk subsystem...", 0x00AAAAAA);
  gfx_swap_buffers();
  for (volatile uint32_t i = 0; i < 8000000; i++)
    ;

  gfx_puts(cx, cy + 12, "[*] Stopping input devices...", 0x00AAAAAA);
  gfx_swap_buffers();
  for (volatile uint32_t i = 0; i < 8000000; i++)
    ;

  gfx_puts(cx, cy + 24, "[*] Flushing graphics buffer...", 0x00AAAAAA);
  gfx_swap_buffers();
  for (volatile uint32_t i = 0; i < 8000000; i++)
    ;

  gfx_puts(cx, cy + 36, "[OK] All subsystems halted.", 0x0080FF80);
  gfx_swap_buffers();

  // 2) ACPI vasitəsilə faktiki poweroff cəhdi
  power_try_acpi();

  // 3) Əgər hələ də sönməyibsə, ən azından sistemi təhlükəsiz halde saxla.
  __asm__ __volatile__("cli");
  while (1) {
    __asm__ __volatile__("hlt");
  }
}

void power_reboot(void) {
  // Klassik 8042 klavye kontrolcüsü reset siqnalı
  uint8_t temp;
  // Output buffer boşalana qədər gözlə
  do {
    temp = inb(0x64);
  } while (temp & 0x02);

  outb(0x64, 0xFE); // Reset command

  // Əgər hər hansı səbəbdən reboot etməsə, HLT döngüsü
  while (1) {
    __asm__ __volatile__("hlt");
  }
}

