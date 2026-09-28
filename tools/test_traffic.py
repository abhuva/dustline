"""Hostile and neutral traffic movement, sensing, durability and physical rams."""
import json
import heapq
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
    passenger_serial_frames={}; passenger_moved=False; passenger_route_error=0; maximum_passengers=0
    backward_without_recovery=0;turning_frames=0;turning_slow_frames=0;previous_passengers={}
    for frame in range(1500):
        s=t.step(); c=t.combat_state(); peak=max(peak,s['cpu'])
        for i,(e,old) in enumerate(zip(c['enemies'],previous['enemies'])):
            if not e['hp'] or e['spawn_id']!=old['spawn_id']: idle[i]=0; continue
            moved=math.dist((e['x'],e['y']),(old['x'],old['y']))>.03
            idle[i]=0 if moved else idle[i]+1; max_idle[i]=max(max_idle[i],idle[i])
        # Inscribed circles must never overlap deeply, regardless of heading.
        live_passengers=[p for p in c['passengers'] if p['hp']]
        maximum_passengers=max(maximum_passengers,len(live_passengers))
        for p in live_passengers:
            passenger_serial_frames[p['serial']]=passenger_serial_frames.get(p['serial'],0)+1
            passenger_moved |= p['moving_frames']>90
            anchor=((p['cell']%64)*128+64,(p['cell']//64)*128+104)
            passenger_route_error=max(passenger_route_error,math.dist((p['x'],p['y']),anchor))
            angle=math.radians(p['heading'])
            forward=math.cos(angle)*p['vx']+math.sin(angle)*p['vy']
            backward_without_recovery+=forward<-.08 and not p['reverse']
            old=previous_passengers.get(p['serial'])
            if old and abs(t.angle_delta(p['heading'],old['heading']))>.25:
                turning_frames+=1;turning_slow_frames+=forward<.22
        previous_passengers={p['serial']:p for p in live_passengers}
        cars=[s]+[e for e in c['enemies'] if e['hp']]+live_passengers
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
    t.check('Neutral passengers stream in a bounded three-car pool and keep moving on road routes',
            c['passengers_spawned']>0 and 0<maximum_passengers<=3 and passenger_moved and
            passenger_route_error<230,
            dict(spawned=c['passengers_spawned'],maximum=maximum_passengers,moved=passenger_moved,
                 route_error=passenger_route_error,passengers=c['passengers']))
    t.check('Off-screen passenger identities persist instead of disappearing at the viewport edge',
            passenger_serial_frames and max(passenger_serial_frames.values())>120,
            passenger_serial_frames)
    t.check('Passengers use high neutral durability without changing hostile population semantics',
            c['passenger_max_hp']>=10 and c['passenger_living']==sum(p['hp']>0 for p in c['passengers']) and
            c['living']==sum(e['hp']>0 for e in c['enemies']),
            dict(max_hp=c['passenger_max_hp'],passengers=c['passenger_living'],enemies=c['living']))
    t.check('Civilian braking never leaks into unintended reverse travel',
            backward_without_recovery==0,backward_without_recovery)
    t.check('Route lookahead carries civilians through bends without stop-turn motion',
            turning_frames>30 and turning_slow_frames*4<turning_frames,
            dict(turning_frames=turning_frames,slow=turning_slow_frames))
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
    fresh(); damaged=None
    # Follow the generated floor graph rather than pointing the player through
    # canyon walls at a moving target. This remains a controller-only runtime
    # test: the reference is used solely to choose joypad steering waypoints.
    from test_wasteland import Reference
    reference=Reference(t,t.state()['seed']); waypoints=[]; pursued=None
    towns={(x//128,y//128) for x,y in reference.towns}

    def route(start,goal):
        queue=[(0,start)];costs={start:0};parents={start:None}
        while queue:
            cost,cell=heapq.heappop(queue)
            if cost!=costs[cell]:continue
            if cell==goal:break
            x,y=cell
            for neighbor in ((x,y-1),(x+1,y),(x,y+1),(x-1,y)):
                if neighbor in towns or reference.wall(*neighbor):continue
                next_cost=cost+(1 if neighbor in reference.roads else 14)
                if next_cost<costs.get(neighbor,1<<30):
                    costs[neighbor]=next_cost;parents[neighbor]=cell
                    heapq.heappush(queue,(next_cost,neighbor))
        if goal not in parents:return []
        cells=[];cell=goal
        while cell!=start:cells.append(cell);cell=parents[cell]
        return list(reversed(cells))

    # Keep one civilian inside the retained simulation bubble until it reaches
    # a town, waits, chooses a new destination and completes its visible U-turn.
    endpoint=None;pursued=None;original_target=None;arrived=False;departure_position=None
    waypoints=[];minimum_departure_forward=100
    for frame in range(6500):
        s=t.state()
        if s['mode']==3:
            t.tap(t.B);continue
        c=t.combat_state();available=[p for p in c['passengers'] if p['hp']]
        target=next((p for p in available if p['serial']==pursued),None)
        if target is None and available:
            target=min(available,key=lambda p:math.dist((p['x'],p['y']),reference.towns[p['target_town']]))
            pursued=target['serial'];original_target=target['target_town'];arrived=False
            departure_position=None;waypoints=[];minimum_departure_forward=100
        if target is None:
            t.step();continue
        angle=math.radians(target['heading'])
        forward=math.cos(angle)*target['vx']+math.sin(angle)*target['vy']
        if target['dwell']:
            arrived=True
        if arrived and target['target_town']!=original_target:
            if departure_position is None:departure_position=(target['x'],target['y'])
            minimum_departure_forward=min(minimum_departure_forward,forward)
            if target['turnaround']==0 and forward>.25 and math.dist(
                    departure_position,(target['x'],target['y']))>20:
                endpoint=dict(passenger=target,minimum_forward=minimum_departure_forward,
                              original_target=original_target,frame=frame,state=s)
                t.capture('traffic/passenger-turnaround');break
        start=(int(s['x'])//128,int(s['y'])//128)
        goal=(target['cell']%64,target['cell']//64)
        if frame%36==0 or not waypoints:
            waypoints=route(start,goal)
        while waypoints and math.dist((s['x'],s['y']),
                (waypoints[0][0]*128+64,waypoints[0][1]*128+64))<82:
            waypoints.pop(0)
        passenger_distance=math.dist((s['x'],s['y']),(target['x'],target['y']))
        if goal in towns and passenger_distance<420:
            keys=0
        else:
            if waypoints:
                lookahead=waypoints[min(1,len(waypoints)-1)]
                gx=lookahead[0]*128+64;gy=lookahead[1]*128+64
            else:
                gx=target['x']+target['vx']*8;gy=target['y']+target['vy']*8
            error=t.angle_delta(math.degrees(math.atan2(gy-s['y'],gx-s['x']))%360,s['heading'])
            keys=(t.A if passenger_distance>190 else 0) | (t.LEFT if error<-5 else t.RIGHT if error>5 else 0)
        stepped=t.step(keys);peak=max(peak,stepped['cpu'])
    t.check('Passengers stop in neutral and make a forward U-turn after reaching a town',
            endpoint is not None and endpoint['minimum_forward']>-.08,endpoint)

    fresh();reference=Reference(t,t.state()['seed']);waypoints=[];pursued=None

    for frame in range(2400):
        s=t.state()
        if s['mode']==3:
            t.tap(t.B);continue
        c=t.combat_state(); available=[p for p in c['passengers'] if p['hp'] and
            min(abs(p['cell']%64-town[0])+abs(p['cell']//64-town[1]) for town in towns)>2]
        if not available:
            t.step();continue
        same=next((p for p in available if p['serial']==pursued),None)
        target=same or min(available,key=lambda p:math.dist((s['x'],s['y']),(p['x'],p['y'])))
        if target['serial']!=pursued:
            pursued=target['serial'];waypoints=[]
        start=(int(s['x'])//128,int(s['y'])//128)
        goal=(target['cell']%64,target['cell']//64)
        if frame%36==0 or not waypoints or goal!=waypoints[-1]:
            waypoints=route(start,goal)
        while waypoints and math.dist((s['x'],s['y']),
                (waypoints[0][0]*128+64,waypoints[0][1]*128+64))<82:
            waypoints.pop(0)
        passenger_distance=math.dist((s['x'],s['y']),(target['x'],target['y']))
        if passenger_distance<210:
            gx=target['x']+target['vx']*8;gy=target['y']+target['vy']*8
        elif waypoints:
            lookahead=waypoints[min(1,len(waypoints)-1)]
            gx=lookahead[0]*128+64;gy=lookahead[1]*128+64
        else:
            gx=target['x'];gy=target['y']
        error=t.angle_delta(math.degrees(math.atan2(gy-s['y'],gx-s['x']))%360,s['heading'])
        keys=(t.A if passenger_distance>48 else 0) | (t.LEFT if error<-5 else t.RIGHT if error>5 else 0)
        if passenger_distance<210 and abs(error)<12:keys|=t.R
        t.step(keys);after=t.combat_state()
        hit=next((p for p in after['passengers'] if p['serial']==target['serial'] and
                  0<p['hp']<after['passenger_max_hp']),None)
        if hit:
            damaged=dict(before=target,after=hit,combat=after)
            t.step(0,2);t.capture('traffic/passenger-damaged');break
    damage_details=damaged or dict(state=t.state(),combat=t.combat_state(),pursued=pursued,
                                   waypoints=waypoints[:4])
    t.check('Player fire damages but does not one-shot a passenger car',
            damaged is not None and damaged['after']['hp']>0,damage_details)
    destroyed=None
    if damaged:
        target=damaged['after'];pursued=target['serial'];waypoints=[]
        killed_before=damaged['combat']['passengers_killed']
        for frame in range(1200):
            s=t.state()
            if s['mode']==3:
                t.tap(t.B);continue
            c=t.combat_state()
            target=next((p for p in c['passengers'] if p['serial']==pursued),None)
            if target and target['explosion']:
                destroyed=dict(passenger=target,combat=c,state=s,killed_before=killed_before)
                t.capture('traffic/passenger-destroyed');break
            if not target or not target['hp']:
                t.step();continue
            start=(int(s['x'])//128,int(s['y'])//128)
            goal=(target['cell']%64,target['cell']//64)
            if frame%36==0 or not waypoints or goal!=waypoints[-1]:
                waypoints=route(start,goal)
            while waypoints and math.dist((s['x'],s['y']),
                    (waypoints[0][0]*128+64,waypoints[0][1]*128+64))<82:
                waypoints.pop(0)
            passenger_distance=math.dist((s['x'],s['y']),(target['x'],target['y']))
            if passenger_distance<210:
                gx=target['x']+target['vx']*8;gy=target['y']+target['vy']*8
            elif waypoints:
                lookahead=waypoints[min(1,len(waypoints)-1)]
                gx=lookahead[0]*128+64;gy=lookahead[1]*128+64
            else:
                gx=target['x'];gy=target['y']
            error=t.angle_delta(math.degrees(math.atan2(gy-s['y'],gx-s['x']))%360,s['heading'])
            keys=(t.A if passenger_distance>48 else 0) | (t.LEFT if error<-5 else t.RIGHT if error>5 else 0)
            if passenger_distance<210 and abs(error)<12:keys|=t.R
            s=t.step(keys);peak=max(peak,s['cpu'])
    survived=None
    if destroyed:
        t.step(0,30);survived=t.state()
    t.check('Destroying a passenger shows its pooled burst and safely returns to driving',
            destroyed is not None and destroyed['passenger']['hp']==0 and
            destroyed['passenger']['explosion']>0 and
            destroyed['combat']['passengers_killed']==destroyed['killed_before']+1 and
            survived is not None and survived['mode']==1,
            dict(destroyed=destroyed,survived=survived))
    t.check('A contacted civilian releases its controls instead of continuously pushing',
            destroyed is not None and destroyed['combat']['last_pair'] in (5,6,7) and
            destroyed['passenger']['contact_pause']>0,
            None if destroyed is None else dict(pair=destroyed['combat']['last_pair'],
                                                pause=destroyed['passenger']['contact_pause']))
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
