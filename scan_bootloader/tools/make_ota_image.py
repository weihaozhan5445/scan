#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
make_ota_image.py - package an application .bin into the SCAN OTA image format.

Usage:
    python make_ota_image.py <app.bin> <out.bin> [major] [minor]

Output layout (little-endian, 16-byte header + payload):
    offset 0 : magic  "SCAN" (0x4E414353)
    offset 4 : ver_major
    offset 5 : ver_minor
    offset 6 : reserved (0)
    offset 8 : payload length (uint32)
    offset 12: CRC-32 of payload (uint32, zlib compatible)
    offset 16: payload (the app binary)

The resulting file is what the device downloads (via ESP8266) into the
W25Q32 staging slot, and what the bootloader flashes at 0x08004000.
"""
import struct
import sys
import zlib

MAGIC_STAGING = 0x4E414353  # "SCAN"
MAX_PAYLOAD = 0xC000         # 48 KB app region
HEADER = struct.Struct("<IBBHI")  # 2+2+2+4+4 = wait: <I B B H I I = 16 bytes

def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    src = sys.argv[1]
    dst = sys.argv[2]
    major = int(sys.argv[3]) if len(sys.argv) > 3 else 1
    minor = int(sys.argv[4]) if len(sys.argv) > 4 else 0

    with open(src, "rb") as f:
        payload = f.read()

    if len(payload) < 0x100:
        print("error: payload too small (%d bytes)" % len(payload))
        sys.exit(1)
    if len(payload) > MAX_PAYLOAD:
        print("error: payload %d bytes exceeds app region %d" % (len(payload), MAX_PAYLOAD))
        sys.exit(1)
    if len(payload) % 4 != 0:
        print("error: payload length must be a multiple of 4")
        sys.exit(1)

    crc = zlib.crc32(payload) & 0xFFFFFFFF
    header = struct.pack("<IBBHII", MAGIC_STAGING, major & 0xFF, minor & 0xFF,
                         0, len(payload), crc)
    assert len(header) == 16, len(header)

    with open(dst, "wb") as f:
        f.write(header)
        f.write(payload)

    print("OTA image written: %s" % dst)
    print("  payload : %d bytes (0x%X)" % (len(payload), len(payload)))
    print("  version : %d.%d" % (major, minor))
    print("  crc32   : 0x%08X" % crc)

if __name__ == "__main__":
    main()
