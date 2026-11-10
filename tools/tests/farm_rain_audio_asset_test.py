import array
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import wave

root = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("rain_generator", root / "tools/audio/generate_rain_audio.py")
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)
path = generator.OUTPUT / "rain.wav"
manifest = json.loads((generator.OUTPUT / "manifest.json").read_text(encoding="utf-8"))
assert manifest["external_samples"] is False
assert manifest["sha256"] == hashlib.sha256(path.read_bytes()).hexdigest()
with wave.open(str(path), "rb") as audio:
    assert (audio.getnchannels(), audio.getsampwidth(), audio.getframerate(), audio.getnframes()) == (1, 2, 24000, 144000)
    pcm = audio.readframes(audio.getnframes())
assert pcm == generator.samples().tobytes()
samples = array.array("h", pcm)
rms = lambda data: math.sqrt(sum((value/32768)**2 for value in data)/len(data))
levels = [rms(samples[i:i+24000]) for i in range(0,len(samples),24000)]
assert min(levels) > .08 and max(levels)/min(levels) < 1.5
assert max(abs(value) for value in samples) < 32760
seam = abs(samples[0]-samples[-1])/32768
assert seam < .10, seam
assert rms(samples[:2400]) > .08 and rms(samples[-2400:]) > .08
print(f"PASS: rain deterministic PCM, provenance, hash, format, headroom, stable RMS and loop seam ({seam:.4f})")
