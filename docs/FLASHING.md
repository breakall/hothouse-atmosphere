# Try Atmosphere v0.3.0 — no compiling required

Download **atmosphere-v0.3.0.zip** from the
[v0.3.0 release](https://github.com/breakall/hothouse-atmosphere/releases/tag/v0.3.0)
and unzip it. Everything below runs from the unzipped folder. You don't need
Git, Make, the Arm toolchain, or a source-code checkout.

## 1. Check your pedal

- Stereo Cleveland Music Co. Hothouse with a Daisy Seed. Development hardware
  was a rev 7 Seed; this release has been hardware-tested on macOS.
- **The Daisy bootloader must already be installed.** This is a `BOOT_SRAM`
  application, not a bootloader installer or STM32 internal-flash image.
- Use a USB data cable and your pedal's normal power arrangement. Close serial
  monitors or other software using the pedal. Keep your previous firmware file
  if you want to switch back.

## 2. Install the two flashing tools

You need Python 3 and [dfu-util](https://dfu-util.sourceforge.net/).

On macOS, if you use Homebrew:

```sh
brew install python dfu-util
```

On Debian/Ubuntu Linux:

```sh
sudo apt install python3 dfu-util
```

Other Linux distributions provide these through their package manager. On
Windows, use Python 3 and the official dfu-util Windows download; dfu-util must
be on PATH and its USB device must have a compatible WinUSB/libusb driver.
See the [dfu-util setup notes](https://dfu-util.sourceforge.net/). Use `python`
instead of `python3` below if that's the name of your Python command. Windows
flashing has not been hardware-tested for this release.

## 3. Start the flasher, then enter update mode

Open a terminal in the unzipped folder and run:

```sh
python3 flash.py atmosphere-v0.3.0.bin --timeout 180
```

Wait for **Watcher ready**. Then, if your pedal already runs Atmosphere or the
Hothouse NAM player, hold **both footswitches for at least two seconds**, until
the LEDs alternate, and release. The helper will find the pedal and upload
automatically.

If your current firmware doesn't provide that gesture, tap the Daisy Seed's
RESET button while the watcher is running to enter an already installed Daisy
bootloader's short update window. See the
[official bootloader guide](https://github.com/daisyaudio/libDaisy/blob/master/doc/md/_a7_Getting-Started-Daisy-Bootloader.md)
if initial bootloader setup or a longer update window is needed.

The helper targets the QSPI application at **0x90040000**. It expects USB DFU
ID **0483:df11** with an external-memory region at **0x90000000**, matching the
development pedal's Daisy bootloader. STM32 ROM/internal-flash DFU is rejected.
Don't use a generic flasher's internal-flash address for this `.bin`.

## 4. Wait for success, then play

The terminal should report both:

```text
Download done.
File downloaded successfully
```

Let it reboot. If it stays in update mode, tap RESET or power-cycle the pedal.
Both effects start bypassed. Play a chord and tap the **right** footswitch to
catch it; tap the **left** to add the selected wet effect.

Open **atmosphere-cheat-sheet.html** from the ZIP in a browser for the panel
diagram and starting recipes.

Optional on macOS/Linux: verify the running version and see your controls:

```sh
python3 control.py get
```

The JSON should include `"version": "0.3.0"`. The optional control tool uses
POSIX serial APIs and is not a Windows serial monitor.

## If the pedal isn't found

Check the cable, power, USB permissions, and whether another app owns the
device. Start the watcher **before** entering update mode. A firmware without
the two-switch gesture may need the Seed's RESET button instead.

If the helper reports STM32 ROM DFU, or your bootloader uses a different USB
ID, it has not uploaded this application. Follow the official bootloader guide
or open an issue with your `dfu-util -l` output; this helper supports only the
DFU interface described above. On Linux, missing USB permissions may require
an appropriate udev rule for the device.

To return to another firmware, use its own installation instructions. Holding
both footswitches in Atmosphere still enters update mode independently of USB
settings.

## What's in the ZIP?

- `atmosphere-v0.3.0.bin` — compiled effects application.
- `flash.py` — flashing helper; Python 3 plus dfu-util required.
- `control.py` — optional USB control tool for macOS/Linux.
- `FLASHING.md` — these instructions.
- `atmosphere-cheat-sheet.html` — offline controls and recipes.
- `SHA256SUMS` — checksums for the included payload files.
- `BUILD_INFO.txt`, `LICENSE`, `NOTICE`, `LIBDAISY_LICENSE` — build and license details.

On macOS, verify the extracted payload with `shasum -a 256 -c SHA256SUMS`;
on Linux use `sha256sum -c SHA256SUMS`.

## Early firmware: known limits

Slip TIME can stick; frozen wet audio cannot be reshaped with MODIFY; precise
overdub alignment needs work; maximum delay feedback decays. MIDI, saved preset
slots, sync, expression and selectable stereo spread are not included. USB
settings stay active until physical takeover, clearing, or power-off.

Complete source and development notes:
[breakall/hothouse-atmosphere](https://github.com/breakall/hothouse-atmosphere).
