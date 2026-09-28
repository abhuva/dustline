#pragma once

#include "generated/shop_catalog.h"

namespace garage_shop {

inline constexpr uint16_t all_items_mask=uint16_t((1u<<count)-1);
inline constexpr uint32_t save_marker=0x53484f50; // "SHOP"

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

inline int index_for_save_id(int save_id) {
    for(int index=0;index<count;++index)if(catalog[index].save_id==save_id)return index;
    return -1;
}

}
