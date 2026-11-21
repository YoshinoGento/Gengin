"""Validate the original crop foliage geometry without a graphics device."""
import importlib.util
import math
import tempfile
from collections import Counter
from pathlib import Path

spec = importlib.util.spec_from_file_location("crop_generator", Path(__file__).parents[1] / "generate_farm_crop_meshes.py")
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)

def validate(triangles):
    assert 0 < len(triangles) <= 800
    edges = Counter()
    volume = 0
    for a, b, c in triangles:
        for p in (a, b, c):
            assert all(math.isfinite(v) for v in p)
            assert -.000001 <= p[1] <= 1.000001
            assert math.hypot(p[0], p[2]) <= 1.000001 # OBJ stores seven decimal places.
        u = [b[i]-a[i] for i in range(3)]
        v = [c[i]-a[i] for i in range(3)]
        cross = (u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0])
        assert sum(v*v for v in cross) > 1e-16
        volume += sum(a[i]*cross[i] for i in range(3))/6
        for p, q in ((a,b), (b,c), (c,a)):
            edges[(p,q)] += 1
    assert volume > 0
    assert all(count == edges[(q,p)] for (p,q),count in edges.items())

turnip = generator.crop_foliage_mesh()
validate(turnip)
points = [tuple(map(float,s.split()[1:])) for s in (generator.ROOT/"crop_turnip_leaves.obj").read_text().splitlines() if s.startswith("v ")]
assert points == [tuple(round(v,7) for v in p) for tri in turnip for p in tri]
with tempfile.TemporaryDirectory() as folder:
    out=Path(folder)
    generator.generate_authored_crops(out)
    files=sorted(out.glob("*.obj"))
    assert len(files)==6
    for asset in files:
        shipped=generator.ROOT/asset.name
        assert shipped.read_bytes()==asset.read_bytes(),shipped
        vertices,faces=[],[]
        for row in shipped.read_text().splitlines():
            fields=row.split()
            if fields[:1]==["v"]: vertices.append(tuple(map(float,fields[1:])))
            if fields[:1]==["f"]:
                face=[int(token.split("/")[0])-1 for token in fields[1:]]
                assert len(face)==3 and all(0<=i<len(vertices) for i in face)
                faces.append(tuple(vertices[i] for i in face))
        validate(faces)
        print(f"PASS {asset.name}: {len(faces)} triangles, deterministic indexed OBJ")
