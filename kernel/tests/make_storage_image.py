#!/usr/bin/env python3
import struct
import sys

SECTOR = 512
SECTORS = 65536
PARTITION_START = 2048
PARTITION_SECTORS = 32768
READ_MARKER = b"JOSHOS-STORAGE-READ-OK"

def main() -> int:
    if len(sys.argv) != 2:
        print("usage: make_storage_image.py OUTPUT", file=sys.stderr)
        return 2

    image = bytearray(SECTOR * SECTORS)
    entry = 446
    image[entry] = 0x80
    image[entry + 4] = 0x83
    struct.pack_into("<I", image, entry + 8, PARTITION_START)
    struct.pack_into("<I", image, entry + 12, PARTITION_SECTORS)
    image[510:512] = b"\x55\xaa"

    start = PARTITION_START * SECTOR
    image[start:start + len(READ_MARKER)] = READ_MARKER

    with open(sys.argv[1], "wb") as handle:
        handle.write(image)
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
