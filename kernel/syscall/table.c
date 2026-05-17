#include <arch/isr.h>
#include <drivers/power.h>
#include <drivers/vga.h>
#include <fs/vfs.h>
#include <kernel/klog.h>
#include <kernel/syscall.h>
#include <kernel/tty.h>
#include <libk/string.h>
#include <mm/kheap.h>
#include <net/udp.h>
#include <proc/process.h>

/* ── Kullanici Pointer Dogrulama ─────────────────────────────────────────
 * Userland pointer'larinin gecerli user-space araliginda olduğunu kontrol eder.
 * Khazar OS x86 32-bit higher-half: user space = 0x00400000 - 0xBFFFFFFF
 * NULL veya kernel adresine dusen pointer'lar reddedilir.
 */
#define USER_SPACE_MIN  0x00400000U
#define USER_SPACE_MAX  0xBFFFFFFFU

static inline bool syscall_validate_ptr(const void *ptr, size_t size) {
  if (!ptr) return false;
  uint32_t addr = (uint32_t)ptr;
  if (addr < USER_SPACE_MIN) return false;
  if (addr > USER_SPACE_MAX) return false;
  if (size > 0 && (addr + (uint32_t)size - 1) > USER_SPACE_MAX) return false;
  return true;
}

// Syscall handler

void syscall_handler(registers_t *regs) {
  uint32_t syscall_num = regs->eax;
  thread_t *thread = scheduler_get_current_thread();
  process_t *proc = thread ? thread->process : NULL;

  if (syscall_num == SYS_REBOOT) { power_reboot(); return; }
  if (syscall_num == SYS_SHUTDOWN) { power_shutdown(); return; }
  if (syscall_num == SYS_EXIT) {
    if (proc) process_terminate(proc, (int)regs->ebx);
    // Yield to scheduler so process can immediately stop executing
    extern registers_t *scheduler_schedule(registers_t *regs);
    // As we can't directly call schedule to switch context without assembly,
    // we just mark thread terminated and return. The interrupt return logic
    // might continue execution if not careful, but the scheduler will pick it up on next timer tick.
    // For immediate effect:
    __asm__ volatile("int $32"); // Trigger timer interrupt to force reschedule
    return;
  }

  int ret = KERR_NOSYS;

  if (syscall_num == SYS_WRITE) {
    int fd      = (int)regs->ebx;
    char *buf   = (char *)regs->ecx;
    size_t size = (size_t)regs->edx;

    if (!syscall_validate_ptr(buf, size)) {
      LOG_WARN("syscall", "SYS_WRITE: invalid user buffer 0x%x", (uint32_t)buf);
      regs->eax = (uint32_t)KERR_FAULT; return;
    }
    if (fd == 1 || fd == 2) {
      terminal_write(buf, size); ret = (int)size;
    } else if (proc && fd >= 0 && fd < MAX_PROCESS_FDS && proc->fd_table[fd]) {
      ret = vfs_write(proc->fd_table[fd], 0, size, (uint8_t *)buf);
    } else { ret = KERR_INVAL; }

  } else if (syscall_num == SYS_READ) {
    int fd        = (int)regs->ebx;
    char *buf     = (char *)regs->ecx;
    uint32_t size = (uint32_t)regs->edx;

    if (!syscall_validate_ptr(buf, size)) {
      LOG_WARN("syscall", "SYS_READ: invalid user buffer 0x%x", (uint32_t)buf);
      regs->eax = (uint32_t)KERR_FAULT; return;
    }
    if (fd == 0) {
      ret = tty_read(buf, size);
    } else if (proc && fd >= 0 && fd < MAX_PROCESS_FDS && proc->fd_table[fd]) {
      ret = vfs_read(proc->fd_table[fd], 0, size, (uint8_t *)buf);
    } else { ret = KERR_INVAL; }

  } else if (syscall_num == SYS_OPEN) {
    char *name = (char *)regs->ebx;
    if (!syscall_validate_ptr(name, 1)) {
      LOG_WARN("syscall", "SYS_OPEN: invalid path 0x%x", (uint32_t)name);
      regs->eax = (uint32_t)KERR_FAULT; return;
    }
    extern vfs_node_t *vfs_find_path(vfs_node_t *root, const char *path);
    vfs_node_t *node = vfs_find_path(vfs_root, name);
    ret = KERR_NOTFOUND;
    if (proc && node) {
      for (int i = 3; i < MAX_PROCESS_FDS; i++) {
        if (proc->fd_table[i] == NULL) { proc->fd_table[i] = node; ret = i; break; }
      }
    }

  } else if (syscall_num == SYS_CLOSE) {
    int fd = (int)regs->ebx;
    if (proc && fd >= 3 && fd < MAX_PROCESS_FDS && proc->fd_table[fd]) {
      vfs_close(proc->fd_table[fd]);
      proc->fd_table[fd] = NULL;
      ret = KERR_OK;
    } else { ret = KERR_INVAL; }

  } else if (syscall_num == 11) { // SYS_EXEC
    char *name = (char *)regs->ebx;
    if (!syscall_validate_ptr(name, 1)) {
      LOG_WARN("syscall", "SYS_EXEC: invalid name 0x%x", (uint32_t)name);
      regs->eax = (uint32_t)KERR_FAULT; return;
    }
    extern vfs_node_t *vfs_find_path(vfs_node_t *root, const char *path);
    vfs_node_t *node = vfs_find_path(vfs_root, name);
    ret = KERR_NOTFOUND;
    if (node) {
      uint8_t *file_buf = (uint8_t *)kmalloc(node->length);
      ret = file_buf ? KERR_NOMEM : KERR_NOMEM;
      if (file_buf) {
        vfs_read(node, 0, node->length, file_buf);
        extern bool pe_load(uint8_t *file_data, uint32_t file_size,
                            uint32_t *entry_point, uint32_t *user_stack_top,
                            void *target_dir);
        extern process_t *process_create_user(const char *name,
                            uint32_t entry_point, uint32_t user_stack_top);
        uint32_t ep = 0, ust = 0;
        process_t *new_proc = process_create_user(name, 0, 0);
        ret = KERR_NOMEM;
        if (new_proc && new_proc->threads) {
          thread_t *new_thread = new_proc->threads;
          if (pe_load(file_buf, node->length, &ep, &ust, new_proc->page_directory)) {
            new_thread->context->eip     = ep;
            new_thread->context->useresp = ust;
            new_thread->context->ebp     = ust;
            char *cmd = (char *)regs->ecx;
            if (cmd && syscall_validate_ptr(cmd, 1))
              kstrncpy(new_proc->cmdline, cmd, 127);
            else
              new_proc->cmdline[0] = '\0';
            extern void scheduler_add_thread(thread_t *thread);
            scheduler_add_thread(new_thread);
            ret = KERR_OK;
          } else {
            new_thread->state = THREAD_STATE_TERMINATED;
            ret = KERR_IO;
          }
        }
        kfree(file_buf);
      }
    }

  } else if (syscall_num == 78) { // SYS_GETDENTS
    int fd    = (int)regs->ebx;
    void *buf = (void *)regs->ecx;
    int index = (int)regs->edx;

    if (!syscall_validate_ptr(buf, 128)) {
      LOG_WARN("syscall", "SYS_GETDENTS: invalid buf 0x%x", (uint32_t)buf);
      regs->eax = (uint32_t)KERR_FAULT; return;
    }
    ret = KERR_INVAL;
    if (proc && fd >= 3 && fd < MAX_PROCESS_FDS && proc->fd_table[fd]) {
      vfs_node_t *dir_node = proc->fd_table[fd];
      if ((dir_node->flags & VFS_DIRECTORY) == VFS_DIRECTORY) {
        vfs_node_t *child = vfs_readdir(dir_node, index);
        if (child) {
          typedef struct {
            uint32_t d_ino; uint32_t d_off; uint16_t d_reclen;
            uint16_t d_mode; unsigned char d_type; char d_name[128];
          } __attribute__((packed)) kernel_dirent_t;
          kernel_dirent_t *out = (kernel_dirent_t *)buf;
          out->d_ino    = child->inode;
          out->d_off    = child->length;
          out->d_reclen = sizeof(kernel_dirent_t);
          out->d_mode   = (uint16_t)child->mask;
          out->d_type   = (child->flags & VFS_DIRECTORY) ? 2 : 1;
          int nl = strlen(child->name); if (nl > 127) nl = 127;
          kstrncpy(out->d_name, child->name, nl);
          out->d_name[nl] = '\0';
          kfree(child);
          ret = 1;
        } else { ret = 0; }
      }
    }

  } else if (syscall_num == 12) { // SYS_GETCMDLINE
    char *buf     = (char *)regs->ebx;
    uint32_t size = (uint32_t)regs->ecx;
    if (!syscall_validate_ptr(buf, size)) {
      regs->eax = (uint32_t)KERR_FAULT; return;
    }
    ret = KERR_INVAL;
    if (proc && size > 0) {
      kstrncpy(buf, proc->cmdline, size - 1);
      buf[size - 1] = '\0';
      ret = KERR_OK;
    }

  } else if (syscall_num == 13) { // SYS_NETSEND
    uint32_t dst_ip    = regs->ebx;
    uint16_t port      = (uint16_t)regs->ecx;
    const uint8_t *buf = (const uint8_t *)regs->edx;
    uint16_t len       = (uint16_t)(regs->esi);
    if (!syscall_validate_ptr(buf, len)) {
      regs->eax = (uint32_t)KERR_FAULT; return;
    }
    ret = udp_send(dst_ip, port, buf, len) ? KERR_OK : KERR_IO;

  } else if (syscall_num == 14) { // SYS_NETRECV
    uint16_t port   = (uint16_t)regs->ebx;
    uint8_t *buf    = (uint8_t *)regs->ecx;
    uint16_t maxlen = (uint16_t)regs->edx;
    if (!syscall_validate_ptr(buf, maxlen)) {
      regs->eax = (uint32_t)KERR_FAULT; return;
    }
    ret = udp_recv(port, buf, maxlen);

  } else if (syscall_num == 15) { // SYS_CHMOD
    char *path    = (char *)regs->ebx;
    uint32_t mode = (uint32_t)regs->ecx;
    if (!syscall_validate_ptr(path, 1)) {
      regs->eax = (uint32_t)KERR_FAULT; return;
    }
    extern vfs_node_t *vfs_find_path(vfs_node_t *root, const char *path);
    vfs_node_t *node = vfs_find_path(vfs_root, path);
    if (node) { node->mask = mode; ret = KERR_OK; }
  } else if (syscall_num == 16) { // SYS_UNLINK
    char *path = (char *)regs->ebx;
    if (!syscall_validate_ptr(path, 1)) { regs->eax = (uint32_t)KERR_FAULT; return; }
    char parent_path[128], name[128];
    int last_slash = -1;
    for(int i=0; path[i]; i++) if(path[i] == '/') last_slash = i;
    if(last_slash == -1) { kstrncpy(parent_path, "/", 2); kstrncpy(name, path, 127); }
    else {
      if(last_slash == 0) { kstrncpy(parent_path, "/", 2); }
      else { kstrncpy(parent_path, path, last_slash); parent_path[last_slash] = '\0'; }
      kstrncpy(name, path + last_slash + 1, 127);
    }
    extern vfs_node_t *vfs_find_path(vfs_node_t *root, const char *path);
    vfs_node_t *parent = vfs_find_path(vfs_root, parent_path);
    if(parent) { ret = vfs_unlink(parent, name) ? KERR_OK : KERR_IO; }
    else { ret = KERR_NOTFOUND; }

  } else if (syscall_num == 17) { // SYS_MKDIR
    char *path = (char *)regs->ebx;
    if (!syscall_validate_ptr(path, 1)) { regs->eax = (uint32_t)KERR_FAULT; return; }
    char parent_path[128], name[128];
    int last_slash = -1;
    for(int i=0; path[i]; i++) if(path[i] == '/') last_slash = i;
    if(last_slash == -1) { kstrncpy(parent_path, "/", 2); kstrncpy(name, path, 127); }
    else {
      if(last_slash == 0) { kstrncpy(parent_path, "/", 2); }
      else { kstrncpy(parent_path, path, last_slash); parent_path[last_slash] = '\0'; }
      kstrncpy(name, path + last_slash + 1, 127);
    }
    extern vfs_node_t *vfs_find_path(vfs_node_t *root, const char *path);
    vfs_node_t *parent = vfs_find_path(vfs_root, parent_path);
    if(parent) { ret = vfs_mkdir(parent, name) ? KERR_OK : KERR_IO; }
    else { ret = KERR_NOTFOUND; }

  } else if (syscall_num == 18) { // SYS_CREATE
    char *path = (char *)regs->ebx;
    if (!syscall_validate_ptr(path, 1)) { regs->eax = (uint32_t)KERR_FAULT; return; }
    char parent_path[128], name[128];
    int last_slash = -1;
    for(int i=0; path[i]; i++) if(path[i] == '/') last_slash = i;
    if(last_slash == -1) { kstrncpy(parent_path, "/", 2); kstrncpy(name, path, 127); }
    else {
      if(last_slash == 0) { kstrncpy(parent_path, "/", 2); }
      else { kstrncpy(parent_path, path, last_slash); parent_path[last_slash] = '\0'; }
      kstrncpy(name, path + last_slash + 1, 127);
    }
    extern vfs_node_t *vfs_find_path(vfs_node_t *root, const char *path);
    vfs_node_t *parent = vfs_find_path(vfs_root, parent_path);
    if(parent) { ret = vfs_create(parent, name) ? KERR_OK : KERR_IO; }
    else { ret = KERR_NOTFOUND; }

  } else if (syscall_num == 19) { // SYS_PS
    char *buf = (char *)regs->ebx;
    uint32_t size = (uint32_t)regs->ecx;
    if (!syscall_validate_ptr(buf, size)) { regs->eax = (uint32_t)KERR_FAULT; return; }
    extern process_t *process_get_list(void);
    process_t *p = process_get_list();
    uint32_t offset = 0;
    while(p && offset < size - 128) {
      extern void ksnprintf(char *str, size_t n, const char *format, ...);
      ksnprintf(buf + offset, size - offset, "PID %d: %s (%s)\n", (int)p->pid, p->name, p->cmdline);
      offset += strlen(buf + offset);
      p = p->next;
    }
    ret = KERR_OK;
  } else if (syscall_num == 7) { // SYS_WAITPID
    uint32_t target_pid = regs->ebx;
    int *status_ptr = (int *)regs->ecx;
    if (status_ptr && !syscall_validate_ptr(status_ptr, sizeof(int))) {
      regs->eax = (uint32_t)KERR_FAULT; return;
    }
    ret = process_waitpid(target_pid, status_ptr);

  } else if (syscall_num == 37) { // SYS_KILL
    uint32_t target_pid = regs->ebx;
    int sig = (int)regs->ecx;
    extern process_t *process_get_by_pid(uint32_t pid);
    process_t *target = process_get_by_pid(target_pid);
    if (!target) {
      ret = KERR_NOTFOUND;
    } else {
      process_terminate(target, 128 + sig); // Simple kill with signal as status
      ret = KERR_OK;
    }
  }

  regs->eax = (uint32_t)ret;
}

void syscall_init(void) {
  register_interrupt_handler(0x80, syscall_handler);
  kprintf("SYSCALL: Initialized at INT 0x80\n");
}
