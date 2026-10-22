#pragma once
#include <esp_heap_caps.h>
namespace litememory {
#if IBT_NO_PSRAM
constexpr unsigned payloadCaps=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
constexpr unsigned frameInterval=200;
#else
constexpr unsigned payloadCaps=MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT;
constexpr unsigned frameInterval=50;
#endif
}
