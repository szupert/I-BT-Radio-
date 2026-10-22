#pragma once
#include <stdint.h>
#include <stddef.h>

namespace ibtplaylist {
// Logical offsets refer to consumed bytes, not the source cursor after read-ahead.
// Pause runs after each bounded read has returned, outside filesystem locks.
template<class Source, class Pause> class IndexReader {
    Source& source;
    Pause pause;
    uint8_t buffer[128];
    size_t at=0, size=0;
    uint32_t offset=0, blocks=0;
    bool failed=false;
    int next() {
        if(failed)return -2;
        if(at==size){
            if(source.available()<=0)return -1;
            const int count=source.read(buffer,sizeof(buffer));
            if(count<=0 || size_t(count)>sizeof(buffer)){failed=true;return -2;}
            at=0;size=size_t(count);++blocks;
            pause();
        }
        if(offset==UINT32_MAX){failed=true;return -2;}
        ++offset;
        return buffer[at++];
    }
public:
    enum Result { End, Line, Error };
    IndexReader(Source& input,Pause yield):source(input),pause(yield){}
    uint32_t position() const {return offset;}
    uint32_t readBlocks() const {return blocks;}
    Result readLine(char* line,size_t capacity,size_t& used,bool& overflow) {
        used=0;overflow=false;
        if(!line || !capacity)return Error;
        bool any=false;
        for(;;){
            const int ch=next();
            if(ch==-2)return Error;
            if(ch==-1){if(!any)return End;break;}
            any=true;
            if(ch=='\n')break;
            if(ch==0 || used>=capacity-1)overflow=true;
            else line[used++]=char(ch);
        }
        if(used && line[used-1]=='\r')--used;
        line[used]=0;
        return Line;
    }
};
}
