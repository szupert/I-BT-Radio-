#pragma once
#include <stdint.h>
namespace lightmenu {
// Eight visible content rows, not a framebuffer or a text allocation.
struct Rows {
    uint32_t keys[8]{},geometry=0;
    uint8_t valid=0;
    bool begin(uint32_t next,bool full){
        const bool changed=full||geometry!=next;
        if(changed){geometry=next;valid=0;}
        return changed;
    }
    bool changed(unsigned row,uint32_t key){
        if(row>=8)return false;
        const uint8_t bit=uint8_t(1u<<row);
        if((valid&bit)&&keys[row]==key)return false;
        keys[row]=key;valid|=bit;return true;
    }
};
}
