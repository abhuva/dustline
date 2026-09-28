#include "map_recipe.h"
#include "wasteland_tiles.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>

namespace {
mapgen::workspace work;
uint8_t input[4096],output[4096],mask[4096];
}
int main() {
    using namespace mapgen;
    // Synchronous updates, independent birth/survival and cardinal/diagonal rules.
    input[32*64+32]=1;
    cellular(input,output,1<<1,0,0,8);
    assert(!output[32*64+32] && output[31*64+31] && output[32*64+31]);
    cellular(input,output,1<<1,0,0,4);
    assert(!output[31*64+31] && output[32*64+31]);
    cellular(input,output,511,511,0,8,mask);
    assert(std::memcmp(input,output,4096)==0);
    mask[31*64+32]=1;
    cellular(input,output,511,511,0,8,mask);
    assert(output[31*64+32] && !output[31*64+31] && output[32*64+32]);
    cellular(input,output,0,0,2,8,mask);
    assert(output[0] && output[65] && !output[130]);

    // Deterministic four-connected components, ties and all-wall inputs.
    std::memset(input,1,4096);
    input[130]=input[131]=input[260]=input[261]=0;
    largest(input,output,work.scratch);
    assert(!output[130] && !output[131] && output[260] && output[261]);
    input[262]=0; largest(input,output,work.scratch);
    assert(output[130] && !output[260] && !output[262]);
    std::memset(input,1,4096); largest(input,output,work.scratch);
    for(auto v:output) assert(v==1);

    node cases[]={
        {op::random,-1,-1,-1,0,{0,0}},
        {op::random,-1,-1,-1,17,{100,0}},
        {op::combine,0,1,-1,0,{0}}
    };
    auto result=execute(cases,3,123,work); assert(result.status==error::ok);
    for(int i=0;i<cells;++i) assert(result.data[i]==1);
    cases[2].p[0]=1; result=execute(cases,3,123,work);
    for(int i=0;i<cells;++i) assert(result.data[i]==0);
    cases[2].p[0]=2; result=execute(cases,3,123,work);
    for(int i=0;i<cells;++i) assert(result.data[i]==0);
    cases[2].p[0]=3; result=execute(cases,3,123,work);
    for(int i=0;i<cells;++i) assert(result.data[i]==1);
    cases[2]={op::world,1,-1,-1,0,{}};
    result=execute(cases,3,0,work);
    assert(result.status==error::ok && result.fallback && work.layout.floor_count()>256);
    assert(!work.layout.solid(work.layout.spawn().x,work.layout.spawn().y));

    // Reject malformed data before indexing a buffer or running unbounded loops.
    assert(execute(cases,0,1,work).status==error::count);
    cases[2].a=2; assert(validate(cases,3)==error::input);
    cases[2].a=-2; assert(validate(cases,3)==error::input);
    cases[2]={op::threshold,0,-1,-1,0,{128}}; assert(validate(cases,3)==error::type);
    cases[2]={op::cellular,0,-1,-1,0,{33,480,496,2,8}}; assert(validate(cases,3)==error::parameter);
    cases[2].p[0]=1; cases[2].p[4]=6; assert(validate(cases,3)==error::parameter);
    cases[2]={op(99),-1,-1,-1,0,{}}; assert(validate(cases,3)==error::opcode);

    // Long chains release buffers; independent branches have a hard budget.
    node chain[max_nodes];chain[0]=default_nodes[0];
    for(int i=1;i<max_nodes;++i) chain[i]={op::invert,i-1,-1,-1,0,{}};
    result=execute(chain,max_nodes,42,work);assert(result.status==error::ok && result.peak_buffers==2);
    node crowded[13];
    for(int i=0;i<7;++i) crowded[i]={op::random,-1,-1,-1,uint32_t(i),{47,2}};
    crowded[7]={op::combine,0,1,-1,0,{}};
    for(int i=8;i<13;++i) crowded[i]={op::combine,i-1,i-6,-1,0,{}};
    assert(execute(crowded,13,42,work).status==error::memory);

    cave_layout legacy;cave_scratch scratch;
    for(uint32_t seed:{0u,1u,0xC0FFEEu,0xffffffffu}) {
        legacy.generate(seed,scratch); result=execute(default_nodes,3,seed,work);
        assert(result.status==error::ok && std::memcmp(result.data,legacy.cells(),4096)==0);
        assert(work.layout.signature()==legacy.signature());
        for(int t=0;t<6;++t) assert(work.layout.town(t).x==legacy.town(t).x && work.layout.town(t).y==legacy.town(t).y);
    }
    // IDs are categorical bytes: painting preserves untouched IDs, including 255.
    node ground_nodes[]={
        {op::field_fill,-1,-1,-1,0,{255}},
        {op::random,-1,-1,-1,0,{0,2}},
        {op::field_paint,0,-1,1,0,{3}},
        {op::materials,2,-1,-1,0,{}}
    };
    result=execute(ground_nodes,4,42,work);assert(result.status==error::ok && result.type==kind::material);
    assert(result.data[0]==3 && result.data[32*64+32]==255);
    ground_nodes[0].p[0]=256;assert(validate(ground_nodes,4)==error::parameter);ground_nodes[0].p[0]=255;
    ground_nodes[2].mask=-1;assert(validate(ground_nodes,4)==error::input);
    node bands[]={ {op::radial,-1,-1,-1,0,{32,32,30,0}}, {op::field_lut,0,-1,-1,0,{}} };
    auto lut=reinterpret_cast<uint8_t*>(bands[1].p);
    for(int value=0;value<256;++value)lut[value]=uint8_t(value<64?0:value<128?3:value<192?127:255);
    result=execute(bands,2,42,work);assert(result.status==error::ok);
    assert(result.data[32*64+32]==0 && result.data[0]==255);
    // Refactored selection retains the original game's art reference at every tile.
    legacy.generate(0xC0FFEE,scratch);
    for(int y=0;y<1024;++y)for(int x=0;x<1024;++x){
        int px=x*8+4,py=y*8+4,town=legacy.town_graphic_at(px,py),expected;
        if(town>=0){auto t=legacy.town(town);expected=96+(town%2)*64+((py-(t.y-64))/8)*8+(px-(t.x-32))/8;}
        else {int material=legacy.solid(px,py)?(legacy.solid(px,py+24)?4:5):legacy.material(px,py);expected=material*16+(y%4)*4+x%4;}
        assert(wasteland_tiles::reference(legacy,nullptr,x,y)==expected);
    }
    auto spawn=legacy.spawn();
    for(int id=0;id<256;++id){
        std::memset(input,id,sizeof(input));
        assert(wasteland_tiles::material(legacy,input,spawn.x,spawn.y)==id);
        assert(wasteland_tiles::surface(legacy,input,spawn.x,spawn.y)==(id==2 || id==3?1:2));
        int ref=wasteland_tiles::reference(legacy,input,spawn.x/8,spawn.y/8);
        assert(ref/16==(id<4?id:0));
    }
    std::puts("PASS ground IDs: byte range, masked paint, bands, fallback art/grip; all legacy tile references unchanged");
    node road_nodes[]={default_nodes[0],default_nodes[1],default_nodes[2],{op::roads,2,-1,-1,0,{32,3}}};
    static int distance[4096],road_distance[4096],queue[4096];
    static uint8_t north_blocked[4096];
    for(int seed=0;seed<128;++seed) {
        road_nodes[0].p[0]=seed==0?100:seed==1?0:47;
        result=execute(road_nodes,4,uint32_t(seed)*2654435761u,work);
        assert(result.status==error::ok && result.type==kind::world);
        assert(std::memcmp(result.data,work.layout.cells(),4096)==0);
        std::memset(north_blocked,0,sizeof(north_blocked));
        for(int t=0;t<6;++t) { auto town=work.layout.town(t);north_blocked[town.y/128*64+town.x/128]=1; }
        auto town=work.layout.town(0);int root=town.y/128*64+town.x/128;
        // Independent distance BFS on valid floor edges, then on the resulting
        // road graph: each town must retain its shortest possible route.
        for(int mode=0;mode<2;++mode) {
            auto* distances=mode?road_distance:distance;
            for(int i=0;i<4096;++i)distances[i]=-1;
            int head=0,tail=1;queue[0]=root;distances[root]=0;
            while(head<tail) {
                int at=queue[head++];
                for(int d=0;d<4;++d) {
                    int x=at%64+road_network::dx[d],y=at/64+road_network::dy[d],next=y*64+x;
                    if(work.layout.wall(x,y))continue;
                    if(mode ? !(work.roads.links[at]&(1<<d)) :
                       ((d==0 && north_blocked[at]) || (d==2 && north_blocked[next])))continue;
                    if(distances[next]<0){distances[next]=distances[at]+1;queue[tail++]=next;}
                }
            }
        }
        for(int t=0;t<6;++t) {
            town=work.layout.town(t);int at=town.y/128*64+town.x/128;
            assert(distance[at]>=0 && road_distance[at]==distance[at]);
        }
        for(int at=0;at<4096;++at)if(work.roads.links[at]) {
            assert(!work.layout.cells()[at] && road_distance[at]>=0);
            for(int d=0;d<4;++d)if(work.roads.links[at]&(1<<d)) {
                int next=at+road_network::dx[d]+road_network::dy[d]*64;
                assert(next>=0 && next<4096 && (work.roads.links[next]&(1<<((d+2)%4))));
            }
            for(int width:{8,32,48}) {
                work.roads.width=width;
                for(int y=4;y<128;y+=8)for(int x=4;x<128;x+=8) {
                    int px=at%64*128+x,py=at/64*128+y;
                    if(!work.roads.contains(px,py))continue;
                    assert(!work.layout.solid(px,py) && !work.layout.town_solid(px,py));
                    assert(work.layout.town_graphic_at(px,py)<0);
                    assert(wasteland_tiles::surface(work.layout,nullptr,px,py,&work.roads)==1);
                    assert(wasteland_tiles::reference(work.layout,nullptr,px/8,py/8,&work.roads)/16==3);
                }
            }
        }
        uint8_t saved[4096];std::memcpy(saved,work.roads.links,4096);
        work.roads.generate(work.layout,work.scratch,32,255);
        assert(std::memcmp(saved,work.roads.links,4096)==0);
        assert(work.roads.material==255);
        if(seed==42) {
            int target=-1;
            for(int at=0;at<4096 && target<0;++at)if(!work.layout.cells()[at] && !work.roads.links[at]) {
                int x=at%64*128+64,y=at/64*128+64;
                if(!work.layout.town_solid(x,y))target=at;
            }
            assert(target>=0);
            int x=target%64*128+64,y=target/64*128+64;
            assert(work.roads.connect_portal(work.layout,work.scratch,x,y));
            assert(work.roads.links[target]);
            for(int at=0;at<4096;++at)for(int d=0;d<4;++d)if(work.roads.links[at]&(1<<d)) {
                int next=at+road_network::dx[d]+road_network::dy[d]*64;
                assert(next>=0 && next<4096 && !work.layout.cells()[next]);
                assert(work.roads.links[next]&(1<<((d+2)%4)));
            }
            assert(!work.roads.connect_portal(work.layout,work.scratch,4,4));
        }
    }
    road_nodes[3].p[0]=257;assert(validate(road_nodes,4)==error::parameter);
    road_nodes[3].p[0]=7;assert(validate(road_nodes,4)==error::parameter);
    road_nodes[3].p[0]=32;road_nodes[3].a=1;assert(validate(road_nodes,4)==error::type);
    road_nodes[3].a=2;road_nodes[3].p[2]=31;assert(validate(road_nodes,4)==error::parameter);
    road_nodes[3].p[0]=256;road_nodes[3].p[2]=0;
    result=execute(road_nodes,4,0xC0FFEE,work);assert(result.status==error::ok);
    // Wide capsules reach into neighbouring logical cells without losing edges
    // at cell seams. Constant width has no noise, even with a non-zero seed.
    road_network straight;straight.width=256;
    int at=32*64+32;straight.links[at]=2;straight.links[at+1]=8;
    for(int x=32*128+68;x<33*128+64;x+=8) {
        assert(straight.contains(x,32*128+104-124));
        assert(straight.contains(x,32*128+104+124));
        assert(!straight.contains(x,32*128+104-132));
        assert(!straight.contains(x,32*128+104+132));
    }
    work.roads.generate(work.layout,work.scratch,256,3);
    int clipped=0,painted=0;
    for(int y=4;y<8192;y+=8)for(int x=4;x<8192;x+=8)if(work.roads.contains(x,y)) {
        int ref=wasteland_tiles::reference(work.layout,nullptr,x/8,y/8,&work.roads);
        if(work.layout.town_graphic_at(x,y)>=0){assert(ref>=96);++clipped;}
        else if(work.layout.solid(x,y)){assert(ref/16==4 || ref/16==5);++clipped;}
        else {assert(ref/16==3);++painted;}
        if(work.layout.solid(x,y) || work.layout.town_solid(x,y))
            assert(wasteland_tiles::surface(work.layout,nullptr,x,y,&work.roads)==3);
    }
    assert(clipped>0 && painted>0);
    execute(default_nodes,3,42,work);assert(work.roads.width==0);
    std::puts("PASS wide roads: 256px cross-cell coverage, fixed width, unchanged BFS routes and obstacle clipping");
    std::puts("PASS roads: 128 seeds, shortest town routes, reciprocal connections, 8/32/48px widths clear of walls/buildings, art/grip and deterministic topology");
    std::printf("PASS map recipes: rules, masks, connectivity, ties, fallback, validation, buffer liveness, legacy parity; workspace %zu bytes\n",sizeof(work));
}
