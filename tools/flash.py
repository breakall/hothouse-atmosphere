#!/usr/bin/env python3
"""Wait for Daisy QSPI DFU and download only a BOOT_SRAM application."""
import argparse
import pathlib
import re
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("binary", type=pathlib.Path)
parser.add_argument("--timeout", type=float, default=60)
args = parser.parse_args()
binary = args.binary.resolve()
if not binary.is_file() or not 8 < binary.stat().st_size <= 480 * 1024:
    raise SystemExit("Expected an existing BOOT_SRAM .bin, between 8 bytes and 480 KiB")
print("Watcher ready. Hold both Hothouse footswitches for at least two seconds.", flush=True)
deadline = time.monotonic() + args.timeout
while time.monotonic() < deadline:
    result = subprocess.run(["dfu-util", "-l"], capture_output=True, text=True, check=False)
    lines = [line for line in result.stdout.splitlines() if "Found DFU:" in line and "[0483:df11]" in line]
    devices = set(re.findall(r'path="([^"]+)"', "\n".join(lines)))
    if len(devices) > 1:
        raise SystemExit("Multiple Daisy/STM32 DFU devices connected; connect only the intended pedal")
    qspi = [line for line in lines if "0x90000000" in line]
    if qspi:
        if len(qspi) != 1:
            raise SystemExit("Ambiguous QSPI target")
        match = re.search(r"alt=(\d+)", qspi[0])
        if not match:
            raise SystemExit("Could not parse QSPI target alternate setting")
        path = re.search(r'path="([^"]+)"', qspi[0])
        if not path:
            raise SystemExit("Could not identify the Daisy USB path")
        # The comma selects DFU IDs specifically; a runtime-only -d filter
        # leaves the DFU ID wildcarded and can match unrelated USB hardware.
        subprocess.run(["dfu-util", "-d", ",0483:df11", "-p", path.group(1), "-a", match.group(1),
                        "-s", "0x90040000:leave", "-D", str(binary)], check=True)
        print("Transfer completed. Verify the rebooted version with tools/control.py get.")
        break
    if lines:
        raise SystemExit("DFU device does not expose Daisy QSPI; refusing STM32 ROM/internal-flash target")
    time.sleep(.25)
else:
    raise SystemExit("No accessible Daisy QSPI DFU device before timeout; check USB device permissions")
