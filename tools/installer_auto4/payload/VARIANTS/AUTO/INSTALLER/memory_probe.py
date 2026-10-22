"""SMALL board memory admission, ESP32 ROM only; never writes flash.

Fixed project wiring: flash default pins 6..11, PSRAM CLK6/CS16.
SPI1 register layout and F5/9F/03/02 protocol follow Espressif IDF
components/esp_psram/esp32/esp_psram_impl_quad.c and soc/spi_reg.h.
This slow SPI sample is NOT the firmware's native 40 MHz quad RAM test.
"""
import argparse
import json
import time
from pathlib import Path

SPI = 0x3FF42000
CMD, ADDR, CTRL, CTRL2, CLOCK, USER, USER1, USER2, MOSI, MISO, PIN = [
    SPI + x for x in (0, 4, 8, 0x14, 0x18, 0x1C, 0x20, 0x24, 0x28, 0x2C, 0x34)]
W0 = SPI + 0x80
ENABLE, PIN16, MATRIX16, MUX16 = 0x3FF44020, 0x3FF440C8, 0x3FF44570, 0x3FF4904C
MUX = [0x3FF49000 + x for x in (0x60, 0x64, 0x68, 0x54, 0x58, 0x5C)]
SAVED = [ADDR, CTRL, CTRL2, CLOCK, USER, USER1, USER2, MOSI, MISO,
         *range(W0, W0 + 64, 4), *MUX, MUX16, PIN16, MATRIX16, ENABLE, PIN]
BLOCKS = (0, 0x40, 0x140, 0x1000, 0x10000, 0x100000, 0x200000, 0x3FFFC0)
BAUD_RATES = (1500000, 921600, 460800, 115200)


def choose_baud(esp):
    """Bidirectional UART sample in idle SPI FIFO, with no SPI/flash command.

    All preflight commands after this function use a VERIFIED 115200 connection.
    A failed high-baud trial is never followed by a blind low-baud flash command.
    """
    saved = [esp.read_reg(W0 + i * 4) for i in range(16)]
    attempts = []
    selected = None
    baseline_ok = True

    def baseline(candidate):
        errors = []
        # A failed CHANGE_BAUD acknowledgement can leave either baud active.
        for current in dict.fromkeys((candidate, 115200)):
            try:
                esp._set_port_baudrate(current)
                esp.flush_input()
                if current != 115200:
                    esp.change_baud(115200)
                esp._set_port_baudrate(115200)
                esp.flush_input()
                # Actual ROM reply, not just successful host UART reconfiguration.
                esp.read_reg(W0, timeout=1)
                return
            except Exception as exc:
                errors.append(str(exc))
        raise RuntimeError('A 115200-as kapcsolat nem igazolhato; nincs feltoltes. ' +
                           '; '.join(errors))

    try:
        for baud in BAUD_RATES:
            print('[SEBESSEG] Automatikus kapcsolatproba: %d baud...' % baud, flush=True)
            passed = False
            error = None
            try:
                baseline_ok = False
                if baud != 115200:
                    esp.change_baud(baud)
                for phase in range(3):
                    values = [((0xC0DB5A81 ^ (i * 0x1020304)) + phase * 0x11111111) & 0xFFFFFFFF
                              for i in range(16)]
                    for i, value in enumerate(values):
                        esp.write_reg(W0 + i * 4, value)
                    got = [esp.read_reg(W0 + i * 4, timeout=1) for i in range(16)]
                    if got != values:
                        raise RuntimeError('Soros adat-visszaolvasasi elteres')
                passed = True
            except Exception as exc:
                error = str(exc)
            finally:
                baseline(baud)
                baseline_ok = True
            attempts.append({'baud': baud, 'result': 'PASS' if passed else 'FAIL', 'error': error})
            if passed:
                selected = baud
                break
    finally:
        # baseline() must have succeeded; these operations also fail closed if not.
        if baseline_ok:
            for i, value in enumerate(saved):
                esp.write_reg(W0 + i * 4, value)
            if [esp.read_reg(W0 + i * 4) for i in range(16)] != saved:
                raise RuntimeError('A sebessegproba lezarasa hibas; nincs feltoltes.')
    if selected is None:
        raise RuntimeError('Egyetlen feltoltesi sebesseg sem ellenorizheto.')
    print('[SEBESSEG] Kivalasztva: %d baud. Eloellenorzes tovabbra is 115200.' % selected, flush=True)
    return {'write_baud': selected, 'preflight_baud': 115200,
            'baseline_verified': True, 'attempts': attempts,
            'scope': 'bidirectional UART register sample; not bulk flash stability proof'}


def choose_profile(flash_mb, ram_status):
    if flash_mb not in (4, 16):
        raise RuntimeError('Nem tamogatott flashmeret; nincs iras.')
    if ram_status not in ('SAMPLE_PASS', 'NO_RESPONSE', 'INVALID_ID', 'UNSTABLE_ID',
                          'SAMPLE_FAIL', 'UNSUPPORTED_CAPACITY'):
        raise RuntimeError('Bizonytalan PSRAM-vizsgalat; nincs iras.')
    return ('psram' if ram_status == 'SAMPLE_PASS' else 'lite') + str(flash_mb)


class SmallPSRAM:
    def __init__(self, esp):
        self.esp = esp

    def wait(self):
        for _ in range(20):
            if not self.esp.read_reg(CMD) & (1 << 18):
                return
        raise RuntimeError('PSRAM SPI idotullepes; nincs automatikus firmware-valasztas.')

    def transaction(self, command=None, address=None, data=b'', read=0, quad=False):
        if command not in (None, 0x9F, 0x03, 0x02) or len(data) > 64 or not 0 <= read <= 64:
            raise RuntimeError('Ervenytelen PSRAM parancs')
        if command is None and not (quad and data == b'\xf5' and read == 0 and address is None):
            raise RuntimeError('Csak QPI-exit lehet parancs nelkul')
        if address is not None and not 0 <= address < 0x800000:
            raise RuntimeError('Ervenytelen PSRAM cim')
        self.wait()
        # Flash CS0 and unused CS2 MUST be disabled throughout every RAM command.
        if self.esp.read_reg(PIN) & 7 != 5:
            raise RuntimeError('PSRAM CS vedelmi hiba; nincs iras.')
        flags = (1 << 5) | (1 << 4)
        if command is not None:
            flags |= 1 << 31
        if address is not None:
            flags |= 1 << 30
        if data:
            flags |= 1 << 27
        if read:
            flags |= 1 << 28
        if quad:
            flags |= 1 << 15
        self.esp.write_reg(USER, flags)
        self.esp.write_reg(USER1, 23 << 26 if address is not None else 0)
        self.esp.write_reg(USER2, (7 << 28) | command if command is not None else 0)
        self.esp.write_reg(ADDR, address << 8 if address is not None else 0)
        self.esp.write_reg(MOSI, max(0, len(data) * 8 - 1))
        self.esp.write_reg(MISO, max(0, read * 8 - 1))
        payload = data.ljust(64, b'\0')
        for i in range(0, max(4, len(data), read), 4):
            self.esp.write_reg(W0 + i, int.from_bytes(payload[i:i+4], 'little'))
        self.esp.write_reg(CMD, 1 << 18)
        self.wait()
        return b''.join(self.esp.read_reg(W0+i).to_bytes(4, 'little')
                        for i in range(0, read, 4))[:read]

    def run(self):
        self.wait()
        saved = {r: self.esp.read_reg(r) for r in SAVED}
        try:
            # Disable all selects BEFORE changing any pin function.
            self.esp.write_reg(PIN, (saved[PIN] & ~(1 << 29)) | 7)
            # Standard flash IOMUX functions. No probing of arbitrary board pins.
            for r in MUX:
                self.esp.write_reg(r, (saved[r] & ~(7 << 12)) | (1 << 12) | (1 << 9))
            self.esp.write_reg(MUX16, (saved[MUX16] & ~(7 << 12)) | (2 << 12))
            self.esp.write_reg(PIN16, saved[PIN16] & ~(1 << 2))  # push-pull
            self.esp.write_reg(MATRIX16, 6)  # SPICS1_OUT_IDX, peripheral output enable
            self.esp.write_reg(ENABLE, saved[ENABLE] | (1 << 16))
            self.esp.write_reg(CTRL, 0)  # MSB wire order, plain SPI
            self.esp.write_reg(CTRL2, 0x11)  # CS setup/hold
            self.esp.write_reg(CLOCK, (39 << 12) | (19 << 6) | 39)  # APB / 40
            self.esp.write_reg(PIN, (saved[PIN] & ~((1 << 29) | 7)) | 5)
            self.transaction(data=b'\xf5', quad=True)
            ids = [self.transaction(0x9F, address=0, read=8) for _ in range(4)]
            report = {'id_reads': [v.hex() for v in ids], 'clock_divisor': 40,
                      'pins': {'clock': 6, 'cs': 16}, 'native_40mhz_quad_test': 'NOT_RUN'}
            if len(set(ids)) != 1:
                return dict(report, status='UNSTABLE_ID')
            if ids[0] in (b'\0' * 8, b'\xff' * 8):
                return dict(report, status='NO_RESPONSE')
            if ids[0][1] != 0x5D:
                return dict(report, status='INVALID_ID')
            eid = ids[0][2]
            capacity = 8 if eid == 0x26 else {0: 2, 1: 4, 2: 8}.get(eid >> 5)
            report['advertised_mb'] = capacity
            if capacity not in (4, 8):
                return dict(report, status='UNSUPPORTED_CAPACITY')
            errors = []
            tested = 0
            # Write all blocks before reading them: catches sampled address aliases too.
            for phase in range(4):
                print('[PSRAM] Iras-olvasas minta %d/4...' % (phase + 1), flush=True)
                expected = {}
                for address in BLOCKS:
                    words = []
                    for offset in range(0, 64, 4):
                        x = ((address + offset) ^ 0xA5C35A3C) & 0xFFFFFFFF
                        words.append((0, 0xFFFFFFFF, x, x ^ 0xFFFFFFFF)[phase])
                    expected[address] = b''.join(x.to_bytes(4, 'little') for x in words)
                    self.transaction(0x02, address, data=expected[address])
                for address in BLOCKS:
                    got = self.transaction(0x03, address, read=64)
                    tested += 64
                    if got != expected[address]:
                        errors.append({'phase': phase, 'address': hex(address),
                                       'expected': expected[address].hex(), 'got': got.hex()})
            return dict(report, status='SAMPLE_FAIL' if errors else 'SAMPLE_PASS',
                        tested_bytes=tested, sampled_blocks=len(BLOCKS), errors=errors)
        finally:
            # Attempt EVERY restoration even if one UART write fails. Fail closed.
            failures = []
            try:
                self.esp.write_reg(PIN, saved[PIN] | 7)
            except Exception as e:
                failures.append(str(e))
            for r in SAVED:
                try:
                    self.esp.write_reg(r, saved[r])
                except Exception as e:
                    failures.append(str(e))
            for r in SAVED:
                try:
                    if self.esp.read_reg(r) != saved[r]:
                        failures.append('register ' + hex(r))
                except Exception as e:
                    failures.append(str(e))
            if failures:
                raise RuntimeError('PSRAM-proba utan regiszter-helyreallitas nem igazolt; nincs iras. ' +
                                   '; '.join(failures[:3]))


def inspect(esp, emit=print):
    if esp.CHIP_NAME != 'ESP32' or getattr(esp, 'IS_STUB', False):
        raise RuntimeError('Csak ESP32 ROM tamogatott.')
    major, minor = esp.get_major_chip_version(), esp.get_minor_chip_version()
    if not 100 <= major * 100 + minor <= 399:
        raise RuntimeError('Nem tamogatott chiprevizio.')
    if esp.get_pkg_version() not in (0, 1, 3, 7) or esp.read_efuse(3) & 3:
        raise RuntimeError('Nem tamogatott SMALL chip/pinout vagy nincs ket mag/BT.')
    if any(esp.get_chip_spi_pads()):
        raise RuntimeError('Egyedi flash labkiosztas; nem futtathato CLK6/CS16 proba.')
    if esp.secure_download_mode or esp.get_secure_boot_enabled() or esp.get_flash_encryption_enabled():
        raise RuntimeError('Vedett eszkoz; nincs automatikus memoria-proba vagy iras.')
    esp.flash_spi_attach(0)
    fid = esp.flash_id(cache=False)
    if fid != esp.flash_id(cache=False) or fid in (0, 0xFFFFFF):
        raise RuntimeError('Nem stabil flashazonosito.')
    mb = {0x16: 4, 0x18: 16}.get((fid >> 16) & 255)
    if mb is None:
        raise RuntimeError('Csak 4 vagy 16 MB flash tamogatott.')
    emit('[FLASH] Felismert meret: %d MB; PSRAM-proba indul.' % mb, flush=True)
    ram = SmallPSRAM(esp).run()
    if esp.flash_id(cache=False) != fid:
        raise RuntimeError('A flash visszaellenorzese hibas; nincs feltoltes.')
    return {'schema': 1, 'chip': 'ESP32', 'revision': major * 100 + minor,
            'mac': ':'.join('%02X' % b for b in esp.read_mac()),
            'flash_id': hex(fid), 'flash_mb': mb, 'psram': ram,
            'profile': choose_profile(mb, ram['status']), 'flash_written': False,
            'registers_restored': True}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', required=True)
    parser.add_argument('--output', required=True)
    a = parser.parse_args()
    from esptool.targets.esp32 import ESP32ROM
    from esptool import __version__
    if __version__ != '5.3.0':
        raise RuntimeError('Az ellenorzott esptool 5.3.0 szukseges.')
    print('[PSRAM] SMALL CLK6/CS16 vizsgalat ROM-ban, flashiras nelkul.', flush=True)
    print('[COM] BOOT nyomva + rovid EN/RESET, majd BOOT elenged, ha meg nincs letoltesi modban.', flush=True)
    with ESP32ROM(a.port, 115200) as esp:
        esp.connect(mode='no-reset', attempts=3)
        result = inspect(esp)
        result['transport'] = choose_baud(esp)
    Path(a.output).write_text(json.dumps(result, indent=2), encoding='utf-8')
    print('[AUTO] %d MB flash, PSRAM: %s -> %s' %
          (result['flash_mb'], result['psram']['status'], result['profile']), flush=True)
    if result['psram']['status'] != 'SAMPLE_PASS':
        print('[PSRAM] Nem igazolt hasznalhato memoria ezen a bekotesen. PSRAM nelkuli profil.', flush=True)
    else:
        print('[PSRAM] Rovid SPI mintateszt sikeres; a firmware teljes nativ indulasi memtesztje meg kotelezo.', flush=True)


if __name__ == '__main__':
    main()
