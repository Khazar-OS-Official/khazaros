# Khazar OS

**Gerçek bir işletim sistemi geliştirme projesi**

## Proje Bilgileri

- **Platform**: x86 32-bit (Protected Mode)
- **Bootloader**: GRUB (Multiboot uyumlu)
- **Geliştirme Ortamı**: Windows 10 + WSL2
- **Toolchain**: i686-linux-gnu-gcc/binutils, i686-w64-mingw32-gcc, NASM
- **Test Ortamı**: VirtualBox

## Özellikler (Phase 1)

✅ Multiboot uyumlu bootloader  
✅ VGA text mode driver (0xB8000)  
✅ Ekrana "Khazar OS" yazdırma  
✅ Renkli terminal çıktısı  

## Dizin Yapısı

```
Khazar OS/
├── boot/
│   └── boot.asm          # Assembly bootloader
├── kernel/
│   ├── kernel.c          # Ana kernel
│   └── vga.c             # VGA driver
├── include/
│   └── vga.h             # VGA header
├── docs/
│   └── setup_environment.md  # Kurulum rehberi
├── linker.ld             # Linker script
├── Makefile              # Build automation
└── grub.cfg              # GRUB config
```

## Kurulum

Detaylı kurulum için `docs/setup_environment.md` dosyasına bakın.

### Hızlı Başlangıç (WSL2)

```bash
# Proje dizinine git
cd /mnt/c/Users/Admin/Desktop/Ramal/Khazar\ OS

# Build
make clean
make all

# ISO oluşturuldu: khazar_os.iso
```

## VirtualBox'ta Çalıştırma

1. VirtualBox'ta yeni VM oluştur
2. Type: Other, Version: Other/Unknown (32-bit)
3. Memory: 128 MB
4. Storage → Add CD/DVD → `khazar_os.iso` seç
5. Start

## Teknik Detaylar

### Multiboot Header
- Magic: `0x1BADB002`
- Flags: `0x00000003` (page align + memory info)
- Checksum: `-(magic + flags)`

### Bellek Haritası
- `0x00000000 - 0x000FFFFF`: BIOS/Donanım (1MB)
- `0x00100000+`: Kernel başlangıcı
- `0x000B8000`: VGA text buffer

### VGA Text Mode
- 80x25 karakter matrisi
- Her karakter 2 byte: [ASCII][Attribute]
- Attribute: 4 bit background + 4 bit foreground

## Gelecek Fazlar

- [ ] Interrupt handling (IDT)
- [ ] Keyboard driver
- [ ] Memory management (paging)
- [ ] Multitasking
- [ ] File system

## Lisans

Bu proje eğitim amaçlıdır.

---

**Khazar OS** - Built with passion 🚀
