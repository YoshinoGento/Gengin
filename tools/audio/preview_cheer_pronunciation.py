"""Compare the four user-reported readings, old then revised, without game SFX."""
import argparse
from pathlib import Path
import wave
import numpy as np

ROOT = Path(__file__).resolve().parents[2]
OLD = ROOT / "generated/submissions/clear_20261117_v3/LE3C_28_ヨシノ_ゲント_水路農業/Resources/farm/contest/audio/cheers"
NEW = ROOT / "project/Resources/farm/contest/audio/cheers"
OUTPUT = ROOT / "generated/codex_checks/cheer_pronunciation_preview"
CLIPS = ("Perfect_R", "CarrotBundle_L", "TurnipStar_R", "NutrientSoil_R")
RATE = 24000


def read(path):
    with wave.open(str(path), "rb") as wav:
        assert (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) == (2,2,RATE)
        return np.frombuffer(wav.readframes(wav.getnframes()), dtype="<i2").reshape(-1,2)


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--new-directory", default=str(NEW.relative_to(ROOT)))
    args = parser.parse_args()
    new = (ROOT / args.new_directory).resolve()
    assert new.is_relative_to(ROOT), "Workspace audio only"
    OUTPUT.mkdir(parents=True, exist_ok=True)
    all_cases = []
    for clip in CLIPS:
        comparison = np.concatenate((read(OLD/(clip+".wav")), np.zeros((int(.7*RATE),2),dtype="<i2"),
                                     read(new/(clip+".wav"))))
        all_cases.extend((comparison, np.zeros((RATE,2),dtype="<i2")))
        with wave.open(str(OUTPUT/(clip+"_before_after.wav")),"wb") as wav:
            wav.setparams((2,2,RATE,0,"NONE","not compressed")); wav.writeframes(comparison.tobytes())
    with wave.open(str(OUTPUT/"FourCorrections_before_after.wav"),"wb") as wav:
        wav.setparams((2,2,RATE,0,"NONE","not compressed")); wav.writeframes(np.concatenate(all_cases).tobytes())
    print(OUTPUT)


if __name__ == "__main__":
    main()
