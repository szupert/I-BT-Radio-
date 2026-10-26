#pragma once
namespace btteardown {
// A deadline is diagnostic; it cannot replace asynchronous SDK acknowledgements.
constexpr bool stackStopAllowed(bool activeProfile,bool profileGone,bool input,bool controlGone){
    return !activeProfile||(profileGone&&(!input||controlGone));
}
}
