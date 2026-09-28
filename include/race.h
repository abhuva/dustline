#pragma once

#include <cstdint>
#include "cave_layout.h"
#include "road_network.h"

namespace races {

constexpr int max_checkpoints=16;
constexpr int checkpoint_radius=96;
constexpr int gate_flag_radius=72;
constexpr int off_course_limit_frames=8*60;
constexpr int no_progress_limit_frames=45*60;

enum class kind { none=0, town=1, overworld=2 };
enum class phase { none=0, ready=1, countdown=2, running=3, result=4 };
enum class outcome { none=0, complete=1, timed_out=2, off_course=3, aborted=4, wrecked=5 };

struct point {
    uint16_t x=0,y=0;
};

struct course {
    kind mode=kind::none;
    phase state=phase::none;
    outcome result=outcome::none;
    int origin_town=-1,target_town=-1;
    bool closed=false;
    point start;
    point checkpoints[max_checkpoints];
    point left_flags[max_checkpoints],right_flags[max_checkpoints];
    uint16_t left_flag_mask=0,right_flag_mask=0;
    int checkpoint_count=0,next_checkpoint=0;
    int route_steps=0,par_frames=0,time_limit=0,time_left=0,elapsed=0;
    int score=0,reward=0,earned=0;
    int countdown_frames=0,off_course_frames=0,no_progress_frames=0;
    uint8_t route_mask[cave_layout::count/8]={};
};

// Allocated only while an offer is generated. Keeping it outside manager avoids
// reserving 20 KiB throughout normal driving.
struct scratch {
    uint16_t queue[cave_layout::count];
    uint16_t path[cave_layout::count];
    int16_t parent[cave_layout::count];
};

class manager {
public:
    void reset(uint32_t map_seed);
    const course& prepare_offer(kind mode,int origin_town,const cave_layout& layout,
                                const road_network* roads,scratch& work);
    bool accept();
    void start();
    bool update(int x,int y);
    void abort(outcome reason=outcome::aborted);
    void acknowledge();
    int take_award();

    const course& current() const { return _current; }
    const course& offered() const { return _offer; }
    bool session() const { return _current.state!=phase::none; }
    bool active() const { return _current.state==phase::ready ||
                                _current.state==phase::countdown ||
                                _current.state==phase::running; }
    int serial() const { return _serial; }
    uint32_t map_seed() const { return _map_seed; }
    bool restore(uint32_t map_seed,int serial);

private:
    bool _road_offer(int origin,const cave_layout& layout,const road_network& roads,scratch& work);
    bool _wild_offer(int origin,const cave_layout& layout,scratch& work);
    bool _find_path(int start,int target,const cave_layout& layout,const road_network* roads,
                    bool exclude_towns,scratch& work,int& length);
    void _mark_path(const scratch& work,int length);
    void _add_checkpoint(int cell,bool road_anchor);
    void _place_flags(const cave_layout& layout);
    void _finish();

    uint32_t _map_seed=0;
    course _offer;
    course _current;
    int _serial=0;
    bool _award_taken=false;
};

const char* kind_name(kind value);
const char* outcome_name(outcome value);

}
