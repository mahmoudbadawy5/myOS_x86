#!/usr/bin/env python3
"""Wrap a raw VGA 8x16 binary font into PSF1 format."""
import sys, os, urllib.request

FONT_URL = "https://raw.githubusercontent.com/viler-int10h/vga-text-mode-fonts/master/FONTS/PC-IBM/VGA8.F16"

def main():
    outpath = sys.argv[1] if len(sys.argv) > 1 else "vga.psf"
    raw_path = outpath + ".raw"

    # Download raw font if not cached
    if not os.path.exists(raw_path):
        print(f"Downloading VGA 8x16 font from {FONT_URL}...")
        urllib.request.urlretrieve(FONT_URL, raw_path)
    else:
        print(f"Using cached font: {raw_path}")

    with open(raw_path, "rb") as f:
        raw = f.read()

    if len(raw) != 4096:
        print(f"Warning: expected 4096 bytes, got {len(raw)}. Padding/truncating.")
        raw = raw[:4096].ljust(4096, b'\x00')

    # PSF1 header: magic=0x36, mode=0x04 (8x default), height=16
    header = bytes([0x36, 0x04, 16])

    with open(outpath, "wb") as f:
        f.write(header)
        f.write(raw)

    print(f"Generated PSF1 font: {outpath} ({len(header) + len(raw)} bytes)")

if __name__ == "__main__":
    main()
