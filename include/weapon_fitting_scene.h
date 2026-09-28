#pragma once
#include "bn_optional.h"
#include "bn_regular_bg_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_vector.h"
#include "combat.h"

class weapon_fitting_scene {
public:
    enum class event { none, redraw, fitted, close };

    weapon_fitting_scene(combat::World& world,int car_type,uint16_t owned_weapons);
    event update(combat::World& world);

    int slot() const { return _slot; }
    int inventory_selection() const { return _inventory_selection; }
    bool inventory_open() const { return _inventory_open; }
    bool info_open() const { return _info_open; }
    combat::Weapon inventory_weapon() const;
    combat::Weapon highlighted_weapon(const combat::World& world) const;

private:
    int _choice_count() const;
    combat::Weapon _choice_weapon(int choice) const;
    int _choice_for(combat::Weapon weapon) const;
    void _set_info(bool visible);
    void _refresh(const combat::World& world);

    bn::optional<bn::regular_bg_ptr> _background;
    bn::sprite_ptr _car_left;
    bn::sprite_ptr _car_right;
    bn::sprite_ptr _front_left;
    bn::sprite_ptr _front_right;
    bn::sprite_ptr _side_left;
    bn::sprite_ptr _side_right;
    bn::sprite_ptr _special_left;
    bn::sprite_ptr _special_right;
    bn::sprite_ptr _cursor;
    bn::vector<bn::sprite_ptr,combat::mount_slot_count> _slot_icons;
    bn::vector<bn::sprite_ptr,9> _inventory_icons;
    int _car_type;
    uint16_t _owned_weapons;
    int _slot=0;
    int _inventory_selection=0;
    bool _inventory_open=false;
    bool _info_open=false;
};
