"""Original, deterministic pixel art, collision data, and synthesized audio.

Imports map2 through import_map2.py; retains the original procedural course
generator for reference. No commercial game assets are used.
"""
from pathlib import Path
import json
import math
import random
import struct
import wave
from PIL import Image, ImageDraw, ImageOps

ROOT = Path(__file__).resolve().parents[1]
GFX = ROOT / "graphics"
GEN = ROOT / "include/generated"
AUDIO = ROOT / "audio"
for folder in (GFX, GEN, AUDIO):
    folder.mkdir(parents=True, exist_ok=True)

# Index zero is transparent on GBA. Every background uses this same palette.
PALETTE = [
    (12, 20, 28), (24, 35, 43), (39, 51, 57), (57, 69, 70),
    (66, 82, 67), (78, 96, 72), (99, 117, 81), (129, 143, 101),
    (121, 88, 61), (156, 116, 73), (195, 150, 90), (238, 187, 79),
    (222, 230, 206), (47, 123, 134), (91, 191, 184), (190, 77, 52),
]
SPRITE_PALETTE = [
    (255, 0, 255), (12, 20, 28), (27, 40, 49), (45, 65, 73),
    (40, 112, 134), (58, 164, 181), (106, 217, 211), (209, 243, 224),
    (238, 187, 79), (163, 98, 46), (234, 93, 59), (125, 45, 41),
    (222, 230, 206), (113, 143, 128), (81, 99, 91), (57, 72, 69),
]

# A compact original 5x7 font, packed into 8x8 GBA sprite cells.
FONT = {
 'A':['01110','10001','10001','11111','10001','10001','10001'],
 'B':['11110','10001','10001','11110','10001','10001','11110'],
 'C':['01111','10000','10000','10000','10000','10000','01111'],
 'D':['11110','10001','10001','10001','10001','10001','11110'],
 'E':['11111','10000','10000','11110','10000','10000','11111'],
 'F':['11111','10000','10000','11110','10000','10000','10000'],
 'G':['01111','10000','10000','10111','10001','10001','01111'],
 'H':['10001','10001','10001','11111','10001','10001','10001'],
 'I':['111','010','010','010','010','010','111'],
 'J':['00111','00010','00010','00010','10010','10010','01100'],
 'K':['10001','10010','10100','11000','10100','10010','10001'],
 'L':['10000','10000','10000','10000','10000','10000','11111'],
 'M':['10001','11011','10101','10101','10001','10001','10001'],
 'N':['10001','11001','10101','10011','10001','10001','10001'],
 'O':['01110','10001','10001','10001','10001','10001','01110'],
 'P':['11110','10001','10001','11110','10000','10000','10000'],
 'Q':['01110','10001','10001','10001','10101','10010','01101'],
 'R':['11110','10001','10001','11110','10100','10010','10001'],
 'S':['01111','10000','10000','01110','00001','00001','11110'],
 'T':['11111','00100','00100','00100','00100','00100','00100'],
 'U':['10001','10001','10001','10001','10001','10001','01110'],
 'V':['10001','10001','10001','10001','10001','01010','00100'],
 'W':['10001','10001','10001','10101','10101','10101','01010'],
 'X':['10001','10001','01010','00100','01010','10001','10001'],
 'Y':['10001','10001','01010','00100','00100','00100','00100'],
 'Z':['11111','00001','00010','00100','01000','10000','11111'],
 '0':['01110','10001','10011','10101','11001','10001','01110'],
 '1':['010','110','010','010','010','010','111'],
 '2':['01110','10001','00001','00010','00100','01000','11111'],
 '3':['11110','00001','00001','01110','00001','00001','11110'],
 '4':['00010','00110','01010','10010','11111','00010','00010'],
 '5':['11111','10000','10000','11110','00001','00001','11110'],
 '6':['01110','10000','10000','11110','10001','10001','01110'],
 '7':['11111','00001','00010','00100','01000','01000','01000'],
 '8':['01110','10001','10001','01110','10001','10001','01110'],
 '9':['01110','10001','10001','01111','00001','00001','01110'],
 ':':['0','1','0','0','1','0','0'], '.':['0','0','0','0','0','1','0'],
 '-':['000','000','000','111','000','000','000'],
 '#':['01010','11111','01010','01010','11111','01010','00000'],
 '%':['11001','11010','00100','01000','10110','00110','00000'],
 '[':['111','100','100','100','100','100','111'],
 ']':['111','001','001','001','001','001','111'],
 '/':['00001','00001','00010','00100','01000','10000','10000'],
 '+':['000','010','010','111','010','010','000'],
 '>':['100','010','001','0001','001','010','100'],
 '?':['01110','10001','00010','00100','00100','00000','00100'],
}

def img(size, color=0, sprite=False):
    result = Image.new('P', size, color)
    palette = SPRITE_PALETTE if sprite else PALETTE
    result.putpalette([c for rgb in palette for c in rgb] + [0] * (768-48))
    return result

def label(im, x, y, text, color=12, scale=1, spacing=6):
    d = ImageDraw.Draw(im)
    for ch in text.upper():
        for yy, row in enumerate(FONT.get(ch, [])):
            for xx, bit in enumerate(row):
                if bit == '1':
                    d.rectangle((x+xx*scale,y+yy*scale,x+(xx+1)*scale-1,y+(yy+1)*scale-1),fill=color)
        x += spacing * scale

def save(name, im, kind, **options):
    im.save(GFX / (name + '.bmp'))
    (GFX / (name + '.json')).write_text(json.dumps(dict(type=kind, **options)), encoding='utf8')

def smooth(points):
    result = []
    for i in range(len(points)):
        a,b,c,d = [points[j % len(points)] for j in (i-1,i,i+1,i+2)]
        for step in range(16):
            t = step / 16
            result.append(tuple(0.5*((2*b[k])+(-a[k]+c[k])*t+
                (2*a[k]-5*b[k]+4*c[k]-d[k])*t*t+
                (-a[k]+3*b[k]-3*c[k]+d[k])*t*t*t) for k in (0,1)))
    result.append(result[0])
    return result

WAYPOINTS = [(460,860),(760,860),(870,780),(870,650),(760,570),
 (650,650),(510,550),(590,410),(810,340),(860,230),(760,140),
 (510,140),(410,240),(260,240),(150,340),(150,650),(270,740),
 (370,740),(410,800),(360,860)]
PATH = smooth(WAYPOINTS)

def track():
    # Draw at half resolution; 2x2 pixel geometry keeps the tile budget modest.
    road = Image.new('L',(512,512),0)
    rd = ImageDraw.Draw(road)
    line = [(round(x/2),round(y/2)) for x,y in PATH]
    rd.line(line, fill=255, width=34, joint='curve')
    edge = Image.new('L',(512,512),0)
    ImageDraw.Draw(edge).line(line,fill=255,width=39,joint='curve')
    im = img((512,512),5)
    d = ImageDraw.Draw(im)
    # Tile-aligned grass variation repeats instead of consuming unique tiles.
    for y in range(0,512,4):
        for x in range(0,512,4):
            d.point((x+1,y+2), fill=4 if (x//4+y//4)%3 else 6)
    im.paste(10,(0,0),edge)
    im.paste(2,(0,0),road)
    # Dirt sector on the west side; same road shape, different tire response.
    dirt = road.copy()
    dd = ImageDraw.Draw(dirt)
    dd.rectangle((125,0,511,511),fill=0)
    dd.rectangle((0,0,511,145),fill=0)
    dd.rectangle((0,325,511,511),fill=0)
    im.paste(8,(0,0),dirt)
    # Dashed centerline with even arc-length spacing.
    travel = 0
    for a,b in zip(line,line[1:]):
        travel += math.dist(a,b)
        if int(travel/9)%2 == 0:
            d.line((a,b),fill=10 if a[0]<125 and 145<a[1]<325 else 3,width=1)
    # Trackside tire walls; aligned to tiles for reuse.
    for x in range(48,464,8):
        for y in (40,464):
            d.rectangle((x,y,x+5,y+4),fill=1)
            d.line((x+1,y,x+4,y),fill=3)
    for y in range(48,464,8):
        for x in (40,472):
            d.rectangle((x,y,x+4,y+5),fill=1)
            d.line((x,y+1,x,y+4),fill=3)
    im = im.resize((1024,1024),Image.Resampling.NEAREST)
    surface = road.resize((1024,1024),Image.Resampling.NEAREST)
    dirt_full = dirt.resize((1024,1024),Image.Resampling.NEAREST)
    d = ImageDraw.Draw(im)
    # Start/finish strip and direction arrow.
    for y in range(828,892,4):
        for x in range(448,456,4):
            d.rectangle((x,y,x+3,y+3),fill=12 if ((y//4+x//4)%2) else 1)
    d.polygon([(490,855),(500,855),(500,850),(511,860),(500,870),(500,865),(490,865)],fill=11)
    # Workshop and paddock inside the circuit, with original signage.
    d.rectangle((470,695,695,794),fill=4)
    for x in range(488,681,32):
        d.line((x,758,x,782),fill=7,width=2)
    d.rectangle((476,690,690,750),fill=1)
    d.rectangle((480,682,682,739),fill=3)
    d.rectangle((480,682,682,689),fill=13)
    for x in range(488,673,8):
        d.line((x,693,x,734),fill=2)
    d.rectangle((497,702,666,721),fill=1)
    label(im,510,708,'DUSTLINE MOTOR WORKS',14)
    label(im,478,913,'PROVING GROUNDS / 01',12)
    label(im,86,550,'DIRT',11)
    label(im,755,460,'LIFT',11,2)
    label(im,732,482,'BEFORE THE TURN',12)
    # Repeating shrubs off-road. They are low scrub, intentionally drive-through.
    rng = random.Random(901)
    for _ in range(200):
        x,y = rng.randrange(12,116)*8,rng.randrange(12,116)*8
        if surface.getpixel((x,y)) or 450<x<710 and 675<y<810:
            continue
        d.rectangle((x+2,y+3,x+13,y+12),fill=4)
        d.rectangle((x,y,x+11,y+9),fill=6)
        d.rectangle((x+2,y,x+8,y+2),fill=7)
    # Solid cones in the straight and tire stacks outside a few bends.
    obstacles = [(565,845,5),(630,877,5),(696,845,5),
                 (917,750,9),(467,555,9),(847,103,9),(116,260,9)]
    for x,y,r in obstacles:
        if r==5:
            d.rectangle((x-5,y+2,x+5,y+5),fill=1)
            d.polygon([(x,y-7),(x-4,y+3),(x+4,y+3)],fill=15)
            d.line((x-2,y-1,x+2,y-1),fill=12,width=2)
        else:
            d.ellipse((x-9,y-7,x+9,y+8),fill=1)
            d.ellipse((x-7,y-7,x+7,y+4),fill=3)
            d.ellipse((x-3,y-5,x+3,y),fill=1)
    # Canonicalize flips when counting actual hardware tile usage.
    unique=set()
    for y in range(0,1024,8):
        for x in range(0,1024,8):
            tile=im.crop((x,y,x+8,y+8))
            variants=[tile,tile.transpose(Image.Transpose.FLIP_LEFT_RIGHT),
                      tile.transpose(Image.Transpose.FLIP_TOP_BOTTOM),tile.rotate(180)]
            unique.add(min(v.tobytes() for v in variants))
    print(f'Track: {len(unique)} unique tiles (limit 1024)')
    if len(unique)>1024:
        raise RuntimeError('Track exceeds GBA tile budget')
    save('track',im,'regular_bg',bpp_mode='bpp_4',big=True)
    im.convert('RGB').save(ROOT/'artifacts/track.png')
    cells=[]
    for y in range(128):
        for x in range(128):
            p=(x*8+4,y*8+4)
            cells.append(2 if dirt_full.getpixel(p) else 1 if surface.getpixel(p) else 0)
    checkpoints = [WAYPOINTS[i] for i in (2,4,6,8,10,12,14,16,18)]
    header = '#pragma once\n#include <cstdint>\nnamespace world {\n'
    header += 'inline constexpr uint8_t surfaces[16384] = {\n'
    header += '\n'.join(','.join(map(str,cells[i:i+128]))+',' for i in range(0,len(cells),128))+'\n};\n'
    header += 'struct Obstacle { int x,y,r; };\ninline constexpr Obstacle obstacles[] = {'
    header += ','.join('{%d,%d,%d}'%v for v in obstacles)+'};\n'
    header += 'struct Point { int x,y; };\ninline constexpr Point checkpoints[] = {'
    header += ','.join('{%d,%d}'%v for v in checkpoints)+'};\n}\n'
    (GEN/'track_data.h').write_text(header,encoding='utf8')
    # Test driver follows exactly the same centerline as the visible track.
    (ROOT/'artifacts/track_path.json').write_text(json.dumps(PATH),encoding='utf8')

def sprites():
    # Render each direction from a flat model; y projection gives a mild elevated view.
    sheet=img((32,32*64),sprite=True)
    buggy_sheet=img((32,32*64),sprite=True)
    old_sheet=img((32,32*64),sprite=True)
    truck_sheet=img((32,32*64),sprite=True)
    pickup_sheet=img((32,32*64),sprite=True)
    # Five independently fitted weapons have 32 possible combinations.  Store
    # them mask-major so runtime can select mask*64+heading while still using a
    # single OBJ overlay for the whole equipped loadout.
    loadout_sheet=img((32,32*64*32),sprite=True)
    enemy_gun_sheet=img((32,32*64),sprite=True)
    for frame in range(64):
        tile=img((32,32),sprite=True)
        d=ImageDraw.Draw(tile)
        angle=frame*math.tau/64
        c,s=math.cos(angle),math.sin(angle)
        def projected(point,z=0):
            x,y=point
            return round(16+x*c-y*s),round(17+(x*s+y*c)*0.82-z)
        def poly(points,color,z=0,draw=d):
            pts=[(round(16+x*c-y*s),round(17+(x*s+y*c)*0.82-z)) for x,y in points]
            draw.polygon(pts,fill=color)
        poly([(-11,-7),(11,-7),(13,6),(-10,8)],1)
        for x in (-7,7):
            for y in (-6,6):
                poly([(x-3,y-2),(x+3,y-2),(x+3,y+2),(x-3,y+2)],1,1)
        hull=[(-11,-5),(-8,-7),(8,-6),(12,-4),(12,4),(8,6),(-8,7),(-11,5)]
        poly(hull,3,1)
        poly(hull,5,3)
        poly([(-10,-5),(8,-5),(11,-3),(-10,-3)],6,3)
        poly([(-5,-4),(4,-4),(6,-3),(6,3),(4,4),(-5,4)],2,4)
        poly([(2,-4),(5,-3),(5,3),(2,4)],6,4)
        poly([(-4,-4),(1,-4),(1,4),(-4,4)],4,5)
        poly([(-4,-4),(1,-4),(1,-3),(-4,-3)],7,5)
        poly([(7,-1),(11,-1),(11,1),(7,1)],8,3)
        for y in (-4,3):
            poly([(10,y),(12,y),(12,y+1),(10,y+1)],7,3)
            poly([(-11,y),(-9,y),(-9,y+1),(-11,y+1)],10,3)
        sheet.paste(tile,(0,frame*32))

        # All alternate bodies keep the same footprint, mount coordinates and
        # swappable body colors (4/5/6) as the original car.
        buggy=img((32,32),sprite=True);bd=ImageDraw.Draw(buggy)
        poly([(-11,-7),(11,-7),(13,5),(-10,7)],1,0,bd)
        for x in (-7,7):
            for y in (-7,7):
                poly([(x-3,y-2),(x+3,y-2),(x+3,y+2),(x-3,y+2)],1,1,bd)
        poly([(-10,-4),(-6,-6),(8,-5),(12,-3),(12,3),(8,5),(-6,6),(-10,4)],3,1,bd)
        poly([(-9,-3),(-5,-5),(9,-4),(12,-2),(12,2),(9,4),(-5,5),(-9,3)],5,3,bd)
        poly([(3,-4),(10,-3),(12,-2),(12,2),(10,3),(3,4)],6,4,bd)
        poly([(-5,-3),(2,-3),(4,-2),(4,2),(2,3),(-5,3)],2,4,bd)
        for y in (-4,4):
            bd.line((projected((-4,y),4),projected((-4,y),9)),fill=4,width=1)
        bd.line((projected((-4,-4),9),projected((-4,4),9)),fill=6,width=1)
        poly([(8,-1),(12,-1),(12,1),(8,1)],8,4,bd)
        for y in (-3,3):poly([(-10,y),(-8,y),(-8,y+1),(-10,y+1)],10,3,bd)
        buggy_sheet.paste(buggy,(0,frame*32))

        old=img((32,32),sprite=True);od=ImageDraw.Draw(old)
        poly([(-12,-7),(11,-7),(13,6),(-11,7)],1,0,od)
        for x in (-7,7):
            for y in (-6,6):
                poly([(x-3,y-2),(x+3,y-2),(x+3,y+2),(x-3,y+2)],1,1,od)
                poly([(x-4,y-2),(x+4,y-2),(x+4,y+2),(x-4,y+2)],4,2,od)
        poly([(-12,-5),(-9,-7),(7,-6),(12,-4),(12,4),(7,6),(-9,7),(-12,5)],3,1,od)
        poly([(-11,-4),(-8,-6),(8,-5),(12,-3),(12,3),(8,5),(-8,6),(-11,4)],5,3,od)
        poly([(-10,-6),(8,-5),(8,-4),(-10,-4)],6,3,od)
        poly([(-5,-4),(2,-4),(5,-2),(5,2),(2,4),(-5,4),(-7,2),(-7,-2)],2,5,od)
        poly([(-6,-4),(2,-4),(4,-2),(4,2),(2,4),(-6,4)],4,6,od)
        poly([(-5,-3),(1,-3),(3,-2),(3,2),(1,3),(-5,3)],2,6,od)
        poly([(4,-3),(11,-3),(12,-2),(12,2),(11,3),(4,3)],6,4,od)
        poly([(10,-3),(12,-2),(12,2),(10,3)],14,5,od)
        for y in (-4,3):poly([(9,y),(11,y),(11,y+1),(9,y+1)],8,5,od)
        for y in (-3,2):poly([(-12,y),(-10,y),(-10,y+1),(-12,y+1)],10,4,od)
        old_sheet.paste(old,(0,frame*32))

        truck=img((32,32),sprite=True);td=ImageDraw.Draw(truck)
        poly([(-12,-8),(12,-8),(13,7),(-12,8)],1,0,td)
        for x in (-8,0,8):
            for y in (-7,7):
                poly([(x-3,y-2),(x+3,y-2),(x+3,y+2),(x-3,y+2)],1,1,td)
        poly([(-12,-6),(11,-6),(13,-4),(13,4),(11,6),(-12,6)],3,1,td)
        poly([(-12,-5),(10,-5),(12,-3),(12,3),(10,5),(-12,5)],5,3,td)
        poly([(-11,-5),(0,-5),(0,5),(-11,5)],4,6,td)
        poly([(-10,-4),(-1,-4),(-1,4),(-10,4)],5,7,td)
        poly([(-9,-4),(-1,-4),(-1,-3),(-9,-3)],6,7,td)
        poly([(0,-5),(9,-5),(12,-3),(12,3),(9,5),(0,5)],6,5,td)
        poly([(2,-4),(8,-4),(10,-2),(10,2),(8,4),(2,4)],2,7,td)
        poly([(1,-4),(9,-4),(9,-3),(1,-3)],7,7,td)
        poly([(10,-3),(12,-2),(12,2),(10,3)],14,5,td)
        for y in (-4,3):poly([(9,y),(11,y),(11,y+1),(9,y+1)],8,6,td)
        for y in (-3,2):poly([(-12,y),(-10,y),(-10,y+1),(-12,y+1)],10,6,td)
        truck_sheet.paste(truck,(0,frame*32))

        pickup=img((32,32),sprite=True);pd=ImageDraw.Draw(pickup)
        poly([(-12,-7),(12,-7),(13,6),(-11,7)],1,0,pd)
        for x in (-8,8):
            for y in (-6,6):
                poly([(x-3,y-2),(x+3,y-2),(x+3,y+2),(x-3,y+2)],1,1,pd)
        poly([(-12,-6),(10,-6),(13,-4),(13,4),(10,6),(-12,6)],3,1,pd)
        poly([(-11,-5),(10,-5),(12,-3),(12,3),(10,5),(-11,5)],5,3,pd)
        poly([(-11,-5),(-3,-5),(-3,5),(-11,5)],6,4,pd)
        poly([(-10,-3),(-4,-3),(-4,3),(-10,3)],2,5,pd)
        poly([(-3,-5),(4,-5),(6,-3),(6,3),(4,5),(-3,5)],4,5,pd)
        poly([(-2,-4),(3,-4),(5,-2),(5,2),(3,4),(-2,4)],2,6,pd)
        poly([(5,-4),(11,-3),(12,-2),(12,2),(11,3),(5,4)],6,4,pd)
        poly([(8,-1),(12,-1),(12,1),(8,1)],8,5,pd)
        for y in (-3,2):poly([(-12,y),(-10,y),(-10,y+1),(-12,y+1)],10,4,pd)
        pickup_sheet.paste(pickup,(0,frame*32))
        # Equipment uses the exact same local coordinates, projection and 64
        # headings as the hull.  Keeping a single combined overlay per loadout
        # makes the first modular-art pass cost only one extra OBJ per vehicle.
        enemy_gun=img((32,32),sprite=True);ed=ImageDraw.Draw(enemy_gun)
        def line(a,b,color,z,width,draw):
            draw.line((projected(a,z),projected(b,z)),fill=color,width=width)
        def deck_gun(draw):
            poly([(3,-2),(8,-2),(9,2),(3,2)],15,7,draw)
            poly([(4,-1),(8,-1),(8,1),(4,1)],14,8,draw)
            line((7,0),(15,0),12,8,1,draw)
            line((13,0),(15,0),8,8,1,draw)
        def chainsaw(draw):
            # A low toothed blade occupies the nose while a deck gun can remain
            # visible above it.
            blade=[]
            for point in range(16):
                a=point*math.tau/16;r=3 if point%2==0 else 2
                blade.append((12+math.cos(a)*r,math.sin(a)*r))
            poly(blade,14,3,draw);poly([(11,-1),(15,-1),(15,1),(11,1)],3,3,draw)
            hub=projected((12,0),4)
            draw.ellipse((hub[0]-1,hub[1]-1,hub[0]+1,hub[1]+1),fill=8)
        def side_guns(draw):
            # Start at the sill so the far mount has little hull overlap.
            for side in (-1,1):
                poly([(-1,side*5),(4,side*5),(4,side*7),(-1,side*7)],15,6,draw)
                line((2,side*6),(2,side*11),14,6,2,draw)
                line((2,side*10),(2,side*12),8,6,1,draw)
        def missiles(draw):
            poly([(-8,-4),(0,-4),(0,4),(-8,4)],15,7,draw)
            for side in (-2,2):
                poly([(-8,side-1),(-1,side-1),(2,side),(-1,side+1),(-8,side+1)],3,9,draw)
                line((-1,side),(2,side),12,9,1,draw)
                line((-8,side),(-6,side),10,9,1,draw)
        def traps(draw):
            # Four compact trap/grenade canisters are clipped to the rear rack.
            for x in (-11,-8):
                for y in (-3,3):
                    p=projected((x,y),6)
                    draw.rectangle((p[0]-1,p[1]-1,p[0]+1,p[1]+1),fill=10,outline=1)
        deck_gun(ed)
        for loadout_mask in range(32):
            loadout=img((32,32),sprite=True);ld=ImageDraw.Draw(loadout)
            if loadout_mask&1:deck_gun(ld)
            if loadout_mask&2:chainsaw(ld)
            if loadout_mask&4:side_guns(ld)
            if loadout_mask&8:missiles(ld)
            if loadout_mask&16:traps(ld)
            loadout_sheet.paste(loadout,(0,(loadout_mask*64+frame)*32))
        enemy_gun_sheet.paste(enemy_gun,(0,frame*32))
    save('car',sheet,'sprite',height=32)
    save('car_sand_buggy',buggy_sheet,'sprite',height=32,bpp_mode='bpp_4')
    save('car_old',old_sheet,'sprite',height=32,bpp_mode='bpp_4')
    save('car_truck',truck_sheet,'sprite',height=32,bpp_mode='bpp_4')
    save('car_pickup',pickup_sheet,'sprite',height=32,bpp_mode='bpp_4')
    save('car_loadout',loadout_sheet,'sprite',height=32,bpp_mode='bpp_4')
    save('car_enemy_gun',enemy_gun_sheet,'sprite',height=32,bpp_mode='bpp_4')
    fx=img((8,8*4),sprite=True)
    d=ImageDraw.Draw(fx)
    d.rectangle((2,2,5,5),fill=2)
    d.rectangle((1,10,5,13),fill=9)
    d.rectangle((2,17,6,22),fill=14)
    d.rectangle((2,26,5,29),fill=8)
    save('particles',fx,'sprite',height=8)
    # Tiny combat UI/effects extend the existing procedural sprite palette.
    bullet=img((8,32),sprite=True); d=ImageDraw.Draw(bullet)
    d.rectangle((2,3,5,4),fill=7); d.rectangle((3,2,4,5),fill=8)
    d.rectangle((2,11,5,12),fill=10); d.rectangle((3,10,4,13),fill=8)
    d.rectangle((2,18,5,21),fill=6);d.point((3,19),fill=7)
    d.rectangle((1,26,6,29),fill=7);d.rectangle((2,25,5,30),fill=15);d.rectangle((3,24,4,31),fill=8)
    save('combat_bullet',bullet,'sprite',height=8)
    # Original 4bpp weapon art. Pre-rotated missiles avoid affine sprite costs.
    saw=img((32,128),sprite=True)
    for frame in range(4):
        d=ImageDraw.Draw(saw);cy=frame*32+16
        points=[]
        for k in range(24):
            a=(k/24+frame/48)*math.tau;r=11 if k%2==0 else 8
            points.append((round(16+math.cos(a)*r),round(cy+math.sin(a)*r)))
        d.polygon(points,fill=12,outline=3)
        d.ellipse((9,cy-7,23,cy+7),fill=14,outline=7)
        d.line((10,cy-2,22,cy+2),fill=3,width=2)
        d.ellipse((13,cy-3,19,cy+3),fill=8,outline=9)
    save('weapon_saw',saw,'sprite',height=32,bpp_mode='bpp_4')
    missile=img((16,256),sprite=True)
    for frame in range(16):
        d=ImageDraw.Draw(missile);a=frame*math.tau/16;cs=math.cos(a);sn=math.sin(a)
        def poly(points,fill):
            d.polygon([(round(7.5+x*cs-y*sn),round(frame*16+7.5+x*sn+y*cs)) for x,y in points],fill=fill)
        poly([(-7,0),(-3,-2),(-3,2)],8)
        poly([(-5,-4),(-1,-2),(4,0),(-1,2),(-5,4)],3)
        poly([(-4,-2),(2,-2),(6,0),(2,2),(-4,2)],12)
        poly([(2,-2),(6,0),(2,2)],10)
    save('weapon_missile',missile,'sprite',height=16,bpp_mode='bpp_4')
    trap=img((16,32),sprite=True)
    for frame in range(2):
        d=ImageDraw.Draw(trap);y=frame*16
        d.ellipse((2,y+4,13,y+13),fill=1)
        d.rectangle((4,y+3,11,y+11),fill=14,outline=3)
        d.line((4,y+9,11,y+9),fill=8,width=2)
        d.rectangle((6,y+4,9,y+6),fill=10 if frame else 2)
    save('weapon_trap',trap,'sprite',height=16,bpp_mode='bpp_4')
    blast=img((32,128),sprite=True)
    for frame in range(4):
        d=ImageDraw.Draw(blast);y=frame*32;r=(7,15,13,8)[frame]
        d.ellipse((16-r,y+16-r,16+r,y+16+r),fill=10 if frame<2 else 9)
        d.ellipse((16-r//2,y+16-r//2,16+r//2,y+16+r//2),fill=7 if frame==0 else 8)
        if frame>=2:d.ellipse((12,y+12,20,y+20),fill=0)
    save('weapon_blast',blast,'sprite',height=32,bpp_mode='bpp_4')
    # Larger, square inventory portraits echo stamped garage-part catalogues.
    # They use the warm metal/oxide half of the sprite palette instead of the
    # bright driving colours, so the dedicated fitting screen has its own mood.
    icons=img((32,32*9),sprite=True)
    for frame in range(9):
        y=frame*32;d=ImageDraw.Draw(icons)
        d.rectangle((3,y+3,28,y+28),fill=2,outline=14)
        d.line((5,y+5,26,y+5),fill=8);d.line((5,y+26,26,y+26),fill=1)
        for xx,yy in ((6,7),(25,7),(6,24),(25,24)):d.point((xx,y+yy),fill=9)
        if frame==0:       # forward gun
            d.rectangle((7,y+13,25,y+18),fill=12,outline=1)
            d.rectangle((10,y+18,15,y+23),fill=9,outline=1)
            d.rectangle((23,y+14,30,y+16),fill=8)
            d.point((13,y+14),fill=15)
        elif frame==1:     # reserved saw
            d.ellipse((7,y+8,24,y+25),fill=13,outline=12)
            d.ellipse((11,y+12,20,y+21),fill=1)
            for xx,yy in ((8,8),(16,6),(24,10),(25,19),(19,25),(10,24),(6,17)):
                d.rectangle((xx,y+yy,xx+2,y+yy+2),fill=12)
        elif frame==2:     # paired side guns
            d.rectangle((13,y+9,18,y+23),fill=12,outline=1)
            d.rectangle((4,y+12,14,y+16),fill=13,outline=1)
            d.rectangle((17,y+17,27,y+21),fill=13,outline=1)
            d.rectangle((2,y+13,7,y+14),fill=8);d.rectangle((25,y+19,30,y+20),fill=8)
        elif frame==3:     # missile
            d.polygon([(4,y+18),(10,y+11),(24,y+11),(29,y+16),(24,y+21),(10,y+21)],fill=12,outline=1)
            d.polygon([(7,y+12),(2,y+8),(10,y+15)],fill=9)
            d.polygon([(7,y+20),(2,y+24),(10,y+17)],fill=9)
            d.rectangle((24,y+14,29,y+17),fill=10)
        elif frame==4:     # trap canister
            d.rounded_rectangle((9,y+7,23,y+25),radius=3,fill=9,outline=1)
            d.rectangle((11,y+10,21,y+19),fill=15,outline=12)
            d.rectangle((13,y+5,19,y+8),fill=13)
            d.rectangle((12,y+19,20,y+23),fill=10)
        elif frame==5:     # empty mounting plate
            d.rectangle((9,y+9,22,y+22),fill=1,outline=13)
            d.line((11,y+11,20,y+20),fill=15,width=2)
            d.line((20,y+11,11,y+20),fill=15,width=2)
        elif frame==6:     # passive radio-signal receiver
            d.rectangle((7,y+16,24,y+24),fill=3,outline=1)
            d.rectangle((10,y+18,21,y+21),fill=13,outline=14)
            d.line((16,y+16,16,y+7),fill=12,width=2)
            d.line((16,y+8,21,y+4),fill=12,width=2)
            d.point((11,y+20),fill=10)
            for radius in (3,6):
                d.arc((16-radius,y+7-radius,16+radius,y+7+radius),200,340,fill=8,width=1)
        elif frame==7:     # long-range sniper
            d.rectangle((5,y+13,27,y+18),fill=12,outline=1)
            d.rectangle((9,y+18,14,y+24),fill=9,outline=1)
            d.rectangle((25,y+14,31,y+16),fill=8)
            d.rectangle((14,y+9,22,y+12),fill=3,outline=1)
            d.line((18,y+9,18,y+6),fill=13)
        else:              # paired forward-firing side mount
            for yy in (11,20):
                d.rectangle((6,y+yy,25,y+yy+4),fill=12,outline=1)
                d.rectangle((23,y+yy+1,30,y+yy+2),fill=8)
            d.rectangle((9,y+14,17,y+20),fill=9,outline=1)
    save('weapon_icons',icons,'sprite',height=32,bpp_mode='bpp_4')

    # Mechanic shop grid: three permanent upgrades followed by every weapon
    # that is not part of the player's starting loadout.
    shop_icons=img((32,32*9),sprite=True)
    for frame in range(9):
        y=frame*32
        if frame>=3:
            weapon_frame=(2,7,8,3,4,6)[frame-3]
            shop_icons.paste(icons.crop((0,weapon_frame*32,32,weapon_frame*32+32)),(0,y))
            continue
        d=ImageDraw.Draw(shop_icons)
        d.rectangle((3,y+3,28,y+28),fill=2,outline=14)
        d.line((5,y+5,26,y+5),fill=8);d.line((5,y+26,26,y+26),fill=1)
        if frame==0:       # salvage magnet
            d.arc((6,y+7,25,y+25),0,180,fill=12,width=4)
            d.rectangle((6,y+15,10,y+24),fill=9);d.rectangle((21,y+15,25,y+24),fill=9)
            for xx,yy in ((10,9),(21,8),(16,5)):d.rectangle((xx,y+yy,xx+2,y+yy+2),fill=8)
        elif frame==1:     # tuned injector
            d.rectangle((8,y+10,23,y+22),fill=3,outline=1)
            d.rectangle((11,y+7,20,y+11),fill=12,outline=1)
            d.line((11,y+15,20,y+15),fill=8,width=2)
            d.polygon([(22,y+11),(29,y+16),(22,y+21)],fill=10,outline=1)
        else:              # reinforced plating
            d.polygon([(16,y+6),(26,y+10),(24,y+22),(16,y+27),(8,y+22),(6,y+10)],fill=12,outline=1)
            d.polygon([(16,y+10),(22,y+12),(21,y+20),(16,y+23),(11,y+20),(10,y+12)],fill=3)
            d.rectangle((14,y+13,18,y+20),fill=8)
    save('shop_icons',shop_icons,'sprite',height=32,bpp_mode='bpp_4')

    # Five original 128x64 hero illustrations are split over two hardware
    # sprites. This lets the fitting bay spend pixels on a deliberately drawn
    # car rather than enlarging the tiny overhead driving sprite.
    garage_cars=img((64,64*10),sprite=True)
    for body in range(5):
        hero=img((128,64),sprite=True);d=ImageDraw.Draw(hero)
        d.ellipse((7,49,121,61),fill=1)
        # Far tyres sit behind the hull and make the oblique view readable.
        for x in (27,91):
            d.ellipse((x-7,18,x+7,31),fill=1,outline=15)
            d.ellipse((x-3,21,x+3,28),fill=14)
        primary=(9,14,11,13,9)[body];light=(10,13,12,14,10)[body]
        if body==0:       # roadster: low armored wedge
            hull=[(6,35),(21,23),(81,20),(112,29),(122,40),(111,50),(25,53),(7,46)]
            d.polygon(hull,fill=primary,outline=1)
            d.polygon([(28,24),(72,22),(90,29),(79,39),(28,39),(18,33)],fill=3,outline=12)
            d.polygon([(32,25),(68,24),(81,29),(72,35),(30,35),(23,31)],fill=1)
            d.polygon([(84,23),(110,29),(117,36),(91,36)],fill=light)
        elif body==1:     # buggy: exposed cage and chopped armor
            d.polygon([(7,36),(24,27),(89,25),(118,35),(114,49),(24,52),(6,45)],fill=9,outline=1)
            d.rectangle((24,31,92,47),fill=10,outline=3)
            d.polygon([(36,19),(73,18),(92,31),(83,40),(28,39),(23,30)],fill=1,outline=13)
            d.line((32,21,84,39),fill=14,width=2);d.line((76,20,31,39),fill=14,width=2)
            d.rectangle((3,38,23,44),fill=12,outline=1);d.rectangle((93,36,124,43),fill=12,outline=1)
        elif body==2:     # old car: rounded bonnet and patched steel
            d.rounded_rectangle((7,22,119,52),radius=11,fill=primary,outline=1,width=2)
            d.polygon([(30,23),(76,21),(95,29),(86,40),(29,40),(20,32)],fill=3,outline=12)
            d.polygon([(35,25),(71,24),(85,29),(78,35),(32,35),(26,31)],fill=1)
            d.rectangle((91,27,117,44),fill=10,outline=9)
            d.line((18,43,68,43),fill=9);d.line((47,23,47,39),fill=14)
        elif body==3:     # truck: slab cab and reinforced rear deck
            d.polygon([(5,29),(72,19),(108,23),(123,34),(120,51),(18,54),(5,46)],fill=13,outline=1)
            d.rectangle((12,27,65,49),fill=14,outline=3)
            d.polygon([(70,22),(102,25),(116,34),(109,43),(70,42)],fill=light,outline=3)
            d.rectangle((21,28,55,39),fill=2,outline=12);d.line((38,28,38,39),fill=14)
            for x in range(15,64,10):d.rectangle((x,44,x+6,48),fill=3)
        else:             # pickup: open salvage tray and compact cab
            d.polygon([(5,29),(78,20),(111,26),(123,37),(117,50),(20,53),(5,45)],fill=primary,outline=1)
            d.rectangle((11,28,61,49),fill=14,outline=3)
            for yy in (32,38,44):d.line((16,yy,57,yy),fill=3)
            d.polygon([(66,23),(101,25),(115,34),(105,42),(67,41)],fill=light,outline=3)
            d.polygon([(74,26),(96,27),(106,33),(99,37),(73,36)],fill=2,outline=12)
        # Near tyres, battered skirt, lamps, seams, rivets and a tow bumper.
        for x in ((24,97) if body!=3 else (20,101)):
            d.ellipse((x-8,42,x+8,58),fill=1,outline=15)
            d.ellipse((x-4,46,x+4,54),fill=14,outline=3)
            d.point((x,50),fill=12)
        d.line((19,50,107,48),fill=3,width=2)
        d.rectangle((115,34,125,43),fill=12,outline=1)
        d.rectangle((2,38,10,46),fill=15);d.line((3,47,18,51),fill=13,width=2)
        d.rectangle((121,40,127,45),fill=14)
        for x,y in ((17,36),(43,44),(68,43),(89,31),(108,42)):d.point((x,y),fill=8)
        for x,y in ((31,46),(58,42),(82,45)):d.line((x,y,x+5,y-2),fill=11)
        for half in range(2):
            garage_cars.paste(hero.crop((half*64,0,half*64+64,64)),(0,(body*2+half)*64))
    save('garage_car_preview',garage_cars,'sprite',height=64,bpp_mode='bpp_4')

    garage_attachments=img((64,64*70),sprite=True)
    for body in range(5):
        for attachment in range(7):
            layer=img((128,64),sprite=True);d=ImageDraw.Draw(layer)
            if attachment==0:       # articulated forward gun
                d.rectangle((84,27,102,37),fill=3,outline=1)
                d.rectangle((89,25,98,40),fill=12,outline=1)
                d.rectangle((99,29,126,33),fill=13,outline=1)
                d.rectangle((119,30,127,31),fill=8);d.point((93,28),fill=10)
            elif attachment==1:     # paired broadside guns
                for x,y,direction in ((54,18,-1),(61,47,1)):
                    d.rectangle((x-7,y-4,x+8,y+4),fill=12,outline=1)
                    end=y+direction*14
                    d.rectangle((x-1,min(y,end),x+2,max(y,end)),fill=13,outline=1)
                    d.rectangle((x-4,y-2,x+5,y+2),fill=9)
            elif attachment==2:     # roof missile rack
                d.rectangle((48,18,83,27),fill=3,outline=1)
                for yy in (18,23):
                    d.polygon([(48,yy),(72,yy),(86,yy+3),(72,yy+6),(48,yy+6)],fill=12,outline=1)
                    d.rectangle((80,yy+2,87,yy+3),fill=10)
            elif attachment==3:     # armored rear trap magazine
                d.rectangle((7,29,22,48),fill=3,outline=1)
                for xx in (10,18):
                    for yy in (32,42):
                        d.rectangle((xx-4,yy-4,xx+4,yy+4),fill=14,outline=1)
                        d.rectangle((xx-2,yy-2,xx+2,yy+2),fill=10)
            elif attachment==4:     # roof radio receiver and whip antenna
                d.rectangle((49,19,78,27),fill=3,outline=1)
                d.rectangle((54,20,72,24),fill=13,outline=14)
                d.point((58,22),fill=10)
                d.line((66,19,70,5),fill=12,width=2)
                d.line((70,5,74,2),fill=12,width=1)
                d.arc((70,1,81,12),175,285,fill=8,width=1)
            elif attachment==5:     # long precision gun and scope
                d.rectangle((81,27,103,37),fill=3,outline=1)
                d.rectangle((88,25,98,40),fill=12,outline=1)
                d.rectangle((98,29,127,32),fill=13,outline=1)
                d.rectangle((102,25,114,28),fill=3,outline=1)
                d.line((108,25,108,22),fill=12)
                d.rectangle((122,30,127,31),fill=8)
            else:                   # paired forward side guns
                for yy in (23,39):
                    d.rectangle((63,yy-3,94,yy+3),fill=12,outline=1)
                    d.rectangle((91,yy-1,124,yy+1),fill=13)
                    d.rectangle((119,yy,127,yy),fill=8)
                d.rectangle((55,25,72,39),fill=3,outline=1)
            for half in range(2):
                frame=body*14+attachment*2+half
                garage_attachments.paste(layer.crop((half*64,0,half*64+64,64)),(0,frame*64))
    save('garage_car_attachments',garage_attachments,'sprite',height=64,bpp_mode='bpp_4')

    fitting_cursor=img((32,64),sprite=True)
    for frame,color in enumerate((8,10)):
        y=frame*32;d=ImageDraw.Draw(fitting_cursor)
        for left,top,right,bottom in ((3,y+3,10,y+3),(3,y+3,3,y+10),(21,y+3,28,y+3),(28,y+3,28,y+10),
                                      (3,y+28,10,y+28),(3,y+21,3,y+28),(21,y+28,28,y+28),(28,y+21,28,y+28)):
            d.line((left,top,right,bottom),fill=color,width=2)
    save('fitting_cursor',fitting_cursor,'sprite',height=32,bpp_mode='bpp_4')
    loadout_ring=img((64,64*4),sprite=True)
    for selected in range(4):
        y=selected*64;d=ImageDraw.Draw(loadout_ring)
        d.ellipse((7,y+7,56,y+56),fill=1,outline=12,width=2)
        d.ellipse((17,y+17,46,y+46),outline=3,width=1)
        # North/east/south/west correspond to hunter, brawler, sweeper and barrage.
        points=[(32,y+8),(55,y+32),(32,y+55),(8,y+32)]
        glyphs=['H','B','S','X']
        for index,(px,py) in enumerate(points):
            color=8 if index==selected else 12
            d.ellipse((px-6,py-6,px+6,py+6),fill=3,outline=color,width=2)
            label(loadout_ring,px-2,py-3,glyphs[index],15 if index==selected else color)
        d.rectangle((28,y+29,35,y+34),fill=14,outline=15)
        d.point((31,y+31),fill=8);d.point((32,y+31),fill=8)
    save('loadout_ring',loadout_ring,'sprite',height=64,bpp_mode='bpp_4')
    salvage=img((16,32),sprite=True);d=ImageDraw.Draw(salvage)
    # Scrap: a compact pile of recoverable metal and circuitry.
    d.rectangle((2,7,13,12),fill=1);d.rectangle((3,5,8,10),fill=14,outline=3)
    d.rectangle((9,6,13,11),fill=12,outline=3);d.line((4,8,12,8),fill=7,width=1);d.point((6,6),fill=8)
    # Energy cell: immediately consumed by the car rather than inventoried.
    d.rectangle((4,19,11,29),fill=1,outline=15);d.rectangle((6,17,9,19),fill=15)
    d.rectangle((6,21,9,27),fill=4);d.point((10,23),fill=8)
    save('salvage_pickup',salvage,'sprite',height=16,bpp_mode='bpp_4')
    hp=img((16,24),sprite=True); d=ImageDraw.Draw(hp)
    for frame in range(3):
        y=frame*8; d.rectangle((0,y+2,15,y+6),fill=1)
        for pip in range(frame+1): d.rectangle((2+pip*4,y+3,4+pip*4,y+5),fill=10 if frame==0 else 8 if frame==1 else 6)
    save('combat_hp',hp,'sprite',height=8)
    burst=img((16,64),sprite=True); d=ImageDraw.Draw(burst)
    for frame in range(4):
        y=frame*16; r=(4,7,6,3)[frame]
        d.ellipse((8-r,y+8-r,8+r,y+8+r),fill=10 if frame<2 else 9)
        if frame<3: d.ellipse((6,y+6,10,y+10),fill=7 if frame==0 else 8)
    save('combat_burst',burst,'sprite',height=16)
    dot=img((8,8),sprite=True)
    ImageDraw.Draw(dot).rectangle((2,2,5,5),fill=8)
    save('dot',dot,'sprite')
    enemy_dot=img((8,8),sprite=True)
    ImageDraw.Draw(enemy_dot).rectangle((3,3,4,4),fill=10)
    save('enemy_dot',enemy_dot,'sprite')
    mission_dot=img((8,8),sprite=True);d=ImageDraw.Draw(mission_dot)
    d.rectangle((1,1,6,6),fill=8);d.rectangle((2,2,5,5),fill=12);d.rectangle((3,3,4,4),fill=10)
    save('mission_dot',mission_dot,'sprite')
    radio_barrel=img((16,16),sprite=True);d=ImageDraw.Draw(radio_barrel)
    d.ellipse((3,2,12,5),fill=14,outline=1)
    d.rectangle((3,4,12,12),fill=9,outline=1)
    d.ellipse((3,10,12,14),fill=9,outline=1)
    d.line((4,6,11,6),fill=15);d.line((4,10,11,10),fill=15)
    d.rectangle((6,5,9,11),fill=10);d.point((7,7),fill=8)
    save('radio_barrel',radio_barrel,'sprite',height=16,bpp_mode='bpp_4')
    radio_chevrons=img((32,32*24),sprite=True)
    for strength in range(3):
        for direction in range(8):
            y=(strength*8+direction)*32;d=ImageDraw.Draw(radio_chevrons)
            angle=direction*math.tau/8;ux=math.cos(angle);uy=math.sin(angle)
            px=-uy;py=ux
            for index in range(strength+1):
                along=(index-strength/2)*6
                cx=15.5+ux*along;cy=y+15.5+uy*along
                tip=(round(cx+ux*4),round(cy+uy*4))
                left=(round(cx-ux*3+px*4),round(cy-uy*3+py*4))
                right=(round(cx-ux*3-px*4),round(cy-uy*3-py*4))
                d.line((left,tip,right),fill=1,width=4,joint='curve')
                d.line((left,tip,right),fill=8,width=2,joint='curve')
    save('radio_chevrons',radio_chevrons,'sprite',height=32,bpp_mode='bpp_4')
    settings_tab=img((8,8*8),sprite=True);d=ImageDraw.Draw(settings_tab)
    for tab in range(4):
        for active in range(2):
            y=(tab*2+active)*8;color=8 if active else 12;dim=12 if active else 14
            d.rectangle((0,y,7,y+7),outline=color)
            if tab==0: # Map grid and position.
                d.line((2,y+2,5,y+5),fill=dim);d.line((5,y+2,2,y+5),fill=dim);d.point((4,y+4),fill=color)
            elif tab==1: # Car / driving tune.
                d.rectangle((2,y+2,5,y+5),fill=dim);d.point((1,y+5),fill=color);d.point((6,y+5),fill=color)
            elif tab==2: # Music note.
                d.line((4,y+1,4,y+5),fill=color);d.line((4,y+1,6,y+2),fill=color);d.rectangle((2,y+5,4,y+6),fill=dim)
            else: # Save cartridge.
                d.rectangle((2,y+1,5,y+6),fill=dim,outline=color);d.rectangle((3,y+2,4,y+3),fill=1);d.point((4,y+5),fill=color)
    save('settings_tab',settings_tab,'sprite',height=8,bpp_mode='bpp_4')
    race_gate=img((16,32),sprite=True);d=ImageDraw.Draw(race_gate)
    for frame in range(2):
        y=frame*16
        outer=7 if frame==0 else 6
        d.ellipse((8-outer,y+8-outer,8+outer,y+8+outer),outline=12,width=2)
        d.ellipse((4,y+4,12,y+12),outline=8 if frame==0 else 10,width=2)
        d.rectangle((7,y+1,9,y+3),fill=8)
        d.rectangle((7,y+13,9,y+15),fill=8)
    save('race_gate',race_gate,'sprite',height=16)
    race_flag=img((8,16),sprite=True);d=ImageDraw.Draw(race_flag)
    d.rectangle((3,3,4,15),fill=12)
    d.polygon(((4,3),(7,5),(4,8)),fill=10)
    d.point((3,2),fill=8)
    save('race_flag',race_flag,'sprite',height=16)
    # Dedicated overview palette: transparent, dark floor, bright wall, grey
    # road, warm town dots. Runtime fills the 64x64 sprite from logical cells.
    overview=img((8,8),sprite=True)
    colors=[(255,0,255),(24,24,24),(224,224,224),(112,112,112),(248,200,64)]+[(0,0,0)]*8+[(224,56,48),(48,184,248),(248,248,224)]
    overview.putpalette([c for rgb in colors for c in rgb]+[0]*(768-48))
    save('overview_palette',overview,'sprite')
    font=img((8,8*94),sprite=True)
    for i in range(94):
        label(font,1,i*8,chr(33+i),12)
    save('font',font,'sprite',height=8)

def screens(path, palette):
    # The authored source is kept separate from the generated Butano BMP.
    # A darkened footer keeps live map-selection text legible on the artwork.
    source=Image.open(GFX/'dustline_title_clean.png').convert('RGB')
    screen=ImageOps.fit(source,(240,160),Image.Resampling.LANCZOS)
    footer=screen.crop((0,124,240,160))
    screen.paste(Image.blend(footer,Image.new('RGB',footer.size),(0.58)),(0,124))
    # BG palette index zero is global on the GBA. The wasteland changes it to
    # its dark transparent colour, so visible title pixels must never use it.
    # Quantize into the other 255 entries and shift every visible index by one.
    screen=screen.quantize(colors=255,method=Image.Quantize.MEDIANCUT,
                           dither=Image.Dither.FLOYDSTEINBERG)
    shifted=Image.new('P',screen.size)
    shifted.putdata([index+1 for index in screen.getdata()])
    shifted.putpalette([16,33,41]+screen.getpalette()[:255*3])
    title=Image.new('P',(256,256),0);title.putpalette(shifted.getpalette())
    # Visible viewport in a centered 256x256 regular BG: (8,48)..(247,207).
    title.paste(shifted,(8,48))
    save('title',title,'regular_bg',bpp_mode='bpp_8')
    hud=img((256,256),0)
    d=ImageDraw.Draw(hud)
    # Driving information lives around the minimap; leave the full viewport open.
    hud.putpalette(palette)
    save('hud',hud,'regular_bg',bpp_mode='bpp_8')
    blank=img((256,256),1); blank.putpalette(palette)
    save('menu_blank',blank,'regular_bg',bpp_mode='bpp_8')
    fitting=img((256,256),1);fitting.putpalette(palette);d=ImageDraw.Draw(fitting)
    # A self-contained, warm mechanical console: oxidized framing, a dusty
    # illustration bay and inset part trays. The composition follows the
    # supplied compact loadout mockup while all pixels and details are original.
    d.rectangle((8,48,247,207),fill=8)
    d.rectangle((10,50,245,205),fill=1,outline=10)
    d.line((11,51,244,51),fill=11)
    d.line((11,204,244,204),fill=3)
    # Header plate and stamped identification strip.
    d.rectangle((11,52,244,65),fill=8,outline=3)
    d.line((12,53,243,53),fill=10)
    d.line((12,64,243,64),fill=1)
    label(fitting,16,55,'LOADOUT',12)
    label(fitting,67,56,'WEAPON FITTING',3)
    d.rectangle((190,54,240,62),fill=2,outline=9)
    label(fitting,197,55,'BAY 01',7)
    for x in (13,242):d.ellipse((x-1,56,x+1,58),fill=3);d.point((x,57),fill=12)

    # Left hero bay: sun-bleached exterior, distant shop silhouettes and grit.
    d.rectangle((11,67,139,145),fill=9,outline=3)
    d.rectangle((14,70,136,142),fill=10)
    d.rectangle((14,70,136,103),fill=9)
    d.rectangle((14,103,136,142),fill=8)
    d.ellipse((103,76,119,92),fill=11)
    d.polygon([(14,102),(35,91),(57,101),(78,88),(101,100),(119,92),(136,101),(136,111),(14,111)],fill=3)
    d.rectangle((19,84,22,104),fill=2);d.rectangle((16,82,25,86),fill=2)
    d.line((21,82,28,73),fill=3);d.line((28,73,33,82),fill=3)
    d.rectangle((120,87,133,105),fill=2);d.rectangle((117,84,136,88),fill=3)
    for x,y in ((18,114),(27,123),(39,109),(52,132),(69,116),(83,137),(101,112),(114,128),(129,117)):
        d.point((x,y),fill=10);d.point((x+1,y),fill=3)
    d.line((14,137,136,137),fill=9)
    label(fitting,17,72,'VEHICLE',12)

    # Three chunky mount cartridges mirror the mockup's bottom loadout strip.
    d.rectangle((11,147,139,198),fill=8,outline=3)
    d.line((12,148,138,148),fill=10)
    for left,title in ((15,'FRONT'),(55,'SIDE'),(95,'TOP')):
        label(fitting,left+2,150,title,12)
        d.rectangle((left,159,left+36,194),fill=1,outline=9)
        d.rectangle((left+2,161,left+34,192),fill=2,outline=3)
        d.line((left+3,162,left+33,162),fill=7)
        d.point((left+3,190),fill=10);d.point((left+32,190),fill=10)

    # Deep inventory recess. Unoccupied cells retain subtle locked silhouettes,
    # making the page feel like a part catalogue without implying selection.
    d.rectangle((142,67,245,198),fill=8,outline=3)
    d.line((143,68,244,68),fill=10)
    d.rectangle((145,71,242,82),fill=2,outline=9)
    label(fitting,149,73,'INVENTORY',12)
    label(fitting,218,74,'PARTS',7)
    for row in range(3):
        for column in range(3):
            left=147+column*33;top=86+row*32
            d.rectangle((left,top,left+28,top+28),fill=1,outline=9)
            d.rectangle((left+2,top+2,left+26,top+26),fill=2,outline=3)
            d.line((left+6,top+20,left+22,top+8),fill=3)
            d.rectangle((left+11,top+11,left+17,top+17),outline=3)
            d.point((left+3,top+3),fill=10);d.point((left+25,top+25),fill=8)
    d.rectangle((146,181,241,195),fill=2,outline=9)
    # Thin footer leaves the maximum possible area to the illustrated panels.
    d.rectangle((10,200,245,205),fill=2)
    label(fitting,14,199,'A FIT',12);label(fitting,83,199,'B BACK',12);label(fitting,157,199,'R DATA',12)
    save('weapon_fitting',fitting,'regular_bg',bpp_mode='bpp_4')
    fitting_info=fitting.copy();fitting_info.putpalette(palette);d=ImageDraw.Draw(fitting_info)
    d.rectangle((142,67,245,198),fill=8,outline=3)
    d.line((143,68,244,68),fill=10)
    d.rectangle((146,72,241,194),fill=1,outline=9)
    d.rectangle((149,75,238,191),fill=2,outline=3)
    label(fitting_info,153,78,'WEAPON DATA',12)
    d.line((153,89,234,89),fill=9)
    for yy in (106,123,140,157):
        d.line((153,yy,231,yy),fill=3)
        d.point((153,yy),fill=9);d.point((231,yy),fill=9)
    d.rectangle((153,174,234,187),outline=8)
    for x in range(155,233,8):d.line((x,176,x+5,184),fill=3)
    save('weapon_fitting_info',fitting_info,'regular_bg',bpp_mode='bpp_4')
    pause=img((256,256),1)
    d=ImageDraw.Draw(pause)
    d.rectangle((16,56,239,198),outline=13,width=2)
    label(pause,32,68,'PIT / STATUS',11,2)
    label(pause,32,92,'A GO  B BRAKE  LEFT/RIGHT STEER',12)
    label(pause,32,104,'R NORMALS / L SPECIAL',11)
    label(pause,32,116,'SELECT VEHICLE LAB / START BACK',12)
    # Leave a dedicated row for the live HP/shield values before the status
    # divider. Contract/race text below it supplies its own context.
    d.line((31,140,224,140),fill=13,width=1)
    pause.putpalette(palette)
    save('pause',pause,'regular_bg',bpp_mode='bpp_8')

def audio():
    rng=random.Random(14)
    rate=16000
    for name,duration in [('engine',0.55),('bump',0.13),('chime',0.20),('skid',0.15),('gun',0.07)]:
        samples=[]
        engine_noise=0
        for i in range(int(rate*duration)):
            t=i/rate
            env=min(1,t*150)*min(1,(duration-t)*100)
            if name=='engine':
                # Low, gently unstable combustion bed.  The old fixed 80/160/320
                # stack became a piercing chord when gameplay pitched it above 2x.
                attack=math.sin(math.pi*.5*min(1,t/.025))
                release=math.sin(math.pi*.5*min(1,(duration-t)/.045))
                env=attack*release
                engine_noise+=(rng.uniform(-1,1)-engine_noise)*.035
                phase=math.tau*(61*t+.018*math.sin(math.tau*2.3*t))
                motion=.92+.06*math.sin(math.tau*.8*t)+.02*math.sin(math.tau*3.7*t+.4)
                value=((.48*math.sin(phase)+.14*math.sin(phase*2+.35)+
                        .055*math.sin(phase*3+1.1)) * motion+
                       .055*math.sin(math.tau*29*t+.7)+.04*engine_noise)*.48
            elif name=='bump':
                value=rng.uniform(-1,1)*(1-t/duration)**2*0.6
            elif name=='skid':
                value=(rng.uniform(-0.4,0.4)+math.sin(math.tau*950*t)*0.16)*0.35
            elif name=='gun':
                value=(rng.uniform(-1,1)*0.6+math.sin(math.tau*180*t)*0.4)*(1-t/duration)**3
            else:
                value=math.sin(math.tau*(660 if t<0.10 else 880)*t)*0.25
            samples.append(int(max(-1,min(1,value*env))*30000))
        with wave.open(str(AUDIO/(name+'.wav')),'wb') as f:
            f.setparams((1,2,rate,len(samples),'NONE','not compressed'))
            f.writeframes(struct.pack('<'+'h'*len(samples),*samples))

def decoration_tiles(palette):
    """Import four alpha-masked patches into a dedicated 4bpp scenery bank."""
    source=Image.open(ROOT/'maps/overworld/wasteland-details-muted.png').convert('RGBA')
    rgba=[]
    for x,y in [(0,0),(1,0),(0,1),(1,1)]:
        art=source.crop((round(x*source.width/2),round(y*source.height/2),
                         round((x+1)*source.width/2),round((y+1)*source.height/2)))
        art.putalpha(art.getchannel('A').point(lambda value:255 if value>=192 else 0))
        bounds=art.getchannel('A').getbbox()
        if not bounds: raise ValueError('Generated decoration is empty')
        art=art.crop(bounds);art.thumbnail((14,14),Image.Resampling.NEAREST)
        patch=Image.new('RGBA',(16,16))
        patch.paste(art,((16-art.width)//2,15-art.height))
        rgba.append(patch)
    # Quantize only opaque pixels; transparent padding must not consume colours.
    pixels=[rgb[:3] for im in rgba for rgb in im.getdata() if rgb[3]]
    strip=Image.new('RGB',(len(pixels),1));strip.putdata(pixels)
    quantized=strip.quantize(colors=15,method=Image.Quantize.MEDIANCUT)
    colors=[tuple((c//8)*8 for c in quantized.getpalette()[i:i+3]) for i in range(0,45,3)]
    detail_palette=[0,0,0]+[c for color in colors for c in color]
    palette[224*3:240*3]=detail_palette
    patches=[]
    for art in rgba:
        im=Image.new('P',(16,16));im.putpalette(detail_palette+[0]*720)
        im.putdata([0 if not p[3] else 1+min(range(15),key=lambda i:
                    sum((p[c]-colors[i][c])**2 for c in range(3))) for p in art.getdata()])
        patches.append(im)
    tiles=[bytes(64)]
    for im in patches:
        for y in (0,8):
            for x in (0,8):tiles.append(im.crop((x,y,x+8,y+8)).tobytes())
    header='#pragma once\n#include "bn_tile.h"\nnamespace decoration_art {\ninline constexpr bn::tile tiles[]={\n'
    packed=[bytes(tile[i] | (tile[i+1]<<4) for i in range(0,64,2)) for tile in tiles]
    header+='\n'.join('{{'+','.join(hex(v) for v in struct.unpack('<8I',tile))+'}},' for tile in packed)+'\n};\n}\n'
    (GEN/'decoration_art.h').write_text(header)
    atlas=Image.new('P',(64,16),0);atlas.putpalette(detail_palette+[0]*720)
    for i,im in enumerate(patches):atlas.paste(im,(i*16,0))
    atlas.save(ROOT/'artifacts/wasteland/decoration.png',transparency=0)
    # Browser/native renderers use the combined 8bpp palette. The GBA uses the
    # same RGB5 colours in a separate 16-colour bank and the packed local indices.
    return [224+pixel if pixel else 0 for pixel in b''.join(tiles)]

if __name__=='__main__':
    (ROOT/'artifacts').mkdir(exist_ok=True)
    from compile_recipe import generate as generate_recipe
    generate_recipe()
    from import_map2 import generate
    path, palette = generate(save, PALETTE, smooth)
    sprites()
    screens(path, palette)
    from town_assets import generate as generate_town
    generate_town(save)
    from open_world import generate as generate_open_world
    generate_open_world(palette,save,label)
    from wasteland_assets import generate as generate_wasteland
    generate_wasteland(palette,save,label)
    audio()
    from music_generator import generate as generate_music
    generate_music()
    print('Generated track, car directions, UI, font, particles, sound and adaptive music.')
