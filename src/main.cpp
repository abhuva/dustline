#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_math.h"
#include "bn_regular_bg_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_font.h"
#include "bn_sprite_text_generator.h"
#include "bn_string.h"
#include "bn_vector.h"
#include "bn_sound_items.h"
#include "bn_sound_handle.h"
#include "bn_sound.h"
#include "bn_music.h"
#include "bn_optional.h"
#include "bn_array.h"
#include "bn_regular_bg_items_title.h"
#include "bn_sprite_items_car.h"
#include "bn_sprite_items_car_sand_buggy.h"
#include "bn_sprite_items_car_old.h"
#include "bn_sprite_items_car_truck.h"
#include "bn_sprite_items_car_pickup.h"
#include "bn_sprite_items_car_loadout.h"
#include "bn_sprite_items_font.h"
#include "bn_sprite_items_particles.h"
#include "bn_sprite_items_dot.h"
#include "bn_sprite_items_mission_dot.h"
#include "bn_sprite_items_settings_tab.h"
#include "bn_sprite_items_race_gate.h"
#include "bn_sprite_items_race_flag.h"
#include "driving.h"
#include "terrain_streamer.h"
#include "bn_unique_ptr.h"
#include "bn_bg_tiles.h"
#include "bn_bg_maps.h"
#include "bn_bg_palettes.h"
#include "bn_sprite_tiles.h"
#include "world_map.h"
#include "wasteland.h"
#include "local_minimap.h"
#include "world_overview.h"
#include "bn_memory.h"
#include "bn_regular_bg_items_hud_waste.h"
#include "bn_regular_bg_items_pause_waste.h"
#include "bn_regular_bg_items_town_blank.h"
#include "bn_regular_bg_items_town_dialog.h"
#include "combat.h"
#include "combat_view.h"
#include "decoration_view.h"
#include "town_scene.h"
#include "weapon_fitting_scene.h"
#include "mission.h"
#include "garage_shop.h"
#include "race.h"
#include "adaptive_music.h"
#include "save_store.h"
#include "radio_signal.h"
#include "radio_signal_view.h"

// Read-only telemetry for emulator regression tests; not a gameplay backdoor.
extern "C" {
// Diagnostic buffers belong in EWRAM; keep the small IWRAM stack available
// for rendering/physics calls rather than reserving it for debug snapshots.
BN_DATA_EWRAM_BSS volatile int dustline_telemetry[66];
BN_DATA_EWRAM_BSS volatile int dustline_combat_telemetry[240];
BN_DATA_EWRAM_BSS volatile int dustline_weapon_telemetry[82];
BN_DATA_EWRAM_BSS volatile int dustline_town_telemetry[10];
BN_DATA_EWRAM_BSS volatile int dustline_mission_telemetry[18];
BN_DATA_EWRAM_BSS volatile int dustline_portal_telemetry[12];
BN_DATA_EWRAM_BSS volatile int dustline_race_telemetry[80];
BN_DATA_EWRAM_BSS volatile int dustline_music_telemetry[16];
BN_DATA_EWRAM_BSS volatile int dustline_progression_telemetry[16];
BN_DATA_EWRAM_BSS volatile int dustline_save_telemetry[13];
BN_DATA_EWRAM_BSS volatile int dustline_radio_telemetry[32];
BN_DATA_EWRAM_BSS volatile int dustline_settings_telemetry[8];
}

namespace {
using bn::fixed;
fixed clamp(fixed v,fixed lo,fixed hi) { return v<lo?lo:v>hi?hi:v; }
void adjust_tuning(driving::Setup& tuning,int property,int direction) {
    switch(property) {
    case 0:
        tuning.acceleration=clamp(tuning.acceleration+fixed(0.005)*direction,0,fixed(0.5));
        break;
    case 1:
        tuning.max_speed=clamp(tuning.max_speed+fixed(0.1)*direction,fixed(0.25),fixed(12));
        break;
    case 2:
        tuning.grip=clamp(tuning.grip+fixed(0.005)*direction,0,fixed(1));
        break;
    case 3:
        tuning.steer=clamp(tuning.steer+fixed(0.1)*direction,0,fixed(12));
        break;
    case 4:
        tuning.coast_drag=clamp(tuning.coast_drag+fixed(0.001)*direction,0,fixed(0.05));
        break;
    case 5:
        tuning.brake_force=clamp(tuning.brake_force+fixed(0.01)*direction,0,fixed(0.3));
        break;
    default:
        tuning.mass=bn::max(100,bn::min(10000,tuning.mass+100*direction));
        break;
    }
}
constexpr int vehicle_type_count=5;
constexpr int settings_panel_count=4;
constexpr int settings_map_panel=0,settings_handling_panel=1,settings_audio_panel=2,settings_save_panel=3;
constexpr int audio_row_count=3;
constexpr int save_row_count=3;
constexpr int handling_row_count=9;
constexpr int handling_battery_row=7;
constexpr int handling_car_row=handling_row_count-1;
constexpr int race_radar_marker_count=1;
constexpr int race_world_marker_linger_frames=60;
constexpr const char* vehicle_names[vehicle_type_count]={
    "ROAD","BUGGY","OLD","TRUCK","PICKUP"
};
constexpr int battery_count=3;
constexpr int battery_capacities[battery_count]={70,100,150};
constexpr int battery_mass[battery_count]={-100,0,200};
constexpr const char* battery_names[battery_count]={"CMP","STD","LRG"};
template<int MaxSize>
void append_fixed(bn::string<MaxSize>& output,fixed value,int decimals) {
    int scale=1;
    for(int index=0;index<decimals;++index)scale*=10;
    const int scaled=(value*scale+fixed(0.5)).integer();
    output+=bn::to_string<8>(scaled/scale);
    if(decimals) {
        output+='.';
        const int fraction=scaled%scale;
        for(int divisor=scale/10;divisor;divisor/=10)output+=char('0'+(fraction/divisor)%10);
    }
}
constexpr int camera_lead_x_limit=80,camera_lead_y_limit=48;
constexpr fixed camera_lead_x_scale=22,camera_lead_y_scale=16,camera_lead_response=fixed(0.12);
constexpr auto font_widths=[] {
    bn::array<int8_t,95> widths{};
    widths.fill(6);
    return widths;
}();
constexpr bn::sprite_font font(bn::sprite_items::font, {}, font_widths);
bn::sprite_text_generator* loading_generator=nullptr;
bn::vector<bn::sprite_ptr,48>* loading_sprites=nullptr;
int loading_progress=0,loading_displayed=-1;
void loading_update(int value) {
    value=value<0?0:value>100?100:value;
    if(value<loading_progress)value=loading_progress;
    loading_progress=value;
    dustline_telemetry[53]=value;
    if(loading_generator && loading_sprites && value!=loading_displayed) {
        loading_displayed=value;
        loading_sprites->clear();
        loading_generator->generate(-48,-18,"GENERATING WORLD",*loading_sprites);
        bn::string<20> bar="[";
        const int filled=value*16/100;
        for(int index=0;index<16;++index)bar+=(index<filled?'#':'-');
        bar+=']';
        loading_generator->generate(-54,4,bar,*loading_sprites);
        bn::string<8> percentage=bn::to_string<4>(value);
        percentage+='%';
        loading_generator->generate(-12,24,percentage,*loading_sprites);
    }
    bn::core::update();
}
void generation_loading_update(int value) { loading_update(value*90/100); }
void spawn_loading_update(int value) { loading_update(90+value*5/100); }

struct Mark {
    bn::sprite_ptr sprite=bn::sprite_items::particles.create_sprite(0,0);
    fixed x=0,y=0;
    int life=0;
    Mark() { sprite.set_visible(false); sprite.set_z_order(10); }
};
}

int main() {
    bn::core::init();
    saves::store cartridge;
    // Title art owns most background tile VRAM. Allocate map graphics only
    // after a map is selected and the title has been released.
    bn::unique_ptr<terrain_streamer> track;
    bn::optional<bn::regular_bg_ptr> hud;
    bn::optional<bn::regular_bg_ptr> overlay=bn::regular_bg_items::title.create_bg(0,0);
    overlay->set_priority(0);
    auto car_sprite=bn::sprite_items::car.create_sprite(0,0);
    car_sprite.set_z_order(-1);
    car_sprite.set_bg_priority(1);
    car_sprite.set_visible(false);
    auto car_loadout_sprite=bn::sprite_items::car_loadout.create_sprite(0,0);
    car_loadout_sprite.set_z_order(-2);
    car_loadout_sprite.set_bg_priority(1);
    car_loadout_sprite.set_visible(false);
    auto minimap_dot=bn::sprite_items::dot.create_sprite(0,0);
    minimap_dot.set_bg_priority(0);
    minimap_dot.set_z_order(-3);
    minimap_dot.set_visible(false);
    auto mission_marker=bn::sprite_items::mission_dot.create_sprite(0,0);
    mission_marker.set_bg_priority(0);
    mission_marker.set_z_order(-4);
    mission_marker.set_visible(false);
    auto settings_goal_marker=bn::sprite_items::mission_dot.create_sprite(0,0);
    settings_goal_marker.set_bg_priority(0);
    settings_goal_marker.set_z_order(-5);
    settings_goal_marker.set_visible(false);
    auto race_world_marker=bn::sprite_items::race_gate.create_sprite(0,0);
    race_world_marker.set_bg_priority(1);
    race_world_marker.set_z_order(-3);
    race_world_marker.set_visible(false);
    auto race_left_flag=bn::sprite_items::race_flag.create_sprite(0,0);
    race_left_flag.set_bg_priority(1);race_left_flag.set_z_order(-2);race_left_flag.set_visible(false);
    auto race_right_flag=bn::sprite_items::race_flag.create_sprite(0,0);
    race_right_flag.set_bg_priority(1);race_right_flag.set_z_order(-2);race_right_flag.set_visible(false);
    race_right_flag.set_horizontal_flip(true);
    bn::vector<bn::sprite_ptr,race_radar_marker_count> race_radar_markers;
    bn::sprite_text_generator text(font);
    text.set_bg_priority(0);
    text.set_z_order(-3);
    bn::vector<bn::sprite_ptr,48> hud_text;
    bn::vector<bn::sprite_ptr,48> loading_text;
    int shown_surface=-1;
    bn::vector<bn::sprite_ptr,12> race_text;
    bn::array<Mark,24> marks;
    int next_mark=0;
    driving::Car car;
    fixed camera_lead_x=0,camera_lead_y=0,camera_x=car.x,camera_y=car.y-6;
    int state=0; // title=0, driving=1, town confirm=3, town=4, loading=5, settings=6, wrecked=7, fitting=8, gate confirm=9
    int settings_return=1;
    int setup=1,car_type=0,battery_selection=1;
    driving::Setup tuning=driving::setups[setup];
    int settings_panel=0,map_selection=0,tuning_selection=0,tuning_repeat=0,tuning_direction=0;
    int audio_selection=0,music_volume=0,sound_volume=10;
    int save_selection=0;
    saves::result save_result=saves::result::none;
    bool erase_confirm=false,loaded_exact_world=false,loaded_position_adjusted=false;
    int load_notice_frames=0;
    bool master_muted=false;
    int selected_map=world_map::start_map();
    int title_selection=0;
    auto compatible_save_available=[&]() {
        saves::game_state value;
        return cartridge.load(value) && world_map::find_persistent_id(value.map_id)>=0;
    };
    bool title_continue_available=compatible_save_available();
    int lap_frames=0,best[3]={0,0,0},laps=0,checkpoint=0;
    int frame=0,notice_frames=180,engine_tick=0,engine_sustain=0;
    int wild_return_frames=0,race_result_frames=0;
    int race_world_linger_checkpoint=-1,race_world_linger_frames=0;
    bool engine_throttle=false;
    fixed engine_volume=0,engine_pitch=0;
    int missed=0;
    bn::optional<bn::sound_handle> engine;
    bn::unique_ptr<local_minimap> radar;
    bn::unique_ptr<world_overview> settings_overview;
    bn::vector<bn::sprite_ptr,4> settings_tabs;
    int ignored_town=-1,current_town=-1,town_visits=0;
    bool town_yes=false,button_guard=false,contract_open=false,race_open=false;
    int ignored_gate=-1,current_gate=-1,gate_visits=0;
    bool gate_yes=false,portals_armed=true;
    int contract_selection=0,race_selection=0;
    missions::manager mission_manager;
    races::manager race_manager;
    int scrap=0,shop_notice=0;
    garage_shop::ownership shop_owned;
    adaptive_music music;
    auto apply_audio=[&]() {
        music.set_user_volume(fixed(music_volume)/10);
        music.set_muted(master_muted);
        bn::sound::set_master_volume(master_muted?fixed(0):fixed(sound_volume)/10);
        if(master_muted || sound_volume==0) {
            bn::sound::stop_all();
            engine.reset();engine_tick=0;engine_sustain=0;engine_volume=0;
        }
    };
    apply_audio();
    // Keep entity pools off the small IWRAM stack; allocation is once at boot.
    bn::unique_ptr<combat::World> combat_storage(new combat::World());
    auto& combat_world=*combat_storage;
    bn::unique_ptr<combat_view> combat_graphics;
    radio_signal::system radio_world;
    bn::unique_ptr<radio_signal_view> radio_graphics;
    bn::unique_ptr<decoration_view> decorations;
    bn::unique_ptr<town_scene> town;
    bn::unique_ptr<weapon_fitting_scene> fitting;
    auto equipped_tuning=[&]() {
        driving::Setup result=tuning;
        result.mass=bn::max(100,tuning.mass+battery_mass[battery_selection]);
        return result;
    };
    auto apply_battery=[&]() {
        combat_world.set_max_energy(battery_capacities[battery_selection]);
        car.mass=equipped_tuning().mass;
    };

    auto reset=[&](bool initialize_combat) {
        if(garage_shop::owns_upgrade(shop_owned,garage_shop::upgrade::tuned_injector))
            tuning.acceleration-=fixed(0.006);
        scrap=0;shop_notice=0;shop_owned=garage_shop::ownership();
        radio_world.reset(wasteland::layout().seed()^
                          (uint32_t(frame)+1)*0x9e3779b9u);
        combat_world.set_salvage_magnet(false);combat_world.set_reinforced_plating(false);
        car=driving::Car();
        car.x=world_map::start_x(); car.y=world_map::start_y();car.heading=world_map::start_heading();
        apply_battery();
        camera_lead_x=0;camera_lead_y=0;camera_x=car.x;camera_y=car.y-6;
        lap_frames=0; checkpoint=0; laps=0;
        notice_frames=180;wild_return_frames=0;race_result_frames=0;
        race_world_linger_checkpoint=-1;race_world_linger_frames=0;
        for(auto& mark:marks) { mark.life=0; mark.sprite.set_visible(false); }
        if(engine && engine->active()) engine->stop();
        engine.reset(); engine_tick=0;engine_sustain=0;engine_volume=0;
        ignored_town=-1; current_town=-1;ignored_gate=-1;current_gate=-1;portals_armed=true;
        if(initialize_combat)combat_world.reset(car,true,spawn_loading_update);
        mission_manager.reset(wasteland::layout().seed());
        race_manager.reset(wasteland::layout().seed());
        contract_open=false;contract_selection=0;race_open=false;race_selection=0;
        fitting.reset();
    };

    auto unload_scene=[&]() {
        town.reset();
        fitting.reset();
        settings_overview.reset();settings_tabs.clear();settings_goal_marker.set_visible(false);
        decorations.reset();
        radar.reset(); overlay.reset(); hud.reset(); track.reset();
        radio_graphics.reset();combat_graphics.reset(); combat_world.clear_bullets();
        car_sprite.set_visible(false);car_loadout_sprite.set_visible(false);
        minimap_dot.set_visible(false);mission_marker.set_visible(false);race_world_marker.set_visible(false);
        race_left_flag.set_visible(false);race_right_flag.set_visible(false);
        for(auto& marker:race_radar_markers)marker.set_visible(false);
        race_radar_markers.clear();
        for(auto& mark:marks) { mark.life=0; mark.sprite.set_visible(false); }
        hud_text.clear();
        shown_surface=-1;
        race_text.clear();
        if(engine && engine->active()) engine->stop();
        engine.reset(); engine_tick=0;engine_sustain=0;engine_volume=0;
        // Flush display-manager references before allocating another scene.
        bn::core::update();
    };
    auto create_scene=[&]() {
        track.reset(new terrain_streamer());
        hud=bn::regular_bg_items::hud_waste.create_bg(0,0);
        hud->set_priority(0);
        radar.reset(new local_minimap(car.x.integer(),car.y.integer()));
        track->set_camera(camera_x.integer(),camera_y.integer());
        track->set_visible(true);
        car_sprite.set_position(car.x-camera_x.integer(),car.y-camera_y.integer());
        car_loadout_sprite.set_position(car_sprite.position());
        car_sprite.set_bg_priority(1);car_sprite.set_z_order(-1);
        car_loadout_sprite.set_bg_priority(1);car_loadout_sprite.set_z_order(-2);
        car_sprite.set_visible(true);
        car_loadout_sprite.set_visible(combat_world.has_weapons());
        minimap_dot.set_visible(true);
        combat_graphics.reset(new combat_view());
        radio_world.load(world_map::index());
        radio_graphics.reset(new radio_signal_view());
        if(wasteland::has_decoration()) {
            decorations.reset(new decoration_view());
            decorations->update(camera_x.integer(),camera_y.integer(),true);
            bn::core::update(); // Populate the initial patch window during scene loading.
            bn::core::update();
        }
    };
    auto load_region=[&](int destination_map,int destination_spawn) {
        const int saved_hp=combat_world.player_hp,saved_shield=combat_world.player_shield;
        const int saved_energy=combat_world.player_energy,saved_shield_delay=combat_world.player_shield_delay;
        const int saved_invulnerability=combat_world.player_invulnerability;
        const auto saved_front=combat_world.fitted_weapon(combat::MountSlot::front);
        const auto saved_side=combat_world.fitted_weapon(combat::MountSlot::side);
        const auto saved_special=combat_world.fitted_weapon(combat::MountSlot::special);
        unload_scene();state=5;dustline_telemetry[2]=5;
        bn::bg_palettes::set_transparent_color(bn::color(2,4,5));
        loading_generator=&text;loading_sprites=&loading_text;
        loading_progress=0;loading_displayed=-1;loading_update(0);
        world_map::select(destination_map,generation_loading_update);loading_update(90);
        const auto arrival=world_map::player_spawn(destination_spawn);
        car=driving::Car();car.x=arrival.x;car.y=arrival.y;car.heading=arrival.heading;
        apply_battery();
        combat_world.reset(car,true,spawn_loading_update);
        combat_world.fit_weapon(combat::MountSlot::front,saved_front);
        combat_world.fit_weapon(combat::MountSlot::side,saved_side);
        combat_world.fit_weapon(combat::MountSlot::special,saved_special);
        combat_world.player_hp=bn::min(saved_hp,combat_world.max_player_hp());
        combat_world.player_shield=bn::min(saved_shield,combat::player_max_shield);
        combat_world.player_energy=bn::min(saved_energy,combat_world.max_player_energy());
        combat_world.player_shield_delay=saved_shield_delay;
        combat_world.player_invulnerability=saved_invulnerability;
        combat_world.player_destroyed=combat_world.player_hp<=0;
        mission_manager.on_map_loaded(destination_map,wasteland::layout(),combat_world.spawns);
        camera_lead_x=0;camera_lead_y=0;camera_x=car.x;camera_y=car.y-6;
        selected_map=destination_map;ignored_town=-1;current_town=-1;
        ignored_gate=-1;current_gate=-1;portals_armed=false;++gate_visits;
        loading_update(96);create_scene();loading_update(100);
        loading_text.clear();loading_generator=nullptr;loading_sprites=nullptr;
        state=1;button_guard=true;
        music.start();bn::sound_items::chime.play(fixed(0.45));
    };

    auto capture_save=[&]() {
        saves::game_state value;
        value.map_id=world_map::persistent_id(world_map::index());
        value.map_seed=wasteland::layout().seed();
        value.map_signature=wasteland::layout().signature();
        value.catalog_signature=world_map::catalog_signature();
        value.x=car.x.data();value.y=car.y.data();value.heading=car.heading.data();
        value.setup=uint8_t(setup);value.car_type=uint8_t(car_type);value.battery=uint8_t(battery_selection);
        value.acceleration=tuning.acceleration.data();value.max_speed=tuning.max_speed.data();
        value.grip=tuning.grip.data();value.steer=tuning.steer.data();
        value.coast_drag=tuning.coast_drag.data();value.brake_force=tuning.brake_force.data();value.mass=tuning.mass;
        value.music_volume=uint8_t(music_volume);value.sound_volume=uint8_t(sound_volume);
        value.master_muted=master_muted;
        for(int index=0;index<3;++index)value.best_laps[index]=best[index];
        value.laps=laps;value.scrap=scrap;value.duplicate_blueprints=0;
        value.blueprint_mask=0;value.crafted_mask=garage_shop::legacy_upgrade_mask(shop_owned);
        value.hp=combat_world.player_hp;value.shield=combat_world.player_shield;
        value.energy=combat_world.player_energy;
        value.front_weapon=uint8_t(combat_world.fitted_weapon(combat::MountSlot::front));
        value.side_weapon=uint8_t(combat_world.fitted_weapon(combat::MountSlot::side));
        value.special_weapon=uint8_t(combat_world.fitted_weapon(combat::MountSlot::special));
        const auto& contract=mission_manager.current();
        auto& saved=value.mission;
        saved.map_seed=int32_t(mission_manager.map_seed());
        saved.kind=int(contract.kind);saved.status=int(contract.state);
        saved.origin_map=contract.origin_map;saved.origin_town=contract.origin_town;
        saved.target_map=contract.target_map;saved.target_town=contract.target_town;
        saved.target_spawn=contract.target_spawn;saved.target_x=contract.target_x;saved.target_y=contract.target_y;
        saved.progress=contract.progress;saved.goal=contract.goal;saved.reward=contract.reward;
        saved.contract_serial=contract.serial;saved.credits=mission_manager.credits();
        saved.completed=mission_manager.completed();saved.serial=mission_manager.serial();
        value.race_map_seed=int32_t(race_manager.map_seed());value.race_serial=race_manager.serial();
        static_assert(garage_shop::ownership_word_count<=saves::shop_ownership_word_capacity,
                      "Shop ownership exceeds the save payload capacity");
        for(int index=0;index<garage_shop::ownership_word_count;++index)
            value.shop_owned[index]=shop_owned.words[index];
        value.shop_ownership_present=1;
        return value;
    };

    auto restore_save=[&](const saves::game_state& value) {
        const int destination_map=world_map::find_persistent_id(value.map_id);
        if(destination_map<0)return false;
        music.stop();unload_scene();state=5;dustline_telemetry[2]=5;
        bn::bg_palettes::set_transparent_color(bn::color(2,4,5));
        loading_generator=&text;loading_sprites=&loading_text;
        loading_progress=0;loading_displayed=-1;loading_update(0);
        world_map::select(destination_map,generation_loading_update);loading_update(90);
        const bool same_map=value.map_seed==wasteland::layout().seed() &&
                            value.map_signature==wasteland::layout().signature();
        const bool same_catalog=value.catalog_signature==world_map::catalog_signature();
        reset(false);
        setup=value.setup;car_type=value.car_type;battery_selection=value.battery;
        tuning=driving::setups[setup];
        tuning.acceleration=fixed::from_data(value.acceleration);
        tuning.max_speed=fixed::from_data(value.max_speed);tuning.grip=fixed::from_data(value.grip);
        tuning.steer=fixed::from_data(value.steer);tuning.coast_drag=fixed::from_data(value.coast_drag);
        tuning.brake_force=fixed::from_data(value.brake_force);tuning.mass=value.mass;
        music_volume=value.music_volume;sound_volume=value.sound_volume;master_muted=value.master_muted;
        for(int index=0;index<3;++index)best[index]=value.best_laps[index];
        laps=value.laps;scrap=value.scrap;shop_notice=0;
        if(value.shop_ownership_present) {
            shop_owned=garage_shop::ownership();
            for(int index=0;index<garage_shop::ownership_word_count;++index)
                shop_owned.words[index]=value.shop_owned[index];
        } else {
            // Version-one saves predate the direct-purchase shop. Preserve
            // crafted upgrades and every weapon that was already equipped.
            shop_owned=garage_shop::ownership();
            for(int index=0;index<3;++index)
                if(value.crafted_mask&(1u<<index)) {
                    for(int item=0;item<garage_shop::count;++item)
                        if(garage_shop::catalog[item].type==garage_shop::kind::upgrade &&
                           garage_shop::catalog[item].value==index)garage_shop::grant(shop_owned,item);
                }
            const combat::Weapon old_weapons[3]={combat::Weapon(value.front_weapon),
                                                  combat::Weapon(value.side_weapon),
                                                  combat::Weapon(value.special_weapon)};
            for(combat::Weapon weapon:old_weapons) {
                const int item=garage_shop::item_for_weapon(weapon);
                if(item>=0)garage_shop::grant(shop_owned,item);
            }
        }
        car=driving::Car();loaded_position_adjusted=false;
        const int saved_x=fixed::from_data(value.x).integer(),saved_y=fixed::from_data(value.y).integer();
        auto driveable=[](int x,int y) {
            return x>=8 && y>=8 && x<world_map::width()-8 && y<world_map::height()-8 &&
                   driving::can_drive(x,y);
        };
        int restored_x=saved_x,restored_y=saved_y;
        if(!driveable(restored_x,restored_y)) {
            loaded_position_adjusted=true;
            restored_x=-1;restored_y=-1;
            // A changed recipe can put an old save inside new collision. Keep
            // the player near that saved place instead of silently using start.
            for(int radius=8;radius<=512 && restored_x<0;radius+=8) {
                for(int offset=-radius;offset<=radius && restored_x<0;offset+=8) {
                    const int candidates[4][2]={{saved_x+offset,saved_y-radius},
                                                {saved_x+offset,saved_y+radius},
                                                {saved_x-radius,saved_y+offset},
                                                {saved_x+radius,saved_y+offset}};
                    for(const auto& candidate:candidates)if(driveable(candidate[0],candidate[1])) {
                        restored_x=candidate[0];restored_y=candidate[1];break;
                    }
                }
            }
            if(restored_x<0) {
                int nearest_distance=0x7fffffff;
                for(int y=64;y<world_map::height();y+=128)for(int x=64;x<world_map::width();x+=128)
                    if(driveable(x,y)) {
                        const int dx=x-saved_x,dy=y-saved_y,distance=dx*dx+dy*dy;
                        if(distance<nearest_distance) { nearest_distance=distance;restored_x=x;restored_y=y; }
                    }
            }
        }
        if(!loaded_position_adjusted) {
            car.x=fixed::from_data(value.x);car.y=fixed::from_data(value.y);
        } else {
            if(restored_x<0) { restored_x=world_map::start_x();restored_y=world_map::start_y(); }
            car.x=restored_x;car.y=restored_y;
        }
        car.heading=fixed::from_data(value.heading);
        apply_battery();
        combat_world.reset(car,true,spawn_loading_update);
        combat_world.set_salvage_magnet(garage_shop::owns_upgrade(shop_owned,garage_shop::upgrade::salvage_magnet));
        combat_world.set_reinforced_plating(garage_shop::owns_upgrade(shop_owned,garage_shop::upgrade::reinforced_plating));
        combat_world.fit_weapon(combat::MountSlot::front,combat::Weapon(value.front_weapon));
        combat_world.fit_weapon(combat::MountSlot::side,combat::Weapon(value.side_weapon));
        combat_world.fit_weapon(combat::MountSlot::special,combat::Weapon(value.special_weapon));
        combat_world.player_hp=bn::min(int(value.hp),combat_world.max_player_hp());
        combat_world.player_shield=bn::min(int(value.shield),combat::player_max_shield);
        combat_world.player_energy=bn::min(int(value.energy),combat_world.max_player_energy());
        combat_world.player_destroyed=combat_world.player_hp<=0;
        {
            missions::contract contract;
            const auto& saved=value.mission;
            if(same_catalog) {
                contract.kind=missions::type(saved.kind);contract.state=missions::status(saved.status);
                contract.origin_map=saved.origin_map;contract.origin_town=saved.origin_town;
                contract.target_map=saved.target_map;contract.target_town=saved.target_town;
                contract.target_spawn=saved.target_spawn;contract.target_x=saved.target_x;contract.target_y=saved.target_y;
                contract.progress=saved.progress;contract.goal=saved.goal;contract.reward=saved.reward;
                contract.serial=saved.contract_serial;
            }
            mission_manager.restore(same_catalog?uint32_t(saved.map_seed):wasteland::layout().seed(),
                                    contract,saved.credits,saved.completed,saved.serial);
            if(same_catalog)mission_manager.on_map_loaded(destination_map,wasteland::layout(),combat_world.spawns);
            race_manager.restore(same_catalog?uint32_t(value.race_map_seed):wasteland::layout().seed(),
                                 value.race_serial);
        }
        selected_map=destination_map;ignored_town=-1;current_town=-1;ignored_gate=-1;current_gate=-1;portals_armed=false;
        camera_lead_x=0;camera_lead_y=0;camera_x=car.x;camera_y=car.y-6;
        loading_update(96);create_scene();loading_update(100);
        loading_text.clear();loading_generator=nullptr;loading_sprites=nullptr;
        apply_audio();state=1;button_guard=true;loaded_exact_world=same_map&&same_catalog;
        music.start();bn::sound_items::chime.play(fixed(0.45));
        return true;
    };
    auto create_race_markers=[&]() {
        race_radar_markers.clear();
        for(int index=0;index<race_radar_marker_count;++index) {
            race_radar_markers.push_back(bn::sprite_items::mission_dot.create_sprite(0,0));
            race_radar_markers.back().set_bg_priority(0);
            race_radar_markers.back().set_z_order(-4);
            race_radar_markers.back().set_visible(false);
        }
    };
    auto prepare_race_offer=[&](races::kind kind) {
        bn::unique_ptr<races::scratch> work(new races::scratch());
        return race_manager.prepare_offer(kind,current_town,wasteland::layout(),wasteland::road(),*work).mode!=
               races::kind::none;
    };
    auto return_wild_race=[&]() {
        const int origin=race_manager.current().origin_town;
        unload_scene();state=4;current_town=origin;++town_visits;
        const auto return_point=wasteland::layout().town(origin);
        car=driving::Car();car.x=return_point.x;car.y=return_point.y;apply_battery();
        camera_lead_x=0;camera_lead_y=0;camera_x=car.x;camera_y=car.y-6;
        combat_world.refill_player();combat_world.clear_bullets();
        town.reset(new town_scene(current_town,setup,shop_owned));
        town->return_to_race_building();
        contract_open=false;race_open=true;button_guard=true;
        overlay=bn::regular_bg_items::town_dialog.create_bg(0,0);
        overlay->set_priority(0);
    };

    while(true) {
        ++frame;
        // Only the driving HUD has values that change without an input event.
        // Menu text is rebuilt on entry or selection changes, avoiding repeated
        // sprite allocation while a static screen is open.
        bool redraw=frame==1 || (state==1 && frame%6==0);
        if(!bn::keypad::a_held() && !bn::keypad::b_held() && !bn::keypad::start_held()) button_guard=false;
        if(state==0 && !title_continue_available)title_selection=0;
        if(state==0 && title_continue_available &&
           (bn::keypad::up_pressed() || bn::keypad::down_pressed())) {
            title_selection=1-title_selection;
            redraw=true;
        }
        if(state==0 && bn::keypad::a_pressed()) {
            if(title_selection==1 && title_continue_available) {
                saves::game_state loaded;
                if(!cartridge.load(loaded)) {
                    save_result=saves::result::no_save;title_selection=0;
                    title_continue_available=false;redraw=true;
                } else if(!restore_save(loaded)) {
                    save_result=saves::result::incompatible;title_selection=0;
                    title_continue_available=false;redraw=true;
                } else {
                    save_result=loaded_position_adjusted?saves::result::loaded_relocated:
                                loaded_exact_world?saves::result::loaded:saves::result::loaded_updated;
                    load_notice_frames=180;redraw=true;
                }
            } else {
                music.stop();
                unload_scene();
                state=5; dustline_telemetry[2]=5;
                bn::bg_palettes::set_transparent_color(bn::color(2,4,5));
                loading_generator=&text;loading_sprites=&loading_text;
                loading_progress=0;loading_displayed=-1;
                loading_update(0);
                selected_map=world_map::start_map();
                world_map::select(selected_map,generation_loading_update);
                loading_update(90);
                reset(true);
                setup=1;car_type=0;battery_selection=1;tuning=driving::setups[setup];
                for(int& value:best)value=0;
                apply_battery();
                loading_update(95);redraw=true;
                loading_update(96);create_scene();loading_update(100);
                loading_text.clear();loading_generator=nullptr;loading_sprites=nullptr;
                state=1;
                save_result=saves::result::none;
                music.start();
                bn::sound_items::chime.play(fixed(0.45));
            }
        } else if((state==1 || state==2) && bn::keypad::start_pressed()) {
            state=state==1?2:1;
            if(state==2) {
                // Free the live driving glyphs before building the pause page.
                // Race HUD text otherwise overlaps the pause allocation in the
                // GBA's 128-entry sprite pool.
                hud_text.clear();race_text.clear();shown_surface=-1;
                bn::core::update();
                overlay=bn::regular_bg_items::pause_waste.create_bg(0,0);
                overlay->set_priority(0);
            } else overlay.reset();
            hud->set_visible(state==1);
            car_sprite.set_visible(state==1);
            car_loadout_sprite.set_visible(state==1 && combat_world.has_weapons());
            minimap_dot.set_visible(state==1);
            mission_marker.set_visible(false);
            race_world_marker.set_visible(false);race_left_flag.set_visible(false);race_right_flag.set_visible(false);
            for(auto& marker:race_radar_markers)marker.set_visible(false);
            if(radar) radar->set_visible(state==1);
            if(engine && engine->active()) engine->stop();
            engine.reset(); engine_tick=0;engine_sustain=0;engine_volume=0;
            hud_text.clear(); redraw=true;
        }

        if(state==2 && bn::keypad::select_pressed()) {
            music.stop();
            state=0;
            title_selection=0;selected_map=world_map::start_map();
            title_continue_available=compatible_save_available();
            unload_scene();
            mission_manager.reset(0);
            race_manager.reset(0);
            overlay=bn::regular_bg_items::title.create_bg(0,0);
            overlay->set_priority(0);
            hud_text.clear(); redraw=true;
        }
        if(state==2 && bn::keypad::b_pressed() && race_manager.active()) {
            const bool wild=race_manager.current().mode==races::kind::overworld;
            race_manager.abort(races::outcome::aborted);
            race_result_frames=180;
            if(wild)return_wild_race();
            else {
                state=1;overlay.reset();hud->set_visible(true);car_sprite.set_visible(true);
                car_loadout_sprite.set_visible(combat_world.has_weapons());radar->set_visible(true);
                minimap_dot.set_visible(true);button_guard=true;
            }
            hud_text.clear();redraw=true;
        }
        if((state==1 || state==4) && bn::keypad::select_pressed()) {
            settings_return=state; state=6;
            settings_panel=settings_map_panel;map_selection=0;
            tuning_repeat=0;tuning_direction=0;erase_confirm=false;
            if(engine && engine->active()) engine->stop();
            engine.reset(); engine_tick=0;engine_sustain=0;engine_volume=0;
            if(hud) hud->set_visible(false);
            car_sprite.set_visible(false);car_loadout_sprite.set_visible(false);
            minimap_dot.set_visible(false);mission_marker.set_visible(false);race_world_marker.set_visible(false);
            race_left_flag.set_visible(false);race_right_flag.set_visible(false);
            for(auto& marker:race_radar_markers)marker.set_visible(false);
            race_radar_markers.clear();
            if(radar) radar->set_visible(false);
            if(town) town->set_visible(false);
            overlay.reset();hud_text.clear();
            shown_surface=-1;
            settings_overview.reset(new world_overview(car.x.integer(),car.y.integer()));settings_tabs.clear();
            settings_overview->set_highlight(car.x.integer(),car.y.integer());
            for(int index=0;index<settings_panel_count;++index) {
                auto tab=bn::sprite_items::settings_tab.create_sprite(-109,-48+index*32,index*2+(index==settings_panel));
                tab.set_bg_priority(0);tab.set_z_order(-5);settings_tabs.push_back(tab);
            }
            redraw=true;
        } else if(state==6) {
            if(bn::keypad::l_pressed() || bn::keypad::r_pressed()) {
                settings_panel=(settings_panel+(bn::keypad::l_pressed()?settings_panel_count-1:1))%
                               settings_panel_count;
                tuning_repeat=0;tuning_direction=0;erase_confirm=false;redraw=true;
                if(settings_panel==settings_map_panel)map_selection=0;
                settings_overview->show_map(settings_panel==settings_map_panel);
                if(settings_panel==settings_map_panel)
                    settings_overview->set_highlight(car.x.integer(),car.y.integer());
                for(int index=0;index<settings_tabs.size();++index)
                    settings_tabs[index].set_tiles(bn::sprite_items::settings_tab.tiles_item(),index*2+(index==settings_panel));
            }
            if(settings_panel==settings_map_panel) {
                const int map_selection_count=1+cave_layout::town_count+world_map::portal_count();
                if(bn::keypad::left_pressed() || bn::keypad::right_pressed()) {
                    map_selection=(map_selection+map_selection_count+
                                  (bn::keypad::left_pressed()?-1:1))%map_selection_count;
                    int selected_x=car.x.integer(),selected_y=car.y.integer();
                    if(map_selection>0 && map_selection<=cave_layout::town_count) {
                        const auto selected_town=wasteland::layout().town(map_selection-1);
                        selected_x=selected_town.x;selected_y=selected_town.y;
                    } else if(map_selection>cave_layout::town_count) {
                        const auto selected_portal=world_map::portal(map_selection-1-cave_layout::town_count);
                        selected_x=selected_portal.x;selected_y=selected_portal.y;
                    }
                    settings_overview->set_highlight(selected_x,selected_y);
                    redraw=true;
                }
            } else if(settings_panel==settings_handling_panel) {
                if(bn::keypad::up_pressed()) {
                    tuning_selection=(tuning_selection+handling_row_count-1)%handling_row_count;redraw=true;
                } else if(bn::keypad::down_pressed()) {
                    tuning_selection=(tuning_selection+1)%handling_row_count;redraw=true;
                }
                const int direction=(bn::keypad::right_held()?1:0)-(bn::keypad::left_held()?1:0);
                if(tuning_selection==handling_car_row) {
                    if(bn::keypad::left_pressed() || bn::keypad::right_pressed()) {
                        car_type=(car_type+vehicle_type_count+direction)%vehicle_type_count;redraw=true;
                    }
                    tuning_direction=0;tuning_repeat=0;
                } else if(tuning_selection==handling_battery_row) {
                    if(bn::keypad::left_pressed() || bn::keypad::right_pressed()) {
                        battery_selection=(battery_selection+battery_count+direction)%battery_count;
                        apply_battery();redraw=true;
                    }
                    tuning_direction=0;tuning_repeat=0;
                } else if(direction) {
                    if(direction!=tuning_direction) { tuning_direction=direction;tuning_repeat=0; }
                    const bool repeat=tuning_repeat>=18 && (tuning_repeat-18)%2==0;
                    if(bn::keypad::left_pressed() || bn::keypad::right_pressed() || repeat) {
                        adjust_tuning(tuning,tuning_selection,direction);redraw=true;
                    }
                    ++tuning_repeat;
                } else { tuning_direction=0;tuning_repeat=0; }
                if(bn::keypad::a_pressed()) {
                    if(tuning_selection==handling_car_row)car_type=0;
                    else if(tuning_selection==handling_battery_row) {
                        battery_selection=1;apply_battery();
                    }
                    else {
                        tuning=driving::setups[setup];
                        if(garage_shop::owns_upgrade(shop_owned,garage_shop::upgrade::tuned_injector))
                            tuning.acceleration+=fixed(0.006);
                    }
                    redraw=true;
                }
            } else if(settings_panel==settings_audio_panel) {
                if(bn::keypad::up_pressed()) {
                    audio_selection=(audio_selection+audio_row_count-1)%audio_row_count;redraw=true;
                } else if(bn::keypad::down_pressed()) {
                    audio_selection=(audio_selection+1)%audio_row_count;redraw=true;
                }
                bool changed=false;
                const int direction=(bn::keypad::right_pressed()?1:0)-(bn::keypad::left_pressed()?1:0);
                if(audio_selection==0 && direction) {
                    music_volume=bn::max(0,bn::min(10,music_volume+direction));changed=true;
                } else if(audio_selection==1 && direction) {
                    sound_volume=bn::max(0,bn::min(10,sound_volume+direction));changed=true;
                } else if(audio_selection==2 && (direction || bn::keypad::a_pressed())) {
                    master_muted=!master_muted;changed=true;
                } else if(bn::keypad::a_pressed()) {
                    if(audio_selection==0)music_volume=10;
                    else sound_volume=10;
                    changed=true;
                }
                if(changed) { apply_audio();redraw=true; }
            } else if(settings_panel==settings_save_panel) {
                if(bn::keypad::up_pressed()) {
                    save_selection=(save_selection+save_row_count-1)%save_row_count;
                    erase_confirm=false;redraw=true;
                } else if(bn::keypad::down_pressed()) {
                    save_selection=(save_selection+1)%save_row_count;
                    erase_confirm=false;redraw=true;
                }
                if(bn::keypad::a_pressed()) {
                    if(save_selection==0) {
                        save_result=cartridge.save(capture_save())?saves::result::saved:saves::result::invalid;
                        erase_confirm=false;redraw=true;
                    } else if(save_selection==1) {
                        saves::game_state loaded;
                        if(!cartridge.load(loaded)) { save_result=saves::result::no_save;redraw=true; }
                        else if(!restore_save(loaded)) { save_result=saves::result::incompatible;redraw=true; }
                        else {
                            save_result=loaded_position_adjusted?saves::result::loaded_relocated:
                                        loaded_exact_world?saves::result::loaded:saves::result::loaded_updated;
                            load_notice_frames=180;redraw=true;
                        }
                    } else if(!erase_confirm) {
                        erase_confirm=true;save_result=saves::result::none;redraw=true;
                    } else {
                        cartridge.erase();erase_confirm=false;save_result=saves::result::erased;redraw=true;
                    }
                }
            }
            if(bn::keypad::b_pressed() || bn::keypad::select_pressed() || bn::keypad::start_pressed()) {
                erase_confirm=false;
                hud_text.clear();
                apply_battery();
                settings_overview.reset();settings_tabs.clear();settings_goal_marker.set_visible(false);
                state=settings_return; button_guard=true; redraw=true;
                if(state==1) {
                    overlay.reset(); hud->set_visible(true); car_sprite.set_visible(true);
                    car_loadout_sprite.set_visible(combat_world.has_weapons());
                    radar->set_visible(true); minimap_dot.set_visible(true);
                    if(race_manager.active())create_race_markers();
                } else if(state==4 && town) {
                    overlay.reset();
                    if(town->menu_open() || contract_open || race_open) {
                        overlay=bn::regular_bg_items::town_dialog.create_bg(0,0);
                        overlay->set_priority(0);
                    }
                    town->set_visible(true);
                }
            }
        }
        if(state==8 && fitting && !button_guard) {
            const auto fitting_event=fitting->update(combat_world);
            if(fitting_event==weapon_fitting_scene::event::close) {
                hud_text.clear();fitting.reset();bn::core::update();
                town->resume();state=4;button_guard=true;redraw=true;
            } else if(fitting_event==weapon_fitting_scene::event::fitted) {
                bn::sound_items::chime.play(fixed(0.32));redraw=true;
            } else if(fitting_event==weapon_fitting_scene::event::redraw)redraw=true;
        }
        if(state==7) {
            if(bn::keypad::a_pressed()) {
                combat_world.revive_player();
                overlay.reset();hud_text.clear();state=1;button_guard=true;redraw=true;
                radar->set_visible(true);minimap_dot.set_visible(true);
                bn::sound_items::chime.play(fixed(0.4));
            }
        } else if(state==3) {
            if(bn::keypad::up_pressed() || bn::keypad::down_pressed() ||
               bn::keypad::left_pressed() || bn::keypad::right_pressed()) { town_yes=!town_yes; redraw=true; }
            if(bn::keypad::b_pressed() || (bn::keypad::a_pressed() && !town_yes)) {
                ignored_town=current_town; state=1; overlay.reset();
                hud_text.clear(); button_guard=true; redraw=true;
            } else if(bn::keypad::a_pressed() && town_yes) {
                combat_world.refill_player();
                bool completed=!race_manager.session() && mission_manager.on_town_enter(world_map::index(),current_town);
                unload_scene(); state=4; ++town_visits;
                town.reset(new town_scene(current_town,setup,shop_owned));
                if(completed)bn::sound_items::chime.play(fixed(0.55));
                redraw=true;
            }
        } else if(state==9) {
            if(bn::keypad::up_pressed() || bn::keypad::down_pressed() ||
               bn::keypad::left_pressed() || bn::keypad::right_pressed()) { gate_yes=!gate_yes;redraw=true; }
            if(bn::keypad::b_pressed() || (bn::keypad::a_pressed() && !gate_yes)) {
                ignored_gate=current_gate;state=1;overlay.reset();hud_text.clear();button_guard=true;redraw=true;
            } else if(bn::keypad::a_pressed() && gate_yes) {
                const auto transition=world_map::portal(current_gate);
                if(transition.destination_map>=0) {
                    music.stop();load_region(transition.destination_map,transition.destination_spawn);redraw=true;
                }
            }
        } else if(state==4 && town && !button_guard) {
            if(race_open) {
                const auto race_state=race_manager.current().state;
                if(bn::keypad::b_pressed()) {
                    race_open=false;overlay.reset();hud_text.clear();redraw=true;
                } else if(race_state==races::phase::none &&
                          (bn::keypad::left_pressed() || bn::keypad::right_pressed())) {
                    race_selection=1-race_selection;
                    prepare_race_offer(race_selection?races::kind::overworld:races::kind::town);
                    redraw=true;
                } else if(bn::keypad::a_pressed()) {
                    if(race_state==races::phase::none) {
                        if(race_manager.offered().mode==races::kind::none)
                            prepare_race_offer(race_selection?races::kind::overworld:races::kind::town);
                        if(race_manager.accept()) {
                            const auto& accepted=race_manager.current();
                            race_world_linger_checkpoint=-1;race_world_linger_frames=0;
                            bn::sound_items::chime.play(fixed(0.45));
                            if(accepted.mode==races::kind::overworld) {
                                const auto start=accepted.start;
                                overlay.reset();hud_text.clear();town.reset();bn::core::update();
                                car=driving::Car();car.x=start.x;car.y=start.y;apply_battery();
                                camera_lead_x=0;camera_lead_y=0;camera_x=car.x;camera_y=car.y-6;
                                combat_world.clear_bullets();create_scene();create_race_markers();
                                ignored_town=-1;race_open=false;state=1;button_guard=true;
                                race_manager.start();
                            } else {
                                race_open=false;overlay.reset();button_guard=true;
                            }
                        }
                        redraw=true;
                    } else if(race_state==races::phase::result) {
                        race_manager.acknowledge();race_result_frames=0;
                        prepare_race_offer(race_selection?races::kind::overworld:races::kind::town);
                        redraw=true;
                    }
                }
            } else if(contract_open) {
                const auto mission_status=mission_manager.current().state;
                if(bn::keypad::b_pressed()) {
                    contract_open=false;overlay.reset();hud_text.clear();redraw=true;
                } else if(mission_status==missions::status::none &&
                          (bn::keypad::left_pressed() || bn::keypad::right_pressed())) {
                    contract_selection=1-contract_selection;redraw=true;
                } else if(bn::keypad::a_pressed()) {
                    if(mission_status==missions::status::none) {
                        auto kind=contract_selection?missions::type::extermination:missions::type::courier;
                        if(mission_manager.accept(kind,world_map::index(),current_town,wasteland::layout(),combat_world.spawns))
                            bn::sound_items::chime.play(fixed(0.45));
                        redraw=true;
                    } else if(mission_status==missions::status::complete) {
                        mission_manager.acknowledge();redraw=true;
                    }
                }
            } else {
                auto town_event=town->update(setup);
                if(town_event==town_scene::event::return_to_world) {
                    overlay.reset();hud_text.clear();town.reset();bn::core::update();
                    car.heading+=180;
                    if(car.heading>=360)car.heading-=360;
                    camera_lead_x=0;camera_lead_y=0;camera_x=car.x;camera_y=car.y-6;
                    create_scene();ignored_town=current_town;state=1;button_guard=true;redraw=true;
                    if(race_manager.current().state==races::phase::ready &&
                       race_manager.current().mode==races::kind::town) {
                        create_race_markers();race_manager.start();
                    }
                } else if(town_event==town_scene::event::menu_opened ||
                          town_event==town_scene::event::contract_opened ||
                          town_event==town_scene::event::race_opened) {
                    contract_open=town_event==town_scene::event::contract_opened;
                    race_open=town_event==town_scene::event::race_opened;
                    if(race_open)
                        prepare_race_offer(race_selection?races::kind::overworld:races::kind::town);
                    overlay=bn::regular_bg_items::town_dialog.create_bg(0,0);
                    overlay->set_priority(0);redraw=true;
                } else if(town_event==town_scene::event::weapon_fitting_opened) {
                    overlay.reset();hud_text.clear();town->suspend();bn::core::update();
                    fitting.reset(new weapon_fitting_scene(combat_world,car_type,
                                                           garage_shop::weapon_mask(shop_owned)));
                    state=8;button_guard=true;redraw=true;
                } else if(town_event==town_scene::event::shop_purchase_requested) {
                    const int id=town->shop_item();
                    BN_ASSERT(id>=0,"Shop purchase requested without a visible item");
                    const auto& item=garage_shop::catalog[id];
                    if(garage_shop::owned(shop_owned,id)) {
                        shop_notice=2;bn::sound_items::bump.play(fixed(0.2));
                    } else if(scrap>=item.scrap && mission_manager.spend_credits(item.credits)) {
                        scrap-=item.scrap;garage_shop::grant(shop_owned,id);shop_notice=1;
                        if(item.type==garage_shop::kind::upgrade) {
                            const auto effect=garage_shop::upgrade(item.value);
                            if(effect==garage_shop::upgrade::salvage_magnet)combat_world.set_salvage_magnet(true);
                            else if(effect==garage_shop::upgrade::tuned_injector)tuning.acceleration+=fixed(0.006);
                            else if(effect==garage_shop::upgrade::reinforced_plating)combat_world.set_reinforced_plating(true);
                        }
                        town->refresh_shop(shop_owned);
                        bn::sound_items::chime.play(fixed(0.5));
                    } else {
                        shop_notice=3;bn::sound_items::bump.play(fixed(0.2));
                    }
                    redraw=true;
                } else if(town_event==town_scene::event::menu_closed ||
                           town_event==town_scene::event::setup_applied) {
                    overlay.reset();town->set_visible(true);redraw=true;
                    if(town_event==town_scene::event::setup_applied) {
                        tuning=driving::setups[setup];
                        if(garage_shop::owns_upgrade(shop_owned,garage_shop::upgrade::tuned_injector))
                            tuning.acceleration+=fixed(0.006);
                        apply_battery();
                        bn::sound_items::chime.play(fixed(0.4));
                    }
                } else if(town_event==town_scene::event::redraw) {
                    redraw=true;
                }
            }
        }
        if(state==1) {
            const bool race_locked=race_manager.current().state==races::phase::countdown ||
                                   wild_return_frames>0;
            driving::Input input {bn::keypad::a_held(),bn::keypad::b_held(),
                (bn::keypad::right_held()?1:0)-(bn::keypad::left_held()?1:0)};
            if(!button_guard) {
                if(race_locked) {
                    input={false,false,0};car.vx=0;car.vy=0;car.yaw=0;car.speed=0;car.slip=0;
                } else car.step(input,equipped_tuning());
                ++lap_frames;
                int combat_start=bn::core::current_cpu_ticks();
                if(!race_locked) {
                    combat_world.step(car,bn::keypad::r_held(),bn::keypad::l_held());
                    const bool radio_fitted=combat_world.fitted_weapon(combat::MountSlot::special)==
                                             combat::Weapon::radio;
                    const int radio_reward=radio_world.step(car,radio_fitted,combat_world.bullets);
                    if(radio_reward) {
                        scrap+=radio_reward;
                        bn::sound_items::chime.play(fixed(0.5));redraw=true;
                    }
                }
                if(combat_world.collected_scrap){scrap+=combat_world.collected_scrap;redraw=true;}
                if(combat_world.collected_energy)bn::sound_items::chime.play(fixed(0.22));
                bool contract_completed=false;
                if(!race_manager.session())
                    for(int i=0;i<combat_world.destroyed_count;++i)
                        contract_completed|=mission_manager.on_enemy_destroyed(world_map::index(),combat_world.destroyed_spawns[i]);
                dustline_combat_telemetry[18]=bn::core::current_cpu_ticks()-combat_start;
                if(combat_world.fired) bn::sound_items::gun.play(fixed(0.28));
                if(contract_completed)bn::sound_items::chime.play(fixed(0.55));
                else if(combat_world.destroyed) bn::sound_items::bump.play(fixed(0.6));
                else if(combat_world.impact) bn::sound_items::bump.play(fixed(0.18));
                const int previous_race_checkpoint=race_manager.current().next_checkpoint;
                if(race_manager.update(car.x.integer(),car.y.integer())) {
                    const int award=race_manager.take_award();
                    mission_manager.award_credits(award);
                    bn::sound_items::chime.play(award?fixed(0.6):fixed(0.3));
                    race_result_frames=180;
                    if(race_manager.current().mode==races::kind::overworld)wild_return_frames=90;
                    redraw=true;
                }
                if(race_manager.current().next_checkpoint>previous_race_checkpoint) {
                    race_world_linger_checkpoint=previous_race_checkpoint;
                    race_world_linger_frames=race_world_marker_linger_frames;
                }
            }
            if(notice_frames>0)--notice_frames;
            if(load_notice_frames>0)--load_notice_frames;
            if(race_result_frames>0)--race_result_frames;
            // Anchor to the car and smooth only the velocity-driven look-ahead.
            // This keeps the car stable while opening more screen in the actual
            // direction of travel, including reverse and controlled slides.
            fixed desired_lead_x=clamp(car.vx*camera_lead_x_scale,-camera_lead_x_limit,camera_lead_x_limit);
            fixed desired_lead_y=clamp(car.vy*camera_lead_y_scale,-camera_lead_y_limit,camera_lead_y_limit);
            camera_lead_x+=(desired_lead_x-camera_lead_x)*camera_lead_response;
            camera_lead_y+=(desired_lead_y-camera_lead_y)*camera_lead_response;
            // Rough surfaces move the camera by about a pixel at speed. The car
            // simulation supplies the deterministic signal so this remains in
            // step with the small lateral material impulses.
            camera_x=clamp(car.x+camera_lead_x+car.terrain_rumble*fixed(0.35),120,world_map::width()-120);
            camera_y=clamp(car.y+camera_lead_y-6+car.terrain_rumble*fixed(1.10),80,world_map::height()-80);
            track->set_camera(camera_x.integer(),camera_y.integer());
            car_sprite.set_position(car.x-camera_x.integer(),car.y-camera_y.integer());
            car_loadout_sprite.set_position(car_sprite.position());
            int direction=((car.heading*64/360).integer()+64)%64;
            switch(car_type) {
            case 1:car_sprite.set_tiles(bn::sprite_items::car_sand_buggy.tiles_item(),direction);break;
            case 2:car_sprite.set_tiles(bn::sprite_items::car_old.tiles_item(),direction);break;
            case 3:car_sprite.set_tiles(bn::sprite_items::car_truck.tiles_item(),direction);break;
            case 4:car_sprite.set_tiles(bn::sprite_items::car_pickup.tiles_item(),direction);break;
            default:car_sprite.set_tiles(bn::sprite_items::car.tiles_item(),direction);break;
            }
            car_loadout_sprite.set_tiles(bn::sprite_items::car_loadout.tiles_item(),
                                         combat_world.weapon_mask()*64+direction);
            radar->set_vitals(combat_world.player_hp,combat_world.max_player_hp(),
                              combat_world.player_shield,combat::player_max_shield,
                              combat_world.player_energy,combat_world.max_player_energy());
            radar->update(car.x.integer(),car.y.integer());
            int marker_x=radar->project_x(car.x.integer());
            int marker_y=radar->project_y(car.y.integer());
            minimap_dot.set_position(local_minimap::screen_x+marker_x,local_minimap::screen_y+marker_y);
            bool sliding=car.slip>fixed(0.28) && car.speed>fixed(0.8);
            if(frame%4==0 && (sliding || (car.surface!=1 && car.speed>fixed(0.6)))) {
                Mark& mark=marks[next_mark]; next_mark=(next_mark+1)%marks.size();
                mark.x=car.x-bn::degrees_lut_cos(car.heading)*9;
                mark.y=car.y-bn::degrees_lut_sin(car.heading)*9;
                mark.life=car.surface==1?96:40;
                mark.sprite.set_tiles(bn::sprite_items::particles.tiles_item(),car.surface==1?0:1);
                mark.sprite.set_bg_priority(2);
            }
            if(car.hit) bn::sound_items::bump.play(fixed(0.5));
            if(sliding && frame%14==0) bn::sound_items::skid.play(fixed(0.13));
            if(input.throttle==engine_throttle)engine_sustain=bn::min(480,engine_sustain+1);
            else { engine_throttle=input.throttle;engine_sustain=0;engine_tick=0; }
            if(master_muted || sound_volume==0) {
                if(engine && engine->active())engine->stop();
                engine.reset();engine_tick=15;engine_volume=0;
            } else if(--engine_tick<=0) {
                const fixed speed_ratio=clamp(car.speed/tuning.max_speed,0,1);
                const fixed pitch=fixed(0.72)+speed_ratio*fixed(0.52)+
                                  (input.throttle?fixed(0.06):fixed(0));
                const int settled_frames=bn::max(0,engine_sustain-90);
                const fixed settled_gain=bn::max(fixed(0.25),fixed(1)-fixed(settled_frames)/520);
                const fixed base_volume=input.throttle?fixed(0.13):fixed(0.045);
                if(engine && engine->active())engine->stop();
                engine_volume=base_volume*settled_gain;engine_pitch=pitch;
                engine=bn::sound_items::engine.play(engine_volume,pitch,0);
                engine_tick=bn::max(18,(fixed(31)/pitch).integer());
            }
            for(auto& mark:marks) {
                if(mark.life>0) --mark.life;
                fixed sx=mark.x-camera_x,sy=mark.y-camera_y;
                bool visible=mark.life>0 && sx>-125 && sx<125 && sy>-85 && sy<85;
                mark.sprite.set_visible(visible);
                if(visible) mark.sprite.set_position(sx,sy);
            }
            shown_surface=world_map::material_at(car.x.integer(),car.y.integer());
            const auto& layout=wasteland::layout();
            int near=layout.nearby_town(car.x.integer(),car.y.integer());
            // Declining or returning suppresses the prompt only while the car
            // remains inside that exact radial zone. Crossing its edge rearms
            // the settlement immediately for the next approach.
            if(ignored_town>=0 && near!=ignored_town)ignored_town=-1;
            if(!combat_world.player_destroyed && !race_manager.active() && near>=0 && near!=ignored_town) {
                state=3; current_town=near; town_yes=false;
                car.vx=0; car.vy=0; car.yaw=0; car.speed=0; car.slip=0;
                if(engine && engine->active()) engine->stop();
                engine.reset(); engine_tick=0;engine_sustain=0;engine_volume=0;
                hud_text.clear();
                radar->set_visible(false); minimap_dot.set_visible(false);
                mission_marker.set_visible(false);
                if(combat_graphics) combat_graphics->update(combat_world,camera_x.integer(),camera_y.integer(),false);
                // Finish the driving frame before allocating dialog art/text.
                // Combat is already paused; this is a scene/UI transition.
                bn::core::update();
                missed+=bn::core::last_missed_frames();
                overlay=bn::regular_bg_items::town_dialog.create_bg(0,0);
                overlay->set_priority(0); hud_text.clear(); redraw=true;
                radar->set_visible(false);
                minimap_dot.set_visible(false);
            }
            if(state==1) {
                const int near_gate=world_map::nearby_portal(car.x.integer(),car.y.integer());
                if(!portals_armed && near_gate<0)portals_armed=true;
                if(ignored_gate>=0 && near_gate!=ignored_gate)ignored_gate=-1;
                if(portals_armed && !combat_world.player_destroyed && !race_manager.active() && near_gate>=0 && near_gate!=ignored_gate) {
                    state=9;current_gate=near_gate;gate_yes=false;
                    car.vx=0;car.vy=0;car.yaw=0;car.speed=0;car.slip=0;
                    if(engine && engine->active())engine->stop();
                    engine.reset();engine_tick=0;engine_sustain=0;engine_volume=0;
                    hud_text.clear();radar->set_visible(false);minimap_dot.set_visible(false);
                    mission_marker.set_visible(false);
                    if(combat_graphics)combat_graphics->update(combat_world,camera_x.integer(),camera_y.integer(),false);
                    bn::core::update();missed+=bn::core::last_missed_frames();
                    overlay=bn::regular_bg_items::town_dialog.create_bg(0,0);
                    overlay->set_priority(0);redraw=true;
                }
            }
            if(state==1) { radar->set_visible(true); minimap_dot.set_visible(true); }
            if(state==1 && combat_world.player_destroyed) {
                const bool wild_race=race_manager.active() &&
                                     race_manager.current().mode==races::kind::overworld;
                if(race_manager.active())race_manager.abort(races::outcome::wrecked);
                race_result_frames=180;
                if(wild_race) {
                    wild_return_frames=1;
                    combat_world.revive_player();
                } else {
                state=7;car.vx=0;car.vy=0;car.yaw=0;car.speed=0;car.slip=0;
                if(engine && engine->active())engine->stop();
                engine.reset();engine_tick=0;engine_sustain=0;engine_volume=0;hud_text.clear();
                radar->set_visible(false);minimap_dot.set_visible(false);
                mission_marker.set_visible(false);
                bn::core::update();missed+=bn::core::last_missed_frames();
                overlay=bn::regular_bg_items::town_dialog.create_bg(0,0);
                overlay->set_priority(0);redraw=true;
                }
            }
            if(state==1 && wild_return_frames>0 && --wild_return_frames==0) {
                return_wild_race();redraw=true;
            }
        } else {
            for(auto& mark:marks) mark.sprite.set_visible(false);
            if(state==0 && redraw) {
                hud_text.clear();
                text.set_center_alignment();
                text.generate(0,48,title_selection==0?"> START NEW GAME":"  START NEW GAME",hud_text);
                if(title_continue_available)
                    text.generate(0,64,title_selection==1?"> CONTINUE GAME":"  CONTINUE GAME",hud_text);
                if(save_result==saves::result::incompatible)
                    text.generate(0,78,"SAVE MAP NOT FOUND",hud_text);
                else if(save_result==saves::result::no_save)
                    text.generate(0,78,"NO SAVE DATA",hud_text);
                text.set_left_alignment();
            }
        }
        if(state==3 && redraw) {
            hud_text.clear();
            bn::string<32> line="ENTER OUTPOST ";
            line+=bn::to_string<3>(current_town+1); line+='?';
            text.generate(-104,38,line,hud_text);
            text.generate(-104,52,town_yes?"> YES     NO":"  YES   > NO",hud_text);
            text.generate(-104,68,"A CONFIRM  B CANCEL",hud_text);
        }
        if(state==9 && redraw) {
            hud_text.clear();const auto transition=world_map::portal(current_gate);
            text.generate(-104,30,"TRAVEL TO",hud_text);
            bn::string<32> line=world_map::name(transition.destination_map);line+='?';
            text.generate(-104,42,line,hud_text);
            text.generate(-104,56,gate_yes?"> YES     NO":"  YES   > NO",hud_text);
            text.generate(-104,70,"A CONFIRM  B CANCEL",hud_text);
        }
        if(state==4 && redraw) {
            hud_text.clear();
            if(town && race_open) {
                const auto& active=race_manager.current();
                if(active.state==races::phase::none) {
                    const auto& offered=race_manager.offered();
                    text.generate(-104,32,"RACE OFFICE",hud_text);
                    bn::string<32> line="<  ";
                    line+=races::kind_name(race_selection?races::kind::overworld:races::kind::town);line+="  >";
                    text.generate(-104,44,line,hud_text);
                    if(offered.mode==races::kind::town) {
                        line="TO OUTPOST ";line+=bn::to_string<3>(offered.target_town+1);
                    } else if(offered.mode==races::kind::overworld) {
                        line=offered.closed?"CLOSED WILDERNESS LOOP":"OPEN WILDERNESS COURSE";
                    } else line="NO COURSE AVAILABLE";
                    text.generate(-104,56,line,hud_text);
                    if(offered.mode!=races::kind::none) {
                        line=bn::to_string<3>(offered.checkpoint_count);line+=" GATES  ";
                        line+=bn::to_string<4>((offered.time_limit+59)/60);line+=" SEC";
                        text.generate(-104,67,line,hud_text);
                        line="PRIZE ";line+=bn::to_string<6>(offered.reward);line+="+  A START";
                        text.generate(-104,74,line,hud_text);
                    }
                } else if(active.state==races::phase::result) {
                    bn::string<32> line=races::outcome_name(active.result);
                    text.generate(-104,38,line,hud_text);
                    if(active.result==races::outcome::complete) {
                        line="SCORE ";line+=bn::to_string<5>(active.score);
                        text.generate(-104,51,line,hud_text);
                        line="EARNED ";line+=bn::to_string<7>(active.earned);line+=" CR";
                        text.generate(-104,63,line,hud_text);
                    } else text.generate(-104,54,"NO PAYMENT",hud_text);
                    text.generate(-104,72,"A NEW RACES   B CLOSE",hud_text);
                } else {
                    bn::string<32> line="ACTIVE ";line+=races::kind_name(active.mode);
                    text.generate(-104,42,line,hud_text);
                    line=bn::to_string<3>(active.next_checkpoint);line+='/';
                    line+=bn::to_string<3>(active.checkpoint_count);line+=" GATES";
                    text.generate(-104,56,line,hud_text);
                    text.generate(-104,72,"B CLOSE",hud_text);
                }
            } else if(town && contract_open) {
                const auto& active=mission_manager.current();
                if(active.state==missions::status::none) {
                    auto kind=contract_selection?missions::type::extermination:missions::type::courier;
                    auto offered=mission_manager.offer(kind,world_map::index(),current_town,wasteland::layout(),combat_world.spawns);
                    text.generate(-104,38,"CONTRACT BOARD",hud_text);
                    bn::string<32> line="<  ";line+=missions::type_name(kind);line+="  >";
                    text.generate(-104,51,line,hud_text);
                    line="REGION ";line+=world_map::name(offered.target_map);
                    text.generate(-104,63,line,hud_text);
                    line="PAY ";line+=bn::to_string<6>(offered.reward);line+=" CR  A ACCEPT";
                    text.generate(-104,75,line,hud_text);
                } else if(active.state==missions::status::active) {
                    bn::string<32> line="ACTIVE ";line+=missions::type_name(active.kind);
                    text.generate(-104,40,line,hud_text);
                    line="REGION ";line+=world_map::name(active.target_map);
                    text.generate(-104,55,line,hud_text);
                    line="PAY ";line+=bn::to_string<6>(active.reward);line+=" CR";
                    text.generate(-104,68,line,hud_text);
                    text.generate(32,68,"B CLOSE",hud_text);
                } else {
                    text.generate(-104,40,"CONTRACT COMPLETE",hud_text);
                    bn::string<32> line="EARNED ";line+=bn::to_string<6>(active.reward);line+=" CR";
                    text.generate(-104,55,line,hud_text);
                    line="TOTAL ";line+=bn::to_string<7>(mission_manager.credits());line+=" CR";
                    text.generate(-104,68,line,hud_text);
                    text.generate(20,68,"A NEW JOBS",hud_text);
                }
            } else if(town && town->menu_open()) {
                if(town->menu_page()==0) {
                    bn::string<32> line="MECHANIC / SETUP   DOWN SHOP";text.generate(-104,38,line,hud_text);
                    line="<  ";line+=driving::setups[town->menu_selection()].name;line+="  >";text.generate(-104,52,line,hud_text);
                    line=bn::to_string<5>(driving::setups[town->menu_selection()].mass);line+=" KG";text.generate(18,52,line,hud_text);
                    text.generate(-104,68,"A FIT     B CANCEL",hud_text);
                } else {
                    const int id=town->shop_item();
                    if(id<0) {
                        text.generate(6,-20,"SOLD OUT",hud_text);
                        text.generate(6,56,"B SETUP",hud_text);
                    } else {
                    const auto& item=garage_shop::catalog[id];
                    if(town->shop_info_open()) {
                        bn::string<32> line=item.name;text.generate(-104,35,line,hud_text);
                        text.generate(-104,49,item.detail,hud_text);
                        line=bn::to_string<5>(item.credits);line+=" GOLD + ";
                        line+=bn::to_string<3>(item.scrap);line+=" SCRAP";text.generate(-104,63,line,hud_text);
                        text.generate(-104,76,"R/B BACK",hud_text);
                    } else {
                        bn::string<32> line=item.name;text.generate(6,-29,line,hud_text);
                        line=bn::to_string<5>(item.credits);line+=" GOLD + ";
                        line+=bn::to_string<3>(item.scrap);line+=" SCR";text.generate(6,-13,line,hud_text);
                        if(scrap<item.scrap || mission_manager.credits()<item.credits)line="NEED RESOURCES";
                        else line="AVAILABLE";
                        text.generate(6,3,line,hud_text);
                        line="GOLD ";line+=bn::to_string<6>(mission_manager.credits());text.generate(6,20,line,hud_text);
                        line="SCRAP ";line+=bn::to_string<5>(scrap);text.generate(6,36,line,hud_text);
                        text.generate(6,56,"A BUY  R INFO",hud_text);
                        text.generate(6,72,"B SETUP",hud_text);
                    }
                    }
                }
            }
        }
        if(state==8 && fitting && redraw) {
            hud_text.clear();
            const auto weapon=fitting->highlighted_weapon(combat_world);
            if(fitting->info_open()) {
                if(weapon==combat::Weapon::empty) {
                    text.generate(22,-22,"EMPTY MOUNT",hud_text);
                    text.generate(22,-5,"NO ENERGY DRAW",hud_text);
                    text.generate(22,12,"REMOVES FITTING",hud_text);
                } else if(weapon==combat::Weapon::radio) {
                    text.generate(22,-30,"RADIO",hud_text);
                    text.generate(22,-12,"PASSIVE RECEIVER",hud_text);
                    text.generate(22,4,"NO ENERGY DRAW",hud_text);
                    text.generate(22,22,"FINDS HIDDEN SIGNALS",hud_text);
                } else {
                    bn::string<24> line=combat::weapon_name(weapon);text.generate(22,-30,line,hud_text);
                    line="DAMAGE ";line+=bn::to_string<2>(weapon==combat::Weapon::missile?combat::missile_damage:
                                                           weapon==combat::Weapon::trap?combat::trap_damage:
                                                           weapon==combat::Weapon::sniper?combat::sniper_damage:1);
                    text.generate(22,-12,line,hud_text);
                    line="ENERGY ";line+=bn::to_string<2>(combat::weapon_energy_costs[int(weapon)]);
                    text.generate(22,4,line,hud_text);
                    if(weapon==combat::Weapon::gun)line="FORWARD FIRE";
                    else if(weapon==combat::Weapon::sides)line="TWIN SIDE FIRE";
                    else if(weapon==combat::Weapon::sniper)line="LONG RANGE / SLOW";
                    else if(weapon==combat::Weapon::front_shooter)line="TWIN FORWARD FIRE";
                    else if(weapon==combat::Weapon::missile)line="HOMING MISSILE";
                    else line="ARMED REAR TRAP";
                    text.generate(22,22,line,hud_text);
                }
            } else {
                bn::string<24> line=fitting->inventory_open()?"CHOICE ":"FITTED ";
                line+=weapon==combat::Weapon::empty?"EMPTY":combat::weapon_name(weapon);
                text.generate(22,58,line,hud_text);
            }
        }
        if(state==7 && redraw) {
            hud_text.clear();
            text.generate(-104,48,"WRECKED",hud_text);
            text.generate(-104,68,"PRESS A TO REVIVE",hud_text);
        }

        if(state==2 && redraw) {
            hud_text.clear();
            const auto& active=mission_manager.current();
            const auto& race=race_manager.current();
            bn::string<40> vitality="HP ";vitality+=bn::to_string<4>(combat_world.player_hp);
            vitality+='/';vitality+=bn::to_string<4>(combat_world.max_player_hp());vitality+="  SH ";
            vitality+=bn::to_string<3>(combat_world.player_shield);vitality+='/';
            vitality+=bn::to_string<3>(combat::player_max_shield);vitality+="  EN ";
            vitality+=bn::to_string<4>(combat_world.player_energy);vitality+='/';
            vitality+=bn::to_string<4>(combat_world.max_player_energy());text.generate(-108,8,vitality,hud_text);
            if(race_manager.active()) {
                bn::string<32> line=races::kind_name(race.mode);line+="  GATE ";
                line+=bn::to_string<3>(race.next_checkpoint+1);line+='/';
                line+=bn::to_string<3>(race.checkpoint_count);
                text.generate(-96,18,line,hud_text);
                line="TIME ";line+=bn::to_string<4>((race.time_left+59)/60);line+=" SEC";
                text.generate(-96,32,line,hud_text);
                text.generate(-96,46,"B ABORT   START RESUME",hud_text);
            } else if(race.state==races::phase::result) {
                bn::string<32> line=races::outcome_name(race.result);line+="  SCORE ";
                line+=bn::to_string<5>(race.score);text.generate(-96,24,line,hud_text);
            } else if(active.state==missions::status::none) {
                text.generate(-96,24,"NO ACTIVE CONTRACT",hud_text);
                text.generate(-96,38,"VISIT AN OUTPOST BOARD",hud_text);
            } else if(active.state==missions::status::complete) {
                text.generate(-96,24,"CONTRACT COMPLETE",hud_text);
                bn::string<32> line="CREDITS ";line+=bn::to_string<8>(mission_manager.credits());
                text.generate(-96,38,line,hud_text);
            } else {
                bn::string<32> line=missions::type_name(active.kind);line+=" / ";
                line+=world_map::name(active.target_map);
                text.generate(-96,24,line,hud_text);
                line="REWARD ";line+=bn::to_string<7>(active.reward);line+=" CR";
                text.generate(-96,38,line,hud_text);
            }
            bn::string<32> totals="JOBS ";totals+=bn::to_string<5>(mission_manager.completed());
            totals+="  CR ";totals+=bn::to_string<8>(mission_manager.credits());totals+="  SCRAP ";totals+=bn::to_string<5>(scrap);
            text.generate(-96,60,totals,hud_text);
        }

        if(state==6 && redraw) {
            hud_text.clear();
            if(settings_panel==settings_map_panel) {
                bn::string<32> line;
                if(map_selection==0)line="CURRENT POSITION";
                else if(map_selection<=cave_layout::town_count) {
                    line=world_map::town_name(map_selection-1);
                } else {
                    const int portal_index=map_selection-1-cave_layout::town_count;
                    const auto portal=world_map::portal(portal_index);
                    if(portal.destination_map>=0) {
                        line="ROAD TO ";line+=world_map::name(portal.destination_map);
                    } else line="UNCONNECTED ROAD";
                }
                text.generate(-line.size()*3,70,line,hud_text);
            } else if(settings_panel==settings_handling_panel) {
                bn::string<32> line=tuning_selection==0?">ACC ":" ACC ";
                append_fixed(line,tuning.acceleration,3);text.generate(-58,-60,line,hud_text);
                line=tuning_selection==1?">SPD ":" SPD ";
                append_fixed(line,tuning.max_speed,2);text.generate(-58,-47,line,hud_text);
                line=tuning_selection==2?">GRP ":" GRP ";
                append_fixed(line,tuning.grip,3);text.generate(-58,-34,line,hud_text);
                line=tuning_selection==3?">STR ":" STR ";
                append_fixed(line,tuning.steer,2);text.generate(-58,-21,line,hud_text);
                line=tuning_selection==4?">CST ":" CST ";
                append_fixed(line,tuning.coast_drag,3);text.generate(-58,-8,line,hud_text);
                line=tuning_selection==5?">BRK ":" BRK ";
                append_fixed(line,tuning.brake_force,2);text.generate(-58,5,line,hud_text);
                line=tuning_selection==6?">MAS ":" MAS ";
                line+=bn::to_string<6>(tuning.mass);text.generate(-58,18,line,hud_text);
                line=tuning_selection==handling_battery_row?">BAT ":" BAT ";
                line+=battery_names[battery_selection];line+=' ';line+=bn::to_string<4>(battery_capacities[battery_selection]);line+='E';
                line+=' ';if(battery_mass[battery_selection]>=0)line+='+';
                line+=bn::to_string<5>(battery_mass[battery_selection]);
                text.generate(-58,31,line,hud_text);
                line=tuning_selection==handling_car_row?">CAR ":" CAR ";
                line+=vehicle_names[car_type];text.generate(-58,44,line,hud_text);
            } else if(settings_panel==settings_audio_panel) {
                bn::string<24> line=audio_selection==0?"> MUSIC    ":"  MUSIC    ";
                line+=bn::to_string<4>(music_volume*10);line+="%";
                text.generate(-58,-34,line,hud_text);
                line=audio_selection==1?"> SOUND FX ":"  SOUND FX ";
                line+=bn::to_string<4>(sound_volume*10);line+="%";
                text.generate(-58,-10,line,hud_text);
                line=audio_selection==2?"> MUTE ALL ":"  MUTE ALL ";
                line+=master_muted?"ON":"OFF";
                text.generate(-58,14,line,hud_text);
            } else {
                bn::string<28> line=save_selection==0?"> SAVE GAME":"  SAVE GAME";
                text.generate(-58,-34,line,hud_text);
                line=save_selection==1?"> LOAD GAME":"  LOAD GAME";
                if(!cartridge.has_save())line+="  --";
                text.generate(-58,-14,line,hud_text);
                line=save_selection==2?"> ERASE SAVE":"  ERASE SAVE";
                text.generate(-58,6,line,hud_text);
                if(erase_confirm)line="A AGAIN TO ERASE";
                else if(save_result==saves::result::saved)line="GAME SAVED";
                else if(save_result==saves::result::loaded)line="GAME LOADED";
                else if(save_result==saves::result::erased)line="SAVE ERASED";
                else if(save_result==saves::result::no_save)line="NO SAVE DATA";
                else if(save_result==saves::result::invalid)line="SAVE FAILED";
                else if(save_result==saves::result::incompatible)line="SAVE MAP NOT FOUND";
                else if(save_result==saves::result::loaded_updated)line="LOADED / WORLD UPDATED";
                else if(save_result==saves::result::loaded_relocated)line="LOADED / POSITION MOVED";
                else if(cartridge.has_save()) {
                    line="SAVE #";line+=bn::to_string<10>(int(cartridge.generation()));
                } else line="NO SAVE DATA";
                text.generate(-58,34,line,hud_text);
            }
        }
        if(state!=1) {
            shown_surface=-1;
            race_text.clear();race_world_marker.set_visible(false);
            race_left_flag.set_visible(false);race_right_flag.set_visible(false);
            for(auto& marker:race_radar_markers)marker.set_visible(false);
        }
        int view_start=bn::core::current_cpu_ticks();
        if(decorations)decorations->update(camera_x.integer(),camera_y.integer(),state==1 || state==3 || state==7 || state==9);
        if(radar)radar->update_enemies(combat_world,state==1);
        mission_marker.set_visible(false);
        settings_goal_marker.set_visible(false);
        race_world_marker.set_visible(false);
        race_left_flag.set_visible(false);race_right_flag.set_visible(false);
        for(auto& marker:race_radar_markers)marker.set_visible(false);
        const auto& active_race=race_manager.current();
        if(radar && state==1 && race_manager.active()) {
            const int marker_end=bn::min(active_race.checkpoint_count,active_race.next_checkpoint+1);
            int marker_slot=0;
            for(int index=active_race.next_checkpoint;index<marker_end;++index) {
                int target_x=radar->project_x(active_race.checkpoints[index].x);
                int target_y=radar->project_y(active_race.checkpoints[index].y);
                while(target_x*target_x+target_y*target_y>
                      local_minimap::marker_radius*local_minimap::marker_radius) {
                    target_x=target_x*7/8;target_y=target_y*7/8;
                }
                if(marker_slot<race_radar_markers.size()) {
                    auto& marker=race_radar_markers[marker_slot++];
                    marker.set_position(local_minimap::screen_x+target_x,local_minimap::screen_y+target_y);
                    marker.set_visible(true);
                }
            }
        }
        const int race_world_checkpoint=race_world_linger_frames>0?
                                        race_world_linger_checkpoint:
                                        race_manager.active()?active_race.next_checkpoint:-1;
        if(state==1 && race_world_checkpoint>=0 &&
           race_world_checkpoint<active_race.checkpoint_count) {
                const auto target=active_race.checkpoints[race_world_checkpoint];
                const int x=int(target.x)-camera_x.integer(),y=int(target.y)-camera_y.integer();
                if(x>-112 && x<112 && y>-72 && y<72) {
                    race_world_marker.set_position(x,y);
                    race_world_marker.set_tiles(bn::sprite_items::race_gate.tiles_item(),(frame/8)&1);
                    race_world_marker.set_visible(true);
                }
                const uint16_t flag_bit=uint16_t(1u<<race_world_checkpoint);
                if(active_race.left_flag_mask&flag_bit) {
                    const auto flag=active_race.left_flags[race_world_checkpoint];
                    const int flag_x=int(flag.x)-camera_x.integer(),flag_y=int(flag.y)-camera_y.integer();
                    if(flag_x>-116 && flag_x<116 && flag_y>-72 && flag_y<72) {
                        race_left_flag.set_position(flag_x,flag_y);race_left_flag.set_visible(true);
                    }
                }
                if(active_race.right_flag_mask&flag_bit) {
                    const auto flag=active_race.right_flags[race_world_checkpoint];
                    const int flag_x=int(flag.x)-camera_x.integer(),flag_y=int(flag.y)-camera_y.integer();
                    if(flag_x>-116 && flag_x<116 && flag_y>-72 && flag_y<72) {
                        race_right_flag.set_position(flag_x,flag_y);race_right_flag.set_visible(true);
                    }
                }
        }
        if(state==1 && redraw) {
            race_text.clear();
            if(load_notice_frames>0) {
                bn::string<32> line="LOADED ";line+=world_map::name(world_map::index());
                text.generate(-114,50,line,race_text);
                if(loaded_position_adjusted)line="NEAREST SAFE POSITION";
                else if(!loaded_exact_world)line="SAVED POSITION / NEW WORLD";
                else line="SAVED POSITION";
                text.generate(-114,65,line,race_text);
            } else if(active_race.state==races::phase::countdown) {
                bn::string<24> line="RACE START ";
                line+=bn::to_string<2>((active_race.countdown_frames+59)/60);
                text.generate(-114,56,line,race_text);
            } else if(active_race.state==races::phase::running) {
                bn::string<28> line="RACE ";line+=bn::to_string<3>(active_race.next_checkpoint+1);
                line+='/';line+=bn::to_string<3>(active_race.checkpoint_count);
                line+="  ";line+=bn::to_string<4>((active_race.time_left+59)/60);line+='S';
                text.generate(-114,56,line,race_text);
                if(active_race.off_course_frames>0)
                    text.generate(-114,70,"RETURN TO COURSE",race_text);
            } else if(active_race.state==races::phase::result && race_result_frames>0) {
                bn::string<24> line=races::outcome_name(active_race.result);
                if(active_race.result==races::outcome::complete) {
                    line+="  SCORE ";line+=bn::to_string<5>(active_race.score);
                }
                text.generate(-114,62,line,race_text);
            }
        }
        const auto& active_mission=mission_manager.current();
        settings_goal_marker.set_visible(false);
        if(state==6 && settings_panel==settings_map_panel) {
            int objective_x=-1,objective_y=-1;
            if(race_manager.active() && active_race.next_checkpoint>=0 &&
               active_race.next_checkpoint<active_race.checkpoint_count) {
                const auto target=active_race.checkpoints[active_race.next_checkpoint];
                objective_x=target.x;objective_y=target.y;
            } else if(!race_manager.session() && active_mission.state==missions::status::active) {
                objective_x=active_mission.target_x;objective_y=active_mission.target_y;
                if(active_mission.target_map!=world_map::index()) {
                    const int route=world_map::route_portal(active_mission.target_map);
                    if(route>=0) { const auto next=world_map::portal(route);objective_x=next.x;objective_y=next.y; }
                    else objective_x=-1;
                }
            }
            if(objective_x>=0) {
                settings_goal_marker.set_position(world_overview::screen_x(objective_x),world_overview::screen_y(objective_y));
                settings_goal_marker.set_visible(true);
            }
        } else if(state==6)minimap_dot.set_visible(false);
        if(radar && state==1 && !race_manager.session() && active_mission.state==missions::status::active) {
            int objective_x=active_mission.target_x,objective_y=active_mission.target_y;
            if(active_mission.target_map!=world_map::index()) {
                const int route=world_map::route_portal(active_mission.target_map);
                if(route>=0) { const auto next=world_map::portal(route);objective_x=next.x;objective_y=next.y; }
            }
            int target_x=radar->project_x(objective_x);
            int target_y=radar->project_y(objective_y);
            while(target_x*target_x+target_y*target_y>
                  local_minimap::marker_radius*local_minimap::marker_radius) {
                target_x=target_x*7/8;target_y=target_y*7/8;
            }
            mission_marker.set_position(local_minimap::screen_x+target_x,local_minimap::screen_y+target_y);
            mission_marker.set_visible(true);
        }
        if(combat_graphics) combat_graphics->update(combat_world,camera_x.integer(),camera_y.integer(),state==1 || state==7);
        const bool radio_fitted=combat_world.fitted_weapon(combat::MountSlot::special)==combat::Weapon::radio;
        if(radio_graphics)radio_graphics->update(radio_world,car,camera_x.integer(),camera_y.integer(),
                                                radio_fitted,state==1);
        adaptive_music::section music_section=adaptive_music::section::cruise;
        if(state==1) {
            bool enemy_near=false,enemy_engaged=false;
            for(const auto& enemy:combat_world.enemies) {
                if(enemy.hp>0) {
                    enemy_near=true;
                    const int dx=bn::abs(enemy.car.x.integer()-car.x.integer());
                    const int dy=bn::abs(enemy.car.y.integer()-car.y.integer());
                    if(dx<=360 && dy<=360 && dx*dx+dy*dy<=360*360)enemy_engaged=true;
                }
            }
            if(enemy_engaged || combat_world.fired || combat_world.impact)
                music_section=adaptive_music::section::combat;
            else if(enemy_near)
                music_section=adaptive_music::section::danger;
            else if(car.speed>tuning.max_speed*fixed(0.58))
                music_section=adaptive_music::section::drive;
        }
        music.set_ducked(state!=1);
        music.update(music_section);
        dustline_combat_telemetry[19]=bn::core::current_cpu_ticks()-view_start;
        dustline_combat_telemetry[0]=0x434F4D42;
        dustline_combat_telemetry[1]=combat_world.ticks;
        dustline_combat_telemetry[2]=combat_world.player_hp;
        dustline_combat_telemetry[3]=combat_world.player_hits;
        dustline_combat_telemetry[4]=combat_world.player_shots;
        dustline_combat_telemetry[5]=combat_world.enemy_shots;
        dustline_combat_telemetry[6]=combat_world.hits;
        dustline_combat_telemetry[7]=combat_world.kills;
        dustline_combat_telemetry[8]=combat_world.wall_hits;
        dustline_combat_telemetry[9]=combat_world.expired;
        dustline_combat_telemetry[10]=combat_world.living();
        dustline_combat_telemetry[12]=sizeof(combat_world);
        dustline_combat_telemetry[13]=combat_graphics?1:0;
        dustline_combat_telemetry[14]=combat_world.player_invulnerability;
        int collisions=0,avoidance=0,recoveries=0,bullets=0;
        for(int i=0;i<combat::enemy_count;++i) {
            const auto& e=combat_world.enemies[i]; int at=20+i*12;
            dustline_combat_telemetry[at]=e.car.x.data(); dustline_combat_telemetry[at+1]=e.car.y.data();
            dustline_combat_telemetry[at+2]=e.car.vx.data(); dustline_combat_telemetry[at+3]=e.car.vy.data();
            dustline_combat_telemetry[at+4]=e.car.heading.data(); dustline_combat_telemetry[at+5]=e.hp;
            dustline_combat_telemetry[at+6]=e.car.collisions; dustline_combat_telemetry[at+7]=e.reverse;
            dustline_combat_telemetry[at+8]=e.avoidance; dustline_combat_telemetry[at+9]=e.recoveries;
            dustline_combat_telemetry[at+10]=e.explosion; dustline_combat_telemetry[at+11]=e.flash;
            collisions+=e.car.collisions; avoidance+=e.avoidance; recoveries+=e.recoveries;
        }
        for(int i=0;i<combat::bullet_count;++i) {
            const auto& b=combat_world.bullets[i]; int at=80+i*4;
            dustline_combat_telemetry[at]=b.x.data(); dustline_combat_telemetry[at+1]=b.y.data();
            dustline_combat_telemetry[at+2]=b.remaining; dustline_combat_telemetry[at+3]=b.hostile;
            bullets+=b.remaining>0;
        }
        dustline_combat_telemetry[11]=bullets;
        dustline_combat_telemetry[15]=collisions; dustline_combat_telemetry[16]=avoidance;
        dustline_combat_telemetry[17]=recoveries;
        dustline_combat_telemetry[176]=combat_world.bumps;
        dustline_combat_telemetry[177]=combat_world.last_bump.data();
        dustline_combat_telemetry[178]=car.mass;
        dustline_combat_telemetry[179]=combat_world.last_pair;
        dustline_combat_telemetry[180]=combat_world.player_bumps;
        for(int i=0;i<combat::enemy_count;++i) {
            const auto& e=combat_world.enemies[i]; int at=184+i*9;
            dustline_combat_telemetry[at]=e.vehicle_avoidance;
            dustline_combat_telemetry[at+1]=e.moving_frames;
            dustline_combat_telemetry[at+2]=e.maneuver;
            dustline_combat_telemetry[at+3]=e.side;
            dustline_combat_telemetry[at+4]=e.car.mass;
            dustline_combat_telemetry[at+5]=e.goal_x;
            dustline_combat_telemetry[at+6]=e.goal_y;
            dustline_combat_telemetry[at+7]=e.stalled;
            dustline_combat_telemetry[at+8]=e.spawn_id;
        }
        dustline_combat_telemetry[229]=combat_world.spawns.count;
        dustline_combat_telemetry[230]=combat_world.spawned;
        dustline_combat_telemetry[231]=combat_world.despawned;
        dustline_combat_telemetry[232]=combat::spawn_cooldown;
        dustline_combat_telemetry[233]=int(reinterpret_cast<uintptr_t>(combat_world.spawns.points));
        dustline_combat_telemetry[234]=sizeof(enemy_spawns::point);
        dustline_combat_telemetry[235]=combat::spawn_range;
        dustline_combat_telemetry[236]=combat::despawn_range;
        dustline_combat_telemetry[237]=combat_world.player_shield;
        dustline_combat_telemetry[238]=combat_world.player_shield_delay;
        dustline_combat_telemetry[239]=combat_world.player_invulnerability;
        dustline_weapon_telemetry[0]=int(combat_world.weapon);
        dustline_weapon_telemetry[1]=combat_world.saw_active;
        dustline_weapon_telemetry[2]=combat_world.saw_x.data();dustline_weapon_telemetry[3]=combat_world.saw_y.data();
        dustline_weapon_telemetry[4]=combat_world.guidance_updates;dustline_weapon_telemetry[5]=combat_world.trap_explosions;
        dustline_weapon_telemetry[6]=combat_world.weapon_mask();dustline_weapon_telemetry[7]=settings_panel;
        for(int i=0;i<combat::weapon_count;++i) {
            dustline_weapon_telemetry[8+i]=combat_world.weapon_shots[i];dustline_weapon_telemetry[17+i]=combat_world.weapon_hits[i];
        }
        for(int i=0;i<combat::missile_count;++i) {
            const auto& m=combat_world.missiles[i];int at=26+i*9;
            dustline_weapon_telemetry[at]=m.x.data();dustline_weapon_telemetry[at+1]=m.y.data();
            dustline_weapon_telemetry[at+2]=m.vx.data();dustline_weapon_telemetry[at+3]=m.vy.data();
            dustline_weapon_telemetry[at+4]=m.remaining;dustline_weapon_telemetry[at+5]=m.age;
            dustline_weapon_telemetry[at+6]=m.target_spawn;dustline_weapon_telemetry[at+7]=m.heading.data();
            dustline_weapon_telemetry[at+8]=m.explosion;
        }
        for(int i=0;i<combat::trap_count;++i) {
            const auto& t=combat_world.traps[i];int at=44+i*5;
            dustline_weapon_telemetry[at]=t.x.data();dustline_weapon_telemetry[at+1]=t.y.data();
            dustline_weapon_telemetry[at+2]=t.remaining;dustline_weapon_telemetry[at+3]=t.arm;dustline_weapon_telemetry[at+4]=t.explosion;
        }
        dustline_weapon_telemetry[74]=fitting?fitting->slot():0;
        dustline_weapon_telemetry[75]=car_type;
        dustline_weapon_telemetry[76]=combat_world.player_energy;
        dustline_weapon_telemetry[77]=combat_world.max_player_energy();
        dustline_weapon_telemetry[78]=int(combat_world.fitted_weapon(combat::MountSlot::front));
        dustline_weapon_telemetry[79]=int(combat_world.fitted_weapon(combat::MountSlot::side));
        dustline_weapon_telemetry[80]=int(combat_world.fitted_weapon(combat::MountSlot::special));
        dustline_weapon_telemetry[81]=fitting&&fitting->inventory_open();
        dustline_radio_telemetry[0]=0x52414449;
        dustline_radio_telemetry[1]=combat_world.fitted_weapon(combat::MountSlot::special)==combat::Weapon::radio;
        dustline_radio_telemetry[2]=radio_world.target();
        dustline_radio_telemetry[3]=radio_world.direction();
        dustline_radio_telemetry[4]=radio_world.strength();
        dustline_radio_telemetry[5]=radio_world.active_count();
        dustline_radio_telemetry[6]=radio_graphics&&radio_graphics->barrel_visible();
        dustline_radio_telemetry[7]=radio_graphics&&radio_graphics->indicator_visible();
        dustline_radio_telemetry[8]=state==0?selected_map:world_map::index();
        dustline_radio_telemetry[9]=radio_signal::detection_radius;
        dustline_radio_telemetry[10]=radio_world.desired_count();
        dustline_radio_telemetry[11]=radio_world.next_respawn_frames();
        dustline_radio_telemetry[12]=int(radio_world.clock());
        const int radio_target=radio_world.target()>=0?radio_world.target():0;
        dustline_radio_telemetry[13]=radio_world.item(radio_target).x;
        dustline_radio_telemetry[14]=radio_world.item(radio_target).y;
        dustline_radio_telemetry[15]=0;
        for(int index=0;index<radio_signal::source_count;++index) {
            const auto& signal=radio_world.item(index);const int at=16+index*3;
            dustline_radio_telemetry[at]=signal.active;
            dustline_radio_telemetry[at+1]=signal.x;
            dustline_radio_telemetry[at+2]=signal.y;
        }
        dustline_radio_telemetry[31]=0;
        dustline_telemetry[0]=0x44555354;
        dustline_telemetry[1]=frame;
        dustline_telemetry[2]=state;
        dustline_telemetry[3]=setup;
        dustline_telemetry[4]=car.x.data(); dustline_telemetry[5]=car.y.data();
        dustline_telemetry[6]=car.vx.data(); dustline_telemetry[7]=car.vy.data();
        dustline_telemetry[8]=car.heading.data(); dustline_telemetry[9]=car.slip.data();
        dustline_telemetry[10]=car.surface; dustline_telemetry[11]=lap_frames;
        dustline_telemetry[12]=best[setup]; dustline_telemetry[13]=laps;
        dustline_telemetry[14]=checkpoint; dustline_telemetry[15]=car.collisions;
        dustline_telemetry[16]=bn::core::last_cpu_usage().data();
        dustline_telemetry[17]=missed;
        dustline_telemetry[18]=camera_x.integer();
        dustline_telemetry[19]=camera_y.integer();
        dustline_telemetry[20]=bn::core::last_vblank_usage().data();
        dustline_telemetry[21]=track?track->uploaded_bytes():0;
        dustline_telemetry[22]=(bn::bg_tiles::used_blocks_count()+bn::bg_maps::used_blocks_count())*2048;
        dustline_telemetry[23]=bn::sprite_tiles::available_tiles_count()*32;
        dustline_telemetry[24]=state==0?selected_map:world_map::index();
        dustline_telemetry[25]=world_map::width();
        dustline_telemetry[26]=world_map::height();
        dustline_telemetry[27]=(car.y.integer()/256)*(world_map::width()/256)+car.x.integer()/256;
        dustline_telemetry[28]=track?track->capacity():0;
        dustline_telemetry[29]=track?track->unique_tiles():0;
        dustline_telemetry[30]=track?track->working_ram_bytes():0;
        dustline_telemetry[31]=world_map::chunk_loads();
        dustline_telemetry[32]=world_map::chunk_decodes();
        dustline_telemetry[33]=world_map::chunk_cache_bytes();
        dustline_telemetry[34]=minimap_dot.x().integer();
        dustline_telemetry[35]=minimap_dot.y().integer();
        dustline_telemetry[36]=wasteland::active()?wasteland::layout().seed():0;
        dustline_telemetry[37]=wasteland::active()?wasteland::layout().signature():0;
        dustline_telemetry[38]=wasteland::generations();
        dustline_telemetry[39]=current_town;
        dustline_telemetry[40]=wasteland::active()?wasteland::layout().town(0).x:0;
        dustline_telemetry[41]=wasteland::active()?wasteland::layout().town(0).y:0;
        dustline_telemetry[42]=wasteland::active()?wasteland::layout_bytes():0;
        dustline_telemetry[43]=wasteland::generation_scratch_bytes();
        dustline_telemetry[44]=bn::memory::available_alloc_ewram();
        dustline_telemetry[45]=wasteland::active()?wasteland::layout().floor_count():0;
        dustline_telemetry[46]=wasteland::generation_updates();
        dustline_telemetry[47]=town_visits;
        dustline_telemetry[48]=radar?radar->center_x():0;
        dustline_telemetry[49]=radar?radar->center_y():0;
        dustline_telemetry[50]=radar?radar->revisions():0;
        dustline_telemetry[51]=1;
        dustline_telemetry[52]=radar?radar->scale():0;
        dustline_telemetry[54]=tuning.acceleration.data();
        dustline_telemetry[55]=tuning.max_speed.data();
        dustline_telemetry[56]=tuning.grip.data();
        dustline_telemetry[57]=tuning.steer.data();
        dustline_telemetry[58]=tuning.mass;
        dustline_telemetry[59]=tuning_selection;
        dustline_telemetry[60]=tuning.coast_drag.data();
        dustline_telemetry[61]=tuning.brake_force.data();
        dustline_telemetry[62]=car.material_id;
        dustline_telemetry[63]=car.material;
        dustline_telemetry[64]=shown_surface;
        dustline_telemetry[65]=car.terrain_rumble.data();
        dustline_town_telemetry[0]=0x544F574E;
        dustline_town_telemetry[1]=town?int(town->current_place()):-1;
        dustline_town_telemetry[2]=town?town->x():0;
        dustline_town_telemetry[3]=town?town->y():0;
        dustline_town_telemetry[4]=town?town->direction():0;
        dustline_town_telemetry[5]=town&&town->menu_open();
        dustline_town_telemetry[6]=town?town->menu_selection():0;
        dustline_town_telemetry[7]=town?town->town_id():-1;
        dustline_town_telemetry[8]=town&&town->prompt_visible();
        dustline_town_telemetry[9]=town&&town->player_visible();
        dustline_progression_telemetry[0]=0x50524752;
        dustline_progression_telemetry[1]=scrap;
        dustline_progression_telemetry[2]=shop_owned.words[0];
        dustline_progression_telemetry[3]=shop_notice;
        dustline_progression_telemetry[4]=town?town->shop_count():-1;
        dustline_progression_telemetry[5]=combat_world.collected_scrap;
        dustline_progression_telemetry[6]=0;
        dustline_progression_telemetry[7]=fitting?2:town?town->menu_page():-1;
        dustline_progression_telemetry[8]=town?town->shop_selection():-1;
        int active_pickups=0;for(const auto& pickup:combat_world.pickups)active_pickups+=pickup.remaining>0;
        dustline_progression_telemetry[9]=active_pickups;
        dustline_progression_telemetry[10]=combat_world.collected_energy;
        dustline_progression_telemetry[11]=fitting?fitting->slot():-1;
        dustline_progression_telemetry[12]=fitting&&fitting->inventory_open();
        dustline_progression_telemetry[13]=fitting?fitting->inventory_selection():-1;
        dustline_progression_telemetry[14]=fitting&&fitting->info_open();
        dustline_progression_telemetry[15]=town&&town->shop_info_open();
        const auto& telemetry_mission=mission_manager.current();
        dustline_mission_telemetry[0]=0x4A4F4253;
        dustline_mission_telemetry[1]=int(telemetry_mission.kind);
        dustline_mission_telemetry[2]=int(telemetry_mission.state);
        dustline_mission_telemetry[3]=telemetry_mission.origin_town;
        dustline_mission_telemetry[4]=telemetry_mission.target_town;
        dustline_mission_telemetry[5]=telemetry_mission.target_spawn;
        dustline_mission_telemetry[6]=telemetry_mission.target_x;
        dustline_mission_telemetry[7]=telemetry_mission.target_y;
        dustline_mission_telemetry[8]=telemetry_mission.progress;
        dustline_mission_telemetry[9]=telemetry_mission.goal;
        dustline_mission_telemetry[10]=telemetry_mission.reward;
        dustline_mission_telemetry[11]=mission_manager.credits();
        dustline_mission_telemetry[12]=mission_manager.completed();
        dustline_mission_telemetry[13]=mission_manager.serial();
        dustline_mission_telemetry[14]=contract_open;
        dustline_mission_telemetry[15]=contract_selection;
        dustline_mission_telemetry[16]=telemetry_mission.origin_map;
        dustline_mission_telemetry[17]=telemetry_mission.target_map;
        dustline_portal_telemetry[0]=0x47415445;
        dustline_portal_telemetry[1]=world_map::portal_count();
        dustline_portal_telemetry[2]=current_gate;
        dustline_portal_telemetry[3]=ignored_gate;
        dustline_portal_telemetry[4]=gate_visits;
        dustline_portal_telemetry[5]=gate_yes;
        dustline_portal_telemetry[6]=state==9;
        if(current_gate>=0 && current_gate<world_map::portal_count()) {
            const auto telemetry_gate=world_map::portal(current_gate);
            dustline_portal_telemetry[7]=telemetry_gate.destination_map;
            dustline_portal_telemetry[8]=telemetry_gate.destination_spawn;
            dustline_portal_telemetry[9]=telemetry_gate.x;
            dustline_portal_telemetry[10]=telemetry_gate.y;
        } else {
            for(int index=7;index<=10;++index)dustline_portal_telemetry[index]=-1;
        }
        dustline_portal_telemetry[11]=telemetry_mission.state==missions::status::active?
                                      world_map::route_portal(telemetry_mission.target_map):-1;
        const auto& telemetry_race=race_manager.current().state==races::phase::none?
                                   race_manager.offered():race_manager.current();
        dustline_race_telemetry[0]=0x52414345;
        dustline_race_telemetry[1]=int(telemetry_race.mode);
        dustline_race_telemetry[2]=int(telemetry_race.state);
        dustline_race_telemetry[3]=int(telemetry_race.result);
        dustline_race_telemetry[4]=telemetry_race.closed;
        dustline_race_telemetry[5]=telemetry_race.origin_town;
        dustline_race_telemetry[6]=telemetry_race.target_town;
        dustline_race_telemetry[7]=telemetry_race.checkpoint_count;
        dustline_race_telemetry[8]=telemetry_race.next_checkpoint;
        dustline_race_telemetry[9]=telemetry_race.time_limit;
        dustline_race_telemetry[10]=telemetry_race.time_left;
        dustline_race_telemetry[11]=telemetry_race.elapsed;
        dustline_race_telemetry[12]=telemetry_race.score;
        dustline_race_telemetry[13]=telemetry_race.reward;
        dustline_race_telemetry[14]=telemetry_race.earned;
        dustline_race_telemetry[15]=telemetry_race.off_course_frames;
        dustline_race_telemetry[16]=telemetry_race.no_progress_frames;
        dustline_race_telemetry[17]=race_open;
        dustline_race_telemetry[18]=race_selection;
        dustline_race_telemetry[19]=telemetry_race.start.x;
        dustline_race_telemetry[20]=telemetry_race.start.y;
        for(int index=0;index<races::max_checkpoints;++index) {
            dustline_race_telemetry[21+index*2]=telemetry_race.checkpoints[index].x;
            dustline_race_telemetry[22+index*2]=telemetry_race.checkpoints[index].y;
        }
        dustline_race_telemetry[53]=race_manager.serial();
        dustline_race_telemetry[54]=mission_manager.credits();
        dustline_race_telemetry[55]=race_world_linger_checkpoint;
        dustline_race_telemetry[56]=race_world_linger_frames;
        dustline_race_telemetry[57]=race_world_marker.visible();
        dustline_race_telemetry[58]=race_left_flag.visible();
        dustline_race_telemetry[59]=race_right_flag.visible();
        dustline_race_telemetry[60]=!race_radar_markers.empty() && race_radar_markers[0].visible();
        dustline_race_telemetry[61]=race_world_checkpoint;
        if(state==1 && race_world_linger_frames>0)--race_world_linger_frames;
        dustline_music_telemetry[0]=0x4D555343;
        dustline_music_telemetry[1]=music.playing();
        dustline_music_telemetry[2]=int(music.active_section());
        dustline_music_telemetry[3]=int(music.target_section());
        dustline_music_telemetry[4]=music.position();
        dustline_music_telemetry[5]=music.downgrade_frames();
        dustline_music_telemetry[6]=adaptive_music_data::bpm;
        dustline_music_telemetry[7]=adaptive_music_data::section_count;
        dustline_music_telemetry[8]=music_volume;
        dustline_music_telemetry[9]=sound_volume;
        dustline_music_telemetry[10]=master_muted;
        dustline_music_telemetry[11]=audio_selection;
        dustline_music_telemetry[12]=bn::sound::master_volume().data();
        dustline_music_telemetry[13]=music.playing()?bn::music::volume().data():0;
        dustline_music_telemetry[14]=engine_volume.data();
        dustline_music_telemetry[15]=engine_pitch.data();
        dustline_save_telemetry[0]=0x53415645;
        dustline_save_telemetry[1]=cartridge.has_save();
        dustline_save_telemetry[2]=int(cartridge.generation());
        dustline_save_telemetry[3]=save_selection;
        dustline_save_telemetry[4]=int(save_result);
        dustline_save_telemetry[5]=erase_confirm;
        dustline_save_telemetry[6]=saves::format_version;
        dustline_save_telemetry[7]=saves::slot_size;
        dustline_save_telemetry[8]=loaded_exact_world;
        dustline_save_telemetry[9]=int(world_map::catalog_signature());
        dustline_save_telemetry[10]=settings_return;
        dustline_save_telemetry[11]=loaded_position_adjusted;
        dustline_save_telemetry[12]=title_selection;
        dustline_settings_telemetry[0]=0x4D454E55;
        dustline_settings_telemetry[1]=settings_panel;
        dustline_settings_telemetry[2]=map_selection;
        dustline_settings_telemetry[3]=1+cave_layout::town_count+world_map::portal_count();
        dustline_settings_telemetry[4]=map_selection==0?0:
                                       map_selection<=cave_layout::town_count?1:2;
        dustline_settings_telemetry[5]=map_selection==0?-1:
                                       map_selection<=cave_layout::town_count?map_selection-1:
                                       map_selection-1-cave_layout::town_count;
        dustline_settings_telemetry[6]=tuning_selection;
        dustline_settings_telemetry[7]=state==6;
        bn::core::update();
        missed+=bn::core::last_missed_frames();
    }
}
