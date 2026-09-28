#include "radio_signal_view.h"
#include "bn_algorithm.h"
#include "bn_sprite_items_radio_barrel.h"
#include "bn_sprite_items_radio_chevrons.h"

radio_signal_view::radio_signal_view() :
    _barrel(bn::sprite_items::radio_barrel.create_sprite(0,0)),
    _indicator(bn::sprite_items::radio_chevrons.create_sprite(0,0)) {
    _barrel.set_bg_priority(1);_barrel.set_z_order(-4);_barrel.set_visible(false);
    _indicator.set_bg_priority(0);_indicator.set_z_order(-12);_indicator.set_visible(false);
}

void radio_signal_view::update(const radio_signal::system& signals,const driving::Car& player,
                               int camera_x,int camera_y,bool receiver_fitted,bool visible) {
    int shown=-1,best=0x7fffffff;
    for(int index=0;index<radio_signal::source_count;++index) {
        const auto& signal=signals.item(index);
        if(!signal.active)continue;
        const int x=signal.x-camera_x,y=signal.y-camera_y;
        if(x>-132 && x<132 && y>-92 && y<92) {
            const int distance=x*x+y*y;
            if(distance<best) { best=distance;shown=index; }
        }
    }
    _barrel_visible=visible && shown>=0;
    _barrel.set_visible(_barrel_visible);
    if(_barrel_visible) {
        const auto& signal=signals.item(shown);
        _barrel.set_position(signal.x-camera_x,signal.y-camera_y);
    }

    const int target=signals.target(),direction=signals.direction(),strength=signals.strength();
    _indicator_visible=visible && receiver_fitted && target>=0 && direction>=0 && strength>0;
    _indicator.set_visible(_indicator_visible);
    if(_indicator_visible) {
        constexpr int direction_x[8]={1,1,0,-1,-1,-1,0,1};
        constexpr int direction_y[8]={0,1,1,1,0,-1,-1,-1};
        int x=(player.x-camera_x).integer()+direction_x[direction]*28;
        int y=(player.y-camera_y).integer()+direction_y[direction]*28;
        x=bn::max(-102,bn::min(102,x));y=bn::max(-62,bn::min(62,y));
        _indicator.set_position(x,y);
        _indicator.set_tiles(bn::sprite_items::radio_chevrons.tiles_item(),(strength-1)*8+direction);
    }
}
