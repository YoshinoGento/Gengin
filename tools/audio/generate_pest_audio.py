"""Original synthetic net swish and soft confirmation pluck. No external samples."""
import array
import hashlib
import json
import math
from pathlib import Path
import random
import sys
import wave

RATE = 24000
DURATION = .34


def synthesize():
    rng = random.Random(731)
    values = []
    filtered = 0.0
    count = round(RATE * DURATION)
    for i in range(count):
        t = i / RATE
        filtered += .12 * (rng.uniform(-1, 1) - filtered)
        swish = filtered * math.sin(math.pi * min(t / .18, 1)) ** 2
        age = max(0, t - .12)
        note = math.sin(math.tau * 740 * age) * min(age / .009, 1) * math.exp(-age * 23)
        edge = min(i / (RATE * .01), (count - 1 - i) / (RATE * .04), 1)
        values.append((swish * .8 + note * .13) * max(edge, 0))
    scale = .4 / max(abs(value) for value in values)
    pcm = array.array("h", (round(value * scale * 32767) for value in values))
    if sys.byteorder != "little":
        pcm.byteswap()
    return pcm.tobytes()


def main():
    output = Path(__file__).resolve().parents[2] / "project/Resources/farm/pests/audio"
    output.mkdir(parents=True, exist_ok=True)
    path = output / "cured.wav"
    with wave.open(str(path), "wb") as writer:
        writer.setparams((1, 2, RATE, 0, "NONE", "not compressed"))
        writer.writeframes(synthesize())
    (output / "manifest.json").write_text(json.dumps({"provenance": __doc__, "rate": RATE,
        "channels": 1, "bits": 16, "seconds": DURATION, "file": path.name,
        "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}, indent=2) + "\n", encoding="ascii")
    print("Generated:", path, path.stat().st_size, "bytes")


if __name__ == "__main__":
    main()
