#include "world_map.h"
#include "bn_math.h"
#include "wasteland.h"
#include "generated/wasteland_art.h"
#include "generated/wasteland_recipe.h"
#include "cardinal_exit.h"

namespace world_map {
namespace {
int selected=0;
constexpr const char* town_names[]={
    "DUSTHAVEN","IRONWELL","RED MESA","ASH CROSS","RUSTPOINT","DRY CREEK",
    "CINDER REST","GREYRIDGE","SALT YARD","COPPER RUN","BLACK PUMP","OLD SPAN",
    "TIN ROOF","HOLLOW WELL","WEST RELAY","BRASS GATE","LOW RIDGE","SCRAPFORD",
    "BONE ROAD","NIGHT POST","SUNDER","DEAD RADIO","BURNT FORD","LAST LIGHT"
};
uint32_t hash_text(const char* text,uint32_t hash=2166136261u) {
    while(*text) { hash^=uint8_t(*text++);hash*=16777619u; }
    return hash;
}
}

void select(int index,void(*progress)(int)) {
    selected=index>=0 && index<wasteland::map_count()?index:0;
    wasteland::generate(selected,progress);
}
int index() { return selected; }
int width() { return cave_layout::extent; }
int height() { return cave_layout::extent; }
int initial_spawn() {
    return selected==map_catalog::start_map?map_catalog::start_spawn:0;
}
int start_x() { return player_spawn(initial_spawn()).x; }
int start_y() { return player_spawn(initial_spawn()).y; }
int start_heading() { return player_spawn(initial_spawn()).heading; }
int start_map() { return map_catalog::start_map; }
int start_spawn() { return map_catalog::start_spawn; }
const char* name(int index) { return wasteland::map_name(index); }
const char* town_name(int town_index) {
    if(town_index<0 || town_index>=cave_layout::town_count)return "UNKNOWN OUTPOST";
    constexpr int count=sizeof(town_names)/sizeof(town_names[0]);
    const int first=int(hash_text(map_catalog::maps[selected].id)%uint32_t(count));
    return town_names[(first+town_index)%count];
}
shop_inventory_info shop_inventory(int town_index) {
    BN_ASSERT(town_index>=0 && town_index<cave_layout::town_count,"Invalid shop town index");
    const auto& shop=map_catalog::maps[selected].shops[town_index];
    return {shop.save_ids,shop.count};
}
uint32_t persistent_id(int index) {
    return index>=0 && index<map_catalog::count?hash_text(map_catalog::maps[index].id):0;
}
int find_persistent_id(uint32_t id) {
    for(int index=0;index<map_catalog::count;++index)if(persistent_id(index)==id)return index;
    return -1;
}
uint32_t catalog_signature() {
    uint32_t result=2166136261u;
    for(const auto& map:map_catalog::maps) {
        result=hash_text(map.id,result);
        for(int shift=0;shift<32;shift+=8) { result^=uint8_t(map.seed>>shift);result*=16777619u; }
    }
    return result;
}
int exit_count() {
    int result=0,mask=exit_mask();for(int side=0;side<4;++side)result+=(mask>>side)&1;return result;
}
int exit_mask() { return map_catalog::maps[selected].exit_mask; }
int exit_side(int ordinal) {
    for(int side=0;side<4;++side)if(exit_mask()&(1<<side))if(ordinal--==0)return side;
    return -1;
}
exit_info exit(int side) {
    BN_ASSERT(side>=0 && side<4 && (exit_mask()&(1<<side)),"Invalid cardinal world exit");
    const auto& value=map_catalog::maps[selected].exits[side];
    const auto zone=cardinal_exit::trigger(cardinal_exit::side(side));
    return {zone.x+zone.width/2,zone.y+zone.height/2,zone.width,zone.height,
            value.destination_map,value.destination_side};
}
player_spawn_info exit_arrival(int side) {
    BN_ASSERT(side>=0 && side<4,"Invalid arrival side");
    const auto value=cardinal_exit::arrival_for(cardinal_exit::side(side));
    return {value.x,value.y,value.heading};
}
int nearby_exit(int x,int y) {
    const auto& entry=map_catalog::maps[selected];
    for(int side=0;side<4;++side)if(entry.exit_mask&(1<<side)) {
        const auto value=exit(side);
        if(value.destination_map>=0 && bn::abs(x-value.x)*2<value.width &&
           bn::abs(y-value.y)*2<value.height)return side;
    }
    return -1;
}
int player_spawn_count() { return map_catalog::maps[selected].player_spawn_count; }
player_spawn_info player_spawn(int index) {
    BN_ASSERT(index>=0 && index<player_spawn_count(),"Invalid player spawn index");
    const auto& value=map_catalog::maps[selected].player_spawns[index];
    return {value.x,value.y,value.heading};
}
int route_exit(int target_map) {
    if(target_map<0 || target_map>=map_catalog::count)return -1;
    return map_catalog::next_exit[selected*map_catalog::count+target_map];
}
int route_distance(int from_map,int to_map) {
    if(from_map<0 || from_map>=map_catalog::count || to_map<0 || to_map>=map_catalog::count)return -1;
    const int value=map_catalog::route_distance[from_map*map_catalog::count+to_map];
    return value==255?-1:value;
}
int tile_slots() { return wasteland_art::banks[wasteland::art_bank()].tile_slots; }
int resident_tile_count() { return wasteland_art::banks[wasteland::art_bank()].unique_tiles; }
int chunk_loads() { return 0; }
int chunk_decodes() { return 0; }
int chunk_cache_bytes() { return 0; }
terrain_sample terrain_at(int x,int y) {
    if(x<0 || y<0 || x>=width() || y>=height())return {0,0,3};
    const auto& art=wasteland_art::banks[wasteland::art_bank()];
    const int material=wasteland::material_at(x,y);
    const bool solid=wasteland::layout().solid(x,y) || wasteland::layout().town_solid(x,y);
    return {material,art.material_behaviors[material],solid?3:art.material_surfaces[material]};
}
int material_at(int x,int y) { return terrain_at(x,y).material; }
int material_kind(int material) {
    return wasteland_art::banks[wasteland::art_bank()].material_behaviors[material&255];
}
const char* material_name(int material) {
    const auto& art=wasteland_art::banks[wasteland::art_bank()];
    return art.material_names[art.material_textures[material&255]];
}
bool solid_at(int x,int y) {
    const auto& cave=wasteland::layout();
    return cave.solid(x,y) || cave.town_solid(x,y);
}
int surface_at(int x,int y) {
    return terrain_at(x,y).surface;
}
uint16_t tile_at(int x,int y) {
    const int columns=width()/8,rows=height()/8;
    x=x<0?0:x>=columns?columns-1:x;
    y=y<0?0:y>=rows?rows-1:y;
    return wasteland::tile_at(x,y);
}
int radar_surface_at(int x,int y) { return wasteland::surface_at(x,y); }
const bn::tile* tiles() { return wasteland_art::banks[wasteland::art_bank()].tiles; }
const bn::color* palette() { return wasteland_art::banks[wasteland::art_bank()].palette; }
int palette_colors() { return wasteland_art::banks[wasteland::art_bank()].palette_colors; }
}
