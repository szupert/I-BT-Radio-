idf_component_register(SRCS "memory.c" "catalog.c"
    INCLUDE_DIRS "include" "include/decoder" "include/decoder/impl" "include/simple_dec"
    REQUIRES heap)
if(NOT CONFIG_IDF_TARGET_ESP32)
    message(FATAL_ERROR "The experimental I-BT-Radio extra-codec archive is pinned to classic ESP32")
endif()
# Scope allocator substitution to this pinned archive, never the SDK/Wi-Fi/BT.
set(IBT_AUDIO_ARCHIVE "${CMAKE_CURRENT_BINARY_DIR}/libibt_audio_codec.a")
execute_process(COMMAND ${CMAKE_OBJCOPY}
    --redefine-sym malloc=ibt_codec_malloc --redefine-sym calloc=ibt_codec_calloc
    --redefine-sym realloc=ibt_codec_realloc --redefine-sym free=ibt_codec_free
    --redefine-sym strdup=ibt_codec_strdup --redefine-sym _Znwj=ibt_codec_new
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/esp32/libesp_audio_codec.a" "${IBT_AUDIO_ARCHIVE}"
    RESULT_VARIABLE IBT_COPY_RESULT)
if(NOT IBT_COPY_RESULT EQUAL 0)
    message(FATAL_ERROR "Pinned codec allocator substitution failed")
endif()
add_prebuilt_library(ibt_acodec "${IBT_AUDIO_ARCHIVE}")
set(IBT_SIMPLE_ARCHIVE "${CMAKE_CURRENT_BINARY_DIR}/libibt_audio_simple_dec.a")
execute_process(COMMAND ${CMAKE_OBJCOPY}
    --redefine-sym malloc=ibt_codec_malloc --redefine-sym calloc=ibt_codec_calloc
    --redefine-sym realloc=ibt_codec_realloc --redefine-sym free=ibt_codec_free
    --redefine-sym strdup=ibt_codec_strdup --redefine-sym _Znwj=ibt_codec_new
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/esp32/libesp_audio_simple_dec.a" "${IBT_SIMPLE_ARCHIVE}"
    RESULT_VARIABLE IBT_SIMPLE_RESULT)
if(NOT IBT_SIMPLE_RESULT EQUAL 0)
    message(FATAL_ERROR "Pinned simple-codec allocator substitution failed")
endif()
add_prebuilt_library(ibt_sdec "${IBT_SIMPLE_ARCHIVE}")
target_link_libraries(${COMPONENT_LIB} PUBLIC "-Wl,--start-group" ibt_sdec ibt_acodec "-Wl,--end-group")
