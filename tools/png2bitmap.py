#!/usr/bin/env python3
"""Convert an image to a 1-bit C array for Max7219Matrix::drawBitmap().

    uv run --with pillow tools/png2bitmap.py logo.png --name kLogo > logo.h

Pixels brighter than --threshold (after alpha is flattened onto black) are
ON. Rows are padded to whole bytes, MSB = leftmost pixel.
"""
import argparse

from PIL import Image


def main():
    p = argparse.ArgumentParser()
    p.add_argument("image")
    p.add_argument("--name", default="kBitmap")
    p.add_argument("--threshold", type=int, default=128)
    p.add_argument("--invert", action="store_true")
    args = p.parse_args()

    img = Image.open(args.image).convert("RGBA")
    bg = Image.new("RGBA", img.size, (0, 0, 0, 255))
    gray = Image.alpha_composite(bg, img).convert("L")
    w, h = gray.size
    px = gray.load()

    stride = (w + 7) // 8
    data = []
    for y in range(h):
        for bx in range(stride):
            byte = 0
            for i in range(8):
                x = bx * 8 + i
                on = x < w and px[x, y] >= args.threshold
                if args.invert:
                    on = not on and x < w
                if on:
                    byte |= 0x80 >> i
            data.append(byte)

    print(f"constexpr uint16_t {args.name}W = {w};")
    print(f"constexpr uint16_t {args.name}H = {h};")
    print(f"const uint8_t {args.name}[] = {{")
    for y in range(h):
        row = data[y * stride:(y + 1) * stride]
        print("    " + ", ".join(f"0x{b:02X}" for b in row) + ",")
    print("};")


if __name__ == "__main__":
    main()
