"""Read-only local ASR audit. Transcript differences are review candidates, not verdicts."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import unicodedata
import wave

ROOT = Path(__file__).resolve().parents[2]
MODEL_ID = "openai/whisper-small"
MODEL = ROOT / "generated/tools/whisper_small_audit"
OUTPUT = ROOT / "generated/codex_checks/cheer_pronunciation"
FILES = {
    "config.json", "generation_config.json", "preprocessor_config.json", "tokenizer_config.json",
    "tokenizer.json", "vocab.json", "merges.txt", "normalizer.json", "added_tokens.json",
    "special_tokens_map.json", "model.safetensors",
}


def normalized(text):
    return "".join(c for c in unicodedata.normalize("NFKC", text)
                   if not c.isspace() and not unicodedata.category(c).startswith("P"))


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--download", action="store_true")
    parser.add_argument("--audio-directory", default="project/Resources/farm/contest/audio/cheers")
    parser.add_argument("--output-directory", default=str(OUTPUT.relative_to(ROOT)))
    parser.add_argument("--changed-only", action="store_true")
    args = parser.parse_args()
    os.environ["HF_HUB_DISABLE_IMPLICIT_TOKEN"] = "1"
    os.environ["HF_HUB_DISABLE_TELEMETRY"] = "1"
    os.environ["HF_HOME"] = str(ROOT / "generated/tools/whisper_audit_cache")
    if args.download:
        from huggingface_hub import HfApi, snapshot_download
        info = HfApi().model_info(MODEL_ID, files_metadata=True, token=False)
        selected = [f for f in info.siblings if f.rfilename in FILES]
        assert "model.safetensors" in [f.rfilename for f in selected]
        assert all(f.size is not None for f in selected)
        size = sum(f.size for f in selected)
        assert size < 1.1 * 1024**3, "Unexpected model size"
        assert shutil.disk_usage(ROOT).free > size + 3 * 1024**3, "Reserve 3GB disk"
        print(f"Download {MODEL_ID}@{info.sha}: {size/1024**2:.1f}MiB, no audio upload", flush=True)
        snapshot_download(MODEL_ID, revision=info.sha, local_dir=MODEL,
                          allow_patterns=sorted(FILES), token=False, max_workers=2)
        (MODEL / "audit_revision.json").write_text(json.dumps({"model": MODEL_ID, "revision": info.sha}),
                                                  encoding="utf-8")
        return
    os.environ["HF_HUB_OFFLINE"] = "1"
    import numpy as np
    import torch
    import torchaudio.functional as audio
    from transformers import AutoProcessor, AutoModelForSpeechSeq2Seq

    torch.set_num_threads(4)
    revision = json.loads((MODEL / "audit_revision.json").read_text(encoding="utf-8"))
    print("Loading local ASR model on CPU", flush=True)
    processor = AutoProcessor.from_pretrained(str(MODEL), local_files_only=True, trust_remote_code=False)
    model = AutoModelForSpeechSeq2Seq.from_pretrained(str(MODEL), local_files_only=True,
        trust_remote_code=False, use_safetensors=True, dtype=torch.float32).eval()
    data = json.loads((ROOT / "tools/audio/contest_cheers.json").read_text(encoding="utf-8"))
    source = (ROOT / args.audio_directory).resolve()
    output = (ROOT / args.output_directory).resolve()
    assert source.is_relative_to(ROOT) and output.is_relative_to(ROOT), "Workspace paths only"
    records = []
    output.mkdir(parents=True, exist_ok=True)
    expected_count = sum(1 for p in data["pairs"] for s in ("L", "R")
                         if not args.changed_only or s in p.get("speech", {}))
    for pair in data["pairs"]:
        for side in ("L", "R"):
            if args.changed_only and side not in pair.get("speech", {}):
                continue
            name = pair["id"] + "_" + side
            path = source / (name + ".wav")
            expected = "".join(pair[side])
            with wave.open(str(path), "rb") as wav:
                channels = wav.getnchannels()
                assert channels in (1, 2) and (wav.getsampwidth(), wav.getframerate()) == (2, 24000)
                samples = np.frombuffer(wav.readframes(wav.getnframes()), dtype="<i2").astype(np.float32)
            mono = samples.reshape(-1, channels).mean(axis=1) / 32768
            resampled = audio.resample(torch.from_numpy(mono), 24000, 16000).numpy()
            features = processor(resampled, sampling_rate=16000, return_tensors="pt", return_attention_mask=True)
            # Never prompt with the expected caption: doing so would bias away from audible discrepancies.
            with torch.inference_mode():
                predicted = model.generate(**features, language="ja", task="transcribe", max_new_tokens=96,
                                           num_beams=3, do_sample=False)
            heard = processor.batch_decode(predicted, skip_special_tokens=True)[0].strip()
            record = dict(clip=name, expected=expected, recognized=heard,
                          exact_normalized_match=normalized(expected) == normalized(heard),
                          sha256=hashlib.sha256(path.read_bytes()).hexdigest())
            records.append(record)
            print(f"{name}: {expected} => {heard}", flush=True)
            report = dict(model=revision, method="CPU float32, Japanese, beam3; unprompted local ASR",
                          limitation="ASR can mishear or normalize words. Not a pronunciation verdict or listening approval.",
                          complete=len(records) == expected_count, clips=records)
            (output / "asr_report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n",
                                                    encoding="utf-8")
    print(f"COMPLETE:{len(records)} audited; shipped assets unchanged", flush=True)


if __name__ == "__main__":
    main()
