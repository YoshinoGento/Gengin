"""Offline solid sampling of authored crop meshes; no runtime voxel allocation."""
import math
from voxel_scenery import MAX_SOLID_CELLS, MAX_SAMPLE_CELLS, surface_quads

CROP_CELL = .065
MAX_COLUMNS = 4096


def solid_crop(source, cell=CROP_CELL):
    if not math.isfinite(cell) or not .05 <= cell <= .15:
        raise ValueError('Crop cell outside supported budget')
    vertices, faces = [], []
    for line in source.splitlines():
        fields = line.split()
        if fields[:1] == ['v']:
            x, y, z = map(float, fields[1:4])
            vertices.append((-x, y, z))
        elif fields[:1] == ['f']:
            if len(fields) != 4:
                raise ValueError('Crop source must be triangulated')
            faces.append(tuple(int(token.split('/')[0])-1 for token in fields[1:]))
    if not vertices or not faces or not all(math.isfinite(c) for p in vertices for c in p):
        raise ValueError('Invalid crop geometry')
    if any(i < 0 or i >= len(vertices) for face in faces for i in face):
        raise ValueError('Crop index out of range')
    bounds = [(math.floor(min(p[i] for p in vertices)/cell),
               math.ceil(max(p[i] for p in vertices)/cell)) for i in range(3)]
    if math.prod(b-a for a,b in bounds) > MAX_SAMPLE_CELLS:
        raise ValueError('Crop sample volume budget exceeded')
    if math.prod(b-a for a,b in bounds[1:]) > MAX_COLUMNS:
        raise ValueError('Crop column budget exceeded')
    cells = set()
    triangles = [tuple(vertices[i] for i in face) for face in faces]
    for iy in range(*bounds[1]):
        for iz in range(*bounds[2]):
            # A tiny deterministic offset avoids rays exactly on shared edges.
            y, z = (iy+.50013)*cell, (iz+.50027)*cell
            hits = []
            for a,b,c in triangles:
                by,bz,cy,cz = b[1]-a[1],b[2]-a[2],c[1]-a[1],c[2]-a[2]
                det = by*cz-bz*cy
                if abs(det) < 1e-10:
                    continue
                u = ((y-a[1])*cz-(z-a[2])*cy)/det
                v = (by*(z-a[2])-bz*(y-a[1]))/det
                if u >= 0 and v >= 0 and u+v <= 1:
                    hits.append(a[0]+u*(b[0]-a[0])+v*(c[0]-a[0]))
            distinct = []
            for hit in sorted(hits):
                if not distinct or hit-distinct[-1] > 1e-6:
                    distinct.append(hit)
            if len(distinct) % 2:
                raise ValueError('Open crop volume: cannot pair ray intersections')
            for left,right in zip(distinct[::2],distinct[1::2]):
                for ix in range(math.ceil(left/cell-.5),math.ceil(right/cell-.5)):
                    cells.add((ix,iy,iz))
                    if len(cells) > MAX_SOLID_CELLS:
                        raise ValueError('Crop occupancy budget exceeded')
    if not cells or len(cells) > MAX_SOLID_CELLS:
        raise ValueError('Crop occupancy budget exceeded')
    return cells


def append_crop(mesh, source, material):
    cells = solid_crop(source)
    for quad in surface_quads(cells):
        points = tuple(tuple(v*CROP_CELL for v in p) for p in quad)
        mesh.quad(points, material, 'voxel_crop')
    return len(cells)
