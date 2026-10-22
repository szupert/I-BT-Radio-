#pragma once
#include <cstdint>
#include <algorithm>
namespace ibtaudio {
// Incremental HTTP chunk framing only. Audio/ICY payload stays opaque.
// A poll consumes at most 64 framing bytes and never waits for a byte.
class LiveChunk {
    enum State {Size,SizeLF,Data,DataCR,DataLF,Trailer,TrailerLF,Done,Error};
    State state_=Size;
    uint32_t size_=0,remaining_=0;
    unsigned digits_=0,line_=0,trailerBytes_=0;
    bool extension_=false;
public:
    void reset(){state_=Size;size_=remaining_=0;digits_=line_=trailerBytes_=0;extension_=false;}
    bool failed()const{return state_==Error;}
    bool done()const{return state_==Done;}
    uint32_t payload(uint32_t available)const{return state_==Data?std::min(remaining_,available):0;}
    bool consume(uint32_t n){
        if(state_!=Data||n>remaining_){state_=Error;return false;}
        remaining_-=n;if(!remaining_)state_=DataCR;return true;
    }
    template<class Read> void poll(Read read){
        for(unsigned budget=0;budget<64 && state_!=Data && state_!=Done && state_!=Error;++budget){
            const int b=read();if(b<0)return;
            switch(state_){
            case Size:
                if(++line_>1024){state_=Error;break;}
                if(b=='\r'){state_=digits_?SizeLF:Error;break;}
                if(b==';'&&digits_){extension_=true;break;}
                if(extension_){if(b<32||b>126)state_=Error;break;}
                {const int v=b>='0'&&b<='9'?b-'0':b>='a'&&b<='f'?b-'a'+10:b>='A'&&b<='F'?b-'A'+10:-1;
                 if(v<0||digits_>=8||size_>(UINT32_MAX-unsigned(v))/16u){state_=Error;break;}
                 size_=size_*16+unsigned(v);++digits_;}break;
            case SizeLF:
                if(b!='\n'){state_=Error;break;}
                remaining_=size_;state_=size_?Data:Trailer;line_=0;break;
            case DataCR:state_=b=='\r'?DataLF:Error;break;
            case DataLF:
                if(b!='\n'){state_=Error;break;}
                state_=Size;size_=digits_=line_=0;extension_=false;break;
            case Trailer:
                if(++trailerBytes_>4096){state_=Error;break;}
                if(b=='\r')state_=TrailerLF;
                else if(b<32||b>126)state_=Error;
                else ++line_;
                break;
            case TrailerLF:
                if(b!='\n'){state_=Error;break;}
                state_=line_?Trailer:Done;line_=0;break;
            default:state_=Error;break;
            }
        }
    }
};
}
