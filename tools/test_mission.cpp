#include <cassert>
#include "mission.h"

int main() {
    cave_layout layout; cave_scratch scratch;
    layout.generate(cave_layout::fixed_seed,scratch);
    enemy_spawns spawns;
    spawns.count=3;
    spawns.points[0].x=1000;spawns.points[0].y=1200;spawns.points[0].hp=0;
    spawns.points[1].x=2000;spawns.points[1].y=2200;spawns.points[1].hp=3;
    spawns.points[2].x=3000;spawns.points[2].y=3200;spawns.points[2].hp=3;

    missions::manager jobs;jobs.reset(layout.seed());
    auto first=jobs.offer(missions::type::courier,0,0,layout,spawns);
    auto repeated=jobs.offer(missions::type::courier,0,0,layout,spawns);
    assert(first.target_map>0 && first.target_town>=0 && first.target_town<cave_layout::town_count);
    assert(first.target_map==repeated.target_map && first.target_town==repeated.target_town && first.reward==repeated.reward);
    assert(jobs.accept(missions::type::courier,0,0,layout,spawns));
    assert(!jobs.accept(missions::type::extermination,0,0,layout,spawns));
    assert(!jobs.on_town_enter(0,first.target_town));
    jobs.on_map_loaded(first.target_map,layout,spawns);
    assert(jobs.current().target_x>0 && jobs.current().target_y>0);
    assert(jobs.on_town_enter(first.target_map,first.target_town));
    assert(jobs.current().state==missions::status::complete && jobs.completed()==1);
    assert(jobs.credits()==first.reward);
    jobs.acknowledge();

    auto hunt=jobs.offer(missions::type::extermination,0,0,layout,spawns);
    assert(hunt.target_map>0 && hunt.target_spawn==-1);
    assert(jobs.accept(missions::type::extermination,0,0,layout,spawns));
    jobs.on_map_loaded(hunt.target_map,layout,spawns);
    hunt=jobs.current();assert(hunt.target_spawn>=0 && hunt.target_spawn<spawns.count);
    assert(!jobs.on_enemy_destroyed(0,hunt.target_spawn));
    assert(jobs.on_enemy_destroyed(hunt.target_map,hunt.target_spawn));
    assert(jobs.completed()==2 && jobs.credits()==first.reward+hunt.reward);

    const auto persisted=jobs.current();
    const int persisted_credits=jobs.credits(),persisted_completed=jobs.completed(),persisted_serial=jobs.serial();
    missions::manager restored;
    assert(restored.restore(jobs.map_seed(),persisted,persisted_credits,persisted_completed,persisted_serial));
    assert(restored.current().state==missions::status::complete && restored.current().target_spawn==persisted.target_spawn);
    assert(restored.credits()==persisted_credits && restored.completed()==persisted_completed &&
           restored.serial()==persisted_serial);

    jobs.reset(1234);
    assert(jobs.current().state==missions::status::none);
    assert(jobs.completed()==0 && jobs.credits()==0 && jobs.serial()==0);
}
