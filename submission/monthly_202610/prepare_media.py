"""Extract real game evidence and edit a captioned monthly introduction."""
from pathlib import Path
import argparse
import json
import shutil
import subprocess
from prepare_monthly import ROOT, OUT, ACCOUNT, VERIFY, write_json, digest

ART = OUT / "media"
NORMAL = OUT / "captures/normal01/capture.mp4"
FARM = OUT / "captures/farm03/capture.mp4"
CEREMONY = ROOT / "generated/exhibitions/gameover_demo_submission_20261028" / (ACCOUNT + "_水路農業.mp4")


def run(args, log=None):
    if log:
        with log.open("wb") as stream:
            subprocess.run(args, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT, check=True)
    else:
        subprocess.run(args, cwd=ROOT, check=True)


def images():
    ART.mkdir(parents=True, exist_ok=True)
    sources = {
        "title": (NORMAL, 15), "evening": (NORMAL, 32), "pond_ripple": (NORMAL, 59.6),
        "arrival": (NORMAL, 90), "hud": (NORMAL, 104), "tilled": (FARM, 12),
        "shop": (FARM, 23), "failed": (CEREMONY, 18), "quality": (CEREMONY, 26),
        "size": (CEREMONY, 33), "passed": (CEREMONY, 46), "missed": (CEREMONY, 61)}
    for name, (source, time) in sources.items():
        run(["ffmpeg", "-nostdin", "-hide_banner", "-loglevel", "error", "-y", "-ss", str(time),
             "-i", str(source), "-frames:v", "1", str(ART / f"{name}.png")])
    before = Path("C:/Users/K024G/Videos/Captures/水路農業 2026_10_16 8_20_01.png")
    shutil.copy2(before, ART / "hud_before.png")
    write_json(VERIFY / "image_sources.json", {
        name: {"video": str(source.relative_to(ROOT)), "seconds": t, "sha256": digest(ART / f"{name}.png")}
        for name, (source, t) in sources.items()})


def video():
    ART.mkdir(parents=True, exist_ok=True)
    segments = [
        (NORMAL, 4, 20, "水路農業｜2026年10月　水の流れと農場づくりをつなぐ", 0),
        (NORMAL, 55, 12, "池へのクリックを、波紋による画面遷移につなげました", 0),
        (NORMAL, 84, 17, "新しい農場への到着を、カメラ演出で見せました", 0),
        (NORMAL, 101, 8, "道具・種を画像に置き換え、常時表示する文章を整理しました", 0),
        (FARM, 6, 10, "選択した畑の状態と、次にできる操作を分けて表示しました", 0),
        (FARM, 18, 10, "種袋の見た目を、ショップと操作欄で対応させました", 0),
        (CEREMONY, 3, 20, "品評会の共通演出：作物を見せてから結果へ（確認用デモで収録）", 0),
        (CEREMONY, 23, 7, "品質の内訳から、次に改善する点を確認できます", 0),
        (CEREMONY, 30, 7, "大きさは、同じ尺度の作物モデルと重さで比較できます", 0),
        (CEREMONY, 37, 10, "入賞時はリボンを添え、結果の違いを見せました", 0),
        (CEREMONY, 51, 10, "未出品も区別し、次の挑戦につながる結果画面にしました", 0),
    ]
    parts = []
    timeline = []
    elapsed = 0
    for i, (src, start, length, caption, gain) in enumerate(segments):
        text = ART / f"caption_{i:02}.txt"
        text.write_text(caption, encoding="utf-8")
        output = ART / f"part_{i:02}.mp4"
        textrel = text.relative_to(ROOT).as_posix()
        filters = ("scale=1152:648:flags=lanczos,pad=1280:720:64:72:color=0x142A2D,setsar=1,"
                   "drawtext=fontfile=project/Resources/fonts/NotoSansJP-VF.ttf:"
                   f"textfile='{textrel}':fontsize=25:fontcolor=white:x=(w-tw)/2:y=22")
        run(["ffmpeg", "-nostdin", "-hide_banner", "-loglevel", "warning", "-y", "-ss", str(start),
             "-i", str(src), "-t", str(length), "-vf", filters,
             "-af", f"volume={gain}dB,afade=t=in:d=0.04,afade=t=out:st={length-.04}:d=0.04",
             "-r", "30", "-c:v", "libx264", "-preset", "medium", "-crf", "20",
             "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "128k", "-ar", "48000", str(output)],
            ART / f"encode_{i:02}.log")
        parts.append(output)
        timeline.append({"source": src.relative_to(ROOT).as_posix(), "start": start,
                         "duration": length, "caption": caption, "output_start": elapsed})
        elapsed += length
    concat = ART / "concat.txt"
    concat.write_text("\n".join(f"file '{p.name}'" for p in parts), encoding="utf-8")
    final = OUT / "documents" / f"{ACCOUNT}_作品紹介動画.mp4"
    final.parent.mkdir(parents=True, exist_ok=True)
    run(["ffmpeg", "-nostdin", "-hide_banner", "-loglevel", "warning", "-y", "-f", "concat",
         "-safe", "0", "-i", str(concat), "-c", "copy", "-movflags", "+faststart", str(final)],
        VERIFY / "video_concat.log")
    run(["ffmpeg", "-nostdin", "-v", "error", "-i", str(final), "-f", "null", "NUL"], VERIFY / "video_decode.log")
    meta = json.loads(subprocess.check_output(["ffprobe", "-v", "error", "-show_streams", "-show_format", "-of", "json", str(final)]))
    assert abs(float(meta["format"]["duration"]) - elapsed) < .4
    assert sum(int(s.get("nb_frames", 0)) for s in meta["streams"] if s["codec_type"] == "video") == elapsed * 30
    write_json(VERIFY / "video_report.json", {"duration": elapsed, "timeline": timeline,
        "sha256": digest(final), "metadata": meta, "decode": "PASS", "visual_review": "pending",
        "footage_note": "Normal-game footage captured Oct28. Contest excerpts are Oct28 shared ceremony from the completed demo; menus excluded. No speed changes."})
    print(f"Encoded {elapsed}s: {final}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("step", choices=("images", "video"))
    globals()[parser.parse_args().step]()
