"""Smoke-test selecting maps backed by different compiled graphics banks."""
import hashlib
import json
import test_rom as t

output=t.OUT/'art-banks'
output.mkdir(exist_ok=True)
assert t.lib.emulator_open(str(t.ROOT/'dist/dustline.gba').encode())
captures=[]
try:
    t.step(0,90)
    for index in range(t.MAP_COUNT):
        state=t.start_map(index);t.step(0,20)
        image=t.capture(f'art-banks/map-{index}')
        captures.append(dict(index=index,map=t.GAME_MAPS[index]['id'],seed=state['seed'],
                             capacity=state['tile_capacity'],unique=state['unique_tiles'],
                             pixels=hashlib.sha256(image.tobytes()).hexdigest()))
        assert 0<state['unique_tiles']<=state['tile_capacity']<=256
        t.tap(t.START);t.tap(t.SELECT)
    assert len({capture['pixels'] for capture in captures})==len(captures)
finally:
    t.lib.emulator_close()
(output/'results.json').write_text(json.dumps(captures,indent=2)+'\n')
print(f'PASS ROM selects {len(captures)} distinct enabled-map art banks and survives repeated scene teardown.')
