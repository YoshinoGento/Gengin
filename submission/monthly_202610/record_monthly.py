"""Record only the isolated monthly game window and its own audio."""
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "monthly_capture", ROOT / "submission/transition_contest_20261013/record_transitions.py")
record = importlib.util.module_from_spec(spec)
spec.loader.exec_module(record)
record.OUT = ROOT / "generated/monthly_202610/captures"

if __name__ == "__main__":
    record.OUT.mkdir(parents=True, exist_ok=True)
    record.main()
