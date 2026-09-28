#include <cassert>
#include "race.h"

int main() {
    cave_layout layout;cave_scratch cave_work;
    layout.generate(cave_layout::fixed_seed,cave_work);
    road_network roads;roads.generate(layout,cave_work,48,3);
    races::scratch work;
    races::manager manager;manager.reset(layout.seed());
    const auto assert_clear_flags=[&](const races::course& value) {
        for(int index=0;index<value.checkpoint_count;++index) {
            const uint16_t bit=uint16_t(1u<<index);
            assert(value.left_flag_mask&bit);
            assert(value.right_flag_mask&bit);
            assert(!layout.solid(value.left_flags[index].x,value.left_flags[index].y));
            assert(!layout.solid(value.right_flags[index].x,value.right_flags[index].y));
        }
    };

    const auto& hub=manager.prepare_offer(races::kind::town,0,layout,&roads,work);
    assert(hub.mode==races::kind::town && hub.target_town>0);
    assert(hub.checkpoint_count>=3 && hub.checkpoint_count<=races::max_checkpoints);
    assert(hub.time_limit>hub.par_frames && hub.reward>0);
    assert_clear_flags(hub);
    const int hub_target=hub.target_town;
    assert(manager.accept());manager.start();
    for(int frame=0;frame<180;++frame)manager.update(hub.start.x,hub.start.y);
    assert(manager.current().state==races::phase::running);
    auto first_gate=manager.current().checkpoints[0];
    manager.update(first_gate.x+races::checkpoint_radius,first_gate.y);
    assert(manager.current().next_checkpoint==1);
    for(int index=1;index<manager.current().checkpoint_count;++index) {
        auto point=manager.current().checkpoints[index];
        manager.update(point.x,point.y);
    }
    assert(manager.current().result==races::outcome::complete);
    assert(manager.current().score>0);
    const int award=manager.take_award();
    assert(award>0 && manager.take_award()==0);
    manager.acknowledge();

    const auto& return_trip=manager.prepare_offer(races::kind::town,hub_target,layout,&roads,work);
    assert(return_trip.target_town==0);
    manager.reset(layout.seed());
    const auto& wild=manager.prepare_offer(races::kind::overworld,0,layout,&roads,work);
    assert(wild.mode==races::kind::overworld && wild.target_town==-1);
    assert(wild.checkpoint_count>=4 && wild.checkpoint_count<=races::max_checkpoints);
    for(int index=0;index<wild.checkpoint_count;++index) {
        const auto point=wild.checkpoints[index];
        assert(layout.nearby_town(point.x,point.y)<0);
    }
    const auto wild_start=wild.start;
    assert(manager.accept());manager.start();
    for(int frame=0;frame<180;++frame)manager.update(wild_start.x,wild_start.y);
    for(int frame=0;frame<races::off_course_limit_frames-1;++frame)manager.update(0,0);
    assert(manager.current().state==races::phase::running);
    manager.update(0,0);
    assert(manager.current().state==races::phase::result);
    assert(manager.current().result==races::outcome::off_course);

    manager.reset(layout.seed());
    const auto& patient=manager.prepare_offer(races::kind::overworld,0,layout,&roads,work);
    assert(patient.time_limit>races::no_progress_limit_frames);
    const auto patient_start=patient.start;
    assert(manager.accept());manager.start();
    for(int frame=0;frame<180;++frame)manager.update(patient_start.x,patient_start.y);
    for(int frame=0;frame<races::no_progress_limit_frames-1;++frame)
        manager.update(patient_start.x,patient_start.y);
    assert(manager.current().state==races::phase::running);
    manager.update(patient_start.x,patient_start.y);
    assert(manager.current().result==races::outcome::off_course);

    bool saw_open=false,saw_closed=false;
    for(uint32_t seed=1;seed<=24 && !(saw_open && saw_closed);++seed) {
        manager.reset(seed);
        const auto& variant=manager.prepare_offer(races::kind::overworld,0,layout,&roads,work);
        assert(variant.mode==races::kind::overworld);
        assert(variant.checkpoint_count==races::max_checkpoints);
        saw_closed|=variant.closed;saw_open|=!variant.closed;
        assert_clear_flags(variant);
        if(variant.closed) {
            const auto finish=variant.checkpoints[variant.checkpoint_count-1];
            assert(finish.x==variant.start.x && finish.y==variant.start.y);
        }
    }
    assert(saw_open && saw_closed);
    assert(manager.restore(0x12345678,19));
    assert(manager.map_seed()==0x12345678 && manager.serial()==19 && !manager.session());
}
