#pragma once

#include "generated/shop_catalog.h"

namespace garage_shop {

inline constexpr int max_visible_stock=9;
inline constexpr int ownership_word_count=max_save_id/32+1;
struct ownership {
    uint32_t words[ownership_word_count]{};
};

inline bool owned_save_id(const ownership& value,int save_id) {
    return save_id>=0 && save_id<=max_save_id &&
           (value.words[save_id/32]&(uint32_t(1)<<unsigned(save_id%32)))!=0;
}
inline bool owned(const ownership& value,int index) {
    return index>=0 && index<count && owned_save_id(value,catalog[index].save_id);
}
inline void grant_save_id(ownership& value,int save_id) {
    if(save_id>=0 && save_id<=max_save_id)value.words[save_id/32]|=uint32_t(1)<<unsigned(save_id%32);
}
inline void grant(ownership& value,int index) {
    if(index>=0 && index<count)grant_save_id(value,catalog[index].save_id);
}
inline bool owns_upgrade(const ownership& value,upgrade candidate) {
    for(int index=0;index<count;++index)
        if(catalog[index].type==kind::upgrade && catalog[index].value==int(candidate))return owned(value,index);
    return false;
}
inline uint8_t legacy_upgrade_mask(const ownership& value) {
    uint8_t result=0;
    for(int index=0;index<3;++index)if(owns_upgrade(value,upgrade(index)))result|=uint8_t(1u<<index);
    return result;
}

inline uint16_t weapon_mask(const ownership& shop_owned) {
    uint16_t result=uint16_t(1u<<int(combat::Weapon::gun));
    for(int index=0;index<count;++index)if(owned(shop_owned,index) && catalog[index].type==kind::weapon)
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
