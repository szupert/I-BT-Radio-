"""Prepare a verified SPIFFS image when moving the radio from small to small16.

Input is a read-only dump of the first 4 MiB of the existing flash. This tool
never connects to the ESP or writes flash. Use a mkspiffs executable matching
the ESP32 filesystem format (4096-byte blocks, 256-byte pages).
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys

OLD_SPIFFS_OFFSET = 0x3B0000
OLD_SPIFFS_SIZE = 0x50000
NEW_SPIFFS_SIZE = 0x3F0000
PARTITION_OFFSET = 0x8000
EXPECTED = {
    "nvs": (1, 2, 0x9000, 0x5000),
    "otadata": (1, 0, 0xE000, 0x2000),
    "app0": (0, 16, 0x10000, 0x1D0000),
    "app1": (0, 17, 0x1E0000, 0x1D0000),
    "spiffs": (1, 130, OLD_SPIFFS_OFFSET, OLD_SPIFFS_SIZE),
}


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def partitions(raw):
    found = {}
    for pos in range(PARTITION_OFFSET, PARTITION_OFFSET + 0x1000, 32):
        entry = raw[pos : pos + 32]
        if len(entry) < 32 or entry[:2] != b"\xaa\x50":
            break
        name = entry[12:28].split(b"\0", 1)[0].decode("ascii")
        offset, size = struct.unpack_from("<II", entry, 4)
        if name in found:
            raise ValueError(f"duplicate partition: {name}")
        found[name] = (entry[2], entry[3], offset, size)
    if found != EXPECTED:
        raise ValueError(f"unexpected old partition layout: {found}")


def files(root):
    result = {}
    for path in root.rglob("*"):
        if path.is_symlink():
            raise ValueError(f"symlink in extracted filesystem: {path}")
        if path.is_file():
            relative = path.relative_to(root).as_posix()
            data = path.read_bytes()
            result[relative] = {"bytes": len(data), "sha256": sha256(data)}
    if "data/playlist.csv" not in result or not result["data/playlist.csv"]["bytes"]:
        raise ValueError("old filesystem has no nonempty data/playlist.csv")
    return result


def run_mkspiffs(executable, args, cwd):
    completed = subprocess.run(
        [str(executable), *args], cwd=cwd, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, text=True, timeout=120, check=False,
    )
    if completed.returncode:
        raise RuntimeError(f"mkspiffs failed ({completed.returncode}): {completed.stdout[-2000:]}")


def prepare(dump_path, output_dir, mkspiffs):
    raw = dump_path.read_bytes()
    if len(raw) != 0x400000:
        raise ValueError(f"expected exactly 4 MiB readback, got {len(raw)} bytes")
    partitions(raw)
    if not mkspiffs.is_file():
        raise ValueError(f"mkspiffs not found: {mkspiffs}")
    if output_dir.exists() and any(output_dir.iterdir()):
        raise ValueError(f"output directory must be new or empty: {output_dir}")
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "unpacked").mkdir()
    (output_dir / "roundtrip").mkdir()
    old_image = raw[OLD_SPIFFS_OFFSET : OLD_SPIFFS_OFFSET + OLD_SPIFFS_SIZE]
    (output_dir / "old-spiffs.bin").write_bytes(old_image)
    run_mkspiffs(mkspiffs, ["-u", "unpacked", "-b", "4096", "-p", "256", "-s", str(OLD_SPIFFS_SIZE), "old-spiffs.bin"], output_dir)
    original_files = files(output_dir / "unpacked")
    run_mkspiffs(mkspiffs, ["-c", "unpacked", "-b", "4096", "-p", "256", "-s", str(NEW_SPIFFS_SIZE), "new-spiffs.bin"], output_dir)
    new_image = (output_dir / "new-spiffs.bin").read_bytes()
    if len(new_image) != NEW_SPIFFS_SIZE:
        raise ValueError("new SPIFFS image has the wrong size")
    run_mkspiffs(mkspiffs, ["-u", "roundtrip", "-b", "4096", "-p", "256", "-s", str(NEW_SPIFFS_SIZE), "new-spiffs.bin"], output_dir)
    if files(output_dir / "roundtrip") != original_files:
        raise ValueError("new SPIFFS image failed per-file roundtrip verification")
    manifest = {
        "status": "PASS_HOST_ONLY",
        "source_dump_sha256": sha256(raw),
        "old_spiffs_offset": OLD_SPIFFS_OFFSET,
        "old_spiffs_size": OLD_SPIFFS_SIZE,
        "old_spiffs_sha256": sha256(old_image),
        "new_spiffs_offset": 0xC10000,
        "new_spiffs_size": NEW_SPIFFS_SIZE,
        "new_spiffs_sha256": sha256(new_image),
        "files": original_files,
    }
    (output_dir / "migration.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dump", type=Path, help="exact 4 MiB readback from flash offset zero")
    parser.add_argument("output", type=Path, help="new or empty output directory")
    parser.add_argument("--mkspiffs", required=True, type=Path)
    args = parser.parse_args()
    try:
        result = prepare(args.dump.resolve(), args.output.resolve(), args.mkspiffs.resolve())
    except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as exc:
        parser.exit(1, f"FAIL: {exc}\n")
    print(f"PASS: {len(result['files'])} files; verified new image SHA-256 {result['new_spiffs_sha256']}")


if __name__ == "__main__":
    main()
