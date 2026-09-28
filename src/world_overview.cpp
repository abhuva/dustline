#include "world_overview.h"
#include "bn_algorithm.h"
#include "bn_bg_palette_item.h"
#include "bn_bg_palette_ptr.h"
#include "bn_bpp_mode.h"
#include "bn_color.h"
#include "bn_memory.h"
#include "bn_size.h"
#include "bn_span.h"
#include "wasteland.h"
#include "world_map.h"

namespace {
constexpr bn::array<bn::color,16> overview_palette={
    bn::color(2,4,5),bn::color(5,10,8),bn::color(25,22,14),bn::color(18,13,7),
    bn::color(30,25,10),bn::color(31,19,7),bn::color(5,29,8),bn::color(31,5,4),
    bn::color(31,31,26),bn::color(0,0,0),bn::color(0,0,0),bn::color(0,0,0),
    bn::color(0,0,0),bn::color(0,0,0),bn::color(0,0,0),bn::color(0,0,0)
};
}

world_overview::world_overview(int player_x,int player_y) :
    _tiles(bn::regular_bg_tiles_ptr::allocate(_pixels.size(),bn::bpp_mode::BPP_4,false)),
    _map(bn::regular_bg_map_ptr::allocate(bn::size(32,32),_tiles,
        bn::bg_palette_ptr::create(bn::bg_palette_item(bn::span<const bn::color>(overview_palette),bn::bpp_mode::BPP_4)))),
    _bg(bn::regular_bg_ptr::create(_map)) {
    for(int y=0;y<64;++y)for(int x=0;x<64;++x) {
        const int cell=wasteland::minimap_cell(x,y);
        const unsigned color=cell==2?1:cell==3?3:2;
        const int px=x*2,py=y*2;
        plot(px,py,color);plot(px+1,py,color);plot(px,py+1,color);plot(px+1,py+1,color);
    }
    for(int index=0;index<cave_layout::town_count;++index) {
        const auto town=wasteland::layout().town(index);
        const int x=bn::max(1,bn::min(pixel_size-2,town.x/world_scale));
        const int y=bn::max(1,bn::min(pixel_size-2,town.y/world_scale));
        for(int yy=-2;yy<=2;++yy)for(int xx=-2;xx<=2;++xx)
            if(xx==0 || yy==0)plot(x+xx,y+yy,4);
    }
    // Derived cardinal exits are shown as compact green squares.
    for(int side=0;side<4;++side)if(world_map::exit_mask()&(1<<side)) {
        const auto portal=world_map::exit(side);
        const int x=bn::max(1,bn::min(pixel_size-2,portal.x/world_scale));
        const int y=bn::max(1,bn::min(pixel_size-2,portal.y/world_scale));
        for(int yy=-1;yy<=1;++yy)for(int xx=-1;xx<=1;++xx)plot(x+xx,y+yy,6);
    }
    // The paused menu snapshot does not move, so baking the player last gives
    // it an unmistakable red dot without spending another OAM entry.
    const int player_pixel_x=bn::max(1,bn::min(pixel_size-2,player_x/world_scale));
    const int player_pixel_y=bn::max(1,bn::min(pixel_size-2,player_y/world_scale));
    for(int yy=-1;yy<=1;++yy)for(int xx=-1;xx<=1;++xx)
        if(xx==0 || yy==0)plot(player_pixel_x+xx,player_pixel_y+yy,7);
    bn::memory::copy(_pixels[0],_pixels.size(),_base_pixels[0]);
    bn::memory::copy(_pixels[0],_pixels.size(),_tiles.vram()->data()[0]);
    upload_map();
    _bg.set_position(center_x,center_y);_bg.set_priority(0);
}

void world_overview::plot(int x,int y,unsigned color) {
    if(x<0 || y<0 || x>=pixel_size || y>=pixel_size)return;
    auto* words=reinterpret_cast<unsigned*>(_pixels.data());
    const int tile=1+(y>>3)*16+(x>>3),word=tile*8+(y&7),shift=(x&7)*4;
    words[word]=(words[word]&~(15u<<shift))|(color<<shift);
}

void world_overview::upload_map() {
    auto* cells=_map.vram()->data();
    const unsigned base=_map.tiles_offset()+(_map.palette().id()<<12);
    for(int index=0;index<1024;++index)cells[index]=base;
    if(_map_visible)for(int y=0;y<16;++y)for(int x=0;x<16;++x)
        cells[(y+8)*32+x+8]=base+1+y*16+x;
}

void world_overview::show_map(bool visible) {
    if(_map_visible==visible)return;
    _map_visible=visible;upload_map();
}

void world_overview::set_highlight(int world_x,int world_y) {
    bn::memory::copy(_base_pixels[0],_base_pixels.size(),_pixels[0]);
    const int x=bn::max(4,bn::min(pixel_size-5,world_x/world_scale));
    const int y=bn::max(4,bn::min(pixel_size-5,world_y/world_scale));
    for(int offset=-4;offset<=4;++offset) {
        plot(x+offset,y-4,6);plot(x+offset,y+4,6);
        plot(x-4,y+offset,6);plot(x+4,y+offset,6);
    }
    plot(x,y-5,8);plot(x,y+5,8);plot(x-5,y,8);plot(x+5,y,8);
    bn::memory::copy(_pixels[0],_pixels.size(),_tiles.vram()->data()[0]);
}

int world_overview::screen_x(int world_x) {
    return center_x-pixel_size/2+bn::max(0,bn::min(pixel_size-1,world_x/world_scale));
}

int world_overview::screen_y(int world_y) {
    return center_y-pixel_size/2+bn::max(0,bn::min(pixel_size-1,world_y/world_scale));
}
