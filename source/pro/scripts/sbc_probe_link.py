"""Diagnostic-only private codec. Rebuilt before link; no SDK modification."""
from pathlib import Path
import subprocess
Import('env')
root=Path('C:/esp32/SYSTEM')
driver=root/'TOOLS/build_sbc_probe.py'
out=root/'LOGS/SBC_WORKER_V1'/env['PIOENV']
obj=out/'sbc_probe.private.o'
env.Append(LINKFLAGS=[str(obj).replace('\\','/')])
env.Depends('$BUILD_DIR/firmware.elf',[str(driver)]+[str(p) for p in (root/'EXPERIMENTS/SBC_WORKER_V1').glob('*') if p.is_file()])
def build_codec(source,target,env):
    subprocess.run([str(root/'TOOLS/platformio/core/penv/Scripts/python.exe'),str(driver),'--private-codec','--build-dir',env.subst('$BUILD_DIR'),'--out-dir',str(out)],check=True)
env.AddPreAction('$BUILD_DIR/firmware.elf',build_codec)
