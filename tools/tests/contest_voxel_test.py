"""Geometry-only regression: keep source crop proportions and bounded solid surface."""
from pathlib import Path
import sys
import math
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'title'))
from contest_voxel import solid_crop, CROP_CELL
from voxel_scenery import surface_quads
from generate_contest_ceremony import Mesh, grass_blade

root=Path(__file__).resolve().parents[2]
for crop in ('turnip','carrot','tomato','pumpkin'):
    path=root/'project/Resources/farm'/f'crop_{crop}.obj'
    source=path.read_text(encoding='utf-8')
    cells=solid_crop(source)
    assert cells==solid_crop(source)
    points=[]
    for line in source.splitlines():
        if line.startswith('v '):
            x,y,z=map(float,line.split()[1:]); points.append((-x,y,z))
    for axis in range(3):
        assert abs(min(p[axis] for p in points)-min(p[axis] for p in cells)*CROP_CELL)<CROP_CELL*1.6
        assert abs(max(p[axis] for p in points)-(max(p[axis] for p in cells)+1)*CROP_CELL)<CROP_CELL*1.6
    quads=surface_quads(cells)
    for quad in quads:
        a,b,c=quad[:3]
        u=tuple(b[i]-a[i] for i in range(3)); v=tuple(c[i]-a[i] for i in range(3))
        n=(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
        axis=next(i for i in range(3) if n[i])
        center=[sum(p[i] for p in quad)/4 for i in range(3)]
        sign=1 if n[axis]>0 else -1
        inside=center.copy(); outside=center.copy()
        inside[axis]-=sign*.01; outside[axis]+=sign*.01
        assert tuple(math.floor(v) for v in inside) in cells
        assert tuple(math.floor(v) for v in outside) not in cells
    print(f'PASS {crop}: {len(cells)} solid cells, {len(quads)} external quads')
for invalid in (0,float('nan'),.001):
    try: solid_crop('',invalid)
    except ValueError: pass
    else: raise AssertionError('invalid grid accepted')

for yaw in (0,.7,2.5):
    mesh=Mesh()
    grass_blade(mesh,1,2,.3,yaw,.1)
    assert len(mesh.triangles)==8
    vertices=set(p for triangle in mesh.triangles for p in triangle[0])
    center=tuple(sum(p[i] for p in vertices)/len(vertices) for i in range(3))
    edges={}
    for points,_,normal,_,_ in mesh.triangles:
        face_center=tuple(sum(p[i] for p in points)/3 for i in range(3))
        assert sum(normal[i]*(face_center[i]-center[i]) for i in range(3))>0
        for a,b in zip(points,points[1:]+points[:1]):
            key=tuple(sorted((a,b))); edges[key]=edges.get(key,0)+1
    assert all(count==2 for count in edges.values())
    assert abs(min(p[1] for p in vertices)+.24)<1e-6
    assert abs(max(p[1] for p in vertices)-.06)<1e-6
print('PASS tapered grass: closed surface, outward normals, grounded roots')
