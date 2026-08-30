#!/usr/bin/env python3
"""Convert a binary Core1 image into a C byte array for the Amiga launcher."""

import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 4:
        print(f"Usage: {Path(sys.argv[0]).name} input.bin output.c symbol", file=sys.stderr)
        return 2

    input_path = Path(sys.argv[1])
    output_path = Path(sys.argv[2])
    symbol = sys.argv[3]
    data = input_path.read_bytes()

    with output_path.open("w", encoding="ascii", newline="\n") as f:
        f.write("/* Generated from the ARM Core1 binary. */\n")
        f.write(f"const unsigned char {symbol}[] = {{\n")
        for pos in range(0, len(data), 12):
            chunk = data[pos:pos + 12]
            f.write("    " + ", ".join(f"0x{b:02x}" for b in chunk) + ",\n")
        f.write("};\n")
        f.write(f"const unsigned int {symbol}_size = sizeof({symbol});\n")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
