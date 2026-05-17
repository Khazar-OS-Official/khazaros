#include <drivers/ata.h>
#include <arch/io.h>
#include <drivers/vga.h>
#include <drivers/serial.h>

// ── Disk məlumatı saxlama ──────────────────────────────────────────────────
static ata_drive_info_t drive_info[2];  // [0] = Master, [1] = Slave

// ── Yardımcı: 400ns gözləmə (alternate status 4 dəfə oxu) ─────────────────
static void ata_io_wait(void) {
  inb(ATA_ALT_STATUS);
  inb(ATA_ALT_STATUS);
  inb(ATA_ALT_STATUS);
  inb(ATA_ALT_STATUS);
}

// ── BSY bitini gözlə (timeout ilə) ─────────────────────────────────────────
static bool ata_wait_bsy(void) {
  for (int timeout = 100000; timeout > 0; timeout--) {
    if (!(inb(ATA_STATUS) & ATA_STATUS_BSY)) return true;
  }
  serial_write_string("[ATA] Timeout waiting for BSY clear!\n");
  return false;
}

// ── DRQ bitini gözlə (timeout ilə) ─────────────────────────────────────────
static bool ata_wait_drq(void) {
  for (int timeout = 100000; timeout > 0; timeout--) {
    uint8_t st = inb(ATA_STATUS);
    if (st & ATA_STATUS_ERR) {
      serial_write_string("[ATA] Error bit set while waiting DRQ!\n");
      return false;
    }
    if (st & ATA_STATUS_DRQ) return true;
  }
  serial_write_string("[ATA] Timeout waiting for DRQ!\n");
  return false;
}

// ── Stringleri IDENTIFY word dizisindən oxu (byte swap) ───────────────────
static void ata_copy_string(char *dst, uint16_t *words, int word_start,
                             int word_count) {
  int di = 0;
  for (int i = word_start; i < word_start + word_count; i++) {
    dst[di++] = (char)(words[i] >> 8);    // high byte əvvəl
    dst[di++] = (char)(words[i] & 0xFF);  // low byte sonra
  }
  // Sona null əlavə et, sağdan boşluqları sil
  dst[di] = '\0';
  for (int i = di - 1; i >= 0 && dst[i] == ' '; i--) dst[i] = '\0';
}

// ── IDENTIFY komutu ─────────────────────────────────────────────────────────
void ata_identify(uint8_t drive) {
  int idx = (drive == ATA_SLAVE) ? 1 : 0;
  drive_info[idx].present = false;

  kprintf("  ATA: Identify drive 0x%x... ", drive);

  outb(ATA_DRIVE_SELECT, drive);
  ata_io_wait();

  // Sektor adres registerlərini sıfırla
  outb(ATA_SECTOR_COUNT, 0);
  outb(ATA_LBA_LOW,      0);
  outb(ATA_LBA_MID,      0);
  outb(ATA_LBA_HIGH,     0);
  outb(ATA_COMMAND,      ATA_CMD_IDENTIFY);

  ata_io_wait();

  // Status 0 veya 0xFF → disk yoxdur / floating bus
  uint8_t status = inb(ATA_STATUS);
  if (status == 0 || status == 0xFF) {
    kprintf("NOT PRESENT (status 0x%x)\n", status);
    return;
  }

  // BSY düşənə qədər gözlə
  if (!ata_wait_bsy()) {
    kprintf("TIMEOUT\n");
    return;
  }

  // ATAPI yoxlaması (non-ATA cihaz)
  uint8_t mid = inb(ATA_LBA_MID);
  uint8_t hi  = inb(ATA_LBA_HIGH);
  if (mid != 0 || hi != 0) {
    kprintf("ATAPI (CD-ROM?)\n");
    return;
  }

  // DRQ gəlsin
  if (!ata_wait_drq()) {
    kprintf("ERROR\n");
    return;
  }

  // 256 word (512 byte) IDENTIFY data oxu
  uint16_t identify_data[256];
  for (int i = 0; i < 256; i++) {
    identify_data[i] = inw(ATA_DATA);
  }

  // Məlumatları parse et
  drive_info[idx].present = true;
  drive_info[idx].cylinders        = identify_data[1];
  drive_info[idx].heads            = identify_data[3];
  drive_info[idx].sectors_per_track= identify_data[6];

  // 28-bit LBA total sector count (word 60-61)
  drive_info[idx].total_sectors =
      (uint32_t)identify_data[60] | ((uint32_t)identify_data[61] << 16);

  // Model string (word 27–46)
  ata_copy_string(drive_info[idx].model,  identify_data, 27, 20);
  // Serial number (word 10–19)
  ata_copy_string(drive_info[idx].serial, identify_data, 10, 10);

  // MB cinsindən ölçü
  uint32_t size_mb = drive_info[idx].total_sectors / 2048;

  kprintf("OK\n");
  kprintf("    Model  : %s\n",   drive_info[idx].model);
  kprintf("    Serial : %s\n",   drive_info[idx].serial);
  kprintf("    Size   : %d MB (%d sectors)\n",
          size_mb, drive_info[idx].total_sectors);

  // Serial porta da çap et
  serial_write_string("[ATA] Drive found: ");
  serial_write_string(drive_info[idx].model);
  serial_write_string("\n");
}

// ── Sektor oxuma (LBA28 PIO) ────────────────────────────────────────────────
bool ata_read_sectors(uint8_t drive, uint32_t lba, uint8_t count,
                      uint16_t *buffer) {
  if (!ata_wait_bsy()) return false;

  uint8_t drive_sel =
      (uint8_t)(0xE0 | ((drive == ATA_SLAVE) ? 0x10 : 0x00) |
                ((lba >> 24) & 0x0F));

  outb(ATA_DRIVE_SELECT, drive_sel);
  ata_io_wait();

  outb(ATA_FEATURES,     0x00);
  outb(ATA_SECTOR_COUNT, count);
  outb(ATA_LBA_LOW,      (uint8_t) lba);
  outb(ATA_LBA_MID,      (uint8_t)(lba >> 8));
  outb(ATA_LBA_HIGH,     (uint8_t)(lba >> 16));
  outb(ATA_COMMAND,      ATA_CMD_READ_PIO);

  for (int j = 0; j < count; j++) {
    if (!ata_wait_bsy())  return false;
    if (!ata_wait_drq())  return false;

    // Hata kontrolü
    if (inb(ATA_STATUS) & ATA_STATUS_DF) {
      serial_write_string("[ATA] Drive fault on read!\n");
      return false;
    }

    for (int i = 0; i < 256; i++) {
      buffer[j * 256 + i] = inw(ATA_DATA);
    }
    ata_io_wait();
  }

  return true;
}

// ── Sektor yazma (LBA28 PIO) ────────────────────────────────────────────────
bool ata_write_sectors(uint8_t drive, uint32_t lba, uint8_t count,
                       uint16_t *buffer) {
  if (!ata_wait_bsy()) return false;

  uint8_t drive_sel =
      (uint8_t)(0xE0 | ((drive == ATA_SLAVE) ? 0x10 : 0x00) |
                ((lba >> 24) & 0x0F));

  outb(ATA_DRIVE_SELECT, drive_sel);
  ata_io_wait();

  outb(ATA_FEATURES,     0x00);
  outb(ATA_SECTOR_COUNT, count);
  outb(ATA_LBA_LOW,      (uint8_t) lba);
  outb(ATA_LBA_MID,      (uint8_t)(lba >> 8));
  outb(ATA_LBA_HIGH,     (uint8_t)(lba >> 16));
  outb(ATA_COMMAND,      ATA_CMD_WRITE_PIO);

  for (int j = 0; j < count; j++) {
    if (!ata_wait_bsy())  return false;
    if (!ata_wait_drq())  return false;

    for (int i = 0; i < 256; i++) {
      outw(ATA_DATA, buffer[j * 256 + i]);
    }
    ata_io_wait();
  }

  // Cache flush
  outb(ATA_COMMAND, ATA_CMD_CACHE_FLUSH);
  if (!ata_wait_bsy()) return false;

  return true;
}

// ── Getter API ──────────────────────────────────────────────────────────────
const ata_drive_info_t *ata_get_drive_info(uint8_t drive) {
  int idx = (drive == ATA_SLAVE) ? 1 : 0;
  if (!drive_info[idx].present) return NULL;
  return &drive_info[idx];
}

uint32_t ata_get_sector_count(uint8_t drive) {
  int idx = (drive == ATA_SLAVE) ? 1 : 0;
  return drive_info[idx].present ? drive_info[idx].total_sectors : 0;
}

// ── Init ────────────────────────────────────────────────────────────────────
void ata_init(void) {
  // Software reset
  outb(ATA_DEV_CTRL, 0x04);  // SRST bit
  ata_io_wait();
  outb(ATA_DEV_CTRL, 0x00);  // Normal operation
  ata_io_wait();

  ata_identify(ATA_MASTER);
  ata_identify(ATA_SLAVE);
}
