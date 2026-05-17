#ifndef PKG_H
#define PKG_H

#include <libk/types.h>

#define PKG_NAME_MAX 32
#define PKG_REPO_PATH "/repo"
#define PKG_LIST_PATH "/system/packages.list"

// Remote server: VirtualBox host-gateway (10.0.2.2)
// The server must be running a simple UDP package server on this port.
#define PKG_SERVER_IP 0x0202000A // 10.0.2.2 (little-endian)
#define PKG_SERVER_PORT 5555

// Maximum size of a downloadable package via one fetch
#define PKG_MAX_FETCH_SIZE (256 * 1024) // 256 KB

typedef struct {
  char name[PKG_NAME_MAX];
  char version[16];
  uint32_t size;
} package_t;

// Local operations
void pkg_init(void);
bool pkg_install(const char *name);
void pkg_list(void);
void pkg_info(const char *name);

// Network operations (Phase 6)
void pkg_remote_list(void);
bool pkg_fetch(const char *name);

#endif // PKG_H
