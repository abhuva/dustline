"""Controller-driven checks for reciprocal cardinal region travel."""
import json
import math
import heapq
import test_rom as t
from test_wasteland import Reference


def drive_toward(x,y,stop_on_prompt=True,limit=600):
    for _ in range(limit):
        if stop_on_prompt and t.portal_state()['prompt']:
            return True
        state=t.state()
        if math.dist((state['x'],state['y']),(x,y))<10:
            return False
        target=math.degrees(math.atan2(y-state['y'],x-state['x']))%360
        delta=t.angle_delta(target,state['heading'])
        steering=t.RIGHT if delta>5 else t.LEFT if delta<-5 else 0
        t.step(t.A|steering,1)
    return t.portal_state()['prompt']


def accept_portal():
    assert t.portal_state()['prompt']
    if not t.portal_state()['yes']:
        t.tap(t.LEFT)
    t.tap(t.A)
    for _ in range(2400):
        state=t.step(0,1)
        if state['mode']==1 and not t.portal_state()['prompt']:
            return state
    t.capture('portals/loading-failed')
    raise AssertionError(f'Region loading did not return to driving: {t.state()} / {t.portal_state()}')


def drive_floor_route(reference,x,y):
    state=t.state();start=(int(state['x'])//128,int(state['y'])//128);goal=(x//128,y//128)
    # Portal travel is what this test exercises.  Treat occupied town cells as
    # obstacles so the controller does not accidentally enter a town prompt or
    # steer into its building while following the coarse cell-centre route.
    towns={(tx//128,ty//128) for tx,ty in reference.towns}
    queue=[(0,start)];costs={start:0};parents={start:None}
    while queue:
        cost,(cx,cy)=heapq.heappop(queue)
        if cost!=costs[(cx,cy)]:continue
        if (cx,cy)==goal:break
        for direction,cell in enumerate(((cx,cy-1),(cx+1,cy),(cx,cy+1),(cx-1,cy))):
            if cell in towns:continue
            if reference.wall(*cell):continue
            next_cost=cost+(1 if cell in reference.roads else 20)
            if next_cost<costs.get(cell,1<<30):
                costs[cell]=next_cost;parents[cell]=(cx,cy);heapq.heappush(queue,(next_cost,cell))
    if goal not in parents:return False
    cells=[];cell=goal
    while cell!=start:cells.append(cell);cell=parents[cell]
    for cx,cy in reversed(cells[:-1]):
        target=(cx*128+64,cy*128+64)
        for attempt in range(3):
            if drive_toward(*target,limit=180):return True
            state=t.state()
            # A 128px coarse cell does not require pixel-perfect parking. At
            # sharp bends the car can already be across the shared edge while
            # its momentum makes circling back to the centre unreliable.
            if math.dist((state['x'],state['y']),target)<104:break
            t.step(t.B,35);t.step(0,3)
        else:
            print('Portal route stalled',dict(cell=(cx,cy),target=target,state=t.state()),flush=True)
            return False
        for _ in range(30):
            state=t.state()
            if math.hypot(state['vx'],state['vy'])<0.15:break
            t.step(t.B)
        t.step(0,2)
    # Settle before the precise trigger approach. This matters when a declined
    # portal was just exited at speed and the route starts/ends in the same cell.
    for _ in range(90):
        state=t.state()
        if math.hypot(state['vx'],state['vy'])<0.12:break
        t.step(t.B)
    t.step(0,3)
    for _ in range(5):
        if drive_toward(x,y,limit=600):return True
        t.step(t.B,60);t.step(0,3)
    return t.portal_state()['prompt']


def run():
    (t.OUT/'portals').mkdir(exist_ok=True)
    links=t.MAP_LIBRARY['world']['connections'];by_id={entry['id']:i for i,entry in enumerate(t.GAME_MAPS)}
    sides=('north','east','south','west')
    triggers={'north':(4096,96),'east':(8096,4096),'south':(4096,8096),'west':(96,4096)}
    arrivals={'north':(4096,448,90),'east':(7744,4096,180),'south':(4096,7744,270),'west':(448,4096,0)}
    # Exercise the closest current border from its manual New Game start.
    candidates=[]
    for link in links:
        for source,destination in ((link['a'],link['b']),(link['b'],link['a'])):
            spawn=next(item for item in t.GAME_MAPS[by_id[source['map']]]['recipe']['playerSpawns'] if item['id']=='start')
            candidates.append((math.dist((spawn['x'],spawn['y']),triggers[source['side']]),source,destination))
    _,outward,back=min(candidates)
    origin_map=by_id[outward['map']];destination_map=by_id[back['map']]
    start=t.start_map(origin_map);signature=start['signature'];initial_generation=start['generations']
    reference=Reference(t,start['seed'])
    source_index=sides.index(outward['side']);source=triggers[outward['side']]
    destination_return=triggers[back['side']];destination_arrival=arrivals[back['side']];origin_arrival=arrivals[outward['side']]
    assert drive_floor_route(reference,*source)
    prompt=t.portal_state();t.step(0,1);t.capture('portals/travel-prompt')
    t.check('Entering a derived cardinal trigger opens a destination confirmation',
            prompt['prompt'] and prompt['current_portal']==source_index and prompt['destination_map']==destination_map,prompt)

    t.tap(t.B);declined=t.portal_state();t.step(0,12)
    t.check('Declining suppresses the exit while the car remains inside it',
            t.state()['mode']==1 and declined['ignored_portal']==source_index and not t.portal_state()['prompt'],t.portal_state())
    for _ in range(120):
        if t.portal_state()['ignored_portal']==-1:break
        t.step(t.A,1)
    t.step(t.B,12);t.step(0,3)
    # The car left straight through the trigger, so reversing retraces that
    # exact safe path and avoids a wide low-speed U-turn inside the same cell.
    for _ in range(300):
        if t.portal_state()['prompt']:break
        t.step(t.B)
    if not t.portal_state()['prompt']:
        assert drive_floor_route(reference,*source)
    rearmed=t.portal_state()['ignored_portal']==-1
    t.check('Leaving and returning immediately rearms the same exit',
            rearmed and t.portal_state()['prompt'] and t.portal_state()['current_portal']==source_index,t.portal_state())

    initial_loadout={key:t.weapon_state()[key] for key in ('front','side','special','energy')}
    destination=accept_portal();after=t.portal_state();loadout=t.weapon_state();t.step(0,1);t.capture('portals/arrived-region')
    t.check('Confirming loads the connected region at its derived opposite-side arrival',
            destination['map']==destination_map and after['visits']==1 and destination['generations']==initial_generation+1 and
            math.dist((destination['x'],destination['y']),destination_arrival[:2])<3 and abs(t.angle_delta(destination_arrival[2],destination['heading']))<2,
            dict(state=destination,portal=after))
    t.check('Travel preserves the global fitted loadout and energy',
            all(loadout[key]==value for key,value in initial_loadout.items()),dict(before=initial_loadout,after=loadout))

    t.step(0,3)
    drive_toward(*destination_return,True,300)
    if not t.portal_state()['prompt']:
        raise AssertionError(f'Could not return through destination exit: {t.state()} / {t.portal_state()}')
    returned=accept_portal();after_return=t.portal_state();t.step(0,1);t.capture('portals/returned-region')
    t.check('The reciprocal destination side permits a return trip without using New Game spawn',
            returned['map']==origin_map and after_return['visits']==2 and
            math.dist((returned['x'],returned['y']),origin_arrival[:2])<3 and abs(t.angle_delta(origin_arrival[2],returned['heading']))<2,
            dict(state=returned,portal=after_return))
    t.check('Revisiting regenerates the region from its fixed recipe',
            returned['generations']==initial_generation+2 and returned['signature']==signature,
            dict(first=signature,returned=returned['signature'],generations=returned['generations']))

    # Traverse every authored border in both directions. Together the current
    # L-shaped world exercises north/south and east/west arrivals on real ROM.
    covered=set()
    for link in links:
        source,target=link['a'],link['b'];source_map=by_id[source['map']];target_map=by_id[target['map']]
        if t.state()['mode']==1:t.tap(t.START)
        if t.state()['mode']==2:t.tap(t.SELECT)
        state=t.start_map(source_map);ref=Reference(t,state['seed'])
        assert drive_floor_route(ref,*triggers[source['side']])
        arrived=accept_portal();expected=arrivals[target['side']]
        t.check(f"{link['id']} arrives inward from {target['side']}",
                arrived['map']==target_map and math.dist((arrived['x'],arrived['y']),expected[:2])<3 and
                abs(t.angle_delta(expected[2],arrived['heading']))<2,arrived)
        covered.update((source['side'],target['side']))
        target_ref=Reference(t,arrived['seed'])
        assert drive_floor_route(target_ref,*triggers[target['side']])
        returned=accept_portal();expected=arrivals[source['side']]
        t.check(f"{link['id']} returns inward from {source['side']}",
                returned['map']==source_map and math.dist((returned['x'],returned['y']),expected[:2])<3 and
                abs(t.angle_delta(expected[2],returned['heading']))<2,returned)
    t.check('ROM travel covers all four cardinal sides',covered==set(sides),sorted(covered))


if __name__=='__main__':
    assert t.lib.emulator_open(str(t.ROOT/'dist/dustline.gba').encode())
    t.step(0,90)
    try:
        run()
    finally:
        t.lib.emulator_close()
        folder=t.OUT/'portals';folder.mkdir(exist_ok=True)
        report=dict(rom_sha256=t.hashlib.sha256((t.ROOT/'dist/dustline.gba').read_bytes()).hexdigest(),checks=t.checks)
        (folder/'test-results.json').write_text(json.dumps(report,indent=2))
    if not all(check['passed'] for check in t.checks):raise SystemExit(1)
