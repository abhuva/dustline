"""Moving-traffic sensing, anti-idle manoeuvres, front guns and physical rams."""
import json
import math

def run(t):
    (t.OUT/'traffic').mkdir(exist_ok=True)
    def fresh():
        if t.state()['mode']==3: t.tap(t.B)
        if t.state()['mode']==1: t.tap(t.START)
        if t.state()['mode']==2: t.tap(t.SELECT)
        assert t.state()['mode']==0
        t.start_map(0)
    fresh(); baseline_missed=t.state()['missed']; start=t.combat_state(); previous=start; peak=0; max_idle=[0]*5; idle=[0]*5
    maximum_penetration=0; samples=[]; valid_shots=True; last_shots=start['enemy_shots']
    for frame in range(1500):
        s=t.step(); c=t.combat_state(); peak=max(peak,s['cpu'])
        for i,(e,old) in enumerate(zip(c['enemies'],previous['enemies'])):
            if not e['hp'] or e['spawn_id']!=old['spawn_id']: idle[i]=0; continue
            moved=math.dist((e['x'],e['y']),(old['x'],old['y']))>.03
            idle[i]=0 if moved else idle[i]+1; max_idle[i]=max(max_idle[i],idle[i])
        # Inscribed circles must never overlap deeply, regardless of heading.
        cars=[s]+[e for e in c['enemies'] if e['hp']]
        for i,a in enumerate(cars):
            for b in cars[i+1:]: maximum_penetration=max(maximum_penetration,14-math.dist((a['x'],a['y']),(b['x'],b['y'])))
        if c['enemy_shots']>last_shots:
            # New hostile bullets have moved exactly one frame (6px from muzzle).
            for bullet in c['projectiles']:
                if bullet['hostile'] and bullet['remaining']==114:
                    candidates=[e for e in c['enemies'] if 17<math.dist((e['x'],e['y']),(bullet['x'],bullet['y']))<22]
                    sensed=False
                    for e in candidates:
                        dx=s['x']-e['x']; dy=s['y']-e['y']; angle=math.radians(e['heading'])
                        front=math.cos(angle)*dx+math.sin(angle)*dy
                        side=-math.sin(angle)*dx+math.cos(angle)*dy
                        # Runtime aiming uses integer fixed-point positions before
                        # the shot advances one frame; telemetry exposes rounded
                        # post-step positions. Keep a one-pixel/one-frame margin.
                        sensed |= front>-1 and abs(side)*6<front+14 and math.hypot(dx,dy)<140
                    valid_shots &= sensed
        last_shots=c['enemy_shots']; previous=c
        if frame%60==0: samples.append(t.capture(f'traffic/idle-{frame:04d}'))
    samples[0].resize((480,320)).save(t.OUT/'traffic/idle.gif',save_all=True,
        append_images=[im.resize((480,320)) for im in samples[1:]],duration=1000,loop=0)
    t.check('Stationary player cannot make enemy drivers permanently park',max(max_idle)<180 and
            all(e['moving_frames']-old['moving_frames']>800 for e,old in zip(c['enemies'],start['enemies'])
                if old['hp'] and e['spawn_id']==old['spawn_id']),
            dict(max_idle=max_idle,enemies=c['enemies']))
    t.check('Sensors see the player and other moving cars',sum(e['vehicle_senses'] for e in c['enemies'])>0,c)
    t.check('Rectangle contacts prevent deep car clumping',maximum_penetration<1,maximum_penetration)
    t.check('Enemy front guns fire only with the player ahead and use player range',
            valid_shots and c['enemy_shots']>0 and c['player_hits']>0 and
            all(b['remaining']<=120 for b in c['projectiles']),c)
    t.check('Enemy cars use the weaker lightweight setup',all(e['mass']==750 for e in c['enemies'] if e['hp']) and
            c['player_mass']>750,c)
    fresh(); initial=t.combat_state(); bumped=None
    for frame in range(320):
        s=t.state(); c=t.combat_state(); target=c['enemies'][0]
        gx=target['x']+target['vx']*8; gy=target['y']+target['vy']*8
        error=t.angle_delta(math.degrees(math.atan2(gy-s['y'],gx-s['x']))%360,s['heading'])
        keys=t.A | (t.LEFT if error<-4 else t.RIGHT if error>4 else 0)
        s=t.step(keys); c=t.combat_state(); peak=max(peak,s['cpu'])
        if s['mode']==3: t.tap(t.B)
        if c['player_bumps']>0:
            bumped=dict(state=s,combat=c); t.step(0,2); t.capture('traffic/ram'); break
    t.check('Controller-driven player ram transfers velocity through real car contacts',bumped is not None,bumped)
    t.check('Low-speed ramming does not damage player health',t.combat_state()['hp']==100)
    t.check('Traffic AI and contact solving stay within the measured frame budget',peak<1 and t.state()['missed']==baseline_missed,
            dict(cpu=peak,missed=t.state()['missed']-baseline_missed))

if __name__=='__main__':
    import test_rom as t
    assert t.lib.emulator_open(str(t.ROOT/'dist/dustline.gba').encode())
    t.step(0,90)
    try: run(t)
    finally:
        t.lib.emulator_close()
        (t.OUT/'traffic/test-results.json').write_text(json.dumps(dict(
            rom_sha256=t.hashlib.sha256((t.ROOT/'dist/dustline.gba').read_bytes()).hexdigest(),checks=t.checks),indent=2))
    if not all(c['passed'] for c in t.checks): raise SystemExit(1)
