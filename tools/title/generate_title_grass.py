"""Generate original tapered meadow leaves, with a restrained four-color pixel palette."""
import json
import math
from pathlib import Path
import random
from generate_title_landscape import Mesh, GRASS, GROUND_Y, FARM_EXCLUSION, ground_height, build as landscape_build

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "project/Resources/title"
TUFT_COUNT = 96
BLADE_COUNT = 5
SEGMENT_COUNT = 3
MAX_HEIGHT = .46
PATCH_RADIUS = 1.35
MAX_LEAN = .18
MAX_WIDTH = .12
MAX_WIND = .68
BEND_HEIGHT = .35
MAX_PATCHES = 1024
PATCH_SPACING = 2.5
SHADOW_DISTANCE = 24.0


def terrain_height(x, z):
    # Match the actual five-unit landscape triangles, not just its analytic height function.
    x0 = -50+math.floor((x+50)/5)*5
    z0 = -74+math.floor((z+74)/5)*5
    u,v = (x-x0)/5,(z-z0)/5
    a,b,c,d = (ground_height(xx,zz) for xx,zz in
               ((x0,z0),(x0,z0+5),(x0+5,z0+5),(x0+5,z0)))
    return a+(b-a)*v+(c-b)*u if v >= u else a+(d-a)*u+(c-d)*v


def patch_layout():
    rng = random.Random(1072026)
    road = [(min(p[0] for p in points)-.1,max(p[0] for p in points)+.1,
             min(p[2] for p in points)-.1,max(p[2] for p in points)+.1)
            for points,_,_,_,section in landscape_build().triangles
            if section in ("road","road_median","road_verge","road_posts")]
    pond = json.loads((OUT/"pond_layout.json").read_text(encoding="utf-8"))
    px,_,pz = pond["center"]
    rx,rz = pond["radii"]
    margin = pond["shoreWidth"]+.12
    forbidden = [FARM_EXCLUSION,*road,(px-rx-margin,px+rx+margin,pz-rz-margin,pz+rz+margin)]
    patches = []
    for zi in range(35):
        for xi in range(38):
            x,z = -46+xi*PATCH_SPACING+rng.uniform(-.06,.06),-4+zi*PATCH_SPACING+rng.uniform(-.06,.06)
            scale = rng.uniform(.95,1.05)
            extent = scale*(PATCH_RADIUS+MAX_LEAN+MAX_WIDTH/2)+MAX_WIND*(MAX_HEIGHT*scale)**2/BEND_HEIGHT
            if abs(x)+extent > 49 or z+extent > 85:
                continue
            if any(x+extent >= a and x-extent <= b and z+extent >= c and z-extent <= d
                   for a,b,c,d in forbidden):
                continue
            y = terrain_height(x,z)
            dx,dy,dz = x-6.5,y-1.9,z+7.5
            forward = dx*math.sin(-.45)+dz*math.cos(-.45)
            depth = forward*math.cos(.12)-dy*math.sin(.12)
            right = dx*math.cos(-.45)-dz*math.sin(-.45)
            up = forward*math.sin(.12)+dy*math.cos(.12)
            # Offline camera footprint filter only; no runtime camera/scene mutation.
            if depth <= 1 or abs(right) > depth*math.tan(.72/2)*1280/720+extent or \
               up+MAX_HEIGHT < -depth*math.tan(.72/2)-extent:
                continue
            patches.append({"position":[x,y,z],"scale":scale,"yaw":0,
                            "tint":[rng.uniform(.86,1),rng.uniform(.92,1),.88,1]})
    # Near instances form a contiguous prefix for the existing single Shadow draw.
    patches.sort(key=lambda p: math.hypot(p["position"][0],p["position"][2]-6))
    assert 0 < len(patches) <= MAX_PATCHES
    return patches


def build(blade_count=BLADE_COUNT, segment_count=SEGMENT_COUNT):
    mesh = Mesh()
    for tuft in range(TUFT_COUNT):
        # Each LOD starts from the same roots and retains the first three blades.
        rng = random.Random(20261007+tuft)
        # Stratified square coverage removes circular-patch holes and uncontrolled overlap.
        x = ((tuft%12+rng.uniform(.08,.92))/12*2-1)*PATCH_RADIUS
        z = ((tuft//12+rng.uniform(.08,.92))/8*2-1)*PATCH_RADIUS
        tuft_height = rng.uniform(.24,MAX_HEIGHT)
        fan = rng.uniform(0,math.tau)
        for blade in range(blade_count):
            yaw = fan + blade*2.40 + rng.uniform(-.26,.26)
            height = tuft_height*(rng.uniform(.87,1) if blade==0 else rng.uniform(.54,.89))
            width = rng.uniform(.065,MAX_WIDTH)
            lean = rng.uniform(.035,.09) if blade==0 else rng.uniform(.09,MAX_LEAN)
            twist = rng.uniform(-.65,.65)
            shade = rng.randrange(2)
            def leaf_point(level, side):
                # Narrow sheath, broad shoulder, then a twisted pointed tip.
                profile = .52 + .48*math.sin(math.pi*level)
                half_width = 0 if level == 1 else round(width*profile*40)/80
                along = lean*level**1.7
                edge_yaw = yaw + twist*level
                return (x+along*math.cos(yaw)-side*half_width*math.sin(edge_yaw),
                        height*level,z+along*math.sin(yaw)+side*half_width*math.cos(edge_yaw))
            for segment in range(segment_count):
                low,high = segment/segment_count,(segment+1)/segment_count
                a,b,c,d = leaf_point(low,-1),leaf_point(high,-1),leaf_point(high,1),leaf_point(low,1)
                band = min(3,shade+segment//2)
                uv = (((band+.5)/4,tuft/TUFT_COUNT),)*3
                mesh.triangle((a,b,d),uv,GRASS,"leaf")
                if high != 1:
                    mesh.triangle((b,c,d),uv,GRASS,"leaf")
    return mesh


def save_mesh(mesh, filename):
    lines = ["# Original meadow leaves; generated by tools/title/generate_title_grass.py", "o TitleGrassPatch", "s off"]
    for points, shade_uv, normal, _, _ in mesh.triangles:
        for i in (0,2,1):
            x,y,z = points[i]
            lines.append(f"v {-x:.7f} {y:.7f} {z:.7f}")
            lines.append(f"vt {shade_uv[i][0]:.7f} {1-shade_uv[i][1]:.7f}")
        lines.append(f"vn {-normal[0]:.7f} {normal[1]:.7f} {normal[2]:.7f}")
    for i in range(len(mesh.triangles)):
        lines.append("f "+" ".join(f"{i*3+j+1}/{i*3+j+1}/{i+1}" for j in range(3)))
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/filename).write_text("\n".join(lines)+"\n",encoding="ascii")


def save():
    mesh, far = build(), build(3,2)
    save_mesh(mesh,"grass_patch.obj")
    save_mesh(far,"grass_far_patch.obj")
    patches = patch_layout()
    shadow_count = sum(math.hypot(p["position"][0],p["position"][2]-6) <= SHADOW_DISTANCE for p in patches)
    layout = {"version":1,"shadowCount":shadow_count,"patches":patches}
    (OUT/"grass_layout.json").write_text(json.dumps(layout,indent=2)+"\n",encoding="ascii")
    metadata = {"tuftsPerPatch":TUFT_COUNT,"bladesPerTuft":BLADE_COUNT,"segmentsPerBlade":SEGMENT_COUNT,
                "trianglesPerPatch":len(mesh.triangles),"instances":len(patches),"shadowInstances":shadow_count,
                "farTrianglesPerPatch":len(far.triangles),"maximumBladeHeight":MAX_HEIGHT,"opaque":True}
    (OUT/"grass_mesh.json").write_text(json.dumps(metadata,indent=2)+"\n",encoding="ascii")
    print(json.dumps(metadata))


if __name__ == "__main__":
    save()
