"""Losslessly share runtime/source assets; leave the original delivery untouched."""
from pathlib import Path, PurePosixPath
import argparse
import collections
import json
import shutil
import subprocess
import time
import xml.etree.ElementTree as ET
import zipfile

from prepare_monthly import ROOT, OUT, ACCOUNT, VERIFY, digest, write_json, protect
from assemble_sources import FINAL, SOURCE, RUNTIME, NS, copy, manifest

HERE = Path(__file__).resolve().parent
STAGE = OUT / "package_small" / ACCOUNT
GAME = STAGE / "03_ゲーム本体"
READY = OUT / "ready_small"
WORK = OUT / "small_build_check"
EXTRACT = OUT / "small_extracted_check"
REPORT = VERIFY / "compact"
PROJECT = "project/CG2_00_01.vcxproj"
CHANGED = {PROJECT, "StartGame.cmd", "BuildRelease.cmd", "BUILD.txt", "SOURCE_REVISION.json"}
DLLS = ("dxcompiler.dll", "dxil.dll")
ROOT_LAUNCHER = (
    '@echo off\nsetlocal\n'
    'for /d %%D in ("%~dp003_*") do (\n'
    '  if exist "%%~fD\\project\\CG2_00_01.sln" (\n'
    '    call "%%~fD\\StartGame.cmd"\n'
    '    exit /b\n'
    '  )\n'
    ')\n'
    'echo Game folder not found. Extract the entire ZIP into a new folder.\n'
    'pause\nexit /b 1\n'
)
ET.register_namespace("", NS["m"])


def text_file(path, text, *, batch=False):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text.replace("\r\n", "\n"), encoding="ascii" if batch else "utf-8-sig",
                    newline="\r\n" if batch else "\n")


def preserve_baseline():
    protect()
    assert manifest(OUT / "ready") == json.loads(
        (VERIFY / "package_report.json").read_text(encoding="utf-8"))["deliverables"]
    assert manifest(SOURCE) == json.loads((VERIFY / "source_manifest.json").read_text(encoding="utf-8"))
    assert manifest(FINAL) == json.loads((VERIFY / "package_report.json").read_text(encoding="utf-8"))["files"]


def shared_project():
    path = GAME / PROJECT
    tree = ET.parse(path)
    outdirs = tree.findall(".//m:OutDir", NS)
    assert len(outdirs) == 1
    outdirs[0].text = "$(ProjectDir)..\\Runtime\\"
    for group in tree.findall("m:PropertyGroup", NS):
        if group.find("m:OutDir", NS) is not None:
            ET.SubElement(group, f"{{{NS['m']}}}TargetName").text = "SuiroNogyo"
    for group in tree.findall("m:ItemDefinitionGroup", NS):
        for event in group.findall("m:PostBuildEvent", NS):
            group.remove(event)
    target = ET.SubElement(tree.getroot(), f"{{{NS['m']}}}Target", {
        "Name": "CheckBundledDxc", "BeforeTargets": "PrepareForBuild"})
    for name in DLLS:
        ET.SubElement(target, f"{{{NS['m']}}}Error", {
            "Condition": f"!Exists('$(ProjectDir)..\\Runtime\\{name}')",
            "Text": f"Missing bundled Runtime/{name}; extract the complete submission ZIP."})
    tree.write(path, encoding="utf-8", xml_declaration=True)


def stage():
    preserve_baseline()
    if STAGE.parent.exists() or WORK.exists() or REPORT.exists():
        raise RuntimeError("Refusing to overwrite an existing compact candidate")
    REPORT.mkdir(parents=True)
    original_entries = manifest(SOURCE)
    for entry in original_entries:
        relative = entry["path"]
        if relative.startswith("project/externals/dxc/"):
            assert Path(relative).name in DLLS
            relative = "Runtime/" + Path(relative).name
        copy(SOURCE / entry["path"], GAME / relative)
    for name in ("SuiroNogyo.exe", "dxcompiler.dll", "dxil.dll"):
        path = GAME / "Runtime" / name
        if path.exists():
            assert digest(path) == digest(RUNTIME / name)
        else:
            copy(RUNTIME / name, path)
    for p in (RUNTIME / "Licenses").rglob("*"):
        if p.is_file():
            dst = GAME / "Licenses" / p.relative_to(RUNTIME / "Licenses")
            if dst.exists():
                assert digest(dst) == digest(p)
            else:
                copy(p, dst)
    copy(RUNTIME / "CREDITS.txt", GAME / "CREDITS.txt")
    shared_project()
    for name in ("StartGame.cmd", "BuildRelease.cmd"):
        text_file(GAME / name, (HERE / ("small_" + name)).read_text(encoding="ascii"), batch=True)
    # The root launcher is ASCII even when the enclosing school filename is Japanese.
    text_file(STAGE / "StartGame.cmd", ROOT_LAUNCHER, batch=True)
    build_text = (SOURCE / "BUILD.txt").read_text(encoding="utf-8-sig")
    build_text = build_text.replace("generated/outputs/Release/CG2_00_01.exe", "Runtime/SuiroNogyo.exe")
    build_text += ("\n■ 軽量版の配置\n"
                   "実行版とソースの素材は project/Resources の1か所にまとめています。\n"
                   "Runtimeにはビルド済みEXEと必須のDXC DLLを同梱しています。\n"
                   "ビルドすると同じRuntime/SuiroNogyo.exeが更新されます。\n"
                   "RuntimeのDLLは外部依存ファイルのため削除しないでください。\n"
                   "Clean/Rebuildで消えるEXEはビルドで再作成されます。\n"
                   "再ビルド確認用：BuildRelease.cmd nopause rebuild\n")
    text_file(GAME / "BUILD.txt", build_text)
    runtime_text = (RUNTIME / "はじめにお読みください.txt").read_text(encoding="utf-8-sig")
    runtime_text = runtime_text.replace("Settingsはこのフォルダ内", "Settingsはprojectフォルダ内")
    runtime_text = runtime_text.replace("Licenses、Resources内", "Licenses、project/Resources内")
    text_file(GAME / "遊び方.txt", runtime_text)
    revision = json.loads((GAME / "SOURCE_REVISION.json").read_text(encoding="utf-8"))
    revision["distribution_changes"].extend([
        "Runtime/source share unchanged project/Resources and license files",
        "Game OutDir is Runtime; TargetName is SuiroNogyo; no duplicate DXC post-build copy",
        "Vendored DXC DLLs stay beside the executable and survive solution Clean/Rebuild"])
    write_json(GAME / "SOURCE_REVISION.json", revision)
    text_file(STAGE / "はじめにお読みください.txt",
              "水路農業 / 2026年10月 月次提出\n\n"
              "土地と水の流れを整えて野菜を育て、品評会に挑む農業ゲームです。\n"
              "今月はタイトルの水辺表現、画面遷移、開始演出、農場UI、品評会を改善しました。\n\n"
              "■ 起動\nZIPをすべて展開し、このフォルダの StartGame.cmd を実行してください。\n"
              "ビルドは不要です。03_ゲーム本体内のStartGame.cmdからも起動できます。\n"
              "Windows x64 / DirectX 12対応環境が必要です。別PC・別GPUでは未確認です。\n\n"
              "01_ポートフォリオ：作品と10月の改善内容（6ページ）\n"
              "02_プログラム説明資料：設計・処理の流れ・制約（8ページ）\n"
              "03_ゲーム本体：実行ファイル・ソースコード・アセット一式\n"
              "04_作品紹介動画：実際の画面による約2分11秒の紹介\n\n"
              "■ ゲーム本体の構成\n"
              "Runtime：ビルド済みEXEと必要なDLL\n"
              "project：ソース、ソリューション、共通のResources\n"
              "tools：資料で説明したアセット生成コード\n"
              "再ビルドは03_ゲーム本体/BUILD.txtをご覧ください。Release / x64専用です。\n"
              "同一素材の二重同梱をなくして容量を削減しています。ゲーム・画質・音質・動画の内容は同じです。\n"
              "素材を共有しているため、Runtimeフォルダだけを移動せず全体を保ってください。\n\n"
              "通常版を収録し、過去の個人セーブは入れていません。\n"
              "初期状態は30日間の大会モードです。農場選択でフリー農業モードにも切り替えられます。\n"
              "品評会の紹介映像・画像は、同じ共通演出を使う確認用デモの撮影を含みます。\n"
              "動画の待ち時間は編集していますが、演出を早送りしてはいません。\n"
              "授業提供の描画・入力基盤を使用しています。制作・資料作成にはAI支援を使用しています。\n"
              "外部素材の条件は03_ゲーム本体のCREDITS.txtとLicensesをご覧ください。\n"
              "音源単体の再配布・無断利用を禁止します。作品の一部としてのみ同梱しています。\n")
    for old, new in (("01_ポートフォリオ", "01_ポートフォリオ"),
                     ("02_プログラム説明資料", "02_プログラム説明資料"),
                     ("05_作品紹介動画", "04_作品紹介動画")):
        shutil.copytree(FINAL / old, STAGE / new)
    verify_content()
    write_json(REPORT / "stage_manifest.json", manifest(STAGE))
    shutil.copytree(GAME, WORK)
    preserve_baseline()
    print("Compact candidate staged; original bytes preserved except distribution metadata/readmes.")


def verify_content():
    mapped = []
    for entry in manifest(SOURCE):
        relative = entry["path"]
        if relative in CHANGED:
            continue
        target = "Runtime/" + Path(relative).name if relative.startswith("project/externals/dxc/") else relative
        assert digest(GAME / target) == entry["sha256"], target
        mapped.append({"original": relative, "shared": target, "sha256": entry["sha256"]})
    for entry in manifest(RUNTIME):
        rel = entry["path"]
        if rel in ("StartGame.cmd", "はじめにお読みください.txt"):
            continue
        if rel.startswith("Resources/"):
            target = "project/" + rel
        elif Path(rel).suffix.lower() in (".exe", ".dll"):
            target = "Runtime/" + rel
        else:
            target = rel
        assert digest(GAME / target) == entry["sha256"], target
    for rel in ("application", "engine"):
        for entry in manifest(GAME / "project" / rel):
            assert digest(ROOT / "project" / rel / entry["path"]) == entry["sha256"]
    for directory in ("01_ポートフォリオ", "02_プログラム説明資料", "04_作品紹介動画"):
        for p in (STAGE / directory).iterdir():
            assert digest(p) == digest(OUT / "ready" / p.name)
    for name in DLLS:
        assert len(list(GAME.rglob(name))) == 1
    write_json(REPORT / "unchanged_source_mapping.json", mapped)


def build():
    preserve_baseline()
    before_dlls = {name: digest(WORK / "Runtime" / name) for name in DLLS}
    # Rebuild runs Clean first, checking that vendored DLLs are not treated as outputs.
    with (REPORT / "rebuild.log").open("wb") as log:
        result = subprocess.run(["cmd.exe", "/d", "/c", str(WORK / "BuildRelease.cmd"), "nopause", "rebuild"],
                                cwd=WORK, stdout=log, stderr=subprocess.STDOUT)
    after_dlls = {name: digest(WORK / "Runtime" / name) for name in DLLS}
    report = {"exit_code": result.returncode, "target": "Rebuild", "configuration": "Release|x64",
              "vendored_dlls_survive_clean": before_dlls == after_dlls,
              "exe_created": (WORK / "Runtime/SuiroNogyo.exe").is_file()}
    write_json(REPORT / "build.json", report)
    assert result.returncode == 0 and report["vendored_dlls_survive_clean"] and report["exe_created"]
    preserve_baseline()
    print(json.dumps(report))


def refresh_launcher():
    preserve_baseline()
    assert manifest(STAGE) == json.loads((REPORT / "stage_manifest.json").read_text(encoding="utf-8"))
    text_file(STAGE / "StartGame.cmd", ROOT_LAUNCHER, batch=True)
    write_json(REPORT / "stage_manifest.json", manifest(STAGE))
    print("Root launcher now selects the shared game tree, not an older runtime-only folder.")


def package():
    preserve_baseline()
    verify_content()
    assert json.loads((REPORT / "build.json").read_text())["exit_code"] == 0
    entries = manifest(STAGE)
    assert entries == json.loads((REPORT / "stage_manifest.json").read_text(encoding="utf-8"))
    denied = {".git", ".codex", ".vs", "__pycache__", "Settings", "node_modules", "human"}
    for p in STAGE.rglob("*"):
        assert not p.is_symlink() and not p.is_junction(), p
        assert not any(part in denied for part in p.relative_to(STAGE).parts), p
        if p.is_file():
            assert p.suffix.lower() not in {".pdb", ".ilk", ".tlog", ".user", ".log", ".pyc", ".zip"}, p
            if p.suffix.lower() == ".obj":
                assert "Resources" in p.parts, p
    if READY.exists() or EXTRACT.exists():
        raise RuntimeError("Refusing to overwrite compact delivery or extraction check")
    READY.mkdir()
    for p in (OUT / "ready").iterdir():
        if p.suffix != ".zip":
            copy(p, READY / p.name)
    archive_path = READY / f"{ACCOUNT}.zip"
    with zipfile.ZipFile(archive_path, "x", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for entry in entries:
            archive.write(STAGE / entry["path"], ACCOUNT + "/" + entry["path"])
    with zipfile.ZipFile(archive_path) as archive:
        assert archive.testzip() is None
        names = archive.namelist()
        assert len(names) == len(set(n.casefold() for n in names))
        for name in names:
            rel = PurePosixPath(name)
            assert not rel.is_absolute() and ".." not in rel.parts and rel.parts[0] == ACCOUNT
            assert (EXTRACT / name).resolve().is_relative_to(EXTRACT.resolve())
        started = time.perf_counter()
        archive.extractall(EXTRACT)
        extraction_seconds = time.perf_counter() - started
    assert manifest(EXTRACT / ACCOUNT) == entries
    assert len(list(READY.iterdir())) == 4
    assert all(0 < p.stat().st_size < 3 * 1024**3 for p in READY.iterdir())
    previous = json.loads((VERIFY / "package_report.json").read_text(encoding="utf-8"))
    original_zip = OUT / "ready" / archive_path.name
    duplicates = collections.defaultdict(list)
    for entry in entries:
        duplicates[entry["sha256"]].append(entry)
    report = {"original_zip_bytes": original_zip.stat().st_size, "compact_zip_bytes": archive_path.stat().st_size,
              "original_uncompressed_bytes": sum(e["bytes"] for e in previous["files"]),
              "compact_uncompressed_bytes": sum(e["bytes"] for e in entries),
              "original_file_count": len(previous["files"]), "compact_file_count": len(entries),
              "remaining_duplicate_raw_bytes": sum(v[0]["bytes"] * (len(v)-1) for v in duplicates.values()),
              "extraction_seconds_this_pc_single_run": extraction_seconds,
              "compression": "standard ZIP/Deflate level9; no lossy asset or video transformation",
              "archive_crc": "PASS", "extracted_all_file_hashes": "PASS", "source_build": "PASS Release|x64 Rebuild",
              "content": "All original game code, required assets, DLLs, EXE, media and license notices retained",
              "native": "pending", "files": entries, "deliverables": manifest(READY), "uploaded": False}
    write_json(REPORT / "package_report.json", report)
    preserve_baseline()
    print(json.dumps({k: v for k, v in report.items() if k not in ("files", "deliverables")}, ensure_ascii=False, indent=2))


def finalize():
    preserve_baseline()
    verify_content()
    report = json.loads((REPORT / "package_report.json").read_text(encoding="utf-8"))
    assert manifest(READY) == report["deliverables"]
    assert manifest(STAGE) == report["files"]
    for entry in report["files"]:
        assert digest(EXTRACT / ACCOUNT / entry["path"]) == entry["sha256"], entry["path"]
    native = json.loads((REPORT / "native_check.json").read_text(encoding="utf-8"))
    assert native["game_exit_code"] == 0 and native["launcher_exit_code"] == 0
    assert native["method"] == "manual observation using Windows Computer Use"
    report["native"] = native
    report["protected_saves_and_previous_deliverables"] = "unchanged"
    report["zip_reduction_percent"] = round(100 * (1 - report["compact_zip_bytes"] / report["original_zip_bytes"]), 2)
    write_json(REPORT / "package_report.json", report)
    print(json.dumps({"zip_reduction_percent": report["zip_reduction_percent"],
                      "deliverables": report["deliverables"], "protection": "PASS"}, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("step", choices=("stage", "build", "package", "refresh_launcher", "finalize"))
    globals()[parser.parse_args().step]()
