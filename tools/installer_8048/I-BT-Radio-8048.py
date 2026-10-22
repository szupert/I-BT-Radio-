import sys,os,ctypes,importlib.util,runpy
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
 lock=kernel.CreateMutexW(None,False,r'Local\I_BT_Radio_Installer')
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
