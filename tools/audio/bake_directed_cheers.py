"""Generate raw cheer takes using the two approved, entirely synthetic references."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import wave

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "tools/audio/contest_cheers.json"
REFERENCE = ROOT / "tools/audio/cheer_reference"
OUTPUT = ROOT / "generated/audio/directed_contest_cheers"
MODEL = "Qwen/Qwen3-TTS-12Hz-1.7B-Base"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    os.environ["HF_HUB_DISABLE_IMPLICIT_TOKEN"] = "1"
    os.environ["HF_HUB_DISABLE_TELEMETRY"] = "1"
    os.environ["HF_HOME"] = str(ROOT / "generated/tools/qwen_voice_preview_cache")
    sys.path.insert(0, str(ROOT / "generated/tools/qwen_voice_preview_client"))
    from gradio_client import Client, handle_file

    OUTPUT.mkdir(parents=True, exist_ok=True)
    data = json.loads(SOURCE.read_text(encoding="utf-8"))
    references = json.loads((REFERENCE / "manifest.json").read_text(encoding="utf-8"))["clips"]
    assert len(references) == 2 and [r["side"] for r in references] == ["L", "R"]
    client = None
    for pair in data["pairs"]:
        assert pair["id"].isascii() and pair["id"].isalnum()
        for side, ref in zip(("L", "R"), references):
            path = REFERENCE / ref["file"]
            assert digest(path) == ref["sha256"]
            text = "".join(pair[side])
            key = {"pair": pair["id"], "side": side, "text": text,
                   "reference_sha256": ref["sha256"], "reference_text": ref["text"],
                   "model": MODEL if text != ref["text"] else "Qwen/Qwen3-TTS-12Hz-1.7B-VoiceDesign"}
            name = pair["id"] + "_" + side
            target, metadata = OUTPUT / (name + ".wav"), OUTPUT / (name + ".json")
            if metadata.exists() and target.exists():
                old = json.loads(metadata.read_text(encoding="utf-8"))
                assert old["request"] == key and old["sha256"] == digest(target), name
                print("Cached " + name, flush=True)
                continue
            if text == ref["text"]:
                filename = path
            else:
                if client is None:
                    client = Client("Qwen/Qwen3-TTS", token=False, download_files=str(OUTPUT / "downloads"))
                print("Generating " + name, flush=True)
                job = client.submit(ref_audio=handle_file(str(path)), ref_text=ref["text"],
                                    target_text=text, language="Japanese", use_xvector_only=False,
                                    model_size="1.7B", api_name="/generate_voice_clone")
                try:
                    filename, status = job.result(timeout=180)
                except TimeoutError:
                    job.cancel()
                    raise
                if not filename:
                    raise RuntimeError(status)
                print(status, flush=True)
            with wave.open(str(filename), "rb") as wav:
                assert (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) == (1, 2, 24000)
                seconds = wav.getnframes() / wav.getframerate()
                assert .3 < seconds < 4.5, (name, seconds)
            shutil.copyfile(filename, target)
            metadata.write_text(json.dumps({"request": key, "sha256": digest(target),
                                            "seconds": seconds}, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
            print(f"Saved {name}: {seconds:.3f}s", flush=True)
    print("COMPLETE: all32 raw takes available; game assets not modified", flush=True)


if __name__ == "__main__":
    main()
