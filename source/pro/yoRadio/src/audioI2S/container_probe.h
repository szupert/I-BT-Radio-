#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
namespace containerprobe {
enum class M4a {Unknown,Aac,Alac,AlacLate};
inline uint32_t be32(const uint8_t* p){return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];}
// Structural atom walk, bounded independently of file length. Never scan raw
// payload for a codec-looking substring. The caller owns all file operations.
template<class Read> class Mp4 {
    Read& read;uint64_t file;unsigned steps=0,reads=0;bool alac=false,aac=false;
    bool get(uint64_t p,uint8_t* out,size_t n){return p<=file&&n<=file-p&&reads+n<=65536&&(reads+=n,read(p,out,n));}
    bool walk(uint64_t first,uint64_t end,unsigned depth){
        if(depth>6)return false;
        for(uint64_t pos=first;pos<end;){
            if(++steps>512||end-pos<8)return false;
            uint8_t h[16];if(!get(pos,h,8))return false;
            uint64_t length=be32(h),header=8;
            if(length==1){if(end-pos<16||!get(pos+8,h+8,8))return false;length=(uint64_t(be32(h+8))<<32)|be32(h+12);header=16;}
            if(length==0)length=end-pos;
            if(length<header||length>end-pos)return false;
            const uint64_t body=pos+header,limit=pos+length;
            if(!memcmp(h+4,"stsd",4)){
                uint8_t full[8];if(length-header<8||!get(body,full,8))return false;
                uint32_t count=be32(full+4);if(count>16)return false;
                uint64_t entry=body+8;
                for(uint32_t i=0;i<count;++i){
                    uint8_t sample[8];if(entry>limit||limit-entry<8||!get(entry,sample,8))return false;
                    uint32_t n=be32(sample);if(n<8||n>limit-entry)return false;
                    if(!memcmp(sample+4,"alac",4))alac=true;
                    if(!memcmp(sample+4,"mp4a",4))aac=true;
                    entry+=n;
                }
            }else if(!memcmp(h+4,"moov",4)||!memcmp(h+4,"trak",4)||!memcmp(h+4,"mdia",4)||!memcmp(h+4,"minf",4)||!memcmp(h+4,"stbl",4)){
                if(!walk(body,limit,depth+1))return false;
            }
            pos=limit;
        }
        return true;
    }
public:
    Mp4(Read& r,uint64_t n):read(r),file(n){}
    M4a probe(){
        uint64_t moov=file,mdat=file;
        for(uint64_t pos=0;pos<file;){
            if(++steps>512||file-pos<8)return M4a::Unknown;
            uint8_t h[16];if(!get(pos,h,8))return M4a::Unknown;
            uint64_t n=be32(h),header=8;
            if(n==1){if(file-pos<16||!get(pos+8,h+8,8))return M4a::Unknown;n=(uint64_t(be32(h+8))<<32)|be32(h+12);header=16;}
            if(!n)n=file-pos;if(n<header||n>file-pos)return M4a::Unknown;
            if(!memcmp(h+4,"mdat",4)&&mdat==file)mdat=pos;
            if(!memcmp(h+4,"moov",4)){moov=pos;if(!walk(pos+header,pos+n,1))return M4a::Unknown;}
            pos+=n;
        }
        if(alac)return moov<mdat?M4a::Alac:M4a::AlacLate;
        return aac?M4a::Aac:M4a::Unknown;
    }
};
template<class Read> M4a m4a(Read read,uint64_t bytes){return Mp4<Read>(read,bytes).probe();}
inline bool finalChunk(bool fileDone,size_t buffered,size_t offered){return fileDone&&buffered<=offered;}
}
