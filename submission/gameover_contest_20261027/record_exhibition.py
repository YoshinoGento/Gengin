"""Reuse window-only recording and target-process audio, without touching game state."""
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "contest_record", ROOT / "submission/transition_contest_20261013/record_transitions.py")
record = importlib.util.module_from_spec(spec)
spec.loader.exec_module(record)
record.OUT = ROOT / "generated/exhibitions/contest_submission_20261027/verification"

if __name__ == "__main__":
    record.main()
