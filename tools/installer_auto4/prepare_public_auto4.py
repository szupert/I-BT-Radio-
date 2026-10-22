"""Bundle existing checked four-profile updater; no firmware build or serial access."""
from pathlib import Path
import shutil,json,hashlib,subprocess,sys,importlib.util
B=Path('C:/esp32/SYSTEM');W=B/'TEMP/PUBLIC_AUTO4_20261022';M=W/'payload/VARIANTS';U=Path('C:/esp32/USER/I-BT-Radio_LATEST/INSTALLER/VARIANTS')
W.mkdir(exist_ok=True);M.mkdir(parents=True,exist_ok=True)
folders=['4MB_PSRAM_NELKUL','4MB_PSRAM','16MB_PSRAM_NELKUL','16MB_PSRAM','AUTO']
for name in folders:
 src=U/name;target=M/name;target.mkdir(exist_ok=True)
 hashes=json.loads((src/'HASHES.json').read_text())
 for rel,v in hashes.items():
  p=src/rel;data=p.read_bytes();assert len(data)==v['bytes'] and hashlib.sha256(data).hexdigest()==v['sha256'],name+'/'+rel
  dst=target/rel;dst.parent.mkdir(parents=True,exist_ok=True);dst.write_bytes(data)
 # Retain updater semantics, replacing only frozen-runtime/tool/log plumbing.
 p=target/'INSTALLER/update.py';s=p.read_text(encoding='utf-8')
 s=s.replace("Path('C:/esp32/SYSTEM/LOGS/UPLOAD')","(Path(os.environ.get('LOCALAPPDATA',str(Path.home())))/'I-BT-Radio'/'Logs')")
 if 'import os' not in s:s='import os\n'+s
 s=s.replace("[sys.executable,'-u','-X','utf8','-m','esptool'","[*esptool_prefix()")
 s=s.replace("[sys.executable, '-u', '-X', 'utf8', '-m', 'esptool'","[*esptool_prefix()")
 s=s.replace('ROOT=Path(__file__).resolve().parent','ROOT=Path(__file__).resolve().parent\ndef esptool_prefix():\n    return [sys.executable,"--internal-esptool"] if getattr(sys,"frozen",False) else [sys.executable,"-u","-X","utf8","-m","esptool"]')
 # AUTO imports checked profile modules; it does not invoke esptool directly.
 if name=='AUTO':
  a="[sys.executable, '-B', '-X', 'utf8', '-u',\n                                  str(ROOT / 'memory_probe.py'), '--port', port,"
  assert a in s
  s=s.replace(a,"[*probe_prefix(), '--port', port,")
  s=s.replace('ROOT = Path(__file__).resolve().parent','ROOT = Path(__file__).resolve().parent\ndef probe_prefix():\n    return [sys.executable,\"--internal-memory-probe\"] if getattr(sys,\"frozen\",False) else [sys.executable,\"-B\",\"-X\",\"utf8\",\"-u\",str(ROOT/\"memory_probe.py\")]')
  s=s.replace("MATRIX.parent.parent / 'RETURN_TO_EVA'", "(Path(os.environ.get('LOCALAPPDATA',str(Path.home())))/'I-BT-Radio'/'RETURN_TO_EVA')")
 else:
  s=s.replace("Path('C:/esp32/USER/I-BT-Radio_LATEST/RETURN_TO_EVA')", "(Path(os.environ.get('LOCALAPPDATA',str(Path.home())))/'I-BT-Radio'/'RETURN_TO_EVA')")
 p.write_text(s,encoding='utf8')
 if name!='AUTO':assert 'def esptool_prefix()' in s
 if name!='AUTO':
  package=target/'INSTALLER/PACKAGE.json';meta=json.loads(package.read_text());meta['files']={rel:dict(bytes=(package.parent/rel).stat().st_size,sha256=hashlib.sha256((package.parent/rel).read_bytes()).hexdigest()) for rel in meta['files']};package.write_text(json.dumps(meta,indent=2),encoding='utf8')
 new={rel:dict(bytes=(target/rel).stat().st_size,sha256=hashlib.sha256((target/rel).read_bytes()).hexdigest()) for rel in hashes}
 (target/'HASHES.json').write_text(json.dumps(new,indent=2),encoding='utf8')
entry=r'''import sys,os,ctypes,importlib.util
from pathlib import Path
def run():
 if '--internal-esptool' in sys.argv:
  sys.argv=[sys.argv[0]]+sys.argv[sys.argv.index('--internal-esptool')+1:]
  import esptool
  esptool._main();return 0
 base=Path(getattr(sys,'_MEIPASS',Path(__file__).resolve().parent))
 if '--internal-memory-probe' in sys.argv:
  import runpy
  args=sys.argv[sys.argv.index('--internal-memory-probe')+1:];probe=base/'payload/VARIANTS/AUTO/INSTALLER/memory_probe.py';sys.argv=[str(probe)]+args;runpy.run_path(str(probe),run_name='__main__');return 0
 if '--extract-payload' in sys.argv:
  import shutil
  target=Path(sys.argv[sys.argv.index('--extract-payload')+1]);assert not target.exists(),'A celmappa mar letezik';shutil.copytree(base/'payload',target);print('PAYLOAD_EXTRACTED_NO_COM');return 0
 root=base/'payload/VARIANTS/AUTO/INSTALLER/update.py'
 sys.path.insert(0,str(root.parent))
 spec=importlib.util.spec_from_file_location('auto_update',root);up=importlib.util.module_from_spec(spec);spec.loader.exec_module(up)
 if '--verify-only' in sys.argv:return up.main()
 kernel=ctypes.WinDLL('kernel32',use_last_error=True);kernel.CreateMutexW.argtypes=[ctypes.c_void_p,ctypes.c_bool,ctypes.c_wchar_p];kernel.CreateMutexW.restype=ctypes.c_void_p
 lock=kernel.CreateMutexW(None,False,r'Local\I_BT_Radio_Installer');kernel.CloseHandle.argtypes=[ctypes.c_void_p]
 if not lock or ctypes.get_last_error()==183:print('Mar fut egy I-BT-Radio telepito. Nincs masodik COM-tulajdonos.');return 2
 try:
  print('I-BT-Radio BETA | automatikus flash/PSRAM felismeres | 2026.10.22.')
  print('4MB/noPSRAM: R211 DIAGNOSZTIKA, BT/BLE/SD/WebUI OFF. Nem stabil kiadas!')
  print('4MB/PSRAM es16MB/PSRAM: R176;16MB/noPSRAM: korabbi R189. A bekotesnek egyeznie kell.')
  print('Nincs automatikus stresszteszt, ujraflasheles vagy vak torles. CTRL+C: leallitas.')
  up.verify_profiles()
  if input('Enter: felismeres es telepites; Q: kilepes: ').strip().lower()=='q':return 0
  sys.argv=[sys.argv[0], '--wait-for-device'];return up.main()
 finally:kernel.CloseHandle(lock)
if __name__=='__main__':
 for stream in [sys.stdout,sys.stderr]:
  if hasattr(stream,'reconfigure'):stream.reconfigure(encoding='utf8',errors='replace')
 internal=any(v in sys.argv for v in ['--internal-esptool','--internal-memory-probe','--extract-payload']);verify='--verify-only' in sys.argv
 try:rc=run()
 except KeyboardInterrupt:print('USER_STOP');rc=130
 except Exception as e:print('HIBA: '+str(e));rc=2
 if not internal and not verify:input('A naplo elmentve. Enter: bezaras. ')
 sys.exit(rc)
'''
(W/'I-BT-Radio-Auto4.py').write_text(entry,encoding='utf8')
# Self-contained raw source payload is published alongside EXE.
lic=B/'ARTIFACTS/PUBLIC_R163/INSTALLER_LICENSES.txt';shutil.copyfile(lic,W/'INSTALLER_LICENSES.txt')
result=subprocess.run([sys.executable,'-B','-X','utf8',str(W/'I-BT-Radio-Auto4.py'),'--verify-only'],capture_output=True,text=True,timeout=25)
assert result.returncode==0,result.stdout+result.stderr
print(result.stdout)
import esptool
toolpath=Path(esptool.__file__).resolve().parent.parent
cmd=[sys.executable,'-m','PyInstaller','--noconfirm','--clean','--onefile','--console','--name','I-BT-Radio-Auto4-Install','--distpath',str(W/'dist'),'--workpath',str(W/'build'),'--specpath',str(W),'--paths',str(toolpath),'--add-data',str(W/'payload')+';payload','--collect-all','esptool','--collect-all','serial',str(W/'I-BT-Radio-Auto4.py')]
with (W/'build.log').open('w',encoding='utf8') as f:rc=subprocess.call(cmd,stdout=f,stderr=subprocess.STDOUT)
assert rc==0,str(W/'build.log')
exe=W/'dist/I-BT-Radio-Auto4-Install.exe'
for args in [['--verify-only'],['--internal-esptool','version'],['--internal-memory-probe','--help']]:
 result=subprocess.run([str(exe)]+args,capture_output=True,text=True,encoding='utf8',timeout=40);assert result.returncode==0,result.stdout+result.stderr
print('PASS_PUBLIC_AUTO4_FROZEN_VERIFY_AND_ESPTOOL_NO_COM',exe.stat().st_size)
meta=dict(result='PASS_PUBLIC_AUTO4_FROZEN_HOST_ONLY',exe=str(exe),bytes=exe.stat().st_size,sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),firmware_profiles={name:json.loads((M/name/'INSTALLER/PACKAGE.json').read_text())['firmware'] for name in folders[:-1]},COM=False,flash=False,physical='NOT_RUN',runtime_included=True)
(B/'LOGS/REPORTS/PUBLIC_AUTO4_20261022.json').write_text(json.dumps(meta,indent=2),encoding='utf8')
