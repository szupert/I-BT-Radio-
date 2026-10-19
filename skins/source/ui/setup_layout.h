#pragma once
namespace setupui {
enum class Hit { None, Reboot, Player, Qr, Scan };
struct Rect { int x,y,w,h; constexpr bool contains(int px,int py) const { return px>=x&&px<x+w&&py>=y&&py<y+h; } };
constexpr Rect reboot(int h){return {8,h-48,92,44};}
constexpr Rect player(int w,int h){return {104,h-48,w-208,44};}
constexpr Rect qr(int w,int h){return {w-100,h-48,92,44};}
constexpr Rect scan(int w){return {w-100,4,92,40};}
constexpr Hit hit(int x,int y,int w,int h){
 return reboot(h).contains(x,y)?Hit::Reboot:player(w,h).contains(x,y)?Hit::Player:
        qr(w,h).contains(x,y)?Hit::Qr:scan(w).contains(x,y)?Hit::Scan:Hit::None;
}
}
