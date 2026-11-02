"""Create a Release-only portable source snapshot and a matching runtime."""
from pathlib import Path
import argparse
import json
import shutil
import subprocess
import xml.etree.ElementTree as ET

from prepare_monthly import ROOT, OUT, ACCOUNT, BASE, VERIFY, digest, write_json, protect

SOURCE = OUT / "source_master"
WORK = OUT / "source_build"
FINAL = OUT / "package" / ACCOUNT
RUNTIME = FINAL / "03_実行版"
DEP = ROOT / "generated/kurihaku_20260928/known_dependencies"
NS = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}
ET.register_namespace("", NS["m"])


def copy(src, dst):
    if not src.is_file() or src.is_symlink():
        raise RuntimeError(f"Invalid source {src}")
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)


def manifest(folder):
    return [{"path": p.relative_to(folder).as_posix(), "bytes": p.stat().st_size,
             "sha256": digest(p)} for p in sorted(folder.rglob("*")) if p.is_file()]


def resources():
    base = ROOT / "project/Resources"
    paths = set()
    for directory in ("farm", "ui", "shader", "generated", "title", "transition"):
        paths.update(p for p in (base / directory).rglob("*")
                     if p.is_file() and p.suffix.lower() in
                     {".obj", ".png", ".json", ".txt", ".wav", ".hlsl", ".hlsli"})
    paths.update(base / p for p in ("text_textures.json", "levels/farm_scene.json",
                 "fonts/NotoSansJP-VF.ttf", "fonts/OFL.txt", "fonts/NotoSerifJP-OFL.txt"))
    return sorted(paths)


def project_files(relative):
    project = ROOT / relative
    copy(project, SOURCE / relative)
    tree = ET.parse(project)
    for kind in ("ClCompile", "ClInclude", "FxCompile"):
        for item in tree.findall(f".//m:{kind}[@Include]", NS):
            p = (project.parent / item.get("Include").replace("\\", "/")).resolve()
            copy(p, SOURCE / p.relative_to(ROOT))


def release_project(path):
    tree = ET.parse(path)
    for parent in list(tree.iter()):
        for node in list(parent):
            if node.tag.endswith("ProjectConfiguration") and node.get("Include", "").lower() != "release|x64":
                parent.remove(node)
            elif any(c in node.get("Condition", "").lower() for c in
                     ("debug|", "development|", "profile|")):
                parent.remove(node)
    for node in tree.findall('.//m:Import[@Label="LocalAppDataPlatform"]', NS):
        node.set("Condition", "false")
    if path.name == "CG2_00_01.vcxproj":
        for node in tree.findall(".//m:AdditionalIncludeDirectories", NS):
            node.text = node.text.replace("externals\\assimp\\include", "externals\\assimp-official\\include")
        for node in tree.findall(".//m:AdditionalLibraryDirectories", NS):
            node.text = node.text.replace("externals\\assimp\\lib\\Release", "externals\\assimp-official\\lib")
        for node in tree.findall(".//m:AdditionalDependencies", NS):
            node.text = node.text.replace("assimp-vc143-mt.lib", "assimp-official.lib;zlibstatic.lib")
        for node in tree.findall(".//m:PostBuildEvent/m:Command", NS):
            node.text = "\n".join(f'copy /Y "$(ProjectDir)externals\\dxc\\{n}" "$(TargetDir){n}"'
                                  for n in ("dxcompiler.dll", "dxil.dll"))
        # Remove IDE-only sample-asset references; the actual build items stay intact.
        for group in tree.findall("m:ItemGroup", NS):
            for node in list(group):
                if node.tag.split("}")[-1] in ("None", "Image", "Text") and node.get("Include"):
                    p = path.parent / node.get("Include").replace("\\", "/")
                    if not p.is_file():
                        group.remove(node)
    tree.write(path, encoding="utf-8", xml_declaration=True)


def stage():
    protect()
    if SOURCE.exists() or WORK.exists():
        raise RuntimeError("Source snapshot already exists")
    dependency = json.loads((DEP / "assimp_build.json").read_text(encoding="utf-8"))
    for entry in dependency["libraries"]:
        assert digest(DEP / entry["path"]) == entry["sha256"]
    projects = ("project/CG2_00_01.vcxproj", "project/externals/imgui/imgui.vcxproj",
                "project/externals/DirectXTex/DirectXTex_Desktop_2022_Win10.vcxproj")
    for relative in projects:
        project_files(relative)
    for relative in ("project/application/farm/data/FarmRules.h",
                     "project/application/title/TitleCloudMotionSystem.h",
                     "project/engine/debug/SkinningDebugWindow.h",
                     "project/externals/DirectXTex/DirectXTex.inl",
                     "project/externals/DirectXTex/Shaders/CompileShaders.cmd",
                     "project/externals/nlohmann/json.hpp", "project/externals/imgui/LICENSE.txt",
                     "project/CG2_00_01.sln", "StartGame.cmd"):
        copy(ROOT / relative, SOURCE / relative)
    for p in (ROOT / "project/externals/DirectXTex/Shaders").glob("*.hlsl"):
        copy(p, SOURCE / p.relative_to(ROOT))
    for p in (ROOT / "project/externals/DirectXTex").iterdir():
        if p.is_file() and p.suffix in (".h", ".inl"):
            copy(p, SOURCE / p.relative_to(ROOT))
    for directory in ("assimp-source/include", "assimp-build/include"):
        for p in (DEP / directory).rglob("*"):
            if p.is_file():
                copy(p, SOURCE / "project/externals/assimp-official/include" / p.relative_to(DEP / directory))
    for p in (DEP / "assimp-source/contrib/utf8cpp").rglob("*"):
        if p.is_file():
            copy(p, SOURCE / "project/externals/assimp-official/contrib/utf8cpp" /
                 p.relative_to(DEP / "assimp-source/contrib/utf8cpp"))
    copy(DEP / "assimp-build/lib/assimp-vc145-mt.lib", SOURCE / "project/externals/assimp-official/lib/assimp-official.lib")
    copy(DEP / "assimp-build/contrib/zlib/zlibstatic.lib", SOURCE / "project/externals/assimp-official/lib/zlibstatic.lib")
    for name in ("dxcompiler.dll", "dxil.dll"):
        copy(DEP / "dxc-release/bin/x64" / name, SOURCE / "project/externals/dxc" / name)
    for p in resources():
        copy(p, SOURCE / p.relative_to(ROOT))
    copy(ROOT / "project/Resources/fonts/NotoSerifJP-VF.ttf", SOURCE / "project/Resources/fonts/NotoSerifJP-VF.ttf")
    # Keep full license notices, but regenerate binary provenance for this build.
    for p in (BASE / "Licenses").rglob("*"):
        if p.is_file() and p.name != "SOURCES.json":
            copy(p, SOURCE / "Licenses" / p.relative_to(BASE / "Licenses"))
    copy(BASE / "Licenses/NotoSansJP_OFL.txt", SOURCE / "project/Resources/fonts/OFL.txt")
    for relative in projects:
        release_project(SOURCE / relative)
    sln = SOURCE / "project/CG2_00_01.sln"
    sln.write_text("\n".join(line for line in sln.read_text(encoding="utf-8-sig").splitlines()
                           if not any(c in line for c in ("Debug|", "Development|", "Profile|"))) + "\n",
                   encoding="utf-8-sig")
    for name in ("BuildRelease.cmd", "BUILD.txt"):
        copy(Path(__file__).parent / name, SOURCE / name)
    write_json(SOURCE / "SOURCE_REVISION.json", {
        "commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        "date": "2026-10-28", "configuration": "Release|x64", "game_code_changed": False,
        "distribution_changes": ["Release-only project configurations", "Portable official Assimp/DXC dependency paths",
                                 "Removed unavailable IDE-only sample items", "Complete Noto Sans license notice"],
        "assimp": dependency, "downloads": json.loads((DEP / "downloads.json").read_text(encoding="utf-8"))})
    write_json(VERIFY / "source_manifest.json", manifest(SOURCE))
    shutil.copytree(SOURCE, WORK)
    protect()
    print(f"Source staged: {len(manifest(SOURCE))} files")


def build():
    with (VERIFY / "source_build.log").open("wb") as log:
        p = subprocess.run(["cmd.exe", "/d", "/c", str(WORK / "BuildRelease.cmd"), "nopause"],
                           cwd=WORK, stdout=log, stderr=subprocess.STDOUT)
    write_json(VERIFY / "source_build.json", {"exit_code": p.returncode, "configuration": "Release|x64"})
    print(f"Source build exit={p.returncode}")
    if p.returncode:
        raise SystemExit(p.returncode)


def complete_headers():
    for p in (ROOT / "project/externals/DirectXTex").iterdir():
        if p.is_file() and p.suffix in (".h", ".inl"):
            for destination in (SOURCE, WORK):
                copy(p, destination / p.relative_to(ROOT))
    write_json(VERIFY / "source_manifest.json", manifest(SOURCE))


def supporting_tools():
    targets = [SOURCE, WORK, FINAL / "04_ソースコード・アセット一式"]
    relatives = ["tools/title/contest_voxel.py", "tools/title/generate_contest_ceremony.py",
                 "tools/title/generate_title_landscape.py", "tools/title/voxel_scenery.py",
                 "tools/title/tree_scenery.py", "tools/ui/farm_item_icons.py",
                 "tools/ui/generate_farm_hud_icons.py"]
    for relative in relatives:
        for target in targets:
            copy(ROOT / relative, target / relative)
    for target in targets:
        (target / "tools/README.txt").write_text(
            "資料で説明したメッシュ・アイコン生成処理のソースです。\n"
            "ゲームのビルド時には実行不要です。生成済みアセットを同梱しています。\n"
            "再生成にはPython 3、Pillow、NumPyおよび参照先のWindowsフォントが必要です。\n"
            "生成処理は同梱アセットを書き換えるため、確認用コピーで実行してください。\n",
            encoding="utf-8-sig")
    write_json(VERIFY / "source_manifest.json", manifest(SOURCE))


def runtime():
    protect()
    assert json.loads((VERIFY / "source_build.json").read_text())["exit_code"] == 0
    assert manifest(SOURCE) == json.loads((VERIFY / "source_manifest.json").read_text(encoding="utf-8"))
    if FINAL.exists():
        raise RuntimeError("Final candidate exists")
    shutil.copytree(SOURCE, FINAL / "04_ソースコード・アセット一式")
    for p in resources():
        relative = p.relative_to(ROOT / "project")
        copy(SOURCE / "project" / relative, RUNTIME / relative)
    for name in ("CG2_00_01.exe", "dxcompiler.dll", "dxil.dll"):
        copy(WORK / "generated/outputs/Release" / name,
             RUNTIME / ("SuiroNogyo.exe" if name.endswith(".exe") else name))
    shutil.copytree(SOURCE / "Licenses", RUNTIME / "Licenses")
    provenance = json.loads((SOURCE / "SOURCE_REVISION.json").read_text(encoding="utf-8"))
    provenance["binaries"] = {n: digest(RUNTIME / n) for n in ("SuiroNogyo.exe", "dxcompiler.dll", "dxil.dll")}
    write_json(RUNTIME / "Licenses/SOURCES.json", provenance)
    for name in ("StartGame.cmd", "CREDITS.txt"):
        copy(OUT / "staging" / ACCOUNT / "03_実行版" / name, RUNTIME / name)
    copy(Path(__file__).parent / "RUNTIME_README.txt", RUNTIME / "はじめにお読みください.txt")
    write_json(VERIFY / "final_runtime_manifest.json", manifest(RUNTIME))
    protect()
    print(f"Matching normal runtime staged: {len(manifest(RUNTIME))} files")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("step", choices=("stage", "build", "runtime", "complete_headers", "supporting_tools"))
    globals()[parser.parse_args().step]()
