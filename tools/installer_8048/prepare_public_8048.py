from pathlib import Path
import json,hashlib,zipfile,shutil,subprocess,sys
B=Path('C:/esp32/SYSTEM');P=B/'SOURCE/I-BT-Radio_MENU_8048S070C';R=B/'RELEASES/MENU8048C-R4';W=B/'TEMP/PUBLIC_8048_20261022';I=W/'payload/INSTALLER';I.mkdir(parents=True,exist_ok=True)
meta=json.loads((R/'RELEASE.json').read_text());archive_sha=hashlib.sha256((R/'source.zip').read_bytes()).hexdigest()
with zipfile.ZipFile(R/'source.zip') as z:
 for item in meta['files']:
  data=z.read(item['path']);assert len(data)==item['bytes'] and hashlib.sha256(data).hexdigest()==item['sha256'],item['path']
 print('SOURCE_CONTENT_MANIFEST_PASS; archive container hash:',archive_sha,'historical archive hash differs:',archive_sha!=meta['zip_sha256'])
for n,key in [('firmware.bin','bin_sha256'),('partitions.bin','partition_sha256')]:assert hashlib.sha256((R/n).read_bytes()).hexdigest()==meta[key]
for n in ['firmware.bin','partitions.bin','bootloader.bin']:shutil.copyfile(R/n,I/n)
shutil.copyfile(P/'tools/partitions_factory.bin',I/'partitions_factory.bin')
shutil.copyfile(B/'TOOLS/platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin',I/'boot_app0.bin')
s=(P/'tools/update.py').read_text(encoding='utf8');s='import os\n'+s
s=s.replace("Path('C:/esp32/SYSTEM/LOGS/UPLOAD')","(Path(os.environ.get('LOCALAPPDATA',str(Path.home())))/'I-BT-Radio'/'Logs')")
s=s.replace("ROOT.parent/'RETURN_TO_EVA'","(Path(os.environ.get('LOCALAPPDATA',str(Path.home())))/'I-BT-Radio'/'RETURN_TO_EVA')")
s=s.replace("[sys.executable,'-u','-X','utf8','-m','esptool'","[*esptool_prefix()")
s=s.replace('ROOT=Path(__file__).resolve().parent','ROOT=Path(__file__).resolve().parent\nWRITE_BAUD=115200\ndef esptool_prefix():\n    return [sys.executable,"--internal-esptool"] if getattr(sys,"frozen",False) else [sys.executable,"-u","-X","utf8","-m","esptool"]')
s=s.replace("str(1500000 if action=='write-flash' else 115200)","str(WRITE_BAUD if action=='write-flash' else 115200)")
s=s.replace('def main():\n','def main():\n    global WRITE_BAUD\n',1)
needle="        validate_flash_info(info)\n";assert needle in s
s=s.replace(needle,needle+'''        print('[SEBESSEG] Ketiranyu S3 kapcsolatproba; a particioolvasas115200 marad.',flush=True)
        probe=[sys.executable,'--internal-speed-probe','--port',port,'--output',str(out/'speed.json')] if getattr(sys,'frozen',False) else [sys.executable,str(ROOT/'speed_probe.py'),'--port',port,'--output',str(out/'speed.json')]
        with (out/'upload.log').open('a',encoding='utf8') as log:code,text=run_live(probe,log,120)
        if code:raise RuntimeError('A sebessegproba nem igazolt; nincs iras.')
        speed=json.loads((out/'speed.json').read_text());WRITE_BAUD=speed['write_baud']
        if WRITE_BAUD not in (1500000,921600,460800,115200) or not speed['baseline_verified']:raise RuntimeError('Hibas sebessegjelentes')
        result['transport']=speed
''')
s=s.replace("print('[4/5] Teljes MENU firmware: 1500000 baud, elo szazalek. SMALL-ra nem toltheto.',flush=True)","print(f'[4/5] Teljes MENU firmware: {WRITE_BAUD} baud, elo szazalek. SMALL-ra nem toltheto.',flush=True)")
assert '1500000 if action' not in s
(I/'update.py').write_text(s,encoding='utf8')
probe=(B/'SCRIPTS/../TEMP/PUBLIC_AUTO4_20261022/payload/VARIANTS/AUTO/INSTALLER/memory_probe.py').resolve().read_text()
choose=probe[probe.index('def choose_baud('):probe.index('\ndef choose_profile(')]
probe='''import argparse,json
from pathlib import Path
from esptool.targets.esp32s3 import ESP32S3ROM
W0=ESP32S3ROM.SPI_REG_BASE+ESP32S3ROM.SPI_W0_OFFS
BAUD_RATES=(1500000,921600,460800,115200)
'''+choose+'''
def main():
 p=argparse.ArgumentParser();p.add_argument('--port',required=True);p.add_argument('--output',required=True);a=p.parse_args()
 esp=ESP32S3ROM(a.port,115200)
 try:
  esp.connect('no_reset');result=choose_baud(esp);Path(a.output).write_text(json.dumps(result,indent=2),encoding='utf8')
 finally:esp._port.close()
if __name__=='__main__':main()
'''
(I/'speed_probe.py').write_text(probe,encoding='utf8')
lic=(P/'.pio/libdeps/menu8048c/LovyanGFX/license.txt').read_bytes();(W/'LICENSE_LovyanGFX.txt').write_bytes(lic)
shutil.copyfile(B/'TEMP/PUBLIC_AUTO4_20261022/INSTALLER_LICENSES.txt',W/'INSTALLER_LICENSES.txt')
package=dict(firmware='MENU8048C-R4',target='ESP32-8048S070C',offsets={'bootloader.bin':0,'partitions.bin':0x8000,'boot_app0.bin':0xe000,'firmware.bin':0x10000},files={p.name:dict(bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in I.iterdir() if p.is_file() and p.name!='PACKAGE.json'})
(I/'PACKAGE.json').write_text(json.dumps(package,indent=2),encoding='utf8')
entry='''import sys,os,ctypes,importlib.util,runpy
from pathlib import Path
def main():
 if '--internal-esptool' in sys.argv:
  sys.argv=[sys.argv[0]]+sys.argv[sys.argv.index('--internal-esptool')+1:];import esptool;esptool._main();return 0
 base=Path(getattr(sys,'_MEIPASS',Path(__file__).resolve().parent));root=base/'payload/INSTALLER'
 if '--internal-speed-probe' in sys.argv:
  args=sys.argv[sys.argv.index('--internal-speed-probe')+1:];sys.argv=[str(root/'speed_probe.py')]+args;runpy.run_path(str(root/'speed_probe.py'),run_name='__main__');return 0
 if '--extract-payload' in sys.argv:
  import shutil
  target=Path(sys.argv[sys.argv.index('--extract-payload')+1]);assert not target.exists();shutil.copytree(base/'payload',target);print('PAYLOAD_EXTRACTED_NO_COM');return 0
 spec=importlib.util.spec_from_file_location('up8048',root/'update.py');up=importlib.util.module_from_spec(spec);spec.loader.exec_module(up)
 if '--verify-only' in sys.argv:return up.main()
 kernel=ctypes.WinDLL('kernel32',use_last_error=True);kernel.CreateMutexW.argtypes=[ctypes.c_void_p,ctypes.c_bool,ctypes.c_wchar_p];kernel.CreateMutexW.restype=ctypes.c_void_p;kernel.CloseHandle.argtypes=[ctypes.c_void_p]
 lock=kernel.CreateMutexW(None,False,r'Local\\I_BT_Radio_Installer')
 if not lock or ctypes.get_last_error()==183:print('Mar fut egy I-BT-Radio telepito.');return 2
 try:
  up.verify();print('I-BT-Radio8048 BETA MENU8048C-R4 | S3/16MB/8MB |800x480 kapacitiv panel')
  print('Menu es felulet DEMO, radiohang/BT/Wi-Fi backend nincs. A paneltipusnak egyeznie kell.')
  if input('Enter: telepites; Q: kilepes: ').strip().lower()=='q':return 0
  sys.argv=[sys.argv[0]];return up.main()
 finally:kernel.CloseHandle(lock)
if __name__=='__main__':
 for stream in [sys.stdout,sys.stderr]:
  if hasattr(stream,'reconfigure'):stream.reconfigure(encoding='utf8',errors='replace')
 internal=any(x in sys.argv for x in ['--internal-esptool','--internal-speed-probe','--extract-payload']);verify='--verify-only' in sys.argv
 try:rc=main()
 except KeyboardInterrupt:print('USER_STOP');rc=130
 except Exception as e:print('HIBA:',str(e));rc=2
 if not internal and not verify:input('Enter: bezaras. ')
 sys.exit(rc)
'''
(W/'I-BT-Radio-8048.py').write_text(entry,encoding='utf8')
# Exact frozen R4 application source, with original release manifest and library licence.
S=B/'TEMP/PUBLIC_FULL_SOURCE_20261022/files/source/8048';S.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(R/'source.zip') as z:
 for n in z.namelist():
  dst=(S/n).resolve();assert dst.is_relative_to(S.resolve());dst.parent.mkdir(parents=True,exist_ok=True);dst.write_bytes(z.read(n))
shutil.copyfile(R/'RELEASE.json',S/'RELEASE.json');(S/'LICENSE_LovyanGFX.txt').write_bytes(lic)
# Build self-contained runtime. No hardware used.
import esptool
cmd=[sys.executable,'-m','PyInstaller','--noconfirm','--clean','--onefile','--console','--name','I-BT-Radio-8048-Install','--distpath',str(W/'dist'),'--workpath',str(W/'build'),'--specpath',str(W),'--paths',str(Path(esptool.__file__).resolve().parent.parent),'--add-data',str(W/'payload')+';payload','--collect-all','esptool','--collect-all','serial',str(W/'I-BT-Radio-8048.py')]
with (W/'build.log').open('w') as f:rc=subprocess.call(cmd,stdout=f,stderr=subprocess.STDOUT)
assert rc==0
exe=W/'dist/I-BT-Radio-8048-Install.exe'
for args in [['--verify-only'],['--internal-esptool','version'],['--internal-speed-probe','--help']]:
 p=subprocess.run([str(exe)]+args,capture_output=True,text=True,encoding='utf8',timeout=40);assert p.returncode==0,p.stdout+p.stderr
report=dict(result='PASS_FROZEN_HOST_NO_COM',version='MENU8048C-R4',exe=str(exe),bytes=exe.stat().st_size,sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),firmware_sha256=meta['bin_sha256'],source_zip_sha256=archive_sha,historical_zip_sha256=meta['zip_sha256'],source_content_manifest='PASS_ALL_31_FILES',hardware='ESP32-8048S070C ESP32-S3 16MBflash 8MBPSRAM 800x480 capacitive',radio_backend=False,physical_public_exe='NOT_RUN',COM=False,flash=False)
(B/'LOGS/REPORTS/PUBLIC_8048_20261022.json').write_text(json.dumps(report,indent=2),encoding='utf8');print('PASS_PUBLIC_8048_FROZEN_NO_COM',report['bytes'])
