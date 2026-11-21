"""First crop sculpt study. Exports only to generated/art_studies, never to game assets."""
from collections import Counter
from math import cos, sin, tau
from pathlib import Path
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "project/Resources/farm"
OUT = ROOT / "generated/art_studies/crop_remodel_01"
sys.path.insert(0, str(ROOT / "tools/title"))
from contest_voxel import solid_crop


def shell(rings):
    rings = np.array(rings)
    n = len(rings[0])
    faces = []
    for lower, upper in zip(rings, rings[1:]):
        for i in range(n):
            j = (i+1) % n
            faces.extend([(lower[i], upper[i], upper[j]), (lower[i], upper[j], lower[j])])
    for i in range(n):
        j = (i+1) % n
        faces += [(rings[0].mean(axis=0), rings[0][i], rings[0][j]),
                  (rings[-1].mean(axis=0), rings[-1][j], rings[-1][i])]
    f = np.array(faces)
    volume = (f[:,0] * np.cross(f[:,1], f[:,2])).sum()
    return f if volume > 0 else f[:, ::-1]


def fruit(n, profile, polar):
    return shell([[polar(tau*i/n, y, r) for i in range(n)] for y, r in profile])


def tube(points, radii, sides=5):
    points = np.array(points)
    rings = []
    for i, p in enumerate(points):
        axis = points[min(i+1, len(points)-1)] - points[max(0, i-1)]
        axis /= np.linalg.norm(axis)
        # Use the previous cross-section to keep bent stems from twisting at turns.
        ref = u if i else np.eye(3)[np.abs(axis).argmin()]
        u = ref - ref.dot(axis)*axis
        u /= np.linalg.norm(u)
        v = np.cross(axis, u)
        rings.append([p+radii[i]*(u*cos(k*tau/sides)+v*sin(k*tau/sides)) for k in range(sides)])
    return shell(rings)


def blade(a,b,width,fold):
    a,b = np.array(a),np.array(b)
    side = np.cross(b-a,[0,1,0]); side /= np.linalg.norm(side)
    m = a+(b-a)*.51
    ridge, base = m+[0,fold,0], m-[0,fold*.50,0]
    contour = [a,m+width*side,b,m-width*side]
    faces = []
    for i in range(4):
        j=(i+1)%4
        faces += [(ridge,contour[i],contour[j]),(base,contour[j],contour[i])]
    f=np.array(faces)
    return f if (f[:,0]*np.cross(f[:,1],f[:,2])).sum()>0 else f[:,::-1]


def carrot():
    def polar(a,y,r):
        r *= .965+.025*cos(3*a+y)+.01*sin(5*a)
        return (.11*(1-y)**2+r*cos(a),y,r*sin(a))
    body=fruit(12,[(0,.025),(.14,.10),(.33,.32),(.60,.62),(.82,.91),(.93,.96),(.986,.67),(1,.22)],polar)
    parts=[]
    for a,h,reach in [(0,.83,.63),(1.49,.99,.44),(2.75,.71,.74),(4.02,.91,.60),(5.26,.68,.70)]:
        radial=np.array([cos(a),0,sin(a)])
        tangent=np.array([-sin(a),0,cos(a)])
        def point(t):
            return radial*(reach*t**1.35)+np.array([0,.022+h*(t-.12*t*t),0])
        parts.append(tube([point(t) for t in (0,.4,.72,1)], [.026,.022,.014,.005]))
        for t,span in [(.30,.18),(.49,.24),(.68,.20),(.83,.13)]:
            for sign in (-1,1):
                parts.append(blade(point(t),point(t+.13)+sign*tangent*span,.057,.023))
    return body,np.concatenate(parts)


def tomato():
    bodies, parts=[],[]
    parts.append(tube([(.03,.015,0),(0,.35,.04),(.06,.72,0),(-.03,.97,.03)],[.055,.057,.038,.015]))
    for x,y,z,rx,h in [(-.47,.13,-.05,.46,.60),(.36,.37,.14,.43,.57),(.04,.025,-.48,.37,.53)]:
        def polar(a,level,r):
            w=r*(.96+.035*cos(5*a+.6))
            return (x+rx*w*cos(a),y+h*level,z+rx*w*sin(a))
        bodies.append(fruit(14,[(.02,.15),(.08,.62),(.26,.90),(.52,1),(.74,.93),(.94,.61),(.98,.33),(.90,.07)],polar))
        # Convert body coordinates to the independently scaled foliage mesh.
        cap=np.array([x*1.3,(y+.925*h)*.85,z*1.3])
        parts.append(tube([[.04,min(.92,cap[1]+.13),.005],cap+[0,.12,0],cap],[.032,.025,.016]))
        for i in range(5):
            a=i*tau/5+x
            parts.append(blade(cap+[0,.027,0],cap+np.array([.24*cos(a),-.025,.24*sin(a)]),.060,.020))
    for a,y in [(2.8,.80),(5.55,.88)]:
        p=np.array([0,y,0])
        parts.append(blade(p,p+[.49*cos(a),.075,.49*sin(a)],.12,.038))
    return np.concatenate(bodies),np.concatenate(parts)


def pumpkin():
    def polar(a,y,r):
        lobes=(1+cos(9*a+.18*sin(3*a)))/2
        radius=r*(.77+.22*lobes**.65)
        return (radius*cos(a), y*(.97+.025*lobes), radius*sin(a))
    body=fruit(45,[(0,.33),(.20,.86),(.48,1),(.71,.94),(.90,.75),(.99,.50),(.95,.29),(.80,.095)],polar)
    stem=tube([(.0,.665,0),(.015,.77,.01),(.055,.858,-.01),(.11,.922,-.065),(.104,.947,-.09)],
              [.125,.11,.078,.061,.064],7)
    vines=[stem,tube([(-.45,.04,.26),(-.73,.11,.19),(-.75,.37,.03),
                      (-.52,.63,-.03),(.0,.69,0)],[.022,.042,.045,.036,.024])]
    for start,end,width in [((-.67,.09,.20),(-.49,.24,.72),.19),
                            ((-.74,.29,.07),(-.94,.41,-.23),.12),
                            ((-.46,.055,.26),(-.11,.16,.71),.18)]:
        vines.append(blade(start,end,width,.045))
    return body,np.concatenate(vines)


def save_obj(path,f,corner_normals=None):
    flat=f.reshape(-1,3)
    vertices, ids=np.unique(flat.round(7),axis=0,return_inverse=True)
    ids=ids.reshape(-1,3)
    n=np.cross(f[:,1]-f[:,0],f[:,2]-f[:,0]); n/=np.linalg.norm(n,axis=1)[:,None]
    lines=[f"o {path.stem}", "s off", "vt 0.5 0.5"]
    lines += ["v %.7f %.7f %.7f"%tuple(p) for p in vertices]
    if corner_normals is None:
        normal_ids=np.broadcast_to(np.arange(len(n))[:,None],ids.shape)
    else:
        n,normal_ids=np.unique(corner_normals.reshape(-1,3).round(7),axis=0,return_inverse=True)
        normal_ids=normal_ids.reshape(ids.shape)
    lines += ["vn %.7f %.7f %.7f"%tuple(p) for p in n]
    lines += ["f "+" ".join(f"{v+1}/1/{j+1}" for v,j in zip(row,nrow)) for row,nrow in zip(ids,normal_ids)]
    path.write_text("\n".join(lines)+"\n",encoding="ascii")


def load_obj(path):
    lines=[s.split() for s in path.read_text(encoding="ascii").splitlines() if s]
    v=np.array([[float(x) for x in s[1:4]] for s in lines if s[0]=="v"])
    return v[np.array([[int(x.split("/")[0])-1 for x in s[1:]] for s in lines if s[0]=="f"])]


def check(f):
    assert 0<len(f)<=800 and np.isfinite(f).all()
    edges=Counter((tuple(tr[i]),tuple(tr[(i+1)%3])) for tr in f for i in range(3))
    assert all(edges[(b,a)]==k for (a,b),k in edges.items())
    assert np.min(np.linalg.norm(np.cross(f[:,1]-f[:,0],f[:,2]-f[:,0]),axis=1))>1e-8
    assert (f[:,0]*np.cross(f[:,1],f[:,2])).sum()>0
    assert f[:,:,1].min()>=-1e-6 and f[:,:,1].max()<=1.000001
    assert np.max(np.hypot(f[:,:,0],f[:,:,2]))<=1.000001


CASES=[("carrot","leaves","ニンジン",carrot,[.30,1,.30],[.4,.4,.4],.96,(221,113,49)),
       ("tomato","stems","トマト",tomato,[.65,.85,.65],[.5,1,.5],0,(211,67,49)),
       ("pumpkin","vines","かぼちゃ",pumpkin,[.65,.75,.65],[.6,.85,.6],0,(72,116,73))]


def render(parts,angle=.40,elev=.32,pixels_per_unit=176):
    w,h=420,322
    im=np.full((h,w,3),(244,246,242),np.uint8)
    yy,xx=np.mgrid[:h,:w]
    im[:]=np.rint(im*(1-.13*np.exp(-((xx-210)/83)**2-((yy-271)/11)**2))[:,:,None])
    depth=np.full((h,w),-np.inf)
    right=np.array([cos(angle),0,-sin(angle)])
    view=np.array([sin(angle)*cos(elev),sin(elev),cos(angle)*cos(elev)])
    up=np.cross(view,right)
    sun=np.array([-.36,.82,.44]); sun/=np.linalg.norm(sun)
    for tris,color,*normals in parts:
        for index,tri in enumerate(tris):
            n=np.cross(tri[1]-tri[0],tri[2]-tri[0]); n/=np.linalg.norm(n)
            if n.dot(view)<=0: continue
            sx=210+tri@right*pixels_per_unit; sy=268-tri@up*pixels_per_unit; sz=tri@view
            x0=max(0,int(np.floor(min(sx)))); x1=min(w,int(np.ceil(max(sx)))+1)
            y0=max(0,int(np.floor(min(sy)))); y1=min(h,int(np.ceil(max(sy)))+1)
            if x1<=x0 or y1<=y0: continue
            y,x=np.mgrid[y0:y1,x0:x1]; x=x+.5; y=y+.5
            den=(sy[1]-sy[2])*(sx[0]-sx[2])+(sx[2]-sx[1])*(sy[0]-sy[2])
            if abs(den)<1e-9: continue
            a=((sy[1]-sy[2])*(x-sx[2])+(sx[2]-sx[1])*(y-sy[2]))/den
            b=((sy[2]-sy[0])*(x-sx[2])+(sx[0]-sx[2])*(y-sy[2]))/den
            c=1-a-b; z=a*sz[0]+b*sz[1]+c*sz[2]
            buf=depth[y0:y1,x0:x1]; hit=(a>=0)&(b>=0)&(c>=0)&(z>buf)
            buf[hit]=z[hit]
            light=max(0,n.dot(sun))
            if normals:
                ns=normals[0][index]
                pn=a[hit,None]*ns[0]+b[hit,None]*ns[1]+c[hit,None]*ns[2]
                light=np.maximum(0,(pn@sun)/np.maximum(1e-8,np.linalg.norm(pn,axis=1)))[:,None]
            im[y0:y1,x0:x1][hit]=np.clip(np.array(color)*(.63+.50*light),0,255)
    return Image.fromarray(im)


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    sheet=Image.new("RGB",(1380,818),"#ffffff")
    draw=ImageDraw.Draw(sheet)
    font=lambda s: ImageFont.truetype("C:/Windows/Fonts/meiryo.ttc",s)
    draw.text((28,12),"水路農業  /  野菜の形を見比べる",font=font(33),fill="#263d37")
    for row,label in enumerate(("現行","試作")):
        draw.text((20,180+row*335),label,font=font(23),fill="#38594d")
    for i,(key,suffix,jp,make,bs,ls,ly,color) in enumerate(CASES):
        stems=("crop_"+key,"crop_"+key+"_"+suffix)
        candidates=make()
        for stem,mesh in zip(stems,candidates):
            path=OUT/(stem+".obj"); save_obj(path,mesh); check(load_obj(path))
        cells=solid_crop((OUT/(stems[0]+".obj")).read_text(encoding="ascii"))
        assert cells
        x=89+i*425
        draw.text((x,65),jp,font=font(22),fill="#263d37")
        for row,folder in enumerate((SOURCE,OUT)):
            body,leaves=[load_obj(folder/(stem+".obj")) for stem in stems]
            picture=render([(body*bs,color),(leaves*ls+[0,ly,0],(90,145,67))])
            sheet.paste(picture,(x,106+row*335))
        print(key,*(len(m) for m in candidates),"triangles; ceremony cells:",len(cells))
    draw.text((89,779),"実と葉は別メッシュ。同じ照明・色・縮尺で比較（本編画面ではありません）。",font=font(18),fill="#53665d")
    sheet.save(OUT/"comparison.png")


if __name__=="__main__":
    main()
