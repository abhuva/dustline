#include "weapon_fitting_scene.h"
#include "bn_keypad.h"
#include "bn_regular_bg_items_weapon_fitting.h"
#include "bn_regular_bg_items_weapon_fitting_info.h"
#include "bn_sprite_items_fitting_cursor.h"
#include "bn_sprite_items_garage_car_attachments.h"
#include "bn_sprite_items_garage_car_preview.h"
#include "bn_sprite_items_weapon_icons.h"

namespace {
constexpr int slot_x[combat::mount_slot_count]={-95,-55,-15};
constexpr int inventory_x[3]={33,66,99};
constexpr int inventory_y[3]={-28,4,36};

int icon_frame(combat::Weapon weapon) {
    return int(weapon);
}
}

weapon_fitting_scene::weapon_fitting_scene(combat::World& world,int car_type,uint16_t owned_weapons) :
    _car_left(bn::sprite_items::garage_car_preview.create_sprite(-82,-20,car_type*2)),
    _car_right(bn::sprite_items::garage_car_preview.create_sprite(-18,-20,car_type*2+1)),
    _front_left(bn::sprite_items::garage_car_attachments.create_sprite(-82,-20,car_type*14)),
    _front_right(bn::sprite_items::garage_car_attachments.create_sprite(-18,-20,car_type*14+1)),
    _side_left(bn::sprite_items::garage_car_attachments.create_sprite(-82,-20,car_type*14+2)),
    _side_right(bn::sprite_items::garage_car_attachments.create_sprite(-18,-20,car_type*14+3)),
    _special_left(bn::sprite_items::garage_car_attachments.create_sprite(-82,-20,car_type*14+4)),
    _special_right(bn::sprite_items::garage_car_attachments.create_sprite(-18,-20,car_type*14+5)),
    _cursor(bn::sprite_items::fitting_cursor.create_sprite(slot_x[0],49)),
    _car_type(car_type),_owned_weapons(owned_weapons) {
    _set_info(false);
    _car_left.set_bg_priority(0);_car_left.set_z_order(-3);
    _car_right.set_bg_priority(0);_car_right.set_z_order(-3);
    for(auto* sprite : { &_front_left, &_front_right, &_side_left, &_side_right,
                         &_special_left, &_special_right }) {
        sprite->set_bg_priority(0);sprite->set_z_order(-5);
    }
    _cursor.set_bg_priority(0);_cursor.set_z_order(-9);
    for(int slot=0;slot<combat::mount_slot_count;++slot) {
        auto icon=bn::sprite_items::weapon_icons.create_sprite(slot_x[slot],49);
        icon.set_bg_priority(0);icon.set_z_order(-8);_slot_icons.push_back(icon);
    }
    for(int index=0;index<9;++index) {
        auto icon=bn::sprite_items::weapon_icons.create_sprite(inventory_x[index%3],inventory_y[index/3],5);
        icon.set_bg_priority(0);icon.set_z_order(-8);icon.set_visible(false);_inventory_icons.push_back(icon);
    }
    _refresh(world);
}

int weapon_fitting_scene::_choice_count() const {
    int result=1;
    const combat::Weapon candidates[3][3]={{combat::Weapon::gun,combat::Weapon::sniper,combat::Weapon::empty},
        {combat::Weapon::sides,combat::Weapon::front_shooter,combat::Weapon::empty},
        {combat::Weapon::missile,combat::Weapon::trap,combat::Weapon::radio}};
    const int available=_slot==int(combat::MountSlot::special)?3:2;
    for(int index=0;index<available;++index)result+=(_owned_weapons&(1u<<int(candidates[_slot][index])))!=0;
    return result;
}

combat::Weapon weapon_fitting_scene::_choice_weapon(int choice) const {
    if(!choice)return combat::Weapon::empty;
    const combat::Weapon candidates[3][3]={{combat::Weapon::gun,combat::Weapon::sniper,combat::Weapon::empty},
        {combat::Weapon::sides,combat::Weapon::front_shooter,combat::Weapon::empty},
        {combat::Weapon::missile,combat::Weapon::trap,combat::Weapon::radio}};
    const int available=_slot==int(combat::MountSlot::special)?3:2;
    for(int index=0;index<available;++index)if(_owned_weapons&(1u<<int(candidates[_slot][index])))
        if(!--choice)return candidates[_slot][index];
    return combat::Weapon::empty;
}

combat::Weapon weapon_fitting_scene::inventory_weapon() const { return _choice_weapon(_inventory_selection); }

combat::Weapon weapon_fitting_scene::highlighted_weapon(const combat::World& world) const {
    return _inventory_open?inventory_weapon():world.fitted_weapon(combat::MountSlot(_slot));
}

int weapon_fitting_scene::_choice_for(combat::Weapon weapon) const {
    for(int choice=0;choice<_choice_count();++choice)if(_choice_weapon(choice)==weapon)return choice;
    return 0;
}

void weapon_fitting_scene::_set_info(bool visible) {
    _info_open=visible;
    _background.reset();
    _background=visible?bn::regular_bg_items::weapon_fitting_info.create_bg(0,0):
                        bn::regular_bg_items::weapon_fitting.create_bg(0,0);
    _background->set_priority(3);
}

void weapon_fitting_scene::_refresh(const combat::World& world) {
    _car_left.set_tiles(bn::sprite_items::garage_car_preview.tiles_item(),_car_type*2);
    _car_right.set_tiles(bn::sprite_items::garage_car_preview.tiles_item(),_car_type*2+1);
    const auto front=world.fitted_weapon(combat::MountSlot::front);
    const auto side=world.fitted_weapon(combat::MountSlot::side);
    const auto special=world.fitted_weapon(combat::MountSlot::special);
    const bool front_visible=front==combat::Weapon::gun || front==combat::Weapon::sniper;
    const bool side_visible=side==combat::Weapon::sides || side==combat::Weapon::front_shooter;
    const bool special_visible=special==combat::Weapon::missile || special==combat::Weapon::trap ||
                               special==combat::Weapon::radio;
    _front_left.set_visible(front_visible);_front_right.set_visible(front_visible);
    _side_left.set_visible(side_visible);_side_right.set_visible(side_visible);
    _special_left.set_visible(special_visible);_special_right.set_visible(special_visible);
    if(front_visible) {
        const int attachment=front==combat::Weapon::sniper?5:0;
        _front_left.set_tiles(bn::sprite_items::garage_car_attachments.tiles_item(),_car_type*14+attachment*2);
        _front_right.set_tiles(bn::sprite_items::garage_car_attachments.tiles_item(),_car_type*14+attachment*2+1);
    }
    if(side_visible) {
        const int attachment=side==combat::Weapon::front_shooter?6:1;
        _side_left.set_tiles(bn::sprite_items::garage_car_attachments.tiles_item(),_car_type*14+attachment*2);
        _side_right.set_tiles(bn::sprite_items::garage_car_attachments.tiles_item(),_car_type*14+attachment*2+1);
    }
    if(special_visible) {
        const int attachment=special==combat::Weapon::missile?2:special==combat::Weapon::trap?3:4;
        _special_left.set_tiles(bn::sprite_items::garage_car_attachments.tiles_item(),
                                _car_type*14+attachment*2);
        _special_right.set_tiles(bn::sprite_items::garage_car_attachments.tiles_item(),
                                 _car_type*14+attachment*2+1);
    }
    for(int slot=0;slot<combat::mount_slot_count;++slot) {
        const auto fitted=world.fitted_weapon(combat::MountSlot(slot));
        _slot_icons[slot].set_tiles(bn::sprite_items::weapon_icons.tiles_item(),icon_frame(fitted));
    }
    const int choices=_choice_count();
    for(int index=0;index<_inventory_icons.size();++index) {
        const bool visible=!_info_open && index<choices;
        _inventory_icons[index].set_visible(visible);
        if(visible)_inventory_icons[index].set_tiles(bn::sprite_items::weapon_icons.tiles_item(),
                                                     icon_frame(_choice_weapon(index)));
    }
    _cursor.set_visible(!_info_open);
    if(!_info_open) {
        if(_inventory_open) {
            _cursor.set_position(inventory_x[_inventory_selection%3],inventory_y[_inventory_selection/3]);
            _cursor.set_tiles(bn::sprite_items::fitting_cursor.tiles_item(),1);
        } else {
            _cursor.set_position(slot_x[_slot],49);
            _cursor.set_tiles(bn::sprite_items::fitting_cursor.tiles_item(),0);
        }
    }
}

weapon_fitting_scene::event weapon_fitting_scene::update(combat::World& world) {
    if(_info_open) {
        if(bn::keypad::b_pressed() || bn::keypad::r_pressed()) {
            _set_info(false);_refresh(world);return event::redraw;
        }
        return event::none;
    }
    if(_inventory_open) {
        const int choices=_choice_count();
        if(bn::keypad::left_pressed() || bn::keypad::up_pressed()) {
            _inventory_selection=(_inventory_selection+choices-1)%choices;_refresh(world);return event::redraw;
        }
        if(bn::keypad::right_pressed() || bn::keypad::down_pressed()) {
            _inventory_selection=(_inventory_selection+1)%choices;_refresh(world);return event::redraw;
        }
        if(bn::keypad::r_pressed()) {_set_info(true);_refresh(world);return event::redraw;}
        if(bn::keypad::b_pressed()) {_inventory_open=false;_refresh(world);return event::redraw;}
        if(bn::keypad::a_pressed()) {
            world.fit_weapon(combat::MountSlot(_slot),inventory_weapon());
            _inventory_open=false;_refresh(world);return event::fitted;
        }
        return event::none;
    }
    if(bn::keypad::left_pressed()) {
        _slot=(_slot+combat::mount_slot_count-1)%combat::mount_slot_count;_refresh(world);return event::redraw;
    }
    if(bn::keypad::right_pressed()) {
        _slot=(_slot+1)%combat::mount_slot_count;_refresh(world);return event::redraw;
    }
    if(bn::keypad::r_pressed()) {_set_info(true);_refresh(world);return event::redraw;}
    if(bn::keypad::a_pressed()) {
        _inventory_selection=_choice_for(world.fitted_weapon(combat::MountSlot(_slot)));
        _inventory_open=true;_refresh(world);return event::redraw;
    }
    if(bn::keypad::b_pressed())return event::close;
    return event::none;
}
