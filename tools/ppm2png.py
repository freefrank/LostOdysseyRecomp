#!/usr/bin/env python3
"""Convert a binary PPM (P6) to PNG without third-party modules.
Usage: ppm2png.py <in.ppm> <out.png>"""
import struct, sys, zlib


def main(src, dst):
    data = open(src, "rb").read()
    parts = data.split(b"\n", 3)
    assert parts[0] == b"P6", "not a P6 ppm"
    w, h = map(int, parts[1].split())
    pixels = parts[3]
    raw = b"".join(b"\x00" + pixels[y * w * 3:(y + 1) * w * 3] for y in range(h))

    def chunk(tag, body):
        c = struct.pack(">I", len(body)) + tag + body
        return c + struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 6))
    png += chunk(b"IEND", b"")
    open(dst, "wb").write(png)
    nonzero = sum(1 for i in range(0, len(pixels), 3 * 97) if pixels[i:i + 3] != b"\x00\x00\x00")
    print(f"{w}x{h}, sampled non-black pixels: {nonzero}")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
