#include "decoration_view.h"
#include "decoration_layout.h"
#include "wasteland.h"
#include "world_map.h"
#include "generated/wasteland_art.h"
#include "bn_regular_bg_tiles_item.h"
#include "bn_regular_bg_tiles_ptr.h"
#include "bn_regular_bg_map_item.h"
#include "bn_bg_palette_ptr.h"
#include "bn_bg_palette_item.h"
#include "bn_size.h"
#include "bn_memory.h"

decoration_view* decoration_view::_active=nullptr;

decoration_view::decoration_view() :
    _map(bn::regular_bg_map_ptr::allocate(bn::size(64,32),
        bn::regular_bg_tiles_ptr::create(bn::regular_bg_tiles_item(
            bn::span<const bn::tile>(wasteland_art::banks[wasteland::art_bank()].decoration_tiles,17),
            bn::bpp_mode::BPP_4,bn::compression_type::NONE)),
        bn::bg_palette_ptr::create(bn::bg_palette_item(bn::span<const bn::color>(world_map::palette()+224,16),bn::bpp_mode::BPP_4)))),
    _bg(bn::regular_bg_ptr::create(_map)) {
    for(auto& p:_positions)p=0xffffffff;
    _tile_offset=_map.tiles_offset()+(_map.palette().id()<<12);
    for(auto& cell:_cells)cell=uint16_t(_tile_offset);
    _vram=_map.vram()->data();_active=this;
    _bg.set_priority(2);_bg.set_visible(false);
}
decoration_view::~decoration_view() { _active=nullptr; }
void decoration_view::commit() {
    if(_active && _active->_pending) {
        auto& view=*_active;
        const auto* source=reinterpret_cast<const unsigned*>(view._cells);
        auto* destination=reinterpret_cast<volatile unsigned*>(view._vram);
        if(view._full) {
            bn::memory::copy(source[0],1024,*const_cast<unsigned*>(destination));
            view._full=false;
        } else for(int row=0;row<8;++row)if(view._dirty[row]) {
            for(int col=0;col<16;++col)if(view._dirty[row]&(1<<col)) {
                // A changed 32px candidate owns four rows of four map cells.
                // Upload only its 32 bytes, instead of the complete 4 KiB map.
                for(int y=0;y<4;++y) {
                    int at=(col/8)*512+(row*4+y)*16+(col&7)*2;
                    destination[at]=source[at];destination[at+1]=source[at+1];
                }
            }
        }
        for(auto& dirty:view._dirty)dirty=0;
        view._pending=false;
    }
}
void decoration_view::update(int x,int y,bool visible,bool rebuild) {
    if(visible) {
        int offset=_map.tiles_offset()+(_map.palette().id()<<12);
        if(offset!=_tile_offset) {
            for(auto& cell:_cells)cell=uint16_t(int(cell)+offset-_tile_offset);
            _tile_offset=offset;_pending=true;_full=true;
        }
        int left=(x-120)/32,top=(y-80)/32;
        if(rebuild && (left!=_left || top!=_top)) {
            for(int cy=top;cy<=top+5;++cy)for(int cx=left;cx<=left+8;++cx) {
                int slot=(cy&7)*16+(cx&15);uint32_t key=(uint32_t(cy)<<16)|uint32_t(cx);
                if(_positions[slot]==key)continue;
                _positions[slot]=key;
                _dirty[cy&7]|=uint16_t(1<<(cx&15));
                uint8_t patch=wasteland::decoration_patch(cx,cy);
                for(int yy=0;yy<4;++yy)for(int xx=0;xx<4;++xx) {
                    int tx=(cx*4+xx)&63,ty=(cy*4+yy)&31;
                    // GBA maps wider than 32 tiles store two 32x32 screenblocks.
                    int at=(tx/32)*1024+ty*32+(tx&31);
                    _cells[at]=uint16_t(_tile_offset+decoration_layout::tile(patch,xx,yy));
                }
            }
            // Cells already contain the tile offset. Copy directly during
            // VBlank instead of rebuilding all 2048 cells through Butano.
            _pending=true;_left=left;_top=top;
        }
        _bg.set_position(256-(x%512),128-(y%256));
    }
    _bg.set_visible(visible);
}
