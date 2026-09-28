#include "town_scene.h"
#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_regular_bg_items_town_exterior.h"
#include "bn_regular_bg_items_garage_interior.h"
#include "bn_sprite_items_town_player.h"
#include "bn_sprite_items_town_interact.h"
#include "bn_sprite_items_shop_icons.h"
#include "bn_sprite_items_fitting_cursor.h"
#include "world_map.h"

namespace {
constexpr town_scene::rect exterior_solids[]={
    {0,0,256,8},{0,0,8,256},{248,0,256,256},
    {0,8,89,68},                 // northwest water and utility compound
    {91,8,172,42},               // north mechanic garage
    {174,8,256,69},              // northeast solar homes
    {0,69,89,116},               // western dispatch and shops
    {169,69,256,116},            // eastern market block
    {31,118,86,146},             // southwest market stalls
    {174,112,208,149},           // public fountain
    {0,147,91,219},              // southwest residential block
    {163,149,256,219},           // southeast workshop block
    {0,218,104,256},{153,218,256,256}, // straight south fence and open gate
};
constexpr town_scene::rect garage_solids[]={
    {0,0,256,8},{0,0,8,256},{248,0,256,256},
    {8,8,77,73},{78,8,179,72},{180,8,248,74}, // north storage and counter
    {8,74,38,207},{218,74,248,207},           // side tool walls
    {40,91,84,164},                           // parked car and left lift
    {163,91,177,151},{207,91,220,151},         // right lift posts
    {8,170,94,213},{177,170,248,213},          // parts and benches
    {0,209,101,256},{157,209,256,256},         // south wall, central exit
};
constexpr town_scene::rect dispatch_trigger={91,78,112,118};
constexpr town_scene::rect race_trigger={148,78,169,118};
constexpr town_scene::rect garage_trigger={102,43,158,63};
constexpr town_scene::rect exterior_exit_trigger={105,226,153,256};
constexpr town_scene::rect garage_counter_trigger={78,70,179,85};
constexpr town_scene::rect garage_fitting_trigger={87,106,116,157};
constexpr town_scene::rect garage_exit_trigger={101,224,157,256};

template<int Size>
bool hits(const town_scene::rect (&areas)[Size],int left,int top,int right,int bottom) {
    for(const auto& area:areas)
        if(right>area.left && left<area.right && bottom>area.top && top<area.bottom) return true;
    return false;
}
}

town_scene::town_scene(int town_id,int setup,const garage_shop::ownership& owned) :
    _player(bn::sprite_items::town_player.create_sprite(0,0)),
    _prompt(bn::sprite_items::town_interact.create_sprite(0,0)),
    _shop_cursor(bn::sprite_items::fitting_cursor.create_sprite(0,0,1)),
    _town_id(town_id),_menu_selection(setup) {
    _player.set_bg_priority(1);
    _player.set_z_order(-2);
    _prompt.set_bg_priority(1);
    _prompt.set_z_order(-3);
    _prompt.set_visible(false);
    _shop_cursor.set_bg_priority(0);_shop_cursor.set_z_order(-9);_shop_cursor.set_visible(false);
    for(int index=0;index<garage_shop::max_visible_stock;++index) {
        auto icon=bn::sprite_items::shop_icons.create_sprite(-82+(index%3)*34,-19+(index/3)*32,0);
        icon.set_bg_priority(0);icon.set_z_order(-8);icon.set_visible(false);_shop_icons.push_back(icon);
    }
    refresh_shop(owned);
    _load(place::exterior);
}

int town_scene::shop_item() const {
    if(_shop_selection<0 || _shop_selection>=_shop_count)return -1;
    return garage_shop::index_for_save_id(_visible_stock[_shop_selection]);
}

void town_scene::refresh_shop(const garage_shop::ownership& owned) {
    _shop_count=0;
    const auto stock=world_map::shop_inventory(_town_id);
    for(int index=0;index<stock.count && _shop_count<garage_shop::max_visible_stock;++index) {
        const int save_id=stock.save_ids[index];
        if(!garage_shop::owned_save_id(owned,save_id))_visible_stock[_shop_count++]=uint8_t(save_id);
    }
    if(_shop_count==0)_shop_selection=0;
    else if(_shop_selection>=_shop_count)_shop_selection=_shop_count-1;
    for(int index=0;index<_shop_icons.size();++index) {
        if(index<_shop_count) {
            const int item=garage_shop::index_for_save_id(_visible_stock[index]);
            BN_ASSERT(item>=0,"Compiled shop contains unknown save ID");
            _shop_icons[index].set_tiles(bn::sprite_items::shop_icons.tiles_item(),
                                         garage_shop::catalog[item].icon_frame);
        }
    }
    if(_background)_refresh_sprite();
}

bool town_scene::_inside(const rect& area,int x,int y) const {
    return x>=area.left && x<area.right && y>=area.top && y<area.bottom;
}

bool town_scene::_blocked(int x,int y) const {
    if(x<6 || x>250 || y<8 || y>250) return true;
    const int left=x-5,right=x+6,top=y-3,bottom=y+4;
    return _place==place::exterior ? hits(exterior_solids,left,top,right,bottom) :
                                     hits(garage_solids,left,top,right,bottom);
}

void town_scene::_load(place next) {
    _background.reset();
    bn::core::update();
    _place=next;
    if(next==place::exterior) {
        _background=bn::regular_bg_items::town_exterior.create_bg(0,0);
        if(_y<100) { _x=128;_y=51;_direction=0; }
        else { _x=128;_y=226;_direction=3; }
    } else {
        _background=bn::regular_bg_items::garage_interior.create_bg(0,0);
        _x=128;_y=228;_direction=3;
    }
    _background->set_priority(3);
    _background->set_visible(_visible);
    _refresh_sprite();
}

void town_scene::_refresh_sprite() {
    int camera_x=_x<120?120:_x>136?136:_x;
    int camera_y=_y<80?80:_y>176?176:_y;
    _background->set_position(128-camera_x,128-camera_y);
    _player.set_position(_x-camera_x,_y-camera_y-13);
    int phase=_walk_ticks ? 1+((_walk_ticks/8)&1) : 0;
    _player.set_tiles(bn::sprite_items::town_player.tiles_item(),_direction*3+phase);
    const bool interaction=_place==place::exterior ?
        (_inside(dispatch_trigger,_x,_y) || _inside(race_trigger,_x,_y) ||
         _inside(garage_trigger,_x,_y) || _inside(exterior_exit_trigger,_x,_y)) :
        (_inside(garage_counter_trigger,_x,_y) || _inside(garage_fitting_trigger,_x,_y) ||
         _inside(garage_exit_trigger,_x,_y));
    if(interaction) {
        const int wiggle=(_prompt_ticks++/8)%4;
        const int bob=wiggle==1?-1:wiggle==3?1:0;
        _prompt.set_position(_x-camera_x+11,_y-camera_y-29+bob);
    } else {
        _prompt_ticks=0;
    }
    _prompt.set_visible(_visible && interaction && !_menu_open);
    const bool shop_visible=_visible && _menu_open && _menu_page==1 && !_shop_info_open;
    for(int index=0;index<_shop_icons.size();++index)
        _shop_icons[index].set_visible(shop_visible && index<_shop_count);
    _shop_cursor.set_visible(shop_visible && _shop_count>0);
    if(shop_visible && _shop_count>0)
        _shop_cursor.set_position(-82+(_shop_selection%3)*34,-19+(_shop_selection/3)*32);
}

town_scene::event town_scene::update(int& setup) {
    if(_menu_open) {
        if(_menu_page) {
            if(_shop_info_open) {
                if(bn::keypad::b_pressed() || bn::keypad::r_pressed()) {
                    _shop_info_open=false;_refresh_sprite();return event::redraw;
                }
                return event::none;
            }
            int next=_shop_selection;
            if(bn::keypad::left_pressed() && _shop_count)next=(_shop_selection+_shop_count-1)%_shop_count;
            else if(bn::keypad::right_pressed() && _shop_count)next=(_shop_selection+1)%_shop_count;
            else if(bn::keypad::up_pressed() && _shop_count) {
                next=_shop_selection-3;
                if(next<0) { next=_shop_selection;while(next+3<_shop_count)next+=3; }
            } else if(bn::keypad::down_pressed() && _shop_count) {
                next=_shop_selection+3;
                if(next>=_shop_count)next=_shop_selection%3< _shop_count?_shop_selection%3:_shop_count-1;
            } else if(bn::keypad::r_pressed() && _shop_count) {
                _shop_info_open=true;_refresh_sprite();return event::redraw;
            } else if(bn::keypad::a_pressed() && _shop_count)return event::shop_purchase_requested;
            else if(bn::keypad::b_pressed()) {_menu_page=0;_refresh_sprite();return event::redraw;}
            else return event::none;
            _shop_selection=next;_refresh_sprite();return event::redraw;
        }
        if(bn::keypad::down_pressed() || bn::keypad::up_pressed()) {
            _menu_page=1;_shop_info_open=false;_refresh_sprite();return event::redraw;
        }
        if(bn::keypad::left_pressed()) {_menu_selection=(_menu_selection+2)%3;return event::redraw;}
        if(bn::keypad::right_pressed()) {_menu_selection=(_menu_selection+1)%3;return event::redraw;}
        if(bn::keypad::b_pressed()) {
            _menu_open=false;
            _refresh_sprite();
            return event::menu_closed;
        }
        if(bn::keypad::a_pressed()) {
            setup=_menu_selection;
            _menu_open=false;
            _refresh_sprite();
            return event::setup_applied;
        }
        return event::none;
    }

    int dx=0,dy=0;
    if(bn::keypad::up_held()) { dy=-1;_direction=3; }
    else if(bn::keypad::down_held()) { dy=1;_direction=0; }
    else if(bn::keypad::left_held()) { dx=-1;_direction=1; }
    else if(bn::keypad::right_held()) { dx=1;_direction=2; }
    if(dx || dy) {
        if(!_blocked(_x+dx,_y+dy)) { _x+=dx;_y+=dy; }
        ++_walk_ticks;
    } else _walk_ticks=0;
    _refresh_sprite();

    if(!bn::keypad::a_pressed()) return event::none;
    if(_place==place::exterior) {
        if(_inside(dispatch_trigger,_x,_y)) {
            _prompt.set_visible(false);
            return event::contract_opened;
        }
        if(_inside(race_trigger,_x,_y)) {
            _prompt.set_visible(false);
            return event::race_opened;
        }
        if(_inside(garage_trigger,_x,_y)) {
            _prompt.set_visible(false);
            _load(place::garage);
            return event::redraw;
        }
        if(_inside(exterior_exit_trigger,_x,_y)) return event::return_to_world;
    } else {
        if(_inside(garage_exit_trigger,_x,_y)) {
            _prompt.set_visible(false);
            _x=128;_y=51;
            _load(place::exterior);
            return event::redraw;
        }
        if(_inside(garage_counter_trigger,_x,_y)) {
            _menu_open=true;
            _menu_selection=setup;
            _menu_page=0;
            _shop_info_open=false;
            _walk_ticks=0;
            _refresh_sprite();
            return event::menu_opened;
        }
        if(_inside(garage_fitting_trigger,_x,_y)) {
            _prompt.set_visible(false);_walk_ticks=0;_refresh_sprite();
            return event::weapon_fitting_opened;
        }
    }
    return event::none;
}

void town_scene::return_to_race_building() {
    if(_place!=place::exterior)_load(place::exterior);
    _x=146;_y=98;_direction=2;_walk_ticks=0;_refresh_sprite();
}

void town_scene::set_visible(bool visible) {
    _visible=visible;
    if(_background)_background->set_visible(visible);
    _player.set_visible(visible);
    if(visible) _refresh_sprite();
    else {
        _prompt.set_visible(false);_shop_cursor.set_visible(false);
        for(auto& icon:_shop_icons)icon.set_visible(false);
    }
}

void town_scene::suspend() {
    _visible=false;_background.reset();_player.set_visible(false);_prompt.set_visible(false);
    _shop_cursor.set_visible(false);for(auto& icon:_shop_icons)icon.set_visible(false);
}

void town_scene::resume() {
    if(!_background) {
        _background=_place==place::exterior?bn::regular_bg_items::town_exterior.create_bg(0,0):
                                            bn::regular_bg_items::garage_interior.create_bg(0,0);
        _background->set_priority(3);
    }
    _visible=true;
    _player.set_visible(true);
    _refresh_sprite();
}
