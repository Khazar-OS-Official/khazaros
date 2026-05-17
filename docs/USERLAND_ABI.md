# Khazar OS - Userland ABI (Application Binary Interface)

This document describes the Application Binary Interface (ABI) for Khazar OS userland applications, including the system call interface and register conventions.

## System Call Interface

Khazar OS uses software interrupts to handle system calls from user space to kernel space. 
The system call interrupt vector is **`int 0x80`**.

### Register Conventions

When making a system call, the following registers must be populated:

- `EAX`: System call number
- `EBX`: First argument (Arg 1)
- `ECX`: Second argument (Arg 2)
- `EDX`: Third argument (Arg 3)
- `ESI`: Fourth argument (Arg 4)

Upon returning from a system call, the result is stored in the **`EAX`** register. Negative values generally indicate an error code (defined in `kernel/klog.h` as `KERR_*`), while zero or positive values indicate success.

### System Call Table

| Number | Macro Name | Arguments | Description |
|---|---|---|---|
| 1 | `SYS_EXIT` | `ebx` = status | Terminates the current process. |
| 3 | `SYS_READ` | `ebx` = fd, `ecx` = buf, `edx` = size | Reads data from a file descriptor. |
| 4 | `SYS_WRITE` | `ebx` = fd, `ecx` = buf, `edx` = size | Writes data to a file descriptor. |
| 5 | `SYS_OPEN` | `ebx` = path | Opens a file and returns its file descriptor. |
| 6 | `SYS_CLOSE` | `ebx` = fd | Closes a file descriptor. |
| 11 | `SYS_EXEC` | `ebx` = path, `ecx` = cmdline | Executes a new process. |
| 12 | `SYS_GETCMDLINE` | `ebx` = buf, `ecx` = size | Gets the command line string of the current process. |
| 13 | `SYS_NETSEND` | `ebx` = ip, `ecx` = port, `edx` = buf, `esi` = len | Sends a UDP network packet. |
| 14 | `SYS_NETRECV` | `ebx` = port, `ecx` = buf, `edx` = maxlen | Receives a UDP network packet. |
| 15 | `SYS_CHMOD` | `ebx` = path, `ecx` = mode | Changes the permissions of a file. |
| 16 | `SYS_UNLINK` | `ebx` = path | Removes a file from the filesystem. |
| 17 | `SYS_MKDIR` | `ebx` = path | Creates a new directory. |
| 18 | `SYS_CREATE` | `ebx` = path | Creates a new empty file. |
| 19 | `SYS_PS` | `ebx` = buf, `ecx` = size | Retrieves process information into the provided buffer. |
| 78 | `SYS_GETDENTS` | `ebx` = fd, `ecx` = buf, `edx` = index | Reads directory entries. |
| 88 | `SYS_REBOOT` | None | Reboots the system. |
| 89 | `SYS_SHUTDOWN` | None | Halts the system. |

### Memory Safety

All pointers passed from userland to the kernel (e.g., buffers, paths) are validated by the kernel to ensure they fall within the user-space memory range (`0x00400000` to `0xBFFFFFFF`). Invalid pointers will result in a `KERR_FAULT` (-5) error code.
