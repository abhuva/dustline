"""Controller-driven checks for the town race office and generated courses."""
import json
import math
import test_rom as t


def run():
    (t.OUT/'races').mkdir(exist_ok=True)
    t.start_map(0)
    for _ in range(360):
        if t.step(t.A)['mode']==3:
            break
    assert t.state()['mode']==3
    t.tap(t.UP);t.tap(t.A);t.step(0,10)

    # The race office mirrors the job office on the east side of the street.
    t.step(t.UP,120);t.step(t.RIGHT,40);t.tap(t.LEFT)
    approach=t.town_state();t.capture('races/race-office-prompt')
    t.check('Separate east-side race office shows its interaction prompt',approach['prompt'],approach)
    t.tap(t.A);town_offer=t.race_state();t.capture('races/town-race-offer')
    t.check('Race office initially offers a hub-to-outer town race',town_offer['menu'] and
            town_offer['kind']==1 and town_offer['origin']==0 and town_offer['target_town']>0 and
            3<=town_offer['checkpoint_count']<=16 and town_offer['time_limit']>0,town_offer)

    t.tap(t.RIGHT);t.step(0,20);wild_offer=t.race_state();t.capture('races/wild-race-offer')
    t.check('Race office also offers a town-free generated wilderness course',wild_offer['kind']==2 and
            wild_offer['target_town']==-1 and wild_offer['checkpoint_count']==16 and
            wild_offer['start_x']>0 and wild_offer['start_y']>0,wild_offer)
    t.tap(t.A)
    for _ in range(60):
        t.step(0)
        if t.state()['mode']==1:
            break
    t.step(0,10);started=t.race_state();t.capture('races/wild-race-start')
    t.check('Starting a wilderness race teleports to its start and begins a countdown',
            t.state()['mode']==1 and started['kind']==2 and started['phase']==2 and
            abs(t.state()['x']-started['start_x'])<4 and abs(t.state()['y']-started['start_y'])<4,
            dict(state=t.state(),race=started))

    while t.race_state()['phase']==2:t.step(0)
    gate_visible=False
    for _ in range(480):
        state=t.state();race=t.race_state();gx,gy=race['checkpoints'][race['next_checkpoint']]
        distance=math.hypot(gx-state['x'],gy-state['y'])
        if distance<105:
            t.step(0,2);t.capture('races/wild-race-gate');gate_visible=True;break
        error=t.angle_delta(math.degrees(math.atan2(gy-state['y'],gx-state['x']))%360,state['heading'])
        keys=t.LEFT if error<-3 else t.RIGHT if error>3 else 0
        if math.hypot(state['vx'],state['vy'])<(1.5 if abs(error)<20 else .7):keys|=t.A
        t.step(keys)
    t.check('The first wilderness gate remains reachable during the extended progress window',
            gate_visible and t.race_state()['phase']==3,
            dict(state=t.state(),race=t.race_state()))

    for _ in range(120):
        race=t.race_state()
        if race['next_checkpoint']>0:
            break
        state=t.state();gx,gy=race['checkpoints'][0]
        error=t.angle_delta(math.degrees(math.atan2(gy-state['y'],gx-state['x']))%360,state['heading'])
        keys=t.LEFT if error<-3 else t.RIGHT if error>3 else 0
        if math.hypot(state['vx'],state['vy'])<(1.5 if abs(error)<20 else .7):keys|=t.A
        t.step(keys)
    linger=t.race_state();t.capture('races/wild-race-gate-linger')
    t.check('Crossing a gate advances the radar immediately but keeps the crossed world gate visible',
            linger['next_checkpoint']==1 and linger['world_linger_checkpoint']==0 and
            0<linger['world_linger_frames']<=60 and linger['world_display_checkpoint']==0 and
            linger['world_gate_visible'] and
            (linger['world_left_flag_visible'] or linger['world_right_flag_visible']) and
            linger['radar_gate_visible'],linger)
    t.step(0,linger['world_linger_frames']-1)
    almost_expired=t.race_state()
    t.check('The crossed world gate remains selected through the last linger frame',
            almost_expired['world_linger_frames']==1 and
            almost_expired['world_display_checkpoint']==0,almost_expired)
    t.step(0)
    expired=t.race_state();t.capture('races/wild-race-gate-expired')
    t.check('The world gate changes after sixty frames without delaying race progress',
            expired['world_linger_frames']==0 and expired['world_display_checkpoint']==1 and
            expired['next_checkpoint']==1 and expired['radar_gate_visible'],expired)

    t.step(0,4);t.tap(t.START);t.step(0,12)
    assert t.state()['mode']==2,t.state()
    t.step(t.B);t.step(0,20)
    returned=t.race_state();t.capture('races/wild-race-aborted')
    t.check('Aborting a wilderness race returns to its originating race office',
            t.state()['mode']==4 and returned['menu'] and returned['phase']==4 and returned['outcome']==4,
            dict(state=t.state(),town=t.town_state(),race=returned))
    t.tap(t.A);t.tap(t.LEFT);town_again=t.race_state()
    t.check('Acknowledging results prepares another town race',town_again['kind']==1 and
            town_again['phase']==1 and town_again['target_town']>0,town_again)
    t.tap(t.A);t.step(0,8)
    accepted=t.race_state()
    t.check('Town race remains ready until the player leaves town',t.state()['mode']==4 and
            accepted['kind']==1 and accepted['phase']==1,accepted)
    t.step(t.LEFT,24);t.step(t.DOWN,140);t.tap(t.DOWN);t.tap(t.A);t.step(0,20)
    if t.state()['mode']==4 and t.town_state()['prompt']:
        t.tap(t.A);t.step(0,20)
    road_start=t.race_state();t.capture('races/town-race-start')
    t.check('Leaving town starts the road-race countdown toward an outer outpost',
            t.state()['mode']==1 and road_start['kind']==1 and road_start['phase']==2 and
            road_start['target_town']>0 and abs(t.state()['x']-road_start['start_x'])<96 and
            abs(t.state()['y']-road_start['start_y'])<96,dict(state=t.state(),race=road_start))


if __name__=='__main__':
    assert t.lib.emulator_open(str(t.ROOT/'dist/dustline.gba').encode())
    t.step(0,90)
    try:
        run()
    finally:
        t.lib.emulator_close()
        report=dict(rom_sha256=t.hashlib.sha256((t.ROOT/'dist/dustline.gba').read_bytes()).hexdigest(),checks=t.checks)
        (t.OUT/'races/test-results.json').write_text(json.dumps(report,indent=2))
    if not all(c['passed'] for c in t.checks):raise SystemExit(1)
