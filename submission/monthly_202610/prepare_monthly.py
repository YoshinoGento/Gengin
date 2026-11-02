"""Stage a fresh monthly submission without changing the game or old deliveries."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "generated/monthly_202610"
ACCOUNT = "LE3C_28_ヨシノ_ゲント"
PACKAGE = OUT / "staging" / ACCOUNT
RUNTIME = PACKAGE / "03_実行版"
QA = OUT / "review_runtime"
VERIFY = OUT / "verification"
BASE = ROOT / "generated/exhibitions/gameover_demo_submission_20261028" / (ACCOUNT + "_水路農業")


def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding="utf-8")


def protect():
    files = [p for p in (ROOT / "project/Settings").rglob("*") if p.is_file()]
    for folder in ("generated/exhibitions/gameover_demo_submission_20261028",
                   "generated/exhibitions/contest_submission_20261027"):
        files.extend(p for p in (ROOT / folder).glob("*") if p.suffix in (".zip", ".mp4"))
    current = {p.relative_to(ROOT).as_posix(): digest(p) for p in files}
    path = VERIFY / "protected.json"
    if path.exists():
        if json.loads(path.read_text(encoding="utf-8")) != current:
            raise RuntimeError("Protected saves or existing delivery changed")
    else:
        write_json(path, current)


def runtime():
    protect()
    if RUNTIME.exists() or QA.exists():
        raise RuntimeError("Refusing to overwrite a runtime")
    RUNTIME.mkdir(parents=True)
    for name in ("Resources", "Licenses"):
        shutil.copytree(BASE / name, RUNTIME / name)
    for path in (RUNTIME / "Resources").rglob("*"):
        if path.is_file():
            source = ROOT / "project" / path.relative_to(RUNTIME)
            if source.is_file():
                shutil.copy2(source, path)
    for name in ("dxcompiler.dll", "dxil.dll"):
        shutil.copy2(ROOT / "generated/outputs/Release" / name, RUNTIME / name)
    shutil.copy2(ROOT / "generated/outputs/Release/CG2_00_01.exe", RUNTIME / "SuiroNogyo.exe")
    write_json(RUNTIME / "Settings/runtime/contest_exhibition.json", {"enabled": False})
    (RUNTIME / "StartGame.cmd").write_text(
        '@echo off\ncd /d "%~dp0"\nstart "" "SuiroNogyo.exe"\n', encoding="ascii")
    (RUNTIME / "はじめにお読みください.txt").write_text(
        "水路農業 / 2026年10月 月次提出・通常版\n\n"
        "ZIPをすべて展開してから、StartGame.cmd を開いてください。\n"
        "Windows x64 / DirectX 12対応環境が必要です。別PC・別GPUでは未検証です。\n\n"
        "タイトルの池をクリック、またはEnter / Spaceで農場選択へ進みます。\n"
        "まずは「この農場でプレイする」からフリー農業モードを開始できます。\n"
        "畑を選び、下部の道具から耕す・水やり・種まき・収穫を行います。\n"
        "品評会は「30日間の大会モード」で体験できます。10・20・30日目に審査します。\n"
        "結果は「結果」「品質」「大きさ」で切り替えて確認できます。\n"
        "農場メニューから保存・読み込みやタイトルへの移動ができます。\n\n"
        "過去の個人セーブは同梱していません。プレイ後のSettingsはこのフォルダ内に作られます。\n"
        "演出の短い紹介は、別添の作品紹介動画でも確認できます。\n"
        "利用素材と外部ライブラリについてはCREDITS.txtとLicensesを参照してください。\n",
        encoding="utf-8-sig")
    credits = (ROOT / "tools/ui/exhibition/CREDITS.txt").read_text(encoding="utf-8-sig")
    (RUNTIME / "CREDITS.txt").write_text(
        credits.replace("水路農業 / ゲームオーバー演出デモ版", "水路農業 / 月次提出・通常版"),
        encoding="utf-8-sig")
    files = sorted(p for p in RUNTIME.rglob("*") if p.is_file())
    for p in files:
        rel = p.relative_to(RUNTIME).as_posix()
        if p.is_symlink() or any(n in p.parts for n in ("human", ".git", ".codex", ".vs")):
            raise RuntimeError(f"Unexpected runtime path: {rel}")
        if rel.startswith("Settings/") and rel != "Settings/runtime/contest_exhibition.json":
            raise RuntimeError(f"Unexpected personal state: {rel}")
    write_json(VERIFY / "runtime_manifest.json", [
        {"path": p.relative_to(RUNTIME).as_posix(), "bytes": p.stat().st_size, "sha256": digest(p)}
        for p in files])
    shutil.copytree(RUNTIME, QA)
    (QA / "SuiroNogyo.exe").rename(QA / "MonthlyCapture.exe")
    write_json(VERIFY / "runtime_report.json", {
        "source_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        "runtime": str(RUNTIME.relative_to(ROOT)), "files": len(files),
        "bytes": sum(p.stat().st_size for p in files), "demo_enabled": False,
        "personal_saves_copied": 0, "native_check": "pending"})
    protect()
    print(f"Staged ordinary runtime: {len(files)} files")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("step", choices=("runtime", "protect"))
    globals()[parser.parse_args().step]()
