#include "save_data.h"

namespace saves {
namespace {
class writer {
public:
    writer(uint8_t* data,int capacity):_data(data),_capacity(capacity) {}
    void u8(uint8_t value) { if(_position<_capacity)_data[_position]=value;else _ok=false;++_position; }
    void u16(uint16_t value) { u8(uint8_t(value));u8(uint8_t(value>>8)); }
    void u32(uint32_t value) { u16(uint16_t(value));u16(uint16_t(value>>16)); }
    void s32(int32_t value) { u32(uint32_t(value)); }
    int size() const { return _position; }
    bool ok() const { return _ok && _position<=_capacity; }
private:
    uint8_t* _data;
    int _capacity,_position=0;
    bool _ok=true;
};

class reader {
public:
    reader(const uint8_t* data,int size):_data(data),_size(size) {}
    uint8_t u8() { if(_position<_size)return _data[_position++];_ok=false;return 0; }
    uint16_t u16() { uint16_t a=u8(),b=u8();return uint16_t(a|(b<<8)); }
    uint32_t u32() { uint32_t a=u16(),b=u16();return a|(b<<16); }
    int32_t s32() { return int32_t(u32()); }
    int remaining() const { return _size-_position; }
    bool done() const { return _ok && _position==_size; }
private:
    const uint8_t* _data;
    int _size,_position=0;
    bool _ok=true;
};

void write_mission(writer& out,const mission_state& value) {
    out.s32(value.map_seed);out.s32(value.kind);out.s32(value.status);
    out.s32(value.origin_map);out.s32(value.origin_town);out.s32(value.target_map);
    out.s32(value.target_town);out.s32(value.target_spawn);out.s32(value.target_x);out.s32(value.target_y);
    out.s32(value.progress);out.s32(value.goal);out.s32(value.reward);out.s32(value.contract_serial);
    out.s32(value.credits);out.s32(value.completed);out.s32(value.serial);
}

void read_mission(reader& in,mission_state& value) {
    value.map_seed=in.s32();value.kind=in.s32();value.status=in.s32();
    value.origin_map=in.s32();value.origin_town=in.s32();value.target_map=in.s32();
    value.target_town=in.s32();value.target_spawn=in.s32();value.target_x=in.s32();value.target_y=in.s32();
    value.progress=in.s32();value.goal=in.s32();value.reward=in.s32();value.contract_serial=in.s32();
    value.credits=in.s32();value.completed=in.s32();value.serial=in.s32();
}

uint32_t crc_byte(uint32_t crc,uint8_t value) {
    crc^=value;
    for(int bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320u&uint32_t(-int32_t(crc&1)));
    return crc;
}

uint32_t crc_u16(uint32_t crc,uint16_t value) {
    crc=crc_byte(crc,uint8_t(value));return crc_byte(crc,uint8_t(value>>8));
}

uint32_t crc_u32(uint32_t crc,uint32_t value) {
    crc=crc_u16(crc,uint16_t(value));return crc_u16(crc,uint16_t(value>>16));
}

bool in_range(int32_t value,int32_t low,int32_t high) { return value>=low && value<=high; }
}

int encode(const game_state& source,uint8_t* destination,int capacity) {
    writer out(destination,capacity);
    out.u32(source.map_id);out.u32(source.map_seed);out.u32(source.map_signature);out.u32(source.catalog_signature);
    out.s32(source.x);out.s32(source.y);out.s32(source.heading);
    out.u8(source.setup);out.u8(source.car_type);out.u8(source.battery);
    out.s32(source.acceleration);out.s32(source.max_speed);out.s32(source.grip);out.s32(source.steer);
    out.s32(source.coast_drag);out.s32(source.brake_force);out.s32(source.mass);
    out.u8(source.music_volume);out.u8(source.sound_volume);out.u8(source.master_muted);
    for(int value:source.best_laps)out.s32(value);
    out.s32(source.laps);out.s32(source.scrap);out.s32(source.duplicate_blueprints);
    out.u8(source.blueprint_mask);out.u8(source.crafted_mask);
    out.s32(source.hp);out.s32(source.shield);out.s32(source.energy);
    out.u8(source.front_weapon);out.u8(source.side_weapon);out.u8(source.special_weapon);
    write_mission(out,source.mission);
    out.s32(source.race_map_seed);out.s32(source.race_serial);
    out.u32(source.radio_collected_low);out.u32(source.radio_collected_high);
    return out.ok()?out.size():-1;
}

bool decode(const uint8_t* source,int size,game_state& destination) {
    if(size<=0 || size>max_payload_size)return false;
    game_state value;reader in(source,size);
    value.map_id=in.u32();value.map_seed=in.u32();value.map_signature=in.u32();value.catalog_signature=in.u32();
    value.x=in.s32();value.y=in.s32();value.heading=in.s32();
    value.setup=in.u8();value.car_type=in.u8();value.battery=in.u8();
    value.acceleration=in.s32();value.max_speed=in.s32();value.grip=in.s32();value.steer=in.s32();
    value.coast_drag=in.s32();value.brake_force=in.s32();value.mass=in.s32();
    value.music_volume=in.u8();value.sound_volume=in.u8();value.master_muted=in.u8();
    for(int32_t& lap:value.best_laps)lap=in.s32();
    value.laps=in.s32();value.scrap=in.s32();value.duplicate_blueprints=in.s32();
    value.blueprint_mask=in.u8();value.crafted_mask=in.u8();
    value.hp=in.s32();value.shield=in.s32();value.energy=in.s32();
    value.front_weapon=in.u8();value.side_weapon=in.u8();value.special_weapon=in.u8();
    read_mission(in,value.mission);
    value.race_map_seed=in.s32();value.race_serial=in.s32();
    // Early version-one saves ended here. The appended words now hold shop
    // ownership plus a marker, but remain optional so both layouts load.
    if(in.remaining()==8) {
        value.radio_collected_low=in.u32();value.radio_collected_high=in.u32();
    } else if(in.remaining()!=0)return false;
    if(!in.done() || !valid(value))return false;
    destination=value;return true;
}

bool valid(const game_state& value) {
    if(!value.map_id || value.setup>=3 || value.car_type>=5 || value.battery>=3)return false;
    constexpr int32_t world_limit=8192*4096;
    if(!in_range(value.x,0,world_limit) || !in_range(value.y,0,world_limit) ||
       !in_range(value.heading,0,360*4096))return false;
    if(value.music_volume>10 || value.sound_volume>10 || value.master_muted>1)return false;
    if(!in_range(value.acceleration,0,2048) || !in_range(value.max_speed,1024,49152) ||
       !in_range(value.grip,0,4096) || !in_range(value.steer,0,49152) ||
       !in_range(value.coast_drag,0,205) || !in_range(value.brake_force,0,1229) ||
       !in_range(value.mass,100,10000))return false;
    if(!in_range(value.laps,0,100000000) || !in_range(value.scrap,0,100000000) ||
       !in_range(value.duplicate_blueprints,0,100000000))return false;
    if(value.blueprint_mask&~7u || value.crafted_mask&~7u)return false;
    if(!in_range(value.hp,0,125) || !in_range(value.shield,0,20) || !in_range(value.energy,0,150))return false;
    if((value.front_weapon!=0 && value.front_weapon!=5 && value.front_weapon!=7) ||
       (value.side_weapon!=2 && value.side_weapon!=5 && value.side_weapon!=8) ||
       (value.special_weapon!=3 && value.special_weapon!=4 && value.special_weapon!=5 &&
        value.special_weapon!=6))return false;
    const auto& mission=value.mission;
    if(!in_range(mission.kind,0,2) || !in_range(mission.status,0,2) ||
       mission.credits<0 || mission.completed<0 || mission.serial<0 || value.race_serial<0)return false;
    for(int lap:value.best_laps)if(lap<0)return false;
    return true;
}

uint32_t checksum(const slot_record& slot) {
    uint32_t crc=0xffffffffu;
    crc=crc_u32(crc,slot.header.magic);crc=crc_u16(crc,slot.header.version);
    crc=crc_u16(crc,slot.header.payload_size);crc=crc_u32(crc,slot.header.generation);
    for(int index=0;index<slot.header.payload_size && index<max_payload_size;++index)
        crc=crc_byte(crc,slot.payload[index]);
    return crc^0xffffffffu;
}

bool slot_valid(const slot_record& slot) {
    if(slot.header.committed!=committed_magic || slot.header.magic!=magic ||
       slot.header.version!=format_version || !slot.header.payload_size ||
       slot.header.payload_size>max_payload_size || slot.header.checksum!=checksum(slot))return false;
    game_state decoded;return decode(slot.payload,slot.header.payload_size,decoded);
}

bool make_slot(const game_state& source,uint32_t generation,slot_record& destination) {
    destination=slot_record();
    const int size=encode(source,destination.payload,max_payload_size);
    if(size<0)return false;
    destination.header.magic=magic;destination.header.version=format_version;
    destination.header.payload_size=uint16_t(size);destination.header.generation=generation;
    destination.header.checksum=checksum(destination);destination.header.committed=committed_magic;
    return true;
}

int select_slot(const slot_record& first,const slot_record& second) {
    const bool first_valid=slot_valid(first),second_valid=slot_valid(second);
    if(!first_valid)return second_valid?1:-1;
    if(!second_valid)return 0;
    return int32_t(first.header.generation-second.header.generation)>0?0:1;
}
}
