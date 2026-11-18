"""Verify the baked speech/subtitle contract without installing a synthesizer."""
import hashlib
import json
from pathlib import Path
import re
import wave
import sys

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/audio"))
from cheer_speech_plan import speech_plan
SOURCE = ROOT / "tools/audio/contest_cheers.json"
DIRECTORY = ROOT / "project/Resources/farm/contest/audio/cheers"


def main():
    data = json.loads(SOURCE.read_text(encoding="utf-8"))
    manifest = json.loads((DIRECTORY / "manifest.json").read_text(encoding="utf-8"))
    assert manifest["source_sha256"] == hashlib.sha256(SOURCE.read_bytes()).hexdigest()
    assert data["credit"] == manifest["credit"] == "Qwen3-TTS"
    assert data["schemaVersion"] == 3 and data["terms"] == manifest["terms"]
    assert data["voices"] == manifest["voices"]
    assert data["voices"]["L"]["reference"] != data["voices"]["R"]["reference"]
    assert data["audition"] == manifest["audition"] and data["audition"]["pair"] == "PumpkinPot"
    reference_dir = ROOT / data["audition"]["referenceDirectory"]
    references = json.loads((reference_dir / "manifest.json").read_text(encoding="utf-8"))["clips"]
    assert len(references)==2 and references[0]["sha256"] != references[1]["sha256"]
    pairs = data["pairs"]
    assert len(pairs) == 16 and len({p["id"] for p in pairs}) == 16
    system = (ROOT / "project/application/farm/system/FarmContestCheerSystem.h").read_text(encoding="utf-8")
    enum = re.search(r"enum class Pair\s*\{(.*?)Count,", system, re.S).group(1)
    assert re.findall(r"\w+", enum) == [p["id"] for p in pairs]
    header = (ROOT / "project/application/farm/data/FarmContestCheerAssets.h").read_text(encoding="utf-8")
    assert "FarmContestSpeech:sha256:" + hashlib.sha256((DIRECTORY/"manifest.json").read_bytes()).hexdigest() in header
    assert float(re.search(r"kMaximumVoiceSeconds = ([0-9.]+)f",header).group(1)) == data["maximumVoiceSeconds"]
    clips = re.findall(r'\{"cheers/([A-Za-z0-9_]+\.wav)", ([0-9.]+)f\}', header)
    expected = [(p, side) for p in pairs for side in ("L", "R")]
    gaps = re.search(r"kReplyGaps\{\{(.*?)\}\};", header, re.S).group(1)
    assert [float(g) for g in re.findall(r"([0-9.]+)f", gaps)] == [p["replyGap"] for p in pairs]
    assert all(.30 <= p["replyGap"] <= .45 for p in pairs)
    assert len(manifest["clips"]) == len(clips) == len(expected) == 32
    pcm_bytes = 0
    for (pair, side), record, (filename, duration) in zip(expected, manifest["clips"], clips):
        assert record["pair"] == pair["id"] and record["side"] == side
        assert filename == record["file"] == pair["id"] + "_" + side + ".wav"
        assert record["text"] == "".join(pair[side])
        plan = speech_plan(pair,side)
        assert record.get("speech", {"segments":[record["text"]],"pauseSeconds":0}) == plan
        ref = references[0 if side=="L" else 1]
        assert ref["sha256"] == record["reference_sha256"]
        assert hashlib.sha256((reference_dir/ref["file"]).read_bytes()).hexdigest() == ref["sha256"]
        assert data["voices"][side]["reference"] == ref["file"]
        if pair["id"] == data["audition"]["pair"]:
            assert ref["text"] == record["text"]
            assert record["model"] == data["audition"]["model"]
        else:
            assert record["model"] == "Qwen/Qwen3-TTS-12Hz-1.7B-Base"
        raw_dir = ROOT / "generated/audio/local_contest_cheers"
        if raw_dir.exists():
            raw = raw_dir / filename
            assert hashlib.sha256(raw.read_bytes()).hexdigest() == record["raw_sha256"]
        assert record["pan"] == data["voices"][side]["pan"]
        # The contract checks generation text, not perceptual pronunciation accuracy.
        path = DIRECTORY / filename
        assert record["sha256"] == hashlib.sha256(path.read_bytes()).hexdigest()
        with wave.open(str(path), "rb") as wav:
            assert (wav.getnchannels(), wav.getsampwidth(), wav.getframerate(), wav.getcomptype()) == (2, 2, 24000, "NONE")
            frames = wav.getnframes()
            assert frames == record["frames"]
            assert abs(frames / 24000 - float(duration)) < .000001
            assert abs(frames / 24000 - record["seconds"]) < .000001
            pcm = wav.readframes(frames)
        samples = np.frombuffer(pcm, dtype="<i2").astype(np.float64).reshape(-1, 2) / 32768
        pauses = record.get("pauses", [])
        assert len(pauses) == len(plan["segments"])-1
        for pause in pauses:
            start, count = pause["startFrame"], pause["frames"]
            assert start >= 0 and count == round(plan["pauseSeconds"]*24000) and start+count <= frames
            assert np.all(samples[start:start+count] == 0), "Missing performed pause"
        assert .3 < frames / 24000 < data["maximumVoiceSeconds"]
        assert np.all(np.isfinite(samples)) and .03 < np.max(np.abs(samples)) < .9
        assert np.max(np.abs(samples[[0, -1]])) == 0
        energy = np.mean(samples * samples, axis=0)
        assert energy[0 if side == "L" else 1] > energy[1 if side == "L" else 0] * 2
        assert np.sqrt(np.mean(np.mean(samples, axis=1)**2)) > .01
        pcm_bytes += len(pcm)
    assert pcm_bytes == int(re.search(r"kPcmBytes = (\d+)", header).group(1))
    assert len(list(DIRECTORY.glob("*.wav"))) == 32
    credits = (DIRECTORY / "CREDITS.txt").read_text(encoding="utf-8")
    assert "Qwen3-TTS" in credits and "VoiceDesign" in credits and "Base" in credits
    print(f"PASS:32 speech assets,distinct speakers,text/enum/header/hash/duration/PCM/pan/fade/mono/credits; PCM={pcm_bytes} bytes")


if __name__ == "__main__":
    main()
