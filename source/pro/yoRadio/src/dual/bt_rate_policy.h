#pragma once
namespace btrate {
constexpr bool valid(unsigned hz) { return hz==16000||hz==32000||hz==44100||hz==48000; }
// Highest SBC rate not above a known source; below 16 kHz the SBC minimum applies.
constexpr unsigned native(unsigned source,unsigned fallback) {
    if(!source)return valid(fallback)?fallback:32000;
    return source>=48000?48000:source>=44100?44100:source>=32000?32000:16000;
}
constexpr unsigned effective(bool web,unsigned selected) { return web?32000:selected; }
constexpr bool selectable(bool web,unsigned hz) { return valid(hz)&&(!web||hz==32000); }
constexpr bool accepted(bool web,unsigned negotiated) { return valid(negotiated)&&(!web||negotiated==32000); }
}
