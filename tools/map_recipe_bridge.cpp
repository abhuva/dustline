#include "map_recipe.h"
#include "wasteland_tiles.h"
#include "enemy_spawns.h"
#include "decoration_layout.h"
#include "cardinal_exit.h"
#include <cstdio>
#include <cstdlib>

namespace {
mapgen::node nodes[mapgen::max_nodes];
#ifdef MAP_RECIPE_GBA_PROBE
mapgen::workspace work __attribute__((section(".ewram")));
#else
mapgen::workspace work;
#endif
mapgen::result result;
uint32_t metadata[20];
uint8_t ground[4096];
bool ground_active=false,world_ready=false;
enemy_spawns spawn_points;
decoration_layout decoration;
bool spawn_ready=false,decoration_active=false;
mapgen::node last_node;
uint32_t current_seed=0;
uint16_t point_coordinates[enemy_spawns::capacity*2];
uint8_t point_profiles[enemy_spawns::capacity];
uint8_t material_textures[256];
uint8_t material_surfaces[256];
bool material_lookups_ready=false;
void ensure_material_lookups() {
    if(material_lookups_ready)return;
    for(int i=0;i<256;++i){material_textures[i]=ground_materials::textures[i];material_surfaces[i]=ground_materials::surfaces[i];}
    material_lookups_ready=true;
}
#ifndef MAP_RECIPE_GBA_PROBE
uint8_t decoration_tiles[1024*1024];
#endif
#ifdef __EMSCRIPTEN__
uint8_t collision[1024*1024];
uint16_t render_tiles[1024*1024];
#endif
}
extern "C" {
mapgen::node* recipe_input() { return nodes; }
uint8_t* recipe_material_textures() { ensure_material_lookups();return material_textures; }
uint8_t* recipe_material_surfaces() { ensure_material_lookups();return material_surfaces; }
const uint8_t* recipe_cells() {
    return world_ready && result.type==mapgen::kind::world?work.layout.cells():result.data;
}
const uint32_t* recipe_meta() { return metadata; }
int recipe_run(int count,uint32_t seed) {
    result=mapgen::execute(nodes,count,seed,work);
    for(auto& v:metadata) v=0;
    metadata[0]=uint32_t(result.status); metadata[1]=uint32_t(result.type);
    metadata[2]=uint32_t(result.peak_buffers); metadata[3]=result.fallback;
    if(result.status!=mapgen::error::ok) return int(result.status);
    last_node=nodes[count-1];current_seed=seed;
    uint32_t signature=2166136261u;
    for(int i=0;i<mapgen::cells;++i) {
        signature=(signature^result.data[i])*16777619u;
        metadata[5]+=!result.data[i];
    }
    metadata[4]=signature;
    if(result.type==mapgen::kind::world) {
        world_ready=true;ground_active=false;
        spawn_ready=false;decoration_active=false;
        auto spawn=work.layout.spawn(); metadata[6]=uint32_t(spawn.x); metadata[7]=uint32_t(spawn.y);
        for(int t=0;t<6;++t) {
            auto town=work.layout.town(t); metadata[8+t*2]=uint32_t(town.x); metadata[9+t*2]=uint32_t(town.y);
        }
    }
    return 0;
}
int recipe_apply_spawns() {
    if(!world_ready || result.status!=mapgen::error::ok || result.type!=mapgen::kind::spawns)return 1;
    spawn_points.generate_recipe(work.layout,result.data,result.auxiliary,last_node.p[0],last_node.p[1],last_node.p[2],last_node.stream);
    spawn_points.exclude_reserved(&work.roads);
    spawn_ready=true;return 0;
}
int recipe_spawn_count() {
    if(!world_ready)return 0;
    if(!spawn_ready){spawn_points.generate(work.layout);spawn_ready=true;}
    return spawn_points.count;
}
const uint16_t* recipe_spawns() {
    for(int i=0;i<recipe_spawn_count();++i){point_coordinates[i*2]=spawn_points.points[i].x;point_coordinates[i*2+1]=spawn_points.points[i].y;}
    return point_coordinates;
}
const uint8_t* recipe_spawn_types() {
    for(int i=0;i<recipe_spawn_count();++i)point_profiles[i]=spawn_points.points[i].profile;
    return point_profiles;
}
int recipe_apply_decoration() {
    if(!world_ready || result.status!=mapgen::error::ok || result.type!=mapgen::kind::decoration)return 1;
    decoration.configure(current_seed,last_node.stream,result.data,last_node.p);decoration_active=true;return 0;
}
#ifndef MAP_RECIPE_GBA_PROBE
const uint8_t* recipe_decorations() {
    for(int cy=0;cy<256;++cy)for(int cx=0;cx<256;++cx) {
        uint8_t patch=decoration_active?decoration.patch(cx,cy,work.layout,&work.roads):0;
        for(int y=0;y<4;++y)for(int x=0;x<4;++x)decoration_tiles[(cy*4+y)*1024+cx*4+x]=decoration_layout::tile(patch,x,y);
    }
    return decoration_tiles;
}
#endif
int recipe_apply_materials() {
    if(!world_ready || result.status!=mapgen::error::ok || result.type!=mapgen::kind::material) return 1;
    for(int i=0;i<4096;++i) ground[i]=result.data[i];
    ground_active=true;return 0;
}
int recipe_connect_portal(int x,int y) {
    if(!world_ready || !work.roads.width)return 0;
    return work.roads.connect_portal(work.layout,work.scratch,x,y)?0:1;
}
int recipe_apply_exits(int mask) {
    if(!world_ready || (mask && !work.roads.width))return mask;
    int failed=0;
    for(int side=0;side<4;++side)if(mask&(1<<side))
        if(!cardinal_exit::apply(work.layout,work.roads,work.scratch,cardinal_exit::side(side)))failed|=1<<side;
    cardinal_exit::finalize(work.layout,uint8_t(mask));
    return failed;
}
const uint8_t* recipe_ground() {
    if(!world_ready)return nullptr;
    if(!ground_active)for(int y=0;y<64;++y)for(int x=0;x<64;++x)ground[y*64+x]=uint8_t(work.layout.material(x*128+64,y*128+64));
    return ground;
}
uint32_t recipe_render_signature() {
    uint32_t value=2166136261u;
    for(int y=0;y<1024;y+=7)for(int x=0;x<1024;x+=7) {
        ensure_material_lookups();
        value=(value^wasteland_tiles::reference(work.layout,ground_active?ground:nullptr,x,y,&work.roads,material_textures))*16777619u;
        value=(value^wasteland_tiles::surface(work.layout,ground_active?ground:nullptr,x*8+4,y*8+4,&work.roads,material_surfaces))*16777619u;
    }
    return value;
}
#ifdef __EMSCRIPTEN__
const uint8_t* recipe_roads() {
    if(!world_ready)return nullptr;
    auto* road_pixels=reinterpret_cast<uint8_t*>(render_tiles);
    for(int y=0;y<1024;++y)for(int x=0;x<1024;++x)
        road_pixels[y*1024+x]=uint8_t(work.roads.contains(x*8+4,y*8+4));
    return road_pixels;
}
const uint8_t* recipe_reserved() {
    if(!world_ready)return nullptr;
    auto* pixels=reinterpret_cast<uint8_t*>(render_tiles);
    for(int y=0;y<1024;++y)for(int x=0;x<1024;++x)
        pixels[y*1024+x]=uint8_t(work.roads.is_reserved(x/16,y/16));
    return pixels;
}
const uint16_t* recipe_tiles() {
    if(!world_ready)return nullptr;
    ensure_material_lookups();
    for(int y=0;y<1024;++y)for(int x=0;x<1024;++x)
        render_tiles[y*1024+x]=wasteland_tiles::reference(work.layout,ground_active?ground:nullptr,x,y,&work.roads,material_textures);
    return render_tiles;
}
const uint8_t* recipe_collision() {
    if(!world_ready) return nullptr;
    for(int y=0;y<1024;++y) for(int x=0;x<1024;++x)
        collision[y*1024+x]=uint8_t(work.layout.solid(x*8+4,y*8+4) || work.layout.town_solid(x*8+4,y*8+4));
    return collision;
}
#elif defined(MAP_RECIPE_GBA_PROBE)
#include "fixtures.h"
volatile uint32_t recipe_probe[4]={};
int main() {
    recipe_probe[0]=0x52435031;
    for(int f=0;f<fixture_count;++f) {
        const auto& fixture=fixtures[f];
        for(int n=0;n<fixture.count;++n) nodes[n]=fixture.program[n];
        recipe_run(fixture.count,fixture.seed);
        const auto* bytes=reinterpret_cast<const uint8_t*>(metadata);
        for(int i=0;i<80+4096;++i) {
            uint8_t actual=i<80?bytes[i]:result.data?result.data[i-80]:0;
            if(actual!=fixture.expected[i]) {
                recipe_probe[2]=uint32_t(f+1);recipe_probe[3]=uint32_t(i+1);
                while(true) asm volatile("nop");
            }
        }
        recipe_probe[1]=uint32_t(f+1);
    }
    for(int f=0;f<render_fixture_count;++f) {
        const auto& fixture=render_fixtures[f];
        for(int n=0;n<fixture.world_count;++n)nodes[n]=fixture.world[n];
        recipe_run(fixture.world_count,fixture.seed);
        if(fixture.material_count){
            for(int n=0;n<fixture.material_count;++n)nodes[n]=fixture.materials[n];
            recipe_run(fixture.material_count,fixture.seed);recipe_apply_materials();
        }
        if(recipe_render_signature()!=fixture.expected){recipe_probe[2]=uint32_t(fixture_count+f+1);while(true)asm volatile("nop");}
        recipe_probe[1]=uint32_t(fixture_count+f+1);
    }
    while(true) asm volatile("nop");
}
#else
int main(int argc,char** argv) {
    if(argc!=4 && argc!=5 && argc!=7 && argc!=8) return 2;
    auto* input=std::fopen(argv[1],"rb"); if(!input) return 3;
    int count=int(std::fread(nodes,sizeof(mapgen::node),mapgen::max_nodes,input)); std::fclose(input);
    int status=recipe_run(count,uint32_t(std::strtoul(argv[2],nullptr,0)));
    if(!status && argc==8 && argv[7][0]!='-') {
        input=std::fopen(argv[7],"rb");if(!input)return 3;
        std::fseek(input,0,SEEK_END);long placement_bytes=std::ftell(input);std::rewind(input);
        if(placement_bytes==1) {
            uint8_t mask=0;std::fread(&mask,1,1,input);
            if(recipe_apply_exits(mask)){std::fclose(input);return 6;}
        } else {
            uint16_t portal[2];
            while(std::fread(portal,sizeof(portal),1,input)==1)
                if(recipe_connect_portal(portal[0],portal[1])){std::fclose(input);return 6;}
        }
        std::fclose(input);metadata[4]=work.layout.signature();metadata[5]=uint32_t(work.layout.floor_count());
    }
    auto* output=std::fopen(argv[3],"wb"); if(!output) return 4;
    std::fwrite(metadata,sizeof(metadata),1,output);
    if(!status) std::fwrite(recipe_cells(),1,mapgen::cells,output);
    if(!status && argc>=5) {
        if(argv[4][0]!='-'){
            input=std::fopen(argv[4],"rb");if(!input){std::fclose(output);return 3;}
            count=int(std::fread(nodes,sizeof(mapgen::node),mapgen::max_nodes,input));std::fclose(input);
            status=recipe_run(count,uint32_t(std::strtoul(argv[2],nullptr,0)));
            if(status || recipe_apply_materials()){std::fprintf(stderr,"material branch failed: %d\\n",status);std::fclose(output);return 5;}
        }
        std::fwrite(recipe_ground(),1,4096,output);
        for(int y=0;y<1024;++y)for(int x=0;x<1024;++x){
            uint16_t tile=wasteland_tiles::reference(work.layout,ground_active?ground:nullptr,x,y,&work.roads);
            std::fwrite(&tile,2,1,output);
        }
        uint32_t signature=recipe_render_signature();std::fwrite(&signature,4,1,output);
        if(argc>=7) {
            for(int arg=5;arg<=6;++arg)if(argv[arg][0]!='-') {
                input=std::fopen(argv[arg],"rb");if(!input){std::fclose(output);return 3;}
                count=int(std::fread(nodes,sizeof(mapgen::node),mapgen::max_nodes,input));std::fclose(input);
                status=recipe_run(count,uint32_t(std::strtoul(argv[2],nullptr,0)));
                int applied=arg==5?recipe_apply_spawns():recipe_apply_decoration();
                if(status || applied){std::fprintf(stderr,"placement branch %d failed: status=%d apply=%d type=%d\\n",arg,status,applied,int(result.type));std::fclose(output);return 5;}
            }
            std::fwrite(recipe_decorations(),1,1024*1024,output);
            uint32_t points=recipe_spawn_count();std::fwrite(&points,4,1,output);
            std::fwrite(recipe_spawns(),4,points,output);
            std::fwrite(recipe_spawn_types(),1,points,output);
        }
    }
    std::fclose(output); return status;
}
#endif
}
