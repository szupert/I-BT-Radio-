import sys,os,ctypes,importlib.util
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
