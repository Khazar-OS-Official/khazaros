#include <drivers/ac97.h>
#include <drivers/pci.h>
#include <drivers/serial.h>
#include <drivers/vga.h>
#include <libk/string.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <arch/isr.h>

static uint32_t nambar = 0;
static uint32_t nabmbar = 0;
static bool ac97_ready = false;

// Buffer descriptor list for DMA
typedef struct {
    uint32_t pointer;
    uint16_t length;
    uint16_t reserved : 14;
    uint8_t bu : 1;
    uint8_t ioc : 1;
} __attribute__((packed)) ac97_bdl_entry_t;

static ac97_bdl_entry_t *bdl = NULL;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t val;
    __asm__ volatile("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

void ac97_init(void) {
    const pci_device_t *dev = pci_find_class(0x04, 0x01); // Multimedia, Audio
    if (!dev) {
        serial_write_string("[AC97] No AC97 Audio Controller found.\n");
        return;
    }

    nambar = pci_read32(dev->bus, dev->dev, dev->func, 0x10) & ~1;
    nabmbar = pci_read32(dev->bus, dev->dev, dev->func, 0x14) & ~1;
    
    if (nambar == 0 || nabmbar == 0) {
        serial_write_string("[AC97] Invalid base addresses.\n");
        return;
    }
    
    // Enable Bus Mastering
    uint16_t command = pci_read16(dev->bus, dev->dev, dev->func, 0x04);
    pci_write16(dev->bus, dev->dev, dev->func, 0x04, command | 0x0005); // I/O Space + Bus Master
    
    char dbg[64];
    ksprintf(dbg, "[AC97] Device found at NAMBAR: 0x%x, NABMBAR: 0x%x\n", nambar, nabmbar);
    serial_write_string(dbg);

    // Reset mixer
    outw(nambar + 0x00, 1);
    
    // Setup Volume (Master and PCM) to 0x0000 (Max Volume)
    outw(nambar + 0x02, 0x0000); // Master Volume
    outw(nambar + 0x18, 0x0000); // PCM Out Volume

    // Allocate Buffer Descriptor List (BDL)
    void *bdl_phys = pmm_alloc_block();
    bdl = (ac97_bdl_entry_t *)P2V(bdl_phys);
    memset(bdl, 0, 4096);
    
    // Point PCM Out (PO) to BDL
    outl(nabmbar + 0x10 + 0x04, (uint32_t)bdl_phys);
    
    ac97_ready = true;
    serial_write_string("[AC97] Initialized successfully.\n");
}

void ac97_play_sample(void *buffer, uint32_t size) {
    if (!ac97_ready || !buffer) return;
    
    // Basic single-buffer play (Wait until not running)
    uint8_t cr = inb(nabmbar + 0x10 + 0x0B);
    if (cr & 1) return; // Already playing
    
    // We assume buffer is physical or identity mapped for DMA
    // We must pass physical address to BDL
    uint32_t phys = (uint32_t)buffer;
    if (phys >= 0xC0000000) phys -= 0xC0000000;
    
    bdl[0].pointer = phys;
    bdl[0].length = size / 2; // In 16-bit samples
    bdl[0].ioc = 1;
    bdl[0].bu = 0; // End of list
    
    // Set Last Valid Index
    outb(nabmbar + 0x10 + 0x05, 0);
    
    // Start play
    outb(nabmbar + 0x10 + 0x0B, 1); // Run/Pause
}
