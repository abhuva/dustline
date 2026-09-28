#include <cassert>
#include <iostream>
#include "road_routes.h"

int main() {
    cave_layout layout;cave_scratch scratch;
    layout.generate(0x00c0ffeeu,scratch);
    road_network roads;roads.generate(layout,scratch,96,3);
    road_routes::town_routes routes;routes.build(layout,&roads,scratch);

    for(int target=0;target<cave_layout::town_count;++target) {
        const auto destination=layout.town(target);
        const int destination_cell=(destination.y/128)*64+destination.x/128;
        assert(routes.direction(target,destination_cell)==road_routes::arrived);
        for(int origin=0;origin<cave_layout::town_count;++origin) {
            const auto start=layout.town(origin);
            int cell=(start.y/128)*64+start.x/128;
            bool visited[cave_layout::count]={};
            for(int step=0;step<cave_layout::count && cell!=destination_cell;++step) {
                assert(!visited[cell]);visited[cell]=true;
                const int direction=routes.direction(target,cell);
                assert(direction>=0 && direction<4);
                assert(roads.at(cell%64,cell/64)&(1<<direction));
                cell+=road_network::dx[direction]+road_network::dy[direction]*64;
            }
            assert(cell==destination_cell);
        }
    }

    routes.build(layout,nullptr,scratch);
    assert(routes.direction(0,0)==road_routes::invalid_direction);
    assert(sizeof(routes)==cave_layout::town_count*cave_layout::count);
    std::cout << "road route fields: ok\n";
}
