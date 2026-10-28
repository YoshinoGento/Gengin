"""Record the demo window and target-process audio without reading personal saves."""
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "demo_record", ROOT / "submission/transition_contest_20261013/record_transitions.py")
record = importlib.util.module_from_spec(spec)
spec.loader.exec_module(record)
record.OUT = ROOT / "generated/exhibitions/gameover_demo_submission_20261028/verification"

if __name__ == "__main__":
    record.main()
