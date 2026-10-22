#pragma once
namespace videotono {
struct Rect {int x,y,w,h;constexpr bool has(int px,int py)const{return px>=x&&py>=y&&px<x+w&&py<y+h;}};
enum class Hit {None,Channels,Radio,Sd,Bluetooth,Memo,Prev,Toggle,Next,Menu,Volume};
constexpr Rect radioKey{38,164,48,23},sdKey{107,164,48,23},btKey{175,164,48,23},memoKey{240,164,48,23};
// Approved original order: caption, indicator, then the unlabelled ivory key.
constexpr Rect sourceLed[4]={{56,160,10,3},{122,160,10,3},{187,160,10,3},{254,160,10,3}};
constexpr Rect prevKey{15,201,43,25},playKey{64,201,43,25},nextKey{112,201,43,25},menuKey{166,201,39,25},volumeKey{224,207,82,18};
constexpr Hit hit(int x,int y){
 return radioKey.has(x,y)?Hit::Radio:sdKey.has(x,y)?Hit::Sd:btKey.has(x,y)?Hit::Bluetooth:memoKey.has(x,y)?Hit::Memo:
 prevKey.has(x,y)?Hit::Prev:playKey.has(x,y)?Hit::Toggle:nextKey.has(x,y)?Hit::Next:menuKey.has(x,y)?Hit::Menu:volumeKey.has(x,y)?Hit::Volume:
 Rect{14,52,256,67}.has(x,y)?Hit::Channels:Rect{0,0,160,28}.has(x,y)?Hit::Menu:Hit::None;
}
constexpr int volume(int x){return x<=225?0:x>=303?254:(x-225)*254/78;}
constexpr bool bootVideoton(unsigned choice,unsigned vintage){return choice==1||(choice==2&&vintage==6);}
}
