#include <drivers/pci.h>
#include <drivers/serial.h>
#include <drivers/ahci.h>
#include <drivers/vga.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <mm/kheap.h>
#include <libk/string.h>
#include <fs/devfs.h>

#define HBA_PxCMD_ST    0x0001
#define HBA_PxCMD_FRE   0x0010
#define HBA_PxCMD_FR    0x4000
#define HBA_PxCMD_CR    0x8000

static hba_mem_t *abar_mem;

static int check_port_type(hba_port_t *port) {
    uint32_t ssts = port->ssts;
    uint8_t ipm = (ssts >> 8) & 0x0F;
    uint8_t det = ssts & 0x0F;

    if (det != HBA_PORT_DET_PRESENT || ipm != HBA_PORT_IPM_ACTIVE) {
        return AHCI_DEV_NULL;
    }

    switch (port->sig) {
        case SATA_SIG_ATAPI: return AHCI_DEV_SATAPI;
        case SATA_SIG_SEMB: return AHCI_DEV_SEMB;
        case SATA_SIG_PM: return AHCI_DEV_PM;
        default: return AHCI_DEV_SATA;
    }
}

static void port_start(hba_port_t *port) {
    /* Timeout ile bekle - real donanim CR bitini hic silmeyebilir */
    int timeout = 500000;
    while ((port->cmd & HBA_PxCMD_CR) && timeout-- > 0);
    if (timeout <= 0) {
        serial_write_string("[AHCI] port_start: CR bit timeout!\n");
        return;
    }
    port->cmd |= HBA_PxCMD_FRE;
    port->cmd |= HBA_PxCMD_ST;
}

static void port_stop(hba_port_t *port) {
    port->cmd &= ~HBA_PxCMD_ST;
    port->cmd &= ~HBA_PxCMD_FRE;
    /* Timeout ile bekle - real donanim FR/CR bitlerini silmeyebilir */
    int timeout = 500000;
    while (timeout-- > 0) {
        if (port->cmd & HBA_PxCMD_FR) continue;
        if (port->cmd & HBA_PxCMD_CR) continue;
        break;
    }
    if (timeout <= 0)
        serial_write_string("[AHCI] port_stop: timeout waiting for FR/CR!\n");
}

static void port_rebase(hba_port_t *port, int portno) {
    (void)portno;
    port_stop(port);

    // Command list offset: 1K per port. We allocate 1 page (4K) for 4 ports, 
    // or simply 1 page per port to be safe.
    void *clb_phys = pmm_alloc_block(); 
    memset((void*)((uint32_t)clb_phys + 0xC0000000), 0, 4096); // Assuming higher half mapping at 0xC0000000
    
    port->clb = (uint32_t)clb_phys;
    port->clbu = 0;

    // FIS offset
    void *fb_phys = pmm_alloc_block();
    memset((void*)((uint32_t)fb_phys + 0xC0000000), 0, 4096);
    port->fb = (uint32_t)fb_phys;
    port->fbu = 0;

    // Command table offset: 256 bytes per command table, 32 commands
    hba_cmd_header_t *cmdheader = (hba_cmd_header_t*)((uint32_t)clb_phys + 0xC0000000);
    for (int i=0; i<32; i++) {
        cmdheader[i].prdtl = 8; // 8 prdt entries per command table
        void *ctba_phys = pmm_alloc_block();
        memset((void*)((uint32_t)ctba_phys + 0xC0000000), 0, 4096);
        cmdheader[i].ctba = (uint32_t)ctba_phys;
        cmdheader[i].ctbau = 0;
    }

    port_start(port);
}

// Find a free command list slot
static int find_cmdslot(hba_port_t *port) {
    // If not set in SACT and CI, the slot is free
    uint32_t slots = (port->sact | port->ci);
    for (int i=0; i<32; i++) {
        if ((slots & 1) == 0)
            return i;
        slots >>= 1;
    }
    return -1;
}

bool ahci_read(hba_port_t *port, uint32_t start_lba, uint32_t count, uint16_t *buf) {
    port->is = (uint32_t)-1; // Clear pending interrupt bits
    int spin = 0; // Spin lock timeout counter
    int slot = find_cmdslot(port);
    if (slot == -1) return false;

    hba_cmd_header_t *cmdheader = (hba_cmd_header_t*)((uint32_t)port->clb + 0xC0000000);
    cmdheader += slot;
    cmdheader->cfl = sizeof(fis_reg_h2d_t)/sizeof(uint32_t); // Command FIS size
    cmdheader->w = 0; // Read from device
    cmdheader->prdtl = (uint16_t)((count - 1) >> 4) + 1; // PRDT entries count

    hba_cmd_tbl_t *cmdtbl = (hba_cmd_tbl_t*)(cmdheader->ctba + 0xC0000000);
    memset(cmdtbl, 0, sizeof(hba_cmd_tbl_t) + (cmdheader->prdtl * sizeof(hba_prdt_entry_t)));

    // 8K bytes (16 sectors) per PRDT
    hba_prdt_entry_t *prdt = (hba_prdt_entry_t*)((uint32_t)cmdtbl + sizeof(hba_cmd_tbl_t));
    int i = 0;
    for (i = 0; i < cmdheader->prdtl - 1; i++) {
        prdt[i].dba = (uint32_t)buf - 0xC0000000; // Virtual to Physical
        prdt[i].dbc = 8192 - 1;     // 8K bytes
        prdt[i].i = 1;
        buf += 4096; // 4K words = 8K bytes
        count -= 16; // 16 sectors
    }
    
    // Last entry
    prdt[i].dba = (uint32_t)buf - 0xC0000000;
    prdt[i].dbc = (count * 512) - 1;
    prdt[i].i = 1;

    // Setup command
    fis_reg_h2d_t *cmdfis = (fis_reg_h2d_t*)(&cmdtbl->cfis);

    cmdfis->fis_type = 0x27; // Register H2D
    cmdfis->c = 1;           // Command
    cmdfis->command = 0x24;  // READ_FPDMA_QUEUED (for NCQ) or 0x25 READ DMA EXT
    // We'll use simple READ DMA EXT for now
    cmdfis->command = 0x25; 

    cmdfis->lba0 = (uint8_t)start_lba;
    cmdfis->lba1 = (uint8_t)(start_lba >> 8);
    cmdfis->lba2 = (uint8_t)(start_lba >> 16);
    cmdfis->device = 1 << 6; // LBA mode

    cmdfis->lba3 = (uint8_t)(start_lba >> 24);
    cmdfis->lba4 = 0; // LBA48 high bits
    cmdfis->lba5 = 0;

    cmdfis->countl = (uint8_t)count;
    cmdfis->counth = (uint8_t)(count >> 8);

    // Issue command
    while ((port->tfd & (0x80 | 0x08)) && spin < 1000000) {
        spin++;
    }
    if (spin == 1000000) {
        serial_write_string("[AHCI] Port is hung\n");
        return false;
    }

    port->ci = 1 << slot; // Issue command

    // Wait for completion (timeout ile - real HW-da sonsuz donma olmasin)
    int cmpl_timeout = 2000000;
    while (cmpl_timeout-- > 0) {
        if ((port->ci & (1 << slot)) == 0)
            break;
        if (port->is & (1 << 30)) { // Task file error
            serial_write_string("[AHCI] Read error\n");
            return false;
        }
    }
    if (cmpl_timeout <= 0) {
        serial_write_string("[AHCI] Read timeout\n");
        return false;
    }

    // Check again
    if (port->is & (1 << 30)) {
        serial_write_string("[AHCI] Read error\n");
        return false;
    }

    return true;
}

bool ahci_write(hba_port_t *port, uint32_t start_lba, uint32_t count, uint16_t *buf) {
    port->is = (uint32_t)-1;
    int spin = 0;
    int slot = find_cmdslot(port);
    if (slot == -1) return false;

    hba_cmd_header_t *cmdheader = (hba_cmd_header_t*)((uint32_t)port->clb + 0xC0000000);
    cmdheader += slot;
    cmdheader->cfl = sizeof(fis_reg_h2d_t)/sizeof(uint32_t);
    cmdheader->w = 1; // Write to device
    cmdheader->prdtl = (uint16_t)((count - 1) >> 4) + 1;

    hba_cmd_tbl_t *cmdtbl = (hba_cmd_tbl_t*)(cmdheader->ctba + 0xC0000000);
    memset(cmdtbl, 0, sizeof(hba_cmd_tbl_t) + (cmdheader->prdtl * sizeof(hba_prdt_entry_t)));

    hba_prdt_entry_t *prdt = (hba_prdt_entry_t*)((uint32_t)cmdtbl + sizeof(hba_cmd_tbl_t));
    int i = 0;
    for (i = 0; i < cmdheader->prdtl - 1; i++) {
        prdt[i].dba = (uint32_t)buf - 0xC0000000; // Virtual to Physical
        prdt[i].dbc = 8192 - 1;
        prdt[i].i = 1;
        buf += 4096;
        count -= 16;
    }
    
    prdt[i].dba = (uint32_t)buf - 0xC0000000;
    prdt[i].dbc = (count * 512) - 1;
    prdt[i].i = 1;

    fis_reg_h2d_t *cmdfis = (fis_reg_h2d_t*)(&cmdtbl->cfis);
    cmdfis->fis_type = 0x27;
    cmdfis->c = 1;
    cmdfis->command = 0x35; // WRITE DMA EXT

    cmdfis->lba0 = (uint8_t)start_lba;
    cmdfis->lba1 = (uint8_t)(start_lba >> 8);
    cmdfis->lba2 = (uint8_t)(start_lba >> 16);
    cmdfis->device = 1 << 6;

    cmdfis->lba3 = (uint8_t)(start_lba >> 24);
    cmdfis->lba4 = 0;
    cmdfis->lba5 = 0;

    cmdfis->countl = (uint8_t)count;
    cmdfis->counth = (uint8_t)(count >> 8);

    while ((port->tfd & (0x80 | 0x08)) && spin < 1000000) spin++;
    if (spin == 1000000) return false;

    port->ci = 1 << slot;

    // Wait for completion (timeout ile)
    int cmpl_timeout = 2000000;
    while (cmpl_timeout-- > 0) {
        if ((port->ci & (1 << slot)) == 0) break;
        if (port->is & (1 << 30)) return false;
    }
    if (cmpl_timeout <= 0) return false;

    return true;
}

hba_port_t *ahci_get_drive(void) {
    if (!abar_mem) return NULL;
    for (int i=0; i<32; i++) {
        if (abar_mem->pi & (1<<i)) {
            if (check_port_type(&abar_mem->ports[i]) == AHCI_DEV_SATA)
                return &abar_mem->ports[i];
        }
    }
    return NULL;
}

static uint32_t ahci_vfs_read(struct vfs_node *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    hba_port_t *port = (hba_port_t *)node->device_data;
    uint32_t start_lba = offset / 512;
    uint32_t count = (size + 511) / 512;
    uint8_t *bounce = (uint8_t *)kmalloc(count * 512);
    if (!bounce) return 0;
    
    if (ahci_read(port, start_lba, count, (uint16_t *)bounce)) {
        uint32_t sector_offset = offset % 512;
        memcpy(buffer, bounce + sector_offset, size);
        kfree(bounce);
        return size;
    }
    kfree(bounce);
    return 0;
}

static uint32_t ahci_vfs_write(struct vfs_node *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    hba_port_t *port = (hba_port_t *)node->device_data;
    uint32_t start_lba = offset / 512;
    uint32_t count = (size + 511) / 512;
    uint8_t *bounce = (uint8_t *)kmalloc(count * 512);
    if (!bounce) return 0;
    
    // If not writing exactly full sectors, we need a read-modify-write cycle
    uint32_t sector_offset = offset % 512;
    if (sector_offset != 0 || (size % 512) != 0) {
        if (!ahci_read(port, start_lba, count, (uint16_t *)bounce)) {
            kfree(bounce);
            return 0;
        }
    }
    
    memcpy(bounce + sector_offset, buffer, size);
    
    if (ahci_write(port, start_lba, count, (uint16_t *)bounce)) {
        kfree(bounce);
        return size;
    }
    kfree(bounce);
    return 0;
}

void ahci_init(void) {
  const pci_device_t *dev = pci_find_class(0x01, 0x06);
  if (!dev) {
    serial_write_string("[AHCI] No SATA controller found.\n");
    return;
  }

  // Get ABAR (AHCI Base Address Register) -> BAR5
  uint32_t abar_phys = pci_read32(dev->bus, dev->dev, dev->func, 0x24);
  
  if (abar_phys & 1) {
    serial_write_string("[AHCI] Error: ABAR is I/O space. Not supported.\n");
    return;
  }
  
  abar_phys &= 0xFFFFFFF0; // Clear lower 4 bits (memory type indicators)
  
  // ABAR is a memory-mapped physical address. We must map it to virtual space.
  // Map 1 page (4KB) for ABAR. Identity mapping since it's MMIO.
  uint32_t aligned_phys = abar_phys & ~0xFFF;
  if (!vmm_map_page((void*)aligned_phys, (void*)aligned_phys, VMM_PRESENT | VMM_WRITABLE | VMM_CACHE_DISABLE)) {
      serial_write_string("[AHCI] Failed to map ABAR!\n");
      return;
  }
  
  abar_mem = (hba_mem_t*)abar_phys;
  
  char dbg[64];
  ksprintf(dbg, "[AHCI] Controller found. ABAR (MMIO base) at 0x%x\n", abar_phys);
  serial_write_string(dbg);
  
  // Enable AHCI awareness
  abar_mem->ghc |= (1 << 31); // AHCI Enable (AE)
  
  uint32_t pi = abar_mem->pi;
  int port_count = 0;
  
  for (int i = 0; i < 32; i++) {
      if (pi & (1 << i)) {
          int type = check_port_type(&abar_mem->ports[i]);
          if (type == AHCI_DEV_SATA) {
              ksprintf(dbg, "[AHCI] Port %d: SATA drive found. Initializing DMA...\n", i);
              serial_write_string(dbg);
              port_rebase(&abar_mem->ports[i], i);
              
              char drive_name[32];
              ksprintf(drive_name, "sd%c", 'a' + port_count);
              vfs_node_t *drive_node = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
              memset(drive_node, 0, sizeof(vfs_node_t));
              kstrncpy(drive_node->name, drive_name, 31);
              drive_node->flags = VFS_BLOCK_DEVICE;
              drive_node->read = ahci_vfs_read;
              drive_node->write = ahci_vfs_write;
              drive_node->device_data = (void *)(uintptr_t)&abar_mem->ports[i];
              devfs_register_device(drive_name, drive_node);
              
              port_count++;
          } else if (type == AHCI_DEV_SATAPI) {
              ksprintf(dbg, "[AHCI] Port %d: SATAPI drive found\n", i);
              serial_write_string(dbg);
              port_count++;
          }
      }
  }
  
  if (port_count == 0) {
      serial_write_string("[AHCI] No drives attached to SATA ports.\n");
  } else {
      serial_write_string("[AHCI] SATA Ports enumerated.\n");
  }
}

