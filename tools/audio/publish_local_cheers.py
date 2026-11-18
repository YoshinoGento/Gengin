"""Validate every local take before publishing PCM, provenance and measured runtime timing."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import wave

import numpy as np
from cheer_speech_plan import speech_plan

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "tools/audio/contest_cheers.json"
RAW = ROOT / "generated/audio/local_contest_cheers"
STAGING = ROOT / "generated/audio/processed_contest_cheers"
OUTPUT = ROOT / "project/Resources/farm/contest/audio/cheers"
HEADER = ROOT / "project/application/farm/data/FarmContestCheerAssets.h"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--stage-only", action="store_true")
    args = parser.parse_args()
    data = json.loads(SOURCE.read_text(encoding="utf-8"))
    assert data["schemaVersion"] == 3 and len(data["pairs"]) == 16
    references = ROOT / data["audition"]["referenceDirectory"]
    refs = json.loads((references / "manifest.json").read_text(encoding="utf-8"))["clips"]
    prepared = []
    for pair in data["pairs"]:
        for side, ref in zip(("L", "R"), refs):
            name = pair["id"] + "_" + side
            raw = RAW / (name + ".wav")
            meta = json.loads((RAW / (name + ".json")).read_text(encoding="utf-8"))
            req = meta["request"]
            assert req["pair"] == pair["id"] and req["side"] == side and req["text"] == "".join(pair[side])
            plan = speech_plan(pair, side)
            assert req.get("speech", {"segments":[req["text"]],"pauseSeconds":0}) == plan
            assert req["reference_sha256"] == ref["sha256"] == digest(references / ref["file"])
            assert data["voices"][side]["reference"] == ref["file"]
            assert meta["sha256"] == digest(raw)
            with wave.open(str(raw), "rb") as wav:
                assert (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) == (1,2,24000)
                samples = np.frombuffer(wav.readframes(wav.getnframes()), dtype="<i2").astype(np.float64)/32768
            voiced = np.flatnonzero(np.abs(samples) > .001)
            assert len(voiced) and np.isfinite(samples).all()
            pauses = meta.get("pauses", [])
            assert len(pauses) == len(plan["segments"])-1
            for pause in pauses:
                start, frames = pause["startFrame"], pause["frames"]
                assert frames == round(plan["pauseSeconds"]*24000) and start >= 0 and start+frames <= len(samples)
                assert np.all(samples[start:start+frames] == 0), "Authored silence missing"
            # Preserve the performed cadence; trim edge silence only, never time-stretch the shout.
            trim = max(0,voiced[0]-480)
            samples = samples[trim:min(len(samples),voiced[-1]+481)]
            assert .3 < len(samples)/24000 < data["maximumVoiceSeconds"], (name,len(samples)/24000)
            samples *= min(.14/np.sqrt(np.mean(samples*samples)), .88/np.max(np.abs(samples)))
            samples[:120] *= np.linspace(0,1,120)
            samples[-120:] *= np.linspace(1,0,120)
            pan = data["voices"][side]["pan"]
            angle = (pan+1)*np.pi/4
            pcm = np.rint(np.column_stack((samples*np.cos(angle),samples*np.sin(angle)))*32767).astype("<i2")
            record = dict(pair=pair["id"],side=side,file=name+".wav",text=req["text"],pan=pan,
                          model=req["model"],reference_sha256=req["reference_sha256"],raw_sha256=meta["sha256"],
                          revision=req["revision"],seed=req["seed"],frames=len(samples),seconds=len(samples)/24000,
                          peak=float(np.max(np.abs(pcm.astype(np.float64))))/32768)
            if "speech" in req:
                record["speech"] = plan
                record["pauses"] = [dict(startFrame=p["startFrame"]-trim, frames=p["frames"]) for p in pauses]
            prepared.append((record,pcm))
    assert len(prepared)==32 and shutil.disk_usage(ROOT).free > 1024**3
    STAGING.mkdir(parents=True,exist_ok=True)
    records = []
    for record, pcm in prepared:
        path = STAGING / record["file"]
        with wave.open(str(path),"wb") as wav:
            wav.setparams((2,2,24000,0,"NONE","not compressed")); wav.writeframes(pcm.tobytes())
        record["sha256"] = digest(path); records.append(record)
    manifest = dict(generator="tools/audio/bake_local_cheers.py then tools/audio/publish_local_cheers.py",
                    credit=data["credit"],terms=data["terms"],source_sha256=digest(SOURCE),voices=data["voices"],
                    audition=data["audition"],maximumVoiceSeconds=data["maximumVoiceSeconds"],clips=records,
                    provenance="Approved synthetic VoiceDesign pair retained; other30 Base local voice-reference takes. No real-person recordings.",
                    listening_status="Two-line direction approved. Pronunciation overrides and authored pauses included; revised-take listening pending.")
    (STAGING / "manifest.json").write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    header = '#pragma once\n#include <array>\n#include <string_view>\n\nnamespace farm::cheerassets {\n'
    header += '// Generated speech metadata; durations measured from the shipped PCM.\n'
    header += f'inline constexpr char kAssetContract[] = "FarmContestSpeech:sha256:{digest(STAGING/"manifest.json")}";\n'
    header += 'struct Clip { const char* file; float seconds; };\n'
    header += f'inline constexpr float kMaximumVoiceSeconds = {data["maximumVoiceSeconds"]:.1f}f;\n'
    header += 'inline constexpr std::array<std::string_view,16> kPairNames{{\n'
    header += ''.join(f'    "{p["id"]}",\n' for p in data["pairs"]) + '}};\n'
    header += 'inline constexpr std::array<Clip,32> kClips{{\n'
    header += ''.join(f'    {{"cheers/{r["file"]}", {r["seconds"]:.6f}f}},\n' for r in records) + '}};\n'
    header += 'inline constexpr std::array<float,16> kReplyGaps{{\n'
    header += ''.join(f'    {p["replyGap"]:.3f}f,\n' for p in data["pairs"]) + '}};\n'
    header += 'inline constexpr unsigned kPcmBytes = ' + str(sum(r["frames"]*4 for r in records)) + ';\n}\n'
    if args.stage_only:
        print(f'Staged32 clips, max={max(r["seconds"] for r in records):.3f}s; game assets unchanged')
        return
    for path in STAGING.iterdir():
        if path.suffix in (".wav",".json"): shutil.copyfile(path,OUTPUT/path.name)
    HEADER.write_text(header,encoding="utf-8")
    print(f'Published32 measured clips, max={max(r["seconds"] for r in records):.3f}s')


if __name__ == "__main__":
    main()
