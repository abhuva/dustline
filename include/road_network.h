#pragma once
#include "cave_layout.h"

// One byte per logical cell: N/E/S/W connections. Generation reuses the cave
// flood queue and one-byte parent directions; no distances or heap are needed.
struct road_network {
    uint8_t links[4096]={};
    int width=0,material=3;
    static constexpr uint8_t reserved=16;
    static constexpr int dx[4]={0,1,0,-1},dy[4]={-1,0,1,0};

    void generate(const cave_layout& layout,cave_scratch& scratch,int pixels,int id,
                  cave_layout::progress_fn progress=nullptr) {
        width=pixels;material=id;
        for(int i=0;i<4096;++i) { links[i]=0;scratch.work[i]=0; }
        // Road anchors sit south of the buildings (64,104 within a cell).
        // Ban the north approach at town cells: it would cross the building.
        // Their carved 3x3 plazas provide east/west/south approaches instead.
        for(int t=0;t<cave_layout::town_count;++t) {
            auto town=layout.town(t);scratch.work[(town.y/128)*64+town.x/128]=16;
        }
        auto town=layout.town(0);
        int root=(town.y/128)*64+town.x/128,head=0,tail=1;
        scratch.queue[0]=uint16_t(root);scratch.work[root]|=5;
        while(head<tail) {
            int at=scratch.queue[head++],x=at%64,y=at/64;
            for(int d=0;d<4;++d) {
                int nx=x+dx[d],ny=y+dy[d];
                if(layout.wall(nx,ny)) continue;
                int next=ny*64+nx;
                if((scratch.work[next]&15) || (d==0 && (scratch.work[at]&16)) ||
                   (d==2 && (scratch.work[next]&16))) continue;
                scratch.work[next]|=uint8_t(((d+2)%4)+1);
                scratch.queue[tail++]=uint16_t(next);
            }
            if(progress && head%256==0) progress(90);
        }
        // Every town follows the same shortest-path tree. Stop at existing
        // roads because their entire route to the root is already marked.
        for(int t=1;t<cave_layout::town_count;++t) {
            town=layout.town(t);int at=(town.y/128)*64+town.x/128;
            while(at!=root && (scratch.work[at]&15)) {
                int d=(scratch.work[at]&15)-1,next=at+dx[d]+dy[d]*64;
                bool joined=(links[next]&15)!=0;
                links[at]|=uint8_t(1<<d);links[next]|=uint8_t(1<<((d+2)%4));at=next;
                if(joined) break;
            }
        }
    }

    // Extend the generated network to an exit anchor. The search starts at
    // the anchor's logical cell and stops at the first existing road, so it
    // finds the shortest traversable connection without crossing canyon walls.
    // Existing access branches are valid destinations for later exits.
    bool connect_portal(const cave_layout& layout,cave_scratch& scratch,int world_x,int world_y) {
        if(!width || layout.solid(world_x,world_y) || layout.town_solid(world_x,world_y))return false;
        const int target_x=world_x/cave_layout::cell_size,target_y=world_y/cave_layout::cell_size;
        if(layout.wall(target_x,target_y))return false;
        const int target=target_y*64+target_x;
        if(links[target]&15)return true;

        for(int i=0;i<4096;++i)scratch.work[i]=0;
        for(int t=0;t<cave_layout::town_count;++t) {
            auto town=layout.town(t);scratch.work[(town.y/128)*64+town.x/128]=16;
        }
        int head=0,tail=1,joined=-1;
        scratch.queue[0]=uint16_t(target);scratch.work[target]|=5;
        while(head<tail && joined<0) {
            int at=scratch.queue[head++],x=at%64,y=at/64;
            for(int d=0;d<4;++d) {
                int nx=x+dx[d],ny=y+dy[d];
                if(layout.wall(nx,ny))continue;
                int next=ny*64+nx;
                if((scratch.work[next]&15) || (d==0 && (scratch.work[at]&16)) ||
                   (d==2 && (scratch.work[next]&16)))continue;
                scratch.work[next]|=uint8_t(((d+2)%4)+1);
                scratch.queue[tail++]=uint16_t(next);
                if(links[next]&15) { joined=next;break; }
            }
        }
        if(joined<0)return false;
        int at=joined;
        while(at!=target) {
            int d=(scratch.work[at]&15)-1,next=at+dx[d]+dy[d]*64;
            links[at]|=uint8_t(1<<d);links[next]|=uint8_t(1<<((d+2)%4));at=next;
        }
        return true;
    }

    unsigned at(int x,int y) const {
        return x<0 || y<0 || x>=64 || y>=64 ? 0 : links[y*64+x]&15;
    }
    bool is_reserved(int x,int y) const {
        return x>=0 && y>=0 && x<64 && y<64 && (links[y*64+x]&reserved);
    }
    bool contains(int x,int y) const {
        if(!width || x<0 || y<0 || x>=8192 || y>=8192) return false;
        x=(x/8)*8+4;y=(y/8)*8+4;
        // Four surrounding anchors suffice for a radius <=128. Test the four
        // connecting strips and rounded ends directly: no neighbour loops,
        // noise, interpolation, or width samples during rendering/driving.
        int gx=(x+64)/128-1,gy=(y+24)/128-1;
        int lx=x-(gx*128+64),ly=y-(gy*128+104),rx=128-lx,ry=128-ly;
        unsigned a=at(gx,gy),b=at(gx+1,gy),c=at(gx,gy+1),d=at(gx+1,gy+1);
        if(!(a|b|c|d))return false;
        int r=width/2;
        if(ly<=r && ((a&2) || (b&8)))return true;
        if(ry<=r && ((c&2) || (d&8)))return true;
        if(lx<=r && ((a&4) || (c&1)))return true;
        if(rx<=r && ((b&4) || (d&1)))return true;
        int rr=r*r;
        return (a && lx*lx+ly*ly<=rr) || (b && rx*rx+ly*ly<=rr) ||
               (c && lx*lx+ry*ry<=rr) || (d && rx*rx+ry*ry<=rr);
    }
};
