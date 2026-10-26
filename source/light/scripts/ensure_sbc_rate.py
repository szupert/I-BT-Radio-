from pathlib import Path
import runpy
Import('env')
project=Path(env.subst('$PROJECT_DIR'))
sdk=Path(env.PioPlatform().get_package_dir('framework-espidf'))/'components/bt/host/bluedroid/btc/profile/std/a2dp'
module=runpy.run_path(str(project/'scripts/sbc_rate_patch.py'))
module['apply'](sdk,project/'scripts/sdk_control')
scratch=runpy.run_path(str(project/'scripts/sbc_scratch_overlay.py'))
scratch['generate'](sdk/'btc_a2dp_source.c',Path(env.subst('$BUILD_DIR'))/'ibt_bt_source_light.c')
avdt=runpy.run_path(str(project/'scripts/avdt_command_overlay.py'))
avdt['generate'](sdk.parents[3]/'stack/avdt/avdt_ccb_act.c',
                 Path(env.subst('$BUILD_DIR'))/'ibt_avdt_ccb_light.c',
                 project/'scripts/sdk_control/r243_avdt_ccb_act.c')
transport=runpy.run_path(str(project/'scripts/bt_transport_overlay.py'))
transport['generate'](sdk.parents[3]/'bta/av/bta_av_aact.c',
                      Path(env.subst('$BUILD_DIR'))/'ibt_bt_transport_light.c',
                      project/'scripts/sdk_control/r244_bta_av_aact.c')

control=runpy.run_path(str(project/'scripts/bt_control_memory_overlay.py'))
for relative,filename,kind in [
    ('stack/l2cap/l2c_utils.c','l2c_utils','l2cap'),
    ('bta/av/bta_av_act.c','bta_av_act','bta'),
    ('stack/avrc/avrc_sdp.c','avrc_sdp','avrc')]:
    control['generate'](sdk.parents[3]/relative,
                        Path(env.subst('$BUILD_DIR'))/('ibt_'+filename+'_light.c'),
                        project/'scripts/sdk_control'/('r245_'+filename+'.c'),kind)

response=runpy.run_path(str(project/'scripts/avrc_response_overlay.py'))
response['generate'](sdk.parents[3]/'stack/avrc/avrc_bld_tg.c',
                     Path(env.subst('$BUILD_DIR'))/'ibt_avrc_bld_tg_light.c',
                     project/'scripts/sdk_control/r246_avrc_bld_tg.c')
guard=runpy.run_path(str(project/'scripts/l2cap_guard_patch.py'))
guard['apply'](sdk.parents[3]/'stack/l2cap/l2c_link.c',project/'scripts/sdk_control/l2c_link.c')

arduino=Path(env.PioPlatform().get_package_dir("framework-arduinoespressif32"))
cancel=runpy.run_path(str(project/"scripts/stream_cancel_patch.py"))
cancel["apply"](arduino/"libraries/NetworkClientSecure/src/ssl_client.cpp",project/"scripts/sdk_control/ssl_client.cpp")

media=runpy.run_path(str(project/'scripts/media_transport_overlay.py'))
for relative,kind in [('stack/avdt/avdt_scb_act.c','avdt_scb_act'),('hci/hci_layer.c','hci_layer')]:
 media['generate'](sdk.parents[3]/relative,Path(env.subst('$BUILD_DIR'))/('ibt_'+kind+'_light.c'),project/'scripts/sdk_control'/('r249_'+kind+'.c'),kind)

sdp=runpy.run_path(str(project/"scripts/sdp_request_overlay.py"))
sdp["generate"](sdk.parents[3]/"stack/sdp/sdp_discovery.c",Path(env.subst("$BUILD_DIR"))/"ibt_sdp_discovery_light.c",project/"scripts/sdk_control/r251_sdp_discovery.c")

response_storage=runpy.run_path(str(project/"scripts/sdp_response_overlay.py"))
response_storage["generate"](Path(env.subst("$BUILD_DIR"))/"ibt_sdp_discovery_light.c")
