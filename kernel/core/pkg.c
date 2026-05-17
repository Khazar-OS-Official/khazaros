#include <drivers/vga.h>
#include <fs/vfs.h>
#include <kernel/pkg.h>
#include <libk/string.h>
#include <mm/kheap.h>
#include <net/arp.h>
#include <net/ethernet.h>
#include <net/ipv4.h>
#include <net/udp.h>

// -----------------------------------------------------------------------
// Internal helpers
// -----------------------------------------------------------------------

// Poll UDP queue until a packet arrives (up to max_iter iterations).
// Returns bytes received, 0 on timeout.
static int pkg_udp_recv_wait(uint8_t *buf, uint16_t maxlen, uint32_t max_iter) {
  for (uint32_t i = 0; i < max_iter; i++) {
    // Port 1234 is the default source port for pkg server interactions
    int got = udp_recv(1234, buf, maxlen);
    if (got > 0)
      return got;

    // Yield to let the network_task and other processes run
    __asm__ volatile("int $0x20");
  }
  return 0;
}

// Ensure ARP is resolved for the server. Returns true if MAC is known.
static bool pkg_ensure_arp(void) {
  uint8_t mac[6];
  if (arp_lookup(PKG_SERVER_IP, mac))
    return true;

  arp_request(PKG_SERVER_IP);

  static uint8_t rx_frame[1518];
  for (uint32_t i = 0; i < 500000; i++) {
    int n = ethernet_receive(rx_frame, sizeof(rx_frame));
    if (n > 14) {
      uint16_t etype = ((uint16_t)rx_frame[12] << 8) | rx_frame[13];
      if (etype == 0x0806)
        arp_handle(rx_frame + 14, (uint16_t)(n - 14));
    }
    if (arp_lookup(PKG_SERVER_IP, mac))
      return true;
  }
  return false;
}

// -----------------------------------------------------------------------
// Local operations
// -----------------------------------------------------------------------

void pkg_init(void) { kprintf("Package Manager: Initializing...\n"); }

bool pkg_install(const char *name) {
  char path[128];
  ksprintf(path, "%s/%s.kzp", PKG_REPO_PATH, name);

  kprintf("pkg: %s paketini yukleyir...\n", name);

  vfs_node_t *node = vfs_find_path(vfs_root, path);
  if (!node) {
    kprintf("pkg: Xehta - %s paketi repoda tapilmadi!\n", path);
    return false;
  }

  uint8_t *buffer = (uint8_t *)kmalloc(node->length);
  if (!buffer) {
    kprintf("pkg: Xehta - Yaddas catmir!\n");
    return false;
  }

  vfs_read(node, 0, node->length, buffer);

  uint32_t magic = *(uint32_t *)buffer;
  if (magic != 0x4B5A504B) { // KZPK
    kprintf("pkg: Xehta - Yanlis paket formati (Invalid Magic)!\n");
    kfree(buffer);
    return false;
  }

  uint32_t file_count = *(uint32_t *)(buffer + 32 + 16 + 4);
  uint8_t *ptr = buffer + 4 + 32 + 16 + 4;

  kprintf("pkg: %d fayl cixarilir...\n", file_count);

  for (uint32_t i = 0; i < file_count; i++) {
    char *filename = (char *)ptr;
    uint32_t size = *(uint32_t *)(ptr + 64);
    uint8_t *data = ptr + 64 + 4;

    kprintf("  -> %s (%d byte)\n", filename, size);

    vfs_node_t *target = vfs_find_path(vfs_root, filename);
    if (!target) {
      char dir_path[128];
      char file_name[128];
      int last_slash = -1;
      for (int k = 0; filename[k]; k++)
        if (filename[k] == '/')
          last_slash = k;

      if (last_slash != -1) {
        int len = (last_slash == 0) ? 1 : last_slash;
        memcpy(dir_path, filename, len);
        dir_path[len] = '\0';
        kstrncpy(file_name, filename + last_slash + 1, 127);
      } else {
        kstrncpy(dir_path, "/", 127);
        kstrncpy(file_name, filename, 127);
      }

      vfs_node_t *dir_node = vfs_find_path(vfs_root, dir_path);
      if (dir_node) {
        target = vfs_create(dir_node, file_name);
      }
    }

    if (target) {
      vfs_write(target, 0, size, data);
    } else {
      kprintf("  [Xehta] %s yaradilmadi!\n", filename);
    }

    ptr += 64 + 4 + size;
  }

  kprintf("pkg: '%s' ugurla yuklendi.\n", name);
  kfree(buffer);
  return true;
}

void pkg_list(void) {
  kprintf("--- Installed Packages ---\n");
  vfs_node_t *node = vfs_finddir(vfs_root, PKG_LIST_PATH);
  if (!node) {
    kprintf("(None)\n");
    return;
  }
  vfs_open(node);

  uint8_t buffer[1024];
  int read = vfs_read(node, 0, 1023, buffer);
  if (read > 0) {
    buffer[read] = '\0';
    kprintf("%s", (char *)buffer);
  }
  vfs_close(node);
}

void pkg_info(const char *name) {
  kprintf("Package: %s\n", name);
  kprintf("Version: 1.0.0 (Alpha)\n");
  kprintf("Maintainer: Khazar OS Team\n");
}

// -----------------------------------------------------------------------
// Network operations (Phase 6)
// -----------------------------------------------------------------------

// pkg_remote_list: Send "LIST\n" to the server, receive a newline-delimited
// list of available packages. Display them on screen.
void pkg_remote_list(void) {
  kprintf("pkg: ARP resolving %d.%d.%d.%d...\n", (PKG_SERVER_IP) & 0xFF,
          (PKG_SERVER_IP >> 8) & 0xFF, (PKG_SERVER_IP >> 16) & 0xFF,
          (PKG_SERVER_IP >> 24) & 0xFF);

  if (!pkg_ensure_arp()) {
    kprintf("pkg: Error - Could not reach server (ARP failed).\n");
    return;
  }

  // Send LIST request
  const char *req = "LIST\n";
  kprintf("pkg: Sending LIST request to server...\n");
  if (!udp_send(PKG_SERVER_IP, PKG_SERVER_PORT, req, 5)) {
    kprintf("pkg: Error - UDP send failed.\n");
    return;
  }

  // Wait for response (allow ~2M iterations ~1-2 seconds in QEMU/VBox)
  static uint8_t resp[1024];
  int n = pkg_udp_recv_wait(resp, sizeof(resp) - 1, 2000000);
  if (n <= 0) {
    kprintf("pkg: Error - No response from server (timeout).\n");
    return;
  }

  resp[n] = '\0';
  kprintf("--- Remote Packages ---\n");
  kprintf("%s\n", (char *)resp);
  kprintf("-----------------------\n");
}

// pkg_fetch: Implements a simple chunked download protocol.
//
// Protocol (stop-and-wait):
//   Client → Server : "GET <name>\n"
//   Server → Client : 4-byte little-endian total size   (first response)
//   Then for each chunk N (0-based):
//     Client → Server : "ACK <N>\n"
//     Server → Client : up to 512 bytes of data
//   When data < 512 bytes → last chunk received.
//   Client → Server : "DONE\n"
//
// The result is written to /repo/<name>.kzp in the VFS.
bool pkg_fetch(const char *name) {
  kprintf("pkg: Fetching '%s' from server...\n", name);

  if (!pkg_ensure_arp()) {
    kprintf("pkg: Error - Cannot reach server.\n");
    return false;
  }

  // Build and send GET request
  char req[64];
  kstrncpy(req, "GET ", 63);
  kstrncpy(req + 4, name, 55);
  int req_len = 0;
  while (req[req_len])
    req_len++;
  req[req_len++] = '\n';
  req[req_len] = '\0';

  if (!udp_send(PKG_SERVER_IP, PKG_SERVER_PORT, req, (uint16_t)req_len)) {
    kprintf("pkg: Error - UDP send failed.\n");
    return false;
  }

  // Wait for server to send 4-byte total size header
  static uint8_t hdr_buf[16];
  int hdr_n = pkg_udp_recv_wait(hdr_buf, sizeof(hdr_buf), 2000000);
  if (hdr_n < 4) {
    kprintf("pkg: Error - Did not receive size header.\n");
    return false;
  }

  uint32_t total_size = *(uint32_t *)hdr_buf;
  if (total_size == 0 || total_size > PKG_MAX_FETCH_SIZE) {
    kprintf("pkg: Error - Invalid package size: %d bytes.\n", total_size);
    return false;
  }
  kprintf("pkg: Package size: %d bytes. Downloading...\n", total_size);

  // Allocate receive buffer
  uint8_t *pkg_buf = (uint8_t *)kmalloc(total_size);
  if (!pkg_buf) {
    kprintf("pkg: Error - Not enough memory (%d bytes needed).\n", total_size);
    return false;
  }

// Download chunks
#define PKG_CHUNK_SIZE 512
  uint32_t received = 0;
  uint32_t chunk_idx = 0;
  static uint8_t chunk_buf[PKG_CHUNK_SIZE + 16];

  while (received < total_size) {
    // Request next chunk
    char ack[32];
    kstrncpy(ack, "ACK ", 31);
    // Write chunk index as decimal
    uint32_t tmp = chunk_idx;
    char num[12];
    int num_len = 0;
    if (tmp == 0) {
      num[num_len++] = '0';
    } else {
      while (tmp > 0) {
        num[num_len++] = '0' + (tmp % 10);
        tmp /= 10;
      }
      // Reverse
      for (int i = 0, j = num_len - 1; i < j; i++, j--) {
        char t = num[i];
        num[i] = num[j];
        num[j] = t;
      }
    }
    num[num_len++] = '\n';
    num[num_len] = '\0';

    int ack_len = 4; // "ACK "
    kstrncpy(ack + 4, num, 27);
    while (ack[ack_len])
      ack_len++;

    if (!udp_send(PKG_SERVER_IP, PKG_SERVER_PORT, ack, (uint16_t)ack_len)) {
      kprintf("pkg: Error sending ACK %d.\n", chunk_idx);
      kfree(pkg_buf);
      return false;
    }

    // Wait for chunk data
    int cn = pkg_udp_recv_wait(chunk_buf, PKG_CHUNK_SIZE, 3000000);
    if (cn <= 0) {
      kprintf("pkg: Error - Timeout waiting for chunk %d.\n", chunk_idx);
      kfree(pkg_buf);
      return false;
    }

    memcpy(pkg_buf + received, chunk_buf, cn);
    received += (uint32_t)cn;
    chunk_idx++;

    // Simple progress bar every 16 chunks
    if ((chunk_idx & 0xF) == 0) {
      kprintf("  [%d / %d bytes]\n", received, total_size);
    }

    if ((uint32_t)cn < PKG_CHUNK_SIZE)
      break; // Last chunk
  }

  // Signal completion
  udp_send(PKG_SERVER_IP, PKG_SERVER_PORT, "DONE\n", 5);

  kprintf("pkg: Download complete. %d bytes received.\n", received);

  // Save to /repo/<name>.kzp
  char save_path[64];
  kstrncpy(save_path, PKG_REPO_PATH, 63);
  int sp_len = 0;
  while (save_path[sp_len])
    sp_len++;
  save_path[sp_len++] = '/';
  kstrncpy(save_path + sp_len, name, 55);
  sp_len = 0;
  while (save_path[sp_len])
    sp_len++;
  kstrncpy(save_path + sp_len, ".kzp", 4);
  save_path[sp_len + 4] = '\0';

  // Find or create /repo directory
  vfs_node_t *repo_dir = vfs_find_path(vfs_root, PKG_REPO_PATH);
  if (!repo_dir) {
    kprintf("pkg: Error - /repo directory not found in VFS.\n");
    kfree(pkg_buf);
    return false;
  }

  // Build just the filename portion: <name>.kzp
  char kzp_name[48];
  kstrncpy(kzp_name, name, 43);
  int kn = 0;
  while (kzp_name[kn])
    kn++;
  kstrncpy(kzp_name + kn, ".kzp", 4);
  kzp_name[kn + 4] = '\0';

  vfs_node_t *pkg_node = vfs_find_path(vfs_root, save_path);
  if (!pkg_node) {
    pkg_node = vfs_create(repo_dir, kzp_name);
  }

  if (!pkg_node) {
    kprintf("pkg: Error - Could not create %s in VFS.\n", save_path);
    kfree(pkg_buf);
    return false;
  }

  vfs_write(pkg_node, 0, received, pkg_buf);
  kfree(pkg_buf);

  kprintf("pkg: '%s.kzp' saved to %s\n", name, PKG_REPO_PATH);
  kprintf("pkg: Run 'pkg install %s' to install it.\n", name);
  return true;
}
