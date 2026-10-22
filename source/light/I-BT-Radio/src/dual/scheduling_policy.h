#pragma once
// V137 scheduling experiment: only active BT OUT changes application cadence.
namespace ibtsched {
constexpr bool active(bool outputSelected,bool transmitting){return outputSelected&&transmitting;}
constexpr unsigned audioPriority(bool bt){return bt?5:3;}
constexpr unsigned serviceDelayMs(bool bt){return bt?20:2;}
constexpr unsigned displayDelayMs(bool bt,unsigned normalMs){return bt?50:normalMs;}
}
