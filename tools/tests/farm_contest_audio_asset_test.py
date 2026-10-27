"""PCM verification, not a subjective listening test."""
import array
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import wave

root = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("contest_synth", root / "tools/audio/generate_contest_audio.py")
synth = importlib.util.module_from_spec(spec)
spec.loader.exec_module(synth)
directory = root / "project/Resources/farm/contest/audio"
manifest = json.loads((directory / "manifest.json").read_text())
total = 0
for entry, (name, duration, notes) in zip(manifest["clips"], synth.SPECS, strict=True):
    path = directory / entry["file"]
    assert path.stem == name
    assert hashlib.sha256(path.read_bytes()).hexdigest() == entry["sha256"]
    with wave.open(str(path), "rb") as source:
        assert (source.getnchannels(), source.getsampwidth(), source.getframerate()) == (1, 2, synth.RATE)
        pcm = source.readframes(source.getnframes())
    assert pcm == synth.synthesize(duration, notes)
    samples = array.array("h", pcm)
    if sys.byteorder != "little":
        samples.byteswap()
    assert samples[0] == samples[-1] == 0
    assert .44 < max(abs(x) for x in samples) / 32767 < .46
    assert abs(sum(samples) / len(samples)) < 30
    assert len(pcm) == entry["pcm_bytes"]
    total += len(pcm)
assert total == 72960
print("PASS: three deterministic mono cues, peak/DC/edges/hash/duration; 72960 PCM bytes")
