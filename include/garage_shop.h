#pragma once

#include <cstdint>
#include "combat.h"

namespace garage_shop {

enum class kind : uint8_t { upgrade,weapon };
enum class upgrade : uint8_t { salvage_magnet,tuned_injector,reinforced_plating };

struct item {
    const char* name;
    const char* detail;
    int gold;
    int scrap;
    kind type;
    int value;
};

inline constexpr int count=9;
inline constexpr uint16_t all_items_mask=uint16_t((1u<<count)-1);
inline constexpr uint32_t save_marker=0x53484f50; // "SHOP"
inline constexpr item catalog[count]={
    {"SALVAGE MAGNET","WIDER PICKUP RANGE",100,6,kind::upgrade,int(upgrade::salvage_magnet)},
    {"TUNED INJECTOR","MORE ACCELERATION",260,14,kind::upgrade,int(upgrade::tuned_injector)},
    {"REINFORCED PLATE","+25 MAX HEALTH",360,20,kind::upgrade,int(upgrade::reinforced_plating)},
    {"TWIN SIDE GUNS","FIRES LEFT + RIGHT",150,8,kind::weapon,int(combat::Weapon::sides)},
    {"LONG SNIPER","SLOW / RANGE 520",320,18,kind::weapon,int(combat::Weapon::sniper)},
    {"FRONT SHOOTER","TWIN FORWARD FIRE",240,12,kind::weapon,int(combat::Weapon::front_shooter)},
    {"SEEKER MISSILE","HOMING TOP WEAPON",300,16,kind::weapon,int(combat::Weapon::missile)},
    {"REAR TRAP","ARMED ROAD MINE",220,12,kind::weapon,int(combat::Weapon::trap)},
    {"SIGNAL RADIO","FINDS SALVAGE",180,10,kind::weapon,int(combat::Weapon::radio)},
};

constexpr uint16_t item_bit(int index) { return uint16_t(1u<<index); }
constexpr bool owned(uint16_t mask,int index) { return (mask&item_bit(index))!=0; }

inline uint16_t weapon_mask(uint16_t shop_mask) {
    uint16_t result=uint16_t(1u<<int(combat::Weapon::gun));
    for(int index=0;index<count;++index)if(owned(shop_mask,index) && catalog[index].type==kind::weapon)
        result|=uint16_t(1u<<catalog[index].value);
    return result;
}

inline int item_for_weapon(combat::Weapon weapon) {
    for(int index=0;index<count;++index)
        if(catalog[index].type==kind::weapon && catalog[index].value==int(weapon))return index;
    return -1;
}

}
