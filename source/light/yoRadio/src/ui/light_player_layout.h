#pragma once
namespace lightplayer {
// 320x240 accepted MINI layout. Cover pixels remain64x64 on flash;
// an aspect-fill row scaler paints the larger background viewport without a framebuffer.
constexpr int vuX=8,vuY=80,vuW=80,vuH=88;
constexpr int clockX=102,clockY=96,clockW=210,clockH=70;
constexpr int coverX=136,coverY=53,coverW=176,coverH=115;
constexpr unsigned signalBars(bool connected,int rssi){return !connected||rssi>=0?0:rssi>=-55?4:rssi>=-67?3:rssi>=-80?2:rssi>=-95?1:0;}
struct Size{int w,h;};
constexpr Size fit(int w,int h){return w<1||h<1?Size{0,0}:w>=h?Size{coverW,(h*coverW)/w>0?(h*coverW)/w:1}:Size{(w*coverH)/h>0?(w*coverH)/h:1,coverH};}
constexpr Size crop(int w,int h){
 if(w<1||h<1)return {0,0};
 return w*coverH>=h*coverW?Size{(w*coverH+h-1)/h,coverH}:Size{coverW,(h*coverW+w-1)/w};
}
}
