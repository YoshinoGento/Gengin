"""Round-trip checks and skin shading shared by the field-space crop studies."""
import numpy as np

from crop_remodel import check, load_obj, save_obj

MIRROR_X = np.array([-1,1,1])


def normalize(v):
    return v/np.linalg.norm(v,axis=-1,keepdims=True)


def skin_normals(f):
    area=np.cross(f[:,1]-f[:,0],f[:,2]-f[:,0])
    vertices,indices=np.unique(f.reshape(-1,3).round(7),axis=0,return_inverse=True)
    indices=indices.reshape(-1,3)
    sums=np.zeros_like(vertices)
    for k in range(3):
        np.add.at(sums,indices[:,k],area)
    # Keep some planar light breaks alongside the round skin; don't soften stems or sepals.
    return normalize(.80*normalize(sums)[indices]+.20*normalize(area)[:,None,:])


def obj_corner_normals(path):
    rows=[s.split() for s in path.read_text(encoding="ascii").splitlines() if s]
    vn=np.array([[float(v) for v in row[1:]] for row in rows if row[0]=="vn"])
    indices=np.array([[int(v.split("/")[2])-1 for v in row[1:]] for row in rows if row[0]=="f"])
    assert indices.min()>=0 and indices.max()<len(vn)
    assert np.allclose(np.linalg.norm(vn,axis=1),1,atol=1e-6)
    return vn[indices]


def export_part(path,mesh,smooth=False):
    ns=skin_normals(mesh) if smooth else None
    # Author in field space; the OBJ importer reflects X and reverses winding.
    save_obj(path,mesh[:,::-1]*MIRROR_X,None if ns is None else ns[:,::-1]*MIRROR_X)
    exported=load_obj(path)
    check(exported)
    actual=exported[:,::-1]*MIRROR_X
    assert np.allclose(actual,mesh,atol=1e-6)
    vn=obj_corner_normals(path)[:,::-1]*MIRROR_X
    fn=normalize(np.cross(actual[:,1]-actual[:,0],actual[:,2]-actual[:,0]))
    assert (vn*fn[:,None,:]).sum(axis=2).min()>0
    if ns is not None:
        assert np.allclose(vn,ns,atol=1e-6)
    return actual,vn if ns is not None else None
