idf_component_register(SRCS
    "main.cpp"
    "core/commandhandler.cpp"
    "core/config.cpp"
    "core/controls.cpp"
    "core/display.cpp"
    "core/mqtt.cpp"
    "core/netserver.cpp"
    "core/network.cpp"
    "core/player.cpp"
    "core/rtcsupport.cpp"
    "core/sdmanager.cpp"
    "core/telnet.cpp"
    "core/timekeeper.cpp"
    "core/touchscreen.cpp"
    "audioI2S/aac_decoder/aac_decoder.cpp"
    "audioI2S/Audio.cpp"
    "audioI2S/extra_decoder.cpp"
    "audioI2S/net_prefetch.cpp"
    "audioI2S/direct_rx_probe.cpp"
    "audioI2S/flac_decoder/flac_decoder.cpp"
    "audioI2S/mp3_decoder/mp3_decoder.cpp"
    "AsyncWebServer/AsyncEventSource.cpp"
    "AsyncWebServer/AsyncTCP.cpp"
    "AsyncWebServer/AsyncWebSocket.cpp"
    "AsyncWebServer/SPIFFSEditor.cpp"
    "AsyncWebServer/WebAuthentication.cpp"
    "AsyncWebServer/WebHandlers.cpp"
    "AsyncWebServer/WebRequest.cpp"
    "AsyncWebServer/WebResponses.cpp"
    "AsyncWebServer/WebServer.cpp"
    "yoEncoder/yoEncoder.cpp"
    "boards/hal.cpp"
    "dual/artwork.cpp"
    "dual/backup_sd.cpp"
    "dual/core.cpp"
    "dual/dns_auto.cpp"
    "dual/ir_receiver.cpp"
    "dual/weather.cpp"
    "dual/weather_light_decode.cpp"
    "dual/sensors/receiver.cpp"
    "dual/sensors/direct_reader.cpp"
    "dual/r10_diag.cpp"
    "dual/input_low_probe.cpp"
    "dual/sbc_probe.cpp"
    "dual/load_probe.cpp"
    "dual/audio_path_probe.cpp"
    "dual/mono_dsp.cpp"
    "dual/stage_cpu.cpp"
    "dual/mp3_offload.cpp"
    "dual/deferred_audio.cpp"
    "dual/online_artwork.cpp"
    "dual/artwork_ab.cpp"
    "dual/bt_audio.cpp"
    "dual/l2cap_guard_diag.cpp"
    "dual/bt_media.cpp"
    "dual/resource_limiter.cpp"
    "dual/station_save_control.cpp"
    "dual/logic.cpp"
    "ui/small/frontend_small.cpp"
    "ui/boot_custom.cpp"
    "pluginsManager/pluginsManager.cpp"
    "dual/v026_probe.cpp"
    "dual/v027_himem.cpp"
    INCLUDE_DIRS "." ".." REQUIRES json ibt_extra_codecs espressif__network_provisioning espressif__mdns protocomm protobuf-c)

if(CONFIG_BT_BLE_ENABLED)
    set(IBT_THEENGS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../THIRD_PARTY/TheengsDecoder/src")
    target_sources(${COMPONENT_LIB} PRIVATE "dual/sensors/sensor_decoder.cpp")
    if(CONFIG_SPIRAM)
        target_sources(${COMPONENT_LIB} PRIVATE "${IBT_THEENGS_ROOT}/decoder.cpp")
    else()
        target_compile_definitions(${COMPONENT_LIB} PRIVATE IBT_BLE_NATIVE_ONLY=1)
    endif()
    target_include_directories(${COMPONENT_LIB} PRIVATE "${IBT_THEENGS_ROOT}" "${IBT_THEENGS_ROOT}/arduino_json/src")
endif()

# V136: one-variable MP3 speed trial; keep the SDK and other codecs at -Os.
set_source_files_properties("audioI2S/mp3_decoder/mp3_decoder.cpp"
    PROPERTIES COMPILE_OPTIONS "-O2")


set_source_files_properties("dual/core.cpp" "dual/ir_receiver.cpp" "ui/small/frontend_small.cpp" "ui/boot_custom.cpp" "dual/online_artwork.cpp" "dual/dns_auto.cpp" "dual/weather.cpp" "dual/weather_light_decode.cpp" PROPERTIES COMPILE_OPTIONS "-fstack-usage")
