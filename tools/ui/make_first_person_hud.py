"""Bake the six tool labels, field feedback, and four hand-held tools at native HUD scale."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parents[2] / "project/Resources"
out = root / "farm/first_person"
out.mkdir(exist_ok=True)
atlas = Image.new("RGBA", (1024, 832))
fonts = {}
def write(draw, xy, msg, size, color="#fff4d6"):
    if size not in fonts:
        fonts[size] = ImageFont.truetype(str(root / "fonts/NotoSansJP-VF.ttf"), size)
        fonts[size].set_variation_by_axes([650])
    draw.text(xy, msg, fill=color, font=fonts[size], anchor="lm")

for tool in range(4):
    im = Image.new("RGBA", (256,256))
    d = ImageDraw.Draw(im)
    def poly(coords, color): d.polygon(coords, fill=color)
    if tool == 0:
        poly([(59,48),(70,42),(206,227),(193,237)], "#79512f")
        poly([(59,48),(63,44),(200,231),(194,236)], "#d5ab73")
        poly([(39,55),(66,37),(109,53),(99,68),(68,54),(50,66)], "#7b9494")
        poly([(99,63),(110,51),(122,82),(106,103),(93,91)], "#334b50")
        poly([(106,96),(121,77),(122,84),(106,103),(99,98)], "#bccac2")
    elif tool == 1:
        poly([(147,106),(165,78),(189,79),(202,96),(199,128),(185,130),(185,103),(174,96),(163,119)], "#476565")
        poly([(115,123),(92,159),(52,108),(35,99),(44,86),(67,91)], "#748d87")
        poly([(32,101),(30,88),(51,71),(61,76),(62,83),(42,102)], "#476764")
        for a,b in [(38,88),(44,83),(51,79)]: d.ellipse((a,b,a+2,b+2), fill="#bccbc2")
        poly([(91,115),(144,97),(183,120),(179,176),(133,196),(96,169)], "#477c78")
        poly([(91,115),(144,97),(183,120),(134,139)], "#a4bdb1")
        poly([(110,116),(145,105),(163,117),(134,128)], "#3a5f5b")
        poly([(135,140),(183,120),(179,176),(135,195)], "#365f61")
    elif tool == 2:
        poly([(68,89),(146,66),(187,145),(108,174)], "#b39c65")
        poly([(68,85),(146,62),(182,142),(104,169)], "#efe3b1")
        poly([(68,85),(146,62),(150,72),(73,96)], "#668452")
        poly([(121,107),(136,102),(139,125),(125,145)], "#b27637")
        poly([(130,104),(117,96),(118,88),(129,94),(135,79),(139,79),(136,102)], "#557343")
    else:
        d.arc((62,67,169,186),176,364,fill="#6d4828",width=10)
        d.arc((64,69,168,187),178,360,fill="#b99259",width=5)
        poly([(39,142),(112,124),(189,143),(177,192),(109,209),(57,186)], "#9e723f")
        poly([(39,142),(110,122),(189,142),(113,168)], "#dbb47b")
        poly([(56,142),(111,130),(172,143),(112,158)], "#64472d")
        for j in range(3):
            d.line([(55+j*3,155+j*10),(111,176+j*10),(183-j*2,153+j*10)],fill="#d3aa70",width=3)
    # The same cuff places all four tools in the same right hand.
    poly([(143,156),(159,145),(175,147),(191,166),(189,187),(211,205),(194,221),(162,197),(148,193),(134,174)], "#a58158")
    poly([(139,153),(153,144),(174,147),(185,161),(180,170),(158,171),(150,184),(138,178),(133,164)], "#ead2a0")
    for j in range(3):
        d.line((144+j*7,151-j*2,153+j*7,162-j*2), fill="#bd9f72",width=2)
    poly([(173,197),(192,180),(256,222),(256,256),(222,256)], "#3c6254")
    poly([(172,195),(194,178),(204,184),(181,207)], "#709481")
    poly([(190,207),(201,195),(256,232),(256,255)], "#517963")
    atlas.paste(im,(256*tool,0))

d=ImageDraw.Draw(atlas)
for i,label in enumerate(["1   鍬","3   じょうろ","2   ニンジンの種","2   トマトの種","2   かぼちゃの種","4   収穫かご"]):
    y=256+i*48
    d.rounded_rectangle((0,y,238,y+45),radius=4,fill="#203e35")
    d.rectangle((0,y+4,4,y+41),fill="#d4b268")
    write(d,(17,y+22),label,21)
messages=["畑に近づいて土か作物を狙う","草地　・　鍬で耕せる","耕した土　・　種をまける","生育中","収穫どき",
          "土を耕した","種をまいた","水をやった","収穫してかごに入れた","まだ育っている","種の在庫がない","この道具はここでは使えない","水は十分",
          "クリックで収穫　・　長押しで葉をつかむ","離して抜く"]
for i,msg in enumerate(messages):
    y=256+i*32
    d.rounded_rectangle((260,y,260+int(d.textlength(msg,font=fonts[21]))+19,y+30),radius=3,fill=(22,45,35,222))
    write(d,(269,y+15),msg,17)
y=740
d.rounded_rectangle((0,y,684,y+32),radius=4,fill=(19,41,35,230))
write(d,(10,y+16),"WASD 移動　左クリック 作業　ホイール 道具　右クリック 種　F6 見下ろし",16)
for i,c in enumerate(["#efebdc","#f8dda0","#aaf6b5"]):
    x=730+i*40+16
    for w,ink in [(5,"#27382f"),(2,c)]:
        for dx,dy in [(1,0),(-1,0),(0,1),(0,-1)]:
            d.line((x+dx*5,y+16+dy*5,x+dx*10,y+16+dy*10), fill=ink,width=w)
    d.ellipse((x-1,y+15,x+1,y+17),fill=c)
atlas.save(out/"hud.png",optimize=True)
