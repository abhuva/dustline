#pragma once
#include <cstdint>
#include "cave_layout.h"
#include "enemy_spawns.h"

// Small session-only contract state. It observes world events but owns no
// simulation, rendering, or map objects.
namespace missions {
enum class type { none=0, courier=1, extermination=2 };
enum class status { none=0, active=1, complete=2 };

struct contract {
    type kind=type::none;
    status state=status::none;
    int origin_map=-1,origin_town=-1,target_map=-1,target_town=-1,target_spawn=-1;
    int target_x=0,target_y=0;
    int progress=0,goal=0,reward=0,serial=0;
};

const char* type_name(type value);

class manager {
public:
    void reset(uint32_t map_seed);
    contract offer(type kind,int origin_map,int origin_town,const cave_layout& layout,
                   const enemy_spawns& spawns) const;
    bool accept(type kind,int origin_map,int origin_town,const cave_layout& layout,
                const enemy_spawns& spawns);
    void on_map_loaded(int map_id,const cave_layout& layout,const enemy_spawns& spawns);
    bool on_town_enter(int map_id,int town_id);
    bool on_enemy_destroyed(int map_id,int spawn_id);
    void acknowledge();
    void award_credits(int value) { if(value>0)_credits+=value; }
    bool spend_credits(int value) { if(value<0 || value>_credits)return false;_credits-=value;return true; }

    const contract& current() const { return _current; }
    int credits() const { return _credits; }
    int completed() const { return _completed; }
    int serial() const { return _serial; }
    uint32_t map_seed() const { return _map_seed; }
    bool restore(uint32_t map_seed,const contract& current,int credits,int completed,int serial);

private:
    void _complete();
    uint32_t _map_seed=0;
    contract _current;
    int _credits=0,_completed=0,_serial=0;
};
}
