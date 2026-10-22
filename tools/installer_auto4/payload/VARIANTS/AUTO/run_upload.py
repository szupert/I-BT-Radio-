from pathlib import Path
import argparse,hashlib,json,subprocess,sys,os,time
ROOT=Path(__file__).resolve().parent

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--verify-only',action='store_true');a=ap.parse_args()
    print('[CSOMAG] Ellenorzes...',flush=True)
    hashes=json.loads((ROOT/'HASHES.json').read_text(encoding='utf-8'))
    required={'run_upload.py','INSTALLER/update.py','INSTALLER/memory_probe.py','INSTALLER/requirements.txt'}
    if not required.issubset(hashes):raise RuntimeError('Hianyos csomag')
    for rel,v in hashes.items():
        p=(ROOT/rel).resolve()
        if ROOT not in p.parents or not p.is_file():raise RuntimeError('Hibas csomagutvonal')
        d=p.read_bytes()
        if len(d)!=v['bytes'] or hashlib.sha256(d).hexdigest()!=v['sha256']:raise RuntimeError('Csomaghiba: '+rel)
    cmd=[sys.executable,'-B','-X','utf8','-u',str(ROOT/'INSTALLER/update.py')]
    rc=subprocess.call(cmd+['--verify-only'])
    if rc or a.verify_only:return rc
    import msvcrt
    lockpath=Path('C:/esp32/USER/I-BT-Radio_LATEST/RETURN_TO_EVA/.upload_test.lock')
    lockpath.parent.mkdir(parents=True,exist_ok=True)
    with lockpath.open('a+b') as lock:
        lock.seek(0,2)
        if lock.tell()==0:lock.write(b'0');lock.flush()
        lock.seek(0)
        while True:
            try:msvcrt.locking(lock.fileno(),msvcrt.LK_NBLCK,1);break
            except OSError:
                print('[VARAKOZAS] Masik feltolto vagy teszt foglalja a radiot. Q: kilepes; Enter: uj ellenorzes.',flush=True)
                if input().strip().lower()=='q':return 130
                lock.seek(0)
        try:
            print('[AUTO] Felismeres, sebessegproba es telepites indul.',flush=True)
            # A locken kivuli, kezzel inditott regi soros teszt sem futhat parhuzamosan.
            script=r'''$ErrorActionPreference='Stop';$a=@(Get-CimInstance Win32_Process|Where-Object {$_.ProcessId -ne $PID -and $_.Name -match '^(python|pythonw|powershell|pwsh)\.exe$' -and $_.CommandLine -match '(?:^|[\\/\s])update\.py(?:"|\s|$)|r\d+_(?:nonstop[^\s]*|clock_diag|targeted_ab|full60|full4h|sd60)\.(?:ps1|py)|full_menu_stress|full_dequant_stress'});if($a.Count){exit 7}'''
            import base64
            code=subprocess.call(['powershell.exe','-NoProfile','-EncodedCommand',base64.b64encode(script.encode('utf-16le')).decode()])
            if code:raise RuntimeError('Masik soros folyamat fut, vagy nem ellenorizheto. Nincs uj feltoltes.')
            return subprocess.call(cmd+['--wait-for-device'])
        finally:lock.seek(0);msvcrt.locking(lock.fileno(),msvcrt.LK_UNLCK,1)

if __name__=='__main__':
    try:sys.exit(main())
    except KeyboardInterrupt:print('USER_STOP');sys.exit(130)
    except Exception as e:print('HIBA: '+str(e),flush=True);sys.exit(2)
