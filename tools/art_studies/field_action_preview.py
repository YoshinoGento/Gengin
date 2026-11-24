"""Render the CPU vertex trace from the farm action test, without a DX12 device."""
import struct
import subprocess
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "generated/codex_checks/action_particles"
TRACE = OUT / "field_vertices.bin"
LABELS = ["乾いた土を耕す", "湿った土を耕す", "ニンジンを抜く", "雨上がりのトマト", "かぼちゃを持ち上げる", "通水・曲がり角・給水"]
W, H, FRAMES = 280, 162, 36


def panel(vertices, shot):
    image = Image.new("RGB", (W, H), "#e2e9df")
    draw = ImageDraw.Draw(image)
    scale = 75 if shot == 5 else 108

    def xy(x, y, z):
        return W / 2 + scale * x, 112 - scale * (.88 * y - .47 * z)

    if shot == 5:
        path = [xy(x, 0, z) for x, z in [(-1.43, -.715), (0, -.715), (0, .715)]]
        draw.line(path, fill="#809b8c", width=33)
        draw.line(path, fill="#80babe", width=23)
    else:
        draw.polygon([xy(x, 0, z) for x, z in [(-.68, -.66), (.68, -.66), (.68, .66), (-.68, .66)]],
                     fill="#a98863" if shot != 1 else "#856647")
    pixels = np.array(image)
    tris = vertices.reshape(-1, 3, 7)
    for tri in sorted(tris, key=lambda v: float(v[:, 2].mean() - .5 * v[:, 1].mean())):
        a, b, c = np.array([xy(*v[:3]) for v in tri])
        left, top = np.maximum(0, np.floor(np.minimum(np.minimum(a, b), c))).astype(int)
        right, bottom = np.minimum([W, H], np.ceil(np.maximum(np.maximum(a, b), c)) + 1).astype(int)
        if right <= left or bottom <= top:
            continue
        yy, xx = np.mgrid[top:bottom, left:right] + .5
        ab, ac = b-a, c-a
        denom = ab[0]*ac[1] - ab[1]*ac[0]
        if abs(denom) < 1e-6:
            continue
        u = ((xx-a[0])*ac[1] - (yy-a[1])*ac[0]) / denom
        v = (ab[0]*(yy-a[1]) - ab[1]*(xx-a[0])) / denom
        mask = (u >= 0) & (v >= 0) & (u+v <= 1)
        rgba = tri[0, 3:] + u[mask, None]*(tri[1, 3:]-tri[0, 3:]) + v[mask, None]*(tri[2, 3:]-tri[0, 3:])
        dst = pixels[top:bottom, left:right]
        alpha = np.clip(rgba[:, 3:], 0, 1)
        dst[mask] = np.clip(rgba[:, :3]*255*alpha + dst[mask]*(1-alpha), 0, 255)
    return Image.fromarray(pixels)


def main():
    subprocess.run([OUT / "farm_action_particles_test.exe", "--trace", TRACE], check=True)
    frames = [Image.new("RGB", (W*3, H*2), "#e2e9df") for _ in range(FRAMES)]
    font = ImageFont.truetype("C:/Windows/Fonts/meiryo.ttc", 15)
    with TRACE.open("rb") as stream:
        for shot, label in enumerate(LABELS):
            for frame in frames:
                count, = struct.unpack("<I", stream.read(4))
                tile = panel(np.frombuffer(stream.read(28*count), dtype="<f4").reshape(-1, 7), shot)
                ImageDraw.Draw(tile).text((12, 7), label, font=font, fill="#23382f")
                frame.paste(tile, (shot % 3*W, shot // 3*H))
        assert not stream.read(1)
    frames[0].save(OUT / "field-actions.gif", save_all=True, append_images=frames[1:], duration=55, loop=0)
    frames[5].save(OUT / "field-actions.png")


if __name__ == "__main__":
    main()
