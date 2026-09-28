#pragma once
#include <cstdint>
#include "cave_layout.h"
#include "road_network.h"
#include "generated/cardinal_exit_profile.h"

namespace cardinal_exit {

enum class side : uint8_t { north=0,east=1,south=2,west=3 };
struct rectangle { int x,y,width,height; };
struct arrival { int x,y,heading; };
inline constexpr int dx[4]={0,1,0,-1},dy[4]={-1,0,1,0};

inline side opposite(side value) { return side((int(value)+2)&3); }

inline int centre_cell() { return cave_layout::columns/2-1; }
inline cave_layout::point boundary_cell(side value) {
    switch(value) {
    case side::north:return {centre_cell(),0};
    case side::east:return {cave_layout::columns-1,centre_cell()};
    case side::south:return {centre_cell(),cave_layout::columns-1};
    default:return {0,centre_cell()};
    }
}
inline bool near_town(const cave_layout& layout,int x,int y) {
    for(int t=0;t<cave_layout::town_count;++t) {
        auto town=layout.town(t);
        int tx=town.x/cave_layout::cell_size,ty=town.y/cave_layout::cell_size;
        int ax=x-tx,ay=y-ty;
        if(ax<0)ax=-ax;
        if(ay<0)ay=-ay;
        if(ax<=1 && ay<=1)return true;
    }
    return false;
}

// Carve the shortest deterministic, town-safe path from a fixed border port
// to existing playable floor, then extend the generated road to the border.
inline bool apply(cave_layout& layout,road_network& roads,cave_scratch& scratch,side value) {
    auto start=boundary_cell(value);const int origin=start.y*64+start.x;
    for(int i=0;i<cave_layout::count;++i)scratch.work[i]=255;
    int head=0,tail=1,target=-1;scratch.queue[0]=uint16_t(origin);scratch.work[origin]=4;
    while(head<tail && target<0) {
        int at=scratch.queue[head++],x=at%64,y=at/64;
        int inward=(int(value)+2)&3;
        int depth=(x-start.x)*dx[inward]+(y-start.y)*dy[inward];
        if(depth>=cardinal_exit_profile::approachDepthCells && !layout.wall(x,y) &&
           !roads.is_reserved(x,y) && !near_town(layout,x,y)) { target=at;break; }
        // Prefer inward, then clockwise/counter-clockwise, and only finally
        // back toward the edge. This makes ties stable on every platform.
        const int order[4]={inward,(inward+1)&3,(inward+3)&3,int(value)};
        for(int index=0;index<4;++index) {
            int direction=order[index],nx=x+dx[direction],ny=y+dy[direction];
            if(nx<0 || ny<0 || nx>=64 || ny>=64 || near_town(layout,nx,ny))continue;
            int next=ny*64+nx;if(scratch.work[next]!=255)continue;
            scratch.work[next]=uint8_t(((direction+2)&3)+1);
            scratch.queue[tail++]=uint16_t(next);
        }
    }
    if(target<0)return false;
    int at=target;
    while(true) {
        int x=at%64,y=at/64;
        layout.set_floor_cell(x,y);roads.links[at]|=road_network::reserved;
        // Three logical cells give the car a readable, forgiving approach.
        int cross=(int(value)+1)&3;
        for(int offset=-cardinal_exit_profile::openingCells/2;offset<=cardinal_exit_profile::openingCells/2;++offset) {
            int xx=x+dx[cross]*offset,yy=y+dy[cross]*offset;
            if(xx>=0 && yy>=0 && xx<64 && yy<64 && !near_town(layout,xx,yy)) {
                layout.set_floor_cell(xx,yy);roads.links[yy*64+xx]|=road_network::reserved;
            }
        }
        if(at==origin)break;
        int direction=(scratch.work[at]&15)-1;
        at+=dx[direction]+dy[direction]*64;
    }
    int world_x=start.x*128+64,world_y=start.y*128+64;
    if(!roads.connect_portal(layout,scratch,world_x,world_y))return false;
    roads.links[origin]|=uint8_t(1<<int(value));
    return true;
}

inline void finalize(cave_layout& layout,uint8_t exit_mask) {
    layout.refresh_signature(cardinal_exit_profile::signature^exit_mask);
}

inline rectangle trigger(side value) {
    constexpr int extent=cave_layout::extent;
    const int width=cardinal_exit_profile::triggerWidth,depth=cardinal_exit_profile::triggerDepth;
    if(value==side::north)return {(extent-width)/2,0,width,depth};
    if(value==side::south)return {(extent-width)/2,extent-depth,width,depth};
    if(value==side::east)return {extent-depth,(extent-width)/2,depth,width};
    return {0,(extent-width)/2,depth,width};
}
inline arrival arrival_for(side value) {
    constexpr int middle=cave_layout::extent/2;
    const int inset=cardinal_exit_profile::arrivalInset;
    if(value==side::north)return {middle,inset,90};
    if(value==side::east)return {cave_layout::extent-inset,middle,180};
    if(value==side::south)return {middle,cave_layout::extent-inset,270};
    return {inset,middle,0};
}
}
