#!/bin/bash
# Khazar OS Build Script for WSL

echo "========================================="
echo "  Khazar OS Build Script"
echo "========================================="

# Proje dizinine git
cd "$(dirname "$0")"
# Temizlik
echo "[1/3] Cleaning previous build..."
make clean

# Build
echo "[2/3] Building Khazar OS..."
make all

# Sonuç kontrolü
if [ -f "khazar_os.iso" ]; then
    echo "[3/3] Build SUCCESS! ✓"
    echo ""
    echo "ISO created: khazar_os.iso"
    echo "Size: $(du -h khazar_os.iso | cut -f1)"
    echo ""
    echo "Next steps:"
    echo "1. Open VirtualBox"
    echo "2. Load khazar_os.iso as CD/DVD"
    echo "3. Start the VM"
else
    echo "[3/3] Build FAILED! ✗"
    exit 1
fi
