"""Joypad-only procedural-world and scene-lifecycle regression tests."""
import json
import math
import struct
import subprocess
from collections import deque


class Reference:
    def __init__(self,api,seed):
        from compile_recipe import PARAM_WORDS,compile_recipe
        node_format=f'<{5+PARAM_WORDS}I'
        pack_program=lambda nodes:b''.join(
            struct.pack(node_format,*(value&0xffffffff for value in node)) for node in nodes)
        library=json.loads((api.OUT/'wasteland/active-map-library.json').read_text())
        maps=[entry for entry in library['maps'] if entry['includeInGame']]
        self.recipe=maps[api.state()['map']]['recipe']
        world=compile_recipe(self.recipe)
        folder=api.ROOT/'build';program=folder/'active-world.program';material=folder/'active-ground.program'
        program.write_bytes(pack_program(world))
        material_arg='-'
        if self.recipe.get('materialOutput') is not None:
            ground=compile_recipe(self.recipe,self.recipe['materialOutput'])
            material.write_bytes(pack_program(ground));material_arg=str(material)
        path=api.OUT/'wasteland'/f'active-seed-{seed}.bin'
        placements=[]
        for key in ('spawnOutput','decorationOutput'):
            arg='-'
            if self.recipe.get(key) is not None:
                branch=compile_recipe(self.recipe,self.recipe[key]);file=folder/(key+'.program')
                file.write_bytes(pack_program(branch));arg=str(file)
            placements.append(arg)
        portal_file=folder/'active-exits.bin';map_id=maps[api.state()['map']]['id'];sides=('north','east','south','west');exit_mask=0
        for link in library['world']['connections']:
            if link['a']['map']==map_id:exit_mask|=1<<sides.index(link['a']['side'])
            if link['b']['map']==map_id:exit_mask|=1<<sides.index(link['b']['side'])
        portal_file.write_bytes(bytes([exit_mask]))
        subprocess.run([str(folder/'map_recipe_bridge'),str(program),str(seed),str(path),material_arg,*placements,str(portal_file)],check=True)
        data=path.read_bytes();meta=struct.unpack_from('<20I',data)
        self.info=(64,64,seed,meta[4],meta[5],meta[3],meta[6],meta[7])
        self.columns=64;self.extent=8192;self.seed=seed;self.cells=data[80:4176]
        self.towns=[struct.unpack_from('<2I',data,32+i*8) for i in range(6)]
        self.render_refs=struct.unpack_from('<1048576H',data,8272)
        self.layout_bytes=4168+(4096 if material_arg!='-' else 0)+(4104 if world[-1][0]==16 else 0)
        # Spawn locations retain a byte-sized profile ID beside each XY anchor.
        self.layout_bytes+=(1296 if placements[0]!='-' else 0)+(4120 if placements[1]!='-' else 0)
        offset=8272+1048576*2+4
        self.decoration=data[offset:offset+1048576]
        count=struct.unpack_from('<I',data,offset+1048576)[0]
        self.spawns=[struct.unpack_from('<HH',data,offset+1048576+4+i*4) for i in range(count)]
        art_data=json.loads((api.ROOT/'tools/map_editor/generated/art.json').read_text())
        bank=art_data['banks'][art_data['mapBanks'][maps[api.state()['map']]['id']]]
        self.decoration_art=bank['decoration']
        self.roads=set()
        if world[-1][0]==16:
            towns=[(x//128,y//128) for x,y in self.towns];root=towns[0]
            queue=deque([root]);parents={root:None}
            while queue:
                x,y=at=queue.popleft()
                for d,next_cell in enumerate(((x,y-1),(x+1,y),(x,y+1),(x-1,y))):
                    if next_cell in parents or self.wall(*next_cell):continue
                    if d==0 and at in towns or d==2 and next_cell in towns:continue
                    parents[next_cell]=at;queue.append(next_cell)
            for at in towns[1:]:
                while at is not None:
                    self.roads.add(at);at=parents[at]
            for portal in self.recipe['portals']:
                target=(portal['x']//128,portal['y']//128)
                if target in self.roads:continue
                queue=deque([target]);portal_parents={target:None};joined=None
                while queue and joined is None:
                    x,y=at=queue.popleft()
                    for d,next_cell in enumerate(((x,y-1),(x+1,y),(x,y+1),(x-1,y))):
                        if next_cell in portal_parents or self.wall(*next_cell):continue
                        if d==0 and at in towns or d==2 and next_cell in towns:continue
                        portal_parents[next_cell]=at;queue.append(next_cell)
                        if next_cell in self.roads:
                            joined=next_cell;break
                at=joined
                while at is not None:
                    self.roads.add(at);at=portal_parents[at]
        self.refs=bank['refs'];self.tiles=bytes(bank['tiles']);self.palette=bytes(bank['palette'])

    def wall(self,x,y):
        return not(0<=x<self.columns and 0<=y<self.columns) or self.cells[y*self.columns+x]!=0

    def solid(self,x,y):
        if not(0<=x<self.extent and 0<=y<self.extent): return True
        x=x//8*8+4; y=y//8*8+4
        cx,cy=x//128,y//128; lx,ly=x%128,y%128
        if not self.wall(cx,cy): return False
        for near,dx,dy,ox,oy in [(lx<48 and ly<48,-1,-1,48,48),
                                  (lx>=80 and ly<48,1,-1,80,48),
                                  (lx<48 and ly>=80,-1,1,48,80),
                                  (lx>=80 and ly>=80,1,1,80,80)]:
            if near and not self.wall(cx+dx,cy) and not self.wall(cx,cy+dy):
                return (lx-ox)**2+(ly-oy)**2<=48**2
        return True

    def surface(self,x,y):
        if self.solid(x,y) or any(tx-24<=x<tx+24 and ty-56<=y<ty-8 for tx,ty in self.towns): return 3
        material=self.render_refs[(y//8)*1024+x//8]//16
        return 1 if material in (2,3) else 2

    def getpixel(self,point):
        x,y=point;ref=self.render_refs[(y//8)*1024+x//8]
        color=self.tiles[self.refs[ref]*64+(y%8)*8+x%8]
        decor=self.decoration[(y//8)*1024+x//8]
        if decor:color=self.decoration_art[decor*64+(y%8)*8+x%8] or color
        return tuple(self.palette[color*3:color*3+3])



def check_hud_radar(t,ref,prefix):
    t.step(0,80); s=t.state(); enemies=t.combat_state()['enemies']; t.step()
    combat_mask=t.combat_pixel_mask(s)
    scale=s['radar_scale'];center_x=120+s['minimap_x'];center_y=80+s['minimap_y']
    sprite_left=center_x-32;sprite_top=center_y-32
    image=t.Image.frombytes('RGBA',(240,160),t.C.string_at(t.lib.emulator_pixels(),240*160*4)).convert('RGB')
    def close(actual,expected): return max(abs(a-b) for a,b in zip(actual,expected))<=8
    # Former second HUD row and bottom control strip must now show the world.
    matches=[]
    for y in (17,23,148,155):
        for x in range(8,167,5):
            if combat_mask(x,y): continue
            expected=ref.getpixel((s['camera_x']-120+x,s['camera_y']-80+y))
            matches.append(close(image.getpixel((x,y)),expected))
    t.check(prefix+' removed HUD strips expose actual terrain',sum(matches)/len(matches)>.99,sum(matches)/len(matches))
    # Every terrain pixel is tied permanently to one logical cell. The player
    # dot is a separate sprite, so ignore only its small overlay footprint.
    towns={(x//128,y//128) for x,y in ref.towns}
    colors={1:(24,24,24),2:(224,224,224),3:(112,112,112),4:(248,200,64)}
    matches=[];corners=[];red_pixels=set()
    for e in enemies:
        ex=int(e['x'])//scale-s['radar_x']//scale
        ey=int(e['y'])//scale-s['radar_y']//scale
        if e['hp']>0 and ex*ex+ey*ey<=19**2:
            red_pixels.update((center_x+ex+dx,center_y+ey+dy) for dx in (-1,0) for dy in (-1,0))
    enemy_matches=[]
    for y in range(64):
        for x in range(64):
            sx,sy=sprite_left+x,sprite_top+y
            if not (0<=sx<240 and 0<=sy<160):continue
            if abs(sx-center_x)<4 and abs(sy-center_y)<4:continue
            # The weapon medallion and centered energy bar intentionally replace the
            # clipped pole segments and overlap a few overview pixels.
            if (x-32)**2+(y-(32-25))**2<=9**2 or (abs(x-32)<=22 and abs(y-(32+25))<=4):continue
            if (sx,sy) in red_pixels:
                enemy_matches.append(close(image.getpixel((sx,sy)),(232,88,56)))
                continue
            cx=(s['radar_x']+(x-32)*scale)//128
            cy=(s['radar_y']+(y-32)*scale)//128
            color=4 if (cx,cy) in towns else 2 if ref.wall(cx,cy) else 3 if (cx,cy) in ref.roads else 1
            d=(x-32)**2+(y-32)**2
            if d<=20**2:expected=colors[color]
            elif d>28**2:
                if combat_mask(sx,sy):continue
                expected=ref.getpixel((s['camera_x']-120+sx,s['camera_y']-80+sy))
                corners.append(close(image.getpixel((sx,sy)),expected))
            else:continue
            matches.append(close(image.getpixel((sx,sy)),expected))
    t.check(prefix+' overview matches all logical wall/floor/road cells and town dots',all(matches),sum(matches)/len(matches))
    t.check(prefix+' circular overview corners show the underlying scene',bool(corners) and all(corners),sum(corners)/len(corners))
    t.check(prefix+' living enemies have red dots at their current map coordinates',
            all(enemy_matches),dict(pixels=len(enemy_matches),matched=sum(enemy_matches)))
    t.check(prefix+' image follows the player at the fixed 2x scale without changing logical cells',
            scale==64 and s['zoom_level']==1 and s['radar_x']==int(s['x'])//scale*scale and
            s['radar_y']==int(s['y'])//scale*scale,s)
    t.check(prefix+' player marker stays centered in the compact HUD',
            s['minimap_x']==89 and s['minimap_y']==46,s)



def check_town_marker(t,ref,prefix):
    s=t.state(); t.step()
    image=t.Image.frombytes('RGBA',(240,160),t.C.string_at(t.lib.emulator_pixels(),240*160*4)).convert('RGB')
    tx,ty=ref.towns[s['town']]
    bounds=(tx-32-s['camera_x']+120,ty-64-s['camera_y']+80,tx+32-s['camera_x']+120,ty-s['camera_y']+80)
    matches=[]
    for wy in range(ty-64,ty):
        for wx in range(tx-32,tx+32):
            expected=ref.getpixel((wx,wy))
            # Only actual non-sand icon pixels, not its composited background.
            sand_ref=ref.refs[(wy//8%4)*4+wx//8%4]
            color=ref.tiles[sand_ref*64+(wy%8)*8+wx%8]
            if expected==tuple(ref.palette[color*3:color*3+3]): continue
            x=wx-s['camera_x']+120; y=wy-s['camera_y']+80
            if not(0<=x<240 and 12<=y<112): matches.append(False); continue
            matches.append(max(abs(a-b) for a,b in zip(image.getpixel((x,y)),expected))<=8)
    t.check(prefix+' town artwork remains visible above the entry dialog',len(matches)>200 and
            sum(matches)/len(matches)>.98,dict(bounds=bounds,icon_pixels=len(matches),match=sum(matches)/max(1,len(matches))))


def run(t):
    initial_missed=t.state()['missed']
    def menu():
        if t.state()['mode']==1: t.tap(t.START)
        if t.state()['mode']==2: t.tap(t.SELECT)
        assert t.state()['mode']==0

    def approach():
        for _ in range(360):
            previous_missed=t.state()['missed']
            s=t.step(t.A)
            if s['missed']>previous_missed:
                t.check('Town approach stays inside driving frame budget',False,s)
            if s['mode']==3:
                t.step(0,4)
                return t.state()
        raise RuntimeError('Could not reach the first outpost')

    def visit(prefix):
        before=t.state(); t.capture(f'wasteland/{prefix}-confirm')
        check_town_marker(t,Reference(t,before['seed']),prefix)
        after=t.step(0,45)
        t.check(prefix+' confirmation freezes driving',after['mode']==3 and
                all(before[k]==after[k] for k in ('x','y','heading','lap_frames')),after)
        t.tap(t.UP); t.tap(t.A); town=t.step(0,16)
        t.capture(f'wasteland/{prefix}-town-exterior')
        walking=t.town_state()
        t.check(prefix+' enters a walkable unloaded town scene',town['mode']==4 and
                walking['place']==0 and walking['x']==128 and walking['y']==226 and
                town['tile_capacity']==0 and 32768<=town['bg_bytes']<=45056 and
                town['town_visits']>before['town_visits'],dict(state=town,town=walking))
        t.check(prefix+' town retains run state',all(town[k]==before[k] for k in
                ('seed','signature','generations','x','y','heading')),town)
        original=town['setup'];t.tap(t.R);t.tap(t.L)
        t.check(prefix+' driving controls do not change setup while walking',t.state()['setup']==original,t.state())

        # Follow the new town's straight central street to the north garage.
        t.step(t.UP,200);t.tap(t.UP);t.tap(t.A);t.step(0,8)
        garage=t.town_state();t.capture(f'wasteland/{prefix}-garage')
        t.check(prefix+' garage door loads a separate walkable interior',
                t.state()['mode']==4 and garage['place']==1 and garage['x']==128 and garage['y']==228,garage)

        # The counter stops the player at the mechanic interaction distance.
        t.step(t.UP,180);at_counter=t.town_state();t.tap(t.A);menu=t.town_state()
        t.capture(f'wasteland/{prefix}-garage-menu')
        t.check(prefix+' garage collision stops at the mechanic counter',72<=at_counter['y']<=78,at_counter)
        t.check(prefix+' mechanic opens the setup menu',menu['menu'] and menu['selection']==original,menu)
        t.tap(t.RIGHT);candidate=t.town_state();t.tap(t.A);fitted=t.state()
        expected_setup=(original+1)%3
        t.check(prefix+' garage menu fits the selected vehicle setup',
                candidate['selection']==expected_setup and fitted['setup']==expected_setup and
                not t.town_state()['menu'],dict(candidate=candidate,state=fitted))

        # Leave through both physical doors and return to the preserved car.
        t.step(t.DOWN,170);t.tap(t.A);exterior=t.town_state()
        t.check(prefix+' garage exit returns to the same town',exterior['place']==0 and
                exterior['x']==128 and exterior['y']==51,exterior)
        t.step(t.DOWN,190);t.tap(t.A)
        if t.state()['mode']==4 and t.town_state()['prompt']:
            t.tap(t.A)
        held=t.state();t.step(0,20);after=t.state()
        t.check(prefix+' return restores exact stopped position and same world',after['mode']==1 and
                all(after[k]==before[k]==held[k] for k in ('seed','signature','generations','x','y')) and
                abs(t.angle_delta(after['heading'],before['heading']))>179 and
                abs(t.angle_delta(after['heading'],before['heading']))<181,after)
        t.check(prefix+' return faces the car away from the town',
                abs(t.angle_delta(after['heading'],before['heading']))>179,after)
        t.check(prefix+' chosen vehicle setup survives the town return',after['setup']==expected_setup,after['setup'])
        t.check(prefix+' return restores terrain allocation',after['tile_capacity']==before['tile_capacity'] and
                after['bg_bytes']<before['bg_bytes'] and after['bg_bytes']>4096,after)
        t.step(0,90)
        t.check(prefix+' return does not immediately reopen prompt',t.state()['mode']==1)
        t.capture(f'wasteland/{prefix}-return')
        t.check_scene_pixels(prefix+' restored graphics match procedural reference',Reference(t,after['seed']))
        return after

    menu(); fixed=t.start_map(0); t.capture('wasteland/start')
    info=json.loads((t.OUT/'wasteland/report.json').read_text())
    ref=Reference(t,fixed['seed'])
    t.check('Fixed wasteland uses reproducible shared generator',fixed['seed']==ref.recipe['seed'] and fixed['signature']==ref.info[3],fixed)
    t.check('Recipe generation retains 8k extent and releases its bounded workspace',fixed['width']==8192 and fixed['height']==8192 and
            fixed['layout_bytes']==ref.layout_bytes and fixed['scratch_bytes']==45136 and fixed['free_ewram']>49152, fixed)
    t.check('All possible wasteland artwork fits its small VRAM reservation',
            fixed['tile_capacity']==info['tile_slots'] and fixed['bg_bytes']<=26624 and
            fixed['sprite_free_bytes']>=2048,fixed)
    t.check('Generator yields frames during loading',fixed['generation_updates']>=100,fixed['generation_updates'])
    t.check_scene_pixels('Initial wasteland graphics match procedural reference',ref)
    check_hud_radar(t,ref,'Fixed')
    before=t.state(); t.tap(t.R); t.tap(t.L); after=t.state()
    t.check('Wasteland L/R does not change setup or position outside town',before['setup']==after['setup'] and
            before['x']==after['x'] and before['y']==after['y'],after)
    t.tap(t.START); before=t.state(); t.tap(t.R); t.tap(t.L); after=t.state()
    t.check('Pause menu does not allow vehicle setup changes',after['mode']==2 and before['setup']==after['setup'],after)
    t.tap(t.START)
    approach(); t.step(0,2); before=t.state(); after=t.step(t.B,2)
    # Check the transition while the held-button guard still freezes simulation.
    # Once released, enemy contacts legitimately move the player's stopped car.
    t.check('Declining town entry preserves position at the transition',after['mode']==1 and
            all(after[k]==before[k] for k in ('x','y','seed','generations')),after)
    declined=[t.step(0) for _ in range(60)]
    t.check('Declining town entry suppresses repeat popup after simulation resumes',
            all(s['mode']==1 and s['seed']==before['seed'] and s['generations']==before['generations'] for s in declined),declined[-1])
    # Reverse across the radial boundary, then approach the same town again.
    # The prompt must re-arm at the circle edge rather than after an arbitrary
    # extra cooldown or a large axis-aligned escape distance.
    left_zone=False
    for _ in range(180):
        s=t.step(t.B)
        dx=s['x']-s['town_x'];dy=s['y']-(s['town_y']-32)
        if dx*dx+dy*dy>=74*74:
            left_zone=True;break
    t.check('Leaving the circular town approach immediately rearms it',left_zone,t.state())
    returned_to_circle=False
    for _ in range(240):
        if t.step(t.A)['mode']==3:
            returned_to_circle=True;break
    t.check('Returning across the same town circle immediately shows the prompt',
            returned_to_circle,t.state())
    if returned_to_circle:t.tap(t.B)
    # Restart fixed mode through the map menu; Select now opens settings.
    t.reset(); approach(); returned=visit('fixed')
    t.reset(); approach(); again=visit('repeat')
    t.check('Repeated scene round trip does not leak EWRAM',again['free_ewram']==returned['free_ewram'],
            [returned['free_ewram'],again['free_ewram']])
    menu(); repeat=t.start_map(0)
    t.check('Restarting fixed mode reproduces layout and spawn',all(repeat[k]==fixed[k] for k in
            ('seed','signature','x','y','floor_cells')) and repeat['generations']==again['generations']+1,repeat)

    # Drive to a reachable canyon wall using only normal inputs. The host fixture
    # supplies coarse waypoints; no emulator memory or car position is written.
    start=(int(repeat['x'])//128,int(repeat['y'])//128)
    queue=deque([start]); prev={start:None}; destination=None; wall=None
    while queue:
        p=queue.popleft(); x,y=p
        neighbours=[(x+1,y),(x,y+1),(x-1,y),(x,y-1)]
        walls=[q for q in neighbours if ref.wall(*q)]
        if walls and abs(x-start[0])+abs(y-start[1])>=4:
            destination=p; wall=walls[0]; break
        for q in neighbours:
            if q not in prev and not ref.wall(*q): prev[q]=p; queue.append(q)
    path=[]; p=destination
    while p is not None: path.append(p); p=prev[p]
    path.reverse()
    goals=[(x*128+64,y*128+80) for x,y in path[1:]]
    goals.append((wall[0]*128+64,wall[1]*128+64))
    goal=0; peak_cpu=0; peak_vblank=0; baseline=t.state()['missed']; wall_collisions=None
    for tick in range(5000):
        s=t.state(); gx,gy=goals[goal]
        if math.dist((s['x'],s['y']),(gx,gy))<22 and goal<len(goals)-1:
            goal+=1; gx,gy=goals[goal]
            if goal==len(goals)-1: wall_collisions=s['collisions']
        error=t.angle_delta(math.degrees(math.atan2(gy-s['y'],gx-s['x']))%360,s['heading'])
        keys=t.LEFT if error<-3 else t.RIGHT if error>3 else 0
        if math.hypot(s['vx'],s['vy'])<(1.7 if abs(error)<20 else .75): keys|=t.A
        t.step(keys)
        peak_cpu=max(peak_cpu,s['cpu']); peak_vblank=max(peak_vblank,s['vblank'])
        if s['mode']==3: t.tap(t.B)
        if tick%240==0: t.check_scene_pixels(f'Canyon streamed pixels at frame {tick}',ref)
        # Earlier traffic can push the player into terrain. Require a NEW wall
        # contact on the final approach, not any collision earlier in the route.
        if wall_collisions is not None and s['collisions']>wall_collisions: break
    s=t.state(); t.capture('wasteland/canyon-wall')
    combat=t.combat_state()
    t.check('Pooled canyon pursuit avoids solid ground and repeated crashes',
            1<=combat['living']<=5 and combat['collisions']<60 and
            all(ref.surface(int(e['x']),int(e['y']))!=3 for e in combat['enemies'] if e['hp']),combat)
    t.check('Car traverses connected clearings and collides with canyon wall',goal==len(goals)-1 and
            wall_collisions is not None and s['collisions']>wall_collisions and
            not ref.solid(int(s['x']),int(s['y'])),dict(goal=goal,goals=goals,state=s))
    check_hud_radar(t,ref,'After driving')
    t.check('Wasteland driving meets CPU and VBlank budgets',peak_cpu<1 and peak_vblank<1 and
            s['missed']==baseline,dict(cpu=peak_cpu,vblank=peak_vblank,missed=s['missed'],baseline=baseline))
    before_wall=t.combat_state()['wall_hits']; t.step(t.R,40)
    t.check('Forward gun is blocked by the approached canyon wall',
            t.combat_state()['wall_hits']>before_wall,t.combat_state())

    menu(); alternate=t.start_map(1); t.capture('wasteland/alternate')
    alternate_ref=Reference(t,alternate['seed'])
    t.check('Second catalog entry uses its saved seed and graph',alternate['seed']==alternate_ref.recipe['seed'] and
            alternate['signature']==alternate_ref.info[3] and alternate['signature']!=fixed['signature'],alternate)
    t.check_scene_pixels('Alternate catalog map matches its compiled recipe',alternate_ref)
    check_hud_radar(t,alternate_ref,'Alternate')
    menu(); t.step(0,37); alternate2=t.start_map(1)
    t.check('Reloading the alternate map is deterministic',alternate2['seed']==alternate['seed'] and
            alternate2['signature']==alternate['signature'],alternate2)
    t.check('Generation scratch is released after each catalog load',abs(alternate2['free_ewram']-alternate['free_ewram'])<512,
            [alternate['free_ewram'],alternate2['free_ewram']])
    menu(); restored=t.start_map(0);restored_ref=Reference(t,restored['seed'])
    t.check_scene_pixels('Switching catalog entries restores the selected map art',restored_ref)
    t.check('Catalog switch replaces rather than stacking procedural layouts',restored['layout_bytes']==fixed['layout_bytes'],restored)
    t.check('Town dialogs, resets and scene switches add no missed driving frames',
            t.state()['missed']==initial_missed,dict(before=initial_missed,after=t.state()['missed']))


if __name__=='__main__':
    import test_rom as t
    assert t.lib.emulator_open(str(t.ROOT/'dist/dustline.gba').encode())
    t.step(0,90)
    try:
        run(t)
    finally:
        t.lib.emulator_close()
        report=dict(rom_sha256=t.hashlib.sha256((t.ROOT/'dist/dustline.gba').read_bytes()).hexdigest(),checks=t.checks)
        (t.OUT/'wasteland/test-results.json').write_text(json.dumps(report,indent=2))
    if not all(c['passed'] for c in t.checks): raise SystemExit(1)
