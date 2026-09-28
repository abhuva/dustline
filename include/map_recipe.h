#pragma once
#include "cave_layout.h"
#include "road_network.h"

// Recipe wire format. Inputs refer to earlier instructions; -1 means absent.
// The editor compiles a DAG into this bounded instruction stream. No heap,
// floating point, JSON parser or editor state is needed on the GBA.
namespace mapgen {
inline constexpr int version=1,max_nodes=64,max_buffers=6,parameter_words=64,words_per_node=5+parameter_words;
enum class op : int32_t { random, cellular, radial, threshold, combine, blend,
                         invert, largest, noise, plasma, voronoi, world,
                         field_lut, field_paint, materials, material_patches, roads,
                         spawns, decoration, field_fill, mask_field };
enum class kind { mask, field, world, material, spawns, decoration };
enum class error { ok, count, opcode, input, type, parameter, memory };
struct node {
    op operation;
    int32_t a=-1,b=-1,mask=-1;
    uint32_t stream=0;
    int32_t p[parameter_words]={};
};
static_assert(sizeof(node)==words_per_node*4,"Recipe wire format must stay fixed");
inline constexpr node default_nodes[]={
    {op::random,-1,-1,-1,0,{47,2}},
    {op::cellular,0,-1,-1,0,{5,0x1e0,0x1f0,2,8}},
    {op::world,1,-1,-1,0,{}}
};
struct workspace {
    uint8_t buffers[max_buffers][cells];
    cave_scratch scratch;
    cave_layout layout;
    road_network roads;
};
struct result {
    error status=error::ok;
    int failed_node=-1,peak_buffers=0;
    const uint8_t* data=nullptr;
    // Placement branches can carry a second, categorical field. For spawns
    // this is the profile ID sampled at each selected anchor.
    const uint8_t* auxiliary=nullptr;
    kind type=kind::mask;
    bool fallback=false;
};
inline kind output_kind(op operation) {
    if(operation==op::spawns)return kind::spawns;
    if(operation==op::decoration)return kind::decoration;
    if(operation==op::materials)return kind::material;
    if(operation==op::roads) return kind::world;
    return operation==op::radial || operation==op::blend || operation==op::noise || operation==op::plasma ||
        operation==op::voronoi || operation==op::field_lut || operation==op::field_paint ||
        operation==op::material_patches || operation==op::field_fill || operation==op::mask_field ? kind::field :
        operation==op::world ? kind::world : kind::mask;
}
inline bool between(int value,int low,int high) { return value>=low && value<=high; }
inline int clamp(int value,int low,int high) { return value<low?low:value>high?high:value; }
inline error validate(const node* nodes,int count) {
    if(count<1 || count>max_nodes) return error::count;
    for(int i=0;i<count;++i) {
        const auto& n=nodes[i]; const auto* p=n.p;
        if(int(n.operation)<0 || int(n.operation)>int(op::mask_field)) return error::opcode;
        bool placement=n.operation==op::spawns || n.operation==op::decoration;
        bool unary=n.operation==op::cellular || n.operation==op::threshold || n.operation==op::invert ||
            n.operation==op::largest || n.operation==op::world || n.operation==op::field_lut ||
            n.operation==op::field_paint || n.operation==op::materials || n.operation==op::roads || n.operation==op::mask_field;
        bool binary=n.operation==op::combine || n.operation==op::blend;
        if(n.a<-1 || n.b<-1 || n.mask<-1 || n.a>=i || n.b>=i || n.mask>=i) return error::input;
        if((!placement && (unary || binary)!=(n.a>=0)) ||
           (n.operation!=op::spawns && binary!=(n.b>=0))) return error::input;
        if(n.mask>=0 && n.operation!=op::cellular && n.operation!=op::blend && n.operation!=op::field_paint) return error::input;
        if(n.operation==op::field_paint && n.mask<0) return error::input;
        kind expected=n.operation==op::roads?kind::world:
            placement || n.operation==op::threshold || n.operation==op::blend || n.operation==op::field_lut ||
            n.operation==op::field_paint || n.operation==op::materials?kind::field:kind::mask;
        if(n.a>=0 && output_kind(nodes[n.a].operation)!=expected) return error::type;
        if(n.b>=0 && output_kind(nodes[n.b].operation)!=(n.operation==op::spawns?kind::field:expected)) return error::type;
        if(n.mask>=0 && output_kind(nodes[n.mask].operation)!=kind::mask) return error::type;
        if((n.operation==op::world || n.operation==op::roads) && i!=count-1 &&
           (nodes[i+1].operation!=op::roads || nodes[i+1].a!=i)) return error::type;
        switch(n.operation) {
        case op::spawns:
            if(!between(p[0],0,258) || !between(p[1],0,1024) || !between(p[2],0,1))return error::parameter;
            break;
        case op::decoration:
            if(!between(p[0],0,100) || !between(p[5],0,1))return error::parameter;
            for(int k=1;k<=4;++k)if(!between(p[k],0,100))return error::parameter;
            break;
        case op::field_fill: if(!between(p[0],0,255))return error::parameter;break;
        case op::roads:
            if(!between(p[0],8,256) || !between(p[1],0,255) ||
               p[2]!=0) return error::parameter;
            break;
        case op::random: if(!between(p[0],0,100) || !between(p[1],0,8)) return error::parameter; break;
        case op::cellular:
            if(!between(p[0],0,32) || !between(p[1],0,511) || !between(p[2],0,511) ||
               !between(p[3],0,8) || (p[4]!=4 && p[4]!=8)) return error::parameter;
            break;
        case op::radial:
            if(!between(p[0],0,63) || !between(p[1],0,63) || !between(p[2],1,90) || !between(p[3],0,1)) return error::parameter;
            break;
        case op::threshold: if(!between(p[0],0,255) || !between(p[1],0,1)) return error::parameter; break;
        case op::combine: if(!between(p[0],0,3)) return error::parameter; break;
        case op::blend: if(!between(p[0],0,255)) return error::parameter; break;
        case op::noise: if(!between(p[0],1,32) || !between(p[1],1,5)) return error::parameter; break;
        case op::plasma: if(!between(p[0],0,255)) return error::parameter; break;
        case op::voronoi: if(!between(p[0],2,32) || !between(p[1],0,1) || !between(p[2],1,64)) return error::parameter; break;
        case op::field_paint: if(!between(p[0],0,255)) return error::parameter;break;
        case op::field_lut: break; // Parameters contain a compiled 256-byte lookup table.
        default: break;
        }
    }
    return error::ok;
}
inline int flood(const uint8_t* input,int origin,cave_scratch& scratch) {
    int head=0,tail=1; scratch.queue[0]=uint16_t(origin); scratch.work[origin]=1;
    while(head<tail) {
        int at=scratch.queue[head++],x=at%side,y=at/side;
        int neighbours[]={at-1,at+1,at-side,at+side};
        bool valid[]={x>0,x<side-1,y>0,y<side-1};
        for(int k=0;k<4;++k) if(valid[k]) {
            int next_cell=neighbours[k];
            if(!input[next_cell] && !scratch.work[next_cell]) {
                scratch.work[next_cell]=1; scratch.queue[tail++]=uint16_t(next_cell);
            }
        }
    }
    return tail;
}
inline void largest(const uint8_t* input,uint8_t* output,cave_scratch& scratch) {
    for(auto& v:scratch.work) v=0;
    int size=0,origin=-1;
    for(int i=0;i<cells;++i) if(!input[i] && !scratch.work[i]) {
        int found=flood(input,i,scratch);
        if(found>size) { size=found; origin=i; } // Row-major tie-breaking.
    }
    for(auto& v:scratch.work) v=0;
    if(origin>=0) flood(input,origin,scratch);
    for(int i=0;i<cells;++i) output[i]=!scratch.work[i];
}
inline int lattice(uint32_t seed,int x,int y) {
    return int(hash(seed^uint32_t(x)*374761393u^uint32_t(y)*668265263u)&255);
}
inline int value_noise(uint32_t seed,int x,int y,int scale) {
    int cx=x/scale,cy=y/scale,fx=(x%scale)*256/scale,fy=(y%scale)*256/scale;
    fx=fx*fx*(768-2*fx)/65536; fy=fy*fy*(768-2*fy)/65536;
    int top=(lattice(seed,cx,cy)*(256-fx)+lattice(seed,cx+1,cy)*fx)/256;
    int bottom=(lattice(seed,cx,cy+1)*(256-fx)+lattice(seed,cx+1,cy+1)*fx)/256;
    return (top*(256-fy)+bottom*fy)/256;
}
inline void plasma(uint8_t* output,uint32_t rng,int roughness,cave_scratch& scratch) {
    // 65x65 diamond-square lattice fits inside the flood queue's byte storage.
    auto* grid=reinterpret_cast<uint8_t*>(scratch.queue);
    for(int i=0;i<65*65;++i) grid[i]=0;
    grid[0]=uint8_t(next(rng)); grid[64]=uint8_t(next(rng));
    grid[64*65]=uint8_t(next(rng)); grid[65*65-1]=uint8_t(next(rng));
    int amplitude=128;
    for(int step=64;step>=2;step/=2) {
        int half=step/2;
        for(int y=half;y<64;y+=step) for(int x=half;x<64;x+=step) {
            int avg=(grid[(y-half)*65+x-half]+grid[(y-half)*65+x+half]+
                     grid[(y+half)*65+x-half]+grid[(y+half)*65+x+half])/4;
            grid[y*65+x]=uint8_t(clamp(avg+int(next(rng)%unsigned(amplitude*2+1))-amplitude,0,255));
        }
        for(int y=0;y<=64;y+=half) for(int x=(y/half%2)?0:half;x<=64;x+=step) {
            int total=0,count=0;
            if(x>=half) { total+=grid[y*65+x-half]; ++count; }
            if(x+half<=64) { total+=grid[y*65+x+half]; ++count; }
            if(y>=half) { total+=grid[(y-half)*65+x]; ++count; }
            if(y+half<=64) { total+=grid[(y+half)*65+x]; ++count; }
            grid[y*65+x]=uint8_t(clamp(total/count+int(next(rng)%unsigned(amplitude*2+1))-amplitude,0,255));
        }
        amplitude=amplitude*roughness/255;
    }
    for(int y=0;y<side;++y) for(int x=0;x<side;++x) output[y*side+x]=grid[y*65+x];
}
inline result execute(const node* nodes,int count,uint32_t seed,workspace& work,progress_fn progress=nullptr) {
    result r; r.status=validate(nodes,count); if(r.status!=error::ok) return r;
    int slots[max_nodes],last[max_nodes],owner[max_buffers];
    for(int i=0;i<count;++i) { slots[i]=-1; last[i]=i; }
    for(auto& v:owner) v=-1;
    for(int i=0;i<count;++i) {
        const int sources[]={nodes[i].a,nodes[i].b,nodes[i].mask};
        for(int source:sources) if(source>=0) last[source]=i;
    }
    for(int i=0;i<count;++i) {
        const auto& n=nodes[i]; const auto* p=n.p;
        for(int s=0;s<max_buffers;++s) if(owner[s]>=0 && last[owner[s]]<i) owner[s]=-1;
        int slot=0; while(slot<max_buffers && owner[slot]>=0) ++slot;
        if(slot==max_buffers) { r.status=error::memory; r.failed_node=i; return r; }
        owner[slot]=i; slots[i]=slot;
        int live=0; for(int v:owner) live+=v>=0;
        if(live>r.peak_buffers) r.peak_buffers=live;
        auto* out=work.buffers[slot];
        const auto* a=n.a<0?nullptr:work.buffers[slots[n.a]];
        const auto* b=n.b<0?nullptr:work.buffers[slots[n.b]];
        const auto* mask=n.mask<0?nullptr:work.buffers[slots[n.mask]];
        uint32_t rng=stream_seed(seed,n.stream);
        switch(n.operation) {
        case op::spawns: case op::decoration:
            for(int at=0;at<cells;++at)out[at]=a?a[at]:255;
            if(n.operation==op::spawns) r.auxiliary=b;
            break;
        case op::field_fill: for(int at=0;at<cells;++at)out[at]=uint8_t(p[0]);break;
        case op::mask_field: for(int at=0;at<cells;++at)out[at]=a[at]?255:0;break;
        case op::random: random_fill(out,rng,p[0],p[1],progress); break;
        case op::cellular:
            for(int at=0;at<cells;++at) out[at]=a[at];
            for(int pass=0;pass<p[0];++pass) {
                cellular(out,work.scratch.work,p[1],p[2],p[3],p[4],mask,progress);
                for(int at=0;at<cells;++at) out[at]=work.scratch.work[at];
                if(progress) progress(i*70/count);
            }
            break;
        case op::radial:
            for(int y=0;y<side;++y) for(int x=0;x<side;++x) {
                int dx=x-p[0],dy=y-p[1],v=clamp((dx*dx+dy*dy)*255/(p[2]*p[2]),0,255);
                out[y*side+x]=uint8_t(p[3]?255-v:v);
            }
            break;
        case op::threshold:
            for(int at=0;at<cells;++at) out[at]=uint8_t((a[at]>=p[0])!=bool(p[1]));
            break;
        case op::combine:
            for(int at=0;at<cells;++at) out[at]=uint8_t(p[0]==0?(a[at] || b[at]):
                p[0]==1?(a[at] && b[at]):p[0]==2?(a[at] && !b[at]):(a[at]!=b[at]));
            break;
        case op::blend:
            for(int at=0;at<cells;++at) {
                int weight=mask?(mask[at]?p[0]:0):p[0];
                out[at]=uint8_t((a[at]*(255-weight)+b[at]*weight)/255);
            }
            break;
        case op::invert: for(int at=0;at<cells;++at) out[at]=!a[at]; break;
        case op::largest: largest(a,out,work.scratch); break;
        case op::noise:
            for(int y=0;y<side;++y) for(int x=0;x<side;++x) {
                int scale=p[0],weight=16,total=0,weights=0;
                for(int octave=0;octave<p[1];++octave) {
                    total+=value_noise(hash(rng+uint32_t(octave)),x,y,scale)*weight;
                    weights+=weight; weight/=2; scale=scale>1?scale/2:1;
                }
                out[y*side+x]=uint8_t(total/weights);
            }
            break;
        case op::plasma: plasma(out,rng,p[0],work.scratch); break;
        case op::voronoi: {
            int sx[32],sy[32];
            for(int k=0;k<p[0];++k) { sx[k]=int(next(rng)%64); sy[k]=int(next(rng)%64); }
            for(int y=0;y<side;++y) for(int x=0;x<side;++x) {
                int first=100000,second=100000;
                for(int k=0;k<p[0];++k) {
                    int dx=x-sx[k],dy=y-sy[k],d=dx*dx+dy*dy;
                    if(d<first) { second=first; first=d; } else if(d<second) second=d;
                }
                out[y*side+x]=uint8_t(clamp((p[1]?second-first:first)*255/(p[2]*p[2]),0,255));
            }
            break;
        }
        case op::world:
            work.roads.width=0;
            work.layout.finish(seed,a,work.scratch,progress);
            for(int at=0;at<cells;++at) out[at]=work.layout.cells()[at];
            r.fallback=work.layout.fallback();
            break;
        case op::roads:
            work.roads.generate(work.layout,work.scratch,p[0],p[1],progress);
            for(int at=0;at<cells;++at) out[at]=a[at];
            break;
        case op::field_lut:
            for(int at=0;at<cells;++at)out[at]=reinterpret_cast<const uint8_t*>(p)[a[at]];
            break;
        case op::field_paint:
            for(int at=0;at<cells;++at) out[at]=mask[at]?uint8_t(p[0]):a[at];
            break;
        case op::materials: for(int at=0;at<cells;++at) out[at]=a[at]; break;
        case op::material_patches:
            for(int y=0;y<side;++y) for(int x=0;x<side;++x) {
                uint32_t v=uint32_t(x/4)*374761393u+uint32_t(y/4)*668265263u+seed;
                v=(v^(v>>13))*1274126177u; v^=v>>16;
                unsigned value=v%16;out[y*side+x]=uint8_t(value<9?0:value<12?1:value<15?2:3);
            }
            break;
        }
        r.data=out; r.type=output_kind(n.operation);
        if(progress) progress((i+1)*70/count);
    }
    if(progress) progress(100);
    return r;
}
}
