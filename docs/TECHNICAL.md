# Khazar OS - Technical Documentation

## Architecture Overview

### Boot Process Flow

```
BIOS → GRUB → Multiboot Header → boot.asm → kernel_main() → VGA Output → Infinite Loop
```

1. **BIOS**: Donanımı başlatır, GRUB'ı yükler
2. **GRUB**: ISO'dan kernel binary'yi bulur, multiboot header'ı kontrol eder
3. **Multiboot Header**: Magic number (0x1BADB002) ile kernel'in geçerli olduğunu doğrular
4. **boot.asm**: Stack kurar, EFLAGS temizler, kernel_main'e atlar
5. **kernel_main()**: Terminal başlatır, mesajları yazar, infinite loop'a girer

### Memory Layout

```
0x00000000 - 0x000003FF  : Interrupt Vector Table (IVT)
0x00000400 - 0x000004FF  : BIOS Data Area
0x00000500 - 0x00007BFF  : Free (konvansiyonel bellek)
0x00007C00 - 0x00007DFF  : Bootloader (512 bytes)
0x00007E00 - 0x0009FFFF  : Free
0x000A0000 - 0x000BFFFF  : Video memory
  0x000B8000 - 0x000B8FA0  : VGA text buffer (80x25x2 = 4000 bytes)
0x000C0000 - 0x000FFFFF  : BIOS ROM
0x00100000+              : Kernel (1MB+)
```

**Neden 1MB'den başlatıyoruz?**
- İlk 1MB BIOS, video bellek ve donanım için rezerve
- Protected mode'da güvenli alan
- Standart OS geliştirme pratiği

## Multiboot Specification

### Header Structure

```c
struct multiboot_header {
    uint32_t magic;      // 0x1BADB002
    uint32_t flags;      // 0x00000003
    uint32_t checksum;   // -(magic + flags)
};
```

### Flags Breakdown (0x00000003)

- Bit 0 (1): Page align modules
- Bit 1 (1): Provide memory information
- Bit 2-15: Reserved (0)
- Bit 16: Video mode info (0)

**Neden bu flagler?**
- Page alignment: Bellek yönetimi için gerekli
- Memory info: Kernel'e kullanılabilir RAM miktarını bildirir

## VGA Text Mode

### Buffer Structure

VGA text buffer `0xB8000` adresinde başlar, 4000 byte (80x25x2).

Her karakter 2 byte:
```
[Byte 0: ASCII] [Byte 1: Attribute]
```

### Attribute Byte

```
Bit 7    : Blink (yanıp sönme)
Bit 6-4  : Background color (3 bit = 8 renk)
Bit 3    : Foreground intensity
Bit 2-0  : Foreground color (3 bit = 8 renk)
```

Örnek:
```c
// Beyaz metin, siyah arka plan
uint8_t attr = 0x0F;  // 0000 1111
// Background: 0 (siyah), Foreground: 15 (beyaz)
```

### Cursor Positioning

```c
size_t index = row * VGA_WIDTH + column;
terminal_buffer[index] = vga_entry(character, color);
```

## Assembly Bootloader Detayları

### Stack Setup

```asm
mov esp, stack_top
```

**Neden ESP?**
- x86'da stack pointer register
- Stack yukarıdan aşağı büyür (high → low address)
- `stack_top` en yüksek adres

### EFLAGS Temizleme

```asm
push 0
popf
```

**Neden gerekli?**
- BIOS/GRUB EFLAGS'i belirsiz durumda bırakabilir
- Interrupt flag, direction flag vb. temizlenir
- Tahmin edilebilir başlangıç durumu

### HLT Instruction

```asm
hlt
```

**Ne yapar?**
- CPU'yu düşük güç moduna alır
- Sonraki interrupt'a kadar bekler
- Infinite loop'ta kullanarak enerji tasarrufu sağlar

## C Kernel Detayları

### Freestanding Environment

```c
-ffreestanding
```

**Ne demek?**
- Standart C kütüphanesi yok (libc)
- `printf`, `malloc` vb. kullanılamaz
- Her şeyi kendimiz yazmalıyız

### No Standard Library

```c
-nostdlib
```

**Neden?**
- Standart kütüphane OS servisleri gerektirir (syscall)
- Kernel henüz bu servisleri sağlamıyor
- Circular dependency: OS, OS servislerine ihtiyaç duyamaz

### Inline Assembly

```c
__asm__ __volatile__("hlt");
```

- `__asm__`: GCC inline assembly
- `__volatile__`: Compiler optimizasyonunu engelle
- `hlt`: CPU instruction

## Linker Script Detayları

### ENTRY Directive

```ld
ENTRY(_start)
```

Entry point'i belirtir. Debugger ve bazı bootloader'lar için gerekli.

### Section Alignment

```ld
.text BLOCK(4K) : ALIGN(4K)
```

**Neden 4KB?**
- x86 page size = 4KB
- Paging implementasyonunda kolaylık
- Cache efficiency

### Section Order

```
.multiboot → .text → .rodata → .data → .bss
```

**Neden bu sıra?**
1. Multiboot header en başta olmalı (GRUB gereksinimi)
2. Code (.text) hemen sonra
3. Read-only data
4. Initialized data
5. Uninitialized data (BSS sıfırlanır)

## Build Process

### Compilation Steps

```bash
# 1. Assembly → Object
nasm -f elf32 boot/boot.asm -o boot/boot.o

# 2. C → Object
i686-linux-gnu-gcc -c kernel/kernel.c -o kernel/kernel.o

# 3. Link
i686-linux-gnu-ld -T linker.ld -o khazar_kernel.bin boot.o kernel.o vga.o

# 4. Create ISO
grub-mkrescue -o khazar_os.iso iso/
```

### ELF Format

Kernel binary ELF formatında:
- **ELF Header**: Magic number, architecture, entry point
- **Program Headers**: Loadable segments
- **Section Headers**: .text, .data, .bss vb.

GRUB ELF'i parse eder ve doğru adreslere yükler.

## Debugging Tips

### Check Multiboot Header

```bash
objdump -h khazar_kernel.bin
# .multiboot section en başta olmalı
```

### Verify Entry Point

```bash
readelf -h khazar_kernel.bin
# Entry point: 0x100000 civarı
```

### Disassemble

```bash
objdump -d khazar_kernel.bin | less
# Assembly kodunu görüntüle
```

### VirtualBox Serial Output

VM Settings → Serial Ports → Enable Serial Port
```c
// kernel.c'ye ekle
outb(0x3F8, 'X');  // COM1'e yaz
```

## Performance Considerations

### VGA Write Speed

Doğrudan bellek erişimi çok hızlı:
- ~1 cycle per write
- DMA yok, CPU direct access

### Infinite Loop Optimization

```c
while(1) { hlt; }
```

`hlt` olmadan CPU %100 kullanır. `hlt` ile ~%0.

## Future Enhancements (Phase 2+)

1. **IDT (Interrupt Descriptor Table)**
   - Keyboard interrupts
   - Timer interrupts
   - Exception handling

2. **GDT (Global Descriptor Table)**
   - Proper segment descriptors
   - Ring 0/3 separation

3. **Paging**
   - Virtual memory
   - Memory protection

4. **Multitasking**
   - Task switching
   - Scheduler

5. **File System**
   - FAT32 or custom FS
   - Disk I/O

## References

- [OSDev Wiki](https://wiki.osdev.org/)
- [Multiboot Specification](https://www.gnu.org/software/grub/manual/multiboot/multiboot.html)
- [Intel x86 Manual](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)
- [VGA Hardware](http://www.osdever.net/FreeVGA/vga/vga.htm)

---

**Khazar OS Technical Documentation v1.0**
