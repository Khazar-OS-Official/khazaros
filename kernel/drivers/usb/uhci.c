#include <drivers/uhci.h>
#include <drivers/pci.h>
#include <drivers/serial.h>
#include <drivers/vga.h>
#include <libk/string.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

static uint32_t io_base = 0;

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t val;
    __asm__ volatile("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

void uhci_init(void) {
    // Find PCI Class 0x0C (Serial Bus), Subclass 0x03 (USB), Prog IF 0x00 (UHCI)
    // pci_find_class searches by class and subclass only in our implementation,
    // but let's just find the first USB controller.
    const pci_device_t *dev = pci_find_class(0x0C, 0x03);
    if (!dev) {
        serial_write_string("[UHCI] No USB Controller found.\n");
        return;
    }

    uint32_t bar4 = pci_read32(dev->bus, dev->dev, dev->func, 0x20); // BAR4 is often used for UHCI I/O
    if ((bar4 & 1) == 0) {
        // If not I/O, try BAR0
        bar4 = pci_read32(dev->bus, dev->dev, dev->func, 0x10);
        if ((bar4 & 1) == 0) {
            serial_write_string("[UHCI] I/O BAR not found for USB.\n");
            return;
        }
    }

    io_base = bar4 & ~3; // Mask out the I/O indicator bits

    // Enable Bus Mastering
    uint16_t command = pci_read16(dev->bus, dev->dev, dev->func, 0x04);
    pci_write16(dev->bus, dev->dev, dev->func, 0x04, command | 0x0005);

    char dbg[64];
    ksprintf(dbg, "[UHCI] USB Controller found at I/O Base 0x%x\n", io_base);
    serial_write_string(dbg);

    // Global Reset
    outw(io_base + 0x00, 0x0004); // USBCMD: Global Reset
    for(volatile int i=0; i<100000; i++); // Wait 50ms (crude loop)
    outw(io_base + 0x00, 0x0000); // Clear Reset

    // Allocate Frame List
    void *frame_list_phys = pmm_alloc_block();
    uint32_t *frame_list = (uint32_t *)P2V(frame_list_phys);
    for (int i = 0; i < 1024; i++) {
        frame_list[i] = 1; // Terminate (T bit set)
    }

    // Set Frame List Base Address
    outl(io_base + 0x08, (uint32_t)frame_list_phys); // FRBASEADD

    // Reset Port 1 and Port 2
    outw(io_base + 0x10, 0x0200); // Port 1 Reset
    outw(io_base + 0x12, 0x0200); // Port 2 Reset
    for(volatile int i=0; i<100000; i++); // Wait 50ms
    
    outw(io_base + 0x10, 0x0000);
    outw(io_base + 0x12, 0x0000);

    // Start Controller
    outw(io_base + 0x00, 0x0001); // Run/Stop bit

    serial_write_string("[UHCI] Basic USB Stack initialized.\n");
}
