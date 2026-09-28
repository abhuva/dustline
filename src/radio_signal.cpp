#include "radio_signal.h"

#include "bn_algorithm.h"
#include "bn_assert.h"
#include "cave_layout.h"
#include "generated/wasteland_recipe.h"
#include "wasteland.h"
#include "world_map.h"

namespace radio_signal {
namespace {

constexpr int source_spacing_cells=5;
constexpr int player_spacing=384;

int absolute(int value) {
    return value<0?-value:value;
}

int distance_squared(int x1,int y1,int x2,int y2) {
    const int dx=x1-x2,dy=y1-y2;
    return dx*dx+dy*dy;
}

uint32_t mix(uint32_t value) {
    value^=value>>16;value*=0x7feb352du;
    value^=value>>15;value*=0x846ca68bu;
    value^=value>>16;
    return value?value:1;
}

struct map_state {
    bn::array<source,source_count> sources{};
    uint32_t rng=1;
    int desired=minimum_source_count;
    bool initialized=false;
};

uint32_t random(map_state& state) {
    uint32_t value=state.rng;
    value^=value<<13;value^=value>>17;value^=value<<5;
    state.rng=value?value:1;
    return state.rng;
}

bool time_reached(uint32_t now,uint32_t target) {
    return int32_t(now-target)>=0;
}

bool away_from_towns(int cell_x,int cell_y) {
    const auto& layout=wasteland::layout();
    for(int index=0;index<cave_layout::town_count;++index) {
        const auto town=layout.town(index);
        if(absolute(cell_x-town.x/cave_layout::cell_size)<=3 &&
           absolute(cell_y-town.y/cave_layout::cell_size)<=3)return false;
    }
    return true;
}

bool away_from_exits(int cell_x,int cell_y) {
    for(int side=0;side<4;++side)if(world_map::exit_mask()&(1<<side)) {
        const auto portal=world_map::exit(side);
        if(absolute(cell_x-portal.x/cave_layout::cell_size)<=2 &&
           absolute(cell_y-portal.y/cave_layout::cell_size)<=2)return false;
    }
    return true;
}

int quantized_direction(int dx,int dy) {
    const int ax=absolute(dx),ay=absolute(dy);
    if(ay*2<ax)return dx>=0?0:4;
    if(ax*2<ay)return dy>=0?2:6;
    if(dx>=0)return dy>=0?1:7;
    return dy>=0?3:5;
}

}

struct system::storage {
    bn::array<map_state,map_catalog::count> maps{};
    uint32_t session_seed=1;
    uint32_t frame=0;
};

system::system() : _storage(new storage()) {
}

system::~system()=default;

void system::reset(uint32_t session_seed) {
    _storage.reset(new storage());
    _storage->session_seed=session_seed?session_seed:1;
    _map_index=0;_target=-1;_direction=-1;_candidate_direction=-1;
    _candidate_frames=0;_strength=0;
}

void system::_spawn(int index,int avoid_x,int avoid_y) {
    auto& state=_storage->maps[_map_index];
    auto eligible=[&](int cell_x,int cell_y) {
        const auto& layout=wasteland::layout();
        if(layout.wall(cell_x,cell_y) || !away_from_towns(cell_x,cell_y) ||
           !away_from_exits(cell_x,cell_y))return false;
        const int x=cell_x*cave_layout::cell_size+cave_layout::cell_size/2;
        const int y=cell_y*cave_layout::cell_size+cave_layout::cell_size/2;
        if(distance_squared(x,y,avoid_x,avoid_y)<player_spacing*player_spacing)return false;
        for(int other=0;other<source_count;++other)if(other!=index && state.sources[other].active) {
            const auto& signal=state.sources[other];
            const int other_x=signal.x/cave_layout::cell_size;
            const int other_y=signal.y/cave_layout::cell_size;
            if(absolute(cell_x-other_x)+absolute(cell_y-other_y)<source_spacing_cells)return false;
        }
        return true;
    };

    int cell_x=-1,cell_y=-1;
    for(int attempt=0;attempt<192 && cell_x<0;++attempt) {
        const int x=2+int(random(state)%(cave_layout::columns-4));
        const int y=2+int(random(state)%(cave_layout::columns-4));
        if(eligible(x,y)) { cell_x=x;cell_y=y; }
    }
    if(cell_x<0) {
        const int start=int(random(state)%cave_layout::count);
        for(int offset=0;offset<cave_layout::count;++offset) {
            const int cell=(start+offset)%cave_layout::count;
            const int x=cell%cave_layout::columns,y=cell/cave_layout::columns;
            if(eligible(x,y)) { cell_x=x;cell_y=y;break; }
        }
    }
    if(cell_x<0)return;

    auto& signal=state.sources[index];
    const int jitter_x=(int(random(state)%7)-3)*8;
    const int jitter_y=(int(random(state)%7)-3)*8;
    signal.x=cell_x*cave_layout::cell_size+cave_layout::cell_size/2+jitter_x;
    signal.y=cell_y*cave_layout::cell_size+cave_layout::cell_size/2+jitter_y;
    signal.ready_at=0;signal.active=true;
}

void system::load(int map_index) {
    BN_ASSERT(map_index>=0 && map_index<map_catalog::count,"Invalid radio map index");
    _map_index=map_index;_target=-1;_direction=-1;_candidate_direction=-1;
    _candidate_frames=0;_strength=0;
    auto& state=_storage->maps[_map_index];
    if(!state.initialized) {
        state.initialized=true;
        state.rng=mix(_storage->session_seed^map_catalog::maps[map_index].seed^
                      (uint32_t(map_index)+1)*0x9e3779b9u);
        state.desired=minimum_source_count+int(random(state)%
                      (source_count-minimum_source_count+1));
        const auto spawn=wasteland::layout().spawn();
        for(int index=0;index<state.desired;++index)_spawn(index,spawn.x,spawn.y);
    } else {
        const auto spawn=wasteland::layout().spawn();
        for(int index=0;index<state.desired;++index) {
            auto& signal=state.sources[index];
            if(!signal.active && signal.ready_at && time_reached(_storage->frame,signal.ready_at))
                _spawn(index,spawn.x,spawn.y);
        }
    }
}

const source& system::item(int index) const {
    BN_ASSERT(index>=0 && index<source_count,"Invalid radio source index");
    return _storage->maps[_map_index].sources[index];
}

int system::active_count() const {
    int result=0;
    const auto& state=_storage->maps[_map_index];
    for(int index=0;index<state.desired;++index)result+=state.sources[index].active;
    return result;
}

int system::desired_count() const {
    return _storage->maps[_map_index].desired;
}

int system::next_respawn_frames() const {
    int result=-1;
    const auto& state=_storage->maps[_map_index];
    for(int index=0;index<state.desired;++index) {
        const auto& signal=state.sources[index];
        if(!signal.active && signal.ready_at) {
            const int remaining=time_reached(_storage->frame,signal.ready_at)?0:
                                int(signal.ready_at-_storage->frame);
            if(result<0 || remaining<result)result=remaining;
        }
    }
    return result;
}

uint32_t system::clock() const {
    return _storage->frame;
}

void system::_choose_target(const driving::Car& player,bool receiver_fitted) {
    if(!receiver_fitted) {
        _target=-1;_direction=-1;_candidate_direction=-1;_candidate_frames=0;_strength=0;
        return;
    }
    const int player_x=player.x.integer(),player_y=player.y.integer();
    const int maximum=detection_radius*detection_radius;
    int nearest=-1,best=maximum+1;
    const auto& state=_storage->maps[_map_index];
    for(int index=0;index<state.desired;++index)if(state.sources[index].active) {
        const auto& signal=state.sources[index];
        const int distance=distance_squared(player_x,player_y,signal.x,signal.y);
        if(distance<=maximum && distance<best) { best=distance;nearest=index; }
    }
    if(nearest!=_target) {
        _target=nearest;_direction=-1;_candidate_direction=-1;_candidate_frames=0;_strength=0;
    }
}

void system::_update_direction_and_strength(const driving::Car& player) {
    if(_target<0)return;
    const auto& signal=item(_target);
    const int dx=signal.x-player.x.integer(),dy=signal.y-player.y.integer();
    const int candidate=quantized_direction(dx,dy);
    if(candidate==_direction) {
        _candidate_direction=-1;_candidate_frames=0;
    } else if(candidate==_candidate_direction) {
        if(++_candidate_frames>=4) {
            _direction=candidate;_candidate_direction=-1;_candidate_frames=0;
        }
    } else {
        _candidate_direction=candidate;_candidate_frames=1;
        if(_direction<0) { _direction=candidate;_candidate_direction=-1;_candidate_frames=0; }
    }

    const int distance=distance_squared(player.x.integer(),player.y.integer(),signal.x,signal.y);
    if(_strength==3)_strength=distance<=336*336?3:(distance<=680*680?2:1);
    else if(_strength==2)_strength=distance<=256*256?3:(distance<=680*680?2:1);
    else _strength=distance<=256*256?3:(distance<=560*560?2:1);
}

int system::step(const driving::Car& player,bool receiver_fitted,
                 bn::array<combat::Bullet,combat::bullet_count>& bullets) {
    ++_storage->frame;
    auto& state=_storage->maps[_map_index];
    const int player_x=player.x.integer(),player_y=player.y.integer();
    for(int index=0;index<state.desired;++index) {
        auto& signal=state.sources[index];
        if(!signal.active && signal.ready_at && time_reached(_storage->frame,signal.ready_at))
            _spawn(index,player_x,player_y);
    }

    int reward=0;
    constexpr int hit_radius_squared=barrel_hit_radius*barrel_hit_radius;
    for(auto& bullet:bullets)if(bullet.remaining && !bullet.hostile) {
        const int bullet_x=bullet.x.integer(),bullet_y=bullet.y.integer();
        for(int index=0;index<state.desired;++index) {
            auto& signal=state.sources[index];
            if(signal.active && distance_squared(bullet_x,bullet_y,signal.x,signal.y)<=hit_radius_squared) {
                signal.active=false;
                signal.ready_at=_storage->frame+respawn_min_frames+
                                random(state)%(respawn_jitter_frames+1);
                bullet.remaining=0;reward+=scrap_reward;
                if(_target==index) {
                    _target=-1;_direction=-1;_candidate_direction=-1;
                    _candidate_frames=0;_strength=0;
                }
                break;
            }
        }
    }
    _choose_target(player,receiver_fitted);
    _update_direction_and_strength(player);
    return reward;
}

}
