"""Bake the approved two-line audition over existing, validated baseline speech assets."""
import hashlib
import json
from pathlib import Path
import wave

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "tools/audio/contest_cheers.json"
OUTPUT = ROOT / "project/Resources/farm/contest/audio/cheers"
HEADER = ROOT / "project/application/farm/data/FarmContestCheerAssets.h"


def main():
    data = json.loads(SOURCE.read_text(encoding="utf-8"))
    assert data["schemaVersion"] == 2, "Historical two-line bake; use publish_local_cheers.py for schema3"
    audition = data["audition"]
    directory = (ROOT / audition["referenceDirectory"]).resolve()
    assert directory.is_relative_to(ROOT)
    references = json.loads((directory / "manifest.json").read_text(encoding="utf-8"))["clips"]
    assert len(references) == 2 and [r["side"] for r in references] == ["L", "R"]
    manifest = json.loads((OUTPUT / "manifest.json").read_text(encoding="utf-8"))
    records = manifest["clips"]
    expected = [(p,s) for p in data["pairs"] for s in ("L","R")]
    assert len(records) == len(expected) == 32
    updates = []
    for (pair, side), record in zip(expected, records):
        assert record["pair"] == pair["id"] and record["side"] == side
        assert record["file"] == pair["id"] + "_" + side + ".wav"
        assert record["text"] == "".join(pair[side])
        assert hashlib.sha256((OUTPUT / record["file"]).read_bytes()).hexdigest() == record["sha256"]
        if pair["id"] != audition["pair"]:
            assert record["style_id"] == data["voices"][side]["styleId"]
            continue
        ref = references[0 if side == "L" else 1]
        raw = (directory / ref["file"]).resolve()
        assert raw.is_relative_to(directory) and ref["text"] == record["text"]
        assert hashlib.sha256(raw.read_bytes()).hexdigest() == ref["sha256"]
        with wave.open(str(raw), "rb") as wav:
            assert (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) == (1,2,24000)
            samples = np.frombuffer(wav.readframes(wav.getnframes()), dtype="<i2").astype(np.float64)/32768
        voiced = np.flatnonzero(np.abs(samples)>.001)
        assert len(voiced) and np.isfinite(samples).all()
        samples = samples[max(0,voiced[0]-480):min(len(samples),voiced[-1]+481)]
        assert .3 < len(samples)/24000 < 4.5
        samples *= min(.14/np.sqrt(np.mean(samples*samples)), .88/np.max(np.abs(samples)))
        samples[:120] *= np.linspace(0,1,120)
        samples[-120:] *= np.linspace(1,0,120)
        angle = (record["pan"]+1)*np.pi/4
        pcm = np.rint(np.column_stack((samples*np.cos(angle), samples*np.sin(angle)))*32767).astype("<i2")
        replacement = {k:record[k] for k in ("pair","side","file","text","pan")}
        replacement.update(model=audition["model"], reference_sha256=ref["sha256"],
                           frames=len(samples), seconds=len(samples)/24000,
                           peak=float(np.max(np.abs(pcm.astype(np.float64))))/32768)
        updates.append((records.index(record),replacement,pcm))
    assert len(updates)==2
    for index, record, pcm in updates:
        target = OUTPUT / record["file"]
        with wave.open(str(target),"wb") as wav:
            wav.setparams((2,2,24000,0,"NONE","not compressed"))
            wav.writeframes(pcm.tobytes())
        record["sha256"] = hashlib.sha256(target.read_bytes()).hexdigest()
        records[index] = record
    manifest.update(credit=data["credit"], audition=audition, voices=data["voices"],
                    source_sha256=hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
                    provenance="PumpkinPot two-line user-approved synthetic audition; other30 original Nemo takes retained.",
                    generator="tools/audio/generate_contest_cheers.py then tools/audio/apply_cheer_audition.py")
    (OUTPUT / "manifest.json").write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    header = "#pragma once\n#include <array>\n#include <string_view>\n\nnamespace farm::cheerassets {\n"
    header += "// Generated speech metadata; durations measured from the shipped PCM.\n"
    header += "struct Clip { const char* file; float seconds; };\n"
    header += "inline constexpr std::array<std::string_view,16> kPairNames{{\n"
    header += "".join(f'    "{p["id"]}",\n' for p in data["pairs"]) + "}};\n"
    header += "inline constexpr std::array<Clip,32> kClips{{\n"
    header += "".join(f'    {{"cheers/{r["file"]}", {r["seconds"]:.6f}f}},\n' for r in records) + "}};\n"
    header += "inline constexpr std::array<float,16> kReplyGaps{{\n"
    header += "".join(f'    {p["replyGap"]:.3f}f,\n' for p in data["pairs"]) + "}};\n"
    header += "inline constexpr unsigned kPcmBytes = " + str(sum(r["frames"]*4 for r in records)) + ";\n}\n"
    HEADER.write_text(header,encoding="utf-8")
    print("Baked two approved takes; remaining30 PCM files preserved")


if __name__ == "__main__":
    main()
