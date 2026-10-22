#pragma once
#include <stddef.h>
#include <stdint.h>
namespace v027 {
struct MemoryReport {
    size_t physical, mappedHeap, mapWindow, highTotal, highFree, highTested;
    uint32_t errors;
    const char* highResult;
};
const MemoryReport& memoryReport();
void testHighMemory();
}
