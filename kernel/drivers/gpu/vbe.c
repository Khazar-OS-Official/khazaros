#include <drivers/vbe.h>
#include <drivers/serial.h>
#include <drivers/vga.h>
#include <mm/vmm.h>

/* ---------------------------------------------------------------
 * VBE / GOP Framebuffer Driver
 * Desteklenen donanım: BIOS VBE + UEFI GOP (GRUB gfxpayload=keep)
 * Intel HD 3000 dahil gerçek donanım için güvenli 64-bit handle.
 * --------------------------------------------------------------- */

static struct vbe_info vbe;

/* LFB için sabit sanal adres: 0xE0000000 (3.5GB+)
 * 0xFD000000 yerine bu kullanılıyor çünkü bazı donanımlarda
 * fiziksel LFB 0xExxxxxxx civarında ve çakışma olabilir. */
#define VBE_VIRT_LFB   0xE0000000U
#define VBE_MAP_SIZE   0x00800000U   /* 8 MB eşleme */

void vbe_init(struct multiboot_info *mbi, uint32_t magic) {
    serial_write_string("[VBE] vbe_init called\n");

    /* --- 1. Magic kontrolü --- */
    if (magic != 0x2BADB002) {
        serial_write_string("[VBE] ERROR: Invalid Multiboot magic!\n");
        kprintf("VBE: ERROR - Invalid Multiboot magic 0x%x\n", magic);
        return;
    }

    /* --- 2. Framebuffer flag kontrolü (bit 12) --- */
    if (!(mbi->flags & (1 << 12))) {
        serial_write_string("[VBE] ERROR: Bootloader did not provide framebuffer info (bit12 missing)!\n");
        kprintf("VBE: ERROR - No framebuffer info from bootloader\n");
        kprintf("VBE: Hint: Add 'set gfxpayload=keep' to grub.cfg\n");
        return;
    }

    /* --- 3. 64-bit framebuffer adresini oku ve 32-bit sınırını kontrol et --- */
    uint64_t fb_addr64 = mbi->framebuffer_addr;
    char dbg[128];

    ksprintf(dbg, "[VBE] FB Physical Address: 0x%llx\n", fb_addr64);
    serial_write_string(dbg);

    /* Adres yüksek 32-bit kısmı sıfır değilse 32-bit kernel erişemez */
    if (fb_addr64 >> 32) {
        serial_write_string("[VBE] ERROR: Framebuffer above 4GB - 32-bit kernel cannot handle!\n");
        kprintf("VBE: ERROR - LFB address > 4GB (high=0x%x low=0x%x)\n",
                (uint32_t)(fb_addr64 >> 32),
                (uint32_t)(fb_addr64 & 0xFFFFFFFF));
        kprintf("VBE: Falling back to text mode.\n");
        vbe.lfb = NULL;
        return;
    }

    uint32_t phys_lfb = (uint32_t)(fb_addr64 & 0xFFFFFFFF);

    /* Adres sıfırsa bootloader framebuffer vermemiş */
    if (phys_lfb == 0) {
        serial_write_string("[VBE] ERROR: Framebuffer address is NULL!\n");
        kprintf("VBE: ERROR - framebuffer_addr is 0!\n");
        vbe.lfb = NULL;
        return;
    }

    /* --- 4. Diğer alanları oku --- */
    vbe.width  = mbi->framebuffer_width;
    vbe.height = mbi->framebuffer_height;
    vbe.pitch  = mbi->framebuffer_pitch;
    vbe.bpp    = mbi->framebuffer_bpp;
    uint8_t type = mbi->framebuffer_type;

    ksprintf(dbg, "[VBE] Res: %dx%d, BPP: %d, Pitch: %d, Type: %d\n", 
             vbe.width, vbe.height, vbe.bpp, vbe.pitch, type);
    serial_write_string(dbg);

    /* Type 1 = Packed (RGB) Linear Framebuffer - ideal
     * Type 0 = UEFI GOP bəzən bunu göstərir amma LFB yenə RGB-dir
     * Type 2 = EGA text modu - grafik işləmir
     *
     * REAL HARDWARE FIX: type==0 da qəbul et.
     * Intel HD 3000 + UEFI GRUB bəzən type=0 göndərir,
     * amma framebuffer fiziki ünvanı və ölçüləri düzgündür.
     * type==2 (text) olduqda isə həqiqətən reject et. */
    if (type == 2) {
        serial_write_string("[VBE] ERROR: Framebuffer is EGA text mode (type=2)! Falling back.\n");
        kprintf("VBE: ERROR - FB type=2 (EGA text). Grafik modu yoxdur.\n");
        vbe.lfb = NULL;
        return;
    }
    if (type != 1) {
        serial_write_string("[VBE] WARNING: FB type != 1, proceeding anyway (UEFI GOP quirk).\n");
        kprintf("VBE: WARNING - FB type=%d (ideal=1). Real HW quirk - proceeding.\n", type);
    }

    /* BPP kontrol: sadece 32-bit destekleniyor */
    if (vbe.bpp != 32) {
        serial_write_string("[VBE] WARNING: BPP != 32, colors may be wrong or crash! Force proceeding.\n");
        kprintf("VBE: WARNING - BPP=%d (expected 32). Proceeding anyway.\n", vbe.bpp);
    }

    /* Boyutlar makul mu? */
    if (vbe.width == 0 || vbe.height == 0 || vbe.pitch == 0) {
        serial_write_string("[VBE] ERROR: Zero width/height/pitch!\n");
        kprintf("VBE: ERROR - Invalid framebuffer dimensions!\n");
        vbe.lfb = NULL;
        return;
    }

    /* --- 5. LFB'yi sanal adrese eşle ---
     * Donanımda LFB adresi 4KB hizalı olmayabilir (GOP'ta nadir de olsa).
     * Bu yüzden hizalama yapıp fazla sayfa mapliyoruz. */
    uint32_t phys_lfb_aligned = phys_lfb & ~0xFFFU;
    uint32_t offset = phys_lfb - phys_lfb_aligned;
    uint32_t virt_lfb = VBE_VIRT_LFB;
    
    uint32_t map_size = vbe.pitch * vbe.height + offset;
    /* En az 8MB eşle, daha fazla gerekiyorsa artır */
    if (map_size < VBE_MAP_SIZE) map_size = VBE_MAP_SIZE;
    /* 4KB hizala (yukarı) */
    map_size = (map_size + 4095) & ~4095U;
    /* Güvenlik: max 16MB ile sınırla (vmm 0xE0000000-0xF0000000 aralığı) */
    if (map_size > 0x01000000U) map_size = 0x01000000U;

    ksprintf(dbg, "[VBE] Mapping LFB: PhysAligned=0x%x, Offset=%d, Size=%d bytes\n", 
             phys_lfb_aligned, offset, map_size);
    serial_write_string(dbg);

    serial_write_string("[VBE] Starting vmm_map_page loop...\n");
    for (uint32_t i = 0; i < map_size; i += 4096) {
        if (!vmm_map_page((void *)(phys_lfb_aligned + i),
                          (void *)(virt_lfb + i),
                          VMM_PRESENT | VMM_WRITABLE)) {
            serial_write_string("[VBE] ERROR: vmm_map_page failed!\n");
            kprintf("VBE: ERROR - vmm_map_page failed at offset 0x%x\n", i);
            vbe.lfb = NULL;
            return;
        }
    }

    /* Gerçek LFB başlangıcı (offset ekleme yapılmış hali) */
    vbe.lfb = (uint32_t *)(virt_lfb + offset);
    serial_write_string("[VBE] LFB mapped successfully\n");
    kprintf("VBE: LFB mapped: phys=0x%x -> virt=0x%p  size=%d bytes\n",
            phys_lfb, vbe.lfb, map_size);

    /* --- 6. LFB hazır --- */
    /* LFB artıq istifadəyə hazırdır. Debug test pikseli yazılmır,
     * çünki birbaşa LFB-ə yazmaq double-buffer sinxronizasiyasını pozur
     * (frontbuffer sıfır qalır, gfx_swap_buffers() üzərinə yazmır). */
    serial_write_string("[VBE] LFB ready. Double-buffer will handle rendering.\n");
    kprintf("VBE: Init complete. Resolution %dx%dx%d\n",
            vbe.width, vbe.height, vbe.bpp);
}

struct vbe_info *vbe_get_info(void) { return &vbe; }
