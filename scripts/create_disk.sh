#!/bin/bash
# Khazar OS - Disk Image Creator Script (V2)
# Bu script WSL2 üzerinde çalıştırılmalıdır.

DISK_NAME="khazar_hd.img"
VDI_NAME="khazar_hd.vdi"
TEST_FILE="TEST.EXE"
VBOX_PATH="/mnt/c/Program Files/Oracle/VirtualBox/VBoxManage.exe"

echo "=== Khazar OS Disk Creator V2 ==="

# 1. Gerekli araçları kontrol et
if ! command -v mkfs.vfat &> /dev/null; then
    echo "Hata: 'dosfstools' yüklü değil."
    exit 1
fi

if ! command -v mcopy &> /dev/null; then
    echo "Hata: 'mtools' yüklü değil."
    exit 1
fi

# 2. 64MB boş bir disk imajı oluştur
echo "Creating 64MB disk image..."
dd if=/dev/zero of=$DISK_NAME bs=1M count=64

# 3. FAT32 olarak formatla (Partisyon tablosu olmadan - Superfloppy)
# Not: Bizim kernel şimdilik direkt sektör 0'ı okuduğu için superfloppy daha kolay
echo "Formatting as FAT32 (Superfloppy)..."
mkfs.vfat -F 32 $DISK_NAME

# 4. Geçerli bir TEST.EXE oluştur (Python yardımıyla)
echo "Generating valid PE executable (TEST.EXE)..."
python3 scripts/gen_pe.py

# 5. Paket Reposunu Hazırla
echo "Preparing package repository..."
mmd -i $DISK_NAME ::/repo || true
python3 scripts/gen_kzp.py test 1.0.0 test.kzp test.txt:/system/test.txt
mcopy -i $DISK_NAME test.kzp ::/repo/

# 6. Klasör yapılarını oluştur ve kopyala
echo "Creating Userland Directory Structure..."
mmd -i $DISK_NAME ::/bin || true
mmd -i $DISK_NAME ::/etc || true
mmd -i $DISK_NAME ::/usr || true
mmd -i $DISK_NAME ::/var || true

echo "Copying TEST.EXE to disk..."
mcopy -i $DISK_NAME $TEST_FILE ::/bin/

echo "Copying Userland Apps to disk (bin)..."
mcopy -i $DISK_NAME iso/bin/*.exe ::/bin/

# 6. Doğrulama
echo "Verifying disk content with mdir:"
mdir -i $DISK_NAME ::/

# 7. VirtualBox için VDI formatına çevir
if [ -f "$VBOX_PATH" ]; then
    echo "Converting to VDI format..."
    rm -f $VDI_NAME
    "$VBOX_PATH" convertfromraw $DISK_NAME $VDI_NAME --format VDI
    
    # UUID Mismatch hatasını önlemek için UUID'yi sabitle
    echo "Setting fixed VDI UUID to avoid registry conflicts..."
    "$VBOX_PATH" internalcommands sethduuid $VDI_NAME 76cbd317-33dd-4054-b924-caaa75ee993b
    
    echo "=== Başarılı! ==="
    echo "VirtualBox'ta '$VDI_NAME' dosyasını kullanın."
else
    echo "VBoxManage.exe bulunamadı, sadece .img güncellendi."
fi
