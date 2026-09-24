"""Offline CPX prototype decoder for explicitly supplied small packages.

Independent Python port of the guest bit codec. Production resource indexing
uses cpx_decode.h; this module is for inspecting individual packages only.
"""

import argparse
from pathlib import Path
import struct


MAX_DECODED = 256 * 1024 * 1024


class Bits:
    def __init__(self, data, width):
        self.data, self.width, self.pos = data, width, 0
        self.left = width
        self.value = self.unit()

    def unit(self):
        n = self.width // 8
        # The guest prefetches after the last bit; only an unused prefetch may
        # read past the end. Actual consumed bits must come from input.
        if self.pos > len(self.data):
            raise ValueError("bitstream exhausted")
        available = min(n, len(self.data) - self.pos)
        self.valid = available * 8
        value = int.from_bytes(self.data[self.pos:self.pos+n].ljust(n, b"\0"), "big")
        self.pos += n
        return value

    def get(self, n):
        if not 0 <= n <= 16:
            raise ValueError("invalid bit width")
        result = 0
        while n:
            take = min(n, self.left)
            if self.width - self.left + take > self.valid:
                raise ValueError("bitstream exhausted")
            self.left -= take
            result = (result << take) | ((self.value >> self.left) & ((1 << take) - 1))
            n -= take
            if not self.left:
                self.value = self.unit()
                self.left = self.width
        return result

    def direct_word(self):
        if self.pos + 2 > len(self.data):
            raise ValueError("truncated direct offset")
        value = int.from_bytes(self.data[self.pos:self.pos+2], "big")
        self.pos += 2
        return value


def decode_block(block, width, output):
    if len(block) < 4:
        raise ValueError("truncated block header")
    a, b, lo, hi = block[:4]
    expected = (lo | hi << 8) + 1
    begin = len(output)
    if a == 255:
        if b != 0 or len(block) < 4 + expected:
            raise ValueError("invalid raw block")
        output.extend(block[4:4+expected])
        return expected
    length_mode, offset_mode = a >> 6, (a >> 4) & 3
    widths = [b & 15, b >> 4, 0]
    if offset_mode < 3:
        widths[offset_mode] = 16
    order = [[0, 1, 2], [1, 0, 2], [2, 0, 1], [2, 2, 2]][(a >> 2) & 3]
    bits = Bits(block[4:], width)
    while len(output) - begin < expected:
        if not bits.get(1):
            output.append(bits.get(8))
            continue
        if offset_mode == 0:
            offset = bits.direct_word() if width == 16 else bits.get(16)
        elif offset_mode == 1:
            offset = bits.get(widths[bits.get(1)])
        elif offset_mode == 2:
            choice = order[0] if not bits.get(1) else order[1+bits.get(1)]
            offset = bits.get(widths[choice])
        else:
            offset = 0
        size_bits = 9
        if length_mode == 1 and not bits.get(1):
            size_bits = 2
        elif length_mode == 2 and not bits.get(1):
            size_bits = 3
        length = bits.get(size_bits) + 3
        if offset + 1 > len(output) - begin or len(output) - begin + length > expected:
            raise ValueError("back-reference outside block")
        for _ in range(length):
            output.append(output[-offset-1])
    return expected


def decode(data):
    if len(data) < 20 or data[:3] != b"cpx":
        raise ValueError("not CPX")
    flags, stored, decoded = struct.unpack_from("<3I", data, 4)
    count = flags >> 16
    if (stored != len(data) or not count or count != (decoded + 65535) // 65536
            or decoded > MAX_DECODED or 16 + 4*count > len(data)):
        raise ValueError("invalid CPX sizes/block count")
    offsets = list(struct.unpack_from(f"<{count}I", data, 16)) + [stored]
    if offsets[0] != 16 + 4*count or any(a >= b for a, b in zip(offsets, offsets[1:])):
        raise ValueError("invalid CPX block table")
    width = 8 if data[4] & 0xf0 == 0x10 else 16
    output = bytearray()
    for i, (a, b) in enumerate(zip(offsets, offsets[1:])):
        size = decode_block(data[a:b], width, output)
        if size != min(65536, decoded - 65536*i):
            raise ValueError("block size does not match container")
    if len(output) != decoded:
        raise ValueError("wrong output size")
    return bytes(output)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path, help="One CPX package")
    parser.add_argument("--output", required=True, type=Path, help="New decoded file")
    args = parser.parse_args(argv)
    try:
        if args.output.exists():
            raise ValueError(f"output already exists: {args.output}")
        data = decode(args.input.read_bytes())
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(data)
        print(f"decoded {len(data)} bytes")
        return 0
    except (OSError, ValueError) as error:
        parser.exit(1, f"error: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
