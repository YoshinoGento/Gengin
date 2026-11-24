"""Check the shipped terrain meshes against the tile-space contract."""
from collections import Counter
from pathlib import Path
import sys
import tempfile

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "art_studies"))
from crop_remodel import SOURCE, load_obj
from field_terrain import write_terrain
from study_mesh import obj_corner_normals


with tempfile.TemporaryDirectory() as folder:
    out = Path(folder)
    write_terrain(out)
    for name in ("soil_ridge.obj", "bank_stone.obj"):
        path = SOURCE / name
        assert path.read_bytes() == (out / name).read_bytes(), name
        faces = load_obj(path)
        assert 0 < len(faces) <= 128 and np.isfinite(faces).all()
        assert np.abs(faces).max() <= 1
        assert faces[:, :, 1].min() == -1 and faces[:, :, 1].max() == 1
        area = np.cross(faces[:, 1] - faces[:, 0], faces[:, 2] - faces[:, 0])
        assert np.linalg.norm(area, axis=1).min() > 1e-8
        assert (area * faces[:, 0]).sum() > 0
        assert (obj_corner_normals(path) * area[:, None, :]).sum(axis=2).min() > 0
        edges = Counter((tuple(f[i]), tuple(f[(i + 1) % 3])) for f in faces for i in range(3))
        assert all(count == 1 and edges[(b, a)] == 1 for (a, b), count in edges.items())
        print(f"PASS {name}: {len(faces)} triangles, closed mesh inside unit box")
    for name in ("bend_water.obj", "bend_lining.obj", "bend_fillet.obj"):
        path=SOURCE/name
        assert path.read_bytes()==(out/name).read_bytes()
        faces=load_obj(path)
        assert np.isfinite(faces).all() and np.abs(faces).max()<=1 and len(faces)<=256
        area=np.cross(faces[:,1]-faces[:,0],faces[:,2]-faces[:,0])
        assert np.linalg.norm(area,axis=1).min()>1e-9
        assert (obj_corner_normals(path)*area[:,None,:]).sum(axis=2).min()>0
        edges=Counter((tuple(f[i]),tuple(f[(i+1)%3])) for f in faces for i in range(3))
        assert all(edges[b,a]==count for (a,b),count in edges.items())
        print(f"PASS {name}: {len(faces)} triangles, finite geometry, closed winding")
