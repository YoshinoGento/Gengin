"""Guard opt-in bounded materials and legacy defaults; not a GPU image test."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
renderer = (ROOT / "project/application/farm/render/FarmRenderer.cpp").read_text(encoding="utf-8")
shader = (ROOT / "project/Resources/shader/Object3d.PS.hlsl").read_text(encoding="utf-8")
title = (ROOT / "project/application/title/TitleFarmRenderer.cpp").read_text(encoding="utf-8")
object_cpp = (ROOT / "project/engine/3d/Object3d.cpp").read_text(encoding="utf-8")
object_h = (ROOT / "project/engine/3d/Object3d.h").read_text(encoding="utf-8")
buffer_layout = (ROOT / "project/engine/3d/ObjectBufferLayout.h").read_text(encoding="utf-8")
for source in (renderer, title):
    assert "BuildFarmMaterialStyle(part)" in source
    assert "SetSurfaceDetail(material.detail)" in source
    assert "material.detail.x > 0.0f" in source
assert "surfaceDetail = {1.0f, 0.0f, 0.0f, 0.0f}" in object_cpp
assert "using Material = graphics::ObjectMaterial;" in object_h
assert "offsetof(ObjectMaterial, surfaceDetail) == 96" in buffer_layout
assert "offsetof(ObjectMaterial, outdoorAmbient) == 112" in buffer_layout
assert "sizeof(ObjectMaterial) == 128" in buffer_layout
assert "spec_dir * directionalShadow * specularGain" in shader
assert "gMaterial.shininess) * specularGain" in shader
assert "EvaluateDirectionalShadow(input.worldPosition, geometricNormal" in shader
assert "ddx(p)" in shader and "ddy(p)" in shader
assert "object.SetEnableLighting(!part.water" in renderer
assert "void FarmRenderer::DrawShadow()" in renderer
assert "gMaterial.specularType == kSpecularTypeNone" in shader
assert "return 0.0f;" in shader.split("gMaterial.specularType == kSpecularTypeNone", 1)[1].split("}", 1)[0]
print("PASS farm material policy: bounded highlights, opt-in grain, legacy defaults; GPU check separate")
