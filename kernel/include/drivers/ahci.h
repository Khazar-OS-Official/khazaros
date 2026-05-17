#ifndef AHCI_H
#define AHCI_H

#include <libk/types.h>

#define SATA_SIG_ATA    0x00000101 // SATA drive
#define SATA_SIG_ATAPI  0xEB140101 // SATAPI drive
#define SATA_SIG_SEMB   0xC33C0101 // Enclosure management bridge
#define SATA_SIG_PM     0x96690101 // Port multiplier

#define AHCI_DEV_NULL 0
#define AHCI_DEV_SATA 1
#define AHCI_DEV_SATAPI 2
#define AHCI_DEV_SEMB 3
#define AHCI_DEV_PM 4

#define HBA_PORT_DET_PRESENT 3
#define HBA_PORT_IPM_ACTIVE 1

typedef volatile struct {
    uint32_t clb;
    uint32_t clbu;
    uint32_t fb;
    uint32_t fbu;
    uint32_t is;
    uint32_t ie;
    uint32_t cmd;
    uint32_t rsv0;
    uint32_t tfd;
    uint32_t sig;
    uint32_t ssts;
    uint32_t sctl;
    uint32_t serr;
    uint32_t sact;
    uint32_t ci;
    uint32_t sntf;
    uint32_t fbs;
    uint32_t rsv1[11];
    uint32_t vendor[4];
} hba_port_t;

typedef volatile struct {
    uint32_t cap;
    uint32_t ghc;
    uint32_t is;
    uint32_t pi;
    uint32_t vs;
    uint32_t ccc_ctl;
    uint32_t ccc_pts;
    uint32_t em_loc;
    uint32_t em_ctl;
    uint32_t cap2;
    uint32_t bohc;
    uint8_t  rsv[0xA0-0x2C];
    uint8_t  vendor[0x100-0xA0];
    hba_port_t ports[32];
} hba_mem_t;

typedef struct {
    uint8_t  cfis[64]; // Command FIS
    uint8_t  acmd[16]; // ATAPI command, 12 or 16 bytes
    uint8_t  rsv[48];  // Reserved
} hba_cmd_tbl_t; // We'll append PRDT dynamically

typedef struct {
    uint32_t dba;      // Data base address
    uint32_t dbau;     // Data base address upper 32 bits
    uint32_t rsv0;     // Reserved
    uint32_t dbc:22;   // Byte count, 4M max
    uint32_t rsv1:9;   // Reserved
    uint32_t i:1;      // Interrupt on completion
} hba_prdt_entry_t;

typedef struct {
    // DW0
    uint8_t  cfl:5;    // Command FIS length in DWORDS, 2 ~ 16
    uint8_t  a:1;      // ATAPI
    uint8_t  w:1;      // Write, 1: H2D, 0: D2H
    uint8_t  p:1;      // Prefetchable
    uint8_t  r:1;      // Reset
    uint8_t  b:1;      // BIST
    uint8_t  c:1;      // Clear busy upon R_OK
    uint8_t  rsv0:1;   // Reserved
    uint8_t  pmp:4;    // Port multiplier port
    uint16_t prdtl;    // Physical region descriptor table length in entries

    // DW1
    volatile uint32_t prdbc;   // Physical region descriptor byte count transferred

    // DW2, 3
    uint32_t ctba;     // Command table descriptor base address
    uint32_t ctbau;    // Command table descriptor base address upper 32 bits

    // DW4 - 7
    uint32_t rsv1[4];  // Reserved
} hba_cmd_header_t;

typedef struct {
    uint8_t  fis_type; // 0x27
    uint8_t  pmport:4;
    uint8_t  rsv0:3;
    uint8_t  c:1;      // 1: Command, 0: Control
    uint8_t  command;
    uint8_t  featurel; // Feature register, 7:0
    uint8_t  lba0;     // LBA low register, 7:0
    uint8_t  lba1;     // LBA mid register, 15:8
    uint8_t  lba2;     // LBA high register, 23:16
    uint8_t  device;   // Device register
    uint8_t  lba3;     // LBA register, 31:24
    uint8_t  lba4;     // LBA register, 39:32
    uint8_t  lba5;     // LBA register, 47:40
    uint8_t  featureh; // Feature register, 15:8
    uint8_t  countl;   // Count register, 7:0
    uint8_t  counth;   // Count register, 15:8
    uint8_t  icc;      // Isochronous command completion
    uint8_t  control;  // Control register
    uint8_t  rsv1[4];  // Reserved
} fis_reg_h2d_t;

void ahci_init(void);
hba_port_t *ahci_get_drive(void);
bool ahci_read(hba_port_t *port, uint32_t start_lba, uint32_t count, uint16_t *buf);
bool ahci_write(hba_port_t *port, uint32_t start_lba, uint32_t count, uint16_t *buf);

#endif
