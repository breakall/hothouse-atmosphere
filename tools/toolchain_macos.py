#!/usr/bin/env python3
"""Install a checksum-pinned official Arm toolchain inside deps/ (Apple silicon)."""
import hashlib
import pathlib
import platform
import subprocess
import urllib.request

root = pathlib.Path(__file__).resolve().parents[1]
if platform.system() != "Darwin" or platform.machine() != "arm64":
    raise SystemExit("This helper targets Apple silicon; install a complete Arm embedded toolchain for your host")
destination = root / "deps/toolchain"
if (destination / "bin/arm-none-eabi-gcc").exists():
    raise SystemExit(f"Toolchain already exists at {destination}")
archive = root / "deps/arm-toolchain.tar.xz"
archive.parent.mkdir(parents=True, exist_ok=True)
url = ("https://developer.arm.com/-/media/Files/downloads/gnu/13.2.rel1/binrel/"
       "arm-gnu-toolchain-13.2.Rel1-darwin-arm64-arm-none-eabi.tar.xz")
expected = "39c44f8af42695b7b871df42e346c09fee670ea8dfc11f17083e296ea2b0d279"
if not archive.exists():
    print("Downloading official Arm GNU 13.2.Rel1…", flush=True)
    urllib.request.urlretrieve(url, archive)
with archive.open("rb") as stream:
    digest = hashlib.file_digest(stream, "sha256").hexdigest()
if digest != expected:
    raise SystemExit("Toolchain checksum mismatch; remove the archive and retry")
destination.mkdir(parents=True, exist_ok=True)
subprocess.run(["tar", "-xJf", str(archive), "-C", str(destination), "--strip-components=1"], check=True)
print("Toolchain ready. Run make deps, then make firmware.")
