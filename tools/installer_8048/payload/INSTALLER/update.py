import os
import argparse, codecs, hashlib, json, queue, re, shutil, struct, subprocess, sys, threading, time
from pathlib import Path
from datetime import datetime
ROOT=Path(__file__).resolve().parent
WRITE_BAUD=115200
def esptool_prefix():
    return [sys.executable,"--internal-esptool"] if getattr(sys,"frozen",False) else [sys.executable,"-u","-X","utf8","-m","esptool"]
VERSION="MENU8048C-R4"
def verify():
    m=json.loads((ROOT/'PACKAGE.json').read_text(encoding='utf-8'))
    if m['firmware']!=VERSION or m['target']!='ESP32-8048S070C': raise RuntimeError('Wrong target/version')
    expected={'bootloader.bin':0,'partitions.bin':0x8000,'boot_app0.bin':0xe000,'firmware.bin':0x10000}
    if m['offsets']!=expected: raise RuntimeError('Invalid flash offsets')
    for name,e in m['files'].items():
        d=(ROOT/name).read_bytes()
        if len(d)!=e['bytes'] or hashlib.sha256(d).hexdigest()!=e['sha256']:raise RuntimeError('Hash mismatch: '+name)
    boot=(ROOT/'bootloader.bin').read_bytes()
    if boot[0]!=0xe9 or boot[2]!=2 or struct.unpack_from('<H',boot,12)[0]!=9:raise RuntimeError('Bootloader must be S3 DIO')
    d=(ROOT/'firmware.bin').read_bytes()
    if d[0]!=0xe9 or struct.unpack_from('<H',d,12)[0]!=9 or VERSION.encode() not in d or len(d)>0x640000:raise RuntimeError('Wrong firmware image')
    factory=(ROOT/'partitions_factory.bin').read_bytes()
    if hashlib.sha256(factory).hexdigest()!='aaae2888c5a6a348004b5b436f47abb25ae32e72d9003902955a998eda723edd':raise RuntimeError('Wrong factory table')
    table=(ROOT/'partitions.bin').read_bytes()
    apps=[]
    for i in range(0,len(table)-31,32):
        magic,typ,sub,offset,size,label,flags=struct.unpack_from('<HBBII16sI',table,i)
        if magic==0x50aa and typ==0:apps.append((offset,size))
    if apps!=[(0x10000,0x640000),(0x650000,0x640000)]:raise RuntimeError('Wrong 16MB partition table')
    print('[PACKAGE] HASH / S3 / PARTICIO PASS',flush=True);return m
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

def detect_port(explicit=None,wait_seconds=60):
    from serial.tools.list_ports import comports
    print('[COM] USB-soros portok felismerese...',flush=True)
    deadline=time.monotonic()+wait_seconds
    announced=False
    while True:
        ports=[p.device for p in comports() if p.vid in (0x10c4,0x1a86,0x0403,0x303a)]
        if explicit:
            match=next((p for p in ports if p.casefold()==explicit.casefold()),None)
            if match:
                print('[COM] Felismert es kivalasztott port: '+match,flush=True);return match
            if time.monotonic()>=deadline:raise RuntimeError('A megadott COM port nem jelent meg: '+explicit)
            if not announced:
                print('[COM] Var a megadott eszkoz csatlakozasara: '+explicit,flush=True);announced=True
            time.sleep(1);continue
        if len(ports)==1:
            print('[COM] Felismert port: '+ports[0],flush=True);return ports[0]
        if len(ports)>1:
            print('[COM] TĂ¶bb eszkĂ¶z talĂˇlhatĂł; vĂˇlaszd ki a rĂˇdiĂł portjĂˇt:',flush=True)
            for i,name in enumerate(ports,1):print(f'  {i}. {name}',flush=True)
            choice=input('COM-sorszĂˇm vagy portnĂ©v: ').strip()
            if choice.isdigit() and 1<=int(choice)<=len(ports):choice=ports[int(choice)-1]
            match=next((p for p in ports if p.casefold()==choice.casefold()),None)
            if not match:raise RuntimeError('Ă‰rvĂ©nytelen portvĂˇlasztĂˇs; semmi nem lett feltĂ¶ltve.')
            print('[COM] Kivalasztott port: '+match,flush=True);return match
        if not announced:
            print('[COM] Var a csatlakozasra... Csatlakoztasd az USB-kabelt (maximum 60 s).',flush=True);announced=True
        if time.monotonic()>=deadline:raise RuntimeError('Nem jelent meg USB-soros port.')
        time.sleep(1)


def validate_flash_info(info):
    if not re.search(r'Detected flash size:\s*16MB',info):raise RuntimeError('Nem 16MB flash; nem irunk')
def validate_existing(old,expected,factory=None):
    if len(old)!=4096:raise RuntimeError('Hibas particioolvasas; feltoltes tiltva')
    if old==b'\xff'*4096 or old[:len(expected)]==expected:return 'partitions.bin'
    if factory is not None and old[:len(factory)]==factory:return 'partitions_factory.bin'
    raise RuntimeError('Ismeretlen particiok; feltoltes tiltva')
def command_for(port,action,args):
    return [*esptool_prefix(),'--chip','esp32s3','--port',port,'--baud',str(WRITE_BAUD if action=='write-flash' else 115200),'--before','default-reset' if action=='flash-id' else 'no-reset','--after','hard-reset' if action=='write-flash' else 'no-reset',*([] if action=='write-flash' else ['--no-stub']),action,*args]
def boot_check(port,log):
    import serial
    s=serial.Serial();s.port=port;s.baudrate=115200;s.timeout=.3;s.dtr=False;s.rts=False;s.open()
    deadline=time.monotonic()+45;next_query=0;data=b''
    try:
        while time.monotonic()<deadline:
            if time.monotonic()>=next_query:s.write(b'status\n');next_query=time.monotonic()+2
            chunk=s.read(2048)
            if chunk:
                text=chunk.decode('utf-8','replace');sys.stdout.write(text);sys.stdout.flush();log.write(text);log.flush();data=(data+chunk)[-8192:]
                if re.search(rb'\[STATUS\] firmware=MENU8048C-R4 display=1 touch=0x(?:5D|14) psram=8388608',data):return
                if b'[FAIL]' in data or b'Guru Meditation' in data:raise RuntimeError('Boot hiba; lasd a naplot')
        raise RuntimeError('Boot / kijelzo / GT911 nem igazolt 45 s alatt')
    finally:s.close()
def flash_args(manifest,partition_image):
    args=['--flash-size','16MB','--flash-mode','keep','--flash-freq','keep']
    for name,offset in manifest['offsets'].items():args.extend([hex(offset),str(ROOT/(partition_image if name=='partitions.bin' else name))])
    return args
def acquire_port_lock(port):
    import msvcrt
    folder=(Path(os.environ.get('LOCALAPPDATA',str(Path.home())))/'I-BT-Radio'/'Logs');folder.mkdir(parents=True,exist_ok=True)
    handle=(folder/(port.upper()+'.upload.lock')).open('a+b')
    if handle.tell()==0:handle.write(b'0');handle.flush()
    handle.seek(0)
    try:msvcrt.locking(handle.fileno(),msvcrt.LK_NBLCK,1)
    except OSError:
        handle.close();raise RuntimeError('Masik feltolto hasznalja ezt a COM-portot. Zard be a masik ablakot.')
    return handle
def main():
    global WRITE_BAUD
    a=argparse.ArgumentParser();a.add_argument('--verify-only',action='store_true');a.add_argument('--port');o=a.parse_args()
    out=(Path(os.environ.get('LOCALAPPDATA',str(Path.home())))/'I-BT-Radio'/'Logs')/datetime.now().strftime('MENU8048C_%Y%m%d_%H%M%S_%f');out.mkdir(parents=True,exist_ok=True)
    result={'firmware':VERSION,'status':'FAIL','physical_display':'NOT_RUN'}
    port_lock=None
    try:
        print('[1/5] ESP32-8048S070C MENU BETA csomagellenorzes...',flush=True);m=verify()
        if o.verify_only:result['status']='PACKAGE_PASS';return 0
        port=detect_port(o.port);result['port']=port
        port_lock=acquire_port_lock(port)
        def call(action,args,timeout=120):
            with (out/'upload.log').open('a',encoding='utf-8') as log:
                code,text=run_live(command_for(port,action,args),log,timeout)
            if code:raise RuntimeError(action+' sikertelen. BOOT nyomva, RESET roviden, BOOT elenged.')
            return text
        print('[2/5] S3 azonositasa 115200 baud. Ha nem kapcsolodik: BOOT + RESET.',flush=True)
        info=call('flash-id',[])
        validate_flash_info(info)
        print('[SEBESSEG] Ketiranyu S3 kapcsolatproba; a particioolvasas115200 marad.',flush=True)
        probe=[sys.executable,'--internal-speed-probe','--port',port,'--output',str(out/'speed.json')] if getattr(sys,'frozen',False) else [sys.executable,str(ROOT/'speed_probe.py'),'--port',port,'--output',str(out/'speed.json')]
        with (out/'upload.log').open('a',encoding='utf8') as log:code,text=run_live(probe,log,120)
        if code:raise RuntimeError('A sebessegproba nem igazolt; nincs iras.')
        speed=json.loads((out/'speed.json').read_text());WRITE_BAUD=speed['write_baud']
        if WRITE_BAUD not in (1500000,921600,460800,115200) or not speed['baseline_verified']:raise RuntimeError('Hibas sebessegjelentes')
        result['transport']=speed
        print('[3/5] Meglevo particiok ellenorzese...',flush=True)
        call('read-flash',['0x8000','0x1000',str(out/'partitions-read.bin')])
        old=(out/'partitions-read.bin').read_bytes();expected=(ROOT/'partitions.bin').read_bytes()
        partition_image=validate_existing(old,expected,(ROOT/'partitions_factory.bin').read_bytes())
        result['partition_image']=partition_image
        if partition_image=='partitions_factory.bin':
            if (ROOT/'firmware.bin').stat().st_size>0x300000:raise RuntimeError('A firmware nem fer el a gyari 3MB alkalmazasban')
            print('[PARTICIO] Gyari kiosztas megtartva; 3MB alkalmazashely, 16MB fizikai flash.',flush=True)
        print('[BACKUP] Regi bootloader / NVS / particio mentese 115200 baud...',flush=True)
        call('read-flash',['0x0','0x10000',str(out/'before-install-64KB.bin')])
        print(f'[4/5] Teljes MENU firmware: {WRITE_BAUD} baud, elo szazalek. SMALL-ra nem toltheto.',flush=True)
        print('[BOOT MODE] A DIO bootloader sajat beallitasa marad, nincs QIO feluliras.',flush=True)
        args=flash_args(m,partition_image)
        output=call('write-flash',args,900)
        if output.count('Hash of data verified.')<4:raise RuntimeError('Nem mind a negy kep visszaellenorzott')
        print('[5/5] Boot / PSRAM / GT911 ellenorzes...',flush=True)
        with (out/'boot.log').open('w',encoding='utf-8') as log:boot_check(port,log)
        result['status']='UPLOAD_AND_BOOT_PASS';print('KESZ. A kijelzo es az erintes fizikailag ellenorizendo.',flush=True);return 0
    except Exception as e:
        result['error']=str(e);print('HIBA: '+str(e),flush=True);return 1
    finally:
        if port_lock is not None:port_lock.close()
        (out/'RESULT.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
        back=(Path(os.environ.get('LOCALAPPDATA',str(Path.home())))/'I-BT-Radio'/'RETURN_TO_EVA')/out.name;back.parent.mkdir(parents=True,exist_ok=True);shutil.copytree(out,back,dirs_exist_ok=True)
        print('NAPLO: '+str(back),flush=True)
if __name__=='__main__':sys.exit(main())
