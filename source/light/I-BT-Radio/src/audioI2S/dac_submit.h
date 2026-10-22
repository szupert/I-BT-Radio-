#pragma once
#include <stddef.h>
#include <stdint.h>

namespace dacsubmit {
struct Result { size_t bytes; bool complete; };
// The writer owns its blocking limit. Retry only the unaccepted suffix of
// already processed PCM: never run filters/gain/fade twice on a short write.
template<class Writer, class Clock>
constexpr Result all(const uint8_t* data, size_t size, Writer writer, Clock clock,
                     uint32_t budgetMs=250) {
    size_t done=0;
    const uint32_t start=clock();
    while(done<size) {
        size_t accepted=0;
        const bool ok=writer(data+done,size-done,accepted);
        if(accepted>size-done)return {done,false};
        done+=accepted;
        if(!ok)return {done,false};
        if(done==size)return {done,true};
        if(uint32_t(clock()-start)>=budgetMs)return {done,false};
    }
    return {done,true};
}
}
