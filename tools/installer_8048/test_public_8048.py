from pathlib import Path
import os,sys,subprocess,json,importlib.util,io,tempfile,unittest
from contextlib import redirect_stdout
B=Path('C:/esp32/SYSTEM');P=B/'SOURCE/I-BT-Radio_MENU_8048S070C';W=B/'TEMP/PUBLIC_8048_20261022';I=W/'payload/INSTALLER'
# Run all original package checks against portable payload, not a connected board.
s=(P/'tools/test_package.py').read_text(encoding='utf8');s=s.replace("U=Path('C:/esp32/USER/I-BT-Radio_MENU_8048S070C')","U=Path('C:/esp32/SYSTEM/TEMP/PUBLIC_8048_20261022/payload')")
s=s.replace("P/'tools/update.py'","U/'INSTALLER/update.py'")
s=s.replace("('write-flash',1500000)","('write-flash',115200)")
s=s.replace("if __name__=='__main__':unittest.main()",'')
ns={'__file__':str(P/'tools/test_package.py'),'__name__':'host8048'};exec(compile(s,str(P/'tools/test_package.py'),'exec'),ns)
result=unittest.TextTestRunner(verbosity=1).run(unittest.defaultTestLoader.loadTestsFromTestCase(ns['Checks']));assert result.wasSuccessful()
def load(n,p):
 spec=importlib.util.spec_from_file_location(n,p);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
probe=load('probe8048',I/'speed_probe.py')
class FakeROM:
 def __init__(self,fail_high=False,fail_restore=False):self.reg={probe.W0+i*4:i+700 for i in range(16)};self.original=dict(self.reg);self.baud=115200;self.fail_high=fail_high;self.fail_restore=fail_restore
 def read_reg(self,r,**kw):
  assert r in self.reg
  if self.fail_restore or (self.fail_high and self.baud==1500000):raise RuntimeError('link failed')
  return self.reg[r]
 def write_reg(self,r,v):assert r in self.reg;self.reg[r]=v
 def change_baud(self,n):self.baud=n
 def _set_port_baudrate(self,n):self.baud=n
 def flush_input(self):pass
for fallback in [False,True]:
 f=FakeROM(fallback)
 with redirect_stdout(io.StringIO()):r=probe.choose_baud(f)
 assert r['write_baud']==(921600 if fallback else 1500000) and r['baseline_verified'] and f.baud==115200 and f.reg==f.original
# If connection cannot be restored, the probe fails instead of permitting a flash.
f=FakeROM();original=f.read_reg;reads=[0]
def broken(r,**kw):
 reads[0]+=1
 if reads[0]>16:raise RuntimeError('lost link')
 return original(r,**kw)
f.read_reg=broken
with redirect_stdout(io.StringIO()):
 try:probe.choose_baud(f)
 except RuntimeError:pass
 else:raise AssertionError('restore failure accepted')
# Portable EXE: no Python on PATH, Unicode/spaced destination, real frozen child dispatch.
meta=json.loads((B/'LOGS/REPORTS/PUBLIC_8048_20261022.json').read_text());exe=Path(meta['exe']);T=W/'portable próba';T.mkdir(exist_ok=True)
import shutil
copy=T/exe.name;shutil.copyfile(exe,copy);env=dict(os.environ,PATH=os.environ['SystemRoot']+'/System32',LOCALAPPDATA=str(T/'localdata'));env.pop('PYTHONPATH',None);env.pop('PYTHONHOME',None)
for args in [['--verify-only'],['--internal-esptool','version'],['--internal-speed-probe','--help']]:
 p=subprocess.run([str(copy)]+args,env=env,capture_output=True,text=True,encoding='utf8',timeout=40);assert p.returncode==0,p.stdout+p.stderr
p=subprocess.run([str(copy)],input='q\n\n',env=env,capture_output=True,text=True,encoding='utf8',timeout=40);assert p.returncode==0,p.stdout+p.stderr
assert not list(T.rglob('upload.log'))
meta.update(engine_host_cases=result.testsRun,baud_selection='PASS_1500000_FALLBACK_921600_RESTORE_FAIL_CLOSED',portable_unicode_no_python_PATH='PASS',normal_Q_before_COM='PASS',internal_speed_probe_dispatch='PASS_FROZEN_HELP',physical_public_exe='NOT_RUN')
(B/'LOGS/REPORTS/PUBLIC_8048_20261022.json').write_text(json.dumps(meta,indent=2),encoding='utf8');print('PASS_PUBLIC_8048_ENGINE_PORTABLE_BAUD_NO_COM')
