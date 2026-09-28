#pragma once

#include <cstdint>
#include "driving.h"
#include "road_routes.h"

namespace passenger_traffic {

constexpr int passenger_count=3;
constexpr int passenger_hp=12;
constexpr int spawn_min_range=300;
constexpr int spawn_max_range=720;
constexpr int retention_range=960;
constexpr int hard_retention_range=1200;
constexpr int distant_grace_frames=180;
using routes=road_routes::town_routes;
constexpr uint8_t invalid_direction=road_routes::invalid_direction;
constexpr uint8_t arrived=road_routes::arrived;

struct passenger {
    driving::Car car;
    driving::Input input{false,false,0};
    int hp=0,flash=0,explosion=0,reverse=0,stalled=0,dwell=0,distant_frames=0;
    int avoidance=0,recoveries=0,moving_frames=0;
    int turnaround=0,contact_pause=0,avoid_timer=0,blocked_frames=0;
    int avoid_side=0;
    uint16_t current_cell=0;
    uint8_t target_town=0,style=0;
    uint16_t serial=0;
    bn::fixed cruise_speed=1;

    bool active() const { return hp>0 || explosion>0; }
};

}
