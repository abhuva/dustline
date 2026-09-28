"""Exact exported placement pixels and preview filtering against native tiles."""
import json
import struct
from pathlib import Path
from PIL import Image,ImageChops
root=Path(__file__).resolve().parents[1]
art=json.loads((root/'tools/map_editor/generated/art.json').read_text())
library=json.loads((root/'maps/map-library.json').read_text())
active=next(entry['recipe'] for entry in library['maps'] if entry['includeInGame'])
profiles={profile['id']:profile['color'] for profile in active['spawnProfiles']}
raw=(root/'build/placement-tests/active.bin').read_bytes()
offset=8272+1048576*2+4
decor=raw[offset:offset+1048576]
with Image.open(root/'artifacts/map_editor/populated-full-map.png') as im:
    assert im.size==(8192,8192) and im.mode=='P'
    for y in range(0,8192,23):
        for x in range(0,8192,29):
            at=y//8*1024+x//8;pixel=y%8*8+x%8
            tile=struct.unpack_from('<H',raw,8272+at*2)[0]
            expected=art['decoration'][decor[at]*64+pixel] or art['tiles'][art['refs'][tile]*64+pixel]
            assert im.getpixel((x,y))==expected,(x,y)
    overview=Image.frombytes('RGBA',(2048,2048),(root/'build/placement-tests/populated-overview.rgba').read_bytes()).convert('RGB')
    diff=ImageChops.difference(overview,im.convert('RGB').resize((2048,2048),Image.Resampling.BOX))
    assert max(high for low,high in diff.getextrema())<=1
    overview.save(root/'artifacts/map_editor/populated-overview.png')
    im.crop((3840,3840,4480,4480)).resize((1280,1280),Image.Resampling.NEAREST).save(root/'artifacts/map_editor/populated-detail.png')
with Image.open(root/'artifacts/map_editor/populated-spawns.png') as im:
    count=struct.unpack_from('<I',raw,offset+1048576)[0]
    types=raw[offset+1048576+4+count*4:offset+1048576+4+count*5]
    for i in range(count):
        x,y=struct.unpack_from('<HH',raw,offset+1048576+4+i*4)
        color=profiles.get(types[i],'#f04040')
        rgb=tuple(int(color[at:at+2],16) for at in (1,3,5))
        expected=min(range(len(art['palette'])//3),key=lambda p:sum((art['palette'][p*3+c]-rgb[c])**2 for c in range(3)))
        assert im.getpixel((x,y))==expected
print('PASS decoration PNG pixels, transparent overlays, exact preview filtering and spawn markers.')
