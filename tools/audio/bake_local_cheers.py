"""Offline synthesis from the two approved synthetic spectators, never a real person's voice."""
import argparse
import hashlib
import gc
import json
import os
from pathlib import Path
import shutil
import time
import wave
from cheer_speech_plan import speech_plan

ROOT = Path(__file__).resolve().parents[2]
MODEL = ROOT / "generated/tools/qwen_local_model"
OUTPUT = ROOT / "generated/audio/local_contest_cheers"
REVISION = "fd4b254389122332181a7c3db7f27e918eec64e3"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--regenerate-changed", action="store_true")
    parser.add_argument("--device", choices=("cuda:0", "cpu"), default="cuda:0")
    parser.add_argument("--max-generated", type=int, choices=range(1,33))
    args = parser.parse_args()
    os.environ["HF_HUB_OFFLINE"] = "1"
    os.environ["HF_HUB_DISABLE_IMPLICIT_TOKEN"] = "1"
    os.environ["HF_HUB_DISABLE_TELEMETRY"] = "1"
    os.environ["HF_HOME"] = str(ROOT / "generated/tools/qwen_local_cache")
    import numpy as np
    import soundfile as sf
    import torch
    from qwen_tts import Qwen3TTSModel

    data = json.loads((ROOT / "tools/audio/contest_cheers.json").read_text(encoding="utf-8"))
    reference = ROOT / data["audition"]["referenceDirectory"]
    refs = json.loads((reference / "manifest.json").read_text(encoding="utf-8"))["clips"]
    assert len(refs) == 2 and [r["side"] for r in refs] == ["L", "R"]
    use_cuda = args.device.startswith("cuda")
    assert not use_cuda or torch.cuda.is_available(), "CUDA is unavailable"
    torch.set_num_threads(4)
    OUTPUT.mkdir(parents=True, exist_ok=True)
    model, prompts, generated = None, {}, 0
    for index, pair in enumerate(data["pairs"]):
        for side, ref in zip(("L", "R"), refs):
            assert pair["id"].isascii() and pair["id"].isalnum()
            raw = reference / ref["file"]
            assert digest(raw) == ref["sha256"]
            text = "".join(pair[side])
            name = pair["id"] + "_" + side
            target, metadata = OUTPUT / (name + ".wav"), OUTPUT / (name + ".json")
            plan = speech_plan(pair, side)
            has_override = side in pair.get("speech", {})
            use_reference = text == ref["text"] and not has_override
            request = dict(pair=pair["id"], side=side, text=text, reference_sha256=ref["sha256"],
                           model=data["audition"]["model"] if use_reference else "Qwen/Qwen3-TTS-12Hz-1.7B-Base",
                           revision=REVISION, seed=17000 + index * 2 + (side == "R"))
            if has_override:
                request["speech"] = plan
            if metadata.exists() and target.exists():
                old = json.loads(metadata.read_text(encoding="utf-8"))
                assert old["sha256"] == digest(target), name
                if old["request"] == request:
                    print("Cached " + name, flush=True)
                    continue
                assert args.regenerate_changed, "Changed speech input: use --regenerate-changed after review"
                backup = OUTPUT / "history" / name / old["sha256"]
                backup.mkdir(parents=True, exist_ok=True)
                for original in (target, metadata):
                    saved = backup / original.name
                    if saved.exists():
                        assert digest(saved) == digest(original), "History collision"
                    else:
                        shutil.copyfile(original, saved)
            # WDDM may back reserved CUDA blocks with pagefile space. Release unused cache between takes.
            if model is not None:
                gc.collect()
                if use_cuda:
                    torch.cuda.empty_cache()
            free = shutil.disk_usage(ROOT).free
            print(f"Free disk: {free/1024**3:.2f}GiB",flush=True)
            assert free > 2 * 1024**3, "Reserve 2GB of free disk"
            started = time.monotonic()
            if use_reference:
                shutil.copyfile(raw, target)
            else:
                if model is None:
                    print("Loading approved-style Base model on " + args.device, flush=True)
                    model = Qwen3TTSModel.from_pretrained(str(MODEL), device_map=args.device, low_cpu_mem_usage=True,
                                                        dtype=torch.bfloat16, attn_implementation="sdpa")
                    assert shutil.disk_usage(ROOT).free > 2 * 1024**3, "Reserve 2GB after model load"
                    for r in refs:
                        prompts[r["side"]] = model.create_voice_clone_prompt(
                            ref_audio=str(reference / r["file"]), ref_text=r["text"], x_vector_only_mode=False)
                print("Generating " + name + ": " + text, flush=True)
                chunks, pauses = [], []
                for part, spoken in enumerate(plan["segments"]):
                    torch.manual_seed(request["seed"] + part * 1000)
                    with torch.inference_mode():
                        samples, rate = model.generate_voice_clone(text=spoken, language="Japanese",
                            voice_clone_prompt=prompts[side], max_new_tokens=96)
                    assert rate == 24000 and len(samples) == 1 and np.isfinite(samples[0]).all()
                    chunk = samples[0]
                    if len(plan["segments"]) > 1:
                        active = np.flatnonzero(np.abs(chunk) > .001)
                        assert len(active), "Empty spoken segment"
                        chunk = chunk[max(0,active[0]-240):min(len(chunk),active[-1]+241)].copy()
                        chunk[:120] *= np.linspace(0,1,120)
                        chunk[-120:] *= np.linspace(1,0,120)
                    if chunks:
                        start = sum(len(c) for c in chunks)
                        gap = int(round(plan["pauseSeconds"] * rate))
                        pauses.append(dict(startFrame=start, frames=gap))
                        chunks.append(np.zeros(gap, dtype=np.float32))
                    chunks.append(chunk)
                sf.write(str(target), np.concatenate(chunks), rate, subtype="PCM_16")
            with wave.open(str(target), "rb") as wav:
                assert (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) == (1, 2, 24000)
                seconds = wav.getnframes() / 24000
                assert .3 < seconds < 7.5, (name, seconds)
            result = dict(request=request, sha256=digest(target), seconds=seconds)
            if "speech" in request:
                result["pauses"] = pauses
            metadata.write_text(json.dumps(result,
                                          ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
            print(f"Saved {name}: {seconds:.3f}s ({time.monotonic()-started:.1f}s)", flush=True)
            if use_cuda:
                print(f"CUDA allocated={torch.cuda.memory_allocated()/1024**3:.2f}GiB reserved={torch.cuda.memory_reserved()/1024**3:.2f}GiB",flush=True)
            generated += 1
            if args.max_generated and generated >= args.max_generated:
                print(f"CHECKPOINT: {generated} take(s) generated. Game assets not modified.", flush=True)
                return
    print("COMPLETE: 32 local raw takes. Game assets not modified.", flush=True)


if __name__ == "__main__":
    main()
