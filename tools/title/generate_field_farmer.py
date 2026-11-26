"""Rigid, pivoted farmhand meshes with one palette shared by all moving parts."""
from pathlib import Path
import math
from PIL import Image, ImageColor

OUT = Path(__file__).resolve().parents[2] / "project/Resources/farm/farmer"
OUT.mkdir(exist_ok=True)
COLORS = ["#deb85f","#efcf80","#bf954a","#983e36","#cf6953","#f0b788","#6f4434",
          "#493b36","#f0deb5","#317277","#24595c","#549096","#ad7845","#5a483c",
          "#869b9d","#c3d0c5"]
CELL, PAD = 64, 5
palette = Image.new("RGB",(CELL*4,CELL*4))
for i,c in enumerate(COLORS):
    base=ImageColor.getrgb(c)
    for y in range(CELL):
        for x in range(CELL):
            # Extruded gutters keep bilinear sampling inside the same material.
            u=min(max(x,PAD),CELL-PAD-1); v=min(max(y,PAD),CELL-PAD-1)
            grain=(((u*47+v*13+u*v*7)%23)/22-.5)*.05
            if i<3:  # Interlaced straw, not painted stripes.
                grain+=.13*math.cos(2*math.pi*(v+((u//5)%2)*2)/5)+.045*math.sin(u*1.6)
            elif i in (9,10,11):
                grain+=.065*math.sin((u+2*v)*1.25)+.022*math.sin(v*1.9)
            elif i==8:
                grain-=.13*(u%17<2)+.085*(v%17<2)
            elif i==12:
                grain+=.10*math.sin(u*.62+math.sin(v*.17))
            palette.putpixel(((i%4)*CELL+x,(i//4)*CELL+y),tuple(round(min(255,ch*(1+grain))) for ch in base))
palette.save(OUT/"palette.png")

class Mesh:
    def __init__(self):
        self.rows=["# Farmhand. Local +Z faces forward; pivots are in metres."]
        self.n=self.normals=0

    def face(self,pts,color):
        # Convert authored left-handed geometry to OBJ and reverse winding.
        self.rows += [f"v {-x:.5f} {y:.5f} {z:.5f}" for x,y,z in pts]
        a,b,c=pts[:3]
        u=[q-p for p,q in zip(a,b)]; v=[q-p for p,q in zip(a,c)]
        nx,ny,nz=u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]
        length=math.sqrt(nx*nx+ny*ny+nz*nz)
        axes=(0,2) if abs(ny)>=max(abs(nx),abs(nz)) else ((2,1) if abs(nx)>abs(nz) else (0,1))
        span=[(min(pt[k] for pt in pts),max(pt[k] for pt in pts)) for k in axes]
        for pt in pts:
            u,v=[(pt[k]-lo)/max(hi-lo,1e-8) for k,(lo,hi) in zip(axes,span)]
            s=((color%4)*CELL+PAD+.5+u*(CELL-2*PAD-1))/(4*CELL)
            t=((color//4)*CELL+PAD+.5+(1-v)*(CELL-2*PAD-1))/(4*CELL)
            self.rows.append(f"vt {s:.6f} {1-t:.6f}")
        self.normals+=1
        self.rows += [f"vn {-nx/length:.6f} {ny/length:.6f} {nz/length:.6f}"]
        self.rows += ["f "+" ".join(f"{self.n+i+1}/{self.n+i+1}/{self.normals}" for i in reversed(range(len(pts))))]
        self.n+=len(pts)

    def prism(self,x,y,z,rx,h,rz,c,n=4):
        if n==4:
            ring=[(-1,-1),(-1,1),(1,1),(1,-1)]
        else:
            ring=[(math.cos(-2*math.pi*i/n), math.sin(-2*math.pi*i/n)) for i in range(n)]
        low=[(x+a*rx,y,z+b*rz) for a,b in ring]
        hi=[(a,b+h,d) for a,b,d in low]
        self.face(hi,c); self.face(low[::-1],c)
        for i in range(n):
            j=(i+1)%n
            self.face([low[i],low[j],hi[j],hi[i]],c)

    def profile(self,rings,colors,n=10):
        # Each ring is height, X/Z radii, Z offset, and brim curl at the sides.
        angles=[-2*math.pi*(i+.5)/n for i in range(n)]
        loops=[[(rx*math.cos(a),y+curl*math.cos(a)**2,z+rz*math.sin(a)) for a in angles]
               for y,rx,rz,z,curl in rings]
        self.face(loops[0][::-1],colors[0])
        for lower,upper,col in zip(loops,loops[1:],colors):
            for i in range(n):
                j=(i+1)%n
                self.face([lower[i],lower[j],upper[j],upper[i]],col)
        self.face(loops[-1],colors[-1])

    def save(self,name):
        (OUT/(name+".obj")).write_text("\n".join(self.rows)+"\n",encoding="ascii")

m=Mesh(); p=m.prism
m.profile([(.435,.151,.118,0,0),(.55,.173,.13,0,0),(.70,.193,.132,-.004,0),
           (.747,.186,.123,0,0),(.793,.122,.095,0,0)],[9,8,8,8])
m.profile([(.419,.147,.105,0,0),(.473,.173,.128,0,0),(.53,.166,.126,0,0)],[9,9])
p(0,.54,.123,.133,.15,.012,9); p(0,.57,.139,.066,.072,.006,11)
for side in (-1,1):
    x=side*.105
    p(x,.665,.111,.023,.12,.025,9); p(x,.67,.14,.012,.023,.006,0)
    p(x,.54,-.124,.022,.24,.012,9)
p(0,.507,0,.173,.032,.133,13); p(.007,.508,.135,.025,.032,.006,2)
p(.129,.446,-.091,.048,.087,.056,12); p(.129,.507,-.10,.052,.027,.057,13)
p(0,.741,.096,.070,.052,.021,3,8); p(.038,.705,.114,.026,.060,.018,4,6)
p(0,.757,0,.09,.067,.09,3,8)
m.profile([(.793,.085,.088,.015,0),(.818,.123,.118,.013,0),(.883,.151,.137,.002,0),
           (.942,.153,.142,0,0),(1.015,.143,.128,0,0)],[5,5,5,5],12)
p(0,.82,-.088,.135,.18,.06,6); p(0,.986,0,.151,.055,.133,6,8)
for s in (-1,1):
    p(s*.060,.908,.131,.013,.021,.008,7,8)
    p(s*.061,.94,.131,.026,.008,.008,6,6)
    p(s*.147,.879,0,.021,.046,.032,5,8)
    p(s*.141,.936,-.023,.014,.06,.044,6,6)
p(0,.876,.135,.025,.033,.024,5,8)
p(0,.847,.132,.023,.004,.005,6,8)
m.profile([(1.008,.298,.245,.012,.029),(1.022,.301,.247,.012,.031),
           (1.052,.184,.151,0,.003),(1.067,.173,.147,0,0),
           (1.095,.169,.143,-.003,0),(1.159,.144,.12,-.008,.008),
           (1.169,.115,.091,-.01,.004)],[2,0,2,3,0,1],16)
m.save("body")
m=Mesh(); p=m.prism
m.profile([(-.169,.067,.076,.002,0),(-.132,.073,.079,0,0),(-.114,.062,.069,0,0),
           (-.037,.075,.077,0,0),(.023,.055,.062,0,0)],[8,8,8,8],8)
m.profile([(-.257,.040,.044,.007,0),(-.219,.048,.051,.006,0),
           (-.158,.053,.057,.004,0)],[5,5],8)
m.profile([(-.311,.039,.045,.017,0),(-.284,.055,.061,.015,0),(-.238,.055,.055,.007,0),
           (-.224,.061,.064,.005,0)],[13,13,13],8)
m.save("arm")
m=Mesh(); p=m.prism
m.profile([(-.28,.058,.059,-.001,0),(-.24,.070,.071,.002,0),(-.166,.073,.082,.006,0),
           (-.111,.083,.09,.004,0),(.012,.081,.094,0,0)],[11,9,10,9],8)
m.profile([(-.416,.089,.124,.028,0),(-.392,.091,.126,.028,0),(-.372,.087,.119,.029,0),
           (-.327,.075,.090,.008,0),(-.27,.083,.083,0,0),(-.253,.085,.085,0,0)],
          [7,13,13,13,13],10)
m.save("leg")
m=Mesh(); p=m.prism
p(0,-.47,0,.017,.62,.018,12,8); p(0,-.475,0,.025,.07,.026,14)
p(0,-.496,-.05,.065,.028,.089,14); p(0,-.536,-.129,.072,.063,.015,15)
m.save("hoe")
