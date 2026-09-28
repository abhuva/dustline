#include "cardinal_exit.h"
#include "generated/wasteland_recipe.h"
#include <cassert>
#include <cstdio>

int main() {
    for(int map_index=0;map_index<map_catalog::count;++map_index) {
        mapgen::workspace work;
        const auto& recipe=map_catalog::maps[map_index];
        auto result=mapgen::execute(recipe.nodes,recipe.count,recipe.seed,work);
        assert(result.status==mapgen::error::ok && result.type==mapgen::kind::world);
        assert(!recipe.exit_mask || work.roads.width);
        for(int side=0;side<4;++side)if(recipe.exit_mask&(1<<side)) {
            assert(cardinal_exit::apply(work.layout,work.roads,work.scratch,cardinal_exit::side(side)));
            auto arrival=cardinal_exit::arrival_for(cardinal_exit::side(side));
            assert(!work.layout.solid(arrival.x,arrival.y));
        }
        cardinal_exit::finalize(work.layout,recipe.exit_mask);
        const uint32_t signature=work.layout.signature();
        mapgen::workspace repeat;
        result=mapgen::execute(recipe.nodes,recipe.count,recipe.seed,repeat);
        for(int side=0;side<4;++side)if(recipe.exit_mask&(1<<side))
            assert(cardinal_exit::apply(repeat.layout,repeat.roads,repeat.scratch,cardinal_exit::side(side)));
        cardinal_exit::finalize(repeat.layout,recipe.exit_mask);
        assert(signature==repeat.layout.signature());
    }
    std::printf("Active cardinal exits passed for %d maps\n",map_catalog::count);
}
