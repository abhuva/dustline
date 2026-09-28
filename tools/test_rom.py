"""Integration tests of the compiled GBA ROM, using headless mGBA.

Only joypad input is written. Telemetry is read from the ELF symbol address.
Run through test.ps1 after build.ps1.
"""
import ctypes as C
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
import zlib
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'artifacts'
OUT.mkdir(exist_ok=True)
MAP_LIBRARY=json.loads((ROOT/'maps/map-library.json').read_text())
GAME_MAPS=[entry for entry in MAP_LIBRARY['maps'] if entry['includeInGame']]
MAP_COUNT=len(GAME_MAPS)
WORLD_START_ID=MAP_LIBRARY['world']['start']['map']
WORLD_START_INDEX=next(i for i,entry in enumerate(GAME_MAPS) if entry['id']==WORLD_START_ID)
A,B,SELECT,START,RIGHT,LEFT,UP,DOWN,R,L=[1<<i for i in range(10)]
NM='/opt/devkitpro/devkitARM/bin/arm-none-eabi-nm'
symbols=subprocess.check_output([NM,str(ROOT/'dustline.elf')],text=True)
address=int(next(line.split()[0] for line in symbols.splitlines() if line.endswith(' dustline_telemetry')),16)
combat_address=int(next(line.split()[0] for line in symbols.splitlines() if line.endswith(' dustline_combat_telemetry')),16)
weapon_address=int(next(line.split()[0] for line in symbols.splitlines() if line.endswith(' dustline_weapon_telemetry')),16)
town_address=int(next(line.split()[0] for line in symbols.splitlines() if line.endswith(' dustline_town_telemetry')),16)
mission_address=int(next(line.split()[0] for line in symbols.splitlines() if line.endswith(' dustline_mission_telemetry')),16)
portal_address=int(next(line.split()[0] for line in symbols.splitlines() if line.endswith(' dustline_portal_telemetry')),16)
race_address=int(next(line.split()[0] for line in symbols.splitlines() if line.endswith(' dustline_race_telemetry')),16)
music_address=int(next(line.split()[0] for line in symbols.splitlines() if line.endswith(' dustline_music_telemetry')),16)
progression_address=int(next(line.split()[0] for line in symbols.splitlines() if line.endswith(' dustline_progression_telemetry')),16)
save_address=int(next(line.split()[0] for line in symbols.splitlines() if line.endswith(' dustline_save_telemetry')),16)
radio_address=int(next(line.split()[0] for line in symbols.splitlines() if line.endswith(' dustline_radio_telemetry')),16)
settings_address=int(next(line.split()[0] for line in symbols.splitlines() if line.endswith(' dustline_settings_telemetry')),16)
shop_address=int(next(line.split()[0] for line in symbols.splitlines() if line.endswith(' dustline_shop_telemetry')),16)
lib=C.CDLL(str(ROOT/'build/emulator_bridge.so'))
lib.emulator_open.argtypes=[C.c_char_p]
lib.emulator_open.restype=C.c_int
lib.emulator_open_with_save.argtypes=[C.c_char_p,C.c_char_p]
lib.emulator_open_with_save.restype=C.c_int
lib.emulator_load_save.argtypes=[C.c_char_p]
lib.emulator_load_save.restype=C.c_int
lib.emulator_state_size.restype=C.c_size_t
lib.emulator_save_state.argtypes=[C.c_void_p,C.c_size_t]
lib.emulator_save_state.restype=C.c_int
lib.emulator_load_state.argtypes=[C.c_void_p,C.c_size_t]
lib.emulator_load_state.restype=C.c_int
lib.emulator_step.argtypes=[C.c_int,C.c_int]
lib.emulator_read.argtypes=[C.c_uint32]
lib.emulator_read.restype=C.c_int32
lib.emulator_pixels.restype=C.c_void_p
lib.emulator_reset.argtypes=[]

def step(keys=0,frames=1):
    lib.emulator_step(keys,frames)
    return state()

def state():
    v=[lib.emulator_read(address+4*i) for i in range(66)]
    return dict(magic=v[0],frame=v[1],mode=v[2],setup=v[3],
                x=v[4]/4096,y=v[5]/4096,vx=v[6]/4096,vy=v[7]/4096,
                heading=v[8]/4096,slip=v[9]/4096,surface=v[10],
                lap_frames=v[11],best=v[12],laps=v[13],gate=v[14],
                collisions=v[15],cpu=v[16]/4096,missed=v[17],camera_x=v[18],camera_y=v[19],
                vblank=v[20]/4096,uploaded_bytes=v[21],bg_bytes=v[22],sprite_free_bytes=v[23],
                map=v[24],width=v[25],height=v[26],chunk=v[27],tile_capacity=v[28],unique_tiles=v[29],
                renderer_working_ram_bytes=v[30],chunk_loads=v[31],chunk_decodes=v[32],chunk_cache_bytes=v[33],
                minimap_x=v[34],minimap_y=v[35],seed=v[36]&0xffffffff,signature=v[37]&0xffffffff,
                generations=v[38],town=v[39],town_x=v[40],town_y=v[41],layout_bytes=v[42],
                scratch_bytes=v[43],free_ewram=v[44],floor_cells=v[45],generation_updates=v[46],town_visits=v[47],
                radar_x=v[48],radar_y=v[49],radar_revisions=v[50],zoom_level=v[51],radar_scale=v[52],
                loading_progress=v[53],tune_acceleration=v[54]/4096,
                tune_max_speed=v[55]/4096,tune_grip=v[56]/4096,
                tune_steer=v[57]/4096,tune_mass=v[58],tune_selection=v[59],
                tune_coast=v[60]/4096,tune_brake=v[61]/4096,
                material_id=v[62],material_kind=v[63],shown_material=v[64],
                terrain_rumble=v[65]/4096)

def capture(name):
    size=lib.emulator_pixel_size()
    raw=C.string_at(lib.emulator_pixels(),240*160*size)
    if size!=4:
        raise RuntimeError(f'Expected 32-bit mGBA pixel buffer, got {size}')
    im=Image.frombytes('RGBA',(240,160),raw).convert('RGB')
    im.save(OUT/(name+'.png'))
    im.resize((960,640),Image.Resampling.NEAREST).save(OUT/(name+'-4x.png'))
    return im

checks=[]

def combat_state():
    v=[lib.emulator_read(combat_address+4*i) for i in range(311)]
    result=dict(zip(('magic','ticks','hp','player_hits','player_shots','enemy_shots','hits','kills',
                    'wall_hits','expired','living','bullets','ram','graphics','invulnerability','collisions',
                    'avoidance','recoveries'),v[:18]))
    result['simulation_cpu']=v[18]/4389
    result['view_cpu']=v[19]/4389
    result['bumps']=v[176]; result['last_bump']=v[177]/4096; result['player_mass']=v[178]
    result['last_pair']=v[179]; result['player_bumps']=v[180]
    result['enemies']=[dict(x=v[a]/4096,y=v[a+1]/4096,vx=v[a+2]/4096,vy=v[a+3]/4096,
                           heading=v[a+4]/4096,hp=v[a+5],collisions=v[a+6],reverse=v[a+7],
                           avoidance=v[a+8],recoveries=v[a+9],explosion=v[a+10],flash=v[a+11])
                       for a in range(20,80,12)]
    for i,e in enumerate(result['enemies']):
        a=184+i*9
        e.update(vehicle_senses=v[a],moving_frames=v[a+1],maneuver=v[a+2],side=v[a+3],
                 mass=v[a+4],goal_x=v[a+5],goal_y=v[a+6],stalled=v[a+7],spawn_id=v[a+8])
    result['projectiles']=[dict(x=v[a]/4096,y=v[a+1]/4096,remaining=v[a+2],hostile=bool(v[a+3]))
                           for a in range(80,176,4) if v[a+2]]
    result.update(spawn_count=v[229],spawned=v[230],despawned=v[231],respawn_delay=v[232],
                   spawn_address=v[233],spawn_stride=v[234],spawn_range=v[235],despawn_range=v[236],
                   shield=v[237],shield_delay=v[238],invulnerability=v[239],invincible=bool(v[239]))
    result['passengers']=[dict(x=v[a]/4096,y=v[a+1]/4096,vx=v[a+2]/4096,vy=v[a+3]/4096,
                               heading=v[a+4]/4096,hp=v[a+5],collisions=v[a+6],reverse=v[a+7],
                               avoidance=v[a+8],recoveries=v[a+9],explosion=v[a+10],flash=v[a+11],
                               cell=v[a+12],target_town=v[a+13],serial=v[a+14],moving_frames=v[a+15])
                          for a in range(240,288,16)]
    for i,passenger in enumerate(result['passengers']):
        a=296+i*5
        passenger.update(dwell=v[a],turnaround=v[a+1],contact_pause=v[a+2],
                         avoid_side=v[a+3],blocked_frames=v[a+4])
    result.update(passenger_living=v[288],passengers_spawned=v[289],passengers_despawned=v[290],
                  passengers_killed=v[291],passenger_max_hp=v[292],passenger_spawn_range=v[293],
                  passenger_retention_range=v[294],passenger_hard_range=v[295])
    return result

def weapon_state():
    v=[lib.emulator_read(weapon_address+4*i) for i in range(82)]
    return dict(selected=v[0],saw=bool(v[1]),saw_x=v[2]/4096,saw_y=v[3]/4096,
                guidance=v[4],explosions=v[5],mask=v[6],settings_panel=v[7],
                garage_slot=v[74],car_type=v[75],energy=v[76],max_energy=v[77],shots=v[8:17],hits=v[17:26],
                front=v[78],side=v[79],special=v[80],inventory_open=bool(v[81]),
                missiles=[dict(x=v[a]/4096,y=v[a+1]/4096,vx=v[a+2]/4096,vy=v[a+3]/4096,
                               remaining=v[a+4],age=v[a+5],target=v[a+6],heading=v[a+7]/4096,explosion=v[a+8])
                          for a in (26,35)],
                traps=[dict(x=v[a]/4096,y=v[a+1]/4096,remaining=v[a+2],arm=v[a+3],explosion=v[a+4])
                       for a in range(44,74,5)])

def settings_state():
    v=[lib.emulator_read(settings_address+4*i) for i in range(8)]
    return dict(magic=v[0],panel=v[1],map_selection=v[2],map_selection_count=v[3],
                map_selection_kind=v[4],map_selection_index=v[5],tuning_selection=v[6],
                open=bool(v[7]))

def town_state():
    v=[lib.emulator_read(town_address+4*i) for i in range(10)]
    return dict(magic=v[0],place=v[1],x=v[2],y=v[3],direction=v[4],
                menu=bool(v[5]),selection=v[6],town=v[7],prompt=bool(v[8]),
                player_visible=bool(v[9]))

def mission_state():
    v=[lib.emulator_read(mission_address+4*i) for i in range(18)]
    return dict(magic=v[0],type=v[1],status=v[2],origin=v[3],target_town=v[4],
                target_spawn=v[5],target_x=v[6],target_y=v[7],progress=v[8],goal=v[9],
                reward=v[10],credits=v[11],completed=v[12],serial=v[13],
                board=bool(v[14]),selection=v[15],origin_map=v[16],target_map=v[17])

def portal_state():
    v=[lib.emulator_read(portal_address+4*i) for i in range(12)]
    return dict(magic=v[0],portal_count=v[1],current_portal=v[2],ignored_portal=v[3],
                visits=v[4],yes=bool(v[5]),prompt=bool(v[6]),destination_map=v[7],
                destination_side=v[8],portal_x=v[9],portal_y=v[10],route_exit=v[11])

def race_state():
    v=[lib.emulator_read(race_address+4*i) for i in range(80)]
    return dict(magic=v[0],kind=v[1],phase=v[2],outcome=v[3],closed=bool(v[4]),
                origin=v[5],target_town=v[6],checkpoint_count=v[7],next_checkpoint=v[8],
                time_limit=v[9],time_left=v[10],elapsed=v[11],score=v[12],reward=v[13],
                earned=v[14],off_course=v[15],no_progress=v[16],menu=bool(v[17]),
                selection=v[18],start_x=v[19],start_y=v[20],
                checkpoints=[(v[21+i*2],v[22+i*2]) for i in range(max(0,min(16,v[7])))],
                serial=v[53],credits=v[54],world_linger_checkpoint=v[55],
                world_linger_frames=v[56],world_gate_visible=bool(v[57]),
                world_left_flag_visible=bool(v[58]),world_right_flag_visible=bool(v[59]),
                radar_gate_visible=bool(v[60]),world_display_checkpoint=v[61])

def audio_state():
    v=[lib.emulator_read(music_address+4*i) for i in range(16)]
    return dict(magic=v[0],playing=bool(v[1]),active=v[2],target=v[3],position=v[4],
                downgrade=v[5],bpm=v[6],sections=v[7],music_volume=v[8],sound_volume=v[9],
                muted=bool(v[10]),selection=v[11],sound_master=v[12]/4096,
                music_output=v[13]/4096,engine_volume=v[14]/4096,engine_pitch=v[15]/4096)

def progression_state():
    v=[lib.emulator_read(progression_address+4*i) for i in range(16)]
    return dict(magic=v[0],scrap=v[1],owned=v[2],shop_notice=v[3],shop_count=v[4],
                collected_scrap=v[5],collected_blueprints=v[6],menu_page=v[7],
                shop_selection=v[8],active_pickups=v[9],collected_energy=v[10],
                loadout_slot=v[11],inventory_open=bool(v[12]),inventory_selection=v[13],
                info_open=bool(v[14]),shop_info_open=bool(v[15]))

def shop_state():
    v=[lib.emulator_read(shop_address+4*i) for i in range(62)]
    towns=[]
    for town in range(6):
        offset=2+town*10
        towns.append(v[offset+1:offset+1+v[offset]])
    return dict(magic=v[0],map=v[1],towns=towns)

def save_state():
    v=[lib.emulator_read(save_address+4*i) for i in range(13)]
    return dict(magic=v[0],valid=bool(v[1]),generation=v[2]&0xffffffff,selection=v[3],
                result=v[4],erase_confirm=bool(v[5]),version=v[6],slot_size=v[7],
                exact_world=bool(v[8]),catalog_signature=v[9]&0xffffffff,return_mode=v[10],
                position_adjusted=bool(v[11]),title_selection=v[12])

def radio_state():
    v=[lib.emulator_read(radio_address+4*i) for i in range(32)]
    return dict(magic=v[0],fitted=bool(v[1]),target=v[2],direction=v[3],strength=v[4],
                active=v[5],barrel_visible=bool(v[6]),indicator_visible=bool(v[7]),map=v[8],
                detection_radius=v[9],desired=v[10],next_respawn=v[11],clock=v[12],
                target_x=v[13],target_y=v[14],
                sources=[dict(active=bool(v[16+i*3]),x=v[17+i*3],y=v[18+i*3])
                         for i in range(5)])

def spawn_state():
    c=combat_state(); points=[]
    for i in range(c['spawn_count']):
        address=c['spawn_address']+i*c['spawn_stride']
        xy=lib.emulator_read(address)&0xffffffff
        timer=lib.emulator_read(address+4); flags=lib.emulator_read(address+8)&0xffffffff
        slot=flags&255
        points.append(dict(x=xy&65535,y=xy>>16,ready_at=timer,slot=slot if slot<128 else slot-256,
                           hp=(flags>>8)&255,profile=(flags>>16)&255,reward_rolls=(flags>>24)&255))
    return points

def combat_pixel_mask(s):
    if s['mode']!=1: return lambda x,y:False
    c=combat_state(); boxes=[]
    for e in c['enemies']:
        if e['hp'] or e['explosion']:
            x=e['x']-s['camera_x']+120; y=e['y']-s['camera_y']+80
            boxes.append((x-20,y-26,x+20,y+20))
    for passenger in c['passengers']:
        if passenger['hp'] or passenger['explosion']:
            x=passenger['x']-s['camera_x']+120;y=passenger['y']-s['camera_y']+80
            boxes.append((x-16,y-16,x+16,y+16))
    for b in c['projectiles']:
        x=b['x']-s['camera_x']+120; y=b['y']-s['camera_y']+80
        boxes.append((x-12,y-12,x+12,y+12))
    return lambda x,y:any(l<=x<=r and top<=y<=bottom for l,top,r,bottom in boxes)
def check(name,condition,details=None):
    checks.append(dict(name=name,passed=bool(condition),details=details))
    print(('PASS' if condition else 'FAIL')+' '+name,details or '',flush=True)

def tap(key):
    step(0,2)
    step(key,2)
    return step(0,2)

def reset():
    # Select now opens settings. Restart tests through the actual map menu.
    selected=state()['map']
    if state()['mode']==1: tap(START)
    if state()['mode']==2: tap(SELECT)
    start_map(selected)
    step(0,48)
    return state()

def _wait_for_driving():
    for _ in range(2400):
        current=step(0,1)
        if current['mode']==1:return current
    capture('loading-failed')
    raise RuntimeError('World loading did not finish')

def _persistent_id(text):
    value=2166136261
    for byte in text.encode('ascii'):
        value=((value^byte)*16777619)&0xffffffff
    return value

def _patch_navigation_save(path,index):
    data=bytearray(path.read_bytes())
    recipe=GAME_MAPS[index]['recipe']
    spawn=next((item for item in recipe['playerSpawns'] if item['id']=='start'),
               recipe['playerSpawns'][0])
    patched=0
    for slot in (0,512):
        if len(data)<slot+512:continue
        magic,version,payload_size,_generation,_checksum,committed=struct.unpack_from('<IHHIII',data,slot)
        if magic!=0x54535544 or version not in (1,2) or committed!=0x45564153 or not 0<payload_size<=256:continue
        payload=slot+20
        struct.pack_into('<IIIiii',data,payload,
                         _persistent_id(GAME_MAPS[index]['id']),0,0,
                         int(spawn['x'])*4096,int(spawn['y'])*4096,int(spawn.get('heading',0))*4096)
        crc=zlib.crc32(data[slot:slot+12]+data[payload:payload+payload_size])&0xffffffff
        struct.pack_into('<I',data,slot+12,crc);patched+=1
    if not patched:raise RuntimeError('Could not create navigation save')
    path.write_bytes(data)

def start_map(index):
    assert state()['mode']==0
    if index==WORLD_START_INDEX:
        if save_state()['title_selection']:tap(UP)
        tap(A)
        return _wait_for_driving()

    # Regions are no longer individually selectable by players. ROM tests that
    # need a particular compiled region create a genuine save through the UI,
    # rewrite only its saved location with a valid CRC, then use Continue.
    path=ROOT/'build/test-navigation.sav';lib.emulator_close();path.unlink(missing_ok=True)
    assert lib.emulator_open_with_save(str(ROOT/'dist/dustline.gba').encode(),str(path).encode())
    step(0,90);tap(A);_wait_for_driving();tap(SELECT);step(0,8)
    for _ in range(3):tap(R)
    tap(A)
    if not save_state()['valid']:raise RuntimeError('Navigation profile was not saved')
    lib.emulator_close();_patch_navigation_save(path,index)
    assert lib.emulator_open_with_save(str(ROOT/'dist/dustline.gba').encode(),str(path).encode())
    step(0,90);tap(DOWN);tap(A)
    _wait_for_driving()
    # Continue deliberately identifies the loaded region/position for three
    # seconds. Let that player-facing confirmation expire before callers make
    # pixel-perfect assertions about the unobstructed driving scene.
    step(0,184)
    return state()

def patch_profile(path,scrap=500,credits=5000,owned=0,front=0,side=5,special=5):
    """Patch a controller-created SRAM profile, retaining its real world identity and CRC."""
    data=bytearray(Path(path).read_bytes())
    patched=0
    for slot in (0,512):
        if len(data)<slot+512:continue
        magic,version,payload_size,generation,checksum,committed=struct.unpack_from('<IHHIII',data,slot)
        if magic!=0x54535544 or version not in (1,2) or committed!=0x45564153 or not 0<payload_size<=256:
            continue
        payload=slot+20
        struct.pack_into('<i',data,payload+78,scrap)
        struct.pack_into('<i',data,payload+159,credits)
        struct.pack_into('<BBB',data,payload+100,front,side,special)
        if version==1:
            struct.pack_into('<II',data,payload+179,owned,0x53484f50)
        else:
            words=data[payload+179]
            if not 1<=words<=4:raise RuntimeError(f'Invalid shop ownership word count {words}')
            struct.pack_into('<I',data,payload+180,owned)
            for index in range(1,words):struct.pack_into('<I',data,payload+180+index*4,0)
        crc=zlib.crc32(data[slot:slot+12]+data[payload:payload+payload_size])&0xffffffff
        struct.pack_into('<I',data,slot+12,crc)
        patched+=1
    if not patched:raise RuntimeError(f'No valid Dustline save slot in {path}')
    Path(path).write_bytes(data)

def load_test_profile(path,**values):
    """Create a save through the UI, fund/own requested gear, then load it through the UI."""
    path=Path(path);path.unlink(missing_ok=True)
    lib.emulator_close()
    assert lib.emulator_open_with_save(str(ROOT/'dist/dustline.gba').encode(),str(path).encode())
    step(0,90);start_map(0);tap(SELECT);step(0,8)
    for _ in range(3):tap(R)
    tap(A)
    if not save_state()['valid']:
        raise RuntimeError(f'Controller-created test profile was not saved: '
                           f'state={state()} settings={settings_state()} save={save_state()}')
    lib.emulator_close();patch_profile(path,**values)
    assert lib.emulator_open_with_save(str(ROOT/'dist/dustline.gba').encode(),str(path).encode())
    step(0,90);tap(DOWN);tap(A)
    for _ in range(2400):
        if state()['mode']==1:break
        step(0,1)
    if state()['mode']!=1:raise RuntimeError('Funded test profile did not load')
    step(0,4)  # Release the load-menu A press and clear the gameplay input guard.
    return state()

def angle_delta(target,current):
    return (target-current+180)%360-180


def check_scene_pixels(name,art):
    s=state(); step()
    combat_mask=combat_pixel_mask(s)
    actual=Image.frombytes('RGBA',(240,160),C.string_at(lib.emulator_pixels(),240*160*4)).convert('RGB')
    matches=[]
    cx=s['x']-s['camera_x']+120; cy=s['y']-s['camera_y']+80
    for py in range(32,132,5):
        for px in range(3,238,5):
            if abs(px-cx)<30 and abs(py-cy)<30 or px>174 and py>94: continue
            if combat_mask(px,py): continue
            expected=art.getpixel((s['camera_x']-120+px,s['camera_y']-80+py))
            matches.append(max(abs(a-b) for a,b in zip(actual.getpixel((px,py)),expected))<=8)
    check(name,sum(matches)/len(matches)>=.99,sum(matches)/len(matches))

def main():
    assert lib.emulator_open(str(ROOT/'dist/dustline.gba').encode())
    s=step(0,90)
    title_reference=capture('title')
    check('Boots to the New Game title menu',s['magic']==0x44555354 and s['mode']==0 and
          not save_state()['valid'] and save_state()['title_selection']==0,s)
    tap(DOWN)
    check('Continue is unavailable when no valid save exists',save_state()['title_selection']==0,save_state())

    # Observe generation one frame at a time so the loading UI and its exported
    # progress value are verified while the generator is still running.
    step(0,2);step(A,1)
    loading=[];loading_capture=False
    for _ in range(2000):
        s=step(0,1)
        if s['mode']==5:
            loading.append(s['loading_progress'])
            if not loading_capture and 25<=s['loading_progress']<=75:
                capture('loading-progress');loading_capture=True
        elif s['mode']==1:
            break
    check('New Game shows monotonic full-load progress',loading_capture and len(set(loading))>=10 and
          all(a<=b for a,b in zip(loading,loading[1:])) and s['mode']==1 and s['loading_progress']==100,
          dict(frames=len(loading),minimum=min(loading) if loading else None,
               maximum=max(loading) if loading else None,distinct=len(set(loading))))
    tap(START);tap(SELECT)
    returned_title=capture('title-returned')
    check('Returning from a map preserves every baked title pixel',
          list(title_reference.crop((0,0,240,40)).getdata())==
          list(returned_title.crop((0,0,240,40)).getdata()))

    observed=[]
    for index,entry in enumerate(GAME_MAPS):
        s=start_map(index);s=step(0,12)
        observed.append((s['seed'],s['signature']))
        check(f'Enabled region {index} uses its saved fixed seed',s['map']==index and
              s['seed']==entry['recipe']['seed'] and s['width']==8192 and s['height']==8192,
              dict(id=entry['id'],state=s))
        if index+1<MAP_COUNT:
            tap(START);tap(SELECT)
    tap(START);tap(SELECT)
    first=start_map(0);first_signature=first['signature']
    tap(START);tap(SELECT);repeat=start_map(0)
    check('Starting a fresh game reproduces its generated world',repeat['seed']==GAME_MAPS[0]['recipe']['seed'] and
          repeat['signature']==first_signature,dict(first=first_signature,repeat=repeat['signature']))
    check('Enabled regions contain distinct generation setups',len(set(observed))>1,observed)

    start=state();s=step(A,60)
    check('Throttle accelerates after New Game',math.hypot(s['vx'],s['vy'])>1.0 and
          math.dist((s['x'],s['y']),(start['x'],start['y']))>40,s)
    lead=math.dist((s['camera_x'],s['camera_y']),(s['x'],s['y']))
    check('Camera opens space in the driving direction',lead>20,dict(lead=lead,state=s))
    tap(START);before=state();after=step(A|LEFT,60)
    check('Pause freezes the procedural driving scene',after['mode']==2 and
          all(before[key]==after[key] for key in ('x','y','heading','lap_frames')),after)
    tap(START)

    from test_wasteland import run
    import sys
    run(sys.modules[__name__])
    from test_settings import run as settings_tests
    settings_tests(sys.modules[__name__])
    from test_combat import run as combat_tests
    combat_tests(sys.modules[__name__])
    from test_traffic import run as traffic_tests
    traffic_tests(sys.modules[__name__])
    from test_spawning import run as spawning_tests
    spawning_tests(sys.modules[__name__])
    from test_weapons import run as weapon_tests
    weapon_tests(sys.modules[__name__])
    lib.emulator_close()


if __name__=='__main__':
    try:
        main()
    finally:
        report=dict(rom_sha256=hashlib.sha256((ROOT/'dist/dustline.gba').read_bytes()).hexdigest(),
                    emulator='libmGBA (Debian bookworm)',checks=checks)
        (OUT/'test-results.json').write_text(json.dumps(report,indent=2),encoding='utf8')
        start=next((i for i,c in enumerate(checks) if c['name']=='Fixed wasteland uses reproducible shared generator'),len(checks))
        (OUT/'wasteland/test-results.json').write_text(json.dumps(dict(report,checks=checks[start:]),indent=2),encoding='utf8')
        combat_start=next((i for i,c in enumerate(checks) if c['name']=='Nearby three-HP enemies spawn on reachable clear floor'),len(checks))
        if combat_start<len(checks):
            (OUT/'combat/test-results.json').write_text(json.dumps(dict(report,checks=checks[combat_start:]),indent=2),encoding='utf8')
        spawning_start=next((i for i,c in enumerate(checks) if c['name']=='World-wide profiled encounter anchors are reachable, sparse and lightweight'),len(checks))
        if spawning_start<len(checks):
            (OUT/'spawning/test-results.json').write_text(json.dumps(dict(report,checks=checks[spawning_start:]),indent=2),encoding='utf8')
        traffic_start=next((i for i,c in enumerate(checks) if c['name']=='Stationary player cannot make enemy drivers permanently park'),len(checks))
        if traffic_start<len(checks):
            (OUT/'traffic/test-results.json').write_text(json.dumps(dict(report,checks=checks[traffic_start:]),indent=2),encoding='utf8')
    if not checks or not all(c['passed'] for c in checks):
        raise SystemExit(1)
