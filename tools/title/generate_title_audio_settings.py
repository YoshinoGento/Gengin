from PIL import Image
from title_text_raster import ROOT, label, save_asset

image = Image.new("RGBA", (640, 360))
for row, text in enumerate(["音量設定", "環境音", "水音", "戻る", "音設定", "保存できません", "初期値"]):
    label(image, (0, row*40+4), text, 28, (240,247,232,255), outline=False)
for column, text in enumerate("0123456789%"):
    label(image, (column*24+1, 283), text, 24, (240,247,232,255), outline=False)
label(image, (0, 324), "審査音", 28, (240,247,232,255), outline=False)
save_asset(image, ROOT / "project/Resources/title/audio_settings.png")
print("PASS smooth title sound settings labels and percent glyphs")
