"""Offline contact sheet of one outdoor gust, using the actual exported meshes."""
from math import sin, cos, tau

import numpy as np
from PIL import Image, ImageDraw, ImageFont

from crop_remodel import OUT, ROOT, SOURCE, load_obj
import generate_farm_arrival as arrival


def obj(path):
    # Same handedness conversion as ModelManager.
    return load_obj(path)[:, ::-1] * [-1, 1, 1]


def grass_panel():
    asset = ROOT / "project/Resources/title/grass_patch.obj"
    faces = obj(asset)
    uvs = np.array([list(map(float,line.split()[1:])) for line in
                    asset.read_text().splitlines() if line.startswith("vt ")])[::3]
    mask = np.abs(faces.mean(axis=1)[:,0]) < .76
    mask &= np.abs(faces.mean(axis=1)[:,2]) < .46
    shades = np.array([(79,118,63),(90,133,62),(117,152,67),(143,161,78)])
    return faces[mask],shades[(uvs[mask,0]*4).astype(int)],1-uvs[mask,1]


def flat_colors(faces, color):
    return np.tile(color,(len(faces),1))


def rotate(faces, rx, rz):
    x,y,z = faces.transpose(2,0,1)
    by,bz = cos(rx)*y-sin(rx)*z,sin(rx)*y+cos(rx)*z
    return np.stack((cos(rz)*x-sin(rz)*by,sin(rz)*x+cos(rz)*by,bz),axis=2)


def draw_model(canvas, origin, scale, faces, colors):
    eye = np.array([.16,.22,.96]); eye /= np.linalg.norm(eye)
    right = np.cross([0,1,0],eye); right /= np.linalg.norm(right)
    up = np.cross(eye,right)
    normal = np.cross(faces[:,1]-faces[:,0],faces[:,2]-faces[:,0])
    normal /= np.maximum(np.linalg.norm(normal,axis=1,keepdims=True),1.e-10)
    normal[normal@eye<0] *= -1
    tint = np.clip(colors*(.71+.36*np.maximum(0,normal@[-.5,.77,.38])[:,None]),0,255).astype(int)
    xy = np.stack((origin[0]+scale*(faces@right), origin[1]-scale*(faces@up)),axis=2)
    points = np.rint(xy).astype(int).reshape(-1,6).tolist()
    draw = ImageDraw.Draw(canvas)
    for i in np.argsort(faces.mean(axis=1)@eye):
        draw.polygon(points[i],fill=tuple(tint[i]))


def run():
    faces,greens,tuft = grass_panel()
    crop = [obj(SOURCE/(name+".obj"))*size for name,size in
            [("crop_tomato",(.42,.60,.42)),("crop_tomato_stems",(.58,1.05,.58))]]
    tree = arrival.Mesh()
    x,z = arrival.TREE_POSITIONS[0]
    tree.tree(x,z,4.3,1.45,20261041,"trunk")
    pivot = np.array([x,arrival.landscape.ground_height(x,z)+2,z])
    trunk = np.array([t[0] for t in tree.triangles if t[3]!=arrival.landscape.FOLIAGE])-pivot
    crown = obj(arrival.OUT/"crown_0.obj")
    axis = np.array([cos(.45), sin(.45)])
    bend = .32*axis
    def gust(phase,location):
        return .08+.92*(.5+.5*np.sin(phase-.85*(location@axis)))**2
    def tilt(mesh, phase, location, response):
        amount = response*gust(phase,np.array(location))
        return rotate(mesh,bend[1]*amount,-bend[0]*amount)
    base=Image.new("RGB",(990,332),"#f3f5ed")
    draw=ImageDraw.Draw(base)
    font=lambda size: ImageFont.truetype("C:/Windows/Fonts/meiryo.ttc",size)
    draw.text((22,12),"農場を通る風",font=font(25),fill="#263c30")
    draw.text((22,52),"草むら",font=font(18),fill="#405549")
    draw.text((360,52),"畑のトマト",font=font(18),fill="#405549")
    draw.text((692,52),"木の枝先",font=font(18),fill="#405549")
    draw.text((22,300),"現行ゲーム用メッシュで再現した動きの比較。各欄は別縮尺。実機映像ではありません。",font=font(14),fill="#506257")
    frames=[]
    for i in range(48):
        phase=tau*i/48
        image=base.copy()
        grass=faces.copy()
        weight=grass[:,:,1]**2/.35*gust(phase+tuft[:,None]*.08,grass[:,:,[0,2]])
        grass[:,:,0]+=weight*bend[0]; grass[:,:,2]+=weight*bend[1]
        draw_model(image,(166,233),177,grass,greens)
        body=tilt(crop[0],phase,(0,8),.14)+[0,.22,0]
        stem=tilt(crop[1],phase,(0,8),.14)+[0,.02,0]
        draw_model(image,(494,270),164,np.concatenate((body,stem)),
                   np.concatenate((flat_colors(body,(192,62,42)),flat_colors(stem,(80,133,65)))))
        moving=tilt(crown,phase,(x,z),.072)
        draw_model(image,(822,199),37,np.concatenate((trunk,moving)),
                   np.concatenate((flat_colors(trunk,(108,87,63)),flat_colors(crown,(88,133,67)))))
        frames.append(image)
    OUT.mkdir(parents=True,exist_ok=True)
    frames[0].save(OUT/"farm_wind.gif",save_all=True,append_images=frames[1:],duration=100,loop=0)


if __name__ == "__main__":
    run()
