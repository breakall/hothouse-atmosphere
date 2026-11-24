# Build and install Atmosphere

Current firmware: 0.3.0. Target: stereo Hothouse with a Daisy Seed.

## Build


Requires Python 3, GNU Make, and the Arm GNU embedded toolchain on PATH
(`arm-none-eabi-gcc`, `arm-none-eabi-g++`, `arm-none-eabi-objcopy`).
The compiler must include newlib and the C++ runtime; a bare compiler-only
package is insufficient. On Apple silicon, a complete, checksum-pinned
[official Arm GNU toolchain](https://developer.arm.com/Tools%20and%20Software/GNU%20Toolchain)
can be installed locally with `python3 tools/toolchain_macos.py` (Python 3.11+).
The top-level Makefile automatically uses `deps/toolchain/bin` when present.

```sh
make deps
make firmware
```

`make deps` fetches libDaisy at the revision pinned in `tools/setup.py` and
builds it. Alternatively, reuse an existing checkout without network access:

```sh
python3 tools/setup.py --local /absolute/path/to/libDaisy
make firmware
```

The local mode **copies** the dependency; subsequent builds do not access the
original folder. `deps/` is ignored by Git. Keep its upstream licenses when
redistributing it. The firmware needs no DaisySP or NAM dependency.

Output: `firmware/build/atmosphere.bin` and `firmware/build/atmosphere.elf`.
The application is built as `BOOT_SRAM` for an existing Daisy bootloader,
with an application download address of `0x90040000`. It does not include a
bootloader installer or write NAM capture storage intentionally.

## Install and recover

Build first. Connect the Hothouse over USB, then run:

```sh
python3 tools/flash.py firmware/build/atmosphere.bin
```

The watcher prints a prompt when it is ready. If the existing NAM application
supports its recovery gesture, hold both switches for at least two seconds.
The tool accepts only a single Daisy DFU device exposing the expected QSPI
application region. STM32 ROM DFU is rejected; installing a bootloader is a
separate operation. Direct USB access is required; a sandboxed empty device
list does not establish that the pedal failed to enter DFU.

After successful download the tool resets the device. If it remains in the
bootloader, press RESET or power-cycle it. Atmosphere reports JSON snapshots with the firmware version, physical and
effective controls, active overrides, effect states and peak measured CPU usage
over USB serial every two seconds by default. CPU is in thousandths of the
1 ms audio budget: 1000 means the measured callback consumed the whole budget.
The diagnostic is a measurement aid; it does not establish hardware validation.

## Desktop checks and sound previews

```sh
make test    # clang++, address and undefined-behavior sanitizers
make render  # nine stereo 48 kHz WAVs in build/audio/
```

Previews use synthesized plucked tones, not a guitar recording. They capture at
2 s, remove live input at 3 s, lower CLOCK at 4 s, and freeze wet at 6 s. They
are useful for comparing algorithms and checking output, not judging fidelity
to the commercial pedal.
