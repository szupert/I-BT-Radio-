#pragma once
#include <stdint.h>
namespace btspeakerui {
struct Rect {int x,y,w,h;constexpr bool contains(int px,int py)const{return px>=x&&py>=y&&px<x+w&&py<y+h;}};
constexpr Rect previous{17,152,67,34},toggle{90,152,68,34},next{164,152,67,34},menu{238,152,66,34},volume{60,198,244,22};
enum class Control:uint8_t {None,Previous,Toggle,Next,Menu,Volume};
constexpr Control hit(int x,int y){return previous.contains(x,y)?Control::Previous:toggle.contains(x,y)?Control::Toggle:next.contains(x,y)?Control::Next:menu.contains(x,y)?Control::Menu:volume.contains(x,y)?Control::Volume:Control::None;}
constexpr unsigned percent(int x){return x<=60?0:x>=260?100:unsigned(x-60)*100u/200u;}
}
