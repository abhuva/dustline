#pragma once

#include <cstdint>
#include "bn_array.h"
#include "bn_unique_ptr.h"
#include "combat.h"
#include "driving.h"

// Repeatable, presentation-independent radio discoveries. A small random
// population is retained in memory while a run is active, but it is not part
// of the cartridge save. Emulator save states naturally preserve it.
namespace radio_signal {

constexpr int minimum_source_count=3;
constexpr int source_count=5;
constexpr int scrap_reward=12;
constexpr int detection_radius=1024;                 // Hard maximum acquisition range.
constexpr int barrel_hit_radius=11;
constexpr int respawn_min_frames=3*60*60;           // About three minutes.
constexpr int respawn_jitter_frames=2*60*60;        // Up to two extra minutes.

struct source {
    int x=0,y=0;
    uint32_t ready_at=0;
    bool active=false;
};

class system {
public:
    system();
    ~system();
    system(const system&)=delete;
    system& operator=(const system&)=delete;

    void reset(uint32_t session_seed);
    void load(int map_index);
    // Returns scrap awarded during this frame.
    int step(const driving::Car& player,bool receiver_fitted,
             bn::array<combat::Bullet,combat::bullet_count>& bullets);

    const source& item(int index) const;
    int target() const { return _target; }
    int direction() const { return _direction; } // E, SE, S, SW, W, NW, N, NE.
    int strength() const { return _strength; }
    int active_count() const;
    int desired_count() const;
    int next_respawn_frames() const;
    uint32_t clock() const;

private:
    struct storage;
    bn::unique_ptr<storage> _storage;
    int _map_index=0;
    int _target=-1,_direction=-1,_candidate_direction=-1,_candidate_frames=0,_strength=0;

    void _spawn(int index,int avoid_x,int avoid_y);
    void _choose_target(const driving::Car& player,bool receiver_fitted);
    void _update_direction_and_strength(const driving::Car& player);
};

}
