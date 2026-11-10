import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import wave

root = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("pest_audio", root / "tools/audio/generate_pest_audio.py")
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)
directory = root / "project/Resources/farm/pests/audio"
manifest = json.loads((directory / "manifest.json").read_text())
path = directory / manifest["file"]
assert hashlib.sha256(path.read_bytes()).hexdigest() == manifest["sha256"]
with wave.open(str(path), "rb") as reader:
    assert (reader.getnchannels(), reader.getsampwidth(), reader.getframerate(), reader.getnframes()) == (1, 2, 24000, 8160)
    pcm = reader.readframes(reader.getnframes())
assert pcm == generator.synthesize()
samples = struct.unpack("<8160h", pcm)
assert samples[0] == 0 and samples[-1] == 0
assert 0 < max(abs(x) for x in samples) < 14000
assert max(abs(b-a) for a, b in zip(samples, samples[1:])) < 6000
print("PASS: deterministic original pest audio, 0.34s mono PCM, nonclipping faded endpoints, manifest hash")
