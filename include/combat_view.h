#pragma once
#include "combat.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_palette_ptr.h"
#include "bn_optional.h"

class combat_view {
public:
    static constexpr int pickup_sprite_count=3;
    combat_view();
    void set_passenger_sprites(bool enabled);
    void update(const combat::World& world,int camera_x,int camera_y,bool visible);
private:
    bn::array<bn::optional<bn::sprite_ptr>,combat::enemy_count> _cars,_mounts,_hp,_bursts;
    bn::array<bn::optional<bn::sprite_ptr>,passenger_traffic::passenger_count> _passenger_cars,_passenger_bursts;
    bn::optional<bn::sprite_palette_ptr> _passenger_palette;
    bn::array<bn::optional<bn::sprite_ptr>,combat::bullet_count> _bullets;
    bn::array<bn::optional<bn::sprite_ptr>,combat::missile_count> _missiles,_missile_bursts;
    bn::array<bn::optional<bn::sprite_ptr>,combat::trap_count> _traps,_trap_bursts;
    bn::array<bn::optional<bn::sprite_ptr>,pickup_sprite_count> _pickups;
    bn::optional<bn::sprite_ptr> _saw;
};
