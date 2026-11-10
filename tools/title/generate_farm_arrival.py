"""Original arrival props/scenery; reuse the existing countryside OBJ export contract."""
from pathlib import Path
import argparse
import json
import math
import random
import struct
import wave
import generate_title_landscape as landscape
import generate_title_grass as grass

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "project/Resources/farm/arrival"
Mesh = landscape.Mesh
TREE_POSITIONS = ((-13,22),(-22,36),(6,30),(18,25),(28,42))
GRASS_SEED = 71893
SHADOW_PATCHES = 60
GRASS_BOUNDS = (-30,30,-14,46)
GRASS_JITTER = .06
GRASS_SCALE = (.95,1.05)
# Runtime plot, two-track road, tool bench and close-up leaf, before wind/patch expansion.
PLOT_KEEPOUT = (-4,4,3.2,12.8)  # Maximum 5x6 farm plus a walking margin.
GRASS_KEEPOUTS = (PLOT_KEEPOUT,(7.1,8.95,-15,24),
                 (-5.65,-3.60,6.5,7.7),(-5.6,-4.35,2.35,3.25))


def build_grass_layout():
    rng = random.Random(GRASS_SEED)
    patches = []
    x0,x1,z0,z1 = GRASS_BOUNDS
    for zi in range(int((z1-z0)/grass.PATCH_SPACING)+1):
        for xi in range(int((x1-x0)/grass.PATCH_SPACING)+1):
            x = x0+xi*grass.PATCH_SPACING+rng.uniform(-GRASS_JITTER,GRASS_JITTER)
            z = z0+zi*grass.PATCH_SPACING+rng.uniform(-GRASS_JITTER,GRASS_JITTER)
            scale = rng.uniform(*GRASS_SCALE)
            extent = (scale*(grass.PATCH_RADIUS+grass.MAX_LEAN+grass.MAX_WIDTH/2)
                      +grass.MAX_WIND*(grass.MAX_HEIGHT*scale)**2/grass.BEND_HEIGHT)
            if any(x+extent >= a and x-extent <= b and z+extent >= c and z-extent <= d
                   for a,b,c,d in GRASS_KEEPOUTS):
                continue
            patches.append({"position":[x,grass.terrain_height(x,z),z],"scale":scale,
                            "yaw":0,"tint":[.40,.55,.22,1]})
    patches.sort(key=lambda p: math.hypot(p["position"][0],p["position"][2]-8))
    if not 0 < len(patches) <= grass.MAX_PATCHES:
        raise ValueError("Arrival grass instance budget exceeded")
    return {"version":1,"shadowCount":min(SHADOW_PATCHES,len(patches)),"patches":patches}


def save_grass():
    layout = build_grass_layout()
    (OUT/"grass_layout.json").write_text(json.dumps(layout),encoding="ascii")
    return len(layout["patches"])


def save(mesh, name, raw_uv=False):
    lines = ["# Original geometry: tools/title/generate_farm_arrival.py", "s off"]
    for points, uv, normal, material, _ in mesh.triangles:
        for i in (0, 2, 1):
            p = points[i]
            lines.append(f"v {-p[0]:.6f} {p[1]:.6f} {p[2]:.6f}")
            u, v = uv[i]
            if not raw_uv:
                u = (material % 3 + .025 + u * .95) / 3
                v = (material // 3 + .025 + v * .95) / 2
            lines.append(f"vt {u:.6f} {1-v:.6f}")
        lines.append(f"vn {-normal[0]:.6f} {normal[1]:.6f} {normal[2]:.6f}")
    for i in range(len(mesh.triangles)):
        lines.append("f " + " ".join(f"{3*i+j+1}/{3*i+j+1}/{i+1}" for j in range(3)))
    (OUT / (name + ".obj")).write_text("\n".join(lines) + "\n", encoding="ascii")
    return len(mesh.triangles)


def cylinder(mesh, start, end, radius, material, section, segments=12):
    direction = tuple(end[i]-start[i] for i in range(3))
    length = math.sqrt(sum(v*v for v in direction))
    axis = tuple(v/length for v in direction)
    helper = (1,0,0) if abs(axis[0]) < .8 else (0,1,0)
    cross = lambda a,b: (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])
    u = cross(axis,helper)
    norm = math.sqrt(sum(v*v for v in u))
    u = tuple(v/norm for v in u)
    v = cross(axis,u)
    def ring(center, i):
        a = math.tau*i/segments
        return tuple(center[j]+radius*(u[j]*math.cos(a)+v[j]*math.sin(a)) for j in range(3))
    for i in range(segments):
        mesh.quad((ring(start,i),ring(end,i),ring(end,i+1),ring(start,i+1)),material,section)
        mesh.triangle((start,ring(start,i+1),ring(start,i)),((.5,.5),(0,0),(1,1)),material,section)
        mesh.triangle((end,ring(end,i),ring(end,i+1)),((.5,.5),(0,0),(1,1)),material,section)


def build_scenery():
    mesh = Mesh()
    mesh.ground()
    # Below all legal terrain levels (minimum=0); replaces only the flat ground draw.
    mesh.rectangle(*PLOT_KEEPOUT,-.225,landscape.DIRT,"under_plot",2)
    mesh.rectangle(7.1,7.7,-15,24,-.227,landscape.DIRT,"road",1)
    mesh.rectangle(8.35,8.95,-15,24,-.227,landscape.DIRT,"road",1)
    for index,(x,z) in enumerate(TREE_POSITIONS):
        mesh.tree(x,z,4.3,1.45,20261041+index,"trunk")
    return mesh


def build():
    OUT.mkdir(parents=True, exist_ok=True)
    counts = {"scenery": save(build_scenery(),"scenery")}

    leaf = Mesh()
    # All blades grow upward from one rooted clump. The hero tip droops without folding back in X.
    def blade(height, reach, yaw, root=(0,0,0), hero=False):
        def section(t, side):
            x=reach*t**1.6
            y=height*(3*t-2*t*t if hero else 1.55*t-.55*t*t)
            width=max(.0005,(.018+.032*math.sin(math.pi*t))*(1-t))
            # A curve-normal ribbon keeps its width and winding through the drooping tip.
            dx=1.6*reach*t**.6
            dy=height*(3-4*t if hero else 1.55-1.1*t)
            length=math.hypot(dx,dy)
            x+=side*width*dy/length
            y-=side*width*dx/length
            return (root[0]+x*math.cos(yaw),root[1]+y,root[2]+x*math.sin(yaw))
        for i in range(16):
            a,b=i/16,(i+1)/16
            leaf.quad((section(a,-1),section(b,-1),section(b,1),section(a,1)),landscape.FOLIAGE,"blade")
    blade(.8,.64,0,hero=True)
    for height,reach,yaw in ((.46,.22,1.7),(.58,.27,2.7),(.38,.31,3.8),(.51,.20,4.8),(.32,.22,5.6)):
        blade(height,reach,yaw)
    counts["leaf"] = save(leaf,"leaf")

    drop = Mesh()
    rings=((-1,.05),(-.75,.65),(-.25,1),(.25,.90),(.7,.52),(1.35,.025))
    for (ya,ra),(yb,rb) in zip(rings,rings[1:]):
        def p(y,r,i):
            a=math.tau*i/20
            return (r*math.cos(a),y,r*math.sin(a))
        for i in range(20):
            drop.quad((p(ya,ra,i),p(yb,rb,i),p(yb,rb,i+1),p(ya,ra,i+1)),0,"dew")
    counts["drop"]=save(drop,"drop",True)
    wet=Mesh()
    for i in range(32):
        a,b=math.tau*i/32,math.tau*(i+1)/32
        wet.triangle(((0,0,0),(math.cos(a),0,math.sin(a)),(math.cos(b),0,math.sin(b))),
                     ((.5,.5),(0,0),(1,1)),0,"wet")
    counts["wet"]=save(wet,"wet",True)

    wood,metal=Mesh(),Mesh()
    wood.box(0,.07,0,.85,.07,.40,landscape.WOOD,"bench")
    for x in (-.68,.68):
        wood.box(x,-.01,.25,.08,.20,.08,landscape.WOOD,"leg")
    cylinder(wood,(-.68,.20,-.25),(-.68,1.26,.07),.035,landscape.WOOD,"hoe_handle")
    metal.box(-.68,.22,-.39,.20,.035,.17,0,"hoe_blade")
    cylinder(metal,(.40,.15,.02),(.40,.52,.02),.22,0,"watering_can",20)
    cylinder(metal,(.53,.25,.02),(.91,.49,.02),.045,0,"spout")
    cylinder(metal,(.91,.49,.02),(.97,.53,.02),.095,0,"rose")
    for i in range(12):
        a,b=math.pi*i/12,math.pi*(i+1)/12
        cylinder(metal,(.40+.19*math.cos(a),.50+.24*math.sin(a),.02),
                 (.40+.19*math.cos(b),.50+.24*math.sin(b),.02),.020,0,"handle",6)
    counts["tools_wood"]=save(wood,"tools_wood")
    counts["tools_metal"]=save(metal,"tools_metal",True)
    for name,slot in (("packet_carrot",1),("packet_tomato",0),("packet_pumpkin",1)):
        packet=Mesh()
        packet.quad(((-.14,0,0),(-.14,.38,0),(.14,.38,0),(.14,0,0)),0,"packet",
                    ((slot*.5,1),(slot*.5,0),((slot+1)*.5,0),((slot+1)*.5,1)))
        counts[name]=save(packet,name,True)

    grass_count = save_grass()
    # Authored, quiet damp impact: no external sound acquisition or redistribution.
    rate=48000
    noise=random.Random(32)
    samples=[]
    for i in range(int(.28*rate)):
        t=i/rate
        envelope=(1-math.exp(-t*900))*math.exp(-t*35)
        value=.18*envelope*(.65*math.sin(math.tau*(740*t-650*t*t))+.35*noise.uniform(-1,1))
        samples.append(struct.pack('<h',int(max(-1,min(1,value))*32767)))
    with wave.open(str(OUT/"dew.wav"),'wb') as audio:
        audio.setparams((1,2,rate,0,'NONE','not compressed'))
        audio.writeframes(b''.join(samples))
    with wave.open(str(ROOT/'project/Resources/title/audio/morning.wav'),'rb') as source:
        with wave.open(str(OUT/'morning.wav'),'wb') as target:
            target.setparams(source.getparams())
            target.writeframes(source.readframes(source.getframerate()*16))
    (OUT/"manifest.json").write_text(json.dumps({"triangles":counts,"grassInstances":grass_count,
        "plotKeepout":PLOT_KEEPOUT,"authorship":"Original procedural geometry and impact sound; existing project countryside/packet textures and grass models reused."},indent=2)+'\n',encoding="ascii")
    print(json.dumps(counts),grass_count,"grass instances")


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group()
    group.add_argument('--scenery-only', action='store_true')
    group.add_argument('--grass-only', action='store_true')
    args = parser.parse_args()
    if args.scenery_only:
        count = save(build_scenery(), 'scenery')
        path = OUT/'manifest.json'
        manifest = json.loads(path.read_text(encoding='ascii'))
        manifest['triangles']['scenery'] = count
        manifest['plotKeepout'] = PLOT_KEEPOUT
        path.write_text(json.dumps(manifest,indent=2)+'\n',encoding='ascii')
        print(f'PASS arrival scenery: {count} triangles; other assets untouched')
    elif args.grass_only:
        count = save_grass()
        path = OUT/'manifest.json'
        manifest = json.loads(path.read_text(encoding='ascii'))
        manifest['grassInstances'] = count
        path.write_text(json.dumps(manifest,indent=2)+'\n',encoding='ascii')
        print(f'PASS arrival grass: {count} instances; other assets untouched')
    else:
        build()
