#pragma once

namespace ibtsource {
enum class Source { Web, Sd };

// Changing the output keeps the selected player source. Phone input still
// leaves SD mode, preserving its existing ownership and teardown behavior.
constexpr Source afterBt(Source current, int requestedMode) {
    return requestedMode == 1 ? Source::Web : current;
}

// Decoder and SD operations belong to the audio task. A connected output is
// required before restarting; otherwise the pairing branch would stop it.
constexpr bool canRestartLocal(bool sd, bool indexing, bool inputSelected,
                               bool outputSelected, bool transmitting) {
    return sd && !indexing && !inputSelected &&
           (!outputSelected || transmitting);
}
}
