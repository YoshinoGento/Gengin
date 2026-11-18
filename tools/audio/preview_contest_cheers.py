"""Render a listening comparison, not a claim of perceptual acceptance."""
import json
from pathlib import Path
import wave

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
OLD = ROOT / "generated/exhibitions/contest_cheer_voices_20261117/Resources/farm/contest/audio/cheers"
NEW = ROOT / "project/Resources/farm/contest/audio/cheers"
OUTPUT = ROOT / "generated/codex_checks/cheer_current_preview"
RATE = 24000


def read(directory, filename):
    with wave.open(str(directory / filename), "rb") as wav:
        assert (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) == (2, 2, RATE)
        return np.frombuffer(wav.readframes(wav.getnframes()), dtype="<i2").reshape(-1, 2).astype(np.float64) / 32768


def save(name, samples):
    assert np.max(np.abs(samples)) < 1
    with wave.open(str(OUTPUT / name), "wb") as wav:
        wav.setparams((2, 2, RATE, 0, "NONE", "not compressed"))
        wav.writeframes(np.rint(samples*32767).astype("<i2").tobytes())


def exchange(directory, pair, gap):
    return np.concatenate((read(directory, pair+"_L.wav"), np.zeros((round(gap*RATE), 2)), read(directory, pair+"_R.wav"))) * .60


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    data = json.loads((ROOT / "tools/audio/contest_cheers.json").read_text(encoding="utf-8"))
    old = json.loads((OLD / "manifest.json").read_text(encoding="utf-8"))
    for pair in (p for p in data["pairs"] if p["id"] in ("PumpkinPot", "LargeDelivery")):
        name = pair["id"]
        old_length = next(c["seconds"] for c in old["clips"] if c["pair"] == name and c["side"] == "L")
        before = exchange(OLD, name, 4.001792-1.7-old_length)
        after = exchange(NEW, name, pair["replyGap"])
        silence = np.zeros((round(.5*RATE), 2))
        save(name+"_new.wav", np.concatenate((silence, after, silence)))
        save(name+"_before_after.wav", np.concatenate((silence, before, np.zeros((RATE,2)), after, silence)))
        print(f"{name}: old response gap={4.001792-1.7-old_length:.3f}s, new={pair['replyGap']:.2f}s")


if __name__ == "__main__":
    main()
