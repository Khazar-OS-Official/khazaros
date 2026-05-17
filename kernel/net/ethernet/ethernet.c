// E1000 (82540EM) Full NIC driver
// Handles TX/RX descriptor rings, MAC read, packet send/recv
#include <arch/io.h>
#include <drivers/pci.h>
#include <drivers/vga.h>
#include <libk/string.h>
#include <mm/kheap.h>
#include <mm/vmm.h>
#include <net/ethernet.h>

// --- E1000 Register Offsets ---
#define REG_CTRL 0x0000
#define REG_STATUS 0x0008
#define REG_EECD 0x0010
#define REG_EEPROM 0x0014
#define REG_ICR 0x00C0
#define REG_IMS 0x00D0
#define REG_IMC 0x00D8
#define REG_RCTL 0x0100
#define REG_TCTL 0x0400
#define REG_RDBAL 0x2800
#define REG_RDBAH 0x2804
#define REG_RDLEN 0x2808
#define REG_RDH 0x2810
#define REG_RDT 0x2818
#define REG_TDBAL 0x3800
#define REG_TDBAH 0x3804
#define REG_TDLEN 0x3808
#define REG_TDH 0x3810
#define REG_TDT 0x3818
#define REG_MTA 0x5200
#define REG_RAL 0x5400
#define REG_RAH 0x5404

// --- CTRL bits ---
#define CTRL_SLU (1 << 6)  // Set Link Up
#define CTRL_RST (1 << 26) // Device Reset

// --- RCTL bits ---
#define RCTL_EN (1 << 1)
#define RCTL_SBP (1 << 2)
#define RCTL_BAM (1 << 15) // Broadcast Accept Mode
#define RCTL_BSIZE_2K (0 << 16)
#define RCTL_SECRC (1 << 26) // Strip CRC

// --- TCTL bits ---
#define TCTL_EN (1 << 1)
#define TCTL_PSP (1 << 3) // Pad Short Packets
#define TCTL_CT_SHIFT 4
#define TCTL_COLD_SHIFT 12

// --- TX Descriptor command bits ---
#define TX_CMD_EOP (1 << 0)  // End of Packet
#define TX_CMD_IFCS (1 << 1) // Insert FCS
#define TX_CMD_RS (1 << 3)   // Report Status
#define TX_STA_DD (1 << 0)   // Descriptor Done

#define NUM_RX_DESC 16
#define NUM_TX_DESC 16
#define PACKET_SIZE 2048

// E1000 Transmit / Receive Descriptors (must be 16-byte aligned)
typedef struct __attribute__((packed)) {
  uint64_t address; // Physical buffer address
  uint16_t length;
  uint16_t checksum;
  uint8_t status;
  uint8_t errors;
  uint16_t special;
} e1000_rx_desc_t;

typedef struct __attribute__((packed)) {
  uint64_t address;
  uint16_t length;
  uint8_t cso;
  uint8_t cmd;
  uint8_t status;
  uint8_t css;
  uint16_t special;
} e1000_tx_desc_t;

// Driver state
static ethernet_device_t eth_dev;
static volatile uint32_t *e1000_mmio = NULL;

static e1000_rx_desc_t rx_descs[NUM_RX_DESC] __attribute__((aligned(16)));
static e1000_tx_desc_t tx_descs[NUM_TX_DESC] __attribute__((aligned(16)));
static uint8_t rx_buf[NUM_RX_DESC][PACKET_SIZE];
static uint8_t tx_buf[NUM_TX_DESC][PACKET_SIZE];
static uint32_t rx_cur = 0;
static uint32_t tx_cur = 0;

// --- Low-level register access ---
static uint32_t e1000_r(uint32_t reg) { return e1000_mmio[reg >> 2]; }
static void e1000_w(uint32_t reg, uint32_t val) { e1000_mmio[reg >> 2] = val; }

// Poll until done bit is set (bit 4), with timeout
static uint16_t e1000_eeprom_read(uint8_t addr) {
  e1000_w(REG_EEPROM, (1) | ((uint32_t)addr << 8));
  uint32_t val = 0;
  for (uint32_t i = 0; i < 10000; i++) {
    val = e1000_r(REG_EEPROM);
    if (val & (1 << 4))
      break;
  }
  return (uint16_t)((val >> 16) & 0xFFFF);
}

static void e1000_read_mac(uint8_t *mac) {
  uint16_t w0 = e1000_eeprom_read(0);
  uint16_t w1 = e1000_eeprom_read(1);
  uint16_t w2 = e1000_eeprom_read(2);
  mac[0] = w0 & 0xFF;
  mac[1] = (w0 >> 8) & 0xFF;
  mac[2] = w1 & 0xFF;
  mac[3] = (w1 >> 8) & 0xFF;
  mac[4] = w2 & 0xFF;
  mac[5] = (w2 >> 8) & 0xFF;
}

// --- Initialize DMA Descriptor Rings ---
static void e1000_init_rx(void) {
  for (int i = 0; i < NUM_RX_DESC; i++) {
    // DMA address MUST be physical - NIC hardware does not know about virtual
    // memory
    rx_descs[i].address = (uint64_t)V2P(rx_buf[i]);
    rx_descs[i].status = 0;
  }
  uint32_t base = (uint32_t)V2P(rx_descs);
  e1000_w(REG_RDBAL, base);
  e1000_w(REG_RDBAH, 0);
  e1000_w(REG_RDLEN, NUM_RX_DESC * sizeof(e1000_rx_desc_t));
  e1000_w(REG_RDH, 0);
  e1000_w(REG_RDT, NUM_RX_DESC - 1);
  e1000_w(REG_RCTL, RCTL_EN | RCTL_SBP | RCTL_BAM | RCTL_SECRC);
}

static void e1000_init_tx(void) {
  for (int i = 0; i < NUM_TX_DESC; i++) {
    // DMA address MUST be physical
    tx_descs[i].address = (uint64_t)V2P(tx_buf[i]);
    tx_descs[i].status = TX_STA_DD;
  }
  uint32_t base = (uint32_t)V2P(tx_descs);
  e1000_w(REG_TDBAL, base);
  e1000_w(REG_TDBAH, 0);
  e1000_w(REG_TDLEN, NUM_TX_DESC * sizeof(e1000_tx_desc_t));
  e1000_w(REG_TDH, 0);
  e1000_w(REG_TDT, 0);
  e1000_w(REG_TCTL, TCTL_EN | TCTL_PSP | (0x0F << TCTL_CT_SHIFT) |
                        (0x40 << TCTL_COLD_SHIFT));
}

// --- PCI BAR0 helper ---
static uint32_t pci_read_bar0(const pci_device_t *dev) {
  uint32_t addr = (1u << 31) | ((uint32_t)dev->bus << 16) |
                  ((uint32_t)dev->dev << 11) | ((uint32_t)dev->func << 8) |
                  0x10;
  outl(0xCF8, addr);
  return inl(0xCFC);
}

// --- Public API ---
bool ethernet_init(void) {
  const pci_device_t *found = NULL;
  for (uint32_t i = 0; i < pci_get_device_count(); i++) {
    const pci_device_t *d = pci_get_device(i);
    if (d->vendor_id == 0x8086 &&
        (d->device_id == 0x100E || d->device_id == 0x100F ||
         d->device_id == 0x1010 || d->device_id == 0x1016)) {
      found = d;
      break;
    }
  }
  if (!found) {
    kprintf("NET: Intel E1000 not found on PCI bus\n");
    return false;
  }

  uint32_t bar0 = pci_read_bar0(found);
  if (bar0 & 1) {
    kprintf("NET: E1000 BAR0 is I/O space, need MMIO\n");
    return false;
  }

  eth_dev.mmio_base = bar0 & 0xFFFFFFF0;
  eth_dev.pci_dev = found;

  // --- Map E1000 MMIO into kernel virtual address space ---
  // The NIC registers live at a physical address that is NOT in our page
  // tables. We use identity-mapping (the E1000 MMIO region is typically above
  // 0xF0000000 which is already above KERNEL_VIRT_BASE, so we map phys == virt
  // for it). We need 128KB (32 pages) to cover all E1000 registers.
  uint32_t mmio_phys = eth_dev.mmio_base;
  for (int pg = 0; pg < 32; pg++) {
    uint32_t phys_page = mmio_phys + pg * 4096;
    // Map phys -> same virtual address (identity mapping for MMIO)
    // Use Write-Through + Cache-Disable (bits 3 and 4 in the PTE)
    vmm_map_page((void *)phys_page, (void *)phys_page,
                 VMM_PRESENT | VMM_WRITABLE);
  }

  e1000_mmio = (volatile uint32_t *)mmio_phys;
  kprintf("NET: E1000 MMIO=0x%x (mapped)\n", eth_dev.mmio_base);

  // Enable PCI Bus Mastering
  uint32_t cmd_addr = (1u << 31) | ((uint32_t)found->bus << 16) |
                      ((uint32_t)found->dev << 11) |
                      ((uint32_t)found->func << 8) | 0x04;
  outl(0xCF8, cmd_addr);
  uint32_t cmd = inl(0xCFC);
  outl(0xCF8, cmd_addr);
  outl(0xCFC, cmd | 0x04); // Bus Master Enable

  // Reset NIC
  e1000_w(REG_CTRL, e1000_r(REG_CTRL) | CTRL_RST);
  volatile uint32_t spin = 1000000;
  while ((e1000_r(REG_CTRL) & CTRL_RST) && spin--)
    ;

  // Enable link
  e1000_w(REG_CTRL, e1000_r(REG_CTRL) | CTRL_SLU);

  // Clear multicast table
  for (int i = 0; i < 128; i++)
    e1000_w(REG_MTA + i * 4, 0);

  // Disable all interrupts
  e1000_w(REG_IMC, 0xFFFFFFFF);
  e1000_r(REG_ICR);

  // Read MAC from EEPROM
  e1000_read_mac(eth_dev.mac);
  kprintf("NET: MAC=%02x:%02x:%02x:%02x:%02x:%02x\n", eth_dev.mac[0],
          eth_dev.mac[1], eth_dev.mac[2], eth_dev.mac[3], eth_dev.mac[4],
          eth_dev.mac[5]);

  // Write MAC to Receive Address registers
  uint32_t ral = eth_dev.mac[0] | (eth_dev.mac[1] << 8) |
                 (eth_dev.mac[2] << 16) | (eth_dev.mac[3] << 24);
  uint32_t rah = eth_dev.mac[4] | (eth_dev.mac[5] << 8) | (1u << 31);
  e1000_w(REG_RAL, ral);
  e1000_w(REG_RAH, rah);

  e1000_init_rx();
  e1000_init_tx();

  eth_dev.initialized = true;
  kprintf("NET: E1000 ready\n");
  return true;
}

bool ethernet_send(const uint8_t *dst_mac, uint16_t ethertype,
                   const void *payload, uint16_t len) {
  if (!eth_dev.initialized)
    return false;

  // Build Ethernet frame in TX buffer
  uint8_t *frame = tx_buf[tx_cur];
  // Destination MAC
  for (int i = 0; i < 6; i++)
    frame[i] = dst_mac[i];
  // Source MAC
  for (int i = 0; i < 6; i++)
    frame[6 + i] = eth_dev.mac[i];
  // EtherType (big-endian)
  frame[12] = (ethertype >> 8) & 0xFF;
  frame[13] = ethertype & 0xFF;
  // Payload
  uint16_t total = 14 + len;
  if (total > PACKET_SIZE)
    total = PACKET_SIZE;
  memcpy(frame + 14, payload, total - 14);

  // Set descriptor
  tx_descs[tx_cur].length = total;
  tx_descs[tx_cur].cmd = TX_CMD_EOP | TX_CMD_IFCS | TX_CMD_RS;
  tx_descs[tx_cur].status = 0;

  tx_cur = (tx_cur + 1) % NUM_TX_DESC;
  e1000_w(REG_TDT, tx_cur);

  // Wait for completion
  volatile uint32_t timeout = 1000000;
  uint32_t prev = (tx_cur == 0) ? NUM_TX_DESC - 1 : tx_cur - 1;
  while (!(tx_descs[prev].status & TX_STA_DD) && timeout--)
    ;

  return true;
}

int ethernet_receive(uint8_t *buf, uint16_t maxlen) {
  if (!eth_dev.initialized)
    return -1;
  if (!(rx_descs[rx_cur].status & 0x01))
    return 0; // No packet yet

  uint16_t len = rx_descs[rx_cur].length;
  if (len > maxlen)
    len = maxlen;
  memcpy(buf, rx_buf[rx_cur], len);
  rx_descs[rx_cur].status = 0;
  e1000_w(REG_RDT, rx_cur);
  rx_cur = (rx_cur + 1) % NUM_RX_DESC;
  return len;
}

bool ethernet_is_ready(void) { return eth_dev.initialized; }
const ethernet_device_t *ethernet_get_device(void) {
  return eth_dev.initialized ? &eth_dev : (ethernet_device_t *)0;
}
