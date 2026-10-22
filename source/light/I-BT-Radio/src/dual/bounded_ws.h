#pragma once
#include <stddef.h>
#include <stdint.h>
namespace ibtlimit {
// Incremental masked, FIN-only client frames. Owns the terminator; never writes
// beyond AsyncTCP's packet. Browser commands fit in 512 bytes. No allocation.
class WsFrame {
public:
    enum Result { More, Ready, Invalid };
    uint8_t payload[513] = {};
    uint16_t length = 0;
    uint8_t opcode = 0;
    Result feed(uint8_t b) {
        if (!body) {
            if (used >= sizeof(header)) return Invalid;
            header[used++] = b;
            if (used == 2) {
                if ((header[0]&0xF0)!=0x80 || !(header[1]&0x80)) return Invalid;
                opcode=header[0]&15;
                if (opcode!=1 && opcode!=8 && opcode!=9 && opcode!=10) return Invalid;
                uint8_t n=header[1]&127;
                if (n==127 || (opcode>=8 && n>125)) return Invalid;
                needed = n==126 ? 8 : 6;
            }
            if (used!=needed) return More;
            const uint8_t n=header[1]&127;
            length=n==126 ? uint16_t((uint16_t(header[2])<<8)|header[3]) : n;
            if (length>512 || (n==126 && length<126)) return Invalid;
            maskAt=needed-4; body=true;
            if (!length) { payload[0]=0; return Ready; }
            return More;
        }
        if (position>=length) return Invalid;
        payload[position]=b^header[maskAt+(position%4)];
        position++;
        if (position==length) { payload[length]=0; return Ready; }
        return More;
    }
    void consume() { used=position=0; needed=2; body=false; length=0; }
private:
    uint8_t header[8] = {}, used=0, needed=2, maskAt=0;
    uint16_t position=0;
    bool body=false;
};
}
