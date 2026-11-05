"""Isolate shop comparisons without overwriting earlier menu evidence."""
import argparse
import json
import shutil
import subprocess
import prepare_menu_qa as qa

qa.OUT = qa.ROOT / "generated/ui_trade_20261102"


def prepare(phase):
    qa.prepare(phase)
    root = qa.OUT / phase
    fixture = qa.OUT / "fixture"
    if not fixture.is_dir():
        raise RuntimeError("Run the trade fixture test before staging")
    shutil.copytree(fixture, root / "runtime/Settings/farm")
    manifest = json.loads((root / "manifest.json").read_text(encoding="utf-8"))
    manifest["fixture"] = "Synthetic UI-only day9, 300G, 3 crop kinds, one protected carrot; not cultivation evidence"
    manifest["fixture_hashes"] = qa.manifest(fixture)
    files = subprocess.check_output(["git", "ls-files", "--cached", "--others", "--exclude-standard",
                                    "project/application", "project/engine"], cwd=qa.ROOT, text=True).splitlines()
    manifest["code"] = {p: qa.digest(qa.ROOT / p) for p in files if (qa.ROOT / p).is_file()}
    (root / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("step", choices=("before", "after", "protect", "build", "tests"))
    parser.add_argument("--configuration", choices=("Release", "Debug"), default="Release")
    args = parser.parse_args()
    if args.step == "protect":
        qa.protect()
        print("Original settings and submissions unchanged")
    elif args.step == "build":
        qa.build("after", args.configuration)
    elif args.step == "tests":
        qa.protect()
        for name in ("farm_trade", "farm_seed_shop", "farm_runtime_view", "farm_harvest_display", "farm_play_loop"):
            with (qa.OUT / f"test_{name}.log").open("wb") as log:
                result = subprocess.run(["cmd.exe", "/d", "/c", str(qa.ROOT / f"tools/tests/run_{name}_test.cmd")],
                                        cwd=qa.ROOT, stdout=log, stderr=subprocess.STDOUT)
            print(name, result.returncode, flush=True)
            if result.returncode:
                raise SystemExit(result.returncode)
        qa.protect()
    else:
        prepare(args.step)
