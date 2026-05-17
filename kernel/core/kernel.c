#include <arch/gdt.h>
#include <arch/idt.h>
#include <arch/isr.h>
#include <drivers/ata.h>
#include <drivers/gfx.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <kernel/events.h>
#include <drivers/pci.h>
#include <drivers/pit.h>
#include <drivers/rtc.h>
#include <drivers/serial.h>
#include <drivers/vbe.h>
#include <drivers/vga.h>
#include <fs/fat32.h>
#include <fs/devfs.h>
#include <kernel/multiboot.h>
#include <kernel/pe.h>
#include <kernel/pkg.h>
#include <kernel/shell.h>
#include <kernel/syscall.h>
#include <kernel/taskbar.h>
#include <kernel/tty.h>
#include <kernel/wm.h>
#include <libk/string.h>
#include <mm/kheap.h>
#include <mm/paging/fault.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <net/arp.h>
#include <net/ethernet.h>
#include <net/ipv4.h>
#include <net/udp.h>
#include <proc/process.h>

static struct multiboot_info saved_mbi;

// 32-bit calling convention: stack arguments
void kernel_main(uint32_t magic, uint32_t mbi_addr) {
  struct multiboot_info *mbi = (struct multiboot_info *)mbi_addr;
  
  // Güvenli kopyalama (VMM_init sonrasında physical mbi unmapped kalabilir)
  if (mbi) {
      saved_mbi = *mbi;
      mbi = &saved_mbi;
  } else {
      // Fallback
      saved_mbi.flags = 0;
      mbi = &saved_mbi;
  }

  // 1. Erken hata ayıklama sistemlerini başlat
  serial_init();
  serial_write_string("\n\n[KERNEL] Khazar OS - Kernel Entry Point Reached.\n");

  terminal_initialize();
  tty_init();
  terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));

  kprintf("########################################\n");
  kprintf("#       KHAZAR OS - Higher Half        #\n");
  kprintf("########################################\n\n");

  // 2. GDT kurulumu
  kprintf("[Step 1] Initializing GDT... ");
  gdt_init();
  kprintf("SUCCESS\n");

  // 3. IDT kurulumu
  kprintf("[Step 2] Initializing IDT... ");
  idt_init();
  kprintf("SUCCESS\n");

  // 3.1. Serial COM1 (Artık yukarda yapılıyor)
  kprintf("[Step 2.1] Serial (COM1) already active.\n");

  // 4. PMM (Fiziksel Bellek Yönetimi) - 32-bit
  kprintf("[Step 3] Initializing PMM... ");
  extern uint32_t _kernel_end;
  uint32_t kernel_end_addr = (uint32_t)&_kernel_end;

  // Let's cap max physical RAM managed by this kernel version to 128MB limit in boot.asm 
  uint32_t mem_size = 128 * 1024 * 1024;
  if (mbi->flags & (1 << 6)) {
    uint32_t mb_mem_size = (mbi->mem_upper * 1024) + 0x100000;
    if (mb_mem_size < mem_size) {
      mem_size = mb_mem_size; // only manage what we actually have
    }
  }

  // 1. Bitmap'i hazırla (Hepsi dolu başlar)
  pmm_init(mem_size, kernel_end_addr);

  // 2. Multiboot memory map'i kullanarak uygun bölgeleri aç
  if (mbi->flags & (1 << 6)) {
      /* mmap_addr GRUB'dan gelen FİZİKSEL adrestir.
       * boot.asm'de PSE ile 4GB identity map yapıldı.
       * Bu yüzden doğrudan fiziksel adresi sanal adres gibi kullanabiliriz. */
      uint32_t mmap_phys = mbi->mmap_addr;
      struct multiboot_mmap_entry *mmap = (struct multiboot_mmap_entry *)mmap_phys;
      uint32_t mmap_length = mbi->mmap_length;
      uint32_t i = 0;
      while (i < mmap_length) {
          struct multiboot_mmap_entry *entry = (struct multiboot_mmap_entry *)((uint32_t)mmap + i);
          if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
              uint32_t region_base = (uint32_t)entry->addr;
              uint32_t region_size = (uint32_t)entry->len;
              if (region_base < mem_size) {
                  if (region_base + region_size > mem_size) {
                      region_size = mem_size - region_base;
                  }
                  pmm_init_region(region_base, region_size);
              }
          }
          i += entry->size + 4;
      }
  } else {
      pmm_init_region(0, mem_size);
  }

  // 3. Kritik alanları rezerve et (Kapat)
  // İlk 1MB (BIOS, VGA, Stack)
  pmm_deinit_region(0, 0x100000);

  // Kernel alanı + PMM Bitmap (Bitmap 4KB yer kaplıyor) - 32-bit
  uint32_t kernel_phys_start = 0x100000; // 1MB
  uint32_t kernel_phys_end = kernel_end_addr - KERNEL_VIRT_BASE + 4096;
  pmm_deinit_region(kernel_phys_start, kernel_phys_end - kernel_phys_start);

  // Kernel Heap Rezerve et (0x500000 fiziksel - 16MB)
  // PMM burayı başkasına vermemeli!
  pmm_deinit_region(0x500000, 16 * 1024 * 1024);

  kprintf("SUCCESS (Total: %d, Free: %d)\n", pmm_get_block_count(),
          pmm_get_free_block_count());

  // 5. VMM (Sanal Bellek Yönetimi)
  kprintf("[Step 4] Initializing VMM... ");
  vmm_init();
  fault_init();
  kprintf("SUCCESS (Free: %d)\n", pmm_get_free_block_count());

  // 6. Heap Allocator
  kprintf("[Step 5] Initializing Kernel Heap... ");
  kheap_init(0xC0500000, 16 * 1024 * 1024); // 16MB heap
  kprintf("SUCCESS\n");

  // EARLY VBE & GFX INIT: VMM init olduqdan derhal sonra GFX basladirik ki, 
  // sonraki adimlarda (PCI, AHCI, Network) yaranan Panic'ler ekranda gorunsun!
  serial_write_string("[DEBUG] Early VBE Init started.\n");
  vbe_init(mbi, magic);
  
  if (vbe_get_info()->lfb == NULL) {
    /* SON CƏHD: MBI-da framebuffer adresi var amma vbe_init reject etdi. */
    struct multiboot_info *m = mbi;
    if ((m->flags & (1 << 12)) &&
        (m->framebuffer_addr & 0xFFFFFFFF) != 0 &&
        (m->framebuffer_addr >> 32) == 0 &&
        m->framebuffer_width > 0 && m->framebuffer_height > 0) {
      
      serial_write_string("[DEBUG] Son cehd: MBI FB manual map edilir...\n");
      uint32_t phys = (uint32_t)(m->framebuffer_addr & 0xFFFFFFFF);
      uint32_t phys_al = phys & ~0xFFFU;
      uint32_t offset  = phys - phys_al;
      uint32_t virt    = 0xE0000000U;
      uint32_t msz     = m->framebuffer_pitch * m->framebuffer_height + offset;
      if (msz < 0x800000) msz = 0x800000;
      msz = (msz + 4095) & ~4095U;
      if (msz > 0x1000000) msz = 0x1000000;

      int ok = 1;
      for (uint32_t i = 0; i < msz; i += 4096) {
        if (!vmm_map_page((void *)(phys_al + i), (void *)(virt + i), VMM_PRESENT | VMM_WRITABLE)) { ok = 0; break; }
      }

      if (ok) {
        struct vbe_info *vi = vbe_get_info();
        vi->width  = m->framebuffer_width;
        vi->height = m->framebuffer_height;
        vi->pitch  = m->framebuffer_pitch;
        vi->bpp    = m->framebuffer_bpp;
        vi->lfb    = (uint32_t *)(virt + offset);
        serial_write_string("[DEBUG] Son cehd BASARILI\n");
      }
    }
  }

  if (vbe_get_info()->lfb != NULL) {
      gfx_init();
      serial_write_string("[DEBUG] GFX Init BASARILI\n");
  } else {
      serial_write_string("[DEBUG] VBE LFB tapilmadi, Panic olarsa yalniz COM1-e gedecek.\n");
  }

  // 7. Process & Scheduler Init (Interruptlardan önce yapmalıyız!)
  kprintf("[Step 6] Initializing Process Management... ");
  serial_write_string("[DEBUG] Calling process_init...\n");
  if (vbe_get_info()->lfb != NULL) { gfx_puts(10, 30, "[Step 6] Init Process Management...", 0x00FFDD57); gfx_swap_buffers(); }
  process_init();
  scheduler_init();
  kprintf("SUCCESS\n");

  // 8. Klavye ve Shell
  kprintf("[Step 7] Initializing Input Systems... ");
  serial_write_string("[DEBUG] Initializing PIT and Keyboard...\n");
  if (vbe_get_info()->lfb != NULL) { gfx_puts(10, 50, "[Step 7] Init PIT and Keyboard...", 0x00FFDD57); gfx_swap_buffers(); }
  pit_init(60);
  keyboard_init();

  // Create a dummy demo task early to ensure ready_queue is not empty
  // DISABLED: task_demo causes crash because it calls gfx_puts before gfx_init
  // extern void task_demo(void);
  // process_create("Demo Task", task_demo);

  serial_write_string("[DEBUG] Enabling interrupts...\n");
  __asm__ __volatile__("sti");
  kprintf("SUCCESS\n");

  // PCI scan
  kprintf("[Step 7.1] Scanning PCI bus... ");
  if (vbe_get_info()->lfb != NULL) { gfx_puts(10, 70, "[Step 7.1] Scanning PCI bus...", 0x00FFDD57); gfx_swap_buffers(); }
  pci_init();
  kprintf("DONE (Devices: %d)\n", pci_get_device_count());

  // Ethernet + Network Stack
  kprintf("[Step 7.2] Initializing Network Stack... ");
  if (vbe_get_info()->lfb != NULL) { gfx_puts(10, 90, "[Step 7.2] Initializing Network Stack...", 0x00FFDD57); gfx_swap_buffers(); }
  if (ethernet_init()) {
    arp_init();
    ipv4_init();
    udp_init();
    kprintf("SUCCESS\n");
    // Start background network task
    extern void network_task(void);
    process_create("Network Task", network_task);
  } else {
    kprintf("NOT FOUND\n");
  }

  // 9. Disk Sistemleri
  kprintf("[Step 8] Initializing Disk (ATA & AHCI)... ");
  serial_write_string("[DEBUG] Initializing ATA & AHCI...\n");
  if (vbe_get_info()->lfb != NULL) { gfx_puts(10, 110, "[Step 8] Init AHCI...", 0x00FFDD57); gfx_swap_buffers(); }
  extern void ahci_init(void);
  ahci_init(); // Detect SATA drives
  
  if (vbe_get_info()->lfb != NULL) { gfx_puts(10, 130, "[Step 8] Init ATA...", 0x00FFDD57); gfx_swap_buffers(); }
  ata_init();  // Detect IDE drives
  kprintf("SUCCESS\n");

  kprintf("[Step 8.1] Initializing Audio & USB... ");
  serial_write_string("[DEBUG] Initializing AC97 & UHCI...\n");
  if (vbe_get_info()->lfb != NULL) { gfx_puts(10, 150, "[Step 8.1] Init AC97 & UHCI...", 0x00FFDD57); gfx_swap_buffers(); }
  extern void ac97_init(void);
  ac97_init();
  extern void uhci_init(void);
  uhci_init();
  kprintf("SUCCESS\n");

  serial_write_string(
      "[DEBUG] Kernel initialization complete. Reaching final loop.\n");
  kprintf("\n--- Khazar OS System Ready ---\n");
  kprintf("[Step 9] Initializing Filesystem (FAT32)... ");
  serial_write_string("[DEBUG] Initializing FAT32...\n");
  if (vbe_get_info()->lfb != NULL) { gfx_puts(10, 170, "[Step 9] Init FAT32...", 0x00FFDD57); gfx_swap_buffers(); }
  if (fat32_init()) {
    kprintf("SUCCESS\n");
    vfs_root = fat32_get_root();
    devfs_init();
    fat32_ls();
  } else {
    kprintf("FAILED\n");
  }

  syscall_init();
  pkg_init();

  if (vbe_get_info()->lfb != NULL) { gfx_puts(10, 190, "ALL DRIVERS LOADED OK! Starting WM...", 0x00FF00); gfx_swap_buffers(); }

  // --- Phase 6: GUI Başlatma ---
  serial_write_string("[DEBUG] === VBE Grafik Başlatma Aşaması ===\n");

  /* Multiboot bilgilerini seri porta dök: nerede takıldığını anlamak için */
  {
    char dbg[64];
    ksprintf(dbg, "[DEBUG] MBI flags   = 0x%x\n", mbi->flags);
    serial_write_string(dbg);
    ksprintf(dbg, "[DEBUG] FB bit12    = %d\n", (mbi->flags >> 12) & 1);
    serial_write_string(dbg);
    ksprintf(dbg, "[DEBUG] FB type     = %d\n", (uint32_t)mbi->framebuffer_type);
    serial_write_string(dbg);
    ksprintf(dbg, "[DEBUG] FB addr_lo  = 0x%x\n", (uint32_t)(mbi->framebuffer_addr & 0xFFFFFFFF));
    serial_write_string(dbg);
    ksprintf(dbg, "[DEBUG] FB addr_hi  = 0x%x\n", (uint32_t)(mbi->framebuffer_addr >> 32));
    serial_write_string(dbg);
    ksprintf(dbg, "[DEBUG] FB width    = %d\n", mbi->framebuffer_width);
    serial_write_string(dbg);
    ksprintf(dbg, "[DEBUG] FB height   = %d\n", mbi->framebuffer_height);
    serial_write_string(dbg);
    ksprintf(dbg, "[DEBUG] FB pitch    = %d\n", mbi->framebuffer_pitch);
    serial_write_string(dbg);
    ksprintf(dbg, "[DEBUG] FB bpp      = %d\n", (uint32_t)mbi->framebuffer_bpp);
    serial_write_string(dbg);
    ksprintf(dbg, "[DEBUG] magic       = 0x%x\n", magic);
    serial_write_string(dbg);
  }

  // --- Phase 6: GUI Başlatma ---
  serial_write_string("[DEBUG] === Window Manager Başlatma ===\n");

  /* Hala NULL-dursa text moduna kec (eslinde gfx_init islemeyecek) */
  if (vbe_get_info()->lfb == NULL) {
    kprintf("VBE: HATA - Grafik modu acilamadi. Metin moduna geciliyor.\n");
    kprintf("Hint: GRUB menuentry icinde 'set gfxpayload=keep' ve\n");
    kprintf("      'terminal_output gfxterm' satirlari olmali!\n");
    kprintf("\nKhazar OS Kabuk Modu'na hosgeldiniz!\n");
    shell_init();
    while (1) {
      shell_update();
      __asm__ __volatile__("hlt");
    }
  }

  gfx_init();
  serial_write_string("[DEBUG] gfx_init tamamlandi\n");

  serial_write_string("[DEBUG] VBE basarili - GUI baslatiliyor\n");
  kprintf("[Step 10] VBE: BASARILI\n");

  // --- Boot Splash Screen ---
  // Merkezi koordinatları ekran boyutuna göre hesapla
  struct vbe_info *vbe_inf = vbe_get_info();
  int sw = (int)vbe_inf->width;
  int sh = (int)vbe_inf->height;
  int cx = sw / 2;
  int cy = sh / 2;

  gfx_clear(0x00000066); // Dark Blueish
  gfx_swap_buffers();

  for (volatile int i = 0; i < 5000000; i++)
    ;
  gfx_puts(cx - 84, cy,      "Initializing Mouse...",       0x00AAAAAA);
  gfx_swap_buffers();
  mouse_init();

  for (volatile int i = 0; i < 3000000; i++)
    ;
  gfx_puts(cx - 100, cy + 20, "Starting Window Manager...", 0x00AAAAAA);
  gfx_swap_buffers();
  rtc_init();
  events_init();
  wm_init();
  taskbar_init();

  // Initialize shell AFTER GUI elements are ready
  // so the logo is not scrolled away by Step 9 SUCCESS prints
  shell_init();

  for (volatile int i = 0; i < 3000000; i++)
    ;

  kprintf("SUCCESS (GUI Active)\n");
  kprintf("\nWelcome to Khazar OS GUI Mode!\n");

  /* GUI ana döngüsü: Back-buffer + Dirty Rect (cursor save/restore) */
  while (1) {
    // 60Hz frame cap: tick dəyişməyənə qədər idle ol
    uint32_t t0 = pit_get_ticks();

    /* 1. Arka plan + pencere + taskbar (tam çizim) */
    gfx_clear(0x00336699);
    wm_update();
    taskbar_update();
    shell_update(); // Process terminal input in GUI mode
    wm_draw();
    taskbar_draw();

    /* 2. Tam çizim sonrası imleç yedeğini geçersiz kıl (restore atlansın) */
    mouse_invalidate_cursor();
    /* 3. İmleç: yedekle → çiz (save_buffer + sprite + alpha) */
    mouse_draw_cursor();

    /* 4. LFB'ye yansıt (Double Buffering) */
    gfx_swap_buffers();

    // 5. Shell komutlarını işle
    shell_update();

    /* CPU idle: növbəti PIT tick-ə qədər yat */
    while (pit_get_ticks() == t0) {
      __asm__ __volatile__("hlt");
    }
  }
}
