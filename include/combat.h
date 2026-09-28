#pragma once
#include "bn_array.h"
#include "driving.h"
#include "enemy_spawns.h"

// Bounded, fixed-point simulation. No sprites, audio or allocation in this layer.
namespace combat {
constexpr int enemy_count=5, bullet_count=24, enemy_hp=3, player_max_hp=100,player_max_shield=20,player_max_energy=100;
constexpr int spawn_range=560, despawn_range=800, spawn_cooldown=1800;
constexpr int player_range=120, enemy_range=480, player_interval=10, enemy_interval=player_interval;
constexpr int shield_recharge_delay=180,shield_recharge_interval=15,revive_invulnerability=120;
// Keep the persisted IDs stable: empty was historically encoded as 5, so new
// equipment must be appended after it rather than reusing that value.
enum class Weapon {
    gun=0, chainsaw=1, sides=2, missile=3, trap=4, empty=5, radio=6,
    sniper=7, front_shooter=8, count=9
};
enum class MountSlot { front, side, special, count };
constexpr int weapon_count=int(Weapon::count), missile_count=2, trap_count=6;
constexpr int mount_slot_count=int(MountSlot::count);
constexpr int pickup_count=10;
constexpr int saw_radius=10,saw_damage=2,missile_damage=3,trap_damage=3,sniper_damage=4;
constexpr int sniper_range=520,sniper_interval=75,front_shooter_interval=18;
constexpr int weapon_energy_costs[weapon_count]={0,1,2,8,6,0,0,5,3};
const char* weapon_name(Weapon weapon);
struct Enemy {
    driving::Car car;
    driving::Input input{false,false,0};
    int hp=0, cooldown=0, flash=0, explosion=0, reverse=0, stalled=0;
    int avoidance=0, recoveries=0;
    int side=1,maneuver=0,goal_x=0,goal_y=0,home_x=0,home_y=0;
    int vehicle_avoidance=0,moving_frames=0;
    int spawn_id=-1;
    spawn_profiles::enemy archetype=spawn_profiles::enemy::raider;
};
struct Bullet {
    bn::fixed x=0,y=0,vx=0,vy=0;
    int remaining=0,damage=1;
    Weapon source=Weapon::gun;
    bool hostile=false;
};
struct Missile {
    bn::fixed x=0,y=0,vx=0,vy=0,heading=0;
    int remaining=0,age=0,target_spawn=-1,explosion=0;
};
struct Trap {
    bn::fixed x=0,y=0;
    int remaining=0,arm=0,explosion=0;
};
struct Pickup {
    bn::fixed x=0,y=0;
    int remaining=0;
    uint8_t kind=0,value=0; // 0 scrap amount, 1 energy amount.
};
class World {
public:
    bn::array<Enemy,enemy_count> enemies;
    bn::array<Bullet,bullet_count> bullets;
    bn::array<Missile,missile_count> missiles;
    bn::array<Trap,trap_count> traps;
    bn::array<Pickup,pickup_count> pickups;
    Weapon weapon=Weapon::gun;
    bn::array<int,weapon_count> weapon_shots{},weapon_hits{};
    bn::fixed saw_x=0,saw_y=0;
    bool saw_active=false;
    int guidance_updates=0,trap_explosions=0;
    enemy_spawns spawns;
    int spawned=0,despawned=0;
    int player_hp=player_max_hp,player_shield=player_max_shield;
    int player_energy=player_max_energy;
    int player_shield_delay=0,player_invulnerability=0;
    int player_hits=0, player_shots=0, enemy_shots=0;
    int hits=0,kills=0,wall_hits=0,expired=0,ticks=0;
    int bumps=0,player_bumps=0,last_pair=-1;
    bn::fixed last_bump=0;
    bn::array<int,enemy_count> destroyed_spawns{};
    int destroyed_count=0;
    bool fired=false,impact=false,destroyed=false,player_destroyed=false;
    int collected_scrap=0;
    int collected_energy=0;
    void reset(const driving::Car& player,bool enabled,cave_layout::progress_fn progress=nullptr);
    BN_CODE_IWRAM void step(driving::Car& player,bool normal_fire,bool special_fire);
    void clear_bullets();
    void refill_player();
    void revive_player();
    void set_max_energy(int maximum);
    bool fit_weapon(MountSlot slot,Weapon weapon);
    void set_salvage_magnet(bool enabled) { _salvage_magnet=enabled; }
    void set_reinforced_plating(bool enabled) {
        int next=player_max_hp+(enabled?25:0);if(next>_player_max_hp)player_hp+=next-_player_max_hp;_player_max_hp=next;
    }
    int weapon_mask() const { return _weapon_mask; }
    Weapon fitted_weapon(MountSlot slot) const { return _fitted[int(slot)]; }
    bool weapon_enabled(Weapon candidate) const {
        if(candidate==Weapon::empty || candidate==Weapon::radio)return false;
        for(Weapon fitted:_fitted)if(fitted==candidate)return true;
        return false;
    }
    bool has_weapons() const { return _weapon_mask!=0; }
    int max_player_hp() const { return _player_max_hp; }
    int max_player_energy() const { return _player_max_energy; }
    int living() const;
private:
    bool _enabled=false;
    int _weapon_mask=0;
    bn::array<Weapon,mount_slot_count> _fitted={Weapon::gun,Weapon::empty,Weapon::empty};
    bool _salvage_magnet=false;
    int _player_max_hp=player_max_hp;
    int _player_max_energy=player_max_energy;
    const spawn_profiles::profile* _profiles=spawn_profiles::fallback_profiles;
    int _profile_count=1;
    bn::array<int,weapon_count> _cooldowns{};
    bn::array<int,enemy_count*(enemy_count+1)/2> _contact_cooldowns{};
    void stream(const driving::Car& player,bool initial=false);
    BN_CODE_IWRAM void think(Enemy& enemy,const driving::Car& player);
    bool shoot(const driving::Car& car,bool hostile,Weapon source=Weapon::gun,int angle=0,
               int lateral=0,int range=player_range,int speed=6,int damage=1);
    void fire_weapon(const driving::Car& player,Weapon candidate);
    void update_specials(const driving::Car& player);
    void damage(Enemy& enemy,int amount,Weapon source);
    void drop_loot(Enemy& enemy);
    void update_pickups(const driving::Car& player);
    void damage_player(int amount);
    void explode(int x,int y,int radius,int amount,Weapon source);
    BN_CODE_IWRAM bool hit_at(Bullet& bullet,const driving::Car& player);
};
}
