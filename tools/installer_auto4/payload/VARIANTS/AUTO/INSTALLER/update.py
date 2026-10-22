import os
"""Automatic flash/PSRAM profile admission before the existing checked updater."""
import argparse
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shutil
import sys
import uuid

ROOT = Path(__file__).resolve().parent
def probe_prefix():
    return [sys.executable,"--internal-memory-probe"] if getattr(sys,"frozen",False) else [sys.executable,"-B","-X","utf8","-u",str(ROOT/"memory_probe.py")]
MATRIX = ROOT.parent.parent
FOLDERS = {'lite4': '4MB_PSRAM_NELKUL', 'lite16': '16MB_PSRAM_NELKUL',
           'psram4': '4MB_PSRAM', 'psram16': '16MB_PSRAM'}


def load_update(profile):
    path = MATRIX / FOLDERS[profile] / 'INSTALLER/update.py'
    spec = importlib.util.spec_from_file_location('checked_' + profile, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def verify_profiles():
    modules = {}
    for profile, folder in FOLDERS.items():
        root = MATRIX / folder
        hashes = json.loads((root / 'HASHES.json').read_text(encoding='utf-8'))
        required = {'run_upload.py',  'INSTALLER/update.py',
                    'INSTALLER/PACKAGE.json', 'INSTALLER/GEOMETRY.json',
                    'INSTALLER/firmware.bin', 'INSTALLER/bootloader.bin',
                    'INSTALLER/partitions.bin', 'INSTALLER/spiffs.bin'}
        if not required.issubset(hashes):
            raise RuntimeError('Hianyos profil: ' + profile)
        for rel, expected in hashes.items():
            path = (root / rel).resolve()
            if root.resolve() not in path.parents:
                raise RuntimeError('Hibas profilutvonal')
            data = path.read_bytes()
            if len(data) != expected['bytes'] or hashlib.sha256(data).hexdigest() != expected['sha256']:
                raise RuntimeError('Profilhash elteres: ' + profile + '/' + rel)
        module = load_update(profile)
        manifest = module.verify()
        if (manifest['profile'] != profile or module._GEOMETRY['profile'] != profile or
                manifest['flash_mb'] != (4 if profile.endswith('4') else 16) or
                manifest['psram_required'] != profile.startswith('psram')):
            raise RuntimeError('Hibas memoria-profil terkep')
        modules[profile] = module
    return modules


def bind_identity(module, proof):
    original = module.select_variant
    def checked(info, manifest):
        macs = re.findall(r'\b(?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}\b', info)
        if proof['mac'].upper() not in [v.upper() for v in macs]:
            raise RuntimeError('A panel azonossaga nem igazolt a proba utan; nincs iras.')
        if module.chip_revision(info) != proof['revision']:
            raise RuntimeError('A chiprevizio megvaltozott a proba utan; nincs iras.')
        return original(info, manifest)
    module.select_variant = checked


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--verify-only', action='store_true')
    parser.add_argument('--port')
    parser.add_argument('--wait-for-device', action='store_true')
    a = parser.parse_args()
    print('[AUTO] Mind a negy firmware es feltolto ellenorzese...', flush=True)
    modules = verify_profiles()
    if a.verify_only:
        print('[AUTO] Negyprofilos csomag PASS; COM nem nyilt meg.', flush=True)
        return 0
    base = modules['lite4']
    port = base.detect_port(a.port, wait_seconds=None if a.wait_for_device else 60)
    out = (Path(os.environ.get('LOCALAPPDATA',str(Path.home())))/'I-BT-Radio'/'Logs') / (
        datetime.now(timezone.utc).strftime('AUTO_MEMORY_%Y%m%d_%H%M%S_') + uuid.uuid4().hex[:8])
    out.mkdir(parents=True)
    result = {'result': 'FAIL_PROBE', 'port': port, 'installer_started': False}
    try:
        with (out / 'probe.log').open('w', encoding='utf-8') as log:
            rc, _ = base.run_live([*probe_prefix(), '--port', port,
                                  '--output', str(out / 'MEMORY.json')], log, 180)
        if rc:
            raise RuntimeError('A memoriafelismeres nem fejezodott be. Nincs torles vagy feltoltes.')
        proof = json.loads((out / 'MEMORY.json').read_text(encoding='utf-8'))
        # Never reuse a previous machine's report; out is unique per invocation.
        from memory_probe import choose_profile
        profile = choose_profile(proof['flash_mb'], proof['psram']['status'])
        if (proof['schema'] != 1 or proof['chip'] != 'ESP32' or
                proof['profile'] != profile or not proof['registers_restored'] or proof['flash_written']):
            raise RuntimeError('Hibas felismeresi bizonyitek')
        selected = modules[profile]
        transport = proof['transport']
        baud = transport['write_baud']
        if (baud not in (1500000, 921600, 460800, 115200) or
                transport.get('preflight_baud') != 115200 or not transport.get('baseline_verified') or
                not any(x.get('baud') == baud and x.get('result') == 'PASS'
                        for x in transport.get('attempts', []))):
            raise RuntimeError('Nincs igazolt automatikus feltoltesi sebesseg.')
        bind_identity(selected, proof)
        result.update(profile=profile, detection=proof, write_baud=baud, result='FAIL_INSTALL')
        print('[AUTO] Kivalasztva: ' + FOLDERS[profile] + '. Kezi memoria-valasztas nem kell.', flush=True)
        receipt = out / 'INSTALL_RESULT.json'
        old_argv = sys.argv
        try:
            sys.argv = [str(selected.ROOT / 'update.py'), '--port', port,
                        '--handoff', str(receipt), '--write-baud', str(baud)]
            result['installer_started'] = True
            rc = selected.main()
        finally:
            sys.argv = old_argv
        if receipt.exists():
            result['installation'] = json.loads(receipt.read_text(encoding='utf-8'))
        result['result'] = 'PASS_FLASH_HASH_BOOT' if rc == 0 else 'FAIL_INSTALL'
        return rc
    except KeyboardInterrupt:
        result['result'] = 'USER_STOP'
        raise
    except Exception as e:
        result['error'] = str(e)
        print('HIBA: ' + str(e), flush=True)
        return 2
    finally:
        (out / 'RESULT.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
        back = (Path(os.environ.get('LOCALAPPDATA',str(Path.home())))/'I-BT-Radio'/'RETURN_TO_EVA') / out.name
        back.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(out, back, dirs_exist_ok=True)
        print('Felismeres es feltoltes naploja: ' + str(back), flush=True)


if __name__ == '__main__':
    sys.exit(main())
