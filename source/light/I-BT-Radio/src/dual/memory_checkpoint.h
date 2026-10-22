#pragma once
#include "../core/options.h"
#if IBT_NO_PSRAM
#include <Arduino.h>
#include <esp_heap_caps.h>
namespace lightmem {
// A historical drop observed during a scope is a correlation, not an allocator
// ownership claim: the other core/driver can allocate concurrently. No heap
// trace buffer or heap-monitor reset, and no output on the normal hot path.
struct Phase {
 const char* name;uint32_t before;
 explicit Phase(const char* n):name(n),before(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)){}
 ~Phase(){const uint32_t after=heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
  if(after<before&&Serial.availableForWrite()>=224){
   // Print::printf allocates above64B in this SDK. Format the rare event in a
   // bounded stack buffer so diagnostics do not create another heap dip.
   char line[224];const int n=snprintf(line,sizeof(line),"[LIGHT-MEM-DIP] phase=%.28s before=%u after=%u current=%u core=%d task=%.16s scope=REGIONAL_HISTORY_CORRELATION_NOT_OWNER\n",name,unsigned(before),unsigned(after),unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)),xPortGetCoreID(),pcTaskGetName(nullptr));
   if(n>0&&unsigned(n)<sizeof(line))Serial.write(reinterpret_cast<const uint8_t*>(line),size_t(n));
  }
 }
};
// Historical value is a sum of regional low-water marks, not a simultaneous
// minimum. Never reset it or use it in place of current admission headroom.
inline void checkpoint(const char* phase){
 constexpr uint32_t caps=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
 Serial.printf("[LIGHT-MEM] phase=%s free=%u largest=%u regional_min_sum=%u\n",phase,
 unsigned(heap_caps_get_free_size(caps)),unsigned(heap_caps_get_largest_free_block(caps)),
 unsigned(heap_caps_get_minimum_free_size(caps)));
}
}
#else
namespace lightmem {struct Phase{explicit Phase(const char*){}};inline void checkpoint(const char*){}}
#endif
