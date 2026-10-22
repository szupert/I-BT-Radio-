import os
import uuid
"""I-BT-Radio updater: preserve data, or explicitly confirmed fresh 4/16 MB install."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import time
import threading
import queue
import codecs
import shutil

ROOT=Path(__file__).resolve().parent
def esptool_prefix():
    return [sys.executable,"--internal-esptool"] if getattr(sys,"frozen",False) else [sys.executable,"-u","-X","utf8","-m","esptool"]
_GEOMETRY=json.loads((ROOT/'GEOMETRY.json').read_text(encoding='utf-8'))
FLASH_MB=int(_GEOMETRY['flash_mb']);APP_SLOT=int(_GEOMETRY['app_slot']);FS_SIZE=int(_GEOMETRY['fs_size']);FS_OFFSET=int(_GEOMETRY['fs_offset'])
assert (FLASH_MB,APP_SLOT,FS_SIZE,FS_OFFSET) in [(16,0x600000,0x3f0000,0xc10000),(4,0x300000,0xf0000,0x310000)]
FLASH_LABEL=str(FLASH_MB)+'MB'
FLASH_CODE={4:2,16:4}[FLASH_MB]

FRESH_IMAGES=((0x1000,'bootloader.bin'),(0x8000,'partitions.bin'),
              (0xe000,'boot_app0.bin'),(0x10000,'firmware.bin'),(FS_OFFSET,'spiffs.bin'))

def chip_revision(info):
    revision=re.search(r'\brevision\s+v(\d+)(?:\.(\d+))?',info,re.I)
    if not revision:raise RuntimeError('Chip revision nem ellenorizheto; nincs torles vagy iras.')
    return int(revision.group(1))*100+int(revision.group(2) or 0)

def validate_variants(manifest):
    variants=manifest.get('firmware_variants',[])
    occupied=[];ids=set()
    for variant in variants:
        lo,hi=int(variant['minimum_revision_full']),int(variant['maximum_revision_full'])
        if not re.fullmatch(r'[A-Za-z0-9_-]+',variant['id']) or variant['id'] in ids:raise RuntimeError('Hibas/dupla firmware profil')
        ids.add(variant['id'])
        if set(variant['image_map'])!={'firmware.bin','bootloader.bin'}:raise RuntimeError('Ervenytelen firmware kep-par')
        if not (100<=lo<=hi<=399):raise RuntimeError('Hibas firmware revision tartomany')
        if any(lo<=b and a<=hi for a,b in occupied):raise RuntimeError('Atfedo firmware revision tartomany')
        occupied.append((lo,hi))
        for logical in ('firmware.bin','bootloader.bin'):
            name=variant['image_map'].get(logical,'')
            if Path(name).name!=name or name not in manifest['files']:raise RuntimeError('Hianyzo/hibas varians kep: '+logical)
            data=(ROOT/name).read_bytes();limit=APP_SLOT-8192 if logical=='firmware.bin' else 0x7000
            if not data or data[0]!=0xe9 or len(data)>limit:raise RuntimeError('Ervenytelen varians kep: '+name)
            if logical=='firmware.bin' and (len(data)<4 or data[3]>>4!=FLASH_CODE or manifest['firmware'].encode('ascii') not in data):raise RuntimeError('Varians firmware-verzio/flashmeret elteres')

def select_variant(info,manifest):
    if not manifest.get('firmware_variants'):
        check_chip_revision(info,manifest);return manifest
    revision=chip_revision(info)
    matches=[v for v in manifest['firmware_variants'] if int(v['minimum_revision_full'])<=revision<=int(v['maximum_revision_full'])]
    if len(matches)!=1:raise RuntimeError('Ehhez a chip-reviziohoz nincs egyertelmu kompatibilis firmware; nincs torles vagy iras.')
    variant=matches[0];selected=dict(manifest)
    selected.update(image_map=variant['image_map'],profile_id=variant['id'],chip_revision_full=revision,minimum_chip_revision_full=int(variant['minimum_revision_full']))
    print('[CHIP] ESP32 rev.%u.%u -> %s / %s' % (revision//100,revision%100,manifest['firmware'],variant['id']),flush=True)
    return selected

def image_path(manifest,logical):
    name=manifest.get('image_map',{}).get(logical,logical)
    if Path(name).name!=name:raise RuntimeError('Invalid firmware image path')
    return ROOT/name

def fresh_images(manifest):
    if manifest.get('fresh_install')!=('SMALL_'+str(FLASH_MB)+'MB_V1'):
        raise RuntimeError('A csomag nem tartalmaz ellenorzott teljes telepitest; nincs torles.')
    limits={'bootloader.bin':0x7000,'partitions.bin':0x1000,'boot_app0.bin':0x2000,'firmware.bin':APP_SLOT,'spiffs.bin':FS_SIZE}
    for offset,name in FRESH_IMAGES:
        path=image_path(manifest,name);expected=manifest['files'].get(path.name)
        if not expected or not path.is_file():raise RuntimeError('Hianyzo teljes telepitesi fajl: '+name)
        data=path.read_bytes()
        if not data or len(data)>limits[name] or len(data)!=expected['bytes'] or hashlib.sha256(data).hexdigest()!=expected['sha256']:
            raise RuntimeError('Hibas teljes telepitesi fajl: '+name+'; nincs torles.')
        if name=='spiffs.bin' and len(data)!=FS_SIZE:raise RuntimeError('Hibas fajlrendszermeret; nincs torles.')
        if name=='bootloader.bin' and data[0]!=0xe9:raise RuntimeError('Hibas bootloader; nincs torles.')
    return [(hex(offset),str(image_path(manifest,name))) for offset,name in FRESH_IMAGES]

def check_chip_revision(info, manifest):
    minimum=int(manifest.get('minimum_chip_revision_full',0))
    if not minimum:return
    revision=re.search(r'\brevision\s+v(\d+)(?:\.(\d+))?',info,re.I)
    if not revision:raise RuntimeError('Chip revision nem ellenorizheto; nincs torles vagy iras.')
    actual=int(revision.group(1))*100+int(revision.group(2) or 0)
    if actual<minimum:
        raise RuntimeError('Nem kompatibilis chip: rev.%s.%s; ez a PSRAM build minimum rev.%s.%s. Nincs torles vagy iras; kulon kompatibilis firmware kell.' % (revision.group(1),revision.group(2) or '0',minimum//100,minimum%100))

def confirm_format():
    print('\nTELJES TELEPITES / URES FLASH',flush=True)
    print('A teljes '+str(FLASH_MB)+' MB flash torlodik: Wi-Fi, allomaslista, beallitasok es fajlok is.',flush=True)
    print('Az SD-kartya nem erintett. Utana uj Wi-Fi es allomaslista-beallitas kell.',flush=True)
    answer=input('Biztosan formazod es ujratelepited? Ird be: IGEN (Enter = nem): ').strip().upper()
    if answer!='IGEN':raise RuntimeError('Formazas megszakitva. Nem tortent torles vagy feltoltes.')

def _safe_log_line(raw):
    text=raw.decode('utf-8','replace') if isinstance(raw,bytes) else str(raw)
    text=re.sub(r'\b(?:\d{1,3}\.){3}\d{1,3}\b','<IP-redacted>',text)
    text=re.sub(r'(?i)\b(ssid|password|passwd)\b.*$',r'\1=<redacted>',text)
    return text.encode('utf-8')

def verify_geometry():
    import struct
    blob=(ROOT/'partitions.bin').read_bytes();parts=[]
    for pos in range(0,len(blob)-31,32):
        magic,kind,sub,offset,size,label,flags=struct.unpack_from('<HBBII16sI',blob,pos)
        if magic in (0xffff,0xebeb):break
        if magic!=0x50aa or not size or offset<0x9000 or offset+size>FLASH_MB*1024*1024:raise RuntimeError('Invalid partition geometry')
        if any(offset<b+length and b<offset+size for _,_,b,length in parts):raise RuntimeError('Overlapping partition geometry')
        parts.append((kind,sub,offset,size))
    if (0,0x10,0x10000,APP_SLOT) not in parts or (1,0x82,FS_OFFSET,FS_SIZE) not in parts:raise RuntimeError('Partition profile mismatch')

def verify():
    manifest=json.loads((ROOT/'PACKAGE.json').read_text(encoding='utf-8'))
    version=manifest.get('firmware','')
    if manifest.get('flash_mb')!=FLASH_MB:raise RuntimeError('Manifest flash geometry mismatch')
    if not re.fullmatch(r'V\d+-R\d+(?:-WIP)?',version) or manifest.get('release')!=version+'-BETA':
        raise RuntimeError('Package version mismatch')
    for name, expected in manifest['files'].items():
        d=(ROOT/name).read_bytes()
        if hashlib.sha256(d).hexdigest()!=expected['sha256'] or len(d)!=expected['bytes']:
            raise RuntimeError('Package mismatch: '+name)
    d=(ROOT/'firmware.bin').read_bytes()
    if version.encode('ascii') not in d or d[0]!=0xe9 or d[3]>>4!=FLASH_CODE or len(d)>APP_SLOT-8192:
        raise RuntimeError('Firmware identity or size mismatch')
    validate_variants(manifest)
    if manifest.get('fresh_install'):
        fresh_images(manifest)
        for variant in manifest.get('firmware_variants',[]):fresh_images(dict(manifest,image_map=variant['image_map']))
    verify_geometry()
    print('[BETA] PACKAGE HASH PASS',flush=True)
    return manifest

def run_live(cmd, log, timeout):
    """Mirror real esptool output immediately, including CR progress, with a deadline."""
    proc=subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    chunks=queue.Queue(); captured=[]
    def reader():
        try:
            while True:
                data=proc.stdout.read1(1024)
                if not data:break
                chunks.put(data)
        finally:chunks.put(None)
    thread=threading.Thread(target=reader,daemon=True);thread.start()
    decoder=codecs.getincrementaldecoder('utf-8')('replace')
    started=time.monotonic();last_notice=started
    try:
        while True:
            now=time.monotonic()
            if now-started>=timeout:raise TimeoutError('A muvelet idotullepes miatt megallt.')
            try:data=chunks.get(timeout=min(1,timeout-(now-started)))
            except queue.Empty:
                if time.monotonic()-last_notice>=5:
                    print(f'\nVarakozas a keszulek valaszara... {int(time.monotonic()-started)} s',flush=True)
                    last_notice=time.monotonic()
                continue
            if data is None:break
            text=decoder.decode(data);captured.append(text)
            log.write(text);log.flush();sys.stdout.write(text);sys.stdout.flush()
        tail=decoder.decode(b'',final=True)
        if tail:captured.append(tail);log.write(tail);sys.stdout.write(tail);sys.stdout.flush()
        code=proc.wait(timeout=max(.1,timeout-(time.monotonic()-started)))
        return code,''.join(captured)
    finally:
        if proc.poll() is None:proc.kill();proc.wait()
        thread.join(timeout=2);proc.stdout.close();log.flush()

def detect_port(explicit=None,wait_seconds=60,stop_file=None):
    from serial.tools.list_ports import comports
    print('[COM] USB-soros portok felismerese...',flush=True)
    deadline=None if wait_seconds is None else time.monotonic()+wait_seconds
    announced=False
    notice=time.monotonic()+30
    while True:
        if stop_file and Path(stop_file).exists():raise KeyboardInterrupt
        if deadline is None and time.monotonic()>=notice:
            print('[COM] Tovabbra is varok az USB-kapcsolatra; nincs automatikus kilepes.',flush=True);notice=time.monotonic()+30
        ports=[p.device for p in comports() if p.vid in (0x10c4,0x1a86,0x0403,0x303a)]
        if explicit:
            match=next((p for p in ports if p.casefold()==explicit.casefold()),None)
            if match:
                print('[COM] Felismert es kivalasztott port: '+match,flush=True);return match
            if deadline is not None and time.monotonic()>=deadline:raise RuntimeError('A megadott COM port nem jelent meg: '+explicit)
            if not announced:
                print('[COM] Var a megadott eszkoz csatlakozasara: '+explicit,flush=True);announced=True
            time.sleep(1);continue
        if len(ports)==1:
            print('[COM] Felismert port: '+ports[0],flush=True);return ports[0]
        if len(ports)>1:
            print('[COM] TĂ„â€šĂ‚Â¶bb eszkĂ„â€šĂ‚Â¶z talĂ„â€šĂ‹â€ˇlhatĂ„â€šÄąâ€š; vĂ„â€šĂ‹â€ˇlaszd ki a rĂ„â€šĂ‹â€ˇdiĂ„â€šÄąâ€š portjĂ„â€šĂ‹â€ˇt:',flush=True)
            for i,name in enumerate(ports,1):print(f'  {i}. {name}',flush=True)
            choice=input('COM-sorszam vagy portnev: ').strip()
            if choice.isdigit() and 1<=int(choice)<=len(ports):choice=ports[int(choice)-1]
            match=next((p for p in ports if p.casefold()==choice.casefold()),None)
            if not match:raise RuntimeError('Ervenytelen portvalasztas; nincs feltoltes.')
            print('[COM] Kivalasztott port: '+match,flush=True);return match
        if not announced:
            print('[COM] Var a csatlakozasra... Csatlakoztasd az USB-kabelt. '+('(maximum 60 s)' if deadline is not None else 'Nincs automatikus kilepes; Ctrl+C = leallitas.'),flush=True);announced=True
        if deadline is not None and time.monotonic()>=deadline:raise RuntimeError('Nem jelent meg USB-soros port.')
        time.sleep(1)

def verify_boot(port, out, firmware_version, seconds=180):
    """Require exact CAPS plus setup completion; CAPS alone is sent before Wi-Fi initialization."""
    import serial
    deadline=time.monotonic()+seconds
    connection=None
    pending=b''
    log_pending=b''
    next_query=0
    next_notice=0
    with (out/'boot.log').open('wb') as log:
        try:
            while time.monotonic()<deadline:
                if connection is None:
                    try:
                        connection=serial.Serial()
                        connection.port=port
                        connection.baudrate=115200
                        connection.timeout=.25
                        connection.dtr=False
                        connection.rts=False
                        connection.open()
                        print('[BOOT] Soros kapcsolat nyitva, BETA firmware valaszra var...',flush=True)
                    except (serial.SerialException,OSError):
                        connection=None
                        time.sleep(.5)
                        continue
                if time.monotonic()>=next_notice:
                    print('[BOOT] Var a teljes setup befejezesere... maximum 180 s',flush=True)
                    next_notice=time.monotonic()+5
                if time.monotonic()>=next_query:
                    connection.write(b'r10 caps\n')
                    next_query=time.monotonic()+2
                chunk=connection.read(1024)
                if not chunk:continue
                log_pending+=chunk
                while b'\n' in log_pending:
                    line,log_pending=log_pending.split(b'\n',1)
                    log.write(_safe_log_line(line)+b'\n')
                log.flush()
                pending=(pending+chunk)[-8192:]
                if b'esp_timer: Not enough memory' in pending or b'abort() was called' in pending or b'Guru Meditation Error' in pending:
                    raise RuntimeError('Firmware boot failure; see boot.log')
                tag=re.escape(firmware_version.encode('ascii'))
                if re.search(rb'\[R10-CAPS\][^\r\n]*firmware='+tag+rb'\b',pending) and re.search(rb'\[BOOT-READY\] firmware='+tag+rb' setup=1\b',pending):
                    print('[BOOT] '+firmware_version+' BETA verzio es setup befejezes PASS',flush=True)
                    return True
        finally:
            if log_pending:log.write(_safe_log_line(log_pending));log.flush()
            if connection is not None:connection.close()
    raise RuntimeError(firmware_version+' setup completion not verified within 180 s; see boot.log')

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--verify-only',action='store_true');parser.add_argument('--port');parser.add_argument('--recovery',action='store_true',help=argparse.SUPPRESS);parser.add_argument('--write-baud',type=int,choices=(115200,460800,921600,1500000),default=1500000);parser.add_argument('--fresh',action='store_true');parser.add_argument('--wait-for-device',action='store_true');parser.add_argument('--handoff');parser.add_argument('--stop-file');a=parser.parse_args()
    write_baud=115200 if a.recovery else a.write_baud
    print("[1/5] BETA csomag es firmware ellenorzese...",flush=True)
    manifest=verify()
    print('[BETA] Firmware: '+manifest['firmware'],flush=True)
    if a.verify_only:return 0
    port=detect_port(a.port,wait_seconds=None,stop_file=a.stop_file) if a.wait_for_device else detect_port(a.port)
    out=(Path(os.environ.get('LOCALAPPDATA',str(Path.home())))/'I-BT-Radio'/'Logs')/(datetime.now(timezone.utc).strftime('UPDATE_%Y%m%d_%H%M%S_%f')+'_'+uuid.uuid4().hex[:8]);out.mkdir(parents=True)
    result={'release':manifest['release'],'firmware':manifest['firmware'],'result':'FAIL_PRECHECK','application_only':True,'transport':'ROM_115200_RAW' if a.recovery else 'STUB_'+str(write_baud),'write_baud':write_baud}
    def call(action,args,after='no-reset',timeout=180):
        cmd=[*esptool_prefix(),'--chip','esp32','--port',port,'--baud',str(115200 if action!='write-flash' else write_baud),'--before','no-reset','--connect-attempts','3',*(['--no-stub'] if action!='write-flash' or a.recovery else []),'--after',after,action,*args]
        with (out/'esptool.log').open('a',encoding='utf-8') as log:
            log.write(subprocess.list2cmdline(cmd)+'\n');log.flush()
            code,text=run_live(cmd,log,timeout)
        if code:
            detail=next((line.strip() for line in reversed(text.splitlines()) if 'fatal error' in line.lower()),'see esptool.log')
            raise RuntimeError(action+' failed: '+detail)
        return text
    try:
        print('[2/5] Var a bootloader csatlakozasra (115200 baud). BOOT nyomva + rovid EN/RESET, majd BOOT elenged.',flush=True)
        info=call('flash-id',[])
        if not re.search(r'Detected flash size:\s*'+str(FLASH_MB)+r'MB\b',info):raise RuntimeError('A csomag '+FLASH_LABEL+' flashhez keszult; az eszkoz merete elter. Nincs torles/iras.')
        manifest=select_variant(info,manifest)
        result.update(profile_id=manifest.get('profile_id','legacy'),chip_revision_full=manifest.get('chip_revision_full'),selected_firmware_sha256=hashlib.sha256(image_path(manifest,'firmware.bin').read_bytes()).hexdigest())
        print('[3/5] Flash azonositva: '+str(FLASH_MB)+' MB. Particiok ellenorzese...',flush=True)
        call('read-flash',['0x8000','0x1000',str(out/'partitions-read.bin')])
        actual=(out/'partitions-read.bin').read_bytes();expected=(ROOT/'partitions.bin').read_bytes()
        if len(actual)!=4096:raise RuntimeError('Hibas particio-visszaolvasas; nincs torles vagy iras.')
        fresh=a.fresh or actual[:len(expected)]!=expected
        images=[('0x10000',str(image_path(manifest,'firmware.bin')))]
        if manifest.get('firmware_variants') and not fresh:
            images.insert(0,('0x1000',str(image_path(manifest,'bootloader.bin'))))
            result.update(application_only=False,update_scope='MATCHED_BOOTLOADER_AND_APP_NVS_PRESERVED')
        if fresh:
            images=fresh_images(manifest)
            print('[PARTICIO] '+('Ures flash felismerve.' if actual==b'\xff'*4096 else 'Teljes telepites szukseges vagy kerve.'),flush=True)
            confirm_format()
            result.update(application_only=False,fresh_install=True,format_confirmed=True,result='FAIL_ERASE')
            print('[FORMAT] Jovahagyva. Teljes flash torlese, elo allapot alabb.',flush=True)
            call('erase-region',['0x0',hex(FLASH_MB*1024*1024)],timeout=600)
        result['selected_images']=[{'offset':offset,'file':Path(path).name,'sha256':hashlib.sha256(Path(path).read_bytes()).hexdigest()} for offset,path in images]
        result['result']='FAIL_WRITE'
        print('[4/5] BETA feltoltes: '+('HELYREALLITAS, ROM 115200 baud, tomorites nelkul' if a.recovery else 'RAM stub, '+str(write_baud)+' baud')+'. Elo szazalek alabb. USB maradjon bedugva.',flush=True)
        addresses=[value for pair in images for value in pair]
        text=call('write-flash',['--flash-size',FLASH_LABEL,'--flash-mode','dio' if fresh else 'keep','--flash-freq','40m' if fresh else 'keep',('--no-compress' if a.recovery else '--compress'),*addresses],after='hard-reset',timeout=max(900, int(sum(Path(path).stat().st_size for _,path in images)*10/write_baud*3)+120))
        if text.count('Hash of data verified.')<len(images):raise RuntimeError('Write verification not confirmed for every image')
        print('[5/5] Flash hash PASS. Indulas es BETA firmware-verzio ellenorzese...',flush=True)
        result['result']='FAIL_BOOT'
        verify_boot(port,out,manifest['firmware'])
        result['result']='PASS_FLASH_HASH_BOOT'
        print('[5/5] '+manifest['firmware']+' setup befejezes PASS. Kovetkezo: a root OLVASS_EL.txt szerinti keszulekteszt.',flush=True)
        return 0
    except Exception as e:
        result['error']=str(e);print('\nHIBA: '+str(e),flush=True)
        if result['result']=='FAIL_WRITE':
            result['recovery_recommended']=not a.recovery
            result['fresh_flash_may_be_empty']=bool(result.get('fresh_install'))
            if result.get('fresh_install'):print('A torles mar megtortent, a flash ures/reszleges lehet. Ez nem onmagaban hardverhiba.',flush=True)
            if not a.recovery:print('Az atvitel nem sikerult. Ellenorizd az USB-kapcsolatot; uj probanal az automata ismet megmeri a sebesseget. Nincs automatikus ujrairas.',flush=True)
        return 2
    finally:
        print('Naplok: '+str(out),flush=True)
        (out/'RESULT.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
        if a.handoff:
            receipt=Path(a.handoff)
            temporary=receipt.with_suffix('.tmp')
            temporary.write_text(json.dumps(result,indent=2),encoding='utf-8')
            temporary.replace(receipt)
        try:
            back=(Path(os.environ.get('LOCALAPPDATA',str(Path.home())))/'I-BT-Radio'/'RETURN_TO_EVA')/out.name
            back.parent.mkdir(parents=True,exist_ok=True)
            shutil.copytree(out,back,dirs_exist_ok=True)
            print('RETURN_TO_EVA: '+str(back),flush=True)
        except Exception as error:print('LOG-MIRROR HIBA: '+str(error),flush=True)

if __name__=='__main__':sys.exit(main())

