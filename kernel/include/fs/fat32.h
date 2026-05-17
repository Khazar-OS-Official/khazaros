#ifndef FAT32_H
#define FAT32_H

#include <libk/types.h>
#include <fs/vfs.h>

// FAT32 Boot Record (BPB) Yapısı
struct fat32_bpb {
  uint8_t jmp[3];
  char oem[8];
  uint16_t bytes_per_sector;
  uint8_t sectors_per_cluster;
  uint16_t reserved_sectors;
  uint8_t fat_count;
  uint16_t root_entry_count;
  uint16_t total_sectors_short;
  uint8_t media_type;
  uint16_t fat_size_short;
  uint16_t sectors_per_track;
  uint16_t head_count;
  uint32_t hidden_sectors;
  uint32_t total_sectors_long;

  // FAT32 Extended Fields
  uint32_t fat_size_long;
  uint16_t flags;
  uint16_t version;
  uint32_t root_cluster;
  uint16_t fs_info_sector;
  uint16_t backup_boot_sector;
  uint8_t reserved[12];
  uint8_t drive_number;
  uint8_t reserved1;
  uint8_t boot_signature;
  uint32_t volume_id;
  char volume_label[11];
  char fs_type[8];
} __attribute__((packed));

// FAT32 Directory Entry
struct fat32_dir_entry {
  uint8_t name[11];
  uint8_t attr;
  uint8_t reserved;
  uint8_t creation_time_tenth;
  uint16_t creation_time;
  uint16_t creation_date;
  uint16_t last_access_date;
  uint16_t cluster_high;
  uint16_t last_mod_time;
  uint16_t last_mod_date;
  uint16_t cluster_low;
  uint32_t size;
} __attribute__((packed));

// FAT32 Fonksiyonları
bool fat32_init(void);
void fat32_ls(void);
uint8_t *fat32_read_file(const char *filename, uint32_t *size);

// VFS Adapter
vfs_node_t *fat32_get_root(void);

#endif // FAT32_H
