#include <drivers/pci.h>
#include <drivers/serial.h>
#include <arch/io.h>

#define RTL8139_VENDOR_ID 0x10EC
#define RTL8139_DEVICE_ID 0x8139

static bool rtl_ready = false;
static uint32_t io_base = 0;

void rtl8139_init(void) {
  const pci_device_t *dev = pci_find_device(RTL8139_VENDOR_ID, RTL8139_DEVICE_ID);
  if (!dev) {
    serial_kprintf("[RTL8139] Device not found.\n");
    return;
  }

  // Bar0
  uint32_t bar0 = pci_read32(dev->bus, dev->dev, dev->func, 0x10);
  io_base = bar0 & ~3;

  serial_kprintf("[RTL8139] Found at IO base 0x%x\n", io_base);

  // Power on the device
  outb(io_base + 0x52, 0x00);

  // Software reset
  outb(io_base + 0x37, 0x10);
  while ((inb(io_base + 0x37) & 0x10) != 0) {
    // Wait for reset
  }

  serial_kprintf("[RTL8139] Hardware Reset Complete.\n");
  rtl_ready = true;
}
