#pragma once
#include "bn_array.h"
#include "bn_regular_bg_map_ptr.h"
#include "bn_regular_bg_ptr.h"
#include "bn_regular_bg_tiles_ptr.h"
#include "bn_tile.h"

// Full 128x128 region overview used by the Select menu. Each procedural
// 64x64 cell is rendered as a stable 2x2 block.
class world_overview {
public:
    static constexpr int center_x=40,center_y=0,pixel_size=128,world_scale=64;
    world_overview(int player_x,int player_y);
    void show_map(bool visible);
    void set_highlight(int world_x,int world_y);
    static int screen_x(int world_x);
    static int screen_y(int world_y);
private:
    bn::array<bn::tile,257> _pixels{}; // Tile zero is the blank menu background.
    bn::array<bn::tile,257> _base_pixels{};
    bn::regular_bg_tiles_ptr _tiles;
    bn::regular_bg_map_ptr _map;
    bn::regular_bg_ptr _bg;
    bool _map_visible=true;
    void plot(int x,int y,unsigned color);
    void upload_map();
};
