#include <drivers/ata.h>
#include <drivers/ahci.h>
#include <drivers/vga.h>
#include <fs/fat32.h>
#include <libk/string.h>
#include <mm/kheap.h>

static struct fat32_bpb bpb;
static bool fat32_ready = false;
static uint32_t first_data_sector = 0;
static uint32_t sectors_per_fat = 0;
static uint8_t fat32_drive = ATA_MASTER; // Used only for ATA fallback
static volatile hba_port_t *fat32_ahci_port = NULL;

static bool disk_read_sectors(uint32_t lba, uint32_t count, uint16_t *buffer) {
    if (fat32_ahci_port) {
        return ahci_read((hba_port_t*)fat32_ahci_port, lba, count, buffer);
    } else {
        return ata_read_sectors(fat32_drive, lba, count, buffer);
    }
}

static bool disk_write_sectors(uint32_t lba, uint32_t count, uint16_t *buffer) {
    if (fat32_ahci_port) {
        return ahci_write((hba_port_t*)fat32_ahci_port, lba, count, buffer);
    } else {
        return ata_write_sectors(fat32_drive, lba, count, buffer);
    }
}

// Belirli bir drive uzerinde FAT32 ara
static bool fat32_probe(uint8_t drive) {
  kprintf("FAT32: Probing drive 0x%x... ", drive);

  uint8_t *buffer = (uint8_t *)kmalloc(512);
  if (!buffer) return false;

  if (!disk_read_sectors(0, 1, (uint16_t *)buffer)) {
    kprintf("READ ERROR\n");
    kfree(buffer);
    return false;
  }

  struct fat32_bpb *temp_bpb = (struct fat32_bpb *)buffer;
  if (temp_bpb->boot_signature != 0x29 && temp_bpb->boot_signature != 0x28) {
    kprintf("INVALID SIGNATURE (0x%x)\n", temp_bpb->boot_signature);
    kfree(buffer);
    return false;
  }

  memcpy(&bpb, buffer, sizeof(struct fat32_bpb));
  fat32_drive = drive;

  sectors_per_fat = bpb.fat_size_long;
  uint32_t root_dir_sectors = 0;
  first_data_sector = bpb.reserved_sectors + (bpb.fat_count * sectors_per_fat) + root_dir_sectors;

  kprintf("SUCCESS! (Ready)\n");
  kfree(buffer);
  return true;
}

// FAT32'yi baslat
bool fat32_init(void) {
  fat32_ready = false;
  fat32_ahci_port = ahci_get_drive();

  if (fat32_ahci_port) {
      kprintf("FAT32: AHCI Drive found. Probing...\n");
      if (fat32_probe(0)) {
          fat32_ready = true;
          return true;
      }
  }

  if (fat32_probe(ATA_MASTER)) { fat32_ready = true; return true; }
  if (fat32_probe(ATA_SLAVE)) { fat32_ready = true; return true; }

  kprintf("FAT32: No valid filesystem found on any drive.\n");
  return false;
}

// Cluster numarasini LBA sektor numarasina cevir
static uint32_t cluster_to_lba(uint32_t cluster) {
  return first_data_sector + (cluster - 2) * bpb.sectors_per_cluster;
}

// FAT tablosundan bir girisi oku
static uint32_t fat32_get_fat_entry(uint32_t cluster) {
  uint32_t fat_sector = bpb.reserved_sectors + (cluster * 4 / 512);
  uint32_t fat_offset = (cluster * 4) % 512;

  uint8_t *buffer = (uint8_t *)kmalloc(512);
  if (!buffer) return 0x0FFFFFFF;

  if (!disk_read_sectors(fat_sector, 1, (uint16_t *)buffer)) {
    kfree(buffer);
    return 0x0FFFFFFF;
  }

  uint32_t entry = *(uint32_t *)(buffer + fat_offset) & 0x0FFFFFFF;
  kfree(buffer);
  return entry;
}

// FAT tablosuna bir giris yaz (Tum FAT kopyalarina yazar)
static bool fat32_set_fat_entry(uint32_t cluster, uint32_t value) {
  uint32_t fat_sector_offset = (cluster * 4 / 512);
  uint32_t fat_offset = (cluster * 4) % 512;

  uint8_t *buffer = (uint8_t *)kmalloc(512);
  if (!buffer) return false;

  for (uint32_t i = 0; i < bpb.fat_count; i++) {
    uint32_t fat_sector = bpb.reserved_sectors + (i * sectors_per_fat) + fat_sector_offset;
    if (!disk_read_sectors(fat_sector, 1, (uint16_t *)buffer)) {
      kfree(buffer); return false;
    }
    *(uint32_t *)(buffer + fat_offset) = value & 0x0FFFFFFF;
    if (!disk_write_sectors(fat_sector, 1, (uint16_t *)buffer)) {
      kfree(buffer); return false;
    }
  }

  kfree(buffer);
  return true;
}

// Bos bir cluster bul ve rezerve et
static uint32_t fat32_allocate_cluster(void) {
  uint32_t total_clusters = bpb.fat_size_long * 512 / 4;
  for (uint32_t i = 2; i < total_clusters; i++) {
    if (fat32_get_fat_entry(i) == 0x00000000) {
      fat32_set_fat_entry(i, 0x0FFFFFFF); // EOC
      return i;
    }
  }
  return 0x0FFFFFFF;
}

static void format_fat_name(const char *src, uint8_t *dest) {
  memset(dest, ' ', 11);
  int i = 0, j = 0;
  while (src[i] != '.' && src[i] != '\0' && j < 8) {
    char c = src[i++];
    if (c >= 'a' && c <= 'z') c -= 32;
    dest[j++] = c;
  }
  while (src[i] != '.' && src[i] != '\0') i++;
  if (src[i] == '.') {
    i++;
    j = 8;
    while (src[i] != '\0' && j < 11) {
      char c = src[i++];
      if (c >= 'a' && c <= 'z') c -= 32;
      dest[j++] = c;
    }
  }
}

static bool fat32_create_entry(vfs_node_t *parent, const char *name,
                               uint8_t attr, uint32_t start_cluster,
                               uint32_t size) {
  uint32_t cluster = parent->impl;
  uint32_t lba = cluster_to_lba(cluster);

  uint8_t *buffer = (uint8_t *)kmalloc(512 * bpb.sectors_per_cluster);
  if (!buffer) return false;

  if (!disk_read_sectors(lba, bpb.sectors_per_cluster, (uint16_t *)buffer)) {
    kfree(buffer); return false;
  }

  struct fat32_dir_entry *entries = (struct fat32_dir_entry *)buffer;
  int slot = -1;

  for (int i = 0; i < 16 * bpb.sectors_per_cluster; i++) {
    if ((uint8_t)entries[i].name[0] == 0x00 || (uint8_t)entries[i].name[0] == 0xE5) {
      slot = i; break;
    }
  }

  if (slot == -1) {
    kprintf("FAT32: Error - No free slots in directory cluster!\n");
    kfree(buffer); return false;
  }

  memset(&entries[slot], 0, sizeof(struct fat32_dir_entry));
  format_fat_name(name, entries[slot].name);
  entries[slot].attr = attr;
  entries[slot].cluster_low = start_cluster & 0xFFFF;
  entries[slot].cluster_high = (start_cluster >> 16) & 0xFFFF;
  entries[slot].size = size;

  if (!disk_write_sectors(lba, bpb.sectors_per_cluster, (uint16_t *)buffer)) {
    kprintf("FAT32: Write error while creating entry\n");
    kfree(buffer); return false;
  }

  kfree(buffer);
  return true;
}

void fat32_ls(void) {
  if (!fat32_ready) { kprintf("FAT32: Not initialized!\n"); return; }

  uint32_t root_lba = cluster_to_lba(bpb.root_cluster);
  uint8_t *buffer = (uint8_t *)kmalloc(512 * bpb.sectors_per_cluster);
  if (!buffer) return;

  disk_read_sectors(root_lba, bpb.sectors_per_cluster, (uint16_t *)buffer);
  struct fat32_dir_entry *entries = (struct fat32_dir_entry *)buffer;

  kprintf("\n--- Root Directory Contents ---\n");
  for (int i = 0; i < 16; i++) {
    if ((uint8_t)entries[i].name[0] == 0x00) break;
    if ((uint8_t)entries[i].name[0] == 0xE5) continue;
    if (entries[i].attr & 0x08) continue;
    if (entries[i].attr & 0x0F) continue;

    char name[12];
    memcpy(name, entries[i].name, 11);
    name[11] = '\0';
    kprintf("%s  -  %d bytes\n", name, entries[i].size);
  }
  kprintf("-------------------------------\n");
  kfree(buffer);
}

static bool fat32_compare_name(const uint8_t *fat_name, const char *input_name) {
  uint8_t normalized[11];
  format_fat_name(input_name, normalized);
  return memcmp(fat_name, normalized, 11) == 0;
}

uint8_t *fat32_read_file(const char *filename, uint32_t *size) {
  if (!fat32_ready) return NULL;

  uint32_t root_lba = cluster_to_lba(bpb.root_cluster);
  uint8_t *buffer = (uint8_t *)kmalloc(512 * bpb.sectors_per_cluster);
  if (!buffer) return NULL;

  disk_read_sectors(root_lba, bpb.sectors_per_cluster, (uint16_t *)buffer);
  struct fat32_dir_entry *entries = (struct fat32_dir_entry *)buffer;

  for (int i = 0; i < 16; i++) {
    if (entries[i].name[0] == 0x00) break;

    if (fat32_compare_name(entries[i].name, filename)) {
      *size = entries[i].size;
      uint32_t cluster = entries[i].cluster_low | (entries[i].cluster_high << 16);
      uint32_t lba = cluster_to_lba(cluster);

      uint32_t sectors_to_read = (*size + 511) / 512;
      uint8_t *file_data = (uint8_t *)kmalloc(sectors_to_read * 512);
      if (file_data) disk_read_sectors(lba, sectors_to_read, (uint16_t *)file_data);

      kfree(buffer);
      return file_data;
    }
  }

  kfree(buffer);
  return NULL;
}

static void fat32_update_entry_size(uint32_t dir_cluster, uint32_t file_cluster, uint32_t new_size) {
  uint32_t lba = cluster_to_lba(dir_cluster);
  uint8_t *buffer = (uint8_t *)kmalloc(512 * bpb.sectors_per_cluster);
  if (!buffer) return;

  if (disk_read_sectors(lba, bpb.sectors_per_cluster, (uint16_t *)buffer)) {
    struct fat32_dir_entry *entries = (struct fat32_dir_entry *)buffer;
    for (int i = 0; i < 16 * bpb.sectors_per_cluster; i++) {
      uint32_t start = entries[i].cluster_low | (entries[i].cluster_high << 16);
      if (start == file_cluster && entries[i].name[0] != 0x00 && entries[i].name[0] != 0xE5) {
        entries[i].size = new_size;
        disk_write_sectors(lba, bpb.sectors_per_cluster, (uint16_t *)buffer);
        break;
      }
    }
  }
  kfree(buffer);
}

// --- VFS Adapter Forward Declarations ---
static uint32_t fat32_vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
static uint32_t fat32_vfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
static vfs_node_t *fat32_vfs_readdir(vfs_node_t *node, uint32_t index);
static vfs_node_t *fat32_vfs_finddir(vfs_node_t *node, char *name);
static vfs_node_t *fat32_vfs_create(vfs_node_t *node, char *name);
static bool fat32_vfs_unlink(vfs_node_t *node, char *name);
static vfs_node_t *fat32_vfs_mkdir(vfs_node_t *node, char *name);

// --- VFS Adapter Implementation ---

static struct vfs_node *fat32_vfs_create(vfs_node_t *parent, char *name) {
  if (!fat32_ready) return NULL;

  uint32_t cluster = fat32_allocate_cluster();
  if (cluster == 0x0FFFFFFF) return NULL;

  if (!fat32_create_entry(parent, name, 0x20, cluster, 0)) {
    fat32_set_fat_entry(cluster, 0x00000000);
    return NULL;
  }

  vfs_node_t *res = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
  memset(res, 0, sizeof(vfs_node_t));
  kstrncpy(res->name, name, 127);
  res->length = 0;
  res->flags = VFS_FILE;
  res->mask = 0666;
  res->uid = 0;
  res->gid = 0;
  res->impl = cluster;
  res->inode = parent->impl;
  res->read = fat32_vfs_read;
  res->write = fat32_vfs_write;
  res->readdir = fat32_vfs_readdir;
  res->finddir = fat32_vfs_finddir;
  res->create = fat32_vfs_create;
  res->unlink = fat32_vfs_unlink;
  res->mkdir = fat32_vfs_mkdir;

  return res;
}

static uint32_t fat32_vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
  if (!fat32_ready || node->length == 0 || size == 0) return 0;
  if (offset >= node->length) return 0;

  uint32_t real_size = size;
  if (offset + size > node->length) real_size = node->length - offset;

  uint32_t cluster_size = bpb.sectors_per_cluster * 512;
  uint32_t current_cluster = node->impl;

  uint32_t skip_clusters = offset / cluster_size;
  for (uint32_t i = 0; i < skip_clusters; i++) {
    current_cluster = fat32_get_fat_entry(current_cluster);
    if (current_cluster >= 0x0FFFFFF8) return 0;
  }

  uint32_t cluster_offset = offset % cluster_size;
  uint32_t bytes_read = 0;

  uint8_t *temp_buffer = (uint8_t *)kmalloc(cluster_size);
  if (!temp_buffer) return 0;

  while (bytes_read < real_size && current_cluster >= 2 && current_cluster < 0x0FFFFFF8) {
    uint32_t lba = cluster_to_lba(current_cluster);
    disk_read_sectors(lba, bpb.sectors_per_cluster, (uint16_t *)temp_buffer);

    uint32_t to_copy = cluster_size - cluster_offset;
    if (to_copy > real_size - bytes_read) to_copy = real_size - bytes_read;

    memcpy(buffer + bytes_read, temp_buffer + cluster_offset, to_copy);
    bytes_read += to_copy;
    cluster_offset = 0;

    if (bytes_read < real_size) {
      current_cluster = fat32_get_fat_entry(current_cluster);
    }
  }

  kfree(temp_buffer);
  return bytes_read;
}

static uint32_t fat32_vfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
  if (!fat32_ready || size == 0) return 0;

  uint32_t cluster_size = bpb.sectors_per_cluster * 512;
  uint32_t current_cluster = node->impl;

  if (current_cluster == 0) {
      current_cluster = fat32_allocate_cluster();
      if (current_cluster == 0x0FFFFFFF) return 0;
      node->impl = current_cluster;
  }

  uint32_t target_cluster_idx = offset / cluster_size;
  for (uint32_t i = 0; i < target_cluster_idx; i++) {
    uint32_t next = fat32_get_fat_entry(current_cluster);
    if (next >= 0x0FFFFFF8) {
        next = fat32_allocate_cluster();
        if (next == 0x0FFFFFFF) return 0;
        fat32_set_fat_entry(current_cluster, next);
    }
    current_cluster = next;
  }

  uint32_t cluster_offset = offset % cluster_size;
  uint32_t bytes_written = 0;
  uint8_t *temp_buffer = (uint8_t *)kmalloc(cluster_size);
  if (!temp_buffer) return 0;

  while (bytes_written < size) {
    uint32_t lba = cluster_to_lba(current_cluster);
    
    uint32_t to_write = cluster_size - cluster_offset;
    if (to_write > size - bytes_written) to_write = size - bytes_written;

    if (to_write < cluster_size) {
        disk_read_sectors(lba, bpb.sectors_per_cluster, (uint16_t *)temp_buffer);
    }
    
    memcpy(temp_buffer + cluster_offset, buffer + bytes_written, to_write);
    disk_write_sectors(lba, bpb.sectors_per_cluster, (uint16_t *)temp_buffer);

    bytes_written += to_write;
    cluster_offset = 0;

    if (bytes_written < size) {
        uint32_t next = fat32_get_fat_entry(current_cluster);
        if (next >= 0x0FFFFFF8) {
            next = fat32_allocate_cluster();
            if (next == 0x0FFFFFFF) break;
            fat32_set_fat_entry(current_cluster, next);
        }
        current_cluster = next;
    }
  }

  kfree(temp_buffer);

  if (offset + bytes_written > node->length) {
    node->length = offset + bytes_written;
    fat32_update_entry_size(node->inode, node->impl, node->length);
  }

  return bytes_written;
}

static vfs_node_t *fat32_vfs_readdir(vfs_node_t *node, uint32_t index) {
  if (!fat32_ready) return NULL;

  uint32_t cluster = node->impl;
  uint32_t lba = cluster_to_lba(cluster);
  uint8_t *buffer = (uint8_t *)kmalloc(512 * bpb.sectors_per_cluster);
  if (!buffer) return NULL;

  disk_read_sectors(lba, bpb.sectors_per_cluster, (uint16_t *)buffer);
  struct fat32_dir_entry *entries = (struct fat32_dir_entry *)buffer;

  uint32_t count = 0;
  for (int i = 0; i < 16 * bpb.sectors_per_cluster; i++) {
    if ((uint8_t)entries[i].name[0] == 0x00) break;
    if ((uint8_t)entries[i].name[0] == 0xE5 || (entries[i].attr & 0x08) || (entries[i].attr & 0x0F)) continue;

    if (count == index) {
      vfs_node_t *res = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
      memset(res, 0, sizeof(vfs_node_t));
      char name[12];
      int ni = 0;
      for (int k = 0; k < 8 && entries[i].name[k] != ' '; k++) name[ni++] = entries[i].name[k];
      if (entries[i].name[8] != ' ') {
        name[ni++] = '.';
        for (int k = 8; k < 11 && entries[i].name[k] != ' '; k++) name[ni++] = entries[i].name[k];
      }
      name[ni] = '\0';
      kstrncpy(res->name, name, 127);
      res->length = entries[i].size;
      res->flags = (entries[i].attr & 0x10) ? VFS_DIRECTORY : VFS_FILE;
      res->mask = (res->flags == VFS_DIRECTORY) ? 0777 : 0666;
      res->uid = 0;
      res->gid = 0;
      res->impl = entries[i].cluster_low | (entries[i].cluster_high << 16);
      res->inode = node->impl;
      res->read = fat32_vfs_read;
      res->write = fat32_vfs_write;
      res->readdir = fat32_vfs_readdir;
      res->finddir = fat32_vfs_finddir;
      res->create = fat32_vfs_create;
      res->unlink = fat32_vfs_unlink;
      res->mkdir = fat32_vfs_mkdir;
      kfree(buffer);
      return res;
    }
    count++;
  }
  kfree(buffer);
  return NULL;
}

static vfs_node_t *fat32_vfs_finddir(vfs_node_t *node, char *name) {
  if (!fat32_ready) return NULL;
  uint32_t cluster = node->impl;
  uint32_t lba = cluster_to_lba(cluster);
  uint8_t *buffer = (uint8_t *)kmalloc(512 * bpb.sectors_per_cluster);
  if (!buffer) return NULL;
  disk_read_sectors(lba, bpb.sectors_per_cluster, (uint16_t *)buffer);
  struct fat32_dir_entry *entries = (struct fat32_dir_entry *)buffer;

  for (int i = 0; i < 16 * bpb.sectors_per_cluster; i++) {
    if (entries[i].name[0] == 0x00) break;
    if (fat32_compare_name(entries[i].name, name)) {
      vfs_node_t *res = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
      memset(res, 0, sizeof(vfs_node_t));
      kstrncpy(res->name, name, 127);
      res->length = entries[i].size;
      res->flags = (entries[i].attr & 0x10) ? VFS_DIRECTORY : VFS_FILE;
      res->mask = (res->flags == VFS_DIRECTORY) ? 0777 : 0666;
      res->uid = 0;
      res->gid = 0;
      res->impl = entries[i].cluster_low | (entries[i].cluster_high << 16);
      res->inode = node->impl;
      res->read = fat32_vfs_read;
      res->write = fat32_vfs_write;
      res->readdir = fat32_vfs_readdir;
      res->finddir = fat32_vfs_finddir;
      res->create = fat32_vfs_create;
      res->unlink = fat32_vfs_unlink;
      res->mkdir = fat32_vfs_mkdir;
      kfree(buffer);
      return res;
    }
  }
  kfree(buffer);
  return NULL;
}

static bool fat32_vfs_unlink(vfs_node_t *parent, char *name) {
  if (!fat32_ready || !parent) return false;

  uint32_t cluster = parent->impl;
  uint32_t lba = cluster_to_lba(cluster);

  uint8_t *buffer = (uint8_t *)kmalloc(512 * bpb.sectors_per_cluster);
  if (!buffer) return false;

  if (!disk_read_sectors(lba, bpb.sectors_per_cluster, (uint16_t *)buffer)) {
    kfree(buffer);
    return false;
  }

  struct fat32_dir_entry *entries = (struct fat32_dir_entry *)buffer;
  uint32_t file_cluster = 0;
  bool found = false;

  for (int i = 0; i < 16 * bpb.sectors_per_cluster; i++) {
    if (entries[i].name[0] == 0x00) break;
    if ((uint8_t)entries[i].name[0] == 0xE5) continue;

    if (fat32_compare_name(entries[i].name, name)) {
      file_cluster = entries[i].cluster_low | (entries[i].cluster_high << 16);
      entries[i].name[0] = 0xE5; // Mark as deleted
      if (!disk_write_sectors(lba, bpb.sectors_per_cluster, (uint16_t *)buffer)) {
        kfree(buffer);
        return false;
      }
      found = true;
      break;
    }
  }

  kfree(buffer);

  if (!found) return false;

  // Free the cluster chain
  uint32_t current = file_cluster;
  while (current >= 2 && current < 0x0FFFFFF8) {
    uint32_t next = fat32_get_fat_entry(current);
    fat32_set_fat_entry(current, 0x00000000); // Mark free
    current = next;
  }

  return true;
}

static vfs_node_t *fat32_vfs_mkdir(vfs_node_t *parent, char *name) {
  if (!fat32_ready || !parent) return NULL;

  uint32_t cluster = fat32_allocate_cluster();
  if (cluster == 0x0FFFFFFF) return NULL;

  if (!fat32_create_entry(parent, name, 0x10, cluster, 0)) { // 0x10 = DIRECTORY
    fat32_set_fat_entry(cluster, 0x00000000);
    return NULL;
  }

  uint32_t lba = cluster_to_lba(cluster);
  uint8_t *buffer = (uint8_t *)kmalloc(512 * bpb.sectors_per_cluster);
  if (!buffer) return NULL;

  memset(buffer, 0, 512 * bpb.sectors_per_cluster);
  struct fat32_dir_entry *entries = (struct fat32_dir_entry *)buffer;

  // .
  memset(entries[0].name, ' ', 11);
  entries[0].name[0] = '.';
  entries[0].attr = 0x10;
  entries[0].cluster_low = cluster & 0xFFFF;
  entries[0].cluster_high = (cluster >> 16) & 0xFFFF;

  // ..
  memset(entries[1].name, ' ', 11);
  entries[1].name[0] = '.';
  entries[1].name[1] = '.';
  entries[1].attr = 0x10;
  entries[1].cluster_low = parent->impl & 0xFFFF;
  entries[1].cluster_high = (parent->impl >> 16) & 0xFFFF;

  disk_write_sectors(lba, bpb.sectors_per_cluster, (uint16_t *)buffer);
  kfree(buffer);

  vfs_node_t *res = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
  memset(res, 0, sizeof(vfs_node_t));
  kstrncpy(res->name, name, 127);
  res->length = 0;
  res->flags = VFS_DIRECTORY;
  res->mask = 0777;
  res->uid = 0;
  res->gid = 0;
  res->impl = cluster;
  res->inode = parent->impl;
  res->read = fat32_vfs_read;
  res->write = fat32_vfs_write;
  res->readdir = fat32_vfs_readdir;
  res->finddir = fat32_vfs_finddir;
  res->create = fat32_vfs_create;
  res->unlink = fat32_vfs_unlink;
  res->mkdir = fat32_vfs_mkdir;

  return res;
}

vfs_node_t *fat32_get_root(void) {
  vfs_node_t *root = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
  memset(root, 0, sizeof(vfs_node_t));
  kstrncpy(root->name, "fat32_root", 127);
  root->flags = VFS_DIRECTORY;
  root->mask = 0777;
  root->uid = 0;
  root->gid = 0;
  root->impl = bpb.root_cluster;
  root->readdir = fat32_vfs_readdir;
  root->finddir = fat32_vfs_finddir;
  root->create = fat32_vfs_create;
  root->unlink = fat32_vfs_unlink;
  root->mkdir = fat32_vfs_mkdir;
  return root;
}
