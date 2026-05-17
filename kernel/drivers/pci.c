#include <drivers/pci.h>
#include <arch/io.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC

#define PCI_MAX_DEVICES 128

static pci_device_t pci_devices[PCI_MAX_DEVICES];
static uint32_t pci_device_count = 0;

static uint32_t pci_make_addr(uint8_t bus, uint8_t dev, uint8_t func,
                              uint8_t offset) {
  return (1u << 31) | ((uint32_t)bus << 16) | ((uint32_t)dev << 11) |
         ((uint32_t)func << 8) | (offset & 0xFC);
}

uint32_t pci_read32(uint8_t bus, uint8_t dev, uint8_t func,
                           uint8_t offset) {
  outl(PCI_CONFIG_ADDRESS, pci_make_addr(bus, dev, func, offset));
  return inl(PCI_CONFIG_DATA);
}

uint16_t pci_read16(uint8_t bus, uint8_t dev, uint8_t func,
                           uint8_t offset) {
  uint32_t v = pci_read32(bus, dev, func, offset);
  uint8_t shift = (offset & 2) ? 16 : 0;
  return (uint16_t)((v >> shift) & 0xFFFF);
}

void pci_write32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t val) {
  outl(PCI_CONFIG_ADDRESS, pci_make_addr(bus, dev, func, offset));
  outl(PCI_CONFIG_DATA, val);
}

void pci_write16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint16_t val) {
  uint32_t v = pci_read32(bus, dev, func, offset);
  if (offset & 2) {
    v = (v & 0x0000FFFF) | ((uint32_t)val << 16);
  } else {
    v = (v & 0xFFFF0000) | (uint32_t)val;
  }
  pci_write32(bus, dev, func, offset, v);
}

static uint8_t pci_read8(uint8_t bus, uint8_t dev, uint8_t func,
                         uint8_t offset) {
  uint32_t v = pci_read32(bus, dev, func, offset);
  uint8_t shift = (offset & 3) * 8;
  return (uint8_t)((v >> shift) & 0xFF);
}

#include <drivers/serial.h>

static void pci_try_add(uint8_t bus, uint8_t dev, uint8_t func) {
  uint16_t vendor = pci_read16(bus, dev, func, 0x00);
  if (vendor == 0xFFFF)
    return;

  if (pci_device_count >= PCI_MAX_DEVICES)
    return;

  pci_device_t *d = &pci_devices[pci_device_count++];
  d->bus = bus;
  d->dev = dev;
  d->func = func;
  d->vendor_id = vendor;
  d->device_id = pci_read16(bus, dev, func, 0x02);
  d->revision = pci_read8(bus, dev, func, 0x08);
  d->prog_if = pci_read8(bus, dev, func, 0x09);
  d->subclass = pci_read8(bus, dev, func, 0x0A);
  d->class_code = pci_read8(bus, dev, func, 0x0B);

  // Print device type
  if (d->class_code == 0x01 && d->subclass == 0x06) {
    serial_kprintf("[PCI] Found SATA AHCI Controller (Vendor: %x, Dev: %x)\n", d->vendor_id, d->device_id);
  } else if (d->class_code == 0x01 && d->subclass == 0x01) {
    serial_kprintf("[PCI] Found IDE Controller (Vendor: %x, Dev: %x)\n", d->vendor_id, d->device_id);
  } else if (d->class_code == 0x0C && d->subclass == 0x03) {
    serial_kprintf("[PCI] Found USB Controller (ProgIF: %x, Vendor: %x)\n", d->prog_if, d->vendor_id);
  } else if (d->vendor_id == 0x10EC && (d->device_id == 0x8139 || d->device_id == 0x8168)) {
    serial_kprintf("[PCI] Found Realtek Network Card (%x)\n", d->device_id);
  } else if (d->vendor_id == 0x8086 && d->class_code == 0x02) {
    serial_kprintf("[PCI] Found Intel Network Card (%x)\n", d->device_id);
  } else if (d->class_code == 0x03) {
    serial_kprintf("[PCI] Found VGA/Display Controller (Vendor: %x)\n", d->vendor_id);
  }
}

void pci_init(void) {
  pci_device_count = 0;

  for (uint16_t bus = 0; bus < 256; bus++) {
    for (uint8_t dev = 0; dev < 32; dev++) {
      // Funksiya 0 oxu – yoxdursa skip
      uint16_t vendor0 = pci_read16((uint8_t)bus, dev, 0, 0x00);
      if (vendor0 == 0xFFFF)
        continue;

      // Header type – multifunction mu?
      uint8_t header_type = pci_read8((uint8_t)bus, dev, 0, 0x0E);
      uint8_t multi = header_type & 0x80;

      // Add func0
      pci_try_add((uint8_t)bus, dev, 0);

      if (multi) {
        for (uint8_t func = 1; func < 8; func++) {
          pci_try_add((uint8_t)bus, dev, func);
        }
      }
    }
  }
}

uint32_t pci_get_device_count(void) { return pci_device_count; }

const pci_device_t *pci_get_device(uint32_t idx) {
  if (idx >= pci_device_count)
    return NULL;
  return &pci_devices[idx];
}

const pci_device_t *pci_find_class(uint8_t class_code, uint8_t subclass) {
  for (uint32_t i = 0; i < pci_device_count; i++) {
    if (pci_devices[i].class_code == class_code &&
        pci_devices[i].subclass == subclass) {
      return &pci_devices[i];
    }
  }
  return NULL;
}

const pci_device_t *pci_find_device(uint16_t vendor_id, uint16_t device_id) {
  for (uint32_t i = 0; i < pci_device_count; i++) {
    if (pci_devices[i].vendor_id == vendor_id &&
        pci_devices[i].device_id == device_id) {
      return &pci_devices[i];
    }
  }
  return NULL;
}

