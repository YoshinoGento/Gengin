"""Export unmodified game captures and a portable presentation reference."""
from pathlib import Path
import html
import json
import shutil
import subprocess
from PIL import Image
from prepare_menu_qa import ROOT, OUT, digest, protect


def export():
    protect()
    destination = OUT / "presentation"
    destination.mkdir(exist_ok=True)
    captures = {}
    for phase in ("before", "after"):
        baseline = json.loads((OUT / phase / "manifest.json").read_text(encoding="utf-8"))
        for relative, expected in baseline["runtime"].items():
            assert digest(OUT / phase / "runtime" / relative) == expected, relative
        target = destination / phase
        target.mkdir(exist_ok=True)
        for source in sorted((OUT / phase / "screenshots").glob("*.png")):
            with Image.open(source) as picture:
                assert picture.size == (1282, 752), (source, picture.size)
            shutil.copy2(source, target / source.name)
            captures[f"{phase}/{source.name}"] = digest(source)
    diagram = ROOT / "generated/ui_presentation_20261016/assets/03_request_flow.png"
    shutil.copy2(diagram, destination / "request_flow_reference.png")
    captures["request_flow_reference.png"] = digest(diagram)
    pages = [
        ("01_farm", "農作業", "本文の操作を14項目から6項目へ整理。従来の操作は「詳しい操作」に残しています。"),
        ("02_terrain", "水路と地形", "農場での編集への入口と、現在の給水状態を分けました。"),
        ("03_records", "セーブ", "保存する操作と、読み込む記録の選択を分けました。"),
        ("04_observe", "育成比較", "比較対象と、比較を開始できない理由を残して表示を絞りました。"),
        ("05_settings", "設定", "音・カメラ・モードを見出しで区切り、音量設定への入口を追加しました。"),
    ]
    parts = ['''<!doctype html><html lang="ja"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>水路農業 UI改善の比較</title>
<style>
body{margin:0;background:#fff;color:#202b2c;font:16px/1.6 "Yu Gothic UI",sans-serif;letter-spacing:0}
main{max-width:1500px;margin:auto;padding:24px}h1{font-size:28px}h2{font-size:22px;margin-top:36px}
section{border-top:1px solid #c7d1d2;margin-top:24px;padding-top:4px;break-inside:avoid}
.pair{display:grid;grid-template-columns:1fr 1fr;gap:20px}figure{margin:0}figcaption{font-weight:bold;margin:8px 0}
img{display:block;width:100%;height:auto}a{color:#066775}.extra{max-width:900px}
@media(max-width:900px){.pair{grid-template-columns:1fr}main{padding:16px}}
@media print{section{break-before:page}main{padding:0}h1{font-size:22px}}
</style><main><h1>水路農業 UI改善の比較</h1>
<p>2026年11月2日。農場メニュー5分類と、ゲーム内の音量設定を整理しました。</p>
<p>すべて実行中のゲームを撮影した未加工PNGです。共通条件は30日間の大会、初期農場、1日目、300G、種0個、保存記録なし、1280×720のゲーム領域です。草の揺れ・ポインター位置・撮影時刻は一致していません。</p>
<p>スライド用の原画像は下の画像から開けます。<a href="発表メモ.md">5分6枚の発表メモ</a> / <a href="README.md">素材一覧と確認範囲</a></p>''']
    for filename, title, explanation in pages:
        parts.append(f"<section><h2>{html.escape(title)}</h2><p>{html.escape(explanation)}</p><div class='pair'>")
        for phase, label in (("before", "変更前"), ("after", "変更後")):
            relative = f"{phase}/{filename}.png"
            assert relative in captures
            parts.append(f"<figure><figcaption>{label}</figcaption><a href='{relative}'><img src='{relative}' alt='{title} {label}' width='1282' height='752'></a></figure>")
        parts.append("</div></section>")
    for filename, title in (("06_farm_details", "従来の操作を残した詳細画面"), ("07_audio", "ゲーム内から開ける音量設定"),
                            ("09_mode_confirmation", "モードを変える前の確認")):
        parts.append(f"<section class='extra'><h2>{title}</h2><a href='after/{filename}.png'><img src='after/{filename}.png' alt='{title}'></a></section>")
    parts.append("<section><h2>処理の役割を説明する補助図</h2><p>10月16日の図を再利用。道具選択を例にした既存の処理の説明で、今回新しく作った経路という意味ではありません。</p><img src='request_flow_reference.png' alt='UIが操作を通知しSystemが状態を変更する流れ'></section></main></html>")
    (destination / "index.html").write_text("\n".join(parts), encoding="utf-8")
    code = subprocess.check_output(["git", "ls-files", "--cached", "--others", "--exclude-standard",
                                    "project/application", "project/engine"], cwd=ROOT, text=True).splitlines()
    report = {
        "date": "2026-11-02", "captures": captures,
        "after_code": {p: digest(ROOT / p) for p in code if (ROOT / p).is_file()},
        "before_revision": json.loads((OUT / "before/manifest.json").read_text(encoding="utf-8"))["revision"],
        "after_revision": "same HEAD plus uncommitted UI changes",
        "build_logs": {name: digest(OUT / name) for name in ("build_before_Release.log", "build_after_Release.log", "build_after_Debug.log")},
        "test_logs": {name: digest(OUT / name) for name in ("test_farm_runtime_view.log", "test_title_audio_settings.log")},
        "native_observed": ["five matching menu pages", "summary/detail mouse round trip", "audio drag to zero",
                            "audio JSON save and reopen", "audio reset to 40", "mode confirmation and cancel"],
        "not_established": ["physical keyboard/pad operation", "audio perception", "other PC/GPU/DPI",
                            "usability time reduction", "all game screens redesigned"],
        "button_metric": {"page": "farm", "region": "body only, includes disabled, excludes tabs/footer",
                          "before": 14, "after": 6},
    }
    (OUT / "evidence.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    (destination / "image_hashes.json").write_text(json.dumps(captures, ensure_ascii=False, indent=2), encoding="utf-8")
    protect()
    print(destination)
    print(f"Verified {len(captures)} unmodified image files")


if __name__ == "__main__":
    export()
