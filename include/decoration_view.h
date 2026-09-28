#pragma once
#include "bn_regular_bg_ptr.h"
#include "bn_regular_bg_map_ptr.h"
#include "bn_regular_bg_map_cell.h"

class decoration_view {
public:
    decoration_view();
    ~decoration_view();
    BN_CODE_IWRAM void update(int x,int y,bool visible,bool rebuild=true);
    static void commit();
private:
    alignas(4) bn::regular_bg_map_cell _cells[2048]{};
    uint32_t _positions[128];
    bn::regular_bg_map_ptr _map;
    bn::regular_bg_ptr _bg;
    int _left=-1000,_top=-1000;
    bn::regular_bg_map_cell* _vram=nullptr;
    int _tile_offset=0;
    bool _pending=true;
    bool _full=true;
    uint16_t _dirty[8]{};
    static decoration_view* _active;
};
