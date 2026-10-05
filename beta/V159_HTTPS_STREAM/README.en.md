# I-BT-Radio V159 — BETA test

**Limited-functionality beta. Testing only; not recommended for integration, daily use, or unattended playback.** Report issues via GitHub Issues. Before uploading logs, remove SSIDs, passwords, IP and MAC addresses, and other personal data.

## Downloads

- [One-click installer ZIP](I-BT-Radio_V159_HTTPS_STREAM_BETA.zip)
- [Installer SHA-256](I-BT-Radio_V159_HTTPS_STREAM_BETA.zip.sha256)
- [Source materials, GPL-3.0 and V159 patch](V159_SOURCE_GPL3.zip)
- [Source ZIP SHA-256](V159_SOURCE_GPL3.zip.sha256)
- [Magyar tesztleírás](README.md)

The installer writes the application to the existing 16 MB layout. The uploader requires Python 3.9+. The menu self-test uses PowerShell and does not flash firmware.

## V159 change

Restores direct HTTPS streaming. Direct HTTP remains enabled; HLS remains disabled. TLS encrypts traffic, but certificate validation is disabled, so the connection is not authenticated.

Firmware SHA-256: `4a7f09075de23e1508daedae1cd6004ad861a3ff509fa9f9cb81232afc4b3bb0`
ELF SHA-256: `aa7d4220335b4340bc6328153f9d45dbb4d6fb032bc23d5961a765e21866b4aa`
Installer ZIP SHA-256: `a93883e1d02f7ef0cee246edfce4eacef3051487a1180815c203a42987b111f0`

## Current test status

In the user's test, V159 uploaded, booted, and radio audio played through the DAC. The menu test immediately returned `start=DENIED` with **zero menu steps**; it did not actually wait, and does not prove that menu navigation works. HTTPS streaming, Bluetooth continuity, and long-term stability are unverified. DAC audio alone is not a full test PASS.

The source archive contains the V157 source base, GPL-3.0 license and V159 HTTPS patch. It has not been rebuilt independently; we do not claim bit-for-bit firmware reproducibility.
