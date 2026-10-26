from pathlib import Path
Import("env")
p=Path(env.subst('$PROJECT_DIR'))/('sdkconfig.'+env['PIOENV'])
s=p.read_text();four=env['PIOENV'] in ('lite4','psram4');size='4MB' if four else '16MB';part='partitions/lite4.csv' if four else 'partitions/small16.csv'
if env['PIOENV'] in ('lite4','lite16'):
    # No application SPP service exists. Keep A2DP, AVRCP, pairing and all PSRAM profiles.
    updated=s.replace('CONFIG_BT_SPP_ENABLED=y','# CONFIG_BT_SPP_ENABLED is not set')
    if updated!=s:p.write_text(updated)
    s=updated
assert ('CONFIG_SPIRAM=y' in s)==env['PIOENV'].startswith('psram')
assert 'CONFIG_BT_ENABLED=y' in s
assert 'CONFIG_ESPTOOLPY_FLASHSIZE_'+size+'=y' in s
assert 'CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="'+part+'"' in s

# Dedicated Light4 SDK lifetime correction, reproducible for ordinary builds.
# Other profiles retain their original SDK sources when switching environments.
import runpy
project=Path(env.subst('$PROJECT_DIR'))
fix=runpy.run_path(str(project/'scripts/btu_shutdown_patch.py'))
sdk=project.parents[1]/'TOOLS/platformio/packages/framework-espidf/components/bt'
control=project/'scripts/sdk_control'
if env['PIOENV']=='lite4':
    fix['apply'](sdk,control)
else:
    for name,relative in fix['FILES'].items():
        path=sdk/relative;original=(control/('r225_'+name)).read_text()
        actual=path.read_text();patched=fix['transform'](name,original)
        if actual not in (original,patched):raise RuntimeError('Unrecognized SDK source: '+str(path))
        if actual!=original:path.write_text(original)


# R233 link-close request runs on BTA owner; other editions keep stock close.
link=runpy.run_path(str(project/'scripts/bt_link_close_patch.py'))
if env['PIOENV']=='lite4':
    link['apply'](sdk,control)
else:
    for name,relative in link['FILES'].items():
        path=sdk/relative;original=(control/('r233_'+name)).read_text(encoding='utf8')
        actual=path.read_text(encoding='utf8');patched=link['transform'](name,original)
        if actual not in (original,patched):raise RuntimeError('Unrecognized SDK source: '+str(path))
        if actual!=original:path.write_text(original,encoding='utf8')

# R247 Light4 only: UART ISR is cache-dependent already; no IRAM ISR may
# call ringbuffer ISR APIs. Keep the driver's IRAM option off explicitly.
if env['PIOENV']=='lite4':
    s=p.read_text()
    assert 'CONFIG_UART_ISR_IN_IRAM=y' not in s # Kconfig may omit default-n lines
    old='# CONFIG_RINGBUF_PLACE_ISR_FUNCTIONS_INTO_FLASH is not set'
    updated=s.replace(old,'CONFIG_RINGBUF_PLACE_ISR_FUNCTIONS_INTO_FLASH=y')
    if updated!=s:p.write_text(updated)
