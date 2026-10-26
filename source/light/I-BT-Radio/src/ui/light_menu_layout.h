#pragma once
// Painter and touch owner share native row geometry. Special lists keep theirs.
namespace lightmenu {
// Only valid visible row contacts reach this policy; headers, drag and scrollbars remain separate.
constexpr unsigned activation(unsigned selected,unsigned touched,unsigned count,bool confirm){
 return count?(confirm?(selected<count?selected:count-1):(touched<count?touched:count-1)):0;
}
constexpr int top(unsigned page){return page==75?53:32;}
constexpr int step(unsigned page,unsigned count,bool alarm=false,int height=240){
 return alarm?44:count?(height-top(page))/int(count>8?8:count):26;
}
constexpr bool back(int x,int y,int width=320){return y>=5&&y<28&&x>=width-81&&x<width-7;}
constexpr bool heldBack(int x,int y,int lastX,int lastY,unsigned elapsed,int width=320){
 return elapsed>=800&&back(x,y,width)&&lastX-x>-12&&lastX-x<12&&lastY-y>-12&&lastY-y<12;
}
}
