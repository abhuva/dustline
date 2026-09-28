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
    value.radio_collected_low=0x80000005;value.radio_collected_high=0x00000002;
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
           decoded.radio_collected_low==source.radio_collected_low &&
           decoded.radio_collected_high==source.radio_collected_high);

    saves::slot_record first,second;
    auto shop=source;
    shop.front_weapon=7;shop.side_weapon=8;shop.special_weapon=6;
    shop.radio_collected_low=0x1ff;shop.radio_collected_high=0x53484f50;
    assert(saves::make_slot(shop,8,first));
    saves::game_state decoded_shop;
    assert(saves::decode(first.payload,first.header.payload_size,decoded_shop));
    assert(decoded_shop.front_weapon==7 && decoded_shop.side_weapon==8 &&
           decoded_shop.special_weapon==6 && decoded_shop.radio_collected_low==0x1ff);

    // Early version-one profiles have no appended shop words and must still decode.
    saves::game_state legacy;
    assert(saves::decode(bytes,size-8,legacy));
    assert(!legacy.radio_collected_low && !legacy.radio_collected_high);

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

    assert(saves::make_slot(source,0xffffffffu,first));
    assert(saves::make_slot(source,1,second));
    assert(saves::select_slot(first,second)==1);
    std::cout<<"PASS versioned save codec, CRC, redundant-slot recovery and generation wrap\n";
}
