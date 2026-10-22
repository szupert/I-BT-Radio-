"""Keep ESP-IDF's flash geometry aligned with PlatformIO's 16 MB profile."""

from pathlib import Path
import re

Import("env")

config_path = Path(env.subst("$PROJECT_DIR")) / f"sdkconfig.{env['PIOENV']}"
existing = config_path.read_text(encoding="utf-8") if config_path.exists() else ""
required = (
    'CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y',
    'CONFIG_ESPTOOLPY_FLASHSIZE="16MB"',
    'CONFIG_PARTITION_TABLE_CUSTOM=y',
    'CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions/small16.csv"',
    'CONFIG_PARTITION_TABLE_FILENAME="partitions/small16.csv"',
)
if not all(line in existing.splitlines() for line in required):
    obsolete = re.compile(
        r"(?:# )?CONFIG_(?:ESPTOOLPY_FLASHSIZE(?:_[A-Z0-9]+)?|"
        r"PARTITION_TABLE_(?:CUSTOM|CUSTOM_FILENAME|FILENAME))(?:=.*| is not set)$"
    )
    kept = [line for line in existing.splitlines() if not obsolete.fullmatch(line)]
    config_path.write_text("\n".join(kept + list(required)) + "\n", encoding="utf-8")
    print(f"[IBT] Seeded 16 MB ESP-IDF flash and partition config: {config_path.name}")

# V083 diagnostic build: kernel accounting, wall-clock microseconds, per-core idle.
profile = {'CONFIG_FREERTOS_USE_TRACE_FACILITY':'y',
           'CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS':'y',
           'CONFIG_FREERTOS_RUN_TIME_STATS_USING_ESP_TIMER':'y',
           'CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID':'y'}
# R14 IPv4-only Wi-Fi radio: no modem/PPP or multicast DNS discovery.
# Keep DHCP/DNS/TCP/TLS and the existing task/buffer configuration unchanged.
profile.update({'CONFIG_LWIP_IPV4':'y',
                'CONFIG_LWIP_IPV6':'n',
                'CONFIG_ESP_MODEM_USE_PPP_MODE':'n',
                'CONFIG_LWIP_PPP_SUPPORT':'n',
                'CONFIG_LWIP_DNS_SUPPORT_MDNS_QUERIES':'n'})
# R17: measured Core1 saturation; move Bluedroid host/SBC to Core0.
# Keep controller Core1, decoder Core1 and all SDK priorities unchanged.
profile.update({'CONFIG_BT_BLUEDROID_PINNED_TO_CORE_0':'y',
                'CONFIG_BT_BLUEDROID_PINNED_TO_CORE_1':'n',
                'CONFIG_BTDM_CTRL_PINNED_TO_CORE_0':'n',
                'CONFIG_BTDM_CTRL_PINNED_TO_CORE_1':'y'})
lines=config_path.read_text(encoding='utf-8').splitlines()
for key,value in profile.items():
    if key+'='+value in lines or (value=='n' and '# '+key+' is not set' in lines):
        continue
    lines=[line for line in lines if not line.startswith(key+'=') and line != '# '+key+' is not set']
    lines.append(key+'='+value)
updated='\n'.join(lines)+'\n'
if updated!=config_path.read_text(encoding='utf-8'):
    config_path.write_text(updated,encoding='utf-8')
