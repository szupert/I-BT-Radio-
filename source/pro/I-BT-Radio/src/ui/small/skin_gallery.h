#pragma once
#include "../../dual/logic.h"
namespace front {
struct SkinGallery {
    int index=-1,painted=-1;
    bool active()const{return index>=0;}
    void select(int n){index=n>=0&&n<7?n:-1;painted=-1;}
    bool input(dual::Action a){
        if(!active())return false;
        using dual::Action;
        if(a==Action::Menu||a==Action::EncoderLong)select(-1);
        else if(a==Action::RotCW||a==Action::Next||a==Action::EncoderShort||a==Action::Select)select((index+1)%7);
        else if(a==Action::RotCCW||a==Action::Prev)select((index+6)%7);
        return true; // no playback/volume side effects during preview
    }
};
}
