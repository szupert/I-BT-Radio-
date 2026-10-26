cmake_minimum_required(VERSION 3.16)
# Offline deterministic reuse of the already pinned local components.
set(ENV{IDF_COMPONENT_MANAGER} "0")
set(local_managed
 "${CMAKE_CURRENT_LIST_DIR}/managed_components/chmorgan__esp-libhelix-mp3"
 "${CMAKE_CURRENT_LIST_DIR}/managed_components/espressif__cbor"
 "${CMAKE_CURRENT_LIST_DIR}/managed_components/espressif__esp-dsp"
 "${CMAKE_CURRENT_LIST_DIR}/managed_components/espressif__esp-zboss-lib"
 "${CMAKE_CURRENT_LIST_DIR}/managed_components/espressif__esp-zigbee-lib"
 "${CMAKE_CURRENT_LIST_DIR}/managed_components/espressif__lan867x"
 "${CMAKE_CURRENT_LIST_DIR}/managed_components/espressif__lan86xx_common"
 "${CMAKE_CURRENT_LIST_DIR}/managed_components/espressif__libsodium"
 "${CMAKE_CURRENT_LIST_DIR}/managed_components/espressif__mdns"
 "${CMAKE_CURRENT_LIST_DIR}/managed_components/espressif__network_provisioning"
 "${CMAKE_CURRENT_LIST_DIR}/managed_components/espressif__qrcode"
 "${CMAKE_CURRENT_LIST_DIR}/managed_components/joltwallet__littlefs"
)
list(APPEND EXTRA_COMPONENT_DIRS ${local_managed})
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ibt_small_lite_r167)

# Generated, version-locked Light4 source override; other profiles retain SDK source.
get_filename_component(ibt_build_profile "${CMAKE_BINARY_DIR}" NAME)
if(ibt_build_profile STREQUAL "lite4" AND NOT BOOTLOADER_BUILD)
  idf_component_get_property(ibt_bt_lib bt COMPONENT_LIB)
  # R247: the Light uses ATT(4), LE signaling(5), SMP(6) and BR SMP(7).
  # Keep every used fixed CID; spare CID slots8..35 are not services.
  # BLE thermometer uses fixed ATT, not15 simultaneous LE CoC registrations.
  # Apply consistently to the entire BT target (including generated units):
  # private structure layouts must never differ between BT compilation units.
  target_compile_definitions(${ibt_bt_lib} PRIVATE
    L2CAP_NUM_FIXED_CHNLS=4 BLE_MAX_L2CAP_CLIENTS=1)

  get_target_property(ibt_bt_sources ${ibt_bt_lib} SOURCES)
  set(ibt_bt_source_found FALSE)
  foreach(ibt_bt_source IN LISTS ibt_bt_sources)
    if(ibt_bt_source MATCHES "btc_a2dp_source[.]c$")
      list(REMOVE_ITEM ibt_bt_sources "${ibt_bt_source}")
      set(ibt_bt_source_found TRUE)
    endif()
  endforeach()
  if(NOT ibt_bt_source_found OR NOT EXISTS "${CMAKE_BINARY_DIR}/ibt_bt_source_light.c")
    message(FATAL_ERROR "Light A2DP source overlay missing or SDK target changed")
  endif()
  set_property(TARGET ${ibt_bt_lib} PROPERTY SOURCES ${ibt_bt_sources})
  target_sources(${ibt_bt_lib} PRIVATE "${CMAKE_BINARY_DIR}/ibt_bt_source_light.c")
  set(ibt_avdt_found FALSE)
  foreach(ibt_bt_source IN LISTS ibt_bt_sources)
    if(ibt_bt_source MATCHES "avdt_ccb_act[.]c$")
      list(REMOVE_ITEM ibt_bt_sources "${ibt_bt_source}")
      set(ibt_avdt_found TRUE)
    endif()
  endforeach()
  if(NOT ibt_avdt_found OR NOT EXISTS "${CMAKE_BINARY_DIR}/ibt_avdt_ccb_light.c")
    message(FATAL_ERROR "Light AVDTP command overlay missing or SDK target changed")
  endif()
  set(ibt_transport_found FALSE)
  foreach(ibt_bt_source IN LISTS ibt_bt_sources)
    if(ibt_bt_source MATCHES "bta_av_aact[.]c$")
      list(REMOVE_ITEM ibt_bt_sources "${ibt_bt_source}")
      set(ibt_transport_found TRUE)
    endif()
  endforeach()
  if(NOT ibt_transport_found OR NOT EXISTS "${CMAKE_BINARY_DIR}/ibt_bt_transport_light.c")
    message(FATAL_ERROR "Light AVDT transport overlay missing or SDK changed")
  endif()
  # One compilation unit per SDK component; shared SDK and other profiles unchanged.
  set_property(TARGET ${ibt_bt_lib} PROPERTY SOURCES ${ibt_bt_sources})
  target_sources(${ibt_bt_lib} PRIVATE "${CMAKE_BINARY_DIR}/ibt_bt_source_light.c" "${CMAKE_BINARY_DIR}/ibt_avdt_ccb_light.c" "${CMAKE_BINARY_DIR}/ibt_bt_transport_light.c")
  target_include_directories(${ibt_bt_lib} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/I-BT-Radio/src")
  # R245: exact-size command allocation and failed SDP startup cleanup.
  get_target_property(ibt_control_sources ${ibt_bt_lib} SOURCES)
  foreach(ibt_control_name IN ITEMS l2c_utils bta_av_act avrc_sdp)
    set(ibt_control_found FALSE)
    foreach(ibt_control_source IN LISTS ibt_control_sources)
      if(ibt_control_source MATCHES "/${ibt_control_name}[.]c$")
        list(REMOVE_ITEM ibt_control_sources "${ibt_control_source}")
        set(ibt_control_found TRUE)
      endif()
    endforeach()
    if(NOT ibt_control_found OR NOT EXISTS "${CMAKE_BINARY_DIR}/ibt_${ibt_control_name}_light.c")
      message(FATAL_ERROR "Light control memory overlay missing or SDK changed: ${ibt_control_name}")
    endif()
    list(APPEND ibt_control_sources "${CMAKE_BINARY_DIR}/ibt_${ibt_control_name}_light.c")
  endforeach()
  set_property(TARGET ${ibt_bt_lib} PROPERTY SOURCES ${ibt_control_sources})
  set(ibt_response_found FALSE)
  foreach(ibt_response_source IN LISTS ibt_control_sources)
    if(ibt_response_source MATCHES "/avrc_bld_tg[.]c$")
      list(REMOVE_ITEM ibt_control_sources "${ibt_response_source}")
      set(ibt_response_found TRUE)
    endif()
  endforeach()
  if(NOT ibt_response_found OR NOT EXISTS "${CMAKE_BINARY_DIR}/ibt_avrc_bld_tg_light.c")
    message(FATAL_ERROR "Light AVRCP response overlay missing or SDK changed")
  endif()
  set_property(TARGET ${ibt_bt_lib} PROPERTY SOURCES ${ibt_control_sources})
  target_sources(${ibt_bt_lib} PRIVATE "${CMAKE_BINARY_DIR}/ibt_avrc_bld_tg_light.c")
  get_target_property(ibt_media_sources ${ibt_bt_lib} SOURCES)
  foreach(ibt_media_name IN ITEMS avdt_scb_act hci_layer sdp_discovery)
    set(ibt_media_found FALSE)
    foreach(ibt_media_source IN LISTS ibt_media_sources)
      if(ibt_media_source MATCHES "/${ibt_media_name}[.]c$")
        list(REMOVE_ITEM ibt_media_sources "${ibt_media_source}")
        set(ibt_media_found TRUE)
      endif()
    endforeach()
    if(NOT ibt_media_found OR NOT EXISTS "${CMAKE_BINARY_DIR}/ibt_${ibt_media_name}_light.c")
      message(FATAL_ERROR "Light media transport overlay missing or SDK changed: ${ibt_media_name}")
    endif()
    list(APPEND ibt_media_sources "${CMAKE_BINARY_DIR}/ibt_${ibt_media_name}_light.c")
  endforeach()
  set_property(TARGET ${ibt_bt_lib} PROPERTY SOURCES ${ibt_media_sources})

endif()

# Offline component-manager mode: Arduino public headers need these manifest dependencies.
# Apply to the Arduino app target only; never pollute bootloader include paths.
if(NOT BOOTLOADER_BUILD)
  idf_component_get_property(lite_arduino_lib framework-arduinoespressif32 COMPONENT_LIB)
  target_link_libraries(${lite_arduino_lib} PUBLIC idf::espressif__network_provisioning idf::espressif__mdns idf::protocomm idf::protobuf-c)
endif()
