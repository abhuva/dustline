"""Basic combat acceptance checks against the real ROM; joypad writes only."""
import json
import math
from test_wasteland import Reference

def run(t):
    (t.OUT/'combat').mkdir(exist_ok=True)
    def menu():
        if t.state()['mode']==3: t.tap(t.B)
        if t.state()['mode']==1: t.tap(t.START)
        if t.state()['mode']==2: t.tap(t.SELECT)
        assert t.state()['mode']==0
    menu(); t.start_map(0)
    initial=t.combat_state(); ref=Reference(t,t.state()['seed'])
    t.capture('combat/encounter')
    t.check('Nearby three-HP enemies spawn on reachable clear floor',1<=initial['living']<=5 and
            all(e['hp']==3 and not ref.wall(int(e['x'])//128,int(e['y'])//128) and
                ref.surface(int(e['x']),int(e['y']))!=3 for e in initial['enemies'] if e['hp']),initial)
    t.check('Player starts with full health, shield and energy',initial['hp']==100 and initial['shield']==20 and
            t.weapon_state()['energy']==100 and
            not initial['invincible'],initial)
    setup=t.state()['setup']; hp_history=[]; peak=0; first_dead=None; grouped_geometry=True
    for frame in range(100):
        s=t.step(t.R); c=t.combat_state(); peak=max(peak,s['cpu'])
        hp_history.append(c['enemies'][0]['hp'])
        for b in c['projectiles']:
            grouped_geometry &= 0<b['remaining']<=120
            if not b['hostile']:
                dx=b['x']-s['x'];dy=b['y']-s['y']
                grouped_geometry &= 0<dx<=135 and abs(dy)<1
        if frame==8: t.capture('combat/gunfire')
        if c['enemies'][0]['hp']==0 and first_dead is None:
            first_dead=frame
        if first_dead is not None and frame==first_dead+2: t.capture('combat/destruction')
    t.check('R fires forward without changing vehicle setup',t.state()['setup']==setup and c['player_shots']>=8,c)
    weapons=t.weapon_state()
    t.check('The starter gun travels forward within half-screen range and its pool stays bounded',
            grouped_geometry and c['bullets']<=24 and weapons['shots'][0]==10 and
            weapons['shots'][2]==0 and weapons['shots'][3]==0 and c['player_shots']==10,
            dict(combat=c,weapons=weapons))
    t.check('Enemy takes three distinct hits then is destroyed',all(h in hp_history for h in (2,1,0)) and
            c['kills']>=1 and c['hits']>=3 and c['living']<=5,dict(history=hp_history,combat=c))
    progression=t.progression_state()
    t.check('Destroyed raider leaves its deterministic authored scrap and energy pickups',
            progression['active_pickups']>=2,progression)
    t.step(0,200); c=t.combat_state(); t.capture('combat/enemy-fire')
    t.check('Enemies drive toward the player and fire',c['enemy_shots']>0 and
            any(math.dist((e['x'],e['y']),(old['x'],old['y']))>15 for e,old in zip(c['enemies'],initial['enemies']) if e['hp']),c)
    t.check('Enemy bullets deal one point through shield before health',c['player_hits']>0 and
            c['hp']+c['shield']>=120-c['player_hits'] and c['hp']+c['shield']<120,c)
    dead_point=t.spawn_state()[initial['enemies'][0]['spawn_id']]
    t.check('Destroyed encounter remains empty during its cooldown',dead_point['slot']==-1 and
            dead_point['hp']==0 and dead_point['ready_at']>c['ticks'],dead_point)
    t.tap(t.SELECT); frozen=t.combat_state(); t.step(t.R,70); later=t.combat_state()
    t.check('Settings freezes enemies, projectiles and weapon timers',t.state()['mode']==6 and
            all(later[k]==frozen[k] for k in frozen if not k.endswith('_cpu')),later)
    t.step(0,3); t.tap(t.SELECT)
    t.tap(t.START); frozen=t.combat_state(); t.step(t.R,60)
    t.check('Pause freezes combat',all(t.combat_state()[k]==frozen[k] for k in frozen if not k.endswith('_cpu')),
            dict(state=t.state(),before=frozen,after=t.combat_state()))
    t.step(0,3); t.tap(t.START)
    # First outpost is east of spawn. Scene unloading must not resurrect kills.
    for _ in range(360):
        if t.step(t.A)['mode']==3: break
    if t.state()['mode']!=3:
        t.capture('combat/approach-failed')
        raise RuntimeError(f'Town approach failed: {t.state()}')
    saved=t.combat_state(); t.tap(t.UP); t.tap(t.A); t.step(0,10); town=t.combat_state()
    t.check('Town unloads combat graphics and clears travelling bullets',t.state()['mode']==4 and
            town['graphics']==0 and town['bullets']==0,town)
    t.check('Entering town fully restores health, shield and energy',town['hp']==100 and town['shield']==20 and
            t.weapon_state()['energy']==t.weapon_state()['max_energy'],dict(combat=town,weapon=t.weapon_state()))
    t.tap(t.R); t.step(0,20)
    t.check('R in town never fires',t.combat_state()['player_shots']==town['player_shots'])
    # Walk around the town's central island and leave through the south gate.
    t.step(t.DOWN,18); t.tap(t.A)
    t.step(0,60); returned=t.combat_state()
    t.check('Town return preserves enemy deaths and HP',t.state()['mode']==1 and returned['kills']==saved['kills'] and
            [e['hp'] for e in returned['enemies']]==[e['hp'] for e in saved['enemies']] and returned['graphics']==1,returned)
    # Drive into nearby canyon boundaries; enemies must use look-ahead and stay
    # outside solid terrain, with recovery rather than constant repeated impacts.
    solid_samples=0; enemy_frames=0; frames=[]; slow=[]; last_missed=t.state()['missed'];starting_missed=last_missed
    for frame in range(900):
        keys=t.A | (t.LEFT if 180<=frame<270 or 550<=frame<630 else 0) | t.R
        s=t.step(keys); c=t.combat_state(); peak=max(peak,s['cpu'])
        if s['missed']>last_missed or s['cpu']>.9:
            slow.append(dict(frame=frame,state=s,combat={k:c[k] for k in ('ticks','bullets','living','avoidance','enemy_shots','simulation_cpu','view_cpu')}))
        last_missed=s['missed']
        if s['mode']==3: t.tap(t.B)
        for e in c['enemies']:
            if e['hp']:
                enemy_frames+=1
                solid_samples+=ref.surface(int(e['x']),int(e['y']))==3
        if frame%30==0: frames.append(t.capture(f'combat/drive-{frame:03d}'))
    frames[0].resize((480,320)).save(t.OUT/'combat/drive.gif',save_all=True,
        append_images=[im.resize((480,320)) for im in frames[1:]],duration=500,loop=0)
    t.check('AI remains on drivable ground',solid_samples==0,dict(samples=enemy_frames,solid=solid_samples))
    t.check('AI look-ahead is exercised without constant impacts',c['avoidance']>0 and c['collisions']<60,c)
    t.check('Projectiles expire at finite range',c['expired']>0,c)
    t.check('Pooled combat fits the measured frame budget',peak<1 and t.state()['missed']==starting_missed,
            dict(cpu=peak,missed=t.state()['missed']-starting_missed,ram=c['ram'],sprite_free=t.state()['sprite_free_bytes'],slow=slow))
    menu(); t.start_map(1); alternate=t.combat_state()
    t.check('Changing catalog maps starts fresh encounter state',0<=alternate['living']<=5 and
            alternate['spawn_count']>0 and alternate['kills']==0 and alternate['hp']==100 and
            alternate['graphics']==1,alternate)
    menu(); t.start_map(0); restored=t.combat_state()
    t.check('Returning to the first catalog map resets combat state',1<=restored['living']<=5 and
            restored['kills']==0 and restored['player_shots']==0 and restored['graphics']==1 and
            restored['hp']==100 and restored['shield']==20,restored)

    # Turn away from the opening swarm after taking damage. Once no bullet has
    # landed for three seconds, the shield must climb back toward twenty.
    for _ in range(600):
        t.step()
        if t.combat_state()['player_hits']:break
    before=t.combat_state();energy_before=t.weapon_state()['energy'];last=before['shield'];recharged=False
    for frame in range(1000):
        s=t.step(t.A | (t.LEFT if frame<120 else 0));now=t.combat_state()
        recharged |= now['shield']>last and now['shield_delay']==0
        last=now['shield']
        if s['mode']==3:t.tap(t.B)
        if recharged:break
    t.check('Shield begins recharging after three quiet seconds by consuming energy',
            before['shield']<20 and recharged and t.weapon_state()['energy']<energy_before,
            dict(before=before,after=t.combat_state(),energy_before=energy_before,
                 energy_after=t.weapon_state()['energy'],frames=frame+1))

    menu();t.start_map(0)
    for frame in range(3000):
        s=t.step()
        if s['mode']==7:break
    t.step(0,2);wreck=t.combat_state();wreck_state=t.state();t.capture('combat/wrecked')
    t.check('Zero health opens the wrecked screen',wreck_state['mode']==7 and wreck['hp']==0 and
            wreck['shield']==0 and wreck['player_hits']==120,dict(state=wreck_state,combat=wreck))
    x,y=wreck_state['x'],wreck_state['y'];wreck_energy=t.weapon_state()['energy']
    t.step(t.A);revived=t.combat_state();revived_state=t.state()
    t.check('A revives in place with full protection and cleared projectiles',revived_state['mode']==1 and
            revived_state['x']==x and revived_state['y']==y and revived['hp']==100 and revived['shield']==20 and
            revived['invulnerability']==120 and revived['bullets']==0 and
            t.weapon_state()['energy']==wreck_energy,
            dict(state=revived_state,combat=revived,energy=t.weapon_state()['energy']))
    t.step(0,2);t.capture('combat/revived')

if __name__=='__main__':
    import test_rom as t
    assert t.lib.emulator_open(str(t.ROOT/'dist/dustline.gba').encode())
    t.step(0,90)
    try: run(t)
    finally:
        t.lib.emulator_close()
        (t.OUT/'combat/test-results.json').write_text(json.dumps(dict(
            rom_sha256=t.hashlib.sha256((t.ROOT/'dist/dustline.gba').read_bytes()).hexdigest(),checks=t.checks),indent=2))
    if not all(c['passed'] for c in t.checks): raise SystemExit(1)
