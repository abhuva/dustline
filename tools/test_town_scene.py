"""Controller-driven vertical-slice test for the walkable town and garage."""
import json
import test_rom as t


def run():
    t.start_map(0)
    fresh=t.weapon_state()
    t.check('A new game owns and fits only the standard front gun',
            (fresh['front'],fresh['side'],fresh['special'])==(0,5,5) and fresh['mask']==1,fresh)
    t.load_test_profile(t.ROOT/'build/dustline-shop-test.sav',scrap=500,credits=5000,owned=0)
    for _ in range(360):
        if t.step(t.A)['mode']==3:
            break
    t.check('Town slice reaches the entry prompt through normal driving',t.state()['mode']==3,t.state())
    t.tap(t.UP);t.tap(t.A);t.step(0,10)
    exterior=t.town_state();t.capture('town/exterior')
    t.check('Town slice starts at the south gate',t.state()['mode']==4 and
            exterior['place']==0 and exterior['x']==128 and exterior['y']==226,exterior)
    t.check('Town south gate shows its animated A prompt',exterior['prompt'],exterior)

    # The redesigned town has a straight, readable public street between its
    # south gate and garage. Rectangular building fronts stop lateral movement.
    t.step(t.UP,100);street=t.town_state()
    t.check('Town central street is open from the south gate',street['x']==128 and
            street['y']==126,street)
    t.check('Interaction prompt stays hidden away from an approach zone',not street['prompt'],street)
    t.step(t.LEFT,80);facade=t.town_state()
    t.check('Town western facade has a rectangular collision edge',90<=facade['x']<=92 and
            facade['y']==126,facade)
    t.step(t.RIGHT,40);t.step(t.UP,120);t.tap(t.DOWN)
    approach=t.town_state();t.capture('town/garage-prompt')
    t.check('Garage approach shows its animated A prompt',approach['prompt'],approach)
    t.tap(t.A);t.step(0,8)
    garage=t.town_state();t.capture('town/garage')
    t.check('Garage opens from its approach box without facing the door',garage['place']==1 and
            garage['x']==128 and garage['y']==228,garage)

    t.step(t.UP,180);t.tap(t.RIGHT);counter=t.town_state()
    t.capture('town/garage-counter-prompt')
    t.check('Garage counter approach shows its animated A prompt',counter['prompt'],counter)
    t.tap(t.A);menu=t.town_state()
    t.capture('town/garage-menu')
    t.check('Garage counter has solid rectangular collision',72<=counter['y']<=78,counter)
    t.check('Mechanic opens without facing the counter',menu['menu'],menu)
    t.tap(t.DOWN);t.capture('town/garage-shop');shop=t.progression_state()
    t.check('Mechanic opens a nine-item direct-purchase shop grid',
            shop['menu_page']==1 and shop['shop_selection']==0 and shop['owned']==0,shop)
    t.tap(t.R);t.capture('town/garage-shop-info');info=t.progression_state()
    t.check('R opens information for the highlighted shop item',info['shop_info_open'],info)
    t.tap(t.R)
    # Purchased stock disappears immediately; repeatedly buying the first
    # packed cell therefore consumes the complete fixed base inventory.
    for expected in range(8,-1,-1):
        before=t.progression_state()
        t.tap(t.A)
        after=t.progression_state()
        t.check('A purchase removes one visible item without an invalid cursor',
                before['shop_count']==expected+1 and after['shop_count']==expected and
                after['shop_selection']<=max(0,expected-1),dict(before=before,after=after))
    purchased=t.progression_state();t.capture('town/garage-shop-purchased')
    t.check('A directly buys every upgrade and weapon using gold plus scrap',
            purchased['owned']==0x1ff and purchased['scrap']==384 and
            t.mission_state()['credits']==2870,purchased)
    t.check('A fully purchased town reports sold out with no selectable cell',
            purchased['shop_count']==0 and purchased['shop_selection']==0,purchased)
    t.tap(t.B)
    t.check('B returns from the shop grid to setup',
            t.progression_state()['menu_page']==0,t.progression_state())
    t.tap(t.B)

    # The parked car is a separate physical interaction. Approach the clear
    # strip immediately to its right from the central garage aisle.
    t.step(t.DOWN,60);t.step(t.LEFT,30)
    fitting_prompt=t.town_state();t.capture('town/garage-fitting-prompt')
    t.check('Parked garage car has its own weapon-fitting A prompt',
            fitting_prompt['prompt'] and fitting_prompt['place']==1,fitting_prompt)
    t.tap(t.A);t.step(0,4);loadout=t.progression_state();weapons=t.weapon_state()
    t.capture('town/garage-weapons')
    t.check('Car trigger loads a separate weapon-fitting scene',
            t.state()['mode']==8 and loadout['menu_page']==2 and loadout['loadout_slot']==0 and
            (weapons['front'],weapons['side'],weapons['special'])==(0,5,5) and weapons['mask']==1,
            dict(state=t.state(),menu=loadout,weapons=weapons))
    t.tap(t.A);opened=t.progression_state();t.capture('town/garage-weapon-inventory')
    t.check('A opens only the compatible inventory for the selected front slot',
            opened['inventory_open'] and opened['inventory_selection']==1,opened)
    t.tap(t.R);info=t.progression_state();t.capture('town/garage-weapon-info')
    t.check('R opens weapon information without changing the fitting',info['info_open'] and
            t.weapon_state()['front']==0,dict(menu=info,weapons=t.weapon_state()))
    t.tap(t.B);t.tap(t.LEFT);t.tap(t.A)
    removed=t.weapon_state()
    t.check('A equips EMPTY from the front inventory and returns to the slot view',
            removed['front']==5 and removed['mask']==0 and not t.progression_state()['inventory_open'],removed)
    t.tap(t.RIGHT);t.tap(t.A);t.tap(t.B)
    t.check('B cancels a compatible inventory without changing the side mount',
            t.weapon_state()['side']==5 and not t.progression_state()['inventory_open'],t.weapon_state())
    t.tap(t.A);t.tap(t.RIGHT);t.tap(t.RIGHT);t.tap(t.A)
    t.check('The side slot accepts the purchased twin forward shooter',
            t.weapon_state()['side']==8,t.weapon_state())
    t.tap(t.RIGHT);t.tap(t.A);t.tap(t.RIGHT);t.tap(t.RIGHT);t.tap(t.A)
    fitted=t.weapon_state();t.capture('town/garage-trap-fitted')
    t.check('The top slot accepts the alternate trap special',
            fitted['special']==4 and fitted['mask']==20,fitted)
    t.tap(t.B);t.step(0,4);returned=t.town_state();t.capture('town/garage-fitting-return')
    t.check('Leaving weapon fitting reloads the garage at the same car trigger',
            t.state()['mode']==4 and returned['place']==1 and returned['x']==fitting_prompt['x'] and
            returned['y']==fitting_prompt['y'],returned)
    t.check('Leaving weapon fitting restores the visible walking player',
            returned['player_visible'],returned)

    # Return to the mechanic through the central aisle.
    t.step(t.RIGHT,30);t.step(t.UP,60);t.tap(t.A)
    t.step(0,4)
    t.tap(t.SELECT);t.step(0,12)
    t.check('Settings can open over the mechanic menu',t.state()['mode']==6,t.state())
    t.tap(t.SELECT);t.capture('town/garage-menu-resumed')
    t.check('Closing settings restores the mechanic menu',t.state()['mode']==4 and
            t.town_state()['menu'],t.town_state())
    original=t.state()['setup'];t.tap(t.RIGHT);t.tap(t.A)
    t.check('Mechanic applies the selected setup',t.state()['setup']==(original+1)%3,t.state())

    t.step(t.DOWN,170);t.tap(t.UP);garage_exit=t.town_state()
    t.capture('town/garage-exit-prompt')
    t.check('Garage exit area shows its animated A prompt',garage_exit['prompt'],garage_exit)
    t.tap(t.A)
    outside=t.town_state()
    t.check('Garage exit works without facing the doorway',outside['place']==0 and
            outside['x']==128 and outside['y']==51,outside)
    t.step(t.DOWN,190);t.tap(t.A);t.step(0,8)
    if t.state()['mode']==4 and t.town_state()['prompt']:
        t.tap(t.A);t.step(0,20)
    t.check('Town south gate restores the overworld',t.state()['mode']==1,t.state())
    before=t.weapon_state();t.tap(t.L);after=t.weapon_state()
    t.check('Garage fittings persist to map play and L deploys the fitted trap',
            (after['front'],after['side'],after['special'])==(5,8,4) and
            after['shots'][4]==before['shots'][4]+1 and any(p['remaining'] for p in after['traps']),after)


if __name__=='__main__':
    assert t.lib.emulator_open(str(t.ROOT/'dist/dustline.gba').encode())
    t.step(0,90)
    try:
        run()
    finally:
        t.lib.emulator_close()
        folder=t.OUT/'town';folder.mkdir(exist_ok=True)
        report=dict(rom_sha256=t.hashlib.sha256((t.ROOT/'dist/dustline.gba').read_bytes()).hexdigest(),checks=t.checks)
        (folder/'test-results.json').write_text(json.dumps(report,indent=2))
    if not all(c['passed'] for c in t.checks): raise SystemExit(1)
