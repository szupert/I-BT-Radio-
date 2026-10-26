#pragma once
namespace sdbtnavigation {
constexpr bool returnHome(bool wasConnected,bool connected,bool pickerPage){return !wasConnected&&connected&&pickerPage;}
constexpr bool libraryTap(int x,int y){return x>=0&&x<160&&y>=0&&y<28;}
}
