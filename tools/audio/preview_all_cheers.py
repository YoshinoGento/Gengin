"""Render the exact shipped exchange cadence for listening, without celebration SFX."""
import argparse
import json
from pathlib import Path
import wave
import numpy as np

ROOT = Path(__file__).resolve().parents[2]
RATE = 24000
SOURCE = ROOT / "project/Resources/farm/contest/audio/cheers"
OUTPUT = ROOT / "generated/codex_checks/cheer_complete_preview"


def read(name):
    with wave.open(str(SOURCE / name),"rb") as wav:
        assert (wav.getnchannels(),wav.getsampwidth(),wav.getframerate()) == (2,2,RATE)
        return np.frombuffer(wav.readframes(wav.getnframes()),dtype="<i2").reshape(-1,2).astype(np.float64)/32768


def save(path,samples):
    assert np.isfinite(samples).all() and np.max(np.abs(samples)) < 1
    with wave.open(str(path),"wb") as wav:
        wav.setparams((2,2,RATE,0,"NONE","not compressed"))
        wav.writeframes(np.rint(samples*32767).astype("<i2").tobytes())


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--output-directory", default=str(OUTPUT.relative_to(ROOT)))
    args = parser.parse_args()
    output = (ROOT / args.output_directory).resolve()
    assert output.is_relative_to(ROOT), "Workspace output only"
    data = json.loads((ROOT/"tools/audio/contest_cheers.json").read_text(encoding="utf-8"))
    output.mkdir(parents=True,exist_ok=True)
    combined, index = [], []
    for number,pair in enumerate(data["pairs"],1):
        name=pair["id"]
        exchange=np.concatenate((read(name+"_L.wav"),np.zeros((round(pair["replyGap"]*RATE),2)),read(name+"_R.wav")))*.60
        save(output/f"{number:02}_{name}.wav",np.concatenate((np.zeros((RATE//2,2)),exchange,np.zeros((RATE//2,2)))))
        combined.extend((exchange,np.zeros((RATE,2))))
        index.append(f'{number:02} {name}: L {"".join(pair["L"])} / R {"".join(pair["R"])}')
    save(output/"All16.wav",np.concatenate(combined))
    (output/"index.txt").write_text("\n".join(index)+"\n",encoding="utf-8")
    print(output)


if __name__ == "__main__": main()
