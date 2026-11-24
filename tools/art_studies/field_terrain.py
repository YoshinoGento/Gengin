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
    for y,scale,relief in [(-1,1,0),(.16,.985,.04),(.78,.94,.08),(1,.86,.04)]:
        rings.append([(x*scale,y-relief*d,z*scale) for (x,z),d in
                      zip(outline,(0,.4,.8,.9,.25,.5,1,.4),strict=True)])
    return shell(rings)


def bend_arc(a):
    return np.array([np.cos(a), np.sin(a)]) * 1.4 - .4


def bend_water():
    outline=[(-1,-1),(1,-1),*map(bend_arc,np.linspace(0,np.pi/2,7)),(-1,1)]
    return shell([[(x,y,z) for x,z in outline] for y in (-1,1)])


def bend_lining():
    # Six fitted voussoirs share a low footing. Radial joints follow the turn.
    stones=[]
    for i in range(6):
        ends=np.array([i,i+1])*np.pi/12
        def ring(t, y, joint):
            a,b=ends+[joint,-joint]
            pts=[]
            for theta,outer in ((a,False),(a,True),(b,True),(b,False)):
                ray=np.array([np.cos(theta),np.sin(theta)])
                radius=1.4 if not outer else 1.65/max(ray)
                if not outer: radius+=.08*t
                pts.append(((-.4+radius*ray[0])/1.25,y,(-.4+radius*ray[1])/1.25))
            return pts
        stones.append(shell([ring(0,-1,0),ring(.3,.52,.004),ring(1,.85+.075*(i%3),.012)]))
    return np.concatenate(stones)


def inner_fillet():
    # Concave outline: fan from the junction, not from its (possibly exterior) centroid.
    rim=np.sin(np.linspace(0,np.pi/2,7))
    arc=[(a-1,b-1) for a,b in zip(rim,rim[::-1])]
    top=np.array([(0,1,0),*((x,1,z) for x,z in arc)])
    base=top*[1,-1,1]
    faces=[]
    for i in range(1,len(top)-1):
        faces.extend([(top[0],top[i],top[i+1]),(base[0],base[i+1],base[i])])
    for i in range(len(top)):
        j=(i+1)%len(top)
        faces.extend([(top[i],base[i],base[j]),(top[i],base[j],top[j])])
    f=np.array(faces)
    return f if (f[:,0]*np.cross(f[:,1],f[:,2])).sum()>0 else f[:,::-1]


def write_terrain(destination=SOURCE):
    for name,mesh in (("soil_ridge",ridge()),("bank_stone",bank_stone()),
                      ("bend_water",bend_water()),("bend_lining",bend_lining()),("bend_fillet",inner_fillet())):
        assert np.isfinite(mesh).all() and np.abs(mesh).max()<=1.000001 and len(mesh)<=256
        save_obj(destination/(name+".obj"),mesh[:,::-1]*MIRROR_X)


if __name__=="__main__":
    write_terrain()
