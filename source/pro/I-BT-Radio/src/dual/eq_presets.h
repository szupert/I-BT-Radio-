#pragma once
#include <stdint.h>

// I-BT-Radio's own curves, not branded/manufacturer presets.
// Own ten-band DAC presets. Selecting a profile applies all bands atomically.
namespace eq10 {
constexpr unsigned Bands = 10;
// Match the ten frequency labels printed on the selected Winamp-style skin.
constexpr uint16_t Hz[Bands] = {70,180,320,600,1000,3000,6000,12000,14000,16000};
enum class Preset : uint8_t { Flat, Rock, Pop, Jazz, Classical, Techno,
                             Dance, HipHop, Vocal, Bass, Count, Custom=255 };
struct Curve { const char* hu; const char* en; const char* de; int8_t db[Bands]; };
constexpr Curve Curves[] = {
 {"Normál","Flat","Neutral",          {0,0,0,0,0,0,0,0,0,0}},
 {"Rock","Rock","Rock",              {4,3,2,0,-2,-1,1,3,4,3}},
 {"Pop","Pop","Pop",                 {-1,0,1,2,3,3,2,1,0,-1}},
 {"Jazz","Jazz","Jazz",              {2,2,1,0,-1,0,1,2,3,2}},
 {"Klasszikus","Classical","Klassik", {2,1,0,-1,-1,0,1,2,2,1}},
 {"Techno","Techno","Techno",        {4,4,2,0,-2,-1,1,3,4,3}},
 {"Dance","Dance","Dance",           {4,3,1,-1,-2,0,2,3,2,1}},
 {"Hip-hop","Hip-hop","Hip-Hop",     {4,4,3,1,-1,0,1,0,1,0}},
 {"Ének","Vocal","Gesang",           {-3,-2,-1,0,2,3,3,2,0,-1}},
 {"Basszus","Bass boost","Bass",     {4,4,3,2,1,0,0,0,0,0}}
};
constexpr unsigned PresetCount=sizeof(Curves)/sizeof(Curves[0]);
static_assert(PresetCount==unsigned(Preset::Count),"preset table/enum mismatch");
struct Settings { int8_t db[Bands]={}; Preset preset=Preset::Flat; bool enabled=false; };
inline const char* name(Preset preset,unsigned language){
 if(unsigned(preset)>=PresetCount)return language==0?"Egyéni":language==2?"Benutzer":"Custom";
 const auto& c=Curves[unsigned(preset)];return language==0?c.hu:language==2?c.de:c.en;
}
inline bool apply(Settings& s,unsigned preset){
 if(preset>=PresetCount)return false;
 for(unsigned i=0;i<Bands;++i)s.db[i]=Curves[preset].db[i];
 s.preset=Preset(preset);s.enabled=true;return true;
}
inline bool edit(Settings& s,unsigned band,int db){
 if(band>=Bands||db < -12||db > 6)return false;
 if(s.db[band]!=db){s.db[band]=int8_t(db);s.preset=Preset::Custom;}
 return true;
}
inline bool flat(const Settings& s){for(auto db:s.db)if(db)return false;return true;}
// Browsing is non-destructive. Only explicit selection applies a whole curve.
struct Picker {
 bool opened=false; unsigned selected=0;
 void open(const Settings& s){opened=true;selected=unsigned(s.preset)<PresetCount?unsigned(s.preset):0;}
 void move(bool forward){if(opened)selected=(selected+(forward?1:PresetCount-1))%PresetCount;}
 bool choose(Settings& s){if(!opened)return false;const bool ok=apply(s,selected);if(ok)opened=false;return ok;}
 void cancel(){opened=false;}
};
enum class Route { Dac, WebBt, SdBt };
// SD-BT is gated by a separate qualification; selecting a preset cannot enable it.
constexpr bool routeAllowed(Route route,bool sdBtQualified=false){
 return route==Route::Dac||(route==Route::SdBt&&sdBtQualified);
}
}
