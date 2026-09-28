"""Purchased weapon fitting, trigger groups, cadence and energy-HUD acceptance."""
import hashlib
import json


def run(t):
    (t.OUT/'weapons').mkdir(exist_ok=True)
    peak=0
    profile=t.ROOT/'build/dustline-weapons-test.sav'

    def step(keys=0,frames=1):
        nonlocal peak
        for _ in range(frames):
            state=t.step(keys);peak=max(peak,state['cpu'])
        return state

    def fitted(front=0,side=2,special=3):
        t.load_test_profile(profile,owned=0x1ff,front=front,side=side,special=special)
        state=t.weapon_state()
        assert (state['front'],state['side'],state['special'])==(front,side,special),state
        return state

    initial=fitted();baseline_missed=t.state()['missed'];step(t.R);grouped=t.weapon_state()
    friendly=[p for p in t.combat_state()['projectiles'] if not p['hostile']]
    t.check('R fires a purchased front gun and both purchased side guns together',
            grouped['shots']==[1,0,2,0,0,0,0,0,0] and grouped['energy']==initial['energy']-2 and
            len(friendly)==3,grouped)
    side_shots=[p for p in friendly if abs(p['x']-t.state()['x'])<1]
    t.check('The broadside mount fires in opposite perpendicular directions',
            len(side_shots)==2 and sorted(round(p['y']-t.state()['y']) for p in side_shots)==[-20,20],side_shots)

    before=t.combat_state()['ticks'];fittings=t.weapon_state();step(t.B,24);after=t.weapon_state()
    t.check('B brakes without firing a weapon or changing the garage fitting',
            t.combat_state()['ticks']==before+24 and
            (after['front'],after['side'],after['special'])==
            (fittings['front'],fittings['side'],fittings['special']) and
            after['shots']==fittings['shots'],after)

    fitted();before=t.state();step(t.L);special=t.weapon_state();missile=special['missiles'][0]
    t.check('L independently fires the fitted top special without firing normal mounts',
            special['shots'][:3]==[0,0,0] and special['shots'][3]==1 and
            not any(special['shots'][4:]) and special['energy']==92 and missile['remaining']>0 and
            18<missile['x']-before['x']<21 and abs(missile['y']-before['y'])<1,special)
    step(0,7);guided=t.weapon_state()['missiles'][0]
    t.check('The purchased missile launches straight, then acquires a target',
            guided['remaining']>0 and guided['target']>=0 and t.weapon_state()['guidance']>0,
            dict(missile=guided,weapon=t.weapon_state()))
    t.capture('weapons/missile-homing')

    fitted(front=7,side=8,special=5);before=t.state();initial=t.weapon_state();step(t.R)
    forward=t.weapon_state();projectiles=[p for p in t.combat_state()['projectiles'] if not p['hostile']]
    long_shots=[p for p in projectiles if p['remaining']>120]
    short_shots=[p for p in projectiles if 0<p['remaining']<=120]
    t.check('R groups the long sniper with the side-slot twin forward shooter',
            forward['shots'][7]==1 and forward['shots'][8]==2 and
            forward['energy']==initial['energy']-8 and len(long_shots)==1 and len(short_shots)==2,
            dict(weapon=forward,projectiles=projectiles))
    t.check('The side-slot forward shooter places two parallel shots along the car heading',
            len(short_shots)==2 and all(p['x']>before['x'] for p in short_shots) and
            sorted(round(p['y']-before['y']) for p in short_shots)==[-7,7],short_shots)
    step(t.R,74);cadence=t.weapon_state()
    t.check('The high-damage sniper has a deliberately slow 75-frame firing interval',
            cadence['shots'][7]==1 and cadence['shots'][8]>=8,cadence)
    t.check('A sniper hit delivers enough damage to destroy a standard three-HP raider',
            cadence['hits'][7]>=1 and t.combat_state()['kills']>=1,
            dict(weapon=cadence,combat=t.combat_state()))
    step(t.R);cadence=t.weapon_state()
    t.check('The sniper fires again only after its full cooldown',cadence['shots'][7]==2,cadence)
    t.capture('weapons/sniper-forward-shooter')

    fitted();step(t.R|t.L,170);energy=t.weapon_state();image=t.capture('weapons/curved-energy-arc')
    amber=(255,206,66)
    amber_pixels=[(x,y) for y in range(140,157) for x in range(181,238) if image.getpixel((x,y))==amber]
    mirrored=sum((image.getpixel((209-d,y))==amber)==(image.getpixel((209+d,y))==amber)
                 for y in range(140,157) for d in range(1,26))
    t.check('Energy is a centered amber segmented arc along the minimap bottom',
            energy['energy']<70 and amber_pixels and mirrored>=400 and
            min(y for _,y in amber_pixels)>=140,
            dict(energy=energy['energy'],amber_pixels=len(amber_pixels),mirrored=mirrored))

    step();t.tap(t.SELECT);t.step(0,12);frozen=t.weapon_state();step(t.R|t.L|t.B,45)
    after_settings=t.weapon_state()
    gameplay_fields=lambda value:{key:item for key,item in value.items() if key!='settings_panel'}
    t.check('Settings freezes fitted weapons, projectiles and all weapon timers',
            gameplay_fields(after_settings)==gameplay_fields(frozen))
    t.tap(t.B);step(0,3);baseline_missed=t.state()['missed'];t.tap(t.START);frozen=t.weapon_state();step(t.R|t.L|t.B,45)
    t.check('Pause freezes both weapon triggers',t.weapon_state()==frozen)
    t.tap(t.START);step()

    # Starting from the title is a new run, independent of the funded test profile.
    t.tap(t.START);t.tap(t.SELECT);t.start_map(0);reset=t.weapon_state()
    fresh_progress=t.progression_state()
    t.check('A new run restores only the standard front gun and clears attacks',
            (reset['front'],reset['side'],reset['special'])==(0,5,5) and reset['mask']==1 and
            t.state()['setup']==1 and fresh_progress['scrap']==0 and fresh_progress['owned']==0 and
            not any(p['remaining'] for p in reset['traps']) and
            not any(m['remaining'] for m in reset['missiles']),
            dict(weapons=reset,progression=fresh_progress,state=t.state()))
    t.check('Grouped weapon fire fits the measured driving frame budget',
            peak<1 and t.state()['missed']==baseline_missed,
            dict(cpu=peak,missed=t.state()['missed'],ram=t.combat_state()['ram']))


if __name__=='__main__':
    import test_rom as t
    assert t.lib.emulator_open(str(t.ROOT/'dist/dustline.gba').encode())
    t.step(0,90)
    try:run(t)
    finally:
        t.lib.emulator_close()
        (t.OUT/'weapons/test-results.json').write_text(json.dumps(dict(
            rom_sha256=hashlib.sha256((t.ROOT/'dist/dustline.gba').read_bytes()).hexdigest(),checks=t.checks),indent=2))
    raise SystemExit(0 if t.checks and all(c['passed'] for c in t.checks) else 1)
