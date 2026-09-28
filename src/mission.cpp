#include "mission.h"
#include "generated/wasteland_recipe.h"

namespace missions {
namespace {
uint32_t mix(uint32_t value) {
    value^=value>>16;value*=0x7feb352du;
    value^=value>>15;value*=0x846ca68bu;
    return value^(value>>16);
}

int route_distance(int from_map,int to_map) {
    if(from_map<0 || from_map>=map_catalog::count || to_map<0 || to_map>=map_catalog::count)
        return -1;
    const uint8_t value=map_catalog::route_distance[from_map*map_catalog::count+to_map];
    return value==255?-1:int(value);
}
}

const char* type_name(type value) {
    constexpr const char* names[]={"NONE","COURIER","HUNT"};
    return names[int(value)];
}

void manager::reset(uint32_t map_seed) {
    _map_seed=map_seed;_current=contract();
    _credits=0;_completed=0;_serial=0;
}

bool manager::restore(uint32_t map_seed,const contract& current,int credits,int completed,int serial) {
    if(int(current.kind)<int(type::none) || int(current.kind)>int(type::extermination) ||
       int(current.state)<int(status::none) || int(current.state)>int(status::complete) ||
       credits<0 || completed<0 || serial<0 || current.progress<0 || current.goal<0 || current.reward<0 ||
       current.origin_map< -1 || current.origin_map>=map_catalog::count ||
       current.target_map< -1 || current.target_map>=map_catalog::count ||
       current.origin_town< -1 || current.origin_town>=cave_layout::town_count ||
       current.target_town< -1 || current.target_town>=cave_layout::town_count ||
       (current.kind==type::none && current.state!=status::none) ||
       (current.kind!=type::none && current.state==status::none)) {
        reset(map_seed);return false;
    }
    _map_seed=map_seed;_current=current;_credits=credits;_completed=completed;_serial=serial;
    return true;
}

contract manager::offer(type kind,int origin_map,int origin,const cave_layout& layout,
                        const enemy_spawns& spawns) const {
    contract result;
    if(kind==type::none || origin<0 || origin>=cave_layout::town_count) return result;
    result.kind=kind;result.origin_map=origin_map;result.origin_town=origin;result.serial=_serial;
    const uint32_t hash=mix(_map_seed^uint32_t(origin*0x9e3779b9u)^uint32_t(_serial*0x85ebca6bu)^uint32_t(int(kind)*97));
    int reachable[64],reachable_count=0;
    for(int candidate=0;candidate<map_catalog::count;++candidate)
        if(candidate!=origin_map && route_distance(origin_map,candidate)>0)
            reachable[reachable_count++]=candidate;
    result.target_map=reachable_count?reachable[hash%uint32_t(reachable_count)]:origin_map;
    const int route_hops=route_distance(origin_map,result.target_map);
    const int hops=route_hops<0?0:route_hops;
    if(kind==type::courier) {
        result.target_town=int((hash>>8)%uint32_t(cave_layout::town_count));
        if(result.target_map==origin_map && result.target_town==origin)
            result.target_town=(origin+1)%cave_layout::town_count;
        if(result.target_map==origin_map) {
            const auto to=layout.town(result.target_town);result.target_x=to.x;result.target_y=to.y;
        }
        result.goal=1;result.reward=250+hops*450;
    } else {
        if(result.target_map==origin_map) {
            if(!spawns.count)return contract();
            result.target_spawn=int((hash>>8)%uint32_t(spawns.count));
            result.target_x=spawns.points[result.target_spawn].x;
            result.target_y=spawns.points[result.target_spawn].y;
        }
        result.goal=1;result.reward=350+hops*550;
    }
    return result;
}

bool manager::accept(type kind,int origin_map,int origin,const cave_layout& layout,const enemy_spawns& spawns) {
    if(_current.state!=status::none)return false;
    auto next=offer(kind,origin_map,origin,layout,spawns);
    if(next.kind==type::none)return false;
    next.state=status::active;_current=next;++_serial;
    return true;
}

void manager::_complete() {
    if(_current.state!=status::active)return;
    _current.progress=_current.goal;_current.state=status::complete;
    _credits+=_current.reward;++_completed;
}

void manager::on_map_loaded(int map_id,const cave_layout& layout,const enemy_spawns& spawns) {
    if(_current.state!=status::active || _current.target_map!=map_id)return;
    if(_current.kind==type::courier) {
        const auto target=layout.town(_current.target_town);
        _current.target_x=target.x;_current.target_y=target.y;
    } else if(_current.kind==type::extermination && spawns.count) {
        if(_current.target_spawn<0 || _current.target_spawn>=spawns.count)
            _current.target_spawn=int(mix(_map_seed^uint32_t(_current.serial*0x85ebca6bu)^uint32_t(map_id*97))%uint32_t(spawns.count));
        _current.target_x=spawns.points[_current.target_spawn].x;
        _current.target_y=spawns.points[_current.target_spawn].y;
    }
}

bool manager::on_town_enter(int map_id,int town_id) {
    if(_current.state==status::active && _current.kind==type::courier &&
       map_id==_current.target_map && town_id==_current.target_town) {
        _complete();return true;
    }
    return false;
}

bool manager::on_enemy_destroyed(int map_id,int spawn_id) {
    if(_current.state==status::active && _current.kind==type::extermination &&
       map_id==_current.target_map && spawn_id==_current.target_spawn) {
        _complete();return true;
    }
    return false;
}

void manager::acknowledge() {
    if(_current.state==status::complete)_current=contract();
}
}
