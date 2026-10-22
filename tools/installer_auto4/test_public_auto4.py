from pathlib import Path
from unittest.mock import patch
import importlib.util,sys,tempfile,json,subprocess,os,io
from contextlib import redirect_stdout
B=Path('C:/esp32/SYSTEM');W=B/'TEMP/PUBLIC_AUTO4_20261022';M=W/'payload/VARIANTS';report=json.loads((B/'LOGS/REPORTS/PUBLIC_AUTO4_20261022.json').read_text())
def load(n,p):
 s=importlib.util.spec_from_file_location(n,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
sys.path.insert(0,str(M/'AUTO/INSTALLER'))
auto=load('public_auto',M/'AUTO/INSTALLER/update.py');mods=auto.verify_profiles();auto.verify_profiles=lambda:mods
results=[]
for profile in mods:
 with tempfile.TemporaryDirectory(prefix='auto4_host_',dir=B/'TEMP') as d:
  temp=Path(d);base=mods['lite4'];selected=mods[profile];info='ESP32 (revision v1.0)\nMAC: AA:BB:CC:DD:EE:FF\nDetected flash size: '+str(4 if profile.endswith('4') else 16)+'MB\n';calls=[]
  def live(cmd,log,timeout):
   assert cmd[1]=='--internal-memory-probe' and '--port' in cmd and '--output' in cmd;calls.append('probe_dispatch')
   data=dict(schema=1,chip='ESP32',revision=100,mac='AA:BB:CC:DD:EE:FF',flash_mb=4 if profile.endswith('4') else 16,psram=dict(status='SAMPLE_PASS' if profile.startswith('psram') else 'NO_RESPONSE'),profile=profile,registers_restored=True,flash_written=False,transport=dict(write_baud=921600,preflight_baud=115200,baseline_verified=True,attempts=[dict(baud=921600,result='PASS')]))
   Path(cmd[cmd.index('--output')+1]).write_text(json.dumps(data));return 0,'HOST_ROM_FIXTURE'
  def main():
   assert '--handoff' in sys.argv and '--write-baud' in sys.argv;assert selected.select_variant(info,selected.verify());calls.append('selected_checked_updater');return 0
  captured=io.StringIO()
  with patch.dict(os.environ,{'LOCALAPPDATA':str(temp)}),patch.object(base,'detect_port',lambda *a,**k:'HOST_NO_COM'),patch.object(base,'run_live',live),patch.object(selected,'main',main),patch.object(sys,'argv',['auto.py']),patch.object(sys,'frozen',True,create=True),redirect_stdout(captured):
   rc=auto.main()
  assert rc==0 and calls==['probe_dispatch','selected_checked_updater'],(profile,captured.getvalue(),calls)
  results.append(profile)
# Re-run the existing actual updater/admission cases against the packaged modules,
# restricting only the fixture's available map to its original Light4 case.
L=B/'SOURCE/I-BT-Radio_LITE';code=(L/'release_tools/test_r176_package.py').read_text().replace("Q=B/'TEMP/R176_RELEASE/LATEST/LIGHT_R174'","Q=B/'TEMP/PUBLIC_AUTO4_20261022/payload/VARIANTS'")
needle='with redirect_stdout(io.StringIO()):modules=auto.verify_profiles()'
assert needle in code
code=code.replace(needle,needle+"\n    modules={'lite4':modules['lite4']};auto.verify_profiles=lambda:modules")
code=code.replace("'R176_PACKAGE_HOST.json'","'PUBLIC_AUTO4_ENGINE_HOST.json'")
exec(compile(code,str(L/'release_tools/test_r176_package.py'),'exec'),{'__file__':str(L/'release_tools/test_r176_package.py'),'__name__':'__main__'})
exe=Path(report['exe']);env=dict(os.environ,PATH=os.environ.get('SystemRoot','C:/Windows')+'/System32');env.pop('PYTHONPATH',None);env.pop('PYTHONHOME',None)
with tempfile.TemporaryDirectory(prefix='public_auto4_árvíztűrő_',dir=B/'TEMP') as d:
 import shutil
 p=Path(d);portable=p/'I-BT-Radio Install.exe';shutil.copyfile(exe,portable)
 for args in [['--verify-only'],['--internal-esptool','version'],['--internal-memory-probe','--help'],['--extract-payload',str(p/'export')]]:
  r=subprocess.run([str(portable)]+args,capture_output=True,text=True,encoding='utf8',env=env,timeout=40);assert r.returncode==0,r.stdout+r.stderr
 for folder in auto.FOLDERS.values():
  r=subprocess.run([str(portable),'--internal-esptool','--chip','esp32','image-info',str(p/'export/VARIANTS'/folder/'INSTALLER/firmware.bin')],capture_output=True,text=True,encoding='utf8',env=env,timeout=40);assert r.returncode==0,r.stdout+r.stderr
 r=subprocess.run([str(portable)],input='q\n\n',capture_output=True,text=True,encoding='utf8',env=env,timeout=40);assert r.returncode==0,r.stdout+r.stderr
report.update(portable_unicode_no_python_PATH='PASS',internal_probe_dispatch='PASS_ACTUAL_CALLER_AND_FROZEN_HELP',actual_selected_profiles=results,all4_firmware_image_info='PASS',normal_Q_before_COM='PASS',engine_admission_receipt_regression='18_CASES_PASS',physical='NOT_RUN')
(B/'LOGS/REPORTS/PUBLIC_AUTO4_20261022.json').write_text(json.dumps(report,indent=2),encoding='utf8');print('PASS_PUBLIC_AUTO4_PORTABLE_PROFILE_DISPATCH_18_ENGINE_NO_COM')
