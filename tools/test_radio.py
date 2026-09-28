"""Controller-driven checks for the optional top-slot signal receiver."""
import json
import math
import test_rom as t
from test_portals import drive_floor_route
from test_wasteland import Reference


def enter_fitting_and_equip_radio():
    for _ in range(360):
        if t.step(t.A)['mode']==3:
            break
    assert t.state()['mode']==3
    t.tap(t.UP);t.tap(t.A);t.step(0,8)
    assert t.state()['mode']==4
    # Straight up the public street and through the garage door.
    t.step(t.UP,180);t.tap(t.DOWN);t.tap(t.A);t.step(0,8)
    assert t.town_state()['place']==1
    # Parked car fitting trigger sits left of the central aisle.
    t.step(t.UP,90);t.step(t.LEFT,22);t.tap(t.A);t.step(0,4)
    assert t.state()['mode']==8
    t.tap(t.RIGHT);t.tap(t.RIGHT)       # TOP slot.
    t.tap(t.A)                         # Open compatible inventory at EMPTY.
    t.tap(t.RIGHT);t.tap(t.RIGHT);t.tap(t.RIGHT) # Missile, trap, then radio.
    t.tap(t.A)
    assert t.weapon_state()['special']==6
    t.capture('radio/garage-radio-fitted')
    t.tap(t.B);t.step(0,4)
    # Leave garage and town through their south exits.
    t.step(t.RIGHT,22);t.step(t.DOWN,120);t.tap(t.UP);t.tap(t.A);t.step(0,4)
    assert t.town_state()['place']==0
    t.step(t.DOWN,190);t.tap(t.A);t.step(0,8)
    if t.state()['mode']==4 and t.town_state()['prompt']:
        t.tap(t.A);t.step(0,20)
    assert t.state()['mode']==1,(t.state(),t.town_state())


def line_up_and_shoot(target_x,target_y):
    """Approach at low speed with the nose on target, independent of route heading."""
    for _ in range(420):
        state=t.state()
        distance=math.dist((state['x'],state['y']),(target_x,target_y))
        target=math.degrees(math.atan2(target_y-state['y'],target_x-state['x']))%360
        delta=t.angle_delta(target,state['heading'])
        steering=t.RIGHT if delta>4 else t.LEFT if delta<-4 else 0
        if abs(delta)>4:
            t.step(t.A|steering)
        elif distance<48:
            t.step(t.B)
        elif distance>78:
            t.step(t.A)
        elif math.hypot(state['vx'],state['vy'])>.12:
            t.step(t.B)
        else:
            break
    for _ in range(3):
        t.step(t.R,2);t.step(0,18)
        if t.radio_state()['active']<t.radio_state()['desired']:
            return


def run():
    (t.OUT/'radio').mkdir(exist_ok=True)
    save_path=t.ROOT/'build/dustline-radio-test.sav'
    start=t.load_test_profile(save_path,owned=0x1ff,side=2);reference=Reference(t,start['seed'])
    default=t.radio_state()
    t.check('Signal overlay is absent before an owned receiver is fitted',
            not default['fitted'] and default['target']==-1 and not default['indicator_visible'],default)

    enter_fitting_and_equip_radio();t.step(0,12)
    acquired=t.radio_state();weapons=t.weapon_state();player=t.state()
    active_sources=[source for source in acquired['sources'] if source['active']]
    nearest=min(active_sources,key=lambda source:math.dist((player['x'],player['y']),(source['x'],source['y'])))
    nearest_distance=math.dist((player['x'],player['y']),(nearest['x'],nearest['y']))
    in_range=nearest_distance<=acquired['detection_radius']
    t.capture('radio/receiver-fitted')
    t.check('Garage radio occupies the top slot and creates a bounded random signal pool',
            acquired['magic']==0x52414449 and acquired['fitted'] and weapons['special']==6 and
            3<=acquired['desired']<=5 and acquired['active']==acquired['desired'] and
            len(active_sources)==acquired['desired'] and acquired['detection_radius']==1024 and
            (acquired['target']>=0)==in_range and acquired['indicator_visible']==in_range,
            dict(radio=acquired,weapons=weapons,nearest_distance=nearest_distance))

    before=t.weapon_state();t.tap(t.L);after=t.weapon_state()
    t.check('Passive radio makes L a zero-energy no-op',
            before['energy']==after['energy'] and before['shots']==after['shots'] and
            not any(item['remaining'] for item in after['missiles']) and
            not any(item['remaining'] for item in after['traps']),after)

    target_x,target_y=nearest['x'],nearest['y']
    drive_floor_route(reference,target_x,target_y)
    near=t.radio_state();t.capture('radio/strong-signal-barrel')
    t.check('Approaching strengthens the overlay and streams the salvage barrel',
            near['strength']==3 and near['barrel_visible'] and near['indicator_visible'],
            dict(state=t.state(),radio=near))

    scrap_before=t.progression_state()['scrap']
    for _ in range(3):
        drive_floor_route(reference,target_x,target_y)
        line_up_and_shoot(target_x,target_y);t.step(0,18)
        if t.radio_state()['active']<acquired['desired']:
            break
    collected=t.radio_state();scrap_after=t.progression_state()['scrap']
    t.capture('radio/barrel-collected')
    t.check('A player bullet removes the barrel, awards scrap and schedules a slow replacement',
            collected['active']==acquired['desired']-1 and
            scrap_after-scrap_before==12 and not collected['barrel_visible'] and
            3*60*60-60<=collected['next_respawn']<=5*60*60,
            dict(before=scrap_before,after=scrap_after,radio=collected))

    # Save through the actual field menu, restart the emulator, then load that
    # profile through joypad input. The fitted receiver is persistent, while
    # repeatable signal locations and cooldowns intentionally are not.
    t.tap(t.SELECT);t.step(0,12)
    while t.weapon_state()['settings_panel']!=3:t.tap(t.R)
    while t.save_state()['selection']!=0:t.tap(t.UP)
    t.tap(t.A)
    assert t.save_state()['valid']
    t.lib.emulator_close()
    assert t.lib.emulator_open_with_save(str(t.ROOT/'dist/dustline.gba').encode(),str(save_path).encode())
    t.step(0,90);t.start_map(0);t.tap(t.SELECT);t.step(0,12)
    t.tap(t.R);t.tap(t.R);t.tap(t.R);t.tap(t.DOWN);t.tap(t.A)
    for _ in range(1800):
        if t.state()['mode']==1:break
        t.step(0,1)
    restored=t.radio_state()
    restored_positions=[(source['x'],source['y']) for source in restored['sources'] if source['active']]
    previous_positions=[(source['x'],source['y']) for source in collected['sources'] if source['active']]
    t.check('Normal save/load restores the receiver but starts a fresh repeatable signal pool',
            t.state()['mode']==1 and restored['fitted'] and
            3<=restored['desired']<=5 and restored['active']==restored['desired'] and
            restored['next_respawn']==-1 and restored_positions!=previous_positions and
            t.weapon_state()['special']==6,
            dict(state=t.state(),radio=restored,weapons=t.weapon_state()))


if __name__=='__main__':
    assert t.lib.emulator_open(str(t.ROOT/'dist/dustline.gba').encode())
    t.step(0,90)
    try:
        run()
    finally:
        t.lib.emulator_close()
        report=dict(rom_sha256=t.hashlib.sha256((t.ROOT/'dist/dustline.gba').read_bytes()).hexdigest(),checks=t.checks)
        (t.OUT/'radio/test-results.json').write_text(json.dumps(report,indent=2))
    if not all(check['passed'] for check in t.checks):raise SystemExit(1)
