#pragma once
#include "codec_memory.h"
// Only the explicitly audited integer-only MP3 workspaces may use this pool.
// No byte/halfword/float access, libc memset or DMA is permitted on these blocks.
// Byte-oriented reservoirs, headers and PCM remain in normal DRAM/PSRAM.
static inline void* ibtCodecIntAlloc(size_t bytes) {
#if IBT_NO_PSRAM && defined(CONFIG_IDF_TARGET_ESP32)
    constexpr unsigned caps=MALLOC_CAP_INTERNAL|MALLOC_CAP_EXEC|MALLOC_CAP_32BIT;
    constexpr size_t reserve=ibtram::reserve==0?0:4096;
    // EXEC/32BIT can come from shared D/IRAM, reducing byte-accessible DRAM.
    // A post-allocation rejection alone can already destroy its low-water mark.
    // Keep the final shared10000B DRAM reserve before/after the attempt.
    // In the worst shared-pool case preserve the global Light 10000B floor
    // during the attempt too. Do not charge pure IRAM as permanent DRAM.
    if(bytes%4==0 && bytes<=SIZE_MAX-ibtram::reserve-64 &&
       heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)>=ibtram::reserve &&
       heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)>=bytes+ibtram::reserve+64 &&
       heap_caps_get_free_size(caps)>=bytes+reserve &&
       heap_caps_get_largest_free_block(caps)>=bytes) {
        if(void* p=heap_caps_malloc(bytes,caps)) {
            if(heap_caps_get_free_size(caps)>=reserve &&
               heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)>=ibtram::reserve)return p;
            heap_caps_free(p);
        }
    }
#endif
    return ibtCodecAlloc(bytes); // Same final shared DRAM reserve for every edition.
}
static inline void ibtCodecZeroWords(void* memory,size_t bytes) {
    // Volatile prevents the compiler replacing this with a byte-addressing memset.
    volatile uint32_t* p=static_cast<volatile uint32_t*>(memory);
    for(size_t i=0;i<bytes/sizeof(uint32_t);++i)p[i]=0;
}
