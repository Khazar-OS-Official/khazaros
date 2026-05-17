#ifndef KLOG_H
#define KLOG_H

/*
 * Khazar OS - Unified Kernel Logging
 * ------------------------------------
 * Kullanim:
 *   LOG_DEBUG("kheap", "Alloc %d bytes", size);
 *   LOG_INFO ("fat32", "Root cluster: %d", cluster);
 *   LOG_WARN ("ahci",  "Port %d hung", i);
 *   LOG_ERROR("vmm",   "Page map failed at 0x%x", addr);
 *
 * Seviyeler: DEBUG < INFO < WARN < ERROR
 * Makro disable icin: #define KLOG_LEVEL 2  (sadece WARN+ERROR)
 */

#ifndef KLOG_LEVEL
#define KLOG_LEVEL 0  /* 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR */
#endif

#include <drivers/serial.h>
#include <drivers/vga.h>

/* ksprintf icin */
extern void ksprintf(char *str, const char *fmt, ...);

/* Dahili: prefix + mesaj + newline seri porta yaz */
static inline void _klog_write(const char *level, const char *subsys,
                                const char *msg) {
    serial_write_string("[");
    serial_write_string(level);
    serial_write_string("][");
    serial_write_string(subsys);
    serial_write_string("] ");
    serial_write_string(msg);
    serial_write_string("\n");
}

/* LOG_DEBUG - sadece gelistirme sirasinda */
#if KLOG_LEVEL <= 0
#define LOG_DEBUG(subsys, fmt, ...) \
    do { char _klog_buf[128]; ksprintf(_klog_buf, fmt, ##__VA_ARGS__); \
         _klog_write("DBG ", subsys, _klog_buf); } while(0)
#else
#define LOG_DEBUG(subsys, fmt, ...) do {} while(0)
#endif

/* LOG_INFO - normal akis */
#if KLOG_LEVEL <= 1
#define LOG_INFO(subsys, fmt, ...) \
    do { char _klog_buf[128]; ksprintf(_klog_buf, fmt, ##__VA_ARGS__); \
         _klog_write("INFO", subsys, _klog_buf); } while(0)
#else
#define LOG_INFO(subsys, fmt, ...) do {} while(0)
#endif

/* LOG_WARN - beklenmedik ama kurtarilabilir durum */
#if KLOG_LEVEL <= 2
#define LOG_WARN(subsys, fmt, ...) \
    do { char _klog_buf[128]; ksprintf(_klog_buf, fmt, ##__VA_ARGS__); \
         _klog_write("WARN", subsys, _klog_buf); } while(0)
#else
#define LOG_WARN(subsys, fmt, ...) do {} while(0)
#endif

/* LOG_ERROR - kritik hata */
#define LOG_ERROR(subsys, fmt, ...) \
    do { char _klog_buf[128]; ksprintf(_klog_buf, fmt, ##__VA_ARGS__); \
         _klog_write("ERR ", subsys, _klog_buf); } while(0)


/* ─── Tutarlı Hata Kodları ────────────────────────────────────────────────
 * Tum kernel subsystem'lari bu degerler uzerinden donus yapar.
 * Negatif int olarak kullanilir (POSIX-benzeri).
 */
#define KERR_OK          0   /* Basari                                  */
#define KERR_GENERIC    -1   /* Genel hata                              */
#define KERR_NOMEM      -2   /* Bellek yetersiz (kmalloc baskisiz)      */
#define KERR_INVAL      -3   /* Gecersiz parametre / NULL pointer       */
#define KERR_IO         -4   /* Disk / donanim okuma-yazma hatasi       */
#define KERR_NOTFOUND   -5   /* Dosya / node / cihaz bulunamadi        */
#define KERR_BUSY       -6   /* Kaynak mesgul (port, slot, kilit)      */
#define KERR_PERM       -7   /* Erisim izni yok                        */
#define KERR_NOSYS      -8   /* Syscall implemente edilmemis           */
#define KERR_FAULT      -9   /* Gecersiz kullanici pointer             */
#define KERR_OVERFLOW   -10  /* Buffer / alan tasimi                   */
#define KERR_CORRUPT    -11  /* Veri / magic bozuk                     */

#endif /* KLOG_H */
