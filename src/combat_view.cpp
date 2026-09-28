#include "combat_view.h"
#include "bn_sprite_items_car.h"
#include "bn_sprite_items_car_enemy_gun.h"
#include "bn_sprite_items_combat_bullet.h"
#include "bn_sprite_items_combat_hp.h"
#include "bn_sprite_items_combat_burst.h"
#include "bn_sprite_items_weapon_saw.h"
#include "bn_sprite_items_weapon_missile.h"
#include "bn_sprite_items_weapon_trap.h"
#include "bn_sprite_items_weapon_blast.h"
#include "bn_sprite_items_salvage_pickup.h"
#include "bn_sprite_items_car_sand_buggy.h"
#include "bn_sprite_items_car_truck.h"
#include "bn_sprite_palette_ptr.h"
#include "bn_color.h"
#include "bn_core.h"

combat_view::combat_view() {
    auto palette=bn::sprite_palette_ptr::create_new(bn::sprite_items::car.palette_item());
    palette.set_color(4,bn::color(15,3,3));
    palette.set_color(5,bn::color(25,7,4));
    palette.set_color(6,bn::color(31,16,8));
    for(int i=0;i<combat::enemy_count;++i) {
        _cars[i]=bn::sprite_items::car.create_sprite(0,0);
        _cars[i]->set_palette(palette); _cars[i]->set_bg_priority(1); _cars[i]->set_z_order(0);
        _mounts[i]=bn::sprite_items::car_enemy_gun.create_sprite(0,0);
        _mounts[i]->set_palette(palette); _mounts[i]->set_bg_priority(1); _mounts[i]->set_z_order(-1);
        _hp[i]=bn::sprite_items::combat_hp.create_sprite(0,0);
        _hp[i]->set_bg_priority(1); _hp[i]->set_z_order(-2);
        _bursts[i]=bn::sprite_items::combat_burst.create_sprite(0,0);
        _bursts[i]->set_bg_priority(1); _bursts[i]->set_z_order(-3);
        _cars[i]->set_visible(false); _mounts[i]->set_visible(false);
        _hp[i]->set_visible(false); _bursts[i]->set_visible(false);
        bn::core::update(); // Scene loading, never charge allocation to driving.
    }
    set_passenger_sprites(true);
    bn::core::update();
    for(auto& b:_bullets) {
        b=bn::sprite_items::combat_bullet.create_sprite(0,0);
        b->set_bg_priority(1); b->set_z_order(-2); b->set_visible(false);
    }
    auto prepare=[](bn::optional<bn::sprite_ptr>& sprite,const bn::sprite_item& item) {
        sprite=item.create_sprite(0,0);sprite->set_bg_priority(1);sprite->set_z_order(-3);sprite->set_visible(false);
    };
    for(int i=0;i<combat::missile_count;++i) {
        prepare(_missiles[i],bn::sprite_items::weapon_missile);prepare(_missile_bursts[i],bn::sprite_items::weapon_blast);
    }
    for(int i=0;i<combat::trap_count;++i) {
        prepare(_traps[i],bn::sprite_items::weapon_trap);prepare(_trap_bursts[i],bn::sprite_items::weapon_blast);
    }
    for(auto& pickup:_pickups)prepare(pickup,bn::sprite_items::salvage_pickup);
    prepare(_saw,bn::sprite_items::weapon_saw);
    bn::core::update();
}
void combat_view::set_passenger_sprites(bool enabled) {
    if(!enabled) {
        for(auto& sprite:_passenger_cars)sprite.reset();
        for(auto& sprite:_passenger_bursts)sprite.reset();
        _passenger_palette.reset();
        return;
    }
    if(_passenger_cars[0])return;
    _passenger_palette=bn::sprite_palette_ptr::create_new(bn::sprite_items::car.palette_item());
    _passenger_palette->set_color(4,bn::color(4,10,18));
    _passenger_palette->set_color(5,bn::color(7,18,27));
    _passenger_palette->set_color(6,bn::color(16,27,31));
    for(int i=0;i<passenger_traffic::passenger_count;++i) {
        _passenger_cars[i]=bn::sprite_items::car.create_sprite(0,0);
        _passenger_cars[i]->set_palette(*_passenger_palette);
        _passenger_cars[i]->set_bg_priority(1);_passenger_cars[i]->set_z_order(0);
        _passenger_cars[i]->set_visible(false);
        _passenger_bursts[i]=bn::sprite_items::combat_burst.create_sprite(0,0);
        _passenger_bursts[i]->set_bg_priority(1);_passenger_bursts[i]->set_z_order(-3);
        _passenger_bursts[i]->set_visible(false);
    }
}
void combat_view::update(const combat::World& world,int cx,int cy,bool visible) {
    for(int i=0;i<combat::enemy_count;++i) {
        const auto& e=world.enemies[i]; int x=e.car.x.integer()-cx,y=e.car.y.integer()-cy;
        bool on=visible && x>-140 && x<140 && y>-90 && y<96;
        bool car_visible=on && e.hp>0 && (!e.flash || e.flash%4<2);
        _cars[i]->set_visible(car_visible);_mounts[i]->set_visible(car_visible && e.archetype!=spawn_profiles::enemy::scout);
        _hp[i]->set_visible(on && e.hp>0 && y>-54);
        _bursts[i]->set_visible(on && e.explosion>0);
        if(on) {
            if(e.hp>0) {
                int direction=((e.car.heading*64/360).integer()+64)%64;
                _cars[i]->set_position(x,y);_mounts[i]->set_position(x,y);
                if(e.archetype==spawn_profiles::enemy::scout)_cars[i]->set_tiles(bn::sprite_items::car_sand_buggy.tiles_item(),direction);
                else if(e.archetype==spawn_profiles::enemy::heavy)_cars[i]->set_tiles(bn::sprite_items::car_truck.tiles_item(),direction);
                else _cars[i]->set_tiles(bn::sprite_items::car.tiles_item(),direction);
                _mounts[i]->set_tiles(bn::sprite_items::car_enemy_gun.tiles_item(),direction);
                _hp[i]->set_position(x,y-19);
                _hp[i]->set_tiles(bn::sprite_items::combat_hp.tiles_item(),e.hp-1);
            }
            if(e.explosion) {
                _bursts[i]->set_position(x,y);
                _bursts[i]->set_tiles(bn::sprite_items::combat_burst.tiles_item(),(24-e.explosion)/6);
            }
        }
    }
    for(int i=0;i<passenger_traffic::passenger_count;++i) {
        if(!_passenger_cars[i])continue;
        const auto& passenger=world.passengers[i];
        const int x=passenger.car.x.integer()-cx,y=passenger.car.y.integer()-cy;
        const bool on=visible && x>-140 && x<140 && y>-90 && y<96;
        const bool car_visible=on && passenger.hp>0 && (!passenger.flash || passenger.flash%4<2);
        _passenger_cars[i]->set_visible(car_visible);
        _passenger_bursts[i]->set_visible(on && passenger.explosion>0);
        if(car_visible) {
            const int direction=((passenger.car.heading*64/360).integer()+64)%64;
            _passenger_cars[i]->set_position(x,y);
            if(passenger.style==1)
                _passenger_cars[i]->set_tiles(bn::sprite_items::car_sand_buggy.tiles_item(),direction);
            else if(passenger.style==2)
                _passenger_cars[i]->set_tiles(bn::sprite_items::car_truck.tiles_item(),direction);
            else _passenger_cars[i]->set_tiles(bn::sprite_items::car.tiles_item(),direction);
        }
        if(on && passenger.explosion) {
            _passenger_bursts[i]->set_position(x,y);
            _passenger_bursts[i]->set_tiles(bn::sprite_items::combat_burst.tiles_item(),
                                            (24-passenger.explosion)/6);
        }
    }
    for(int i=0;i<combat::bullet_count;++i) {
        const auto& b=world.bullets[i]; int x=b.x.integer()-cx,y=b.y.integer()-cy;
        bool on=visible && b.remaining>0 && x>-124 && x<124 && y>-64 && y<84;
        _bullets[i]->set_visible(on);
        if(on) {
            _bullets[i]->set_position(x,y);
            const int frame=b.hostile?1:b.source==combat::Weapon::sides?2:
                            b.source==combat::Weapon::sniper?3:0;
            _bullets[i]->set_tiles(bn::sprite_items::combat_bullet.tiles_item(),frame);
        }
    }
    auto effect=[&](bn::sprite_ptr& sprite,int x,int y,bool active,const bn::sprite_tiles_item& tiles,int frame) {
        bool on=visible && active && x>-140 && x<140 && y>-96 && y<96;
        sprite.set_visible(on);
        if(on){sprite.set_position(x,y);sprite.set_tiles(tiles,frame);}
    };
    effect(*_saw,world.saw_x.integer()-cx,world.saw_y.integer()-cy,world.saw_active,
           bn::sprite_items::weapon_saw.tiles_item(),(world.ticks/2)%4);
    for(int i=0;i<combat::missile_count;++i) {
        const auto& m=world.missiles[i];int x=m.x.integer()-cx,y=m.y.integer()-cy;
        effect(*_missiles[i],x,y,m.remaining>0,bn::sprite_items::weapon_missile.tiles_item(),(m.heading*16/360).integer()%16);
        effect(*_missile_bursts[i],x,y,m.explosion>0,bn::sprite_items::weapon_blast.tiles_item(),(24-m.explosion)/6);
    }
    for(int i=0;i<combat::trap_count;++i) {
        const auto& t=world.traps[i];int x=t.x.integer()-cx,y=t.y.integer()-cy;
        effect(*_traps[i],x,y,t.remaining>0,bn::sprite_items::weapon_trap.tiles_item(),!t.arm && (world.ticks/12)%2);
        effect(*_trap_bursts[i],x,y,t.explosion>0,bn::sprite_items::weapon_blast.tiles_item(),(24-t.explosion)/6);
    }
    int pickup_sprite=0;
    for(const auto& pickup:world.pickups)if(pickup.remaining && pickup_sprite<pickup_sprite_count) {
        int x=pickup.x.integer()-cx,y=pickup.y.integer()-cy;
        effect(*_pickups[pickup_sprite++],x,y,true,bn::sprite_items::salvage_pickup.tiles_item(),pickup.kind);
    }
    while(pickup_sprite<pickup_sprite_count)_pickups[pickup_sprite++]->set_visible(false);
}
