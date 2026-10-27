"""Original, deterministic short wooden/plucked judging cues. No external samples."""
import array
import hashlib
import json
import math
from pathlib import Path
import sys
import wave

RATE = 24000
SPECS = (
    ("assess", .22, ((0, 285),)),
    ("passed", .65, ((0, 440), (.14, 660))),
    ("failed", .65, ((0, 330), (.16, 262))),
)


def synthesize(duration, notes):
    values = []
    count = round(RATE * duration)
    for i in range(count):
        t = i / RATE
        value = 0.0
        for start, frequency in notes:
            age = t - start
            if age < 0:
                continue
            attack = min(1, age / .009)
            wave_value = math.sin(math.tau * frequency * age)
            wave_value += .22 * math.sin(math.tau * frequency * 2.76 * age)
            value += attack * math.exp(-age * 13) * wave_value
        edge = min(i / (RATE * .012), (count - 1 - i) / (RATE * .035), 1)
        values.append(value * (.5 - .5 * math.cos(math.pi * max(0, edge))))
    scale = .45 / max(abs(v) for v in values)
    pcm = array.array("h", (round(v * scale * 32767) for v in values))
    if sys.byteorder != "little":
        pcm.byteswap()
    return pcm.tobytes()


def main():
    output = Path(__file__).resolve().parents[2] / "project/Resources/farm/contest/audio"
    output.mkdir(parents=True, exist_ok=True)
    manifest = {"provenance": __doc__, "rate": RATE, "channels": 1, "bits": 16, "clips": []}
    for name, duration, notes in SPECS:
        pcm = synthesize(duration, notes)
        path = output / (name + ".wav")
        with wave.open(str(path), "wb") as writer:
            writer.setparams((1, 2, RATE, 0, "NONE", "not compressed"))
            writer.writeframes(pcm)
        manifest["clips"].append({"file": path.name, "frames": len(pcm)//2,
                                  "pcm_bytes": len(pcm), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="ascii")
    print("Generated contest cues:", sum(c["pcm_bytes"] for c in manifest["clips"]), "PCM bytes")


if __name__ == "__main__":
    main()
