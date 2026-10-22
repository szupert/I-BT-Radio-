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

