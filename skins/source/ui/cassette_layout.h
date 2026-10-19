#pragma once
#include "../dual/logic.h"
namespace cassette {
constexpr int width=320,height=213;
struct View {int x,y,w,h;};
inline View view(dual::Size size){int w=size.w,h=w*height/width;if(h>size.h){h=size.h;w=h*width/height;}return {(size.w-w)/2,(size.h-h)/2,w,h};}
inline bool btQuickHit(dual::Size size,dual::Point p){
    View v=view(size);if(v.w<=0||v.h<=0||p.x<v.x||p.y<v.y||p.x>=v.x+v.w||p.y>=v.y+v.h)return false;
    int x=(p.x-v.x)*width/v.w,y=(p.y-v.y)*height/v.h;
    return x<40&&y<42;
}
inline dual::Action hit(dual::Size size,dual::Point p){
    View v=view(size);if(v.w<=0||v.h<=0||p.x<v.x||p.y<v.y||p.x>=v.x+v.w||p.y>=v.y+v.h)return dual::Action::None;
    int x=(p.x-v.x)*width/v.w,y=(p.y-v.y)*height/v.h;
    if(y<65||y>=129)return dual::Action::None;
    if(x>=53&&x<128)return dual::Action::Prev;
    if(x>=192&&x<267)return dual::Action::Next;
    if(x>=133&&x<187&&y>=73&&y<121)return dual::Action::Toggle;
    return dual::Action::None;
}
// Label is cut around the original moulding and the complete reel/window cavity.
inline bool sticker(int x,int y){
    if(x<18||x>=302||y<21||y>=143)return false;
    if(y<34&&(x<32-(y-21)||x>=288+(y-21)))return false;
    if(y>=132&&(x<18+(y-132)||x>=302-(y-132)))return false;
    if(y>=64&&y<127){
        const int dy=y-95,dx=x<85?85-x:x>235?x-235:0;
        if(dx*dx+dy*dy<=32*32)return false;
    }
    return true;
}
}
