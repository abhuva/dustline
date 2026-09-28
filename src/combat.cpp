#include "combat.h"
#include "bn_math.h"
#include "bn_unique_ptr.h"
#include "world_map.h"
#include "wasteland.h"

namespace combat {
namespace {
using bn::fixed;
int abs(int n) { return n<0?-n:n; }
fixed wrap(fixed a) { if(a<0) a+=360; if(a>=360) a-=360; return a; }
fixed missile_speed(int age) {
    // A readable launch that builds urgency without returning to the old 4px/frame dart.
    return bn::min(fixed(3),fixed(1.25)+fixed(bn::max(0,age-1))/32);
}
const driving::Setup& enemy_setup(spawn_profiles::enemy type) {
    return type==spawn_profiles::enemy::scout?driving::setups[1]:type==spawn_profiles::enemy::heavy?driving::setups[2]:driving::setups[3];
}
int enemy_health(spawn_profiles::enemy type) { return type==spawn_profiles::enemy::scout?2:type==spawn_profiles::enemy::heavy?3:enemy_hp; }
const driving::Setup civilian_setup={"PASSENGER",fixed(0.036),fixed(2.05),fixed(0.15),fixed(2.45),
                                     fixed(0.0015),fixed(0.10),1050};
int delta(fixed target,fixed heading) {
    int result=(target-heading).integer();
    if(result>180) result-=360;
    if(result<-180) result+=360;
    return result;
}
BN_CODE_IWRAM bool solid(int x,int y) {
    // Collision-only queries avoid material/palette work in the AI's feelers.
    if(wasteland::active()) {
        const auto& cave=wasteland::layout();
        return cave.solid(x,y) || cave.town_solid(x,y);
    }
    return world_map::surface_at(x,y)==3;
}
BN_CODE_IWRAM bool clear(int x,int y) {
    return !solid(x,y) && !solid(x-9,y) && !solid(x+9,y) && !solid(x,y-9) && !solid(x,y+9);
}
bool close(int x,int y,const driving::Car& car,int radius) {
    int dx=x-car.x.integer(),dy=y-car.y.integer();
    return abs(dx)<=radius && abs(dy)<=radius && dx*dx+dy*dy<=radius*radius;
}
bool line_clear(int x,int y,int to_x,int to_y) {
    int dx=to_x-x,dy=to_y-y,steps=bn::max(abs(dx),abs(dy))/2+1;
    for(int i=0;i<=steps;++i)if(solid(x+dx*i/steps,y+dy*i/steps))return false;
    return true;
}
int road_anchor_x(int cell) { return (cell%64)*cave_layout::cell_size+64; }
int road_anchor_y(int cell) { return (cell/64)*cave_layout::cell_size+104; }
int direction_heading(int direction) {
    constexpr int headings[]={270,0,90,180};
    return headings[direction&3];
}
fixed forward_speed(const driving::Car& car) {
    return bn::degrees_lut_cos(car.heading)*car.vx+bn::degrees_lut_sin(car.heading)*car.vy;
}
driving::Input passenger_input(const driving::Car& car,fixed limit,int steer) {
    const fixed forward=forward_speed(car);
    // Player braking deliberately becomes reverse below walking speed. Traffic
    // must opt in to that separately: ordinary stops become neutral instead.
    if(limit<=fixed(0.03))return {false,forward>fixed(0.08),steer};
    if(forward<fixed(-0.04))return {true,false,steer};
    return {forward<limit-fixed(0.05),forward>limit+fixed(0.14),steer};
}
}

const char* weapon_name(Weapon weapon) {
    constexpr const char* names[]={"GUN","SAW","SIDES","SEEK","TRAP","EMPTY","RADIO","SNIPER","FRONT SHOOTER"};
    return names[int(weapon)];
}
int World::living() const { int n=0; for(const auto& e:enemies) n+=e.hp>0; return n; }
int World::passenger_living() const {
    int count=0;for(const auto& passenger:passengers)count+=passenger.hp>0;return count;
}
void World::clear_bullets() {
    for(auto& b:bullets) b.remaining=0;
    for(auto& m:missiles)m=Missile();
    for(auto& t:traps)t=Trap();
    saw_active=false;
}
void World::refill_player() {
    player_hp=_player_max_hp;player_shield=player_max_shield;
    player_energy=_player_max_energy;
    player_shield_delay=0;player_invulnerability=0;player_destroyed=false;
}
void World::revive_player() {
    const int retained_energy=player_energy;
    refill_player();
    player_energy=retained_energy;
    player_invulnerability=revive_invulnerability;
    clear_bullets();
}
void World::set_max_energy(int maximum) {
    _player_max_energy=bn::max(1,maximum);
    player_energy=bn::min(player_energy,_player_max_energy);
}
bool World::fit_weapon(MountSlot slot,Weapon next) {
    const bool compatible=next==Weapon::empty ||
        (slot==MountSlot::front && (next==Weapon::gun || next==Weapon::sniper)) ||
        (slot==MountSlot::side && (next==Weapon::sides || next==Weapon::front_shooter)) ||
        (slot==MountSlot::special && (next==Weapon::missile || next==Weapon::trap || next==Weapon::radio));
    if(!compatible)return false;
    _fitted[int(slot)]=next;
    _weapon_mask=0;
    for(Weapon fitted:_fitted)switch(fitted) {
    case Weapon::gun:case Weapon::sniper:_weapon_mask|=1;break;
    case Weapon::chainsaw:_weapon_mask|=2;break;
    case Weapon::sides:case Weapon::front_shooter:_weapon_mask|=4;break;
    case Weapon::missile:_weapon_mask|=8;break;
    case Weapon::trap:_weapon_mask|=16;break;
    default:break;
    }
    weapon=_fitted[int(MountSlot::front)];
    saw_active=false;
    return true;
}
void World::reset(const driving::Car& player,bool enabled,cave_layout::progress_fn progress) {
    _enabled=enabled; _cooldowns.fill(0); ticks=0;
    _fitted={Weapon::gun,Weapon::empty,Weapon::empty};fit_weapon(MountSlot::front,Weapon::gun);
    weapon_shots.fill(0);weapon_hits.fill(0);guidance_updates=trap_explosions=0;
    refill_player(); player_hits=player_shots=enemy_shots=hits=kills=wall_hits=expired=0;
    bumps=player_bumps=0; last_pair=-1; last_bump=0; _contact_cooldowns.fill(0);
    clear_bullets(); for(auto& p:pickups)p=Pickup(); fired=impact=destroyed=false;destroyed_count=0;collected_scrap=0;collected_energy=0;
    spawned=despawned=0; spawns.count=0;
    passengers_spawned=passengers_despawned=passengers_killed=0;
    _traffic_rng=mapgen::hash(wasteland::fixed_seed()^0x50415353u);
    _next_passenger_spawn=60;_passenger_serial=0;_passenger_scan=0;
    for(auto& e:enemies) e=Enemy();
    for(auto& passenger:passengers)passenger=passenger_traffic::passenger();
    _passenger_routes.reset();
    if(enabled) {
        _profiles=wasteland::active_spawn_profiles();_profile_count=wasteland::active_spawn_profile_count();
        wasteland::populate_spawns(spawns,progress);
        bn::unique_ptr<cave_scratch> route_work(new cave_scratch());
        _passenger_routes.reset(new passenger_traffic::routes());
        _passenger_routes->build(wasteland::layout(),wasteland::road(),*route_work);
        for(int i=0;i<enemy_count;++i) stream(player,true);
    }
}

void World::stream(const driving::Car& player,bool initial) {
    // Only five cars exist. Distant anchors retain HP/cooldown, never run AI.
    for(auto& e:enemies) if(e.hp>0 && !close(e.car.x.integer(),e.car.y.integer(),player,despawn_range)) {
        auto& p=spawns.points[e.spawn_id]; p.hp=uint8_t(e.hp); p.slot=-1;
        e=Enemy(); ++despawned;
    }
    int slot=-1;
    for(int i=0;i<enemy_count;++i) if(!enemies[i].hp && !enemies[i].explosion) { slot=i; break; }
    if(slot<0) return;
    int nearest=-1,best=spawn_range*spawn_range+1;
    for(int i=0;i<spawns.count;++i) {
        const auto& p=spawns.points[i];
        if(p.slot>=0 || ticks<p.ready_at) continue;
        int dx=int(p.x)-player.x.integer(),dy=int(p.y)-player.y.integer();
        if(abs(dx)>spawn_range || abs(dy)>spawn_range) continue;
        // Includes the maximum velocity camera lead and sprite extent. Never
        // instantiate a streamed enemy inside the visible approach area.
        if(!initial && abs(dx)<224 && abs(dy)<152) continue;
        int distance=dx*dx+dy*dy;
        if(distance>=best) continue;
        bool free=true;
        for(const auto& other:enemies) if(other.hp>0 && close(p.x,p.y,other.car,48)) { free=false; break; }
        if(free) { nearest=i; best=distance; }
    }
    if(nearest<0) return;
    auto& p=spawns.points[nearest]; auto& e=enemies[slot]; e=Enemy();
    const auto& profile=spawn_profiles::find(_profiles,_profile_count,p.profile);
    e.spawn_id=nearest;e.archetype=profile.enemy_type;
    e.hp=p.hp?bn::min(int(p.hp),enemy_health(e.archetype)):enemy_health(e.archetype);
    p.hp=uint8_t(e.hp);p.ready_at=0;p.slot=int8_t(slot);
    e.car.x=p.x; e.car.y=p.y; e.car.mass=enemy_setup(e.archetype).mass;
    e.home_x=p.x; e.home_y=p.y; e.side=nearest%2?1:-1;
    e.car.heading=wrap(bn::degrees_atan2(player.y.integer()-p.y,player.x.integer()-p.x));
    e.cooldown=45+slot*11; ++spawned;
    // A reused body must not inherit the previous occupant's contact cooldown.
    clear_contact_cooldowns(slot+1);
}

void World::clear_contact_cooldowns(int actor) {
    int pair=0;
    for(int a=0;a<=vehicle_count;++a)for(int b=a+1;b<=vehicle_count;++b,++pair)
        if(a==actor || b==actor)_contact_cooldowns[pair]=0;
}

void World::stream_passengers(const driving::Car& player) {
    const auto* roads=wasteland::road();
    if(!roads || !roads->width)return;
    for(auto& passenger:passengers)if(passenger.hp>0) {
        const int dx=passenger.car.x.integer()-player.x.integer();
        const int dy=passenger.car.y.integer()-player.y.integer();
        const int distance=dx*dx+dy*dy;
        if(distance>passenger_traffic::retention_range*passenger_traffic::retention_range)
            ++passenger.distant_frames;
        else passenger.distant_frames=0;
        if(distance>passenger_traffic::hard_retention_range*passenger_traffic::hard_retention_range ||
           passenger.distant_frames>passenger_traffic::distant_grace_frames) {
            passenger=passenger_traffic::passenger();++passengers_despawned;
        }
    }
    if(ticks<_next_passenger_spawn)return;
    int slot=-1;
    for(int i=0;i<passenger_traffic::passenger_count;++i)
        if(!passengers[i].active()){slot=i;break;}
    if(slot<0){_next_passenger_spawn=ticks+180;return;}

    if(!_passenger_scan)_traffic_rng=mapgen::hash(_traffic_rng+0x9e3779b9u);
    const int player_cell_x=player.x.integer()/cave_layout::cell_size;
    const int player_cell_y=player.y.integer()/cave_layout::cell_size;
    int chosen=-1,target=-1;
    // Walk the local square in a deterministic permutation and accept the
    // first suitable road cell. Scoring every one of the 169 cells made the
    // occasional creation frame much more expensive than ordinary traffic.
    constexpr int side=13,total=side*side,step=37;
    const int start=int(_traffic_rng%total);
    constexpr int scans_per_tick=20;
    const int scan_end=bn::min(total,int(_passenger_scan)+scans_per_tick);
    for(int scan=_passenger_scan;scan<scan_end;++scan) {
            const int local=(start+scan*step)%total;
            const int xx=player_cell_x-6+local%side;
            const int yy=player_cell_y-6+local/side;
            if(xx<0 || xx>=64 || yy<0 || yy>=64)continue;
            const int cell=yy*64+xx;
            if(!(roads->at(xx,yy)) || roads->is_reserved(xx,yy))continue;
            const int x=road_anchor_x(cell),y=road_anchor_y(cell);
            const int dx=x-player.x.integer(),dy=y-player.y.integer(),distance=dx*dx+dy*dy;
            if(distance<passenger_traffic::spawn_min_range*passenger_traffic::spawn_min_range ||
               distance>passenger_traffic::spawn_max_range*passenger_traffic::spawn_max_range)continue;
            // Use a camera-safe rectangle, not only radial distance, to prevent pop-in.
            if(abs(dx)<280 && abs(dy)<190)continue;
            bool occupied=false;
            for(const auto& enemy:enemies)if(enemy.hp>0 && close(x,y,enemy.car,56)){occupied=true;break;}
            for(const auto& other:passengers)if(other.hp>0 && close(x,y,other.car,56)){occupied=true;break;}
            if(occupied)continue;
            int candidate_target=-1;
            const int first=int((_traffic_rng+uint32_t(cell*17))%cave_layout::town_count);
            for(int offset=0;offset<cave_layout::town_count;++offset) {
                const int town=(first+offset)%cave_layout::town_count;
                if(_passenger_routes->direction(town,cell)<4){candidate_target=town;break;}
            }
            if(candidate_target<0)continue;
            chosen=cell;target=candidate_target;break;
    }
    if(chosen<0) {
        if(scan_end<total) {_passenger_scan=uint8_t(scan_end);return;}
        _passenger_scan=0;_next_passenger_spawn=ticks+90;return;
    }
    _passenger_scan=0;

    auto& passenger=passengers[slot];passenger=passenger_traffic::passenger();
    passenger.hp=passenger_traffic::passenger_hp;passenger.current_cell=uint16_t(chosen);
    passenger.target_town=uint8_t(target);passenger.serial=++_passenger_serial;
    // A shared civilian silhouette avoids swapping a full 64-direction vehicle
    // sheet during driving. The blue palette and absent mount/HP bar carry the
    // neutral read; behavioral variety comes from the road AI.
    passenger.style=0;
    passenger.avoid_side=passenger.serial&1?1:-1;
    passenger.cruise_speed=fixed(1.05)+fixed(int((_traffic_rng>>8)%6))/10;
    const int direction=_passenger_routes->direction(target,chosen);
    const int lane=bn::min(20,bn::max(10,roads->width/5));
    passenger.car.x=road_anchor_x(chosen)-road_network::dy[direction]*lane;
    passenger.car.y=road_anchor_y(chosen)+road_network::dx[direction]*lane;
    passenger.car.heading=direction_heading(direction);passenger.car.mass=civilian_setup.mass;
    passenger.input={true,false,0};++passengers_spawned;
    clear_contact_cooldowns(1+enemy_count+slot);
    _next_passenger_spawn=ticks+180+int((_traffic_rng>>16)%241);
}

void World::think(Enemy& e,const driving::Car& player) {
    auto& c=e.car;
    int dx=player.x.integer()-c.x.integer(),dy=player.y.integer()-c.y.integer();
    bool engaged=abs(dx)<=enemy_range && abs(dy)<=enemy_range && dx*dx+dy*dy<=enemy_range*enemy_range;
    // Predict moving traffic once, then reuse those centres for every feeler.
    int sensor_x[vehicle_count],sensor_y[vehicle_count],count=1;
    sensor_x[0]=(player.x+player.vx*12).integer(); sensor_y[0]=(player.y+player.vy*12).integer();
    for(const auto& other:enemies) if(&other!=&e && other.hp>0) {
        sensor_x[count]=(other.car.x+other.car.vx*12).integer();
        sensor_y[count]=(other.car.y+other.car.vy*12).integer(); ++count;
    }
    for(const auto& passenger:passengers)if(passenger.hp>0) {
        sensor_x[count]=(passenger.car.x+passenger.car.vx*12).integer();
        sensor_y[count]=(passenger.car.y+passenger.car.vy*12).integer();++count;
    }
    auto open=[&](int x,int y) {
        for(int i=0;i<count;++i) {
            int xx=x-sensor_x[i],yy=y-sensor_y[i];
            if(abs(xx)<24 && abs(yy)<24 && xx*xx+yy*yy<24*24) {
                ++e.vehicle_avoidance; return false;
            }
        }
        return clear(x,y);
    };
    if(e.reverse) {
        int x=(c.x-bn::degrees_lut_cos(c.heading)*20).integer();
        int y=(c.y-bn::degrees_lut_sin(c.heading)*20).integer();
        if(open(x,y)) return; // Keep the recovery's reverse/steering inputs.
        e.reverse=0; e.side=-e.side; e.input={true,false,e.side}; return;
    }
    // A short player lead plus a fan of collision-clear headings. Steering still
    // goes through the same momentum/traction model as the player, never teleporting.
    fixed target=wrap(bn::degrees_atan2(dy+(player.vy*12).integer(),dx+(player.vx*12).integer()));
    // Close passes lead into a short flanking leg, then another attack run.
    // No "park at shooting distance" state, even against a stationary player.
    if(engaged && !e.maneuver && dx*dx+dy*dy<64*64) {
        fixed escape=wrap(target+e.side*105);
        e.goal_x=(c.x+bn::degrees_lut_cos(escape)*120).integer();
        e.goal_y=(c.y+bn::degrees_lut_sin(escape)*120).integer(); e.maneuver=100;
    }
    if(!engaged) {
        fixed patrol=((ticks/180+(&e-&enemies[0]))%4)*90;
        target=wrap(bn::degrees_atan2(e.home_y+(bn::degrees_lut_sin(patrol)*72).integer()-c.y.integer(),
                                    e.home_x+(bn::degrees_lut_cos(patrol)*72).integer()-c.x.integer()));
    } else if(e.maneuver) target=wrap(bn::degrees_atan2(e.goal_y-c.y.integer(),e.goal_x-c.x.integer()));
    int best=-100000,best_offset=0,best_clearance=0;
    constexpr int angles[]={0,-45,45,-90,90};
    for(int offset:angles) {
        fixed angle=wrap(c.heading+offset);
        fixed cx=bn::degrees_lut_cos(angle),sy=bn::degrees_lut_sin(angle);
        int distance=0;
        for(int ahead=12;ahead<=60;ahead+=24) {
            if(!open((c.x+cx*ahead).integer(),(c.y+sy*ahead).integer())) break;
            distance=ahead;
        }
        int score=distance*8-abs(delta(target,angle))*2-abs(offset)/4;
        if(score>best) { best=score; best_offset=offset; best_clearance=distance; }
        if(offset==0 && distance==60 && abs(delta(target,c.heading))<25) break;
    }
    int error=best_offset?best_offset:delta(target,c.heading);
    int steer=error>7?1:error<-7?-1:0;
    // Actual velocity predicts drift into walls even when the nose points clear.
    bool drift_clear=open((c.x+c.vx*24).integer(),(c.y+c.vy*24).integer());
    bool danger=best_clearance<36 || !drift_clear;
    if(danger || abs(best_offset)>=35) ++e.avoidance;
    if(best_clearance<12 || e.stalled>90) {
        e.reverse=42; e.stalled=0; ++e.recoveries;
        e.side=-e.side;
        e.input={false,true,steer? -steer:1}; return;
    }
    fixed limit=danger?fixed(0.5):abs(error)>45?fixed(0.85):engaged?fixed(1.4):fixed(0.65);
    e.input={c.speed<limit,c.speed>limit+fixed(0.15),steer};
}

void World::think(passenger_traffic::passenger& passenger,const driving::Car& player) {
    const auto* roads=wasteland::road();
    if(!roads || !roads->width){passenger.input={false,false,0};return;}
    auto& car=passenger.car;
    if(passenger.dwell || passenger.contact_pause) {
        passenger.input=passenger_input(car,0,0);passenger.stalled=0;return;
    }
    if(passenger.reverse) {
        const uint8_t direction=_passenger_routes->direction(passenger.target_town,passenger.current_cell);
        const int target=direction<4?direction_heading(direction):car.heading.integer();
        const int error=delta(target,car.heading);
        passenger.input={false,true,error>5?-1:error<-5?1:0};return;
    }

    auto reacquire=[&]() {
        const int cx=car.x.integer()/cave_layout::cell_size;
        const int cy=car.y.integer()/cave_layout::cell_size;
        int nearest=-1,best=0x7fffffff;
        for(int yy=bn::max(0,cy-2);yy<=bn::min(63,cy+2);++yy)
            for(int xx=bn::max(0,cx-2);xx<=bn::min(63,cx+2);++xx) {
                const int cell=yy*64+xx;
                if(!roads->at(xx,yy) ||
                   _passenger_routes->direction(passenger.target_town,cell)==passenger_traffic::invalid_direction)
                    continue;
                const int dx=road_anchor_x(cell)-car.x.integer(),dy=road_anchor_y(cell)-car.y.integer();
                const int distance=dx*dx+dy*dy;
                if(distance<best){best=distance;nearest=cell;}
            }
        if(nearest>=0)passenger.current_cell=uint16_t(nearest);
    };
    // Collision impulses and corner cutting are allowed to displace a car.
    // Prefer the road cell it physically occupies before doing a wider search.
    const int car_x=car.x.integer()/cave_layout::cell_size;
    const int car_y=car.y.integer()/cave_layout::cell_size;
    if(car_x>=0 && car_x<64 && car_y>=0 && car_y<64) {
        const int occupied=car_y*64+car_x;
        if(roads->at(car_x,car_y) &&
           _passenger_routes->direction(passenger.target_town,occupied)!=passenger_traffic::invalid_direction)
            passenger.current_cell=uint16_t(occupied);
    }
    int anchor_dx=road_anchor_x(passenger.current_cell)-car.x.integer();
    int anchor_dy=road_anchor_y(passenger.current_cell)-car.y.integer();
    uint8_t direction=_passenger_routes->direction(passenger.target_town,passenger.current_cell);
    if(direction==passenger_traffic::invalid_direction ||
       anchor_dx*anchor_dx+anchor_dy*anchor_dy>220*220) {
        reacquire();direction=_passenger_routes->direction(passenger.target_town,passenger.current_cell);
    }
    if(direction==passenger_traffic::invalid_direction) {
        passenger.input=passenger_input(car,0,0);return;
    }
    if(direction==passenger_traffic::arrived) {
        const int dx=road_anchor_x(passenger.current_cell)-car.x.integer();
        const int dy=road_anchor_y(passenger.current_cell)-car.y.integer();
        if(dx*dx+dy*dy<48*48) {
            passenger.dwell=90;passenger.blocked_frames=0;passenger.stalled=0;
            passenger.input=passenger_input(car,0,0);return;
        }
    }

    int goal_x=road_anchor_x(passenger.current_cell),goal_y=road_anchor_y(passenger.current_cell);
    uint8_t next_direction=passenger_traffic::invalid_direction;
    if(direction<4) {
        const int next=int(passenger.current_cell)+road_network::dx[direction]+
                       road_network::dy[direction]*64;
        const int lane=bn::min(20,bn::max(10,roads->width/5));
        goal_x=road_anchor_x(next)-road_network::dy[direction]*lane;
        goal_y=road_anchor_y(next)+road_network::dx[direction]*lane;
        next_direction=_passenger_routes->direction(passenger.target_town,next);
        // Look partway around the following segment. This anticipates bends
        // without taking a large chord through nearby canyon walls.
        if(next_direction<4) {
            const int after=next+road_network::dx[next_direction]+road_network::dy[next_direction]*64;
            const int after_x=road_anchor_x(after)-road_network::dy[next_direction]*lane;
            const int after_y=road_anchor_y(after)+road_network::dx[next_direction]*lane;
            const int look_x=(goal_x*3+after_x)/4;
            const int look_y=(goal_y*3+after_y)/4;
            if(clear(look_x,look_y)){goal_x=look_x;goal_y=look_y;}
        }
    }
    int dx=goal_x-car.x.integer(),dy=goal_y-car.y.integer();
    fixed target=wrap(bn::degrees_atan2(dy,dx));
    int error=delta(target,car.heading);
    int steer=error>5?1:error<-5?-1:0;
    if(passenger.turnaround && abs(error)>150)steer=passenger.avoid_side;
    fixed limit=passenger.cruise_speed;
    if(direction<4 && next_direction<4 && next_direction!=direction)
        limit=bn::min(limit,fixed(1.05));
    if(passenger.turnaround)limit=bn::min(limit,fixed(0.55));
    else if(abs(error)>70)limit=bn::min(limit,fixed(0.65));
    else if(abs(error)>42)limit=bn::min(limit,fixed(0.95));

    int nearest_ahead=10000,nearest_side=0;
    fixed nearest_speed=0;
    const fixed cs=bn::degrees_lut_cos(car.heading),sn=bn::degrees_lut_sin(car.heading);
    auto consider=[&](const driving::Car& other) {
        const int ox=(other.x+other.vx*12).integer()-(car.x+car.vx*12).integer();
        const int oy=(other.y+other.vy*12).integer()-(car.y+car.vy*12).integer();
        const int front=(cs*ox+sn*oy).integer(),side=(-sn*ox+cs*oy).integer();
        if(front>0 && front<150 && abs(side)<42 && front<nearest_ahead) {
            nearest_ahead=front;nearest_side=side;
            nearest_speed=cs*other.vx+sn*other.vy;
        }
    };
    consider(player);
    for(const auto& enemy:enemies)if(enemy.hp>0)consider(enemy.car);
    for(const auto& other:passengers)if(&other!=&passenger && other.hp>0)consider(other.car);
    if(nearest_ahead<150) {
        ++passenger.avoidance;
        ++passenger.blocked_frames;
        if(nearest_ahead<48)limit=0;
        else {
            const fixed following=bn::max(fixed(0.35),nearest_speed+fixed(nearest_ahead-42)/75);
            limit=bn::min(limit,following);
        }
        if(nearest_ahead>50) {
            const int ahead=bn::min(74,nearest_ahead);
            const bool left_clear=clear((car.x+cs*ahead-sn*24).integer(),
                                        (car.y+sn*ahead+cs*24).integer());
            const bool right_clear=clear((car.x+cs*ahead+sn*24).integer(),
                                         (car.y+sn*ahead-cs*24).integer());
            int side=passenger.avoid_side;
            if(nearest_side>4)side=-1;
            else if(nearest_side<-4)side=1;
            if(side>0 && !left_clear)side=right_clear?-1:0;
            if(side<0 && !right_clear)side=left_clear?1:0;
            if(side) {
                passenger.avoid_side=side;passenger.avoid_timer=36;
                goal_x+=(-sn*side*22).integer();goal_y+=(cs*side*22).integer();
                dx=goal_x-car.x.integer();dy=goal_y-car.y.integer();
                target=wrap(bn::degrees_atan2(dy,dx));error=delta(target,car.heading);
                steer=error>5?1:error<-5?-1:0;
            }
        }
    } else {
        passenger.blocked_frames=0;
        if(passenger.avoid_timer) {
            goal_x+=(-sn*passenger.avoid_side*14).integer();
            goal_y+=(cs*passenger.avoid_side*14).integer();
            dx=goal_x-car.x.integer();dy=goal_y-car.y.integer();
            error=delta(wrap(bn::degrees_atan2(dy,dx)),car.heading);
            steer=error>5?1:error<-5?-1:0;
        }
    }
    const bool forward_clear=clear((car.x+cs*28).integer(),(car.y+sn*28).integer());
    if(!forward_clear){limit=bn::min(limit,fixed(0.35));++passenger.avoidance;}
    if(limit==0)passenger.stalled=0;
    if(passenger.stalled>120 && !passenger.turnaround) {
        passenger.reverse=45;passenger.stalled=0;++passenger.recoveries;
        passenger.input={false,true,steer?-steer:1};return;
    }
    if(passenger.turnaround && abs(error)<42)passenger.turnaround=0;
    passenger.input=passenger_input(car,limit,steer);
}

bool World::shoot(const driving::Car& car,bool hostile,Weapon source,int angle,int lateral,
                  int range,int speed,int damage_amount) {
    for(auto& b:bullets) if(b.remaining==0) {
        fixed heading=wrap(car.heading+angle);
        fixed cx=bn::degrees_lut_cos(heading),sy=bn::degrees_lut_sin(heading);
        // Muzzle clearance is swept too: no firing through a wall at point blank.
        for(int d=2;d<=14;d+=4) if(solid((car.x+cx*d).integer(),(car.y+sy*d).integer())) {
            ++wall_hits; return false;
        }
        b.x=car.x+cx*14-sy*lateral;b.y=car.y+sy*14+cx*lateral;
        b.vx=cx*speed;b.vy=sy*speed;b.remaining=range;b.damage=damage_amount;
        b.hostile=hostile;b.source=source;
        if(hostile)++enemy_shots;
        else { ++player_shots;++weapon_shots[int(source)];fired=true; }
        return true;
    }
    return false;
}

void World::damage(Enemy& e,int amount,Weapon source) {
    e.hp=bn::max(0,e.hp-amount);++hits;++weapon_hits[int(source)];e.flash=12;impact=true;
    auto& p=spawns.points[e.spawn_id];p.hp=uint8_t(e.hp);
    if(!e.hp) {
        ++kills;e.explosion=24;destroyed=true;
        drop_loot(e);
        if(destroyed_count<destroyed_spawns.size())destroyed_spawns[destroyed_count++]=e.spawn_id;
        const auto& profile=spawn_profiles::find(_profiles,_profile_count,p.profile);
        p.slot=-1;p.ready_at=ticks+profile.respawn_frames;
    }
}
void World::damage(passenger_traffic::passenger& passenger,int amount,Weapon source,bool player_attack) {
    passenger.hp=bn::max(0,passenger.hp-amount);passenger.flash=12;impact=true;
    if(player_attack){++hits;++weapon_hits[int(source)];}
    if(!passenger.hp) {
        passenger.explosion=24;destroyed=true;
        if(player_attack)++passengers_killed;
    }
}
void World::drop_loot(Enemy& e) {
    auto& anchor=spawns.points[e.spawn_id];const auto& profile=spawn_profiles::find(_profiles,_profile_count,anchor.profile);
    uint32_t roll=mapgen::hash(wasteland::fixed_seed()^uint32_t(e.spawn_id*374761393u)^uint32_t(++anchor.reward_rolls*668265263u));
    auto add=[&](uint8_t kind,uint8_t value,int offset) {
        for(auto& pickup:pickups)if(!pickup.remaining){pickup.x=e.car.x+offset;pickup.y=e.car.y+(offset?6:0);pickup.kind=kind;pickup.value=value;pickup.remaining=1800;return;}
    };
    if(int(roll%100)<profile.scrap_chance) {
        int range=profile.scrap_max-profile.scrap_min+1;
        uint8_t amount=uint8_t(profile.scrap_min+(range>0?int((roll>>8)%unsigned(range)):0));if(amount)add(0,amount,-7);
    }
    const uint32_t energy_roll=mapgen::hash(roll^0x9E3779B9u);
    if(int(energy_roll%100)<profile.energy_chance) {
        const int range=profile.energy_max-profile.energy_min+1;
        const uint8_t amount=uint8_t(profile.energy_min+(range>0?int((energy_roll>>8)%unsigned(range)):0));
        if(amount)add(1,amount,0);
    }
}
void World::update_pickups(const driving::Car& player) {
    const int magnet=_salvage_magnet?96:48;
    for(auto& pickup:pickups)if(pickup.remaining) {
        --pickup.remaining;int dx=player.x.integer()-pickup.x.integer(),dy=player.y.integer()-pickup.y.integer();
        if(pickup.kind==1 && player_energy>=_player_max_energy)continue;
        if(abs(dx)<=magnet && abs(dy)<=magnet && dx*dx+dy*dy<=magnet*magnet) {pickup.x+=fixed(dx)/8;pickup.y+=fixed(dy)/8;}
        if(abs(dx)<=12 && abs(dy)<=12) {
            if(pickup.kind==1) {
                const int before=player_energy;player_energy=bn::min(_player_max_energy,player_energy+pickup.value);
                collected_energy+=player_energy-before;
            } else collected_scrap+=pickup.value;
            pickup.remaining=0;
        }
    }
}
void World::damage_player(int amount) {
    if(player_destroyed || player_invulnerability)return;
    ++player_hits;impact=true;player_shield_delay=shield_recharge_delay;
    const int shield_damage=bn::min(player_shield,amount);
    player_shield-=shield_damage;amount-=shield_damage;
    player_hp=bn::max(0,player_hp-amount);
    if(!player_hp)player_destroyed=true;
}
void World::explode(int x,int y,int radius,int amount,Weapon source) {
    for(auto& e:enemies)if(e.hp>0 && close(x,y,e.car,radius) &&
        line_clear(x,y,e.car.x.integer(),e.car.y.integer()))damage(e,amount,source);
    for(auto& passenger:passengers)if(passenger.hp>0 && close(x,y,passenger.car,radius) &&
        line_clear(x,y,passenger.car.x.integer(),passenger.car.y.integer()))
        damage(passenger,amount,source,true);
}
void World::fire_weapon(const driving::Car& player,Weapon candidate) {
    if(!weapon_enabled(candidate))return;
    weapon=candidate;
    const int id=int(candidate);
    fixed cs=bn::degrees_lut_cos(player.heading),sn=bn::degrees_lut_sin(player.heading);
    if(candidate==Weapon::chainsaw) {
        if(player_energy<weapon_energy_costs[id])return;
        saw_x=player.x+cs*24;saw_y=player.y+sn*24;
        saw_active=line_clear(player.x.integer(),player.y.integer(),saw_x.integer(),saw_y.integer());
        if(saw_active && !_cooldowns[id]) {
            explode(saw_x.integer(),saw_y.integer(),saw_radius+11,saw_damage,candidate);
            player_energy-=weapon_energy_costs[id];++weapon_shots[id];++player_shots;_cooldowns[id]=8;
        }
        return;
    }
    if(_cooldowns[id])return;
    if(player_energy<weapon_energy_costs[id])return;
    switch(candidate) {
    case Weapon::gun:shoot(player,false,Weapon::gun);_cooldowns[id]=player_interval;break;
    case Weapon::sniper:
        if(shoot(player,false,Weapon::sniper,0,0,sniper_range,8,sniper_damage))
            player_energy-=weapon_energy_costs[id];
        _cooldowns[id]=sniper_interval;break;
    case Weapon::sides: {
        int free=0;for(const auto& b:bullets)free+=!b.remaining;
        if(free>=2) {
            const bool left=shoot(player,false,Weapon::sides,-90),right=shoot(player,false,Weapon::sides,90);
            if(left || right)player_energy-=weapon_energy_costs[id];
        }
        _cooldowns[id]=16;break;
    }
    case Weapon::front_shooter: {
        int free=0;for(const auto& b:bullets)free+=!b.remaining;
        if(free>=2) {
            const bool left=shoot(player,false,Weapon::front_shooter,0,-7);
            const bool right=shoot(player,false,Weapon::front_shooter,0,7);
            if(left || right)player_energy-=weapon_energy_costs[id];
        }
        _cooldowns[id]=front_shooter_interval;break;
    }
    case Weapon::missile:
        for(auto& m:missiles)if(!m.remaining && !m.explosion) {
            fixed x=player.x+cs*18,y=player.y+sn*18;
            if(line_clear(player.x.integer(),player.y.integer(),x.integer(),y.integer())) {
                fixed speed=missile_speed(0);
                m=Missile();m.x=x;m.y=y;m.vx=cs*speed;m.vy=sn*speed;m.heading=player.heading;m.remaining=150;
                player_energy-=weapon_energy_costs[id];++weapon_shots[id];++player_shots;fired=true;
            } else ++wall_hits;
            break;
        }
        _cooldowns[id]=36;break;
    case Weapon::trap:
        for(auto& t:traps)if(!t.remaining && !t.explosion) {
            fixed x=player.x-cs*22,y=player.y-sn*22;
            if(clear(x.integer(),y.integer()) && line_clear(player.x.integer(),player.y.integer(),x.integer(),y.integer())) {
                t=Trap();t.x=x;t.y=y;t.remaining=900;t.arm=18;
                player_energy-=weapon_energy_costs[id];++weapon_shots[id];++player_shots;fired=true;
            } else ++wall_hits;
            break;
        }
        _cooldowns[id]=45;break;
    default:break;
    }
}
void World::update_specials(const driving::Car&) {
    for(auto& m:missiles) {
        if(m.explosion)--m.explosion;
        if(!m.remaining)continue;
        ++m.age;
        // Launch straight first. Track a spawn identity, so a recycled enemy
        // slot cannot silently become the old target. Reacquire after a loss.
        if(m.age>=8 && (m.age-8)%6==0) {
            const Enemy* target=nullptr;
            for(const auto& e:enemies)if(e.hp>0 && e.spawn_id==m.target_spawn)target=&e;
            if(!target) {
                int best=0x7fffffff;
                for(const auto& e:enemies)if(e.hp>0) {
                    int dx=e.car.x.integer()-m.x.integer(),dy=e.car.y.integer()-m.y.integer(),d=dx*dx+dy*dy;
                    if(d<best){best=d;target=&e;}
                }
            }
            m.target_spawn=target?target->spawn_id:-1;
            if(target) {
                m.heading=wrap(bn::degrees_atan2(target->car.y.integer()-m.y.integer(),target->car.x.integer()-m.x.integer()));
                ++guidance_updates;
            }
        }
        fixed speed=missile_speed(m.age);
        m.vx=bn::degrees_lut_cos(m.heading)*speed;m.vy=bn::degrees_lut_sin(m.heading)*speed;
        for(int sub=0;sub<2 && m.remaining;++sub) {
            m.x+=m.vx/2;m.y+=m.vy/2;
            if(solid(m.x.integer(),m.y.integer())) { ++wall_hits;m.remaining=0;m.explosion=24; }
            else for(auto& e:enemies)if(e.hp>0 && close(m.x.integer(),m.y.integer(),e.car,12)) {
                damage(e,missile_damage,Weapon::missile);m.remaining=0;m.explosion=24;break;
            }
            if(m.remaining)for(auto& passenger:passengers)
                if(passenger.hp>0 && close(m.x.integer(),m.y.integer(),passenger.car,12)) {
                    damage(passenger,missile_damage,Weapon::missile,true);
                    m.remaining=0;m.explosion=24;break;
                }
        }
        if(m.remaining && !--m.remaining)++expired;
    }
    for(auto& t:traps) {
        if(t.explosion)--t.explosion;
        if(!t.remaining)continue;
        if(!--t.remaining){++expired;continue;}
        if(t.arm){--t.arm;continue;}
        for(const auto& e:enemies)if(e.hp>0 && close(t.x.integer(),t.y.integer(),e.car,18) &&
            line_clear(t.x.integer(),t.y.integer(),e.car.x.integer(),e.car.y.integer())) {
            t.remaining=0;t.explosion=24;++trap_explosions;impact=true;
            explode(t.x.integer(),t.y.integer(),32,trap_damage,Weapon::trap);break;
        }
        if(t.remaining)for(const auto& passenger:passengers)
            if(passenger.hp>0 && close(t.x.integer(),t.y.integer(),passenger.car,18) &&
               line_clear(t.x.integer(),t.y.integer(),passenger.car.x.integer(),passenger.car.y.integer())) {
                t.remaining=0;t.explosion=24;++trap_explosions;impact=true;
                explode(t.x.integer(),t.y.integer(),32,trap_damage,Weapon::trap);break;
            }
    }
}
bool World::hit_at(Bullet& b,const driving::Car& player) {
    int x=b.x.integer(),y=b.y.integer();
    if(solid(x,y)) { ++wall_hits; return true; }
    if(b.hostile) {
        if(close(x,y,player,10)) { damage_player(1); return true; }
        for(auto& passenger:passengers)if(passenger.hp>0 && close(x,y,passenger.car,11)) {
            damage(passenger,b.damage,b.source,false);return true;
        }
    } else for(auto& e:enemies) if(e.hp>0 && close(x,y,e.car,11)) {
        damage(e,b.damage,b.source);
        return true;
    }
    if(!b.hostile)for(auto& passenger:passengers)
        if(passenger.hp>0 && close(x,y,passenger.car,11)) {
            damage(passenger,b.damage,b.source,true);return true;
        }
    return false;
}

void World::update_passengers(const driving::Car& player) {
    for(int i=0;i<passenger_traffic::passenger_count;++i) {
        auto& passenger=passengers[i];
        if(passenger.flash)--passenger.flash;
        if(passenger.explosion) {
            if(!--passenger.explosion)passenger=passenger_traffic::passenger();
            continue;
        }
        if(passenger.hp<=0)continue;
        if(passenger.reverse)--passenger.reverse;
        if(passenger.turnaround)--passenger.turnaround;
        if(passenger.contact_pause) {
            --passenger.contact_pause;
            passenger.input={false,false,0};
            passenger.stalled=0;
        }
        if(passenger.avoid_timer)--passenger.avoid_timer;
        if(passenger.dwell && !--passenger.dwell) {
            const int first=(passenger.target_town+1+passenger.serial)%cave_layout::town_count;
            for(int offset=0;offset<cave_layout::town_count;++offset) {
                const int town=(first+offset)%cave_layout::town_count;
                if(town!=passenger.target_town &&
                   _passenger_routes->direction(town,passenger.current_cell)<4) {
                    passenger.target_town=uint8_t(town);
                    passenger.turnaround=180;
                    passenger.avoid_side=(passenger.serial+town)&1?1:-1;
                    break;
                }
            }
        }
        const int player_dx=passenger.car.x.integer()-player.x.integer();
        const int player_dy=passenger.car.y.integer()-player.y.integer();
        const bool near=abs(player_dx)<260 && abs(player_dy)<180;
        // Nearby traffic reacts at the normal AI cadence. Far outside the
        // screen its last control input remains useful for much longer, so
        // stagger route planning across sixteen frames. Keep both cadences
        // off hostile streaming frames and avoid stacking distant planning on
        // the most expensive hostile thinker.
        const bool planning_frame=near?ticks%passenger_traffic::passenger_count==i:
                                   (ticks&15)==i*4+1;
        if((ticks&7) && ticks%enemy_count && planning_frame)
            think(passenger,player);
        // A staggered thinker can leave a brake command active for multiple
        // physics frames. Never let an ordinary stop cross into the reverse
        // behavior reserved for explicit stuck recovery.
        if(passenger.input.brake && !passenger.reverse && forward_speed(passenger.car)<=fixed(0.08))
            passenger.input.brake=false;
        if(!passenger.reverse && !passenger.contact_pause) {
            const fixed forward=forward_speed(passenger.car);
            if(forward<0) {
                const fixed cs=bn::degrees_lut_cos(passenger.car.heading);
                const fixed sn=bn::degrees_lut_sin(passenger.car.heading);
                // Tight steering can rotate the body past its residual
                // momentum. Remove only that backwards component; lateral
                // motion and externally imparted collision motion remain.
                passenger.car.vx-=cs*forward;passenger.car.vy-=sn*forward;
            }
        }
        fixed old_x=passenger.car.x,old_y=passenger.car.y;
        // Off-screen traffic only needs periodic handling/terrain correction;
        // integrate velocity on the intervening frames so motion remains
        // continuous and turning around still reveals the same approaching car.
        if(near || (ticks&3)==i+1)
            passenger.car.step(passenger.input,civilian_setup);
        else {
            passenger.car.x+=passenger.car.vx;
            passenger.car.y+=passenger.car.vy;
        }
        const bool deliberate_stop=passenger.dwell || passenger.contact_pause || passenger.blocked_frames;
        if(deliberate_stop)passenger.stalled=0;
        else if(bn::abs(passenger.car.x-old_x)+bn::abs(passenger.car.y-old_y)<fixed(0.035))
            ++passenger.stalled;
        else {passenger.stalled=0;++passenger.moving_frames;}
    }
}

void World::resolve_vehicle_contacts(driving::Car& player) {
    for(auto& cooldown:_contact_cooldowns)if(cooldown)--cooldown;
    // Small fixed population: two solver passes over all live pairs. Broad phase
    // rejects distant cars before any rectangle projections or terrain queries.
    driving::Car* cars[vehicle_count+1];bool active[vehicle_count+1];
    cars[0]=&player;active[0]=true;
    for(int i=0;i<enemy_count;++i){cars[i+1]=&enemies[i].car;active[i+1]=enemies[i].hp>0;}
    for(int i=0;i<passenger_traffic::passenger_count;++i) {
        cars[1+enemy_count+i]=&passengers[i].car;
        // Distant traffic is deliberately a coarse simulation. It cannot be
        // seen or reached by projectiles, so defer physical contacts until it
        // enters the retained encounter bubble around the player.
        active[1+enemy_count+i]=passengers[i].hp>0 &&
            bn::abs(passengers[i].car.x-player.x)<360 &&
            bn::abs(passengers[i].car.y-player.y)<260;
    }
    for(int pass=0;pass<2;++pass) {
        int pair=0;
        for(int a=0;a<=vehicle_count;++a)for(int b=a+1;b<=vehicle_count;++b,++pair) {
            if(!active[a] || !active[b])continue;
            // Avoid a ROM-to-IWRAM call for the overwhelmingly common distant
            // pair. driving::collide repeats this guard for direct callers.
            if(bn::abs(cars[a]->x-cars[b]->x)>27 || bn::abs(cars[a]->y-cars[b]->y)>27)
                continue;
            fixed closing=driving::collide(*cars[a],*cars[b]);
            if(closing>fixed(0.04)) {
                auto yield_passenger=[&](int actor) {
                    if(actor<=enemy_count)return;
                    auto& passenger=passengers[actor-1-enemy_count];
                    passenger.contact_pause=bn::max(passenger.contact_pause,18);
                    passenger.blocked_frames=bn::max(passenger.blocked_frames,12);
                    passenger.input={false,false,0};passenger.stalled=0;
                };
                yield_passenger(a);yield_passenger(b);
            }
            if(closing>fixed(0.15) && !_contact_cooldowns[pair]) {
                ++bumps;player_bumps+=a==0;last_pair=pair;
                last_bump=closing;impact=true;_contact_cooldowns[pair]=12;
            }
        }
    }
}

void World::step(driving::Car& player,bool normal_fire,bool special_fire) {
    fired=impact=destroyed=false;destroyed_count=0;collected_scrap=0;collected_energy=0;
    if(!_enabled) return;
    ++ticks;
    if(player_invulnerability)--player_invulnerability;
    if(player_shield_delay)--player_shield_delay;
    else if(player_shield<player_max_shield && player_energy>0 && ticks%shield_recharge_interval==0) {
        ++player_shield;--player_energy;
    }
    if(ticks%8==0) stream(player);
    if(ticks%16==0)stream_passengers(player);
    for(auto& cooldown:_cooldowns)if(cooldown)--cooldown;
    for(int i=0;i<enemy_count;++i) {
        auto& e=enemies[i];
        if(e.flash) --e.flash;
        if(e.explosion) --e.explosion;
        if(e.hp<=0) continue;
        if(e.cooldown) --e.cooldown;
        if(e.maneuver) --e.maneuver;
        if(e.reverse) --e.reverse;
        if(ticks%enemy_count==i) think(e,player);
        fixed old_x=e.car.x,old_y=e.car.y;
        e.car.step(e.input,enemy_setup(e.archetype));
        if(bn::abs(e.car.x-old_x)+bn::abs(e.car.y-old_y)<fixed(0.04)) ++e.stalled;
        else { e.stalled=0; ++e.moving_frames; }
    }
    update_passengers(player);
    resolve_vehicle_contacts(player);
    saw_active=false;
    if(normal_fire) {
        fire_weapon(player,_fitted[int(MountSlot::front)]);
        fire_weapon(player,_fitted[int(MountSlot::side)]);
    }
    if(special_fire)fire_weapon(player,_fitted[int(MountSlot::special)]);
    for(auto& e:enemies) {
        if(e.hp<=0) continue;
        if(e.archetype!=spawn_profiles::enemy::scout && !e.cooldown && !e.reverse && close(player.x.integer(),player.y.integer(),e.car,player_range+14)) {
            int dx=player.x.integer()-e.car.x.integer(),dy=player.y.integer()-e.car.y.integer();
            fixed cs=bn::degrees_lut_cos(e.car.heading),sn=bn::degrees_lut_sin(e.car.heading);
            int front=(cs*dx+sn*dy).integer(),side=(-sn*dx+cs*dy).integer();
            if(front>0 && abs(side)*6<front) {
                // Only fire on clear line of sight. Bullets also test every 2px.
                int steps=bn::max(abs(dx),abs(dy))/8+1;
                bool visible=true;
                for(int k=1;k<=steps;++k) if(solid(e.car.x.integer()+dx*k/steps,e.car.y.integer()+dy*k/steps)) {
                    visible=false; break;
                }
                if(visible) { shoot(e.car,true);e.cooldown=enemy_interval; }
                else e.cooldown=12;
            }
        }
    }
    update_specials(player);
    for(auto& b:bullets) if(b.remaining) {
        // Three-pixel sweeps remain far smaller than an enemy hit radius while
        // avoiding a redundant third terrain/enemy pass for every live shot.
        for(int sub=0;sub<2 && b.remaining; ++sub) {
            b.x+=b.vx/2; b.y+=b.vy/2; b.remaining-=3;
            if(hit_at(b,player)) b.remaining=0;
            else if(b.remaining<=0) { b.remaining=0; ++expired; }
        }
    }
    update_pickups(player);
}
}
