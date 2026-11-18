"""Original deterministic judging, snare-roll and celebration cues. No external samples."""
import array
import hashlib
import json
import math
import random
from pathlib import Path
import sys
import wave

RATE = 24000
SPECS = (
    ("assess", .22, ((0, 285),)),
    ("passed", .65, ((0, 440), (.14, 660))),
    ("failed", .65, ((0, 330), (.16, 262))),
    ("clear", 4.6, ((.12, 392), (.30, 494), (.48, 587), (.76, 784), (1.12, 587),
                    (1.42, 784), (1.42, 988), (1.42, 1175))),
    ("drumroll", 1.0, ()),
)


def synthesize(duration, notes, celebration=False):
    values = []
    rng = random.Random(20261117)
    claps = tuple(.9 + i*.088 + rng.uniform(-.026, .026) for i in range(34))
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
            if celebration:
                value += .20 * attack * math.exp(-age * 3.8) * (
                    math.sin(math.tau * frequency * age) + .25 * math.sin(math.tau * frequency * 2 * age))
        if celebration:
            # Bounded noise transients suggest a pop and scattered claps; no sampled recording.
            envelope = .8 * math.exp(-t*70)
            for clap in claps:
                age = t-clap
                if 0 <= age < .13:
                    envelope += .16 * math.exp(-age*45)
            value += rng.uniform(-1, 1)*envelope
        edge = min(i / (RATE * .012), (count - 1 - i) / (RATE * .035), 1)
        values.append(value * (.5 - .5 * math.cos(math.pi * max(0, edge))))
    return encode(values)


def encode(values):
    scale = .45 / max(abs(v) for v in values)
    pcm = array.array("h", (round(v * scale * 32767) for v in values))
    if sys.byteorder != "little":
        pcm.byteswap()
    return pcm.tobytes()


def drumroll(duration):
    rng = random.Random(20261118)
    taps, start = [], 0.0
    while start < duration - .025:
        progress = start / duration
        taps.append((start, .16 + .84 * progress**1.4))
        start += .065 - .042 * progress
    values, low_noise = [], 0.0
    count = round(RATE * duration)
    for i in range(count):
        t = i / RATE
        noise = rng.uniform(-1, 1)
        low_noise += .12 * (noise - low_noise)
        value = 0.0
        for start, strength in taps:
            age = t - start
            if 0 <= age < .14:
                attack = min(1, age / .0015)
                snare = (noise - low_noise) * math.exp(-age * 38)
                body = .30 * math.sin(math.tau * 180 * age) * math.exp(-age * 65)
                value += strength * attack * (snare + body)
        edge = min(i / (RATE * .006), (count - 1 - i) / (RATE * .012), 1)
        values.append(value * max(0, edge))
    return encode(values)


def main():
    output = Path(__file__).resolve().parents[2] / "project/Resources/farm/contest/audio"
    output.mkdir(parents=True, exist_ok=True)
    manifest = {"provenance": __doc__, "rate": RATE, "channels": 1, "bits": 16, "clips": []}
    for name, duration, notes in SPECS:
        pcm = drumroll(duration) if name == "drumroll" else synthesize(duration, notes, name == "clear")
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
