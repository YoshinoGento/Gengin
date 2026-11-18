"""Audition two synthetic stock-free voices; never changes shipped game assets."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import wave

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "generated/codex_checks/cheer_directed_preview"
MODEL = "Qwen/Qwen3-TTS-12Hz-1.7B-VoiceDesign"
DIRECTIONS = {
    "L": (
        "Native Japanese male spectator in his forties at a bodybuilding competition. "
        "Deep, rough, chesty voice, projecting a loud exuberant SHOUT across the hall. "
        "Explosive attack and a strong sustained final vowel, excited and warmly admiring. "
        "A spontaneous short crowd heckle, not an announcer, not narration, not reading. "
        "Speak only the supplied Japanese words, no added words, music or sound effects."
    ),
    "R": (
        "Native Japanese male spectator in his twenties enthusiastically answering his friend "
        "at a bodybuilding competition. A brighter, higher, slightly hoarse voice. "
        "SHOUT the playful punchline with an excited laughing smile and forceful projection, "
        "rushing into the answer, stress the final command and sustain its final vowel. "
        "Spontaneous crowd banter, not a presenter or text reader. "
        "Speak only the supplied Japanese words, no added words, music or sound effects."
    ),
}


def mix_preview():
    import numpy as np

    clips = []
    for side, pan in (("L", -.35), ("R", .35)):
        with wave.open(str(OUTPUT / f"PumpkinPot_{side}_raw.wav"), "rb") as wav:
            assert (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) == (1, 2, 24000)
            samples = np.frombuffer(wav.readframes(wav.getnframes()), dtype="<i2").astype(np.float64) / 32768
        voiced = np.flatnonzero(np.abs(samples) > .001)
        assert len(voiced)
        samples = samples[max(0, voiced[0] - 480):min(len(samples), voiced[-1] + 481)]
        samples *= min(.14 / np.sqrt(np.mean(samples*samples)), .88 / np.max(np.abs(samples)))
        samples[:120] *= np.linspace(0, 1, 120)
        samples[-120:] *= np.linspace(1, 0, 120)
        angle = (pan + 1) * np.pi / 4
        clips.append(np.column_stack((samples*np.cos(angle), samples*np.sin(angle))) * .60)
    stereo = np.concatenate((np.zeros((12000, 2)), clips[0], np.zeros((8160, 2)), clips[1], np.zeros((12000, 2))))
    assert np.isfinite(stereo).all() and np.max(np.abs(stereo)) < 1
    target = OUTPUT / "PumpkinPot_directed.wav"
    with wave.open(str(target), "wb") as wav:
        wav.setparams((2, 2, 24000, 0, "NONE", "not compressed"))
        wav.writeframes(np.rint(stereo*32767).astype("<i2").tobytes())
    print(target, flush=True)


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--mix-only", action="store_true")
    if parser.parse_args().mix_only:
        mix_preview()
        return
    os.environ["HF_HUB_DISABLE_IMPLICIT_TOKEN"] = "1"
    os.environ["HF_HUB_DISABLE_TELEMETRY"] = "1"
    os.environ["HF_HOME"] = str(ROOT / "generated/tools/qwen_voice_preview_cache")
    sys.path.insert(0, str(ROOT / "generated/tools/qwen_voice_preview_client"))
    from gradio_client import Client

    OUTPUT.mkdir(parents=True, exist_ok=True)
    source = json.loads((ROOT / "tools/audio/contest_cheers.json").read_text(encoding="utf-8"))
    pair = next(p for p in source["pairs"] if p["id"] == "PumpkinPot")
    client = Client("Qwen/Qwen3-TTS", token=False, download_files=str(OUTPUT / "downloads"))
    records = []
    for side in ("L", "R"):
        text = "".join(pair[side])
        print(f"Requesting {side}: {text}", flush=True)
        job = client.submit(text=text, language="Japanese", voice_description=DIRECTIONS[side],
                            api_name="/generate_voice_design")
        try:
            filename, status = job.result(timeout=180)
        except TimeoutError:
            job.cancel()
            raise
        print(status, flush=True)
        if not filename:
            raise RuntimeError("The public demo did not return an audio file")
        target = OUTPUT / f"PumpkinPot_{side}_raw.wav"
        shutil.copyfile(filename, target)
        records.append({"side": side, "text": text, "direction": DIRECTIONS[side],
                        "file": target.name, "sha256": hashlib.sha256(target.read_bytes()).hexdigest()})
        (OUTPUT / "manifest.json").write_text(json.dumps({
            "status": "audition only; not integrated or listening-approved",
            "model": MODEL, "license": "https://huggingface.co/" + MODEL + "/blob/main/LICENSE",
            "source": "https://huggingface.co/spaces/Qwen/Qwen3-TTS",
            "synthetic": True, "clips": records,
        }, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(target, flush=True)
    mix_preview()


if __name__ == "__main__":
    main()
