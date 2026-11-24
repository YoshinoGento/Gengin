"""Two-fruit tomato study; dimensions below use the mature plant's field space."""
from math import cos, sin, tau

import numpy as np
from PIL import Image, ImageDraw, ImageFont

from crop_remodel import ROOT, fruit, render, solid_crop, tomato, tube
from study_mesh import MIRROR_X, export_part, normalize

OUT = ROOT/"generated/art_studies/tomato_02"
# FarmMeshLayout::BuildFarmCropMeshParts at growth=1, size=1.
BODY = np.array([.42,.60,.42])
LEAF = np.array([.58,1.05,.58])
ROOT_Y, STEM_Y = .22,.02
PROFILE = [(-.98,.13),(-.90,.45),(-.68,.75),(-.34,.94),(.05,1),
           (.42,.94),(.73,.78),(.92,.55),(.97,.30),(.79,.07)]


def leaflet(points,widths,fold):
    """Closed leaf with pointed ends and an arched midrib along the inner stations."""
    points=np.array(points)
    side=np.cross(points[-1]-points[0],[0,1,0])
    side/=np.linalg.norm(side)
    rings=[]
    for p,w in zip(points[1:-1],widths,strict=True):
        rings.append([p+side*w,p+[0,w*fold,0],p-side*w,p-[0,w*.18,0]])
    faces=[]
    for k in range(4):
        j=(k+1)%4
        faces += [(points[0],rings[0][j],rings[0][k]),(points[-1],rings[-1][k],rings[-1][j])]
        for a,b in zip(rings,rings[1:]):
            faces += [(a[k],a[j],b[k]),(b[k],a[j],b[j])]
    f=np.array(faces)
    return f if (f[:,0]*np.cross(f[:,1],f[:,2])).sum()>0 else f[:,::-1]


def make():
    bodies=[]
    leaves=[tube([(0,.029,-.106),(.015,.34,-.179),(-.028,.60,-.157),
                  (.005,.78,-.085),(.052,.89,-.093),(.106,.985,-.08)],
                 [.023,.021,.017,.015,.011,.005],6)]
    for x,y,z,r,h,angle in [(-.171,.409,.045,.210,.172,.32),(.202,.540,-.034,.159,.131,-.75)]:
        def polar(a,t,w):
            # Lobes are confined mainly to the shoulders, not grooves down the ripe fruit.
            lobe=cos(5*a+angle)
            radius=r*w*(1+.022*lobe*max(0,t)+.008*cos(3*a-angle))
            return (x+radius*cos(a),y+h*(t+.027*lobe*w*max(0,t)),z+radius*sin(a))
        bodies.append(fruit(19,PROFILE,polar))
        socket=np.array([x,y+.80*h,z])
        joint=np.array([.005,.78,-.085])
        leaves.append(tube([joint,joint+[x*.40,-.004,z*.25],
                            [x*.93,socket[1]+.075,z-.022],socket+[.006,.025,-.004],socket],
                           [.012,.012,.010,.009,.010]))
        for i in range(5):
            a=i*tau/5+angle
            axis=np.array([cos(a),0,sin(a)])
            tangent=np.array([-sin(a),0,cos(a)])
            top=np.array([x,y,z])
            # Curve up out of the stem scar, over the shoulder, then taper down the skin.
            leaves.append(leaflet([socket,top+.26*r*axis+[0,h*1.10,0],
                                   top+.49*r*axis+[0,h*1.105,0],
                                   top+.71*r*axis+.05*r*tangent+[0,.98*h,0]],
                                  [.115*r,.080*r],.28))
    for base,a,length,arch in [((-.028,.60,-.157),2.75,.39,.06),
                               ((.005,.78,-.085),.40,.36,.095),
                               ((.052,.89,-.093),2.02,.28,.08)]:
        along=np.array([cos(a),0,sin(a)])
        across=np.array([-sin(a),0,cos(a)])
        def point(t):
            return base+along*(length*t)+[0,arch*sin(np.pi*t)-.028*t,0]
        leaves.append(tube([point(t) for t in (0,.32,.60,.82,1)],[.011,.009,.006,.004,.002],4))
        for t,span in ((.28,.28),(.55,.24)):
            for sign in (-1,1):
                p=point(t+.045*(sign==1))
                d=length*(along*.22+sign*across*span)
                leaves.append(leaflet([p,p+d*.32+[0,.010,0],p+d*.57+[0,.007,0],p+d-[0,.023,0]],
                                      [length*.088,length*.069],.31))
        p=point(.78)
        leaves.append(leaflet([p,point(.91)+[0,.010,0],point(1.02),point(1.22)-[0,.015,0]],
                              [length*.102,length*.075],.32))
    tip=np.array([.106,.985,-.08])
    for offset in [(-.068,.030,.031),(.046,.016,-.052)]:
        d=np.array(offset)
        leaves.append(leaflet([tip-[.010,.018,0],tip+.34*d+[0,.016,0],
                               tip+.70*d+[0,.020,0],tip+d],[.015,.012],.48))
    return (np.concatenate(bodies)-[0,ROOT_Y,0])/BODY,(np.concatenate(leaves)-[0,STEM_Y,0])/LEAF


def parts(body,leaves,normals=None):
    skin=(body*BODY+[0,ROOT_Y,0],(205,58,45))
    if normals is not None:
        skin+= (normalize(normals/BODY),)
    return [skin,(leaves*LEAF+[0,STEM_Y,0],(77,132,62))]


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    b,l=make()
    body,normals=export_part(OUT/"crop_tomato.obj",b,smooth=True)
    stems,_=export_part(OUT/"crop_tomato_stems.obj",l)
    cells=solid_crop((OUT/"crop_tomato.obj").read_text())
    assert cells
    after=parts(body,stems,normals)
    before=parts(*(f[:,::-1]*MIRROR_X for f in tomato()))
    sheet=Image.new("RGB",(1024,412),"white")
    draw=ImageDraw.Draw(sheet)
    font=lambda size:ImageFont.truetype("C:/Windows/Fonts/meiryo.ttc",size)
    draw.text((22,12),"トマトの房",font=font(27),fill="#283d32")
    for title,x,p in [("前の試作",18,before),("今回の試作",356,after)]:
        art=render(p,angle=.17,elev=.39,pixels_per_unit=246)
        sheet.paste(art.crop((46,0,374,300)),(x,92))
        draw.text((x,60),title,font=font(18),fill="#405849")
    x=694
    draw.text((x,60),"上から見た形",font=font(18),fill="#405849")
    art=render(after,angle=.17,elev=.94,pixels_per_unit=245)
    overhead=art.crop((32,76,388,288))
    overhead.thumbnail((310,182),Image.Resampling.LANCZOS)
    sheet.paste(overhead,(x,92))
    draw.text((x,284),"縮小したとき",font=font(16),fill="#405849")
    for j,p in enumerate((before,after)):
        small=render(p,angle=.17,elev=.94,pixels_per_unit=245).crop((92,105,328,285))
        sheet.paste(small.resize((65,50),Image.Resampling.LANCZOS),(x+j*100,320))
        draw.text((x+j*100+10,373),("前案","今回")[j],font=font(13),fill="#405849")
    sheet.save(OUT/"tomato_comparison.png")
    print(f"PASS: fruit {len(b)}, foliage {len(l)} triangles, {len(cells)} voxel cells")


if __name__=="__main__":
    main()
