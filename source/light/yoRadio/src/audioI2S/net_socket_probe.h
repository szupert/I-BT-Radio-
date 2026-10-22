#pragma once
#include <stdint.h>
#include <errno.h>
namespace ibtnet {
enum class PeekState : uint8_t { Unseen, Waiting, Data, Eof, Error };
constexpr PeekState classifyPeek(int result,int error){
    return result>0?PeekState::Data:result==0?PeekState::Eof:
        (error==EAGAIN||error==EWOULDBLOCK||error==EINTR)?PeekState::Waiting:PeekState::Error;
}
// Plain TCP only. TLS must retain its own framing/owner, and buffered data
// must be drained before looking for EOF on the underlying socket.
constexpr bool shouldPeek(bool tls,int available,uint32_t now,uint32_t previous){
    return !tls&&available==0&&uint32_t(now-previous)>=1000;
}
constexpr const char* peekName(PeekState state){
    return state==PeekState::Waiting?"WAIT":state==PeekState::Data?"DATA":
           state==PeekState::Eof?"EOF":state==PeekState::Error?"ERROR":"UNSEEN";
}
}
