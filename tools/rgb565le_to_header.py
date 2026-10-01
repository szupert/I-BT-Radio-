"""Convert a headerless RGB565LE skin image into an Arduino C array."""
from pathlib import Path
import argparse
import struct

parser = argparse.ArgumentParser()
parser.add_argument("input", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("symbol")
parser.add_argument("width", type=int)
parser.add_argument("height", type=int)
args = parser.parse_args()
data = args.input.read_bytes()
assert len(data) == args.width * args.height * 2, "Unexpected image size"
values = struct.unpack("<" + "H" * (args.width * args.height), data)
lines = ["#pragma once", "#include <stdint.h>",
         f"// {args.width}x{args.height}, row-major RGB565; {len(data)} bytes in flash.",
         f"static const uint16_t {args.symbol}[{len(values)}] = {{"]
for offset in range(0, len(values), 24):
    lines.append(",".join(f"0x{value:04x}" for value in values[offset:offset + 24]) + ",")
lines.append("};")
args.output.write_text("\n".join(lines) + "\n", encoding="ascii")
print(args.output, len(values), "pixels")
