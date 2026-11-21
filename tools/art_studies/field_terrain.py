"""Static meshes in unit-box space, scaled and sheared by the existing tile layout."""
import numpy as np
from crop_remodel import SOURCE, shell, save_obj
from study_mesh import MIRROR_X


def ridge():
    profile=[(-1,-1),(1,-1),(.93,-.34),(.63,.46),(.21,.95),(-.18,1),(-.60,.49),(-.89,-.21)]
    rings=[]
    for z,width,height,shift in [(-1,.37,.20,-.07),(-.82,.81,.76,-.08),(-.37,.91,.95,.045),
                                  (.22,.94,1,-.03),(.75,.86,.86,.07),(1,.46,.28,.02)]:
        rings.append([(x*width+shift,(y+1)*height-1,z) for x,y in profile])
    mound=shell(rings)
    pieces=[mound]
    # Three attached clods break the straight furrow edge without another scene object.
    for x,z,s in [(-.81,-.54,.82),(.82,.12,1.),(-.78,.62,.68)]:
        ledge=np.array([[-.15,-.79,-.060],[.16,-.79,-.044],[.14,-.79,.055],[-.12,-.79,.081]])
        bottom=np.array([0.,-1.,0.]); peak=np.array([-.028,.38,.011])
        faces=[]
        for i,p in enumerate(ledge):
            q=ledge[(i+1)%4]
            faces.extend(((bottom,p,q),(peak,q,p)))
        pieces.append(np.array(faces)*[s,1,s]+[x,0,z])
    return np.concatenate(pieces)


def bank_stone():
    # A continuous footing below the waterline; only the exposed stone shoulders recede.
    outline=[(-1,-.86),(-.89,-1),(.80,-1),(1,-.82),(1,.92),(.87,1),(-.84,1),(-1,.75)]
    rings=[]
    for y,scale,relief in [(-1,1,0),(.42,.99,.035),(.80,.95,.08),(1,.82,.03)]:
        rings.append([(x*scale,y-relief*d,z*scale) for (x,z),d in
                      zip(outline,(0,.4,.8,.9,.25,.5,1,.4),strict=True)])
    return shell(rings)


def write_terrain(destination=SOURCE):
    for name,mesh in (("soil_ridge",ridge()),("bank_stone",bank_stone())):
        assert np.isfinite(mesh).all() and np.abs(mesh).max()<=1 and len(mesh)<=128
        save_obj(destination/(name+".obj"),mesh[:,::-1]*MIRROR_X)


if __name__=="__main__":
    write_terrain()
