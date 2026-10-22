"""Host regression for the offline small -> small16 SPIFFS migration."""

import argparse
from pathlib import Path
import struct
import tempfile

from prepare_small16_migration import (
    OLD_SPIFFS_OFFSET, OLD_SPIFFS_SIZE, NEW_SPIFFS_SIZE,
    prepare, run_mkspiffs,
)


def fixture(root, mkspiffs):
    contents = root / "contents"
    (contents / "data").mkdir(parents=True)
    (contents / "www").mkdir()
    (contents / "data" / "playlist.csv").write_bytes(b"Station\thttp://example.invalid/live\n")
    (contents / "www" / "index.html").write_bytes(b"<html>radio</html>")
    run_mkspiffs(mkspiffs, ["-c", "contents", "-b", "4096", "-p", "256", "-s", str(OLD_SPIFFS_SIZE), "old.bin"], root)
    raw = bytearray(b"\xff" * 0x400000)
    entries = [
        ("nvs", 1, 2, 0x9000, 0x5000),
        ("otadata", 1, 0, 0xE000, 0x2000),
        ("app0", 0, 16, 0x10000, 0x1D0000),
        ("app1", 0, 17, 0x1E0000, 0x1D0000),
        ("spiffs", 1, 130, OLD_SPIFFS_OFFSET, OLD_SPIFFS_SIZE),
    ]
    for i, (name, kind, subtype, offset, size) in enumerate(entries):
        struct.pack_into("<HBBII16sI", raw, 0x8000 + i * 32, 0x50AA, kind, subtype, offset, size, name.encode(), 0)
    raw[OLD_SPIFFS_OFFSET:OLD_SPIFFS_OFFSET + OLD_SPIFFS_SIZE] = (root / "old.bin").read_bytes()
    path = root / "dump.bin"
    path.write_bytes(raw)
    return path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--mkspiffs", required=True, type=Path)
    mkspiffs = parser.parse_args().mkspiffs.resolve()
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        dump = fixture(root, mkspiffs)
        manifest = prepare(dump, root / "prepared", mkspiffs)
        assert len(manifest["files"]) == 2
        assert (root / "prepared" / "new-spiffs.bin").stat().st_size == NEW_SPIFFS_SIZE
        try:
            prepare(dump, root / "prepared", mkspiffs)
        except ValueError:
            pass
        else:
            raise AssertionError("nonempty output directory accepted")
        raw = bytearray(dump.read_bytes())
        raw[0x8000 + 4 * 32 + 8] ^= 1
        damaged = root / "wrong-partition.bin"
        damaged.write_bytes(raw)
        try:
            prepare(damaged, root / "rejected", mkspiffs)
        except ValueError:
            pass
        else:
            raise AssertionError("wrong partition table accepted")
        (root / "contents" / "data" / "playlist.csv").unlink()
        run_mkspiffs(mkspiffs, ["-c", "contents", "-b", "4096", "-p", "256", "-s", str(OLD_SPIFFS_SIZE), "no-playlist.bin"], root)
        raw = bytearray(dump.read_bytes())
        raw[OLD_SPIFFS_OFFSET:OLD_SPIFFS_OFFSET + OLD_SPIFFS_SIZE] = (root / "no-playlist.bin").read_bytes()
        no_playlist = root / "no-playlist-dump.bin"
        no_playlist.write_bytes(raw)
        try:
            prepare(no_playlist, root / "rejected-no-playlist", mkspiffs)
        except ValueError:
            pass
        else:
            raise AssertionError("filesystem without playlist accepted")
    print("PASS: roundtrip, nonempty-output, wrong-layout, missing-playlist")


if __name__ == "__main__":
    main()
