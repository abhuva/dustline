#pragma once
#include <cstdint>
#include "mapgen_kernels.h"

// Same integer-only generator is compiled on the GBA and in host regression tests.
// Logical cells are 128px; graphics/collision refine their corners on an 8px grid.
struct cave_scratch {
    uint8_t work[64*64];
    uint16_t queue[64*64];
};

class cave_layout {
public:
    static constexpr int columns=64, cell_size=128, extent=columns*cell_size;
    static constexpr int count=columns*columns, town_count=6;
    static constexpr uint32_t fixed_seed=0x00C0FFEE;
    struct point { int x,y; };
    using progress_fn=void(*)(int);

    void generate(uint32_t seed,cave_scratch& scratch,progress_fn progress=nullptr) {
        uint32_t rng=seed?seed:1;
        // 4/5 rule: the center counts too, so >=5 out of 9 equals a wall with
        // >=4 wall neighbours surviving, or floor with >=5 neighbours becoming wall.
        mapgen::random_fill(_cells,rng,47,2,progress);
        for(int pass=0;pass<5;++pass) {
            mapgen::cellular(_cells,scratch.work,0x1e0,0x1f0,2,8);
            for(int i=0;i<count;++i) _cells[i]=scratch.work[i];
            if(progress) progress(16+pass*10);
        }
        finish(seed,_cells,scratch,progress);
    }

    // The editor and ROM share this visible finalization stage, including the
    // original pathological-seed fallback and placement/collision behavior.
    void finish(uint32_t seed,const uint8_t* input,cave_scratch& scratch,progress_fn progress=nullptr) {
        _seed=seed;
        for(int i=0;i<count;++i)
            _cells[i]=uint8_t(mapgen::edge(i%columns,i/columns,2) || input[i]!=0);
        // Locate the largest 4-connected floor component, then wall off the rest.
        for(auto& value:scratch.work) value=0;
        int largest=0,origin=0;
        for(int i=0;i<count;++i) if(!_cells[i] && !scratch.work[i]) {
            int size=flood(i,scratch,1,progress);
            if(size>largest) { largest=size; origin=i; }
        }
        // Pathological all-solid seeds still get a connected playable clearing.
        // Host seed sweeps report this fallback; it never silently returns no spawn.
        _fallback=largest<256;
        if(_fallback) {
            for(int y=columns/2-12;y<columns/2+12;++y)
                for(int x=columns/2-12;x<columns/2+12;++x) _cells[y*columns+x]=0;
            origin=(columns/2)*columns+columns/2;
        }
        for(auto& value:scratch.work) value=0;
        flood(origin,scratch,1,progress);
        for(int i=0;i<count;++i) _cells[i]=uint8_t(!scratch.work[i]);
        // Spawn nearest the center of the reachable component, with an eastward
        // approach to a nearby first destination so the town loop is easy to try.
        int best=0x7fffffff,sx=columns/2,sy=columns/2;
        for(int y=4;y<columns-4;++y) for(int x=4;x<columns-6;++x) if(!wall(x,y)) {
            int d=abs(x-columns/2)+abs(y-columns/2);
            if(d<best) { best=d; sx=x; sy=y; }
        }
        carve(sx,sy,1); carve(sx+1,sy,1); carve(sx+2,sy,1);
        _spawn={sx*cell_size+64,sy*cell_size+80};
        _towns[0]={(sx+2)*cell_size+64,sy*cell_size+80};
        // Prefer branch ends, spread across the component. Carving a small plaza
        // around each reachable anchor only adds connected floor, never isolates it.
        for(int t=1;t<town_count;++t) {
            int bx=sx,by=sy,score=-1;
            for(int y=4;y<columns-4;++y) for(int x=4;x<columns-4;++x) if(!wall(x,y)) {
                int distance=1000;
                for(int j=0;j<t;++j) {
                    int d=abs(x-_towns[j].x/cell_size)+abs(y-_towns[j].y/cell_size);
                    if(d<distance) distance=d;
                }
                int exits=!wall(x-1,y)+!wall(x+1,y)+!wall(x,y-1)+!wall(x,y+1);
                int candidate=distance*16+(exits<=2?160:0)-exits*8;
                if(distance>=8 && candidate>score) { score=candidate; bx=x; by=y; }
            }
            carve(bx,by,1); _towns[t]={bx*cell_size+64,by*cell_size+80};
            if(progress) progress(86+t*2);
        }
        _floor_count=0; _signature=2166136261u;
        for(int i=0;i<count;++i) { _floor_count+=!_cells[i]; _signature=(_signature^_cells[i])*16777619u; }
        if(progress) progress(100);
    }

    bool wall(int x,int y) const {
        return x<0 || y<0 || x>=columns || y>=columns || _cells[y*columns+x];
    }
    bool solid(int x,int y) const {
        if(x<0 || y<0 || x>=extent || y>=extent) return true;
        // Match graphics' 8px cells exactly. Round exposed convex wall corners
        // inward: the coarse floor remains fully drivable, at least 128px wide.
        x=(x/8)*8+4; y=(y/8)*8+4;
        int cx=x/cell_size,cy=y/cell_size;
        if(!wall(cx,cy)) return false;
        int lx=x%cell_size,ly=y%cell_size;
        constexpr int r=48;
        if(lx<r && ly<r && !wall(cx-1,cy) && !wall(cx,cy-1))
            return (lx-r)*(lx-r)+(ly-r)*(ly-r)<=r*r;
        if(lx>=cell_size-r && ly<r && !wall(cx+1,cy) && !wall(cx,cy-1))
            return (lx-(cell_size-r))*(lx-(cell_size-r))+(ly-r)*(ly-r)<=r*r;
        if(lx<r && ly>=cell_size-r && !wall(cx-1,cy) && !wall(cx,cy+1))
            return (lx-r)*(lx-r)+(ly-(cell_size-r))*(ly-(cell_size-r))<=r*r;
        if(lx>=cell_size-r && ly>=cell_size-r && !wall(cx+1,cy) && !wall(cx,cy+1))
            return (lx-(cell_size-r))*(lx-(cell_size-r))+(ly-(cell_size-r))*(ly-(cell_size-r))<=r*r;
        return true;
    }
    int material(int x,int y) const {
        for(const auto& t:_towns) if(abs(x-t.x)<192 && abs(y-t.y)<192) return 0;
        // Coherent patches, not salt-and-pepper per-tile noise. Roads are rare remnants.
        uint32_t v=uint32_t(x/512)*374761393u+uint32_t(y/512)*668265263u+_seed;
        v=(v^(v>>13))*1274126177u; v^=v>>16;
        unsigned kind=v%16;
        return kind<9?0:kind<12?1:kind<15?2:3;
    }
    int town_graphic_at(int x,int y) const {
        for(int t=0;t<town_count;++t)
            if(x>=_towns[t].x-32 && x<_towns[t].x+32 && y>=_towns[t].y-64 && y<_towns[t].y) return t;
        return -1;
    }
    bool town_solid(int x,int y) const {
        for(const auto& t:_towns)
            if(x>=t.x-24 && x<t.x+24 && y>=t.y-56 && y<t.y-8) return true;
        return false;
    }
    int nearby_town(int x,int y) const {
        for(int t=0;t<town_count;++t) {
            // Town points sit at the road entrance along the artwork's south
            // edge. Centre the interaction circle on the visible settlement,
            // so it is reached symmetrically from every approach direction.
            constexpr int radius=72;
            int dx=x-_towns[t].x,dy=y-(_towns[t].y-32);
            if(abs(dx)<radius && abs(dy)<radius && dx*dx+dy*dy<radius*radius) return t;
        }
        return -1;
    }
    point spawn() const { return _spawn; }
    point town(int t) const { return _towns[t]; }
    uint32_t seed() const { return _seed; }
    uint32_t signature() const { return _signature; }
    int floor_count() const { return _floor_count; }
    bool fallback() const { return _fallback; }
    const uint8_t* cells() const { return _cells; }
    void set_floor_cell(int x,int y) {
        if(x>=0 && y>=0 && x<columns && y<columns) _cells[y*columns+x]=0;
    }
    void refresh_signature(uint32_t overlay_tag=0) {
        _floor_count=0; _signature=2166136261u;
        for(int i=0;i<count;++i) { _floor_count+=!_cells[i]; _signature=(_signature^_cells[i])*16777619u; }
        _signature=(_signature^overlay_tag)*16777619u;
    }
private:
    uint8_t _cells[count]={};
    point _towns[town_count]={},_spawn={};
    uint32_t _seed=0,_signature=0;
    int _floor_count=0;
    bool _fallback=false;
    static int abs(int v) { return v<0?-v:v; }
    void carve(int x,int y,int radius) {
        for(int yy=y-radius;yy<=y+radius;++yy) for(int xx=x-radius;xx<=x+radius;++xx)
            if(xx>=2 && yy>=2 && xx<columns-2 && yy<columns-2) _cells[yy*columns+xx]=0;
    }
    int flood(int origin,cave_scratch& s,uint8_t tag,progress_fn progress) const {
        int head=0,tail=1; s.queue[0]=uint16_t(origin); s.work[origin]=tag;
        while(head<tail) {
            int at=s.queue[head++],x=at%columns,y=at/columns;
            const int neighbours[]={at-1,at+1,at-columns,at+columns};
            const bool valid[]={x>0,x<columns-1,y>0,y<columns-1};
            for(int k=0;k<4;++k) if(valid[k]) {
                int next=neighbours[k];
                if(!_cells[next] && !s.work[next]) { s.work[next]=tag; s.queue[tail++]=uint16_t(next); }
            }
            if(progress && head%256==0) progress(75);
        }
        return tail;
    }
};
