#!/bin/bash
# ─────────────────────────────────────────────────────────────────
#  Khazar OS — Quick Rebuild Script
#  Run inside WSL2 / Linux:  bash rebuild.sh
# ─────────────────────────────────────────────────────────────────

PROJ="/mnt/c/Users/Admin/Desktop/Ramal/Khazar OS-self-kernel-version"

cd "$PROJ" || { echo "✗ Project directory not found: $PROJ"; exit 1; }

echo ""
echo "┌─────────────────────────────────────────┐"
echo "│        Khazar OS — Build System         │"
echo "└─────────────────────────────────────────┘"
echo ""

# Temizlik
echo "[1/3] Cleaning build artefacts..."
make clean > /dev/null 2>&1

# Derleme
echo "[2/3] Compiling kernel + userland..."
make all 2>&1 | grep -E "(Compiling|Assembling|Linking|error:|warning:|Build)"

# Sonuç
echo ""
if [ -f "khazar_os.iso" ]; then
    ISO_SIZE=$(du -sh khazar_os.iso | cut -f1)
    KERN_SIZE=$(du -sh khazar_kernel.bin | cut -f1)
    echo "┌──────────────────────────────────────────┐"
    echo "│  ✓ BUILD SUCCESSFUL                      │"
    echo "│  ISO  : khazar_os.iso   ($ISO_SIZE)              │"
    echo "│  Kern : khazar_kernel.bin ($KERN_SIZE)          │"
    echo "│                                          │"
    echo "│  → Restart VirtualBox VM to test         │"
    echo "│  → Or: qemu-system-i386 -cdrom khazar_os.iso │"
    echo "└──────────────────────────────────────────┘"
else
    echo "┌──────────────────────────────────────────┐"
    echo "│  ✗ BUILD FAILED                          │"
    echo "│  Run: make all 2>&1 | less               │"
    echo "│  to see the full error log               │"
    echo "└──────────────────────────────────────────┘"
    exit 1
fi
