#pragma once
namespace mp3balance {
// Only the audio-owner's WEB MP3 -> BT path is eligible.
constexpr bool eligible(bool web,bool bt,bool playing,unsigned codec){
 return web && bt && playing && codec==2;
}
}
