"""Spawn streaming acceptance tests: real ROM, read-only telemetry, joypad only."""
from collections import deque
import json
import math
from test_wasteland import Reference

def run(t):
    (t.OUT/'spawning').mkdir(exist_ok=True)
    def fresh():
        if t.state()['mode']==3: t.tap(t.B)
        if t.state()['mode']==1: t.tap(t.START)
        if t.state()['mode']==2: t.tap(t.SELECT)
        t.start_map(0)
    fresh()
    # The long route is calibrated for RALLY. Other suites intentionally leave
    # different player setups selected; use the real town controls to normalize.
    # A slower route can bring five pursuers home and legitimately keep the
    # killed anchor waiting for a free slot, rather than exercise its respawn.
    if t.state()['setup']!=1:
        for _ in range(400):
            if t.step(t.A)['mode']==3: break
        assert t.state()['mode']==3
        t.tap(t.UP); t.tap(t.A)
        assert t.state()['mode']==4
        t.step(t.UP,200);t.tap(t.UP);t.tap(t.A)
        assert t.town_state()['place']==1
        t.step(t.UP,180);t.tap(t.A)
        assert t.town_state()['menu']
        for _ in range(3):
            if t.town_state()['selection']==1: break
            t.tap(t.RIGHT)
        t.tap(t.A)
        assert t.state()['setup']==1
        t.step(t.DOWN,170);t.tap(t.A)
        t.step(t.DOWN,190);t.tap(t.A)
        t.step(0,60); fresh()
    baseline_missed=t.state()['missed']
    original=t.spawn_state(); c=t.combat_state(); ref=Reference(t,t.state()['seed'])
    t.check('Runtime spawn coordinates match the exported recipe',[(p['x'],p['y']) for p in original]==ref.spawns,dict(points=len(original)))
    t.check('World-wide profiled encounter anchors are reachable, sparse and lightweight',len(original)>=32 and
            c['spawn_stride']==12 and c['ram']<5500 and {p['profile'] for p in original}=={0,1,2} and
            all(not ref.wall(p['x']//128,p['y']//128) and ref.surface(p['x'],p['y'])!=3 for p in original),
            dict(points=len(original),profiles=sorted({p['profile'] for p in original}),ram=c['ram'],record_bytes=c['spawn_stride']))
    victim=c['enemies'][0]['spawn_id']
    for _ in range(100):
        t.step(t.R)
        if t.spawn_state()[victim]['hp']==0: break
    killed=t.spawn_state()[victim]; kill_tick=t.combat_state()['ticks']
    t.step(0,2); t.capture('spawning/destroyed')
    profile_delays={0:1800,1:1080,2:3000}; expected_delay=profile_delays[killed['profile']]
    t.check('Kill starts its authored profile cooldown',killed['hp']==0 and killed['slot']==-1 and
            killed['ready_at']-kill_tick==expected_delay,killed)
    peak=0; cooldown_ok=True
    for _ in range(expected_delay+80):
        s=t.step(); c=t.combat_state(); peak=max(peak,s['cpu'])
        p=t.spawn_state()[victim]
        if c['ticks']<killed['ready_at']: cooldown_ok &= p['slot']==-1 and p['hp']==0
    p=t.spawn_state()[victim]; s=t.state()
    t.check('Authored cooldown blocks immediate respawn; visible anchor stays empty after expiry',cooldown_ok and
            c['ticks']>=killed['ready_at'] and abs(p['x']-s['x'])<192 and abs(p['y']-s['y'])<144 and
            p['slot']==-1 and p['hp']==0,dict(point=p,ticks=c['ticks']))
    # Controller-driven journey out of the starting district and back. Returning
    # to an expired killed anchor also proves slot reuse is not a permanent kill.
    start=(int(s['x'])//128,int(s['y'])//128)
    towns={(x//128,y//128) for x,y in ref.towns}
    queue=deque([start]); previous={start:None}; destination=None
    while queue:
        at=queue.popleft(); x,y=at
        if math.dist(at,start)>=22: destination=at; break
        for q in ((x+1,y),(x,y+1),(x-1,y),(x,y-1)):
            if q not in previous and q not in towns and not ref.wall(*q): previous[q]=at; queue.append(q)
    assert destination is not None
    path=[]; at=destination
    while at is not None: path.append(at); at=previous[at]
    path.reverse()
    def route_goals(nodes):
        # Keep turns and endpoints, so a straight road does not force the car
        # to slow at every 128px cell. The exported map lets pursuers keep up
        # with the old per-cell crawl, which never exercised their despawn.
        nodes=list(nodes)
        points=[nodes[i] for i in range(len(nodes)) if i in (0,len(nodes)-1) or
                (nodes[i][0]-nodes[i-1][0],nodes[i][1]-nodes[i-1][1]) !=
                (nodes[i+1][0]-nodes[i][0],nodes[i+1][1]-nodes[i][1])]
        return [(x*128+64,y*128+80) for x,y in points]
    baseline=t.combat_state(); seen=set(); departed=set(); returned=set()
    valid=True; hidden=True; hp_preserved=True; max_live=0; deaths=baseline['kills']; snapshots=[]
    last=t.combat_state(); completed=[]; route_end=[]
    for leg,nodes in enumerate((path[1:],list(reversed(path[:-1])))):
        goals=route_goals(nodes); goal=0
        for frame in range(6500):
            s=t.state(); gx,gy=goals[goal]
            distance=math.dist((s['x'],s['y']),(gx,gy))
            if distance<26:
                goal+=1
                if goal==len(goals): break
                gx,gy=goals[goal]; distance=math.dist((s['x'],s['y']),(gx,gy))
            error=t.angle_delta(math.degrees(math.atan2(gy-s['y'],gx-s['x']))%360,s['heading'])
            keys=t.LEFT if error<-3 else t.RIGHT if error>3 else 0
            target=3.0 if abs(error)<20 and distance>100 else .8
            speed=math.hypot(s['vx'],s['vy'])
            if speed<target: keys|=t.A
            elif speed>target+.15: keys|=t.B
            s=t.step(keys)
            # This suite measures encounter streaming, so recover from the real
            # combat death screen and continue the controller-driven route.
            if s['mode']==7:
                t.tap(t.A);s=t.state()
            c=t.combat_state(); points=t.spawn_state(); peak=max(peak,s['cpu'])
            live=[e for e in c['enemies'] if e['hp']]; max_live=max(max_live,len(live))
            valid &= len(live)<=5 and len({e['spawn_id'] for e in live})==len(live)
            for i,e in enumerate(c['enemies']):
                if e['hp']:
                    p=points[e['spawn_id']]; seen.add(e['spawn_id'])
                    valid &= p['slot']==i and p['hp']==e['hp']
                    if not last['enemies'][i]['hp'] or e['spawn_id']!=last['enemies'][i]['spawn_id']:
                        hidden &= abs(p['x']-s['camera_x'])>140 or abs(p['y']-s['camera_y'])>104
                        if e['spawn_id'] in departed: returned.add(e['spawn_id'])
                old=last['enemies'][i]
                if old['hp'] and (not e['hp'] or old['spawn_id']!=e['spawn_id']):
                    departed.add(old['spawn_id'])
                    hp_preserved &= points[old['spawn_id']]['hp']==old['hp']
            for i,p in enumerate(points):
                if p['slot']>=0: valid &= c['enemies'][p['slot']]['spawn_id']==i and c['enemies'][p['slot']]['hp']>0
            last=c
            if t.state()['mode']==3: t.tap(t.B)
            if frame%300==0: snapshots.append(t.capture(f'spawning/leg-{leg}-{frame:04d}'))
        completed.append(goal==len(goals))
        route_end.append(dict(goal=goal,goals=len(goals),x=s['x'],y=s['y'],heading=s['heading']))
    c=t.combat_state(); p=t.spawn_state()[victim]
    t.capture('spawning/returned')
    t.check('Joypad route traverses distant encounter districts and returns',all(completed),
            dict(completed=completed,path=path,route_end=route_end))
    t.check('Streaming reuses five slots without duplicate point ownership',valid and max_live==5 and len(seen)>5,
            dict(peak_active=max_live,unique_encounters=len(seen),spawned=c['spawned']))
    t.check('Distant cars despawn without counting as kills and retain HP',c['despawned']>baseline['despawned'] and
            c['kills']==deaths and hp_preserved,dict(despawned=c['despawned'],kills=c['kills'],departed=sorted(departed)))
    # The five active slots may all be occupied when the route reaches the
    # destroyed anchor. In that case it remains eligible until a slot frees.
    expired_waiting=p['hp']==0 and p['slot']==-1 and c['ticks']>=p['ready_at']
    respawned=p['hp']==3 and p['slot']>=0
    t.check('Returning reactivates dormant encounters and keeps expired destroyed points eligible',
            bool(returned) and (respawned or expired_waiting),dict(returned=sorted(returned),victim=p))
    t.check('New streamed cars appear outside the viewport',hidden)
    t.check('Spawn streaming fits measured frame budget',peak<1 and t.state()['missed']==baseline_missed,
            dict(cpu=peak,missed=t.state()['missed']-baseline_missed))
    fresh()
    baseline_missed=t.state()['missed']
    t.check('New fixed run resets encounter state but reproduces anchors',
            [(p['x'],p['y']) for p in original]==[(p['x'],p['y']) for p in t.spawn_state()] and
            t.combat_state()['kills']==0 and all(p['hp']==3 and p['ready_at']==0 for p in t.spawn_state()))
    # A single deliberate shot, not just full-health survivors: damage must
    # survive despawning AND the next activation of that same encounter. R only
    # fires the fitted mounts, so the gun-only starting loadout gives the
    # forward target exactly one hit.
    assert t.weapon_state()['front']==0 and t.weapon_state()['side']==5
    victim=t.combat_state()['enemies'][0]['spawn_id']
    t.step(t.R); t.step(0,24); damaged=t.spawn_state()[victim]
    t.check('Single-shot survivor records two HP on its anchor',damaged['hp']==2,damaged)
    t.capture('spawning/damaged')
    # A returning district can legitimately have all five slots occupied. Allow
    # a second circuit to free a slot, then stop when the damaged car activates.
    dormant=False; reactivated=False; peak=0
    for leg,nodes in enumerate((path[1:],list(reversed(path[:-1])))*2):
        goal=0; goals=route_goals(nodes)
        for frame in range(6500):
            s=t.state(); gx,gy=goals[goal]; distance=math.dist((s['x'],s['y']),(gx,gy))
            if distance<26:
                goal+=1
                if goal==len(goals): break
                gx,gy=goals[goal]; distance=math.dist((s['x'],s['y']),(gx,gy))
            error=t.angle_delta(math.degrees(math.atan2(gy-s['y'],gx-s['x']))%360,s['heading'])
            keys=t.LEFT if error<-3 else t.RIGHT if error>3 else 0
            target=3.0 if abs(error)<20 and distance>100 else .8
            speed=math.hypot(s['vx'],s['vy'])
            if speed<target: keys|=t.A
            elif speed>target+.15: keys|=t.B
            s=t.step(keys)
            if s['mode']==7:
                t.tap(t.A);s=t.state()
            peak=max(peak,s['cpu']); p=t.spawn_state()[victim]
            dormant |= p['slot']==-1 and p['hp']==2
            reactivated |= dormant and p['slot']>=0 and p['hp']==2
            if reactivated:break
            if s['mode']==3: t.tap(t.B)
        if reactivated:break
    t.capture('spawning/damaged-return')
    t.check('Damaged survivor despawns and returns with two HP, not healed',dormant and reactivated and
            t.spawn_state()[victim]['hp']==2,dict(dormant=dormant,reactivated=reactivated,point=t.spawn_state()[victim]))
    t.check('Damaged-survivor streaming route stays within frame budget',peak<1 and t.state()['missed']==baseline_missed,
            dict(cpu=peak,missed=t.state()['missed']-baseline_missed))

if __name__=='__main__':
    import test_rom as t
    assert t.lib.emulator_open(str(t.ROOT/'dist/dustline.gba').encode())
    t.step(0,90)
    try: run(t)
    finally:
        t.lib.emulator_close()
        (t.OUT/'spawning/test-results.json').write_text(json.dumps(dict(
            rom_sha256=t.hashlib.sha256((t.ROOT/'dist/dustline.gba').read_bytes()).hexdigest(),checks=t.checks),indent=2))
    if not all(c['passed'] for c in t.checks): raise SystemExit(1)
