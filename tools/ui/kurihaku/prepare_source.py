"""Stage the matching Release source with the previously distributed licensed dependencies."""
import shutil
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

root = Path(__file__).resolve().parents[3]
source = (root / sys.argv[1]).resolve()
if not source.is_relative_to(root / "generated") or source.exists():
    raise SystemExit("Use a fresh workspace generated directory")
old = root / "generated/kurihaku_20260928/combined_submission_01/実行ファイル（水道農業）/Source"
dst = source / "project"
project = root / "project"
dst.mkdir(parents=True)
for folder in ("application", "engine"):
    for f in (project / folder).rglob("*"):
        if f.suffix.lower() in {".cpp", ".h", ".hpp", ".inl", ".inc", ".c"}:
            dest = dst / f.relative_to(project)
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(f, dest)
shutil.copytree(old / "project/externals", dst / "externals")
for name in ("CG2_00_01.sln", "CG2_00_01.vcxproj.filters"):
    shutil.copy2(project / name, dst / name)
for name in ("BUILD_SOURCE.cmd", "RunBuilt.cmd"):
    shutil.copy2(old / name, source / name)

ns = "http://schemas.microsoft.com/developer/msbuild/2003"
ET.register_namespace("", ns)
tree = ET.parse(project / "CG2_00_01.vcxproj")
for group in tree.findall(f"{{{ns}}}ItemDefinitionGroup"):
    if "Release|x64" not in group.get("Condition", ""):
        continue
    for node in group.iter():
        if not node.text:
            continue
        node.text = node.text.replace(r"externals\assimp\include", r"externals\assimp-official\include")
        node.text = node.text.replace(r"externals\assimp\lib\Release", r"externals\assimp-official\lib")
        node.text = node.text.replace("assimp-vc143-mt.lib", "assimp-official.lib;zlibstatic.lib")
for node in tree.iter():
    item = node.get("Include", "")
    if item.startswith("Resources\\") and Path(item).suffix in {".hlsl", ".hlsli"}:
        target = dst / item
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(project / item, target)
tree.write(dst / "CG2_00_01.vcxproj", encoding="utf-8", xml_declaration=True)
