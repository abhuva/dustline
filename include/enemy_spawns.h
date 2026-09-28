#pragma once
#include "cave_layout.h"
#include "road_network.h"
#include "spawn_profiles.h"

// Persistent encounter records, not simulated vehicles. Shared with host tests.
// One anchor per 512px sector containing reachable floor, plus the starter pair.
struct enemy_spawns {
    static constexpr int capacity=258, spacing=512;
    struct point {
        uint16_t x=0,y=0;
        int ready_at=0;
        int8_t slot=-1;
        uint8_t hp=3;
        uint8_t profile=0;
        uint8_t reward_rolls=0;
    };
    point points[capacity];
    int count=0;

    void generate(const cave_layout& cave,cave_layout::progress_fn progress=nullptr) {
        count=0;
        auto start=cave.spawn();
        // Keep two immediately testable opponents in the carved starting clearing.
        add(cave,start.x+104,start.y);
        add(cave,start.x-144,start.y+48);
        for(int sy=0;sy<16;++sy) {
            for(int sx=0;sx<16;++sx) {
                uint32_t hash=cave.seed() ^ uint32_t(sx*374761393u+sy*668265263u);
                hash=(hash^(hash>>13))*1274126177u; hash^=hash>>16;
                for(int attempt=0;attempt<16;++attempt) {
                    int cell=(int(hash&15)+attempt*5)&15;
                    int x=sx*spacing+(cell%4)*128+64;
                    int y=sy*spacing+(cell/4)*128+64;
                    int dx=x-start.x,dy=y-start.y;
                    if(dx*dx+dy*dy<384*384) continue;
                    if(add(cave,x,y)) break;
                }
            }
            if(progress) progress(sy);
        }
    }
    // Select from the established, reachable sector anchors. Counts are targets;
    // zero mask weights and spacing can reduce the number actually available.
    void generate_recipe(const cave_layout& cave,const uint8_t* field,const uint8_t* types,
                         int target,int minimum_spacing,
                         bool starters,uint32_t stream,cave_layout::progress_fn progress=nullptr) {
        generate(cave,progress);
        uint16_t weights[capacity]{};bool selected[capacity]{};
        int total=0,placed=0;
        for(int i=0;i<count;++i) {
            weights[i]=uint16_t(field?field[(points[i].y/128)*64+points[i].x/128]:255);
            // Starter positions are explicit recipe options, subject to the mask.
            if(i<2 && !starters)weights[i]=0;
            total+=weights[i];
        }
        uint32_t rng=mapgen::stream_seed(cave.seed(),stream);
        while(placed<target && total) {
            int chosen=-1;
            if(starters)for(int i=0;i<2 && i<count;++i)if(weights[i]){chosen=i;break;}
            if(chosen<0) {
                int pick=int(mapgen::next(rng)%unsigned(total));
                for(int i=0;i<count;++i)if((pick-=weights[i])<0){chosen=i;break;}
            }
            selected[chosen]=true;++placed;
            for(int i=0;i<count;++i)if(weights[i]) {
                int dx=int(points[i].x)-points[chosen].x,dy=int(points[i].y)-points[chosen].y;
                if(i==chosen || dx*dx+dy*dy<minimum_spacing*minimum_spacing){total-=weights[i];weights[i]=0;}
            }
            if(progress && placed%8==0)progress(placed);
        }
        int out=0;
        for(int i=0;i<count;++i)if(selected[i]) {
            points[out]=points[i];
            int cell=(points[i].y/128)*64+points[i].x/128;
            points[out].profile=types?types[cell]:0;
            ++out;
        }
        count=out;
    }
    void exclude_reserved(const road_network* roads) {
        if(!roads)return;
        int out=0;
        for(int i=0;i<count;++i)if(!roads->is_reserved(points[i].x/128,points[i].y/128))
            points[out++]=points[i];
        count=out;
    }
private:
    bool add(const cave_layout& cave,int x,int y) {
        if(cave.wall(x/128,y/128) || cave.nearby_town(x,y)>=0) return false;
        for(int dy=-16;dy<=16;dy+=16) for(int dx=-16;dx<=16;dx+=16)
            if(cave.solid(x+dx,y+dy) || cave.town_solid(x+dx,y+dy)) return false;
        auto& p=points[count++]; p=point(); p.x=uint16_t(x); p.y=uint16_t(y);
        return true;
    }
};
