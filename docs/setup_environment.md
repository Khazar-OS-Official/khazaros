# Khazar OS - Development Environment Setup (Windows 10)

## Gereksinimler

- Windows 10/11
- VirtualBox 6.0+
- WSL2 (Windows Subsystem for Linux)

## Adım 1: WSL2 Kurulumu

### PowerShell'i Yönetici Olarak Aç ve Çalıştır:

```powershell
wsl --install -d Ubuntu
```

Bilgisayarı yeniden başlat. Ubuntu ilk açılışta kullanıcı adı ve şifre iste yecek.

## Adım 2: WSL2 İçinde Gerekli Paketleri Kur

WSL Ubuntu terminalini aç ve şu komutları çalıştır:

```bash
sudo apt update
sudo apt upgrade -y

# Build araçları
sudo apt install -y build-essential nasm xorriso grub-pc-bin grub-common mtools gcc-i686-linux-gnu binutils-i686-linux-gnu mingw-w64

# Cross-compiler bağımlılıkları
sudo apt install -y bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo
```

## Adım 3: Cross-Compiler Kurulumu

Not: Makefile default olaraq `i686-linux-gnu-*` toolchain istifadə edir. Aşağıdakı manual `i686-elf` qurulumu seçilərsə build zamanı `make CC=i686-elf-gcc LD=i686-elf-ld` kimi override verilməlidir.

### Seçenek A: Manuel Derleme (Önerilen)

```bash
# Geçici dizin oluştur
mkdir -p ~/cross-compiler
cd ~/cross-compiler

# Binutils indir ve derle
wget https://ftp.gnu.org/gnu/binutils/binutils-2.39.tar.gz
tar -xzf binutils-2.39.tar.gz
mkdir build-binutils
cd build-binutils
../binutils-2.39/configure --target=i686-elf --prefix=/usr/local/cross --disable-nls --disable-werror
make -j$(nproc)
sudo make install
cd ..

# GCC indir ve derle
wget https://ftp.gnu.org/gnu/gcc/gcc-12.2.0/gcc-12.2.0.tar.gz
tar -xzf gcc-12.2.0.tar.gz
mkdir build-gcc
cd build-gcc
../gcc-12.2.0/configure --target=i686-elf --prefix=/usr/local/cross --disable-nls --enable-languages=c,c++ --without-headers
make all-gcc -j$(nproc)
make all-target-libgcc -j$(nproc)
sudo make install-gcc
sudo make install-target-libgcc

# PATH'e ekle
echo 'export PATH="/usr/local/cross/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

### Seçenek B: Hazır Paket (Daha Hızlı)

```bash
# Bazı dağıtımlarda hazır paket var
sudo apt install -y gcc-i686-linux-gnu binutils-i686-linux-gnu
```

**Not**: Seçenek B çalışmazsa mutlaka Seçenek A'yı kullan.

## Adım 4: Kurulumu Doğrula

```bash
i686-linux-gnu-gcc --version
i686-linux-gnu-ld --version
i686-w64-mingw32-gcc --version
nasm --version
grub-mkrescue --version
xorriso --version
```

Hepsi versiyon numarası göstermeli.

## Adım 5: Proje Dizinine Eriş

WSL2'den Windows dosyalarına `/mnt/c/` üzerinden erişilir:

```bash
cd "/mnt/c/Users/Admin/Desktop/Ramal/Khazar OS-self-kernel-version"
```

## Adım 6: Build Testi

```bash
make clean
make all
```

Başarılı olursa `khazar_os.iso` dosyası oluşacak.

## Sorun Giderme

### "i686-linux-gnu-gcc: command not found"
- PATH'i kontrol et: `echo $PATH`
- `/usr/local/cross/bin` PATH'te olmalı

### "grub-mkrescue: command not found"
- `sudo apt install grub-pc-bin grub-common`

### Build hataları
- Tüm kaynak dosyaların Unix line endings (LF) kullandığından emin ol
- `dos2unix` ile dönüştür: `sudo apt install dos2unix && dos2unix *.asm *.c *.h`

## VirtualBox Yapılandırması

1. VirtualBox'ı aç
2. Yeni → Name: "Khazar OS"
3. Type: Other, Version: Other/Unknown (32-bit)
4. Memory: 128 MB
5. Hard disk: Yok
6. Settings → Storage → Controller: IDE → Add Optical Drive
7. `khazar_os.iso` seç
8. Start

## Tahmini Kurulum Süresi

- WSL2 kurulumu: 10-15 dakika
- Cross-compiler derleme: 30-60 dakika (Seçenek A)
- Hazır paket: 5 dakika (Seçenek B)
