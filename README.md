# Atmosphere

**Catch a moment. Turn it into a landscape.**

Experimental stereo firmware for the **Cleveland Music Co. Hothouse**, inspired
by the Chase Bliss MOOD MkII. An always-listening micro-looper meets a separate
set of spatial effects. Catch something you just played, reshape it, then play
over whatever it becomes.

**Version 0.3.0 · Stereo Hothouse / Daisy Seed · Early, playable, still evolving**

## Two sides. Plenty to get lost in.

### Wet effects

- **Reverb — scattered echoes become a cloudy wash.** Keep the reflections
  distinct or blur them into a soft backdrop.
- **Delay — give a phrase somewhere to go.** Clean repeats, adjustable feedback,
  and a hold-to-freeze second layer.
- **Slip — little shards of harmony.** Granular fragments pitch up, pitch down,
  or run backwards. Glitchy at the edges; sweet in the middle.

### Micro-looper

- **Tape — rewind reality.** Shorten a phrase, reverse it, or move between
  half-speed and four-times speed.
- **Stretch — make a small sound hang around.** Crawl through a loop, scan
  backwards, or park on a grain and let it hover.
- **Envelope — your picking interrupts the loop.** Playing holds a repeating
  slice; quiet down and the loop moves on.

Feed the wet side your guitar, your loop, or both. Roll **CLOCK** back after
capturing to slow the texture, drop its pitch and roughen its edges.

## Your first atmosphere

1. Set the toggles to **Reverb / Loop routing / Tape**: left up, middle down,
   right in the middle.
2. Put **MIX** and **WET TIME** at noon, both **MODIFY** knobs around two
   o'clock, and **CLOCK** and **LOOP LENGTH** fully clockwise.
3. Play a chord, then tap the **right footswitch** while it rings. You've caught
   what you just played—the switch doesn't start a new recording.
4. Tap the **left footswitch** to wash the loop through reverb. Play over it.
   Turn CLOCK down. Switch to Stretch. See what survives.

## Know your panel

Front view, matching the numbers on the Hothouse:

```text
   1 MIX         2 WET TIME       3 WET MODIFY
   4 CLOCK       5 LOOP LENGTH    6 LOOP MODIFY

   WET MODE        ROUTING         LOOP MODE

         WET                     LOOP
```

| Knob | What it does |
| --- | --- |
| **1 · MIX** | Live guitar ↔ processed sound. |
| **2 · WET TIME** | Reverb size/decay, delay time, or Slip grain size.* |
| **3 · WET MODIFY** | Reverb smear, delay feedback, or Slip pitch/direction. |
| **4 · CLOCK** | Clockwise: cleaner and faster. Back it off: longer, lower, rougher. |
| **5 · LOOP LENGTH** | Tape loop length, or slice/grain size in Envelope and Stretch. |
| **6 · LOOP MODIFY** | Tape speed/direction, Stretch movement, or Envelope sensitivity. |

| Toggle | Up | Middle | Down |
| --- | --- | --- | --- |
| **Left · Wet mode** | Reverb | Delay | Slip |
| **Middle · Routing** | Guitar into wet | Guitar + loop into wet | Loop into wet |
| **Right · Loop mode** | Envelope | Tape | Stretch |

- **Left footswitch:** tap for wet on/off; hold to freeze the wet sound.
- **Right footswitch:** tap to catch a loop or return to listening; hold to
  overdub dry guitar onto an existing loop.
- **Both held for two seconds:** USB firmware-update mode.

Taps happen on release; holds engage after 0.4 seconds. Solid LEDs show active
sections. Blinking means freeze or overdub; a faint right-LED pulse means
listening. There is no tap-tempo function.

The [HTML field guide](docs/atmosphere-cheat-sheet.html) adds a pedal diagram
and three recipes: **Cloud bed**, **Reverse postcard**, and **Octave
constellation**. Download and open it in a browser.

## Try it on your Hothouse

You'll need a **stereo Hothouse with a Daisy Seed**, USB access, and an installed
Daisy bootloader. Development used a rev 7 Seed. Atmosphere replaces the pedal's
current effects application.

Start with the [build and installation guide](docs/BUILD.md). It covers building
the firmware, USB flashing and recovery. Use stereo cabling for both channels;
the firmware doesn't automatically duplicate a mono input.

## Set a sound over USB. Tweak it by hand.

The included tool reads and sets every knob, toggle and footswitch, plus effect
states. Settings stay put without a timer or an open computer connection.
Move a physical control and **only that control** takes over.

```sh
python3 tools/control.py get
python3 tools/control.py set mix 0.5
python3 tools/control.py clear all
```

Loops and USB settings clear at power-off. More in the [USB guide](docs/USB_DEBUG.md).

## A work in progress

Atmosphere uses its own DSP. It isn't Chase Bliss firmware or an exact MOOD clone.

- **\* Slip TIME can stick:** its grain-size control needs a fix.
- Frozen wet audio repeats, but MODIFY can't reshape it while frozen.
- Overdubbing works; precise layer alignment needs more work.
- Maximum delay feedback still fades; bypass trails aren't implemented.
- No MIDI, saved preset slots, tempo sync, expression control or selectable
  stereo spread. Maximum listening history ranges from roughly 0.5–8 seconds.

The development pedal has been played and USB control checked on hardware.
Automated audio and control tests pass. Broader playing tests are welcome.
See the [use-case audit](docs/MOOD_USE_CASE_AUDIT.md) and
[validation notes](docs/VALIDATION.md) for the details.

## Open source, independent, grateful

GPL-3.0-or-later. See [LICENSE](LICENSE) and [NOTICE](NOTICE).
Built on [Electrosmith libDaisy](https://github.com/electro-smith/libDaisy)
and the [Cleveland Music Co. Hothouse driver](https://github.com/clevelandmusicco/HothouseExamples).
Inspired by [Chase Bliss MOOD MkII](https://www.chasebliss.com/mood-mkii).
Not affiliated with or endorsed by Chase Bliss, Cleveland Music Co. or Electrosmith.
