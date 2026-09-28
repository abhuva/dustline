#pragma once

#include <cstdint>
#include "cave_layout.h"
#include "road_network.h"

namespace road_routes {

constexpr uint8_t invalid_direction=255;
constexpr uint8_t arrived=4;

// A direction field for each town keeps per-frame navigation constant-time.
struct town_routes {
    uint8_t toward[cave_layout::town_count][cave_layout::count];

    void clear() {
        for(int town=0;town<cave_layout::town_count;++town)
            for(int cell=0;cell<cave_layout::count;++cell)
                toward[town][cell]=invalid_direction;
    }

    void build(const cave_layout& layout,const road_network* roads,cave_scratch& scratch) {
        clear();
        if(!roads || !roads->width)return;
        for(int town=0;town<cave_layout::town_count;++town) {
            const auto destination=layout.town(town);
            const int root=(destination.y/cave_layout::cell_size)*cave_layout::columns+
                           destination.x/cave_layout::cell_size;
            int head=0,tail=1;
            scratch.queue[0]=uint16_t(root);toward[town][root]=arrived;
            while(head<tail) {
                const int at=scratch.queue[head++],x=at%cave_layout::columns,
                          y=at/cave_layout::columns;
                const unsigned links=roads->at(x,y);
                for(int direction=0;direction<4;++direction)if(links&(1<<direction)) {
                    const int nx=x+road_network::dx[direction],ny=y+road_network::dy[direction];
                    if(nx<0 || ny<0 || nx>=cave_layout::columns || ny>=cave_layout::columns)continue;
                    const int next=ny*cave_layout::columns+nx;
                    if(toward[town][next]!=invalid_direction)continue;
                    toward[town][next]=uint8_t((direction+2)%4);
                    scratch.queue[tail++]=uint16_t(next);
                }
            }
        }
    }

    uint8_t direction(int town,int cell) const {
        if(town<0 || town>=cave_layout::town_count || cell<0 || cell>=cave_layout::count)
            return invalid_direction;
        return toward[town][cell];
    }
};

}
