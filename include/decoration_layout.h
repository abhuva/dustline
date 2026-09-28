#pragma once
#include "cave_layout.h"
#include "road_network.h"

// One small, stateless patch per 32px candidate cell. Positions stay inside the
// cell and on an 8px boundary so the GBA can stream a transparent tile layer.
struct decoration_layout {
    uint8_t density[4096]{};
    int weights[4]{};
    uint32_t seed=0;
    int exclude_roads=1;
    void configure(uint32_t world_seed,uint32_t stream,const uint8_t* field,const int32_t* p) {
        seed=mapgen::stream_seed(world_seed,stream);
        for(int i=0;i<4096;++i)density[i]=uint8_t((field?field[i]:255)*p[0]/100);
        for(int i=0;i<4;++i)weights[i]=p[i+1];
        exclude_roads=p[5];
    }
    uint8_t patch(int cx,int cy,const cave_layout& cave,const road_network* roads) const {
        if(cx<0 || cy<0 || cx>=256 || cy>=256)return 0;
        unsigned chance=density[(cy/4)*64+cx/4];
        if(!chance)return 0;
        uint32_t key=seed^uint32_t(cx)*374761393u^uint32_t(cy)*668265263u;
        if(mapgen::hash(key)%255>=chance)return 0;
        uint32_t offset=mapgen::hash(key^0x97a54bc1u);
        int ox=int(offset%3)*8,oy=int((offset>>8)%3)*8;
        int x=cx*32+ox,y=cy*32+oy;
        // Settlement bounds and sample coordinates are all aligned to 8px:
        // one rectangle test is equivalent to testing all nine sample points.
        for(int i=0;i<cave_layout::town_count;++i) {
            auto t=cave.town(i);
            if(x+16>=t.x-32 && x<t.x+32 && y+16>=t.y-64 && y<t.y)return 0;
        }
        bool floor=!cave.wall(x/128,y/128) && !cave.wall((x+16)/128,y/128) &&
            !cave.wall(x/128,(y+16)/128) && !cave.wall((x+16)/128,(y+16)/128);
        for(int yy=0;yy<=16;yy+=8)for(int xx=0;xx<=16;xx+=8) {
            if((roads && roads->is_reserved((x+xx)/128,(y+yy)/128)) ||
               (!floor && cave.solid(x+xx,y+yy)) ||
               (exclude_roads && roads && roads->width && roads->contains(x+xx,y+yy)))return 0;
        }
        int total=weights[0]+weights[1]+weights[2]+weights[3];
        if(!total)return 0;
        int pick=int(mapgen::hash(key^0xb1273a55u)%unsigned(total)),type=0;
        while(type<3 && pick>=weights[type])pick-=weights[type++];
        return uint8_t(type+1+(ox/8)*8+(oy/8)*32);
    }
    static uint8_t tile(uint8_t patch,int tx,int ty) {
        if(!patch)return 0;
        int x=tx-((patch>>3)&3),y=ty-((patch>>5)&3);
        return x>=0 && x<2 && y>=0 && y<2?uint8_t(1+((patch&7)-1)*4+y*2+x):0;
    }
};
