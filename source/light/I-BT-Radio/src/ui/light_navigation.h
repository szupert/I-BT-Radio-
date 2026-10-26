#pragma once
namespace lightnav {
constexpr unsigned mainCount=6,controlsCount=7;
enum Mode : unsigned {Player,Clock,Weather};
constexpr unsigned next(unsigned current,bool left){return (current+(left?1u:2u))%3u;}
constexpr bool wifiHit(int x,int y,bool bluetooth=false){return !bluetooth&&x>=240&&x<282&&y>=0&&y<28;}
// Diagnostic menu motion uses the same rows as the normal physical menu.
constexpr unsigned mainDestination(unsigned row){return row==0?6:row==1?3:row==2?7:0;}
}
