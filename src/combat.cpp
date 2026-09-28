#include "combat.h"
#include "bn_math.h"
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
}

const char* weapon_name(Weapon weapon) {
    constexpr const char* names[]={"GUN","SAW","SIDES","SEEK","TRAP","EMPTY","RADIO","SNIPER","FRONT SHOOTER"};
    return names[int(weapon)];
}
int World::living() const { int n=0; for(const auto& e:enemies) n+=e.hp>0; return n; }
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
    for(auto& e:enemies) e=Enemy();
    if(enabled) {
        _profiles=wasteland::active_spawn_profiles();_profile_count=wasteland::active_spawn_profile_count();
        wasteland::populate_spawns(spawns,progress);
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
    int pair=0;
    for(int a=0;a<=enemy_count;++a) for(int b=a+1;b<=enemy_count;++b,++pair)
        if(a==slot+1 || b==slot+1) _contact_cooldowns[pair]=0;
}

void World::think(Enemy& e,const driving::Car& player) {
    auto& c=e.car;
    int dx=player.x.integer()-c.x.integer(),dy=player.y.integer()-c.y.integer();
    bool engaged=abs(dx)<=enemy_range && abs(dy)<=enemy_range && dx*dx+dy*dy<=enemy_range*enemy_range;
    // Predict moving traffic once, then reuse those centres for every feeler.
    int sensor_x[enemy_count],sensor_y[enemy_count],count=1;
    sensor_x[0]=(player.x+player.vx*12).integer(); sensor_y[0]=(player.y+player.vy*12).integer();
    for(const auto& other:enemies) if(&other!=&e && other.hp>0) {
        sensor_x[count]=(other.car.x+other.car.vx*12).integer();
        sensor_y[count]=(other.car.y+other.car.vy*12).integer(); ++count;
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
    }
}
bool World::hit_at(Bullet& b,const driving::Car& player) {
    int x=b.x.integer(),y=b.y.integer();
    if(solid(x,y)) { ++wall_hits; return true; }
    if(b.hostile) {
        if(close(x,y,player,10)) { damage_player(1); return true; }
    } else for(auto& e:enemies) if(e.hp>0 && close(x,y,e.car,11)) {
        damage(e,b.damage,b.source);
        return true;
    }
    return false;
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
    for(auto& cooldown:_contact_cooldowns) if(cooldown) --cooldown;
    // Small fixed population: three solver passes over fifteen pairs. Broad phase
    // rejects distant cars before any rectangle projections or terrain queries.
    for(int pass=0;pass<3;++pass) {
        int pair=0;
        for(int a=0;a<=enemy_count;++a) for(int b=a+1;b<=enemy_count;++b,++pair) {
            if((a && enemies[a-1].hp<=0) || enemies[b-1].hp<=0) continue;
            auto& ca=a?enemies[a-1].car:player; auto& cb=enemies[b-1].car;
            fixed closing=driving::collide(ca,cb);
            if(closing>fixed(0.15) && !_contact_cooldowns[pair]) {
                ++bumps; player_bumps+=a==0; last_pair=pair;
                last_bump=closing; impact=true; _contact_cooldowns[pair]=12;
            }
        }
    }
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
