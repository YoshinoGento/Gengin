"""Orthographic material samples of shipping meshes; not a DX12 frame capture."""
from functools import cache
from PIL import Image, ImageDraw, ImageFont

from crop_remodel import ROOT, SOURCE, load_obj, render
from study_mesh import MIRROR_X


@cache
def obj(name):
    return load_obj(SOURCE/(name+".obj"))[:,::-1]*MIRROR_X


def placed(name,scale,position,tint):
    return obj(name)*scale+position,tint


def samples():
    box="unit_box"
    earth=(119,78,49); soil=(152,103,65)
    bed=[placed(box,(.715,.10,.715),(0,.36,0),earth),
         placed(box,(.715,.04,.715),(0,.50,0),soil)]
    for i in (-1,0,1):
        bed.append(placed("soil_ridge",(.075,.012,.519),(i*.325,.552,0),soil))
    channel=[placed(box,(.64,.10,1.0),(0,.25,0),earth),
             placed(box,(.55,.004,1.0),(0,.448,0),(61,141,165))]
    for i in range(5):
        for side in (-1,1):
            palette=((121,131,120),(138,138,120),(125,135,123))
            channel.append(placed("bank_stone",(.088,.0825,.20),(side*.555,.4325,-.8+i*.4),
                                  palette[(i+side)%3]))
    tools=[placed("arrival/tools_wood",(1,1,1),(0,.12,0),(149,112,69)),
           placed("arrival/tools_metal",(1,1,1),(0,.12,0),(153,165,162))]
    return [("耕した土",bed,.56,.50,184),
            ("水路の岸石",[(f+[0,.34,0],color) for f,color in channel],.57,.57,146),
            ("農具台",tools,2.88,.46,170)]


def main():
    sheet=Image.new("RGB",(1188,370),"white")
    draw=ImageDraw.Draw(sheet)
    font=ImageFont.truetype("C:/Windows/Fonts/meiryo.ttc",19)
    for i,(title,parts,angle,elev,scale) in enumerate(samples()):
        x=12+i*392
        draw.text((x+2,9),title,font=font,fill="#374a41")
        img=render(parts,angle=angle,elev=elev,pixels_per_unit=scale)
        sheet.paste(img.resize((380,291),Image.Resampling.LANCZOS),(x,44))
    draw.text((14,341),"本編OBJの形状見本。色と照明は比較用。",font=ImageFont.truetype("C:/Windows/Fonts/meiryo.ttc",15),fill="#516258")
    out=ROOT/"generated/art_studies/field_materials"
    out.mkdir(parents=True,exist_ok=True)
    sheet.save(out/"materials.png")


if __name__=="__main__":
    main()
