#!/bin/bash
# Creates a FAT12 disk image for use as a secondary drive in QEMU
set -e

IMG="fat12.img"
SIZE_KB=1440  # 1.44MB floppy

if [ -f "$IMG" ]; then
    echo "[FAT12] $IMG already exists, skipping (remove to rebuild)"
    exit 0
fi

echo "[FAT12] Creating ${SIZE_KB}KB FAT12 image: $IMG"
mkfs.fat -F 12 -C "$IMG" "$SIZE_KB"

echo "[FAT12] Adding files from fat12_files/..."
if [ -d fat12_files ]; then
    for f in fat12_files/*; do
        [ -f "$f" ] || continue
        echo "  Adding: $(basename $f)"
        mcopy -i "$IMG" "$f" "::$(basename $f)"
    done
fi

echo "[FAT12] Done: $IMG"
