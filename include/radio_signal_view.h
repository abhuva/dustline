#pragma once

#include "bn_sprite_ptr.h"
#include "driving.h"
#include "radio_signal.h"

class radio_signal_view {
public:
    radio_signal_view();
    void update(const radio_signal::system& signals,const driving::Car& player,
                int camera_x,int camera_y,bool receiver_fitted,bool visible);
    bool barrel_visible() const { return _barrel_visible; }
    bool indicator_visible() const { return _indicator_visible; }

private:
    bn::sprite_ptr _barrel;
    bn::sprite_ptr _indicator;
    bool _barrel_visible=false,_indicator_visible=false;
};

