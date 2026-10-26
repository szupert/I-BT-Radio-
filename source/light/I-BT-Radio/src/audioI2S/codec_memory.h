#pragma once
#include <cstddef>
#include <cstdint>
#include <esp_heap_caps.h>
#include "../dual/ram_reserve.h"
// Decoder workspaces must not consume the internal reserve needed by Wi-Fi/BT.
// Retain fast internal allocation when there is ample room; otherwise use PSRAM.
static inline void* ibtCodecAlloc(size_t bytes) {
    constexpr size_t reserve = ibtram::reserve;
    if constexpr (reserve == 0) {
        if (void* p=heap_caps_malloc(bytes,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)) return p;
        return heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    }
    constexpr size_t overhead = 64;
    if (bytes <= SIZE_MAX-reserve-overhead &&
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT) >= bytes+reserve+overhead &&
        heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)>=bytes+overhead) {
        if (void* p=heap_caps_malloc(bytes,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)) {
            if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)>=reserve)return p;
            heap_caps_free(p); // Include allocator bookkeeping in the preserved reserve.
        }
    }
    return heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
}
