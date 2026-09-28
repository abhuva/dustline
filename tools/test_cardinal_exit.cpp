#include "cardinal_exit.h"
#include <cassert>
#include <cstdio>

int main() {
    for(uint32_t seed=1;seed<=64;++seed) {
        cave_scratch scratch;cave_layout layout;road_network roads;
        layout.generate(seed*2654435761u,scratch);roads.generate(layout,scratch,80,3);
        uint32_t signatures[4]={};
        for(int raw=0;raw<4;++raw) {
            auto side=cardinal_exit::side(raw);
            assert(cardinal_exit::apply(layout,roads,scratch,side));
            auto cell=cardinal_exit::boundary_cell(side);
            assert(!layout.wall(cell.x,cell.y));
            assert(roads.is_reserved(cell.x,cell.y));
            assert(roads.at(cell.x,cell.y)&(1<<raw));
            auto zone=cardinal_exit::trigger(side);
            auto arrival=cardinal_exit::arrival_for(side);
            assert(zone.x>=0 && zone.y>=0 && zone.x+zone.width<=cave_layout::extent);
            assert(zone.y+zone.height<=cave_layout::extent && !layout.solid(arrival.x,arrival.y));
            signatures[raw]=layout.signature();
            assert(raw==0 || signatures[raw]!=signatures[raw-1]);
        }
    }
    std::puts("Cardinal exit overlay passed 64 seeds on all four sides");
}
