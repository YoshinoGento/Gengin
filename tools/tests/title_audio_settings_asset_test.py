from pathlib import Path
import xml.etree.ElementTree as ET
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
with Image.open(ROOT / "project/Resources/title/audio_settings.png") as asset:
    image = asset.convert("RGBA")
assert image.size == (640,360)
assert any(0 < a < 255 for a in image.getchannel("A").tobytes())
low = image.resize((320,180),Image.Resampling.NEAREST)
assert low.resize(image.size,Image.Resampling.NEAREST).tobytes() != image.tobytes()
widths = [280,176,176,92,140,280,140]
for row, width in enumerate(widths):
    alpha = image.getchannel("A").crop((0,row*40,640,(row+1)*40))
    bounds = alpha.getbbox()
    assert bounds and bounds[2] <= width and bounds[3] < 40, (row,bounds)
for glyph in range(11):
    assert image.getchannel("A").crop((glyph*24,280,(glyph+1)*24,312)).getbbox()
judging = image.getchannel("A").crop((0,320,640,360)).getbbox()
assert judging and judging[2] <= 176 and judging[3] < 40
ns = {"m":"http://schemas.microsoft.com/developer/msbuild/2003"}
required = [
    "application\\title\\TitleAudioSettingsSystem.cpp",
    "application\\title\\TitleAudioSettingsSystem.h",
    "application\\title\\TitleAudioSettingsView.cpp",
    "application\\title\\TitleAudioSettingsView.h",
    "application\\title\\TitleAudioSettingsLayout.h",
    "Resources\\title\\audio_settings.png",
]
for name in ("CG2_00_01.vcxproj","CG2_00_01.vcxproj.filters"):
    doc = ET.parse(ROOT / "project" / name)
    includes = [e.attrib["Include"] for e in doc.findall(".//*[@Include]",ns)]
    for path in required:
        assert includes.count(path) == 1, (name,path)
print("PASS title sound settings: antialiasing, label/glyph bounds, project/filter registrations")
