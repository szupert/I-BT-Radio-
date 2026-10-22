from pathlib import Path
import runpy
Import('env')
project=Path(env.subst('$PROJECT_DIR'))
sdk=Path(env.PioPlatform().get_package_dir('framework-espidf'))/'components/bt/host/bluedroid/btc/profile/std/a2dp'
module=runpy.run_path(str(project/'scripts/sbc_rate_patch.py'))
module['apply'](sdk,project/'scripts/sdk_control')
guard=runpy.run_path(str(project/'scripts/l2cap_guard_patch.py'))
guard['apply'](sdk.parents[3]/'stack/l2cap/l2c_link.c',project/'scripts/sdk_control/l2c_link.c')
