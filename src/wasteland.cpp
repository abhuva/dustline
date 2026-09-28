#include "wasteland.h"
#include "generated/wasteland_art.h"
#include "generated/wasteland_recipe.h"
#include "bn_unique_ptr.h"
#include "wasteland_tiles.h"
#include "decoration_layout.h"
#include "cardinal_exit.h"

namespace wasteland {
namespace {
bn::unique_ptr<cave_layout> current;
struct ground_grid { uint8_t ids[64*64]; };
bn::unique_ptr<ground_grid> ground;
bn::unique_ptr<road_network> roads;
struct spawn_locations { uint16_t xy[enemy_spawns::capacity*2];uint8_t profiles[enemy_spawns::capacity];int count=0; };
bn::unique_ptr<spawn_locations> spawn_points;
bn::unique_ptr<decoration_layout> decoration;
int serial=0,updates=0;
int selected_map=0;
cave_layout::progress_fn loading_callback=nullptr;
int progress_done=0,progress_stage=0,progress_total=1,progress_last=0;
void begin_stage(int nodes) { progress_stage=nodes; }
void progress(int value) {
    ++updates;
    value=value<0?0:value>100?100:value;
    int overall=(progress_done*100+value*progress_stage)/progress_total;
    if(overall<progress_last)overall=progress_last;
    progress_last=overall;
    if(loading_callback)loading_callback(overall);
}
void finish_stage() {
    progress(100);
    progress_done+=progress_stage;
    progress_stage=0;
}
}
void generate(int map_index,cave_layout::progress_fn callback) {
    BN_ASSERT(map_index>=0 && map_index<map_catalog::count,"Invalid map catalog index");
    selected_map=map_index;
    const auto& recipe=map_catalog::maps[selected_map];
    const uint32_t seed=recipe.seed;
    if(!current) current.reset(new cave_layout());
    bn::unique_ptr<mapgen::workspace> scratch(new mapgen::workspace());
    loading_callback=callback; updates=0;
    progress_done=0;progress_stage=0;progress_last=0;
    progress_total=recipe.count+recipe.material_count+recipe.spawn_count+recipe.decoration_count+(recipe.spawn_count?1:0);
    if(!progress_total)progress_total=1;
    if(loading_callback)loading_callback(0);
    begin_stage(recipe.count);
    auto result=mapgen::execute(recipe.nodes,recipe.count,seed,*scratch,progress);
    BN_ASSERT(result.status==mapgen::error::ok && result.type==mapgen::kind::world,"Invalid map recipe");
    finish_stage();
    *current=scratch->layout;
    if(scratch->roads.width) {
        if(!roads) roads.reset(new road_network());
        *roads=scratch->roads;
        for(int side=0;side<4;++side)if(recipe.exit_mask&(1<<side))
            BN_ASSERT(cardinal_exit::apply(*current,*roads,scratch->scratch,cardinal_exit::side(side)),
                      "Could not connect cardinal map exit");
        cardinal_exit::finalize(*current,recipe.exit_mask);
    } else roads.reset();
    if(recipe.material_count) {
        begin_stage(recipe.material_count);
        auto materials=mapgen::execute(recipe.material_nodes,recipe.material_count,seed,*scratch,progress);
        BN_ASSERT(materials.status==mapgen::error::ok && materials.type==mapgen::kind::material,"Invalid ground recipe");
        finish_stage();
        if(!ground) ground.reset(new ground_grid());
        for(int i=0;i<64*64;++i) ground->ids[i]=materials.data[i];
    } else ground.reset();
    if(recipe.spawn_count) {
        begin_stage(recipe.spawn_count);
        auto field=mapgen::execute(recipe.spawn_nodes,recipe.spawn_count,seed,*scratch,progress);
        BN_ASSERT(field.status==mapgen::error::ok && field.type==mapgen::kind::spawns);
        finish_stage();
        const auto& config=recipe.spawn_nodes[recipe.spawn_count-1];
        bn::unique_ptr<enemy_spawns> generated(new enemy_spawns());
        begin_stage(1);
        generated->generate_recipe(*current,field.data,field.auxiliary,config.p[0],config.p[1],config.p[2],config.stream,progress);
        finish_stage();
        if(!spawn_points)spawn_points.reset(new spawn_locations());
        spawn_points->count=generated->count;
        for(int i=0;i<generated->count;++i) {
            spawn_points->xy[i*2]=generated->points[i].x;spawn_points->xy[i*2+1]=generated->points[i].y;
            spawn_points->profiles[i]=generated->points[i].profile;
        }
    } else spawn_points.reset();
    if(recipe.decoration_count) {
        begin_stage(recipe.decoration_count);
        auto field=mapgen::execute(recipe.decoration_nodes,recipe.decoration_count,seed,*scratch,progress);
        BN_ASSERT(field.status==mapgen::error::ok && field.type==mapgen::kind::decoration);
        finish_stage();
        if(!decoration)decoration.reset(new decoration_layout());
        const auto& config=recipe.decoration_nodes[recipe.decoration_count-1];
        int32_t parameters[6];
        for(int i=0;i<6;++i)parameters[i]=config.p[i];
        const int enabled=wasteland_art::banks[recipe.art_bank].decoration_enabled_mask;
        for(int i=0;i<4;++i)if(!(enabled&(1<<i)))parameters[i+1]=0;
        decoration->configure(seed,config.stream,field.data,parameters);
    } else decoration.reset();
    progress_done=progress_total;progress_stage=0;progress(100);
    loading_callback=nullptr; ++serial;
}
void release() { current.reset(); ground.reset(); roads.reset();spawn_points.reset();decoration.reset(); }
void populate_spawns(enemy_spawns& out,cave_layout::progress_fn callback) {
    if(!spawn_points){out.generate(*current,callback);return;}
    out.count=spawn_points->count;
    for(int i=0;i<out.count;++i){out.points[i]=enemy_spawns::point();out.points[i].x=spawn_points->xy[i*2];out.points[i].y=spawn_points->xy[i*2+1];out.points[i].profile=spawn_points->profiles[i];}
}
const spawn_profiles::profile* active_spawn_profiles() { return map_catalog::maps[selected_map].spawn_profiles; }
int active_spawn_profile_count() { return map_catalog::maps[selected_map].spawn_profile_count; }
bool has_decoration() { return bool(decoration); }
uint8_t decoration_patch(int x,int y) { return decoration?decoration->patch(x,y,*current,roads.get()):0; }
bool active() { return bool(current); }
const cave_layout& layout() { BN_ASSERT(current); return *current; }
int map_count() { return map_catalog::count; }
const char* map_name(int index) { BN_ASSERT(index>=0 && index<map_catalog::count);return map_catalog::maps[index].name; }
uint32_t fixed_seed() { return map_catalog::maps[selected_map].seed; }
int art_bank() { return map_catalog::maps[selected_map].art_bank; }
const road_network* road() { return roads.get(); }
int minimap_cell(int x,int y) {
    if(current->wall(x,y))return 2;
    return roads && (roads->links[y*64+x]&15)?3:1;
}
int generations() { return serial; }
int generation_updates() { return updates; }
int generation_scratch_bytes() { return sizeof(mapgen::workspace); }
int layout_bytes() { return sizeof(cave_layout)+(ground?sizeof(ground_grid):0)+(roads?sizeof(road_network):0)+(spawn_points?sizeof(spawn_locations):0)+(decoration?sizeof(decoration_layout):0); }
int material_at(int x,int y) {
    return wasteland_tiles::material(*current,ground?ground->ids:nullptr,x,y,roads.get());
}
int surface_at(int x,int y) {
    const auto& art=wasteland_art::banks[art_bank()];
    if(current->solid(x,y) || current->town_solid(x,y))return 3;
    return art.material_surfaces[material_at(x,y)];
}
uint16_t tile_at(int x,int y) {
    const auto& art=wasteland_art::banks[art_bank()];
    return art.refs[wasteland_tiles::reference(*current,ground?ground->ids:nullptr,x,y,roads.get(),art.material_textures)];
}
}
