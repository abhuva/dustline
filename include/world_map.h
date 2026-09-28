#pragma once
#include "bn_tile.h"
#include "bn_color.h"

// Scene data stays in ROM; only nearby chunk IDs are decoded into fixed RAM.
// Physics queries surfaces without knowing the renderer.
namespace world_map {
void select(int index,void(*progress)(int)=nullptr);
int index();
int width();
int height();
int start_x();
int start_y();
int start_heading();
int start_map();
int start_spawn();
const char* name(int index);
const char* town_name(int town_index);
uint32_t persistent_id(int index);
int find_persistent_id(uint32_t id);
uint32_t catalog_signature();
struct portal_info {
    int x,y,width,height;
    int destination_map,destination_spawn;
};
struct player_spawn_info { int x,y,heading; };
int portal_count();
portal_info portal(int index);
int nearby_portal(int x,int y);
int player_spawn_count();
player_spawn_info player_spawn(int index);
int route_portal(int target_map);
int route_distance(int from_map,int to_map);
int tile_slots();
// Nonzero when the complete tile vocabulary fits in the reserved VRAM.
int resident_tile_count();
int chunk_loads();
int chunk_decodes();
int chunk_cache_bytes();
struct terrain_sample { int material; int kind; int surface; };
terrain_sample terrain_at(int x,int y);
int material_at(int x,int y);
int material_kind(int material);
const char* material_name(int material);
int surface_at(int x, int y);
bool solid_at(int x,int y);
int radar_surface_at(int x,int y);
uint16_t tile_at(int x, int y);
const bn::tile* tiles();
const bn::color* palette();
int palette_colors();
}
