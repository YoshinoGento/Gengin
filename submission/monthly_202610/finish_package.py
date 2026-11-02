"""Validate, package, extract, and compare the four required deliverables."""
from pathlib import Path, PurePosixPath
import json
import re
import shutil
import subprocess
import zipfile
from pypdf import PdfReader
from prepare_monthly import ROOT, OUT, ACCOUNT, VERIFY, digest, write_json, protect
from assemble_sources import SOURCE, WORK, FINAL, RUNTIME, copy, manifest

READY = OUT / "ready"


def check_documents():
    results = []
    for kind, count in (("ポートフォリオ", 6), ("プログラム説明資料", 8)):
        p = OUT / "documents" / f"{ACCOUNT}_{kind}.pdf"
        reader = PdfReader(p)
        assert len(reader.pages) == count
        fonts = set()
        for page in reader.pages:
            text = page.extract_text()
            assert len(text) > 150 and "\ufffd" not in text
            for font in page["/Resources"]["/Font"].values():
                obj = font.get_object()
                name = str(obj.get("/BaseFont", ""))
                if "Meiryo" in name:
                    descriptor = obj["/FontDescriptor"].get_object()
                    assert any(key in descriptor for key in ("/FontFile", "/FontFile2", "/FontFile3"))
                    assert "/ToUnicode" in obj
                    fonts.add(name)
        assert len(fonts) >= 2
        boxes = json.loads((VERIFY / f"{kind}_bounds.json").read_text(encoding="utf-8"))
        for i, a in enumerate(boxes):
            for b in boxes[i+1:]:
                if a[0] != b[0]:
                    continue
                if min(a[1]+a[3], b[1]+b[3])-max(a[1], b[1]) > 1 and min(a[2]+a[4], b[2]+b[4])-max(a[2], b[2]) > 1:
                    raise RuntimeError(f"Overlapping text: {a[-1]} / {b[-1]}")
        results.append({"file": p.name, "pages": count, "embedded_Japanese_fonts": sorted(fonts),
                        "text_bounds_and_overlap": "PASS", "visual_review": "all pages rendered and inspected"})
    write_json(VERIFY / "documents_report.json", results)


def check_video():
    p = OUT / "documents" / f"{ACCOUNT}_作品紹介動画.mp4"
    report = json.loads((VERIFY / "video_report.json").read_text(encoding="utf-8"))
    assert digest(p) == report["sha256"]
    result = subprocess.run(["ffmpeg", "-nostdin", "-hide_banner", "-i", str(p), "-af", "volumedetect",
                             "-vn", "-sn", "-f", "null", "NUL"], capture_output=True, text=True, encoding="utf-8")
    assert result.returncode == 0
    (VERIFY / "video_audio.log").write_text(result.stderr, encoding="utf-8")
    peak = float(re.search(r"max_volume: (-?[\d.]+) dB", result.stderr).group(1))
    mean = float(re.search(r"mean_volume: (-?[\d.]+) dB", result.stderr).group(1))
    assert -65 < mean < -10 and -30 < peak < -1
    report.update({"visual_review": "source/final chapter samples inspected; no synthetic gameplay", "audio_peak_db": peak,
                   "audio_mean_db": mean, "audio_boundary": "Objective level check only; subjective balance not independently judged"})
    write_json(VERIFY / "video_report.json", report)


def check_source():
    assert json.loads((VERIFY / "source_build.json").read_text())["exit_code"] == 0
    expected = json.loads((VERIFY / "source_manifest.json").read_text(encoding="utf-8"))
    assert manifest(SOURCE) == expected
    assert manifest(FINAL / "04_ソースコード・アセット一式") == expected
    for entry in expected:
        p = WORK / entry["path"]
        assert digest(p) == entry["sha256"], entry["path"]
        if entry["path"].startswith(("project/application/", "project/engine/")):
            assert digest(ROOT / entry["path"]) == entry["sha256"], "Game source changed"
    assert digest(RUNTIME / "SuiroNogyo.exe") == digest(WORK / "generated/outputs/Release/CG2_00_01.exe")
    for n in ("dxcompiler.dll", "dxil.dll"):
        assert digest(RUNTIME/n) == digest(WORK/"generated/outputs/Release"/n)
    assert not (RUNTIME / "Settings").exists()
    assert not (RUNTIME / "Resources/exhibition").exists()
    write_json(VERIFY / "native_report.json", {
        "built_runtime_sha256": digest(RUNTIME / "SuiroNogyo.exe"),
        "native_check": "PASS ordinary farm selection, full arrival animation, farm HUD, clean exit0 in final_review",
        "earlier_normal_capture": "title click, ripple, arrival, HUD, tilling, seed purchase, exit0; same game code/assets but development dependency build",
        "boundary": "No full30-day playthrough, other-PC/GPU/DPI validation or renewed contest playback on the final dependency build"})


def main():
    protect()
    check_source()
    check_documents()
    check_video()
    if READY.exists() or (OUT / "extracted_check").exists():
        raise RuntimeError("Refusing to overwrite final delivery")
    READY.mkdir(parents=True)
    for directory, kind, ext in (("01_ポートフォリオ", "ポートフォリオ", "pdf"),
                                 ("02_プログラム説明資料", "プログラム説明資料", "pdf"),
                                 ("05_作品紹介動画", "作品紹介動画", "mp4")):
        p = OUT / "documents" / f"{ACCOUNT}_{kind}.{ext}"
        copy(p, FINAL / directory / p.name)
        copy(p, READY / p.name)
    (FINAL / "はじめにお読みください.txt").write_text(
        "水路農業 / 2026年10月 月次提出\n\n"
        "土地と水の流れを整えて野菜を育て、品評会に挑む農業ゲームです。\n"
        "今月はタイトルの水辺表現、画面遷移、開始演出、農場UI、品評会を改善しました。\n\n"
        "01_ポートフォリオ：作品と10月の改善内容（6ページ）\n"
        "02_プログラム説明資料：設計・処理の流れ・制約（8ページ）\n"
        "03_実行版：すべて展開後、StartGame.cmd で起動します。\n"
        "04_ソースコード・アセット一式：BUILD.txtを参照してください。Release / x64専用です。\n"
        "05_作品紹介動画：実際の画面による約2分11秒の紹介です。\n\n"
        "実行版は通常版です。過去の個人セーブは入れていません。\n"
        "初期状態は30日間の大会モードです。農場選択でフリー農業モードにも切り替えられます。\n"
        "品評会の紹介映像・画像は、同じ共通演出を使う確認用デモの撮影を含みます。\n"
        "動画の待ち時間は編集していますが、演出を早送りしてはいません。\n\n"
        "Windows x64 / DirectX 12対応環境が必要です。別PC・別GPUでは未確認です。\n"
        "ビルド済み実行版と、別フォルダでビルド確認した対応ソースを同梱しています。\n"
        "授業提供の描画・入力基盤を使用しています。制作・資料作成にはAI支援を使用しています。\n"
        "外部素材の利用条件は実行版のCREDITS.txtとLicensesを参照してください。\n"
        "音源単体の再配布・無断利用を禁止します。作品の一部としてのみ同梱しています。\n",
        encoding="utf-8-sig")
    entries = manifest(FINAL)
    denied = {".git", ".codex", ".vs", "__pycache__", "Settings", "node_modules", "human"}
    for p in FINAL.rglob("*"):
        assert not p.is_symlink() and not p.is_junction()
        assert not any(part in denied for part in p.relative_to(FINAL).parts), p
        if p.is_file():
            assert p.suffix.lower() not in {".pdb", ".ilk", ".tlog", ".user", ".log", ".pyc", ".zip"}, p
            if p.suffix.lower() == ".obj":
                assert "Resources" in p.parts, p
    target = READY / f"{ACCOUNT}.zip"
    with zipfile.ZipFile(target, "x", zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for e in entries:
            archive.write(FINAL / e["path"], ACCOUNT + "/" + e["path"])
    extracted = OUT / "extracted_check"
    with zipfile.ZipFile(target) as archive:
        assert archive.testzip() is None
        names = archive.namelist()
        assert len(names) == len(set(n.casefold() for n in names))
        for name in names:
            rel = PurePosixPath(name)
            assert not rel.is_absolute() and ".." not in rel.parts and rel.parts[0] == ACCOUNT
            assert (extracted / name).resolve().is_relative_to(extracted.resolve())
        archive.extractall(extracted)
    assert manifest(extracted / ACCOUNT) == entries
    for p in READY.iterdir():
        assert 0 < p.stat().st_size < 3*1024**3, p
    assert len(list(READY.iterdir())) == 4
    for p in READY.iterdir():
        if p.suffix != ".zip":
            duplicates = [e for e in entries if Path(e["path"]).name == p.name]
            assert len(duplicates) == 1 and duplicates[0]["sha256"] == digest(p)
    protect()
    write_json(VERIFY / "package_report.json", {
        "files": entries, "deliverables": manifest(READY), "archive_crc": "PASS",
        "extraction_all_file_hashes": "PASS", "source_build": "PASS Release|x64 from independent copy",
        "savedata_included": 0, "protected_saves_and_earlier_deliveries": "unchanged",
        "baseline": "User identifies previous submission as September; supplied older PDFs dated August. October claims compared against end-September3540754 and September28 guide.",
        "uploaded": False})
    print(json.dumps(manifest(READY), ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
