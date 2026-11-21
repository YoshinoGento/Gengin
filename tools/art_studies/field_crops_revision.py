"""Carrot and kabocha companion sculpts for the two-fruit tomato study."""
from dataclasses import dataclass
from math import cos, pi, sin, tau

import numpy as np
from PIL import Image, ImageDraw, ImageFont

from crop_remodel import ROOT, carrot, fruit, pumpkin, render, shell, solid_crop, tube
from study_mesh import MIRROR_X, export_part, normalize

OUT=ROOT/"generated/art_studies/field_crops_02"
UP=np.array([0.,1.,0.])


def folded_outline(root,direction,outline,ridge_height):
    """A creased, closed leaf cut along its silhouette rather than a flat card."""
    side=normalize(np.cross(direction,UP))
    width=np.linalg.norm(direction)
    rim=np.array([root+t*direction+w*width*side+UP*width*.08*sin(pi*t) for t,w in outline])
    mid=root+.45*direction
    faces=[]
    for a,b in zip(rim,np.roll(rim,-1,axis=0)):
        faces.extend([(mid+UP*ridge_height,a,b),(mid-UP*ridge_height*.26,b,a)])
    f=np.array(faces)
    return f if (f[:,0]*np.cross(f[:,1],f[:,2])).sum()>0 else f[:,::-1]


def carrot_sculpt():
    profile=[(.007,.012),(.063,.049),(.12,.084),(.20,.16),(.29,.24),(.39,.35),
             (.46,.417),(.474,.426),(.49,.447),(.59,.54),(.67,.615),(.685,.618),
             (.702,.636),(.78,.69),(.87,.725),(.932,.692),(.971,.567),(.988,.382),(.976,.097)]
    def polar(a,y,r):
        irregular=1+.024*sin(3*a+1.2*y)+.015*cos(2*a-.6)
        # Shallow broken root scars, not concentric rings around the whole carrot.
        if y in (.474,.685):
            irregular-=.034*max(0,cos(a+(1 if y==.474 else -1.4)))**2
        return (.125*(1-y)**2+r*irregular*cos(a),y,r*.92*irregular*sin(a)-.05*(1-y))
    root=fruit(19,profile,polar)
    leaves=[]
    pinna=[(0,0),(.27,.24),(.43,.105),(.67,.23),(.64,.081),(1,0),
           (.59,-.212),(.24,-.183)]
    for a,h,r in [(.1,.72,.73),(1.53,.97,.46),(2.69,.72,.81),(4.09,.88,.61),(5.32,.64,.77)]:
        axis=np.array([cos(a),0,sin(a)])
        across=np.array([-sin(a),0,cos(a)])
        def at(t):
            return axis*(.026+r*t**1.38)+UP*(.014+h*(1.55*t-.65*t*t))
        leaves.append(tube([at(t) for t in (0,.31,.55,.75,1)], [.019,.016,.012,.008,.0038],4))
        for t,span in ((.31,.298),(.55,.259),(.75,.183)):
            for sign in (-1,1):
                p=at(t+.014*(sign==1))
                d=at(t+.17)-at(t)+across*(sign*span)
                leaves.append(folded_outline(p,d,pinna,.012))
        p=at(.90)
        leaves.append(folded_outline(p,at(1.06)-p,pinna,.009))
    return root,np.concatenate(leaves)


def pumpkin_sculpt():
    body_scale=np.array([.46,.65,.46])
    vine_scale=np.array([.58,.80,.58])
    # Fruit rests off-centre, leaving room for broad leaves on the runner side within one tile.
    center=np.array([.040,0,.006])
    def polar(a,y,r):
        theta=a+.045*sin(3*a)+.084*(y-.4)
        groove=((1+cos(8*theta))/2)**2
        radius=r*(1-(.080+.085*y)*groove)*(.981+.019*cos(3*a+.3))
        return (center[0]/body_scale[0]+radius*cos(a),y*(1+.045*cos(8*theta+pi)*r),
                center[2]/body_scale[2]+.934*radius*sin(a))
    body=fruit(40,[(.003,.30),(.064,.63),(.22,.85),(.42,.91),(.59,.87),
                   (.745,.67),(.798,.46),(.78,.27),(.688,.095)],polar)
    socket=center+[0,.015+.65*.688,0]
    def stem_ring(dy,dx,dz,r):
        return [socket+[dx+r*(1+.19*cos(5*a))*cos(a+.11*dy),dy,dz+r*(1+.19*cos(5*a))*sin(a+.11*dy)]
                for a in np.arange(15)*tau/15]
    stem=shell([stem_ring(.031,0,0,.083),stem_ring(.019,-.002,.001,.057),
                stem_ring(.052,-.006,-.003,.034),stem_ring(.096,-.027,-.012,.026),
                stem_ring(.124,-.048,-.023,.028),stem_ring(.132,-.056,-.029,.023)])
    runner=[(-.50,.036,.003),(-.425,.064,-.057),(-.390,.125,-.11),
            (-.367,.222,-.109),(-.323,.350,-.104),(-.235,.462,-.106),
            (-.084,.545,-.071),socket+[-.042,.112,-.019]]
    foliage=[stem,tube(runner,[.011,.017,.018,.013,.012,.012,.012,.014],6)]
    # Palmate leaves: three pronounced points with shallow side notches and a folded centre.
    outline=[(0,0),(-.13,.23),(.05,.43),(.23,.27),(.41,.53),(.49,.29),
             (.59,.17),(1,0),(.59,-.21),(.44,-.34),(.31,-.51),(.18,-.25),(-.09,-.30)]
    for base,petiole,tip in [(runner[2],(-.300,.196,.070),(-.488,.123,.248)),
                             (runner[1],(-.437,.112,-.131),(-.362,.088,-.324)),
                             (runner[4],(-.295,.380,-.168),(-.143,.338,-.313))]:
        p=np.array(petiole)
        foliage.append(tube([base,(np.array(base)+p)/2+UP*.015,p],[.009,.007,.004],5))
        foliage.append(folded_outline(p,np.array(tip)-p,outline,.030))
    # The tendril grows from the same low node, away from the fruit's footprint.
    curl=[np.array(runner[1]),np.array([-.478,.103,-.062])]
    for i in range(12):
        a=i*.68
        curl.append(np.array([-.503-.012*sin(a),.126+.005*i-.020*cos(a),-.068+.020*sin(a)]))
    foliage.append(tube(curl,[.004]*(len(curl)-1)+[.002],4))
    return body,(np.concatenate(foliage)-[0,.02,0])/vine_scale


@dataclass(frozen=True)
class Crop:
    key:str
    leaf:str
    label:str
    body_scale:tuple
    leaf_scale:tuple
    body_y:float
    leaf_y:float
    color:tuple
    green:tuple


CROPS=[
    (Crop("carrot","leaves","ニンジン",(.18,.55,.18),(.32,.37,.32),-.55*.84,.55*(.96-.84),
          (223,111,44),(76,130,61)),carrot,carrot_sculpt),
    (Crop("pumpkin","vines","かぼちゃ",(.46,.65,.46),(.58,.80,.58),.015,.020,
          (61,107,71),(88,133,68)),pumpkin,pumpkin_sculpt)]


def arrange(c,b,l,normals=None,planted=False):
    offset=.65 if planted else -c.body_y
    body=(b*c.body_scale+[0,c.body_y+offset,0],c.color)
    if normals is not None:
        body+= (normalize(normals/c.body_scale),)
    parts=[body,(l*c.leaf_scale+[0,c.leaf_y+offset,0],c.green)]
    if planted:
        plane=np.array([[(-.56,offset,-.56),(-.56,offset,.56),(.56,offset,.56)],
                        [(-.56,offset,-.56),(.56,offset,.56),(.56,offset,-.56)]])
        parts.append((plane,(135,107,81)))
    return parts


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    sheet=Image.new("RGB",(1008,634),"white")
    draw=ImageDraw.Draw(sheet)
    font=lambda n:ImageFont.truetype("C:/Windows/Fonts/meiryo.ttc",n)
    for col,title in enumerate(("前の試作","今回の試作","畑に植わった状態")):
        draw.text((20+col*333,12),title,font=font(20),fill="#344b3f")
    for row,(c,old,new) in enumerate(CROPS):
        b,l=new()
        body_path=OUT/f"crop_{c.key}.obj"
        b,normals=export_part(body_path,b,smooth=True)
        l,_=export_part(OUT/f"crop_{c.key}_{c.leaf}.obj",l)
        cells=solid_crop(body_path.read_text())
        assert cells
        original=arrange(c,*(f[:,::-1]*MIRROR_X for f in old()))
        ground=arrange(c,b,l,normals,True)
        skin=ground[0][0][:,:,1]
        soil_y=ground[-1][0][0,0,1]
        if c.key=="carrot":
            assert skin.min()<soil_y<skin.max()
        else:
            assert skin.min()>=soil_y
        for col,p in enumerate((original,arrange(c,b,l,normals),ground)):
            field=col==2
            art=render(p,angle=.22,elev=.91 if field else .44,pixels_per_unit=195 if field else 260)
            tile=art.crop((32,10,388,310)).resize((320,270),Image.Resampling.LANCZOS)
            x,y=12+col*333,58+row*289
            sheet.paste(tile,(x,y))
            if not col: draw.text((x+12,y+10),c.label,font=font(16),fill="#425849")
        print(f"PASS {c.key}: {len(b)} + {len(l)} triangles, {len(cells)} voxel cells")
    sheet.save(OUT/"comparison.png")


if __name__=="__main__":
    main()
