# Live USB control — Atmosphere 0.3.0

The pedal exposes its panel and stored USB settings through newline-delimited
commands on USB CDC serial, with JSON replies. Audio and control application
remain in the audio callback. USB reception only queues bytes; parsing,
formatting and nonblocking transmission run in main.

## Command-line tool

Python 3 on macOS/Linux; no pip packages needed. Close other serial monitors
before using this tool. Use `--port /dev/cu.usbmodem...` if discovery is ambiguous.

```sh
python3 tools/control.py get
python3 tools/control.py watch --interval 200
python3 tools/control.py set mix 0.5
python3 tools/control.py set wet_mode slip
python3 tools/control.py set routing loop
python3 tools/control.py set loop_mode stretch
python3 tools/control.py set wet_on on
python3 tools/control.py set freeze on
python3 tools/control.py clear freeze
python3 tools/control.py tap fs2
python3 tools/control.py clear all
```

`get` returns physical positions, effective control values, actual DSP effect
states, the requested overrides, effective clock divider and peak CPU usage.
Snapshots use a request ID so buffered streaming reports cannot be mistaken
for a fresh read. A queued change is confirmed by a snapshot whose `applied`
sequence has reached that command's acknowledgement ID.

`watch` streams JSON every 200 ms by default (100–10000 ms supported). Ctrl-C
stops the client and restores the default two-second reporting interval.
Stopping the monitor or disconnecting USB does not clear stored settings.

## Controls and units

| Name | Physical source | USB value |
|---|---|---|
| `mix` | Knob 1 | 0–1 |
| `time` | Knob 2 | 0–1 |
| `wet_modify` | Knob 3 | 0–1 |
| `clock` | Knob 4 | 0–1 |
| `length` | Knob 5 | 0–1 |
| `loop_modify` | Knob 6 | 0–1 |
| `wet_mode` | Toggle 1 | 0 Reverb / 1 Delay / 2 Slip |
| `routing` | Toggle 2 | 0 Input / 1 Both / 2 Loop |
| `loop_mode` | Toggle 3 | 0 Envelope / 1 Tape / 2 Stretch |
| `fs1` | Left footswitch | 0 released / 1 pressed |
| `fs2` | Right footswitch | 0 released / 1 pressed |
| `wet_on` | Wet section's latched state | 0/1 |
| `loop_on` | Listening/captured playback state | 0 listening / 1 captured playback |
| `freeze` | Wet hold function | 0/1 |
| `overdub` | Loop hold function | 0/1 |

The CLI accepts the mode names in the table, plus `on`/`off` and
`pressed`/`released`. The wire protocol uses numeric values. Knob telemetry
has four decimal places (0.01% increments), reflecting the smoothed physical
readings rather than raw ADC counts. Unoverridden DSP knobs retain their
original floating-point hardware values; USB does not quantize their path.

## Override behavior

- Only explicitly set controls are overridden. Physical readings remain visible
  and all other controls continue to follow the panel.
- USB values act as a preset: they have no timeout and remain active without
  a USB connection or monitor. They currently live in RAM, so power cycling
  clears them; this is not nonvolatile preset storage.
- Moving a physical knob by about 1% of its full travel from its position at
  the last USB set releases only that knob. Small ADC fluctuations are ignored,
  while slow movements accumulate toward the threshold. The physical value
  takes over immediately; there is no need to match the preset value first.
- Changing a physical toggle releases only that toggle. A footswitch transition
  releases its virtual button, associated on/off state and freeze/overdub hold
  override, leaving the rest of the preset untouched.
- `clear NAME` explicitly releases one override; `clear all` releases all.
- `wet_on` and `loop_on` set the latched state. The next physical switch
  action releases that side's overrides and works normally. Explicitly clearing
  an on/off override leaves the current latched state in place.
- `freeze` requires wet to be on; `overdub` requires captured playback. A request
  without its prerequisite remains visible under `overrides`, but `effective`
  reports the actual inactive state. Set the prerequisites explicitly when needed.
- `set loop_on 1` captures only when playback is currently off. Changing to zero
  resumes listening and replaces the old capture. This is a real musical action.
- `fs1`/`fs2` simulate actual button levels: release before 400 ms produces a tap;
  holding 400 ms activates freeze/overdub when available. `tap` uses a 100 ms
  press, releases it, confirms application, then clears that button override.
- Clearing a button override cancels its pending gesture and waits for both
  effective buttons to release. It does not manufacture a tap on clearing. To perform a tap, use an explicit press/release or `tap`.
- Direct `freeze` and `overdub` overrides can be active together for debugging.
  Simulating both footswitches together cancels individual gestures, as on the
  panel, but cannot request DFU. Physical recovery always observes the real
  buttons, even if USB is overriding them.

The DSP receives overridden controls at the next 48-sample callback, normally
within about 1 ms. Existing DSP smoothing still applies. Some abrupt changes,
such as clock or playback direction, intentionally change the sound immediately.
The musical differences recorded in the 0.1.2 use-case audit, including Slip
TIME and non-editable wet freeze, are not fixed by exposing their controls.

## Wire protocol

ASCII, case-sensitive, one command per LF or CRLF line. Maximum line length:
127 bytes. Invalid commands get an error without modifying controls.

```text
get
get 12345
set mix 0.5
set wet_mode 2
set fs1 1
set fs1 0
clear mix
clear all
watch 200
watch 0
ping
dfu
```

`get N` echoes a nonzero unsigned 32-bit request ID. Periodic reports and plain
`get` use request zero. `set`/`clear` reply with
`{"type":"ack","id":N,"queued":true}`. Poll `get` until `applied >= N`
to confirm the audio callback has applied the command. IDs restart after reboot.
`ping` replies with `{"type":"pong"}`. Errors have `type: error` and a symbolic
`error` value. Physical takeover is reflected by the affected key disappearing from `overrides`.

Output is bounded and nonblocking; periodic telemetry can be skipped while USB
is busy. Command parsing backs off when its reply queue fills. Overflowing RX
or an oversized line discards the damaged line through a newline instead of
executing a suffix. The client must read replies while sending commands. If
RX overflow is reported, send a newline and retry the complete command.

## Firmware updates without opening the enclosure

Build first and start `tools/flash.py` before requesting DFU from a second
terminal (or a second agent tool session):

```sh
python3 tools/control.py dfu
```

The command triggers bootloader entry, including the LED sequence, then USB
disconnects. A command reply is not guaranteed during reset; verify the DFU
transfer and the rebooted version. The physical two-switch hold still works
independently of USB and its override state.

## Validation

`make test` includes parser range/NaN checks, packet fragmentation, overflow
recovery, persistent presets, ADC noise rejection, physical takeover, explicit clearing,
coherent concurrent snapshot reads, bounded
USB output with an unread endpoint, buffer ownership, and the host client's
request/reply transport through a pseudo-terminal. Existing audio and gesture
regressions also run. See [VALIDATION.md](VALIDATION.md) for hardware results.
