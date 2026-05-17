#ifndef ATA_H
#define ATA_H

#include <libk/types.h>

// ── ATA I/O Portları (Primary Bus) ──────────────────────────────────────────
#define ATA_DATA         0x1F0
#define ATA_ERROR        0x1F1
#define ATA_FEATURES     0x1F1
#define ATA_SECTOR_COUNT 0x1F2
#define ATA_LBA_LOW      0x1F3
#define ATA_LBA_MID      0x1F4
#define ATA_LBA_HIGH     0x1F5
#define ATA_DRIVE_SELECT 0x1F6
#define ATA_COMMAND      0x1F7
#define ATA_STATUS       0x1F7
#define ATA_ALT_STATUS   0x3F6   // Alternate status (no IRQ clear)
#define ATA_DEV_CTRL     0x3F6   // Device control

// ── ATA Komutları ────────────────────────────────────────────────────────────
#define ATA_CMD_READ_PIO    0x20
#define ATA_CMD_WRITE_PIO   0x30
#define ATA_CMD_CACHE_FLUSH 0xE7
#define ATA_CMD_IDENTIFY    0xEC

// ── Drive Seçimi ─────────────────────────────────────────────────────────────
#define ATA_MASTER 0xA0
#define ATA_SLAVE  0xB0

// ── Status Bitləri ───────────────────────────────────────────────────────────
#define ATA_STATUS_BSY  0x80  // Busy
#define ATA_STATUS_DRDY 0x40  // Drive Ready
#define ATA_STATUS_DF   0x20  // Drive Fault
#define ATA_STATUS_DSC  0x10  // Seek Complete
#define ATA_STATUS_DRQ  0x08  // Data Request Ready
#define ATA_STATUS_CORR 0x04  // Corrected Data
#define ATA_STATUS_IDX  0x02  // Index
#define ATA_STATUS_ERR  0x01  // Error

// ── Disk məlumat strukturu ───────────────────────────────────────────────────
typedef struct {
  bool     present;
  uint32_t total_sectors;
  char     model[41];    // Null-terminated model string
  char     serial[21];   // Null-terminated serial string
  uint16_t cylinders;
  uint16_t heads;
  uint16_t sectors_per_track;
} ata_drive_info_t;

// ── API ──────────────────────────────────────────────────────────────────────
void     ata_init(void);
void     ata_identify(uint8_t drive);

bool     ata_read_sectors(uint8_t drive, uint32_t lba, uint8_t count,
                          uint16_t *buffer);
bool     ata_write_sectors(uint8_t drive, uint32_t lba, uint8_t count,
                           uint16_t *buffer);

// Returns pointer to internal drive info (NULL if not present)
const ata_drive_info_t *ata_get_drive_info(uint8_t drive);

// Returns total sectors for the given drive (0 if not present)
uint32_t ata_get_sector_count(uint8_t drive);

#endif // ATA_H
