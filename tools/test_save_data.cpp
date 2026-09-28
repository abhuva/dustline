#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include "save_data.h"

namespace {
saves::game_state example() {
    saves::game_state value;
    value.map_id=0x12345678;value.map_seed=0x89abcdef;value.map_signature=0x10203040;
    value.catalog_signature=0x55667788;value.x=4000*4096;value.y=2200*4096;value.heading=135*4096;
    value.setup=2;value.car_type=4;value.battery=2;
    value.acceleration=135;value.max_speed=13000;value.grip=492;value.steer=11878;
    value.coast_drag=4;value.brake_force=369;value.mass=2300;
    value.music_volume=7;value.sound_volume=8;value.master_muted=1;
    value.best_laps[0]=3600;value.best_laps[1]=4200;value.laps=12;
    value.scrap=44;value.duplicate_blueprints=3;value.blueprint_mask=7;value.crafted_mask=5;
    value.hp=73;value.shield=11;value.energy=91;value.front_weapon=0;value.side_weapon=2;value.special_weapon=4;
    value.mission.map_seed=int32_t(0x89abcdef);value.mission.kind=2;value.mission.status=1;
    value.mission.origin_map=0;value.mission.origin_town=1;value.mission.target_map=3;
    value.mission.target_town=-1;value.mission.target_spawn=17;value.mission.target_x=1234;
    value.mission.target_y=5678;value.mission.goal=1;value.mission.reward=900;
    value.mission.contract_serial=4;value.mission.credits=1250;value.mission.completed=2;value.mission.serial=5;
    value.race_map_seed=int32_t(0x89abcdef);value.race_serial=7;
    value.shop_owned[0]=0x80000005;value.shop_owned[1]=0x00000002;
    value.shop_ownership_present=1;
    return value;
}
}

int main() {
    const auto source=example();
    uint8_t bytes[saves::max_payload_size]{};
    const int size=saves::encode(source,bytes,sizeof(bytes));
    assert(size>0 && size<saves::max_payload_size);
    saves::game_state decoded;
    assert(saves::decode(bytes,size,decoded));
    assert(decoded.map_id==source.map_id && decoded.x==source.x && decoded.car_type==source.car_type);
    assert(decoded.scrap==source.scrap && decoded.mission.target_spawn==17 && decoded.race_serial==7 &&
           decoded.shop_owned[0]==source.shop_owned[0] && decoded.shop_owned[1]==source.shop_owned[1] &&
           decoded.shop_ownership_present);

    saves::slot_record first,second;
    auto shop=source;
    shop.front_weapon=7;shop.side_weapon=8;shop.special_weapon=6;
    shop.shop_owned[0]=0x1ff;shop.shop_ownership_present=1;
    assert(saves::make_slot(shop,8,first));
    saves::game_state decoded_shop;
    assert(first.header.version==2);
    assert(saves::decode(first.header.version,first.payload,first.header.payload_size,decoded_shop));
    assert(decoded_shop.front_weapon==7 && decoded_shop.side_weapon==8 &&
           decoded_shop.special_weapon==6 && decoded_shop.shop_owned[0]==0x1ff);

    // Reconstruct both historical v1 payload lengths from the unchanged common prefix.
    constexpr int v1_common_size=179;
    saves::game_state legacy;
    assert(saves::decode(1,bytes,v1_common_size,legacy));
    assert(!legacy.shop_ownership_present && !legacy.shop_owned[0]);
    uint8_t late_v1[saves::max_payload_size]{};
    std::memcpy(late_v1,bytes,v1_common_size);
    const uint32_t owned=0x1ff,marker=0x53484f50;
    std::memcpy(late_v1+v1_common_size,&owned,4);std::memcpy(late_v1+v1_common_size+4,&marker,4);
    assert(saves::decode(1,late_v1,v1_common_size+8,legacy));
    assert(legacy.shop_ownership_present && legacy.shop_owned[0]==owned);

    first=saves::slot_record();
    assert(saves::make_slot(source,4,first));
    assert(saves::slot_valid(first) && saves::select_slot(first,second)==0);
    assert(saves::make_slot(source,5,second));
    assert(saves::select_slot(first,second)==1);

    auto interrupted=second;interrupted.header.committed=0;
    assert(!saves::slot_valid(interrupted) && saves::select_slot(first,interrupted)==0);
    auto corrupt=second;corrupt.payload[9]^=0x80;
    assert(!saves::slot_valid(corrupt) && saves::select_slot(first,corrupt)==0);
    auto wrong_version=second;++wrong_version.header.version;
    assert(!saves::slot_valid(wrong_version));

    auto bad_count=second;bad_count.payload[v1_common_size]=5;
    bad_count.header.checksum=saves::checksum(bad_count);
    assert(!saves::slot_valid(bad_count));

    assert(saves::make_slot(source,0xffffffffu,first));
    assert(saves::make_slot(source,1,second));
    assert(saves::select_slot(first,second)==1);
    std::cout<<"PASS versioned save codec, CRC, redundant-slot recovery and generation wrap\n";
}
