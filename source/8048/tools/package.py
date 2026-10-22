from pathlib import Path
import hashlib,json,shutil,sys,zipfile
P=Path(__file__).resolve().parents[1]
B=Path('C:/esp32/SYSTEM/BUILD_OUTPUT/MENU_8048S070C/menu8048c')
U=Path(sys.argv[1]) if len(sys.argv)>1 else Path('C:/esp32/USER/I-BT-Radio_MENU_8048S070C')
I=U/'INSTALLER';I.mkdir(parents=True,exist_ok=True)
for name in ['preview.png','LICENSE_Arduino_GFX.txt']:
    f=U/name
    if f.exists():
        assert f.resolve().is_relative_to(U.resolve());f.unlink()
for name in ['firmware.bin','bootloader.bin','partitions.bin']:shutil.copy2(B/name,I/name)
shutil.copy2('C:/esp32/SYSTEM/TOOLS/platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin',I/'boot_app0.bin')
for name in ['update.py','menu_test.py','requirements.txt','START_INSTALLER.bat','START_SELFTEST.bat','partitions_factory.bin']:shutil.copy2(P/'tools'/name,I/name)
shutil.copy2(P/'.pio/libdeps/menu8048c/LovyanGFX/license.txt',U/'LICENSE_LovyanGFX.txt')
shutil.copy2(P/'README.md',U/'README.md')
shutil.copy2(P/'skin_audit.json',U/'skin_audit.json')
(U/'1_FELTOLTES.bat').write_text('@echo off\r\ncall "%~dp0INSTALLER\\START_INSTALLER.bat" %*\r\n',encoding='utf-8')
(U/'2_MENU_SELFTEST.bat').write_text('@echo off\r\ncall "%~dp0INSTALLER\\START_SELFTEST.bat" %*\r\n',encoding='utf-8')
files={}
for f in sorted(U.rglob('*')):
    if f.is_file() and 'RETURN_TO_EVA' not in f.parts and '__pycache__' not in f.parts and f.name!='PACKAGE.json':
        rel=f.relative_to(I).as_posix() if f.is_relative_to(I) else '../'+f.relative_to(U).as_posix()
        data=f.read_bytes();files[rel]={'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}
m={'firmware':'MENU8048C-R4','target':'ESP32-8048S070C','offsets':{'bootloader.bin':0,'partitions.bin':0x8000,'boot_app0.bin':0xe000,'firmware.bin':0x10000},'files':files}
(I/'PACKAGE.json').write_text(json.dumps(m,indent=2),encoding='utf-8')
z=U.parent/(U.name+'.zip')
with zipfile.ZipFile(z,'w',zipfile.ZIP_DEFLATED) as archive:
    for f in sorted(U.rglob('*')):
        if f.is_file() and 'RETURN_TO_EVA' not in f.parts and '__pycache__' not in f.parts:archive.write(f,f.relative_to(U.parent))
print(U);print(z);print('ZIP SHA256',hashlib.sha256(z.read_bytes()).hexdigest())
