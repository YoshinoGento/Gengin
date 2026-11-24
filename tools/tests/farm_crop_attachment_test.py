"""Check exported foliage against the smooth and ceremony fruit, in mature plant space."""
from pathlib import Path
import sys
import numpy as np

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/art_studies"))
from crop_remodel import load_obj

FARM = ROOT / "project/Resources/farm"
PARTS = {
    "pumpkin": ("vines", (.46,.65,.46), (.58,.80,.58), .015, .020),
    "tomato": ("stems", (.42,.60,.42), (.58,1.05,.58), .22, .020),
    "carrot": ("leaves", (.18,.55,.18), (.32,.37,.32), 0, .55*.96),
    "turnip": ("leaves", (.28,.40,.28), (.32,.37,.32), 0, .40*.96),
}

def parts(crop, ceremony):
    name, body_scale, leaf_scale, base, leaf_y = PARTS[crop]
    if crop == "turnip" and ceremony:
        body_scale, leaf_scale, leaf_y = (.65,.75,.65), (.4,.4,.4), .75*.96
    if crop == "tomato" and ceremony:
        body_scale = leaf_scale = (1,1,1)
        base = leaf_y = 0.
    folder = FARM / "contest/ceremony" if ceremony else FARM
    def read(suffix, scale, y):
        return load_obj(folder / f"crop_{crop}{suffix}.obj") * [-1,1,1] * scale + [0,y,0]
    return read("", body_scale, base), read("_"+name, leaf_scale, leaf_y)

def samples(mesh):
    weights = [(i/10, j/10, 1-(i+j)/10) for i in range(11) for j in range(11-i)]
    points = np.einsum("wk,fkd->fwd", weights, mesh)
    return np.unique(points.reshape(-1,3).round(7), axis=0)

def inside(points, mesh):
    # Upward ray crossings. Jitter only in the projection plane to break shared-edge ties.
    x,y,z = points.T
    x, z = x+1.731e-8, z+2.593e-8
    parity = np.zeros(len(points), dtype=bool)
    for a,b,c in mesh:
        ab,ac = b-a,c-a
        det = ab[0]*ac[2]-ab[2]*ac[0]
        if abs(det) < 1e-12: continue
        ix = np.flatnonzero((x>=min(a[0],b[0],c[0])) & (x<=max(a[0],b[0],c[0])) &
                            (z>=min(a[2],b[2],c[2])) & (z<=max(a[2],b[2],c[2])))
        dx,dz = x[ix]-a[0],z[ix]-a[2]
        u,v = (dx*ac[2]-dz*ac[0])/det,(ab[0]*dz-ab[2]*dx)/det
        parity[ix] ^= (u>=0)&(v>=0)&(u+v<=1)&(a[1]+u*ab[1]+v*ac[1]>y[ix])
    return parity

def sockets(crop, p, ceremony):
    x,y,z = p.T
    if crop == "pumpkin":
        return (np.hypot(x-.040,z-.006) <= .103) & (y >= .449)
    if crop == "tomato":
        if ceremony:
            return (np.hypot(x,z) <= .085) & (y >= .630)
        return ((np.hypot(x+.171,z-.045) <= .057) & (y>=.536) |
                (np.hypot(x-.202,z+.034) <= .045) & (y>=.637))
    crown = PARTS[crop][4] if crop != "turnip" or not ceremony else .75*.96
    return (np.hypot(x,z) <= .060) & (y >= crown-.003)

def in_body_space(p, crop, bend):
    # Rotate around each part's installed origin, then undo the body's own rotation.
    def rotation(response):
        ax,az = bend*response*np.sin(.45), -bend*response*np.cos(.45)
        cx,sx,cz,sz = np.cos(ax),np.sin(ax),np.cos(az),np.sin(az)
        return np.array([[cz,sz,0],[-cx*sz,cx*cz,sx],[sx*sz,-sx*cz,cx]])
    if crop in ("pumpkin", "tomato"):
        # Fruit and stalk now share the stalk pivot (verified on the actual C++ pose separately).
        return p
    leaf_y = PARTS[crop][4]
    return (p-[0,leaf_y,0]) @ rotation(.31) + [0,leaf_y,0]

def run():
    errors = []
    for crop in PARTS:
        for ceremony in (False, True):
            body, foliage = parts(crop, ceremony)
            p = samples(foliage)
            for wind in ((0,) if ceremony else (0,-.68,-.34,.34,.68)):
                q = in_body_space(p, crop, wind) if wind else p
                contact = q[inside(q, body)]
                crossed = contact[~sockets(crop, contact, ceremony)]
                if len(crossed):
                    errors.append((crop, ceremony, wind, len(crossed), crossed.min(axis=0), crossed.max(axis=0)))
                assert len(contact), f"detached crown: {crop}, {ceremony}, {wind}"
                if crop == "tomato" and not ceremony:
                    assert contact[:,0].min() < 0 < contact[:,0].max(), "detached tomato calyx"
            if ceremony and crop == "tomato":
                assert p[:,1].min() >= .630 and p[:,1].max() < .82, "expected only a calyx at the crown"
                # One fruit centred on its own flat base, not the old offset two-fruit cluster.
                section = np.column_stack((np.arange(-.6,.61,.02),np.full(61,.36),np.full(61,.015)))
                occupied = inside(section,body)
                assert occupied[30] and np.count_nonzero(np.diff(occupied))==2, "single tomato cross-section"
            if ceremony and crop == "pumpkin":
                assert p[:,1].min() > .45 and p[:,1].max() < .61, "unexpected vine below the stalk"
                assert np.hypot(p[:,0]-.04,p[:,2]-.006).max() < .11, "unexpected runner or leaf"
            print(f'Checked {crop} {"ceremony" if ceremony else "field"}: {len(p)} samples per pose')
    assert not errors, errors

if __name__ == "__main__":
    run()
