# Khazar OS - Quick Start Guide

## Adım Adım Başlangıç

### 1. WSL2 Kurulumu (İlk Kez)

PowerShell'i **yönetici olarak** aç:

```powershell
wsl --install -d Ubuntu
```

Bilgisayarı yeniden başlat.

### 2. WSL2'yi Aç

Başlat menüsünden "Ubuntu" ara ve aç.

### 3. Gerekli Paketleri Kur

```bash
sudo apt update
sudo apt install -y build-essential nasm xorriso grub-pc-bin grub-common mtools gcc-i686-linux-gnu binutils-i686-linux-gnu mingw-w64
```

### 4. Cross-Compiler Kur

**Seçenek A - Hızlı (Önerilen):**
```bash
sudo apt install -y gcc-multilib g++-multilib
```

**Seçenek B - Manuel (Eğer A çalışmazsa):**
`docs/setup_environment.md` dosyasındaki detaylı talimatları takip et.

### 5. Proje Dizinine Git

```bash
cd "/mnt/c/Users/Admin/Desktop/Ramal/Khazar OS-self-kernel-version"
```

### 6. Build Et

```bash
chmod +x build.sh
./build.sh
```

veya

```bash
make all
```

### 7. VirtualBox'ta Çalıştır

1. **VirtualBox'ı aç**
2. **Yeni VM oluştur:**
   - Name: Khazar OS
   - Type: Other
   - Version: Other/Unknown (32-bit)
   - Memory: 128 MB
   - Hard disk: Yok (Don't add a virtual hard disk)
3. **Settings → Storage:**
   - Controller: IDE → Add Optical Drive
   - Dosya seç: `khazar_os.iso`
4. **Start** butonuna tıkla

### 8. Sonuç

GRUB menüsü görünecek, "Khazar OS" seçeneğini seç. Ekranda şunu görmelisin:

```
Khazar OS
Version 0.1 - Phase 1

System initialized successfully!
Kernel is running...

[OK] Boot completed

Welcome to Khazar OS!
```

## Sorun Giderme

### "i686-linux-gnu-gcc: command not found"

```bash
# Multilib kullan
sudo apt install gcc-multilib g++-multilib

# Makefile defaults can also be overridden without editing the file:
# make CC="gcc -m32" LD=ld
```

### "grub-mkrescue: command not found"

```bash
sudo apt install grub-pc-bin grub-common xorriso
```

### VirtualBox boot etmiyor

1. VM Settings → System → Enable I/O APIC
2. VM Settings → System → Processor → 1 CPU
3. ISO dosyasının doğru seçildiğinden emin ol

### Kod değişikliği yaptım ama etki etmiyor

```bash
make clean
make all
```

## Kod Değiştirme

### Ekran mesajını değiştir:

`kernel/kernel.c` dosyasını aç ve `terminal_writestring()` satırlarını düzenle.

### Renkleri değiştir:

`include/vga.h` dosyasında renk enum'larını kullan:
- `VGA_COLOR_WHITE`
- `VGA_COLOR_LIGHT_BLUE`
- `VGA_COLOR_LIGHT_GREEN`
- vb.

Örnek:
```c
terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK));
terminal_writestring("Mavi yazı!\n");
```

## Sonraki Adımlar

Phase 2'de şunları ekleyeceğiz:
- Keyboard driver
- Interrupt handling (IDT)
- Timer
- Daha gelişmiş terminal

---

**Tebrikler!** İlk işletim sistemini oluşturdun! 🎉
