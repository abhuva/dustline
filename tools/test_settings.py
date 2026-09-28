"""Field-menu map, handling, battery, audio and save checks in the ROM."""
import json
import math
import struct
import zlib
from test_wasteland import Reference,check_hud_radar


PRESETS=(
    (0.045,2.85,0.165,2.65,0.001,0.09,950),
    (0.053,3.20,0.120,2.90,0.001,0.09,1100),
    (0.033,2.65,0.105,2.25,0.001,0.09,2200),
)


def close(a,b,tolerance=0.001):
    return abs(a-b)<=tolerance


def run(t):
    (t.OUT/'settings').mkdir(exist_ok=True)

    car_assets=('car','car_sand_buggy','car_old','car_truck','car_pickup')
    car_images=[t.Image.open(t.ROOT/f'graphics/{name}.bmp') for name in car_assets]
    base_palette=car_images[0].getpalette()[:48]
    t.check('All five car bodies share the palette-swap contract and 64 headings',
            all(image.size==(32,32*64) and image.getpalette()[:48]==base_palette and
                {4,5,6}.issubset(set(image.getdata())) for image in car_images),
            [dict(name=name,size=image.size) for name,image in zip(car_assets,car_images)])
    for image in car_images:image.close()

    def menu():
        if t.state()['mode']==3:t.tap(t.B)
        if t.state()['mode']==1:t.tap(t.START)
        if t.state()['mode']==2:t.tap(t.SELECT)
        assert t.state()['mode']==0

    def hold(key,frames):
        t.step(0,2);t.step(key,frames);return t.step(0,2)

    menu()
    loose_map=next((i for i,entry in enumerate(t.GAME_MAPS) if entry['id']=='voronoi-passages'),None)
    if loose_map is not None:
        t.start_map(loose_map)
        # The authored Portal access road now runs through the old straight-line
        # sample route. Turn off that branch first, then measure loose ground.
        loose_samples=[t.step(t.A|t.RIGHT) for _ in range(30)]
        loose_samples.extend(t.step(t.A) for _ in range(70))
        loose_slip=max(abs(sample['slip']) for sample in loose_samples if sample['material_kind']==0)
        t.check('Loose ground creates a sustained lateral wander without camera chatter',
                loose_slip>0.04 and max(abs(sample['terrain_rumble']) for sample in loose_samples)==0,
                dict(peak_slip=loose_slip,material=loose_samples[-1]['material_kind']))

    menu();initial=t.start_map(0);ref=Reference(t,initial['seed'])
    t.check('Wasteland opens at the fixed 2x player-centered view',
            initial['zoom_level']==1 and initial['radar_scale']==64,initial)
    down_idle=t.step(t.DOWN,18)
    t.check('Down has no driving action',
            all(down_idle[key]==initial[key] for key in ('x','y','vx','vy','heading')),
            dict(before=initial,after=down_idle))

    accelerated=initial
    for _ in range(40):
        accelerated=t.step(t.A)
    coast_start=math.hypot(accelerated['vx'],accelerated['vy'])
    coast_end=math.hypot(*(t.step(0,6)[axis] for axis in ('vx','vy')))

    # Repeat the same deterministic run so braking and coasting begin from the
    # same location, speed, encounter state and terrain sample.
    menu();initial=t.start_map(0);ref=Reference(t,initial['seed'])
    t.step(t.DOWN,18)
    rough_slip=0;rough_rumble=0
    for _ in range(40):
        accelerated=t.step(t.A)
        if accelerated['material_kind'] in (1,2):
            rough_slip=max(rough_slip,abs(accelerated['slip']))
            rough_rumble=max(rough_rumble,abs(accelerated['terrain_rumble']))
    brake_start=math.hypot(accelerated['vx'],accelerated['vy'])
    brake_end=math.hypot(*(t.step(t.B,6)[axis] for axis in ('vx','vy')))
    coast_drop=coast_start-coast_end
    brake_drop=brake_start-brake_end
    t.check('B braking is materially stronger than neutral coasting',
            t.state()['mode']==1 and coast_drop>0 and
            abs(brake_start-coast_start)<0.001 and brake_end<coast_end*0.7,
            dict(coast_start=coast_start,coast_end=coast_end,coast_drop=coast_drop,
                 brake_start=brake_start,brake_end=brake_end,brake_drop=brake_drop))
    t.check('Stony ground produces soft lateral chatter and a rumble signal',
            rough_slip>0.003 and rough_rumble>0.5,
            dict(peak_slip=rough_slip,peak_rumble=rough_rumble,
                 material=accelerated['material_kind']))

    # Up/Down have no driving action and no longer zoom the minimap.
    generation=initial['generations']
    before_zoom=t.state();t.tap(t.UP);t.tap(t.DOWN);t.step(0,24);s=t.state()
    t.capture('settings/radar-fixed-2x')
    t.check('Driving Up/Down leave the fixed minimap unchanged',s['mode']==1 and
            s['zoom_level']==1 and s['radar_scale']==64 and s['generations']==generation and
            s['radar_revisions']==before_zoom['radar_revisions'],s)
    check_hud_radar(t,ref,'fixed 2x minimap')

    before=t.state();t.tap(t.SELECT);opened=t.step(0,12)
    map_view=t.capture('settings/map-view')
    t.check('Select opens the full-map tab without resetting the car',opened['mode']==6 and
            t.weapon_state()['settings_panel']==0 and
            opened['generations']==before['generations'] and opened['signature']==before['signature'],opened)
    def overview_pixel(x,y):
        return map_view.getpixel((96+int(x)//64,16+int(y)//64))
    player_color=overview_pixel(opened['x'],opened['y'])
    portal_colors=[overview_pixel(item['x'],item['y']) for item in t.GAME_MAPS[opened['map']]['recipe']['portals']]
    t.check('Full map uses a red player cross and green authored Portals',
            player_color[0]>player_color[1]*2 and player_color[0]>player_color[2]*2 and
            portal_colors and all(color[1]>color[0]*2 and color[1]>color[2]*2 for color in portal_colors),
            dict(player=player_color,portals=portal_colors))
    initial_map_selection=t.settings_state()
    t.tap(t.RIGHT);town_selection=t.settings_state();town_view=t.capture('settings/map-town-highlight')
    for _ in range(6):t.tap(t.RIGHT)
    portal_selection=t.settings_state();portal_view=t.capture('settings/map-portal-highlight')
    t.tap(t.LEFT);previous_selection=t.settings_state()
    for _ in range(len(t.GAME_MAPS[opened['map']]['recipe']['portals'])+1):t.tap(t.RIGHT)
    wrapped_player=t.settings_state()
    t.tap(t.LEFT);reverse_wrapped=t.settings_state();t.tap(t.RIGHT)
    t.check('Map Left/Right cycles Player, all towns and all Portals in both directions',
            initial_map_selection['map_selection']==0 and initial_map_selection['map_selection_kind']==0 and
            town_selection['map_selection']==1 and town_selection['map_selection_kind']==1 and
            town_selection['map_selection_index']==0 and portal_selection['map_selection']==7 and
            portal_selection['map_selection_kind']==2 and portal_selection['map_selection_index']==0 and
            previous_selection['map_selection']==6 and
            wrapped_player['map_selection']==0 and wrapped_player['map_selection_kind']==0 and
            reverse_wrapped['map_selection']==initial_map_selection['map_selection_count']-1 and
            reverse_wrapped['map_selection_kind']==2 and
            initial_map_selection['map_selection_count']==1+6+len(t.GAME_MAPS[opened['map']]['recipe']['portals']),
            dict(initial=initial_map_selection,town=town_selection,portal=portal_selection,
                 previous=previous_selection,wrapped=wrapped_player,reverse_wrapped=reverse_wrapped))
    def bright_pixels(image,x,y):
        sx=96+int(x)//64;sy=16+int(y)//64
        return sum(max(image.getpixel((px,py)))-min(image.getpixel((px,py)))>80
                   for py in range(max(0,sy-8),min(160,sy+9))
                   for px in range(max(0,sx-8),min(240,sx+9)))
    first_town=ref.towns[0];first_portal=t.GAME_MAPS[opened['map']]['recipe']['portals'][0]
    t.check('Selected towns and Portals receive a large high-contrast map marker',
            bright_pixels(town_view,*first_town)>20 and
            bright_pixels(portal_view,first_portal['x'],first_portal['y'])>20,
            dict(town=bright_pixels(town_view,*first_town),
                 portal=bright_pixels(portal_view,first_portal['x'],first_portal['y'])))
    frozen=t.step(0,40)
    t.check('Select menu freezes position, velocity and simulation time',all(frozen[k]==opened[k] for k in
            ('x','y','vx','vy','heading','lap_frames','seed','signature')),frozen)

    t.tap(t.R);t.capture('settings/handling-lab')
    t.check('R selects the Driving tab and resets Map selection to Player on return',
            t.weapon_state()['settings_panel']==1,t.weapon_state())
    default_loadout=t.weapon_state()
    t.tap(t.R);audio=t.audio_state();t.capture('settings/audio-control')
    t.check('R opens Music with music off and sound effects at full volume',
            t.weapon_state()['settings_panel']==2 and default_loadout['mask']==1 and
            (default_loadout['front'],default_loadout['side'],default_loadout['special'])==(0,5,5) and audio['music_volume']==0 and
            audio['sound_volume']==10 and not audio['muted'] and audio['sound_master']==1,audio)
    t.tap(t.RIGHT)                   # Music 10%.
    t.tap(t.DOWN);t.tap(t.LEFT);t.tap(t.LEFT)  # Sound effects 80%.
    t.tap(t.DOWN);t.tap(t.A)            # Mute all.
    muted=t.audio_state();t.capture('settings/audio-muted')
    t.check('Audio levels are independently adjustable and master mute silences both buses',
            muted['music_volume']==1 and muted['sound_volume']==8 and muted['muted'] and
            muted['sound_master']==0 and muted['music_output']==0,muted)
    t.tap(t.B);t.step(0,4);t.tap(t.SELECT);t.step(0,12);t.tap(t.R);t.tap(t.R)
    persisted=t.audio_state()
    t.check('Audio choices persist after closing and reopening the panel',
            persisted['music_volume']==1 and persisted['sound_volume']==8 and persisted['muted'],persisted)
    t.tap(t.A)                       # Unmute.
    t.tap(t.UP);t.tap(t.A)            # Reset sound effects.
    t.tap(t.UP);t.tap(t.A)            # Reset music.
    restored_audio=t.audio_state()
    t.check('A restores both buses and unmute reapplies them immediately',
            restored_audio['music_volume']==10 and restored_audio['sound_volume']==10 and
            not restored_audio['muted'] and restored_audio['sound_master']==1 and
            restored_audio['music_output']>0,restored_audio)

    # Attach a real mGBA battery-save file before exercising the third panel.
    # Closing and reopening the core below verifies emulator persistence rather
    # than merely retaining SRAM in one emulator process.
    save_path=t.ROOT/'build/dustline-emulator-test.sav'
    save_path.unlink(missing_ok=True)
    assert t.lib.emulator_load_save(str(save_path).encode())

    def patch_active_save(**fields):
        """Model a newer ROM reading an older, otherwise valid battery save."""
        data=bytearray(save_path.read_bytes());records=[]
        for slot in range(2):
            base=slot*512
            magic,version,payload_size,generation,_checksum,committed=struct.unpack_from('<IHHIII',data,base)
            if magic==0x54535544 and version==1 and 0<payload_size<=256 and committed==0x45564153:
                records.append((generation,base,payload_size))
        assert records
        _generation,base,payload_size=max(records)
        offsets=dict(map_seed=4,map_signature=8,catalog_signature=12,x=16,y=20)
        for name,value in fields.items():struct.pack_into('<I',data,base+20+offsets[name],value&0xffffffff)
        checksum=zlib.crc32(data[base:base+12])
        checksum=zlib.crc32(data[base+20:base+20+payload_size],checksum)&0xffffffff
        struct.pack_into('<I',data,base+12,checksum)
        save_path.write_bytes(data)

    def load_from_title(_map_index):
        assert t.state()['mode']==0 and t.save_state()['valid']
        t.tap(t.DOWN)
        assert t.save_state()['title_selection']==1
        t.tap(t.A)
        for _ in range(150):
            if t.state()['mode']==1:return
            t.step(0,30)
        raise AssertionError('Timed out continuing the saved game')

    # The third panel owns explicit development saves. Start clean, save the
    # baseline, alter it, close mGBA, and load through joypad input after reopen.
    t.tap(t.R)                    # Music -> Save/Load.
    t.tap(t.DOWN);t.tap(t.DOWN);t.tap(t.A)
    confirm=t.save_state();t.capture('settings/save-erase-confirm')
    t.check('Erase Save requires a second confirmation press',
            t.weapon_state()['settings_panel']==3 and confirm['erase_confirm'],confirm)
    t.tap(t.A)
    erased=t.save_state()
    t.check('Confirmed erase invalidates both cartridge slots',
            not erased['valid'] and erased['result']==3,erased)
    t.tap(t.UP);t.tap(t.UP);saved_position=t.state();t.tap(t.A)
    saved=t.save_state();t.capture('settings/save-data')
    t.check('Save Game commits the first versioned SRAM generation',
            saved['valid'] and saved['generation']==1 and saved['result']==1 and
            saved['version']==1 and saved['slot_size']==512,saved)
    t.tap(t.A);alternated=t.save_state()
    t.check('A second save advances onto the redundant SRAM copy',
            alternated['valid'] and alternated['generation']==2 and alternated['result']==1,alternated)

    # mGBA snapshots include cartridge SRAM as well as the live machine. Prove
    # that erasing after a snapshot and then loading it restores the game save.
    snapshot_size=t.lib.emulator_state_size()
    snapshot=(t.C.c_ubyte*snapshot_size)()
    assert snapshot_size and t.lib.emulator_save_state(snapshot,snapshot_size)
    t.tap(t.DOWN);t.tap(t.DOWN);t.tap(t.A);t.tap(t.A)
    snapshot_erased=t.save_state()
    assert not snapshot_erased['valid']
    assert t.lib.emulator_load_state(snapshot,snapshot_size)
    t.step(0,1);snapshot_restored=t.save_state()
    t.check('mGBA save-state restores the in-game SRAM profile',
            snapshot_restored['valid'] and snapshot_restored['generation']==2,snapshot_restored)
    # Loading a core snapshot restores SRAM bytes but does not necessarily mark
    # the separate battery file dirty. A normal game save makes the following
    # close/reopen battery-persistence check independent of that mGBA detail.
    t.tap(t.A);persisted=t.save_state()
    assert persisted['valid'] and persisted['generation']==3

    t.tap(t.L);t.tap(t.L)          # Save page -> Driving page.
    t.tap(t.RIGHT)                   # Change acceleration after the save.
    t.tap(t.R);t.tap(t.LEFT)      # Driving -> Music; change music volume.
    changed=t.state();changed_audio=t.audio_state()
    t.check('Post-save state diverges before reboot',
            changed['tune_acceleration']>saved_position['tune_acceleration'] and
            changed_audio['music_volume']==9,dict(state=changed,audio=changed_audio))

    t.lib.emulator_close()
    t.check('mGBA flushes cartridge SRAM to a battery-save file on exit',
            save_path.exists() and save_path.stat().st_size>=1024,
            dict(path=str(save_path),size=save_path.stat().st_size if save_path.exists() else 0))
    assert t.lib.emulator_open_with_save(str(t.ROOT/'dist/dustline.gba').encode(),str(save_path).encode())
    t.step(0,90)
    after_reset=t.save_state()
    t.check('Cartridge SRAM remains valid after closing and reopening mGBA',
            t.state()['mode']==0 and after_reset['valid'] and after_reset['generation']==3,after_reset)
    t.capture('settings/title-continue')
    t.tap(t.DOWN);continue_selected=t.save_state();t.tap(t.UP)
    t.check('A valid save exposes a selectable Continue Game title entry',
            continue_selected['title_selection']==1 and t.save_state()['title_selection']==0,
            continue_selected)
    t.start_map(0);t.tap(t.SELECT);t.step(0,12);t.tap(t.R);t.tap(t.R);t.tap(t.R);t.tap(t.DOWN);t.tap(t.A)
    for _ in range(150):
        if t.state()['mode']==1:break
        t.step(0,30)
    loaded=t.state();loaded_audio=t.audio_state();loaded_save=t.save_state()
    t.capture('settings/save-loaded')
    t.check('Load Game restores position, handling and audio after reboot',
            loaded['mode']==1 and loaded_save['result']==2 and loaded_save['exact_world'] and
            close(loaded['x'],saved_position['x']) and close(loaded['y'],saved_position['y']) and
            close(loaded['tune_acceleration'],saved_position['tune_acceleration']) and
            loaded_audio['music_volume']==10,
            dict(state=loaded,audio=loaded_audio,save=loaded_save,saved_position=saved_position))

    # A map recipe or world-graph edit must not quietly turn LOAD into NEW GAME.
    # Change only the compatibility fingerprints and repair the slot CRC, as a
    # real battery save from a previous build would contain them.
    t.lib.emulator_close()
    patch_active_save(map_seed=0x31415926,map_signature=0x27182818,catalog_signature=0x16180339)
    assert t.lib.emulator_open_with_save(str(t.ROOT/'dist/dustline.gba').encode(),str(save_path).encode())
    t.step(0,90);load_from_title(1 if len(t.GAME_MAPS)>1 else 0)
    updated=t.state();updated_save=t.save_state();t.capture('settings/save-loaded-updated-world')
    t.check('An older-world save still restores its map and exact driveable position',
            updated_save['result']==7 and not updated_save['exact_world'] and
            not updated_save['position_adjusted'] and updated['map']==saved_position['map'] and
            close(updated['x'],saved_position['x']) and close(updated['y'],saved_position['y']),
            dict(state=updated,save=updated_save,saved_position=saved_position))

    # If updated collision does cover the old point, loading remains successful
    # and explicitly reports that it selected the nearest safe floor instead.
    t.lib.emulator_close();patch_active_save(x=0,y=0)
    assert t.lib.emulator_open_with_save(str(t.ROOT/'dist/dustline.gba').encode(),str(save_path).encode())
    t.step(0,90);load_from_title(1 if len(t.GAME_MAPS)>1 else 0)
    relocated=t.state();relocated_save=t.save_state();t.capture('settings/save-loaded-relocated')
    t.check('A saved point covered by changed collision relocates visibly instead of using the start silently',
            relocated_save['result']==8 and relocated_save['position_adjusted'] and
            relocated['map']==saved_position['map'] and relocated['x']>=8 and relocated['y']>=8,
            dict(state=relocated,save=relocated_save))

    # Put the profile back at its genuine test location before the remaining
    # driving/town checks; those intentionally approach the nearby first town.
    t.lib.emulator_close()
    patch_active_save(x=round(saved_position['x']*4096),y=round(saved_position['y']*4096))
    assert t.lib.emulator_open_with_save(str(t.ROOT/'dist/dustline.gba').encode(),str(save_path).encode())
    t.step(0,90);load_from_title(1 if len(t.GAME_MAPS)>1 else 0)

    t.tap(t.SELECT);t.step(0,12);t.tap(t.R);t.tap(t.R);t.tap(t.R)
    # Title Continue does not visit the in-game LOAD row, so this newly booted
    # session still has SAVE selected. Move across LOAD to ERASE.
    t.tap(t.DOWN);t.tap(t.DOWN);t.tap(t.A);t.tap(t.A)
    final_erase=t.save_state()
    t.check('Erasing after a load removes the persistent profile',
            not final_erase['valid'] and final_erase['result']==3,final_erase)
    save_path.unlink(missing_ok=True)
    t.tap(t.L);t.tap(t.L)          # Save page -> Driving page.

    t.check('Weapon fitting is absent from the field settings panels',
            t.weapon_state()['settings_panel']==1 and t.weapon_state()['mask']==1,t.weapon_state())

    # The CAR row is the last Handling entry; every body uses the fitted garage weapons.
    t.tap(t.UP)
    for expected,name in enumerate(('sand-buggy','old-car','truck','pickup'),1):
        t.tap(t.RIGHT);selected=t.weapon_state()
        t.check(f'Handling panel selects the {name} body',selected['car_type']==expected,selected)
        t.tap(t.B);t.step(0,4);t.capture(f'settings/car-{name}')
        t.tap(t.SELECT);t.step(0,12);t.tap(t.R)
    t.tap(t.A)
    t.check('A restores the original roadster on the CAR row',t.weapon_state()['car_type']==0,t.weapon_state())
    t.tap(t.UP);standard=t.weapon_state();t.tap(t.RIGHT);large=t.weapon_state()
    t.capture('settings/battery-large')
    t.check('Handling panel equips a larger battery with a mass tradeoff',
            standard['max_energy']==100 and large['max_energy']==150 and
            t.combat_state()['player_mass']==opened['tune_mass']+200,
            dict(standard=standard,large=large,combat=t.combat_state()))
    t.tap(t.A)
    t.check('A restores the standard 100-energy battery',t.weapon_state()['max_energy']==100,t.weapon_state())
    t.tap(t.DOWN);t.tap(t.DOWN)

    base=PRESETS[opened['setup']]
    t.check('Handling lab starts from the fitted preset',
            close(opened['tune_acceleration'],base[0]) and close(opened['tune_max_speed'],base[1]) and
            close(opened['tune_grip'],base[2]) and close(opened['tune_steer'],base[3]) and
            close(opened['tune_coast'],base[4]) and close(opened['tune_brake'],base[5]) and
            opened['tune_mass']==base[6],opened)

    # Verify individual steps in both directions before exercising the broad ranges.
    t.tap(t.RIGHT);acc=t.state()
    t.tap(t.DOWN);t.tap(t.RIGHT);speed=t.state()
    t.tap(t.DOWN);t.tap(t.LEFT);grip=t.state()
    t.tap(t.DOWN);t.tap(t.RIGHT);steer=t.state()
    t.tap(t.DOWN);t.tap(t.RIGHT);coast=t.state()
    t.tap(t.DOWN);t.tap(t.LEFT);brake=t.state()
    t.tap(t.DOWN);t.tap(t.RIGHT);mass=t.state()
    t.check('Every handling property supports manual increase and decrease',
            close(acc['tune_acceleration'],base[0]+0.005) and
            close(speed['tune_max_speed'],base[1]+0.1) and
            close(grip['tune_grip'],base[2]-0.005) and
            close(steer['tune_steer'],base[3]+0.1) and
            close(coast['tune_coast'],base[4]+0.001) and
            close(brake['tune_brake'],base[5]-0.01) and mass['tune_mass']==base[6]+100,mass)

    # Held Left/Right repeats quickly enough to reach intentionally extreme test values.
    t.tap(t.A)
    for _ in range(6):t.tap(t.UP)
    hold(t.RIGHT,220)
    t.tap(t.DOWN);hold(t.RIGHT,200)
    t.tap(t.DOWN);hold(t.RIGHT,400)
    t.tap(t.DOWN);hold(t.RIGHT,210)
    t.tap(t.DOWN);hold(t.RIGHT,130)
    t.tap(t.DOWN);hold(t.RIGHT,100)
    t.tap(t.DOWN);hold(t.RIGHT,210)
    extreme=t.state();t.capture('settings/handling-extremes')
    t.check('Handling lab exposes very wide clamped test ranges',
            close(extreme['tune_acceleration'],0.5) and close(extreme['tune_max_speed'],12) and
            close(extreme['tune_grip'],1) and close(extreme['tune_steer'],12) and
            close(extreme['tune_coast'],0.05) and close(extreme['tune_brake'],0.3) and
            extreme['tune_mass']==10000,extreme)
    hold(t.LEFT,240);minimum=t.state()
    t.check('Mass can also be reduced across a wide range',minimum['tune_mass']==100,minimum)

    # Reset, make a small custom tune, and verify it survives normal scene changes.
    t.tap(t.A)
    t.tap(t.DOWN);t.tap(t.DOWN);t.tap(t.DOWN);t.tap(t.RIGHT)  # Skip BAT/CAR; ACC +0.005.
    t.tap(t.UP);t.tap(t.UP);t.tap(t.UP);t.tap(t.LEFT)   # Skip CAR/BAT; MASS -100.
    custom=t.state();t.capture('settings/handling-custom')
    t.tap(t.B);t.step(0,12);driving=t.state()
    t.check('Closing the lab applies the custom values to the player car',driving['mode']==1 and
            close(driving['tune_acceleration'],base[0]+0.005) and
            driving['tune_mass']==base[6]-100 and t.combat_state()['player_mass']==base[6]-100,driving)
    t.tap(t.START);t.step(0,20);t.tap(t.START);resumed=t.step(0,10)
    t.check('Pause and resume preserve the custom handling tune',
            close(resumed['tune_acceleration'],base[0]+0.005) and resumed['tune_mass']==base[6]-100,resumed)

    for _ in range(360):
        if t.step(t.A)['mode']==3:break
    assert t.state()['mode']==3
    t.tap(t.UP);t.tap(t.A);t.step(0,12)
    town=t.state();assert town['mode']==4
    t.tap(t.SELECT);inside=t.step(0,12)
    t.check('Select menu also opens in town without loading driving graphics',inside['mode']==6 and
            t.weapon_state()['settings_panel']==0 and inside['tile_capacity']==0 and
            inside['tune_mass']==base[6]-100,inside)
    t.tap(t.SELECT);t.step(0,10)
    t.step(t.DOWN,18);t.tap(t.A);returned=t.step(0,45)
    t.check('Town round trip preserves the custom handling tune',returned['mode']==1 and
            close(returned['tune_acceleration'],base[0]+0.005) and returned['tune_mass']==base[6]-100,returned)

    # Do not leak an extreme or custom test tune into the remaining combat suites.
    t.tap(t.SELECT);t.step(0,12);t.tap(t.R);t.tap(t.A);t.tap(t.B);final=t.step(0,12)
    t.check('A restores all fitted preset values',close(final['tune_acceleration'],base[0]) and
            close(final['tune_max_speed'],base[1]) and close(final['tune_grip'],base[2]) and
            close(final['tune_steer'],base[3]) and close(final['tune_coast'],base[4]) and
            close(final['tune_brake'],base[5]) and final['tune_mass']==base[6],final)
    t.check('The open field menu remains stable after its one-time map build',
            frozen['missed']==opened['missed'],dict(opened=opened['missed'],settled=frozen['missed']))


if __name__=='__main__':
    import test_rom as t
    assert t.lib.emulator_open(str(t.ROOT/'dist/dustline.gba').encode())
    t.step(0,90)
    try:run(t)
    finally:
        t.lib.emulator_close()
        (t.OUT/'settings/test-results.json').write_text(json.dumps(dict(
            rom_sha256=t.hashlib.sha256((t.ROOT/'dist/dustline.gba').read_bytes()).hexdigest(),checks=t.checks),indent=2))
    if not all(c['passed'] for c in t.checks):raise SystemExit(1)
