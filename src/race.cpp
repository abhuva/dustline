#include "race.h"

namespace races {
namespace {
uint32_t mix(uint32_t value) {
    value^=value>>16;value*=0x7feb352du;
    value^=value>>15;value*=0x846ca68bu;
    return value^(value>>16);
}

uint32_t next_random(uint32_t& value) {
    value=mix(value+0x9e3779b9u);
    return value;
}

int abs(int value) { return value<0?-value:value; }

int integer_sqrt(int value) {
    if(value<=0)return 0;
    unsigned root=0,bit=1u<<30,input=unsigned(value);
    while(bit>input)bit>>=2;
    while(bit) {
        if(input>=root+bit) {
            input-=root+bit;root=(root>>1)+bit;
        } else root>>=1;
        bit>>=2;
    }
    return int(root);
}

bool route_bit(const course& value,int cell) {
    return value.route_mask[cell>>3]&(1<<(cell&7));
}

void set_route_bit(course& value,int cell) {
    value.route_mask[cell>>3]|=uint8_t(1<<(cell&7));
}

bool near_town(const cave_layout& layout,int cell,int radius) {
    const int x=cell%64,y=cell/64;
    for(int index=0;index<cave_layout::town_count;++index) {
        auto town=layout.town(index);
        if(abs(x-town.x/128)<=radius && abs(y-town.y/128)<=radius)return true;
    }
    return false;
}

bool clear_floor(const cave_layout& layout,int cell) {
    const int x=cell%64,y=cell/64;
    if(x<3 || y<3 || x>=61 || y>=61 || layout.wall(x,y))return false;
    return !layout.wall(x-1,y) && !layout.wall(x+1,y) &&
           !layout.wall(x,y-1) && !layout.wall(x,y+1);
}

bool clear_flag(const cave_layout& layout,int x,int y) {
    constexpr int half_width=5,half_height=8;
    return !layout.solid(x,y) &&
           !layout.solid(x-half_width,y-half_height) &&
           !layout.solid(x+half_width,y-half_height) &&
           !layout.solid(x-half_width,y+half_height) &&
           !layout.solid(x+half_width,y+half_height);
}
}

const char* kind_name(kind value) {
    constexpr const char* names[]={"NONE","TOWN RACE","WILD RACE"};
    return names[int(value)];
}

const char* outcome_name(outcome value) {
    constexpr const char* names[]={"NONE","FINISHED","TIME OUT","OFF COURSE","ABORTED","WRECKED"};
    return names[int(value)];
}

void manager::reset(uint32_t map_seed) {
    _map_seed=map_seed;_offer=course();_current=course();_serial=0;_award_taken=false;
}

bool manager::restore(uint32_t map_seed,int serial) {
    if(serial<0) { reset(map_seed);return false; }
    _map_seed=map_seed;_offer=course();_current=course();_serial=serial;_award_taken=false;
    return true;
}

bool manager::_find_path(int start,int target,const cave_layout& layout,const road_network* roads,
                         bool exclude_towns,scratch& work,int& length) {
    for(int index=0;index<cave_layout::count;++index)work.parent[index]=-1;
    int head=0,tail=1;work.queue[0]=uint16_t(start);work.parent[start]=start;
    while(head<tail && work.parent[target]<0) {
        const int at=work.queue[head++],x=at%64,y=at/64;
        for(int direction=0;direction<4;++direction) {
            const int nx=x+road_network::dx[direction],ny=y+road_network::dy[direction];
            if(nx<0 || ny<0 || nx>=64 || ny>=64)continue;
            const int next=ny*64+nx;
            if(work.parent[next]>=0 || layout.wall(nx,ny))continue;
            if(roads) {
                if(!(roads->at(x,y)&(1<<direction)))continue;
            } else if(exclude_towns && near_town(layout,next,3))continue;
            work.parent[next]=int16_t(at);work.queue[tail++]=uint16_t(next);
        }
    }
    if(work.parent[target]<0)return false;
    length=0;
    for(int at=target;;at=work.parent[at]) {
        work.path[length++]=uint16_t(at);
        if(at==start)break;
    }
    for(int left=0,right=length-1;left<right;++left,--right) {
        const uint16_t swap=work.path[left];work.path[left]=work.path[right];work.path[right]=swap;
    }
    return true;
}

void manager::_mark_path(const scratch& work,int length) {
    for(int index=0;index<length;++index)set_route_bit(_offer,work.path[index]);
    _offer.route_steps+=length>0?length-1:0;
}

void manager::_add_checkpoint(int cell,bool road_anchor) {
    if(_offer.checkpoint_count>=max_checkpoints)return;
    point& output=_offer.checkpoints[_offer.checkpoint_count++];
    output.x=uint16_t((cell%64)*128+64);
    output.y=uint16_t((cell/64)*128+(road_anchor?104:64));
}

void manager::_place_flags(const cave_layout& layout) {
    _offer.left_flag_mask=0;_offer.right_flag_mask=0;
    for(int index=0;index<_offer.checkpoint_count;++index) {
        const point center=_offer.checkpoints[index];
        const point before=index?_offer.checkpoints[index-1]:_offer.start;
        point after;
        if(index+1<_offer.checkpoint_count)after=_offer.checkpoints[index+1];
        else if(_offer.closed && _offer.checkpoint_count>1)after=_offer.checkpoints[0];
        else after=center;
        int dx=int(after.x)-int(before.x),dy=int(after.y)-int(before.y);
        if(!dx && !dy) { dx=int(center.x)-int(before.x);dy=int(center.y)-int(before.y); }
        const int length=integer_sqrt(dx*dx+dy*dy);
        if(!length)continue;
        for(int side=0;side<2;++side) {
            const int sign=side?-1:1;
            point placed{};bool valid=false;
            // The ideal posts sit exactly on the checkpoint radius line. If a
            // nearby wall occupies that point, pull only that post inward.
            for(int radius=gate_flag_radius;radius>=8 && !valid;radius-=8) {
                const int base_x=int(center.x)-dy*radius*sign/length;
                const int base_y=int(center.y)+dx*radius*sign/length;
                for(int shift_index=0;shift_index<5 && !valid;++shift_index) {
                    const int shift=shift_index==0?0:((shift_index+1)/2)*8*(shift_index&1?1:-1);
                    const int x=base_x+dx*shift/length;
                    const int y=base_y+dy*shift/length;
                    if(clear_flag(layout,x,y)) { placed={uint16_t(x),uint16_t(y)};valid=true; }
                }
            }
            if(valid) {
                const uint16_t bit=uint16_t(1u<<index);
                if(side) { _offer.right_flags[index]=placed;_offer.right_flag_mask|=bit; }
                else { _offer.left_flags[index]=placed;_offer.left_flag_mask|=bit; }
            }
        }
    }
}

bool manager::_road_offer(int origin,const cave_layout& layout,const road_network& roads,scratch& work) {
    if(!roads.width)return false;
    uint32_t random=mix(_map_seed^uint32_t(origin*0x9e3779b9u)^uint32_t(_serial*0x85ebca6bu)^0x52414345u);
    const int target=origin==0?1+int(random%uint32_t(cave_layout::town_count-1)):0;
    auto from=layout.town(origin),to=layout.town(target);
    const int start=(from.y/128)*64+from.x/128;
    const int finish=(to.y/128)*64+to.x/128;
    int length=0;
    if(!_find_path(start,finish,layout,&roads,false,work,length))return false;
    _offer.target_town=target;
    _offer.start={uint16_t(from.x),uint16_t(from.y)};
    _mark_path(work,length);
    const int desired=length<9?3:length<20?5:length<36?8:12;
    for(int index=1;index<=desired;++index) {
        const int path_index=index*(length-1)/desired;
        _add_checkpoint(work.path[path_index],true);
    }
    const int pixels=_offer.route_steps*128;
    _offer.par_frames=240+pixels*100/155+desired*18;
    _offer.time_limit=_offer.par_frames*2;
    _offer.reward=220+pixels/9+desired*12;
    _place_flags(layout);
    return _offer.checkpoint_count>0;
}

bool manager::_wild_offer(int origin,const cave_layout& layout,scratch& work) {
    uint32_t random=mix(_map_seed^uint32_t(origin*0x9e3779b9u)^uint32_t(_serial*0x85ebca6bu)^0x57494c44u);
    _offer.closed=(next_random(random)&1)!=0;
    int anchors[5]={};
    for(int anchor=0;anchor<(_offer.closed?4:5);++anchor) {
        bool found=false;
        for(int attempt=0;attempt<512 && !found;++attempt) {
            const int candidate=int(next_random(random)%cave_layout::count);
            if(!clear_floor(layout,candidate) || near_town(layout,candidate,3))continue;
            if(anchor) {
                const int distance=abs(candidate%64-anchors[anchor-1]%64)+
                                   abs(candidate/64-anchors[anchor-1]/64);
                if(distance<7 || distance>22)continue;
            }
            int length=0;
            if(anchor && !_find_path(anchors[anchor-1],candidate,layout,nullptr,true,work,length))continue;
            anchors[anchor]=candidate;found=true;
        }
        if(!found)return false;
    }
    const int anchor_count=_offer.closed?4:5;
    _offer.start={uint16_t((anchors[0]%64)*128+64),uint16_t((anchors[0]/64)*128+64)};
    const int segments=_offer.closed?anchor_count:anchor_count-1;
    for(int segment=0;segment<segments;++segment) {
        const int from=anchors[segment];
        const int to=_offer.closed && segment==segments-1?anchors[0]:anchors[segment+1];
        int length=0;
        if(!_find_path(from,to,layout,nullptr,true,work,length))return false;
        _mark_path(work,length);
        // Four evenly-spaced gates per leg keep wilderness navigation readable
        // without changing the generated route itself.
        for(int division=1;division<=4;++division)
            _add_checkpoint(work.path[division*(length-1)/4],false);
    }
    const int pixels=_offer.route_steps*128;
    _offer.par_frames=300+pixels*100/115+_offer.checkpoint_count*24;
    _offer.time_limit=_offer.par_frames*2;
    _offer.reward=320+pixels/7+_offer.checkpoint_count*18;
    _place_flags(layout);
    return _offer.checkpoint_count>=4;
}

const course& manager::prepare_offer(kind mode,int origin,const cave_layout& layout,
                                     const road_network* roads,scratch& work) {
    if(_offer.mode==mode && _offer.origin_town==origin && _offer.state==phase::ready)return _offer;
    _offer=course();_offer.mode=mode;_offer.state=phase::ready;_offer.origin_town=origin;
    bool valid=false;
    if(mode==kind::town && roads)valid=_road_offer(origin,layout,*roads,work);
    else if(mode==kind::overworld)valid=_wild_offer(origin,layout,work);
    if(!valid)_offer=course();
    return _offer;
}

bool manager::accept() {
    if(session() || _offer.state!=phase::ready || !_offer.checkpoint_count)return false;
    _current=_offer;_offer=course();++_serial;_award_taken=false;return true;
}

void manager::start() {
    if(_current.state!=phase::ready)return;
    _current.state=phase::countdown;_current.countdown_frames=180;
    _current.time_left=_current.time_limit;_current.elapsed=0;
}

void manager::_finish() {
    _current.state=phase::result;_current.result=outcome::complete;
    const int elapsed=_current.elapsed>0?_current.elapsed:1;
    _current.score=_current.par_frames*1000/elapsed;
    if(_current.score>1999)_current.score=1999;
    if(_current.score>=1100)_current.earned=_current.reward*2;
    else if(_current.score>=900)_current.earned=_current.reward*3/2;
    else if(_current.score>=700)_current.earned=_current.reward;
    else _current.earned=0;
}

bool manager::update(int x,int y) {
    if(_current.state==phase::countdown) {
        if(--_current.countdown_frames<=0)_current.state=phase::running;
        return false;
    }
    if(_current.state!=phase::running)return false;
    ++_current.elapsed;--_current.time_left;++_current.no_progress_frames;
    if(_current.next_checkpoint<_current.checkpoint_count) {
        const auto target=_current.checkpoints[_current.next_checkpoint];
        const int dx=x-int(target.x),dy=y-int(target.y);
        if(dx*dx+dy*dy<=checkpoint_radius*checkpoint_radius) {
            ++_current.next_checkpoint;_current.no_progress_frames=0;
            if(_current.next_checkpoint==_current.checkpoint_count) {
                _finish();return true;
            }
        }
    }
    const int cx=x/128,cy=y/128;
    bool on_route=false;
    for(int yy=cy-2;yy<=cy+2 && !on_route;++yy)
        for(int xx=cx-2;xx<=cx+2;++xx)
            if(xx>=0 && yy>=0 && xx<64 && yy<64 && route_bit(_current,yy*64+xx)) {
                on_route=true;break;
            }
    _current.off_course_frames=on_route?0:_current.off_course_frames+1;
    if(_current.time_left<=0)abort(outcome::timed_out);
    else if(_current.off_course_frames>=off_course_limit_frames ||
            _current.no_progress_frames>=no_progress_limit_frames)
        abort(outcome::off_course);
    return _current.state==phase::result;
}

void manager::abort(outcome reason) {
    if(!active())return;
    _current.state=phase::result;_current.result=reason;_current.score=0;_current.earned=0;
}

void manager::acknowledge() {
    if(_current.state==phase::result)_current=course();
}

int manager::take_award() {
    if(_current.state!=phase::result || _current.result!=outcome::complete || _award_taken)return 0;
    _award_taken=true;return _current.earned;
}
}
