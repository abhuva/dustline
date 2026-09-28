#pragma once

#include <cstdint>

// Stable, presentation-independent cartridge format.  This layer deliberately
// knows nothing about Butano or SRAM so its codec can also be host-tested.
namespace saves {

constexpr uint32_t magic=0x54535544; // "DUST" in little endian.
constexpr uint32_t committed_magic=0x45564153; // "SAVE".
constexpr uint16_t legacy_format_version=1;
constexpr uint16_t format_version=2;
constexpr int max_payload_size=256;
constexpr int slot_size=512;
constexpr int slot_count=2;
constexpr int shop_ownership_word_capacity=4;

struct mission_state {
    int32_t map_seed=0;
    int32_t kind=0,status=0;
    int32_t origin_map=-1,origin_town=-1,target_map=-1,target_town=-1,target_spawn=-1;
    int32_t target_x=0,target_y=0,progress=0,goal=0,reward=0,contract_serial=0;
    int32_t credits=0,completed=0,serial=0;
};

struct game_state {
    uint32_t map_id=0,map_seed=0,map_signature=0,catalog_signature=0;
    int32_t x=0,y=0,heading=0;
    uint8_t setup=0,car_type=0,battery=0;
    int32_t acceleration=0,max_speed=0,grip=0,steer=0,coast_drag=0,brake_force=0,mass=0;
    uint8_t music_volume=0,sound_volume=10,master_muted=0;
    int32_t best_laps[3]={0,0,0};
    int32_t laps=0;
    // Kept in the version-one byte layout for old cartridges. New saves write
    // zero to the retired counters and mirror the three upgrade bits in crafted_mask.
    int32_t scrap=0,duplicate_blueprints=0;
    uint8_t blueprint_mask=0,crafted_mask=0;
    int32_t hp=100,shield=20,energy=100;
    uint8_t front_weapon=0,side_weapon=5,special_weapon=5;
    mission_state mission;
    int32_t race_map_seed=0,race_serial=0;
    // Version two stores a counted immutable-save-ID bitset. The presence flag
    // is set by both v2 and marked late-v1 profiles; early v1 saves migrate
    // their crafted/equipped items in the gameplay layer.
    uint32_t shop_owned[shop_ownership_word_capacity]{};
    uint8_t shop_ownership_present=0;
};

struct slot_header {
    uint32_t magic=0;
    uint16_t version=0;
    uint16_t payload_size=0;
    uint32_t generation=0;
    uint32_t checksum=0;
    uint32_t committed=0;
};

struct slot_record {
    slot_header header;
    uint8_t payload[max_payload_size]{};
    uint8_t padding[slot_size-sizeof(slot_header)-max_payload_size]{};
};

static_assert(sizeof(slot_header)==20,"Unexpected save header layout");
static_assert(sizeof(slot_record)==slot_size,"Unexpected save slot layout");

int encode(const game_state& source,uint8_t* destination,int capacity);
bool decode(uint16_t version,const uint8_t* source,int size,game_state& destination);
inline bool decode(const uint8_t* source,int size,game_state& destination) {
    return decode(format_version,source,size,destination);
}
bool valid(const game_state& value);
uint32_t checksum(const slot_record& slot);
bool slot_valid(const slot_record& slot);
bool make_slot(const game_state& source,uint32_t generation,slot_record& destination);
// Returns -1 if neither slot is usable, otherwise the newest valid slot index.
int select_slot(const slot_record& first,const slot_record& second);

}
