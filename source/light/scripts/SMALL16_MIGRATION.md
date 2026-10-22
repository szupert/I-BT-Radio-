# Preserving data when moving from `small` to `small16`

The old `small` partition table places SPIFFS at `0x3B0000`, size `0x50000`. The 16 MB `small16` table places it at `0xC10000`, size `0x3F0000`. Flashing the 16 MB app without first preserving the old filesystem would overwrite the old SPIFFS region. NVS remains at `0x9000`, but its contents should also be preserved in the full first-4-MiB readback.

When the ESP is available, first identify the chip and verify that it physically reports **16 MB flash**. Read back the first `0x400000` bytes into a local file and hash that file. Keep an untouched copy. A representative esptool command is:

```text
esptool --chip esp32 --port COMx read_flash 0x0 0x400000 old-first4m.bin
```

Prepare a new filesystem image offline:

```text
python scripts/prepare_small16_migration.py old-first4m.bin migration-output --mkspiffs PATH_TO_MKSPIFFS
```

The tool checks the exact old partition layout, unpacks the old SPIFFS, requires a nonempty playlist, repacks it to the new size, unpacks that image again, and compares every file by size and SHA-256. It writes `migration.json` only after all checks pass. A wrong layout, incomplete dump, failed unpack, failed image build, changed file, or nonempty output directory fails closed. The output includes the original raw SPIFFS image and unpacked files for recovery. Wi-Fi credentials can be present in these files; keep the folder private.

This is **preparation only**. There is no automatic flash step. A later hardware test package must verify the ESP readback, the 16 MB chip, the new image hash, and an explicit restore/readback sequence before declaring the migration passed. Do not use the synthetic host-test image on a device.
