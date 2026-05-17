#include <drivers/pci.h>
#include <drivers/serial.h>
#include <arch/io.h>
#include <libk/string.h>
#include <mm/kheap.h>
#include <mm/vmm.h>
#include <net/ethernet.h>

#define REG_MAC      0x00
#define REG_MAR      0x08
#define REG_TNPDS    0x20
#define REG_CMD      0x37
#define REG_TPPOLL   0x38
#define REG_IMR      0x3C
#define REG_ISR      0x3E
#define REG_TCR      0x40
#define REG_RCR      0x44
#define REG_9346CR   0x50
#define REG_CONFIG0  0x51
#define REG_CONFIG1  0x52
#define REG_RMS      0xDA
#define REG_RDSAR    0xE4
#define REG_MTPS     0xEC

#define NUM_RX_DESC  16
#define NUM_TX_DESC  16
#define PACKET_SIZE  1536

typedef struct __attribute__((packed)) {
  uint32_t command;
  uint32_t vlan;
  uint32_t low_buf;
  uint32_t high_buf;
} rtl8169_desc_t;

static bool rtl_ready = false;
static uint32_t io_base = 0;
static uint8_t rtl_mac[6] = {0};

static rtl8169_desc_t rtl_rx_descs[NUM_RX_DESC] __attribute__((aligned(256)));
static rtl8169_desc_t rtl_tx_descs[NUM_TX_DESC] __attribute__((aligned(256)));
static uint8_t rtl_rx_buf[NUM_RX_DESC][PACKET_SIZE];
static uint8_t rtl_tx_buf[NUM_TX_DESC][PACKET_SIZE];
static uint32_t rx_cur = 0;
static uint32_t tx_cur = 0;

bool rtl8169_init(const pci_device_t *dev) {
  if (!dev) return false;

  // Read BAR0 (I/O base)
  uint32_t bar0 = pci_read32(dev->bus, dev->dev, dev->func, 0x10);
  if (!(bar0 & 1)) {
    serial_kprintf("[RTL8169] Error: BAR0 is not Port I/O space.\n");
    return false;
  }
  io_base = bar0 & ~3;
  serial_kprintf("[RTL8169] Found Realtek PCIe NIC at I/O base 0x%x\n", io_base);

  // Enable PCI Bus Mastering
  uint32_t cmd_addr = (1u << 31) | ((uint32_t)dev->bus << 16) |
                      ((uint32_t)dev->dev << 11) |
                      ((uint32_t)dev->func << 8) | 0x04;
  outl(0xCF8, cmd_addr);
  uint32_t cmd = inl(0xCFC);
  outl(0xCF8, cmd_addr);
  outl(0xCFC, cmd | 0x04); // Bus Master Enable

  // 1. Software Reset
  outb(io_base + REG_CMD, 0x10);
  volatile uint32_t spin = 1000000;
  while ((inb(io_base + REG_CMD) & 0x10) && spin--) {
    // wait for reset
  }
  serial_kprintf("[RTL8169] Software reset completed.\n");

  // 2. Unlock configuration registers
  outb(io_base + REG_9346CR, 0xC0);

  // 3. Set Max Rx Packet Size (RMS)
  outw(io_base + REG_RMS, PACKET_SIZE);

  // 4. Set Max Tx Packet Size (MTPS)
  outb(io_base + REG_MTPS, 0x3F); // ~2KB limit

  // Initialize Descriptors
  memset(rtl_rx_descs, 0, sizeof(rtl_rx_descs));
  memset(rtl_tx_descs, 0, sizeof(rtl_tx_descs));

  for (int i = 0; i < NUM_RX_DESC; i++) {
    rtl_rx_descs[i].low_buf = (uint32_t)V2P(rtl_rx_buf[i]);
    rtl_rx_descs[i].high_buf = 0;
    // Command: OWN bit (31) set, and packet size limit
    rtl_rx_descs[i].command = (1u << 31) | PACKET_SIZE;
  }
  // Set EOR (End of Ring) on last Rx descriptor
  rtl_rx_descs[NUM_RX_DESC - 1].command |= (1u << 30);

  for (int i = 0; i < NUM_TX_DESC; i++) {
    rtl_tx_descs[i].low_buf = (uint32_t)V2P(rtl_tx_buf[i]);
    rtl_tx_descs[i].high_buf = 0;
    rtl_tx_descs[i].command = 0; // Host owned initially
  }
  // Set EOR (End of Ring) on last Tx descriptor
  rtl_tx_descs[NUM_TX_DESC - 1].command |= (1u << 30);

  // 5. Write Tx Descriptor Start address
  outl(io_base + REG_TNPDS, (uint32_t)V2P(rtl_tx_descs));
  outl(io_base + REG_TNPDS + 4, 0);

  // 6. Write Rx Descriptor Start address
  outl(io_base + REG_RDSAR, (uint32_t)V2P(rtl_rx_descs));
  outl(io_base + REG_RDSAR + 4, 0);

  // 7. Configure TCR (Transmit Configuration)
  // MXDMA = 1024 bytes (0x7 << 8), standard IFG (0x3 << 24)
  outl(io_base + REG_TCR, (0x3 << 24) | (0x7 << 8));

  // 8. Configure RCR (Receive Configuration)
  // AAP (bit 0), APM (bit 1), AM (bit 2), AB (bit 3), MXDMA 1024 (0x7 << 8)
  outl(io_base + REG_RCR, (0x7 << 8) | 0x0F);

  // 9. Lock configuration registers
  outb(io_base + REG_9346CR, 0x00);

  // 10. Disable interrupts & clear status
  outw(io_base + REG_IMR, 0x0000);
  outw(io_base + REG_ISR, 0xFFFF);

  // 11. Read MAC
  for (int i = 0; i < 6; i++) {
    rtl_mac[i] = inb(io_base + REG_MAC + i);
  }
  serial_kprintf("[RTL8169] MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n",
                 rtl_mac[0], rtl_mac[1], rtl_mac[2], rtl_mac[3], rtl_mac[4], rtl_mac[5]);

  // 12. Enable TE (0x08) & RE (0x04)
  outb(io_base + REG_CMD, 0x0C);

  rx_cur = 0;
  tx_cur = 0;
  rtl_ready = true;
  return true;
}

bool rtl8169_send(const uint8_t *dst_mac, uint16_t ethertype, const void *payload, uint16_t len) {
  if (!rtl_ready) return false;

  // Poll until current descriptor has OWN = 0 (owned by Host)
  volatile uint32_t timeout = 1000000;
  while ((rtl_tx_descs[tx_cur].command & (1u << 31)) && timeout--) {
    __asm__ volatile("pause");
  }
  if (timeout == 0) {
    serial_kprintf("[RTL8169] Send timeout: TX queue is full!\n");
    return false;
  }

  // Build Ethernet Frame
  uint8_t *frame = rtl_tx_buf[tx_cur];
  for (int i = 0; i < 6; i++) frame[i] = dst_mac[i];
  for (int i = 0; i < 6; i++) frame[6 + i] = rtl_mac[i];
  frame[12] = (ethertype >> 8) & 0xFF;
  frame[13] = ethertype & 0xFF;

  uint32_t total = 14 + len;
  if (total > PACKET_SIZE) total = PACKET_SIZE;
  memcpy(frame + 14, payload, total - 14);

  // Setup descriptor command (OWN=1, FS=1, LS=1, plus packet length)
  uint32_t cmd_val = (1u << 31) | (1u << 29) | (1u << 28) | total;
  // Preserve EOR if this is the last descriptor
  if (tx_cur == NUM_TX_DESC - 1) {
    cmd_val |= (1u << 30);
  }
  rtl_tx_descs[tx_cur].command = cmd_val;

  // Trigger transmission (TPPoll normal queue trigger)
  outb(io_base + REG_TPPOLL, 0x40);

  tx_cur = (tx_cur + 1) % NUM_TX_DESC;
  return true;
}

int rtl8169_receive(uint8_t *buf, uint16_t maxlen) {
  if (!rtl_ready) return -1;

  uint32_t cmd_val = rtl_rx_descs[rx_cur].command;
  // If OWN bit is set, NIC still owns the buffer (no packet yet)
  if (cmd_val & (1u << 31)) {
    return 0;
  }

  // Get packet length (lower 14 bits)
  uint32_t len = cmd_val & 0x3FFF;
  if (len > maxlen) len = maxlen;

  memcpy(buf, rtl_rx_buf[rx_cur], len);

  // Restore ownership to NIC
  uint32_t restore_cmd = (1u << 31) | PACKET_SIZE;
  if (rx_cur == NUM_RX_DESC - 1) {
    restore_cmd |= (1u << 30); // EOR
  }
  rtl_rx_descs[rx_cur].command = restore_cmd;

  rx_cur = (rx_cur + 1) % NUM_RX_DESC;
  return len;
}

bool rtl8169_is_ready(void) { return rtl_ready; }

void rtl8169_get_mac(uint8_t *mac) {
  for (int i = 0; i < 6; i++) mac[i] = rtl_mac[i];
}
