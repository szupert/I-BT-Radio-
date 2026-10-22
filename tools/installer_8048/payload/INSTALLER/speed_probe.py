import argparse,json
from pathlib import Path
from esptool.targets.esp32s3 import ESP32S3ROM
W0=ESP32S3ROM.SPI_REG_BASE+ESP32S3ROM.SPI_W0_OFFS
BAUD_RATES=(1500000,921600,460800,115200)
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


def main():
 p=argparse.ArgumentParser();p.add_argument('--port',required=True);p.add_argument('--output',required=True);a=p.parse_args()
 esp=ESP32S3ROM(a.port,115200)
 try:
  esp.connect('no_reset');result=choose_baud(esp);Path(a.output).write_text(json.dumps(result,indent=2),encoding='utf8')
 finally:esp._port.close()
if __name__=='__main__':main()
