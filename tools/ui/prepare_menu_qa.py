"""Keep independent, hashed before/after runtimes for the UI presentation."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "generated/ui_presentation_20261102"
sys.path.insert(0, str(ROOT / "submission/monthly_202610"))
from assemble_sources import resources


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def manifest(root):
    return {p.relative_to(root).as_posix(): digest(p) for p in sorted(root.rglob("*")) if p.is_file()}


def protect():
    paths = [ROOT / "project/Settings", ROOT / "generated/monthly_202610/ready",
             ROOT / "generated/monthly_202610/ready_small",
             ROOT / "generated/exhibitions/gameover_demo_submission_20261028"]
    value = {p.relative_to(ROOT).as_posix(): manifest(p) for p in paths}
    path = OUT / "protected.json"
    OUT.mkdir(parents=True, exist_ok=True)
    if path.exists():
        assert value == json.loads(path.read_text(encoding="utf-8")), "Protected files changed"
    else:
        path.write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding="utf-8")


def prepare(which):
    protect()
    target = OUT / which / "runtime"
    if target.exists():
        raise RuntimeError("Refusing to overwrite a recorded runtime")
    for source in resources():
        destination = target / source.relative_to(ROOT / "project")
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
    for name in ("CG2_00_01.exe", "dxcompiler.dll", "dxil.dll"):
        shutil.copy2(ROOT / "generated/outputs/Release" / name,
                     target / ("MenuQA.exe" if name.endswith(".exe") else name))
    revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    files = subprocess.check_output(["git", "ls-files", "project/application", "project/engine"], cwd=ROOT, text=True).splitlines()
    report = {"revision": revision, "runtime": manifest(target),
              "code": {p: digest(ROOT / p) for p in files if (ROOT / p).is_file()},
              "fixture": "No Settings; new 30-day tournament, day1, 300G; 1280x720 client",
              "capture": "pending; use only actual game frames"}
    (OUT / which / "manifest.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    (OUT / which / "screenshots").mkdir()
    protect()
    print(target)


def build(which, configuration):
    protect()
    command = OUT / f"build_{which}_{configuration}.cmd"
    command.write_text('@echo off\r\nsetlocal\r\nset "QA_PATH=%PATH%"\r\nset Path=\r\nset "PATH=%QA_PATH%"\r\n'
                       f'"C:\\Program Files\\Microsoft Visual Studio\\18\\Community\\MSBuild\\Current\\Bin\\MSBuild.exe" '
                       f'"{ROOT / "project/CG2_00_01.sln"}" /t:Build /p:Configuration={configuration} '
                       '/p:Platform=x64 /p:CL_MPCount=2 /m:2 /nr:false /v:minimal /nologo\r\nexit /b %errorlevel%\r\n', encoding="ascii")
    with (OUT / f"build_{which}_{configuration}.log").open("wb") as log:
        result = subprocess.run(["cmd.exe", "/d", "/c", str(command)], cwd=ROOT / "project", stdout=log, stderr=subprocess.STDOUT)
    print(f"{which} {configuration} build exit={result.returncode}")
    if result.returncode:
        raise SystemExit(result.returncode)
    protect()


def tests():
    protect()
    for name in ("farm_runtime_view", "title_audio_settings"):
        with (OUT / f"test_{name}.log").open("wb") as log:
            result = subprocess.run(["cmd.exe", "/d", "/c", str(ROOT / f"tools/tests/run_{name}_test.cmd")],
                                    cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
        print(f"{name} test exit={result.returncode}")
        if result.returncode:
            raise SystemExit(result.returncode)
    protect()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("step", choices=("before", "after", "protect", "build", "tests"))
    parser.add_argument("--which", choices=("before", "after"), default="before")
    parser.add_argument("--configuration", choices=("Release", "Debug"), default="Release")
    args = parser.parse_args()
    if args.step == "build":
        build(args.which, args.configuration)
    elif args.step == "tests":
        tests()
    elif args.step == "protect":
        protect()
        print("Protected saves and deliveries unchanged")
    else:
        prepare(args.step)
