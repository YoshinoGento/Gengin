"""Bake licensed stock voices offline. No synthesizer is shipped with the game."""
import argparse
import hashlib
import io
import json
import math
from pathlib import Path
import sys
import wave

ROOT = Path(__file__).resolve().parents[2]
SOURCE = Path(__file__).with_name("contest_cheers.json")
OUTPUT = ROOT / "project/Resources/farm/contest/audio/cheers"
HEADER = ROOT / "project/application/farm/data/FarmContestCheerAssets.h"


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--runtime", type=Path, default=ROOT / "generated/tools/voicevox_0_17_1/runtime")
    parser.add_argument("--python-package", type=Path, default=ROOT / "generated/tools/voicevox_0_17_1/python")
    args = parser.parse_args()
    sys.path.insert(0, str(args.python_package.resolve()))
    import numpy as np
    from voicevox_core.blocking import Onnxruntime, OpenJtalk, Synthesizer, VoiceModelFile

    data = json.loads(SOURCE.read_text(encoding="utf-8"))
    pairs = data["pairs"]
    assert data["schemaVersion"] == 2 and len(pairs) == 16, "Historical Nemo bake; use bake_local_cheers.py for schema3"
    assert len({p["id"] for p in pairs}) == len(pairs)
    OUTPUT.mkdir(parents=True, exist_ok=True)
    runtime = args.runtime.resolve()
    engine = Onnxruntime.load_once(filename=str(runtime / "onnxruntime/lib/voicevox_onnxruntime.dll"))
    dictionary = OpenJtalk(runtime / "dict/open_jtalk_dic_utf_8-1.11")
    model_paths = [runtime / "models/vvms" / name for name in sorted({v["model"] for v in data["voices"].values()})]
    records = []
    with Synthesizer(engine, dictionary, acceleration_mode="CPU", cpu_num_threads=4) as synth:
        for model_path in model_paths:
            with VoiceModelFile.open(model_path) as model:
                synth.load_voice_model(model)
        for pair in pairs:
            assert pair["id"].isascii() and pair["id"].isalnum()
            assert .30 <= pair["replyGap"] <= .45
            for side in ("L", "R"):
                assert len(pair[side]) == 2
                text = "".join(pair[side])
                voice = data["voices"][side]
                query = synth.create_audio_query(text, voice["styleId"])
                query.speed_scale = voice["speed"]
                query.pitch_scale = voice["pitch"]
                query.intonation_scale = voice["intonation"]
                query.pre_phoneme_length = .04
                query.post_phoneme_length = .06
                query.output_sampling_rate = 24000
                query.output_stereo = False
                # Preserve lexical accents; reduce recital pauses and sustain only the final voiced vowel.
                for phrase in query.accent_phrases:
                    pause = phrase.pause_mora
                    if "pauseSeconds" not in voice:
                        continue
                    if isinstance(pause, dict):
                        # Core 0.17.1 returns the optional pause as a dict, unlike its ordinary Mora values.
                        pause["vowel_length"] = min(pause["vowel_length"], voice["pauseSeconds"])
                    elif pause is not None:
                        pause.vowel_length = min(pause.vowel_length, voice["pauseSeconds"])
                assert query.accent_phrases and query.accent_phrases[-1].moras
                final_mora = query.accent_phrases[-1].moras[-1]
                if "tailStretch" in voice and final_mora.vowel in ("a", "i", "u", "e", "o"):
                    final_mora.vowel_length = min(.40, final_mora.vowel_length * voice["tailStretch"])
                wav = synth.synthesis(query, voice["styleId"], enable_interrogative_upspeak=False)
                with wave.open(io.BytesIO(wav), "rb") as source:
                    assert source.getnchannels() == 1 and source.getsampwidth() == 2 and source.getframerate() == 24000
                    samples = np.frombuffer(source.readframes(source.getnframes()), dtype="<i2").astype(np.float64) / 32768
                # Remove synthesis padding, preserving 20ms of both ends; avoid clipping initial consonants.
                voiced = np.flatnonzero(np.abs(samples) > .001)
                assert len(voiced) and np.all(np.isfinite(samples))
                samples = samples[max(0, voiced[0]-480):min(len(samples), voiced[-1]+481)]
                rms = float(np.sqrt(np.mean(samples*samples)))
                peak = float(np.max(np.abs(samples)))
                samples *= min(.14 / rms, .88 / peak)
                ramp = min(120, len(samples)//2)
                samples[:ramp] *= np.linspace(0, 1, ramp)
                samples[-ramp:] *= np.linspace(1, 0, ramp)
                angle = (voice["pan"] + 1) * math.pi / 4
                stereo = np.column_stack((samples*math.cos(angle), samples*math.sin(angle)))
                pcm = np.rint(stereo*32767).astype("<i2")
                duration = len(samples) / 24000
                assert .3 < duration < 4.5, (pair["id"], side, duration)
                filename = pair["id"] + "_" + side + ".wav"
                target = OUTPUT / filename
                with wave.open(str(target), "wb") as out:
                    out.setparams((2, 2, 24000, 0, "NONE", "not compressed"))
                    out.writeframes(pcm.tobytes())
                records.append({"pair":pair["id"], "side":side, "file":filename, "text":text,
                    "style_id":voice["styleId"], "pan":voice["pan"], "seconds":duration, "kana":query.kana,
                    "frames":len(samples), "sha256":hashlib.sha256(target.read_bytes()).hexdigest(),
                    "peak":float(np.max(np.abs(pcm.astype(np.float64))))/32768})
                print(f"{filename}: {duration:.3f}s", flush=True)
    manifest = {"generator":"tools/audio/generate_contest_cheers.py", "credit":data["credit"],
        "terms":data["terms"], "core_version":"0.17.1", "model_version":"0.17.1",
        "model_sha256":{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in model_paths},
        "source_sha256":hashlib.sha256(SOURCE.read_bytes()).hexdigest(), "voices":data["voices"], "clips":records}
    (OUTPUT / "manifest.json").write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    header = "#pragma once\n#include <array>\n#include <string_view>\n\nnamespace farm::cheerassets {\n"
    header += "// Generated by tools/audio/generate_contest_cheers.py; durations measured from the shipped PCM.\n"
    header += "struct Clip { const char* file; float seconds; };\n"
    header += "inline constexpr std::array<std::string_view,16> kPairNames{{\n"
    header += "".join(f'    "{p["id"]}",\n' for p in pairs) + "}};\n"
    header += "inline constexpr std::array<Clip,32> kClips{{\n"
    header += "".join(f'    {{"cheers/{r["file"]}", {r["seconds"]:.6f}f}},\n' for r in records) + "}};\n"
    header += "inline constexpr std::array<float,16> kReplyGaps{{\n"
    header += "".join(f'    {p["replyGap"]:.3f}f,\n' for p in pairs) + "}};\n"
    header += "inline constexpr unsigned kPcmBytes = " + str(sum(r["frames"]*4 for r in records)) + ";\n}\n"
    HEADER.write_text(header,encoding="utf-8")


if __name__ == "__main__":
    main()
