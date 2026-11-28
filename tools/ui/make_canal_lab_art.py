"""Bake the trial field from the same terrain file the native scene reads."""
from pathlib import Path
import math
import random
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2] / "project/Resources"
OUT = ROOT / "farm/canal_lab"
rows = (OUT / "field.txt").read_text(encoding="ascii").splitlines()
W, H, X, Y = 84, 60, 178, 148
r = random.Random(4217)
INK, PAPER = "#0c2422", "#fff3cc"


def label(d, xy, msg, size, color=PAPER, serif=False):
    font = ImageFont.truetype(str(ROOT / "fonts" / ("NotoSerifJP-VF.ttf" if serif else "NotoSansJP-VF.ttf")), size)
    font.set_variation_by_axes([600 if size >= 20 else 500])
    d.text(xy, msg, fill=color, font=font, anchor="lm", stroke_width=0)


def stone(d, x, y, s):
    d.polygon([(x-s,y),(x-s+3,y-s//2),(x,y-s),(x+s-3,y-s+2),(x+s,y),(x+3,y+4)],fill="#4f6253")
    d.polygon([(x-s,y-3),(x-s+3,y-s//2-2),(x,y-s-2),(x+s-4,y-s),(x+s-2,y-4),(x,y-1)],fill="#aeb49b")
    d.polygon([(x-s+3,y-s//2-2),(x,y-s-2),(x+4,y-s+1),(x-4,y-s//2)],fill="#d5d7bb")


bg = Image.new("RGB",(1280,720),"#418a8b")
d = ImageDraw.Draw(bg)
for y in range(76,654,5):
    c = ("#478f8e","#44908e","#448b8a","#408889")[(y//39)%4]
    d.rectangle((0,y,1280,y+4), fill=c)
for _ in range(1500):
    x,y=r.randrange(1280),r.randrange(85,650)
    d.line((x,y,x+ r.randrange(3,25),y),fill=r.choice(["#5ba09b","#589997","#6ba9a0","#387f81"]),width=r.choice([1,2,2]))
for _ in range(75):
    x,y=r.randrange(1280),r.randrange(110,630)
    d.arc((x,y,x+23,y+5),10,170,fill="#87bcb0")
d.rectangle((X-6,Y+2,X+11*W+5,Y+7*H+12),fill="#316e70")
d.rectangle((X-3,Y-3,X+11*W+2,Y+7*H+5),fill="#ae9870")
for j,row in enumerate(rows):
    for i,kind in enumerate(row):
        x,y=X+i*W,Y+j*H
        grass=r.choice(["#92a568","#91a369","#97a96c","#94a66b"])
        d.rectangle((x,y,x+W-1,y+H-1),fill=grass)
        for _ in range(48):
            a,b=x+r.randint(3,W-8),y+r.randint(4,H-5)
            d.rectangle((a,b,a+r.choice([1,2,4]),b+1),fill=r.choice(["#879b60","#a9b87c","#92a569"]))
        d.line((x+3,y+1,x+17,y+1),fill="#b0be82")
        d.line((x+1,y+2,x+1,y+9),fill="#b0be82")
        if kind=="#":
            for ox,oy,s in [(19,23,13),(50,21,20),(37,42,17),(64,44,9)]:
                stone(d,x+ox,y+oy+6,s)
        elif kind=="~" or kind=="O":
            inset=5 if kind=="O" else 2
            rect=(x+inset,y+inset,x+W-inset,y+H-inset)
            d.rounded_rectangle(rect,radius=14,fill="#718252")
            d.rounded_rectangle((rect[0]+2,rect[1]+3,rect[2]-2,rect[3]-2),radius=12,fill="#a68a5e" if kind=="~" else "#897658")
            for _ in range(18):
                a,b=x+r.randint(10,W-16),y+r.randint(9,H-10)
                d.line((a,b,a+5,b+1),fill="#c0a777")
            if kind=="O":
                d.rectangle((x+W-19,y+6,x+W-15,y+30),fill="#5b513a")
                d.rectangle((x+W-21,y+5,x+W-12,y+9),fill="#f5df9e")
                label(d,(x+12,y+36),"溜め池",14)
        elif r.random()<.34:
            a,b=x+r.randint(15,W-15),y+r.randint(13,H-12)
            for off in (-3,0,3): d.line((a,b,a+off,b-r.randint(3,6)),fill="#6c874e",width=2)
# The shelving shore and reed clusters remain outside the playable bank.
for cx,cy in [(57,208),(91,518),(1169,155),(1194,545),(360,609),(871,114)]:
    for _ in range(18):
        x,y=cx+r.randrange(-24,24),cy+r.randrange(-7,7)
        d.line((x-5,y+3,x+6,y+3),fill="#72aba1",width=2)
        h=r.randint(7,21); d.line((x,y,x-3,y-h),fill="#657c4b",width=2)
        if h>15: d.line((x-3,y-h,x-4,y-h-4),fill="#9e895a",width=3)
d.rectangle((0,0,1280,76),fill=INK);d.rectangle((0,73,1280,77),fill="#c2a56c")
d.rectangle((40,21,44,54),fill="#e7bf73")
label(d,(60,37),"水路の試験田",34,serif=True)
label(d,(329,39),"中央の溜め池へ、水を引く",22,"#dbe8cf")
label(d,(1054,39),"鍬のけいこ場",20,"#b7d2b8",True)
bg.save(OUT/"field.png",optimize=True)

sheet=Image.new("RGBA",(16*W,4*H))
def tile(mask,water):
    im=Image.new("RGBA",(W,H)); p=ImageDraw.Draw(im)
    def channel(fill,rw,rh,dy):
        cx,cy=W//2,H//2+dy
        p.rounded_rectangle((cx-rw,cy-rh,cx+rw,cy+rh),radius=8,fill=fill)
        if mask&1: p.rectangle((cx-rw,0,cx+rw,cy),fill)
        if mask&2: p.rectangle((cx,cy-rh,W-1,cy+rh),fill)
        if mask&4: p.rectangle((cx-rw,cy,cx+rw,H-1),fill)
        if mask&8: p.rectangle((0,cy-rh,cx,cy+rh),fill)
    if water:
        channel("#124960",17,12,1);channel("#148da9",16,10,1)
    else:
        channel("#7e7950",26,18,0);channel("#c29d68",24,17,-1)
        channel("#482e19",20,15,-1);channel("#794d29",19,12,2)
    return im
for mask in range(16):
    for q in range(2): sheet.paste(tile(mask,q),(W*mask,H*q))

def decal(col,row,paint):
    im=Image.new("RGBA",(W,H));p=ImageDraw.Draw(im);paint(p);sheet.paste(im,(W*col,H*row))

def mound(p,side):
    cx,cy=[(42,6),(78,30),(42,53),(6,30)][side]
    for j in range(-2,3):
        x=cx+(j*8 if side%2==0 else 0); y=cy+(j*6 if side%2 else 0)
        p.ellipse((x-8,y-4,x+8,y+4),fill="#635139")
        p.polygon([(x-8,y),(x-5,y-5),(x+1,y-7),(x+6,y-4),(x+8,y),(x,y+2)],fill="#a67a4d")
        p.line((x-4,y-5,x+1,y-7,x+4,y-5),fill="#ddad75",width=2)
for side in range(4): decal(side,2,lambda p,s=side:mound(p,s))
def pile(p):
    for x,y,s in [(66,45,5),(59,48,7),(71,49,4)]:
        p.polygon([(x-s,y),(x-2,y-s),(x+3,y-s+1),(x+s,y),(x,y+2)],fill="#a47449")
        p.line((x-3,y-3,x,y-s,x+3,y-s+1),fill="#e6b983",width=2)
decal(4,2,pile)
def focus(p):
    for x,y,dx,dy in [(3,3,1,1),(80,3,-1,1),(3,56,1,-1),(80,56,-1,-1)]:
        p.line((x+dx*12,y,x,y,x,y+dy*9),fill="#ffdfa1",width=3)
decal(5,2,focus)
def hoe(p):
    p.line((16,49,54,13),fill="#604a35",width=7)
    p.line((16,46,54,11),fill="#e1b783",width=4)
    p.polygon([(48,12),(53,8),(69,16),(66,26),(57,25),(59,18)],fill="#435c5a")
    p.line((54,9,69,16,66,25),fill="#c8d8cb",width=3)
decal(6,2,hoe)
for k in range(4):
    def splash(p,k=k):
        for j in range(7):
            a=j*.9; x=42+math.cos(a)*(6+k*5); y=32+math.sin(a)*(4+k*3)-7*math.sin(k)
            p.rectangle((x,y,x+4-k//2,y+3-k//2),fill="#deb585" if j%2 else "#8b6948")
    decal(7+k,2,splash)
def pool(p):
    p.rounded_rectangle((4,4,W-5,H-5),radius=14,fill="#14546b")
    p.rounded_rectangle((7,7,W-8,H-8),radius=12,fill="#1b96ab")
    p.line((15,19,37,19), fill="#a6d6c8",width=2)
decal(11,2,pool)
decal(12,2,lambda p:p.arc((31,25,53,32),5,167,fill="#d6eee0",width=2))
sheet.save(OUT/"tools.png",optimize=True)

hud=Image.new("RGBA",(1280,240))
for q in range(2):
    y=q*80; p=ImageDraw.Draw(hud)
    p.rectangle((0,y,1279,y+79),fill=INK)
    for i,(x,word) in enumerate([(35,"溝を引く"),(220,"土を寄せる")]):
        p.rounded_rectangle((x,y+11,x+168,y+65),radius=5,fill="#e9c98c" if i==q else "#34544b")
        label(p,(x+17,y+37),word,23,INK if i==q else PAPER)
    label(p,(422,y+30),"水際から畑へドラッグ" if q==0 else "掘った溝から、土を置く側へひと払い",18)
    label(p,(422,y+55),"矢印で移動　Enter ＋ 矢印で掘る　／　Space ＋ 矢印で土寄せ",14,"#b6ccbb")
    p.rounded_rectangle((1050,y+13,1237,y+64),radius=5,outline="#91a68c",width=2)
    label(p,(1067,y+39),"やり直す   R",20)
for i,msg in enumerate(["低い岸から水が漏れている","溜め池に水が届いた","溜め池が満ちた"]):
    x=i*420;y=160
    p.rounded_rectangle((x+1,y+1,x+376,y+45),radius=5,fill="#284943",outline="#d5bf8b",width=2)
    label(p,(x+18,y+23),msg,20)
hud.save(OUT/"hud.png",optimize=True)
