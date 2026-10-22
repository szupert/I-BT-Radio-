#pragma once
#include "logic.h"
namespace dual {
inline bool actionRepeats(Action a){return a==Action::VolDown||a==Action::VolUp;}
inline bool actionChangesStation(Action a){return a==Action::Prev||a==Action::Next;}
inline Action mainScreenAction(Action a){return a==Action::Select?Action::Toggle:a;}
}
