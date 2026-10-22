import argparse,json,sys,time,shutil,re
from datetime import datetime
from pathlib import Path
from update import detect_port
import serial
root=Path(__file__).resolve().parent
out=Path('C:/esp32/SYSTEM/LOGS/STRESS_TEST')/datetime.now().strftime('MENU8048C_%Y%m%d_%H%M%S_%f');out.mkdir(parents=True,exist_ok=True)
result={'firmware':'MENU8048C-R4','status':'FAIL','touch':'NOT_RUN','audio':'NOT_APPLICABLE'}
s=None
try:
    port=detect_port();s=serial.Serial();s.port=port;s.baudrate=115200;s.timeout=.2;s.dtr=False;s.rts=False;s.open()
    data=b'';end=time.monotonic()+20;query=0
    with (out/'serial.log').open('wb') as log:
        while time.monotonic()<end:
            if time.monotonic()>query:s.write(b'status\n');query=time.monotonic()+2
            chunk=s.read(2048);log.write(chunk);data=(data+chunk)[-8192:];print(chunk.decode('utf-8','replace'),end='',flush=True)
            if b'firmware=MENU8048C-R4 display=1' in data:break
        else:raise RuntimeError('Nem a MENU8048C-R4 fut, vagy nincs kijelzo')
        s.write(b'selftest\n');data=b'';end=time.monotonic()+120
        while time.monotonic()<end:
            chunk=s.read(4096);log.write(chunk);log.flush();data=(data+chunk)[-8192:];print(chunk.decode('utf-8','replace'),end='',flush=True)
            if b'Guru Meditation' in data or b'[BOOT]' in data:raise RuntimeError('Reset/crash')
            if re.search(rb'RENDER_PASS pages=55 rows=([0-9]+)',data):
                measured=int(re.search(rb'RENDER_PASS pages=55 rows=([0-9]+)',data).group(1))
                if measured!=287:raise RuntimeError('Hiányos menusor coverage: '+str(measured))
                result['status']='RENDER_PASS';result['pages']=55;result['rows']=measured;break
        else:raise RuntimeError('Selftest timeout')
except Exception as e:result['error']=str(e);print('HIBA:',e)
finally:
    if s:
        try:s.write(b'stop\n')
        except Exception:pass
        s.close()
    (out/'RESULT.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    back=root.parent/'RETURN_TO_EVA'/out.name;shutil.copytree(out,back,dirs_exist_ok=True);print('NAPLO:',back)
sys.exit(0 if result['status']=='RENDER_PASS' else 1)
