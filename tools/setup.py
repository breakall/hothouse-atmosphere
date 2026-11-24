#!/usr/bin/env python3
"""Obtain pinned libDaisy without depending on the NAM project at build time."""
import argparse
import os
import pathlib
import shutil
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
REVISION = "f7727edb9a1febdd174b5310a7bc65340dae8700"
parser = argparse.ArgumentParser()
parser.add_argument("--local", type=pathlib.Path, help="copy an existing libDaisy checkout (offline)")
args = parser.parse_args()
destination = ROOT / "deps" / "libDaisy"
if destination.exists():
    print(f"Using existing dependency at {destination}", flush=True)
elif args.local:
    source = args.local.resolve()
    revision = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    if revision != REVISION:
        raise SystemExit(f"Expected libDaisy {REVISION}, found {revision}")
    shutil.copytree(source, destination, ignore=shutil.ignore_patterns(".git", "build", "examples", "tests"))
else:
    destination.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["git", "clone", "https://github.com/electro-smith/libDaisy.git", str(destination)], check=True)
    subprocess.run(["git", "-C", str(destination), "checkout", REVISION], check=True)
    subprocess.run(["git", "-C", str(destination), "submodule", "update", "--init", "--recursive"], check=True)
environment = os.environ.copy()
if (ROOT / "deps/toolchain/bin/arm-none-eabi-gcc").is_file():
    environment["PATH"] = str(ROOT / "deps/toolchain/bin") + os.pathsep + environment["PATH"]
subprocess.run(["make", "-C", str(destination), "-j4"], env=environment, check=True)
