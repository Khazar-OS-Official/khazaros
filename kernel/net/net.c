#include <drivers/vga.h>
#include <net/arp.h>
#include <net/ethernet.h>
#include <net/ipv4.h>
#include <proc/process.h>


// network_task: Kernel thread that polls the NIC in the background.
// This allows other processes (like the shell or GUI) to remain responsive
// even when high network traffic is present, and enables non-blocking RX.
void network_task(void) {
  kprintf("Network: Background Task started.\n");

  uint8_t frame[1518];

  while (1) {
    int len = ethernet_receive(frame, sizeof(frame));
    if (len > 0) {
      if (len < 14)
        continue;

      uint16_t ethertype = (frame[12] << 8) | frame[13];

      if (ethertype == 0x0800) { // IPv4
        ipv4_handle(frame, (uint16_t)len);
      } else if (ethertype == 0x0806) { // ARP
        arp_handle(frame + 14, (uint16_t)(len - 14));
      }
    }

    // Since we don't have wait queues yet, we yield to others.
    // In a Round-Robin scheduler, this thread will run again very soon.
    // We could add a small delay or use a 'hlt' if idle, but for now
    // we want to process packets as fast as possible.
    __asm__ volatile("int $0x20"); // Trigger PIT/Scheduler yield
  }
}
