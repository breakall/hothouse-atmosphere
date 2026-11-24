# MOOD MkII use-case audit — Atmosphere 0.1.2

Reviewed October 30, 2026. Scope: the current firmware, not a proposed feature
set. This is a review of documented musical workflows, not a measured ranking
of what MOOD owners use most often.

Atmosphere supports the central capture-and-transform workflow. It does not
yet provide full MkII behavior: identical mode names conceal several significant
control, freeze, timing and performance differences. Sonic equivalence is
unverified; there is no reference MOOD available for A/B listening.

## Evidence and limits

- The primary-source feature reference is [MOOD_MKII_REFERENCE.md](MOOD_MKII_REFERENCE.md),
  with official Chase Bliss product, main-manual, feature-summary and MIDI links.
- Code reviewed: [engine.cpp](../firmware/engine.cpp),
  [engine.h](../firmware/engine.h), [main.cpp](../firmware/main.cpp),
  [gestures.h](../firmware/gestures.h).
- `make test` passes the existing sanitizer, capture, bypass, gesture and splice
  checks, including 135 mode/routing/clock stability combinations and 48 splice
  cases. Those counts do not establish faithful behavior or sound.
- `make audit` runs [use_case_audit.cpp](../tests/use_case_audit.cpp) against
  the real portable engine, with control updates every 48 samples where timing
  changes matter. Its `LIMIT` results deliberately characterize current gaps;
  success means the report is reproduced, not that parity has passed.
- Hardware evidence: firmware 0.1.2 boots, its status has reported both sections
  engaged and a peak callback measurement of 8.9% in the observed configuration,
  and the user confirmed the repeat pop was fixed. This is not worst-case CPU
  validation or an audio-quality survey of every mode.
- This audit changes no firmware and performs no flash.

## Workflow coverage

The workflow selection follows the official sources linked in the reference
note. All implementation descriptions and verdicts below are independently
derived from Atmosphere's code or the indicated probes.

| Musical use | Verdict | What Atmosphere currently does | Verification |
|---|---|---|---|
| Catch a chord or phrase and play over it | Supported | RIGHT captures recent stereo history; MIX blends processed material with the live instrument. Playback remains in RAM until replaced or powered off. | Existing capture/bypass tests; code |
| Reverse, slow or pitch a tape micro-loop | Supported, with its own tuning | Eight signed playback rates; LENGTH crops the capture. CLOCK adds further octave changes. Splices consume an overlap region, slightly shortening playback. | Code; existing eight-zone splice cases |
| Build a slow granular pad | Supported as an approximation | Stretch advances grain anchors slowly, backward, or not at all; LENGTH changes grain size. Local grain playback remains forward, including while the anchors scan backward. | Stationary-grain probe; code |
| Stutter in response to guitar dynamics | Supported | The louder input repeats a slice; silence releases progression. Sensitivity is adjustable. This is stuttering, not simply an output noise gate. | Envelope hold/release probe |
| Process a captured loop with reverb, delay or Slip while leaving live guitar dry | Supported structurally | T2 DOWN feeds only the loop to wet DSP; global MIX controls the dry guitar contribution. No separate live/loop output levels. | Signal-path code |
| Process live input and keep the loop separate, or process both | Supported structurally | T2 UP feeds input and mixes the unprocessed loop separately; MIDDLE feeds a fixed-gain sum into wet DSP. | Signal-path code |
| Record ambience into the next capture | Supported | The listening recorder receives dry input plus half the wet output whenever wet is engaged, regardless of T2. It does not record exactly the final MIX setting. | Wet-tail re-capture probe; code |
| Add more playing to a captured loop | Partial | Holding RIGHT adds dry input. The independent write head advances only during overdub, with fixed attenuation and no adjustable fade, refresh or latch. | Existing overdub test; code |
| Live granular pitch/reverse effects | Partial, with a control defect | Slip has semitone playback-rate commands, but TIME can remain stuck; extreme reverse reads also hit buffer clamps. | TIME comparison probe; bounds analysis |
| Hold an ambient pad or second loop in the wet section | Partial | LEFT hold repeats recent wet output. Both wet and micro-loop can sound together. The wet capture's duration and algorithm are locked until release. | Frozen-audio probe; code |
| Reshape or reharmonize a frozen wet sound | Missing behavior | Wet returns the captured output before executing its algorithm. MODIFY cannot reshape the frozen texture; TIME does not resize that captured region. CLOCK still retimes it. | Opposite frozen Slip commands produce identical audio |
| Keep an ordinary delay circulating indefinitely | Partial | Maximum feedback is 0.995, so repeats decay. Freeze gives stable repetition, but locks the captured wet segment instead of leaving a fully editable delay network. | Delay impulse probe |
| Seamlessly retain ambience when bypassing wet | Missing option | Wet tails may keep evolving internally, but are no longer routed to the output. The transition is a short blend, not audible trails. | Bypass-tail probe |
| Preserve stereo material | Supported | Tape/Envelope/Stretch and Delay retain separate input channels. Reverb also mixes channels. Mono input is not automatically duplicated. | Left-only capture probe; code |
| Deliberately create moving stereo imagery | Mostly missing | No selectable spread, width or per-mode stereo motion. Tape uses the same playback direction for both channels; Delay feedback is not ping-pong. Reverb has its own fixed cross-channel structure. | Left-only Tape probe; code |
| Continuously refresh a live stretch/delay texture | Missing dedicated behavior | Captures can be manually replaced, but there is no refresh/no-dub mode or adjustable replacement/decay control. | Capture and overdub code |
| Play the captured sound from a keyboard or sequencer | Missing | No MIDI-note input, note-based transposition, output ADSR or Synth Mode. | Application/USB code |
| Dial in a deliberate vintage/lo-fi character | Partial | Reduced-clock aliasing is available, but there is no selectable Classic behavior, noise control or intentional capture deterioration outside the fixed overdub attenuation. | DSP/control code |
| Coordinate rhythms, automated changes or preset recalls | Missing | No channel timing sync, MIDI clock/CC/PC/notes, preset storage, expression/CV processing, or internal ramping. USB currently emits status only. | Complete application/control-path review |

## Significant behavior gaps

### 1. Slip TIME is not reliably functional

`Configure()` changes `delay_b_` only after `delay_fade_` reaches one.
Initialization schedules a delay-head crossfade, but only the Delay algorithm
advances it. Slip consumes `delay_b_` without completing that transition.
Consequently, a fresh start in Slip gives identical audio for TIME 10% and 90%.
After visiting Delay, a subsequent Slip setting may update once and stick again.

This is a defect, not a deliberate tonal approximation. It means the cheat
sheet's suggested Slip grain sizes are not reliable in the current firmware.
The octave pitch command still works; the grain-size part of that recipe needs
this fix before it can be considered verified.

The same stale delay state also supplies the wet-freeze capture length in
other modes. Reverb TIME itself still works because its algorithm reads the
parameter directly.

### 2. Wet freeze captures output instead of freezing editable DSP state

`SetFreeze()` captures a window of `freeze_history_`. The frozen branch of
`Wet()` returns before Delay, Slip or Reverb processing. A note can remain
audible, but turning the wet modifier does not change its pitch, direction or
diffusion while held. This blocks significant frozen-sound performance uses.

### 3. Timing and layering are limited

Atmosphere has five internal clock rates: 48/24/12/6/3 kHz. Its 24,000-frame
capture gives nominal listening windows of 0.5/1/2/4/8 seconds, with a small
overlap reduction during playback. See the official reference for the broader
MkII clock and capture range. There is no continuous sweep or non-octave step.

Overdub writes one captured frame per internal tick using a separate cursor.
It resumes at its last write position rather than tracking the audible tape or
grain read position. Its full-buffer period also differs from the shortened
splice playback period. Ordinary accurately aligned layering is therefore not
established, even at forward unity speed. Intentional evolving textures are
possible, but that is not a substitute for predictable layering.

### 4. Hands-free combinations and routing controls are incomplete

Holding both footswitches immediately cancels the individual holds, and after
two seconds requests DFU. Simultaneous wet freeze and loop overdubbing cannot
be performed through this interface. Holds are momentary, with no latch option.

Only a global MIX exists. There is no independent wet/loop balance, direct-loop
blend when routed through wet, adjustable overdub decay, wet tone control,
stereo width or refresh/live mode. Several workflows can be approximated by
manual knob movement and capture replacement, but cannot be recalled reliably.

### 5. Further sound-quality limitations

These are code-derived risks, not measured comparisons to a commercial unit:

- Reverse Slip at 2x asks for negative delays during part of its grain cycle;
  the clamp forces those reads to a fixed near-current position. Large grains
  can also hit the maximum delay bound. Reverse pitch quality is therefore
  compromised at those extremes.
- Reverb is an original seven-tap cluster plus two diffusers. Having similar
  controls does not establish matching density, decay or modulation.
- Lower clock settings downsample without an anti-alias filter. High-speed
  playback also lacks band-limited resampling. Crunch is available; polished
  high-fidelity pitch behavior has not been demonstrated.
- Mode and abrupt parameter changes have only limited transition handling.
  A continuous sine splice test is not sufficient evidence of click-free
  performance under every control movement or envelope trigger.
- Audio bypass is digital. An analog dry-through path cannot be obtained just
  by changing this firmware on the current signal path.

## Current three cheat-sheet recipes

| Recipe | Result of audit |
|---|---|
| Cloud bed | The workflow is supported: stationary/slow grains and loop-through-reverb. The particular timbre still needs listening tests. |
| Reverse postcard | Reverse Tape through Delay is supported; delay repeats decay and there is no rhythmic synchronization. |
| Octave constellation | Captured loop through forward 2x Slip is structurally supported. Its short-grain TIME setting is currently unreliable, so the full recipe is only partially verified. |

## Reproducible audit results

```text
make audit
LIMIT slip_time: TIME 10% vs 90% after fresh boot gives max output difference 0.000000000
LIMIT frozen_edit: reverse vs forward Slip MODIFY gives max difference 0.000000000; captured audio remains audible
LIMIT tape_spread: left-only capture produces right-channel energy 0.000000000
LIMIT bypass_trails: audible tail energy after 100 ms bypass = 0.000000000
SUPPORTED stretch_hold: consecutive stationary grains differ by at most 0.000000000
SUPPORTED envelope_stutter: held slice difference 0.000000000; silence releases progression (difference 0.157318)
SUPPORTED wet_recapture: resampled tail energy with live input silent = 10.642745
LIMIT delay_infinite: repeat 100 / repeat 1 amplitude = 0.608815
```

The maximum-feedback delay retains about 61% of the first repeat's amplitude at
repeat 100 in this impulse test. These numbers describe the synthetic fixtures,
not a general perceptual rating.

## Recommended order of work

1. Fix Slip TIME, decouple grain control from delay-transition state, and
   correct reverse read-head bounds. These affect ordinary panel controls.
2. Make overdubbing track a deliberate audible loop timeline; test repeated
   layer alignment. Add adjustable fade if evolving layers are a priority.
3. Implement editable, mode-aware freeze and a safe way to combine or latch
   hold actions without sacrificing recovery.
4. Add audible bypass trails and independent loop/wet levels, then extend
   stereo motion and the useful capture/clock range.
5. Add presets and automation/sync suited to the HX Stomp setup. MIDI transport
   and the physical routing between devices must be designed; existing USB
   status logging is not MIDI or remote control.

USB readout of knobs and toggles would make recipe verification easier, but it
is diagnostic support, not a replacement for these musical behavior fixes.
