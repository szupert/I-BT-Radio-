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

# Offline component-manager mode: Arduino public headers need these manifest dependencies.
# Apply to the Arduino app target only; never pollute bootloader include paths.
if(NOT BOOTLOADER_BUILD)
  idf_component_get_property(lite_arduino_lib framework-arduinoespressif32 COMPONENT_LIB)
  target_link_libraries(${lite_arduino_lib} PUBLIC idf::espressif__network_provisioning idf::espressif__mdns idf::protocomm idf::protobuf-c)
endif()
