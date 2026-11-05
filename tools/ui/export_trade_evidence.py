"""Export original shop/sale captures, with matched-state and test provenance."""
import html
import json
import shutil
from PIL import Image
from prepare_trade_qa import qa


def export():
    qa.protect()
    destination = qa.OUT / "presentation"
    destination.mkdir(exist_ok=True)
    captures = {}
    baselines = {}
    for phase in ("before", "after"):
        root = qa.OUT / phase
        baseline = json.loads((root / "manifest.json").read_text(encoding="utf-8"))
        baselines[phase] = baseline
        for relative, expected in baseline["runtime"].items():
            if qa.digest(root / "runtime" / relative) != expected:
                raise RuntimeError(f"Recorded runtime changed: {phase}/{relative}")
        target = destination / phase
        target.mkdir(exist_ok=True)
        for source in sorted((root / "screenshots").glob("*.jpg")):
            with Image.open(source) as picture:
                if picture.size != (1282, 752) or picture.format != "JPEG":
                    raise RuntimeError(f"Unexpected capture format: {source}")
            shutil.copy2(source, target / source.name)
            captures[f"{phase}/{source.name}"] = qa.digest(source)
    if baselines["before"]["fixture_hashes"] != baselines["after"]["fixture_hashes"]:
        raise RuntimeError("Comparison fixtures differ")
    for relative, expected in baselines["after"]["code"].items():
        if qa.digest(qa.ROOT / relative) != expected:
            raise RuntimeError(f"Source changed since the after build: {relative}")

    pairs = [
        ("種屋：選択と購入内容", "before/01_shop.jpg", "after/01_shop.jpg",
         "種類の入口を3つ並べ、個数・合計・購入後の残金・購入ボタンを右側にまとめました。両画像とも300G、ニンジンの種5個、購入数1です。"),
        ("種屋：確認画面", "before/02_shop_confirm.jpg", "after/02_shop_confirm.jpg",
         "確認する金額の近くに確定ボタンを置きました。確認中は種類と個数を変更できません。両画像とも購入確定前です。"),
        ("収穫物：売却と詳細の分離", "before/03_inventory.jpg", "after/03_sale_overview.jpg",
         "同じ収穫物を、変更前は収穫記録単位、変更後の入口は作物の種類単位で表示しています。同じページ構造の比較ではなく、入口で見せる情報の比較です。品質・大きさ・出品予約は詳細画面に残しました。"),
    ]
    extras = [
        ("after/04_sale_confirm.jpg", "売却前の確認", "9個を1390Gで売却し、保護中の1個を残す見積もりです。"),
        ("after/05_storage_details.jpg", "残している保管・大会の詳細", "必要なときに品質や大きさ、保護・出品予約を確認する画面です。ここ自体の全面整理は未実施です。"),
        ("after/06_crop_quote.jpg", "種類別の見積もり", "ニンジンは所持3個のうち2個を売却できます。保護中の1個は除外しています。"),
        ("after/07_sale_complete.jpg", "売却後の状態", "ここからは取引後の検証画像です。9個の売却後、保護したニンジン1個だけが残りました。"),
        ("after/08_empty_sale.jpg", "売れる作物がない状態", "トマトの在庫が0個なので、確認ボタンを無効にして理由を表示しています。"),
        ("after/09_purchase_complete.jpg", "購入後の状態", "売却後1690Gからニンジンの種2個を120Gで購入。所持金1570G、種7個です。右の残金1450Gは、さらに同じ個数を買う場合の次の見積もりです。"),
    ]
    parts = ['''<!doctype html><html lang="ja"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>水路農業 種屋・売却UIの比較</title>
<style>
body{margin:0;background:#fff;color:#202b2c;font:16px/1.6 "Yu Gothic UI",sans-serif;letter-spacing:0}
main{max-width:1500px;margin:auto;padding:24px}h1{font-size:28px}h2{font-size:22px;margin-top:30px}
section{border-top:1px solid #c7d1d2;margin-top:24px;padding-top:4px;break-inside:avoid}
.pair{display:grid;grid-template-columns:1fr 1fr;gap:20px}figure{margin:0}figcaption{font-weight:bold;margin:8px 0}
img{display:block;width:100%;height:auto}a{color:#066775}.extra{max-width:1000px}
@media(max-width:900px){.pair{grid-template-columns:1fr}main{padding:16px}}
@media print{section{break-before:page}main{padding:0}}
</style><main><h1>水路農業 種屋・売却UIの比較</h1>
<p>2026年11月2日。種を買う操作と、収穫物を売る操作に必要な情報を整理しました。</p>
<p>未加工の実行画面JPEGです。比較用の専用データは9日目、フリー農業、300G、各種の種5個、収穫物10個（うち保護1個）。栽培の成果を示すデータではありません。ゲーム領域1280×720、ウィンドウ込み1282×752。草の風・カーソル位置は一致していません。</p>
<p><a href="発表メモ.md">5分・6枚への組み込み方とカンペ</a> / <a href="README.md">素材と確認範囲</a></p>''']

    def figure(relative, label):
        if relative not in captures:
            raise RuntimeError(f"Missing evidence: {relative}")
        return (f"<figure><figcaption>{html.escape(label)}</figcaption><a href='{relative}'>"
                f"<img src='{relative}' alt='{html.escape(label)}' width='1282' height='752'></a></figure>")

    for title, before, after, explanation in pairs:
        parts.append(f"<section><h2>{title}</h2><p>{explanation}</p><div class='pair'>")
        parts.extend((figure(before, "変更前"), figure(after, "変更後"), "</div></section>"))
    for relative, title, explanation in extras:
        parts.append(f"<section class='extra'><h2>{title}</h2><p>{explanation}</p>{figure(relative, title)}</section>")
    parts.append("</main></html>")
    (destination / "index.html").write_text("\n".join(parts), encoding="utf-8")
    (destination / "image_hashes.json").write_text(json.dumps(captures, ensure_ascii=False, indent=2), encoding="utf-8")
    report = {
        "date": "2026-11-02", "captures": captures,
        "manifest_hashes": {p: qa.digest(qa.OUT / p / "manifest.json") for p in baselines},
        "fixtures_match": True,
        "baseline_scope": "Before already includes the preceding five-menu overview work; not the October baseline",
        "before_code_limit": "Before manifest covers tracked sources only; runtime hashes include the actual executable and resources",
        "build_logs": {p.name: qa.digest(p) for p in qa.OUT.glob("build_*_*.log")},
        "test_logs": {p.name: qa.digest(p) for p in qa.OUT.glob("test_*.log")},
        "native_observed": ["all three seed tabs", "purchase confirmation/cancel", "purchase 2 seeds for 120G",
                            "sale details round trip", "crop-specific quote", "sale confirmation/cancel",
                            "9 crops sold for 1390G, one protected retained", "empty sale disabled",
                            "normal window close exit0"],
        "not_established": ["physical keyboard/pad", "Debug native UI", "other PC/GPU/DPI",
                            "usability time reduction", "full season", "all game screens redesigned"],
    }
    (qa.OUT / "evidence.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    qa.protect()
    print(f"Verified {len(captures)} unmodified JPEG captures: {destination}")


if __name__ == "__main__":
    export()
