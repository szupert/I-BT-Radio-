#pragma once
#include <cstddef>
#include <cstdint>
#include <esp_heap_caps.h>
#include "../dual/resource_policy.h"
namespace lightpayload {
inline void* zero(size_t bytes){
 constexpr unsigned caps=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
 constexpr size_t reserve=ibtlimit::Policy::reserve,overhead=64;
 if(!bytes||bytes>SIZE_MAX-reserve-overhead||heap_caps_get_free_size(caps)<bytes+reserve+overhead||heap_caps_get_largest_free_block(caps)<bytes+overhead)return nullptr;
 void*p=heap_caps_calloc(bytes,1,caps);
 if(p&&heap_caps_get_free_size(caps)<reserve){heap_caps_free(p);return nullptr;}
 return p;
}
}
