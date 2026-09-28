#pragma once
#include <cstdint>

namespace spawn_profiles {
enum class enemy : uint8_t { scout, raider, heavy };

struct profile {
    uint8_t id=0;
    enemy enemy_type=enemy::raider;
    uint16_t respawn_frames=1800;
    uint8_t scrap_chance=70;
    uint8_t scrap_min=1;
    uint8_t scrap_max=3;
    uint8_t energy_chance=25;
    uint8_t energy_min=8;
    uint8_t energy_max=16;
};

inline constexpr profile fallback_profiles[]={
    {0,enemy::raider,1800,70,1,3,25,8,16}
};

inline const profile& find(const profile* profiles,int count,uint8_t id) {
    for(int index=0;index<count;++index) if(profiles[index].id==id) return profiles[index];
    return fallback_profiles[0];
}
}
