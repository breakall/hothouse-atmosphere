# Validation and first hardware session

## Build verified on October 30, 2026

Arm GNU 13.2.Rel1 successfully compiled and linked the BOOT_SRAM application.
The binary is about 95 KiB. The link map places the engine at `0xc0000000`
in external SDRAM (992,956 bytes), with approximately 97 KiB SRAM, 26 KiB
DTCM, and 17 KiB DMA RAM used. Startup disassembly confirms that global
constructors do not touch the engine's SDRAM before `hw.Init()`.
The linker reports the usual bare-metal newlib syscall-stub and RWX-segment
warnings; no application compile warnings were emitted.

## Desktop coverage

### 0.3.0 preset-style USB settings

Removed override expiration and monitor keepalives. Each USB-set knob/toggle
retains its value until physical takeover, explicit clearing or power-off.
Knob takeover compares the current physical reading to its position at the
last USB set, using a 1% full-travel threshold to reject ADC noise. Toggle
changes release only that toggle. Physical footswitch transitions release that
side's virtual button, on/off and hold controls; physical recovery is unchanged.

`make test` passes, including 60,000 simulated panel samples with ADC noise,
slow accumulated movement, fresh anchoring after a new USB set, isolated toggle
takeover, and independent left/right footswitch takeover. Existing transport,
audio and recovery regressions remain green. The ARM application compiled and
was installed over USB DFU; its binary is 114,392 bytes.
On hardware, all nine knob/toggle recipe values remained active after 35 seconds
without any USB command or keepalive. The test then cleared its settings and
returned the panel to physical control. Actual hand-operated takeover has not
been separately confirmed on hardware; that behavior is covered by the desktop
panel-input tests above.

### 0.2.1 live USB control

Added full physical/effective panel telemetry, temporary overrides for all
eleven panel controls and four effect states, nonblocking queued commands,
request-correlated snapshots, simulated switch presses, streaming and USB DFU.
Desktop checks cover malformed/range/NaN inputs, packet fragmentation, RX
overflow recovery, maximum-size JSON, override clearing, concurrent snapshot
publication, busy USB buffer ownership, and a pseudo-terminal host transport.
All existing audio and splice regressions still pass; no DSP algorithms changed.

The ARM binary is 114,296 bytes, with approximately 46 KiB DTCM and 111 KiB SRAM
used; external SDRAM remains 992,956 bytes. Hardware installation succeeded and
USB reported version 0.2.1. The previous interim 0.2.0 interface was used to
request DFU remotely, confirming that future updates need no manual gesture.

Hardware writes/readback verified all six knobs, all three toggles, both virtual
button levels and the four logical effect states. A virtual two-button hold
lasting over two seconds did not trigger DFU. The test restored the original
effect states and cleared the overrides. See [USB_DEBUG.md](USB_DEBUG.md).
Automatic expiry was also verified on hardware: after setting TIME remotely and
sending no keepalives for over 30 seconds, a fresh request-correlated snapshot
showed an empty override map and effective controls matching the physical panel.
The observed peak callback load reached 22.4% during this debug exercise; this
is an observed measurement rather than an exhaustive worst-case bound.

### 0.1.1 loop-splice fix

The reported repeat pop was reproduced using `make test` with a clean sine
capture whose period did not align with the recording window. The previous
crossfade blended the end toward the first 96 frames, then reset the cursor to
frame zero, causing a second discontinuity. The largest adjacent-sample jump
was 0.251870 on a 0.3-amplitude signal. Capture engagement stayed smooth in
this reproduction (maximum jump 0.005189).

The loop reader now overlaps the end and beginning into one continuous cycle,
with the playback cursor wrapping at the overlap boundary. Tape, envelope,
stretch and wet-output freeze use the corrected splice. The overlap is up to
240 internal frames, capped at one eighth of the source region; playback is
shorter than the unspliced captured region by that overlap.

The same reproduction now measures 0.005986 at repeats. An additional 48 cases
cover all loop modes, eight tape-speed knob positions and full/cropped lengths
with a nonzero physical ring start. All pass the sample-discontinuity limit
under address/undefined-behavior sanitizers; worst jump across these cases is
0.023505, including 4x playback. Hardware listening remains necessary to confirm
the user's exact sound is resolved.

### 0.1.2 USB logging and recovery

Recovery from the initially installed firmware was delayed until USB serial
was opened. A host reproduction using the actual libDaisy logger confirmed
that `StartLog(false)` only starts in nonblocking mode: after two successful
packets, subsequent reports busy-wait until the USB endpoint accepts data.
That can stall the main loop before it services `boot_requested`.

The application now uses a single nonblocking transmit attempt per status
report. Two persistent buffers preserve the driver's ownership of an in-flight
packet while a rejected report is dropped. The regression exercises a USB
endpoint that accepts two packets and then stays busy; it also verifies that
rejected writes never overwrite the in-flight buffer. The main loop stays
available to service recovery independently of a terminal connection.

`make test` exercises exact stereo bypass, capture of audio preceding the
footswitch press, playback with silent input, full-wet input rejection,
dry-input overdubbing, empty capture, all 135 combinations of wet mode, loop
mode, routing, and clock, pot movement during playback, freeze, resuming
recording after overdubbing, and tap/hold/recovery chord handling. Address and
undefined-behavior sanitizers check memory access during these paths.

These tests check safety and basic behavior, not perceptual equivalence.
`make render` creates synthetic audio for all nine pairs of effects modes.

## Target pedal validation

The 0.1.2 update subsequently wrote 97,456 bytes to `0x90040000`, with both
successful dfu-util completion messages. Rebooted USB serial repeatedly
reported `Atmosphere 0.1.2 state=0 peak_cpu_permille=62`, confirming this build
is running (6.2% measured peak callback load with effects bypassed). The old
application's blocked recovery completed while its serial output was drained.
The 0.1.2 recovery path without a serial reader and the audible result of the
splice fix still need user verification on the pedal.

Initial USB installation completed on October 30, 2026: dfu-util wrote 97,072
bytes to `0x90040000` and reported `Download done` and `File downloaded
successfully`. The rebooted device at `/dev/cu.usbmodem3466376232331` repeatedly
reported `Atmosphere 0.1.0 state=0 peak_cpu_permille=46`. This confirms startup
with both sections bypassed and a measured 4.6% peak callback load in that
state only. Active-mode load, audio quality and the new application's recovery
gesture remain untested. The initial flash used the NAM application's gesture.

Remaining hardware checks:

- Record USB identity, installed bootloader, and existing application backup
  path before the first installation. Retain the known NAM binary for rollback.
- Flash only the QSPI application area with the supplied watcher. Check both
  successful dfu-util transfer and the rebooted serial firmware identifier.
- Verify dry bypass and channel separation using different signals on L/R.
- Confirm physical knob and toggle numbering, LED brightness, and gestures.
- Exercise all modes and routes, including long tails and held overdubs.
- Sweep clock, length, direction, and mode while playing. Listen for clipping,
  objectionable clicks, aliasing, unexpected silence, and excessive noise.
- Log peak CPU load at the fastest clock with reverb plus stretch; require
  headroom below the 1000-permille deadline under sustained control movement.
- Test two-switch recovery from every effect/hold state, then reinstall the
  application to prove the recovery path works.

## Known implementation limits

- A 24,000-frame stereo history gives 0.5 s at the highest internal clock and
  8 s at the lowest. Changing clock changes playback speed, pitch and quality.
- No resampling anti-alias filter: lower clock settings intentionally alias.
  Linear reconstruction softens the held output. High-speed tape playback also
  aliases; quality at these extremes needs listening tests.
- Tape playback uses eight rates: reverse 4/2/1/0.5, forward 0.5/1/2/4.
- Stretch is granular. It preserves approximate local pitch while changing
  progression; it is not a phase-vocoder or transient-preserving stretcher.
- Overdubs use a separate chronological write head with bounded accumulation,
  rather than reproducing the commercial pedal's exact write-head behavior.
- Freeze repeats a recent wet-output segment. The tail/head overlap removes
  the abrupt wrap; periodic crossfade coloration can still be audible.
- Mode changes blend over 240 internal ticks; input/output and enable mixing
  are smoothed. Existing wet-buffer contents remain across algorithm changes,
  which can create transitional artifacts.
- Maximum delay is about 0.92 s at the highest clock, 14.7 s at the lowest.
- Digital bypass still passes through the Hothouse ADC/DAC; it is not a relay
  or analog bypass implementation.
