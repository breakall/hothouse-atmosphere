# MOOD MkII reference for the Atmosphere audit

Reviewed 2026-10-30. This is a primary-source capability reference, not a popularity ranking or a claim of sonic equivalence. The accompanying Atmosphere audit must establish what its code actually does; shared mode names alone do not establish equivalence.

## Source scope

The [current product page](https://www.chasebliss.com/mood-mkii) still links the main manual below, uploaded in March 2023. Its MIDI link redirects to a newer upload dated July 2026. Upload timestamps are not firmware version numbers; these sources do not identify the firmware installed on a particular MOOD.

- [Main manual](https://static1.squarespace.com/static/622176a9b8d15d57ffbf5700/t/64111ea7c3f6d569218566b4/1678843563430/MOOD%2BMKII_Manual_Pedal_Chase%2BBliss.pdf): printed pages cited below; printed pp28–29 are PDF page 16. The clock diagram was visually checked in the original PDF.
- [Current MIDI manual](https://static1.squarespace.com/static/622176a9b8d15d57ffbf5700/t/6a6baf5dd753aa52bddd8a30/1785442141195/MOOD%2BMKII_MIDI%2BManual_Pedal_Chase%2BBliss.pdf).
- [Official feature summary](https://static1.squarespace.com/static/622176a9b8d15d57ffbf5700/t/64111e44b7e4db6a404ad61f/1678843461129/MOOD%2BMKII_Whats%2BNew_Pedal_Chase%2BBliss.pdf).

## Core workflows documented in the main manual

All rows below summarize the [main manual](https://static1.squarespace.com/static/622176a9b8d15d57ffbf5700/t/64111ea7c3f6d569218566b4/1678843563430/MOOD%2BMKII_Manual_Pedal_Chase%2BBliss.pdf).

| Workflow | Required behavior | Printed pages |
|---|---|---|
| Ambient bed | Capture recent audio; process loop, live input, or both. | 28–29, 40–41 |
| Reverb texture | MODIFY morphs discrete echoes into smear. | 24 |
| Two simultaneous loops | Freeze delay alongside micro-loop; maximum feedback sustains repeats. TIME changes avoid pitch bends. | 22–25 |
| Slip remix | Resample live audio; semitone playback steps, forward/reverse. Frozen Slip remains pitch-editable. | 22–27 |
| Tape | Forward/reverse speeds 0.5×, 1×, 2×, 4×; shorten captured material. | 32–33 |
| Env stutter | Input above threshold repeats the current slice until input subsides. | 32 |
| Stretch drone | Approach noon to stall progression; LENGTH edits frozen slice. | 34–35 |
| Trail Catcher | Bypass/re-engage looper to capture wet tails, regardless routing. Overdubs record clean input only. | 28–31, 40–41 |
| Wet-only loop treatment | Routed loop replaces its dry version; optional direct-loop blend. | 40–41 |
| Stereo movement | Tape: opposite directions per side; delay: ping-pong; Slip: panning. | 36–39 |
| Musical clock | 11 steps, 2–64 kHz; full-loop endpoints 32–1 seconds. Lowering clock retimes both channels. | 18–21, 28 |
| Performance | Optional latching holds, bypass trails, continuous clock, internal modulation. | 44–47 |

## MkII expansion workflows

The [official feature summary](https://static1.squarespace.com/static/622176a9b8d15d57ffbf5700/t/64111e44b7e4db6a404ad61f/1678843461129/MOOD%2BMKII_Whats%2BNew_Pedal_Chase%2BBliss.pdf) documents these additional audit targets:

- Build layers through overdubbing while retaining captures when switching looper modes.
- Fade older loop material progressively, producing evolving textures or delay-like repeats.
- Refresh the looper continuously for delayed live stretching and warping.
- Synchronize one channel's timing to the other, in either direction.
- Balance channel volumes, darken wet effects with a high-cut, and blend unprocessed loop audio.
- Generate stereo motion from mono material; use forward or reverse stretching.
- Select Classic behavior for intentional noise, loop deterioration, and bendier delay changes.

These are documented possibilities, not evidence of which settings users choose most frequently.

## Presets, external timing, and instrument use

The [MIDI manual](https://static1.squarespace.com/static/622176a9b8d15d57ffbf5700/t/6a6baf5dd753aa52bddd8a30/1785442141195/MOOD%2BMKII_MIDI%2BManual_Pedal_Chase%2BBliss.pdf) establishes:

- Up to 122 preset slots; the first two correspond to the pedal's preset toggle.
- External MIDI clock and independently selectable channel divisions; MIDI tap tempo via CC107 or CC93.
- CC control of knobs, hidden settings, footswitch functions, and DIP options.
- Keyboard/sequencer notes transpose the clock in semitones in monophonic Synth Mode. Output can drone continuously, gate with notes, or follow ADSR; pitch bend, velocity, and portamento expand performance control. MIDI clock is ignored in Synth Mode.

The [product specifications](https://www.chasebliss.com/mood-mkii) additionally identify expression/CV control, internal modulation, stereo I/O, two onboard presets, analog dry-through, and buffered bypass. These are separate integration requirements from the six audio modes.

## Interpretation for testing

Suggested audit scenarios are original test design: capture a chord and play dry melody over its ambience; freeze one channel while altering the other; trigger stutters with quiet/loud playing; resample a reverb tail; change loop mode without recapture; change clock while tails ring; add successive layers; and test two different stereo inputs. Listening against a real MOOD or controlled recordings is still necessary to assess timbre, transient behavior, and perceptual quality.
