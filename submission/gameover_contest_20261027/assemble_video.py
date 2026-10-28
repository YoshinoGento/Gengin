"""Trim verified exhibition recordings at 1x; preserve actual game audio and UI."""
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "generated/exhibitions/contest_submission_20261027"
WORK = OUT / "verification"
NAME = "LE3C_28_ヨシノ_ゲント_水路農業"
CLIPS = (
    ("recording01", 0, 10, "Title pond and exhibition entry"),
    ("recording01", 63, 76, "Final failure reveal and 40-point shortage"),
    ("recording01", 97, 108, "Quality radar and water-care advice"),
    ("recording02", 38, 50, "Actual specimen and ideal-quality size reference"),
    ("recording04", 7, 18, "Passing reveal for comparison"),
    ("recording03", 12, 23, "Missing entry reason and next action"),
)


def run(args):
    return subprocess.run(args, check=True, capture_output=True, text=True,
                          encoding="utf-8", errors="replace")


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main():
    target = OUT / (NAME + ".mp4")
    if target.exists():
        raise FileExistsError(target)
    takes = list(dict.fromkeys(take for take, *_ in CLIPS))
    sources = {}
    arguments = ["ffmpeg", "-nostdin", "-hide_banner", "-loglevel", "error"]
    for take in takes:
        source = WORK / take / "capture.mp4"
        verified = json.loads((source.parent / "verification.json").read_text(encoding="utf-8"))
        if verified["decode_check"] != "passed":
            raise RuntimeError("Decoded process-only recording required")
        sources[take] = {"path": str(source), "sha256": digest(source),
                         "audio": verified["audio"], "alignment": verified["alignment"]}
        arguments += ["-i", str(source)]
    graph = []
    for index, (take, start, end, _) in enumerate(CLIPS):
        source_index = takes.index(take)
        graph += [f"[{source_index}:v]trim=start={start}:end={end},setpts=PTS-STARTPTS[v{index}]",
                  f"[{source_index}:a]atrim=start={start}:end={end},asetpts=PTS-STARTPTS,"
                  f"afade=t=in:d=0.03,afade=t=out:st={end-start-.03}:d=0.03[a{index}]"]
    graph.append("".join(f"[v{i}][a{i}]" for i in range(len(CLIPS))) +
                 f"concat=n={len(CLIPS)}:v=1:a=1[v][a]")
    run(arguments + ["-filter_complex", ";".join(graph), "-map", "[v]", "-map", "[a]",
                     "-c:v", "libx264", "-preset", "medium", "-crf", "20", "-pix_fmt", "yuv420p",
                     "-r", "30", "-fps_mode", "cfr", "-c:a", "aac", "-b:a", "128k",
                     "-movflags", "+faststart", str(target)])
    decoded = run(["ffmpeg", "-nostdin", "-v", "error", "-xerror", "-i", str(target),
                   "-map", "0:v:0", "-map", "0:a:0", "-f", "null", "NUL"])
    if decoded.stderr.strip():
        raise RuntimeError(decoded.stderr)
    metadata = json.loads(run(["ffprobe", "-v", "error", "-show_streams", "-show_format",
                               "-of", "json", str(target)]).stdout)
    video = next(s for s in metadata["streams"] if s["codec_type"] == "video")
    audio = next(s for s in metadata["streams"] if s["codec_type"] == "audio")
    duration = sum(end-start for _, start, end, _ in CLIPS)
    if (video["codec_name"], video["width"], video["height"], video["r_frame_rate"],
        audio["codec_name"], int(video["nb_frames"])) != ("h264", 1280, 720, "30/1", "aac", duration*30):
        raise RuntimeError("Unexpected delivery streams/frames")
    if abs(float(metadata["format"]["duration"]) - duration) > .1:
        raise RuntimeError("Wrong delivery duration")
    levels = run(["ffmpeg", "-nostdin", "-hide_banner", "-i", str(target), "-vn",
                  "-af", "volumedetect", "-f", "null", "NUL"]).stderr
    (WORK / "video_levels.log").write_text(levels, encoding="utf-8")
    peak = re.search(r"max_volume: (-?[0-9.]+) dB", levels)
    if peak is None or not -80 < float(peak[1]) < 0:
        raise RuntimeError("Silent or clipped recorded audio")
    result = {"output": str(target), "bytes": target.stat().st_size, "sha256": digest(target),
              "duration_seconds": duration, "decode_check": "passed", "streams": metadata,
              "runtime_exe_sha256": digest(OUT / NAME / "SuiroNogyoExhibition.exe"),
              "sources": sources, "clips": [{"take": take, "start": start, "end": end, "content": label}
                                             for take, start, end, label in CLIPS],
              "editing": "Actual exhibition UI at 1x; idle sections removed; no added visuals or sounds; 30ms audio edge fades only",
              "visual_review": "pending", "external_upload": "not performed"}
    (WORK / "video_report.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"PASS {duration}s, {target.stat().st_size} bytes, H264/AAC, full decode, peak {peak[1]}dBFS")


if __name__ == "__main__":
    main()
