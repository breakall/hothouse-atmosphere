#!/usr/bin/env python3
"""Read and set Atmosphere controls via USB CDC (POSIX, no packages)."""
import argparse
import glob
import json
import math
import os
import select
import termios
import time

MODES = {
    "wet_mode": ["reverb", "delay", "slip"],
    "routing": ["input", "both", "loop"],
    "loop_mode": ["envelope", "tape", "stretch"],
}
KNOBS = ["mix", "time", "wet_modify", "clock", "length", "loop_modify"]
FIELDS = KNOBS + list(MODES) + ["fs1", "fs2", "wet_on", "loop_on", "freeze", "overdub"]


def find_port():
    known = glob.glob("/dev/cu.usbmodem346637623233*")
    candidates = known or glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/ttyACM*")
    if len(candidates) != 1:
        raise RuntimeError("Specify --port; expected one Hothouse serial device, found " + repr(candidates))
    return candidates[0]


class Pedal:
    def __init__(self, port):
        self.fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        self.buffer = bytearray()
        self.request = time.time_ns() & 0xFFFFFFFF
        self.original = termios.tcgetattr(self.fd)
        cfg = termios.tcgetattr(self.fd)
        cfg[0] = cfg[1] = cfg[3] = 0
        cfg[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
        cfg[4] = cfg[5] = termios.B115200
        cfg[6][termios.VMIN] = cfg[6][termios.VTIME] = 0
        termios.tcsetattr(self.fd, termios.TCSANOW, cfg)

    def close(self):
        try:
            termios.tcsetattr(self.fd, termios.TCSANOW, self.original)
        except OSError:
            pass
        os.close(self.fd)

    def send(self, text):
        pending = (text + "\n").encode("ascii")
        deadline = time.monotonic() + 3
        while pending:
            if time.monotonic() >= deadline:
                raise TimeoutError("USB write timed out")
            if select.select([], [self.fd], [], .1)[1]:
                try:
                    pending = pending[os.write(self.fd, pending):]
                except BlockingIOError:
                    pass

    def read(self, timeout=3):
        deadline = time.monotonic() + timeout
        while True:
            if b"\n" in self.buffer:
                line, _, self.buffer = self.buffer.partition(b"\n")
                try:
                    return json.loads(line)
                except (json.JSONDecodeError, UnicodeDecodeError):
                    # Partial startup data or an older firmware's status line.
                    continue
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("No JSON reply; this interface requires Atmosphere 0.2.1 or newer")
            if select.select([self.fd], [], [], remaining)[0]:
                chunk = os.read(self.fd, 4096)
                if not chunk:
                    raise ConnectionError("Pedal disconnected")
                self.buffer.extend(chunk)
                if len(self.buffer) > 65536:
                    raise RuntimeError("Oversized USB response")

    def receive_until(self, predicate):
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            response = self.read(deadline - time.monotonic())
            if response.get("type") == "error":
                raise RuntimeError(response["error"])
            if predicate(response):
                return response
        raise TimeoutError("Expected USB response not received")

    def get(self, applied=0):
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            self.request = ((self.request + 1) & 0xFFFFFFFF) or 1
            request = self.request
            self.send(f"get {request}")
            state = self.receive_until(lambda r: r.get("type") == "state" and r.get("request") == request)
            if state["applied"] >= applied:
                return state
        raise TimeoutError("Audio callback did not apply command")

    def change(self, command):
        self.send(command)
        reply = self.receive_until(lambda r: r.get("type") == "ack" and "id" in r)
        return self.get(reply["id"])


def value_for(field, value):
    if field in MODES and value in MODES[field]:
        return str(MODES[field].index(value))
    if value in ("on", "pressed", "true"):
        return "1"
    if value in ("off", "released", "false"):
        return "0"
    v = float(value)
    limit = 1 if field not in MODES else 2
    if not math.isfinite(v) or not 0 <= v <= limit or (field not in KNOBS and v != int(v)):
        raise ValueError("Control value out of range")
    return str(v) if field in KNOBS else str(int(v))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Serial path; automatically finds a single Hothouse")
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("get", help="Read physical positions, effective values, overrides and effect state")
    watch = sub.add_parser("watch", help="Stream live control values as JSON until Ctrl-C")
    watch.add_argument("--interval", type=int, default=200, help="100–10000 ms (default 200)")
    set_parser = sub.add_parser("set", help="Override one control; knobs 0..1, toggles 0..2, states/buttons 0/1")
    set_parser.add_argument("field", choices=FIELDS)
    set_parser.add_argument("value")
    clear = sub.add_parser("clear", help="Release one override or all overrides")
    clear.add_argument("field", nargs="?", default="all", choices=FIELDS + ["all"])
    tap = sub.add_parser("tap", help="Simulate a short press and release, then restore the physical switch")
    tap.add_argument("field", choices=["fs1", "fs2"])
    sub.add_parser("dfu", help="Enter the Daisy bootloader for a firmware update")
    args = parser.parse_args()
    pedal = Pedal(args.port or find_port())
    try:
        # Verify protocol identity before sending any state-changing command.
        state = pedal.get()
        if not isinstance(state.get("version"), str) or "physical" not in state:
            raise RuntimeError("Unexpected device protocol")
        if args.command == "get":
            print(json.dumps(state, indent=2))
        elif args.command == "set":
            print(json.dumps(pedal.change(f"set {args.field} {value_for(args.field, args.value)}"), indent=2))
        elif args.command == "clear":
            print(json.dumps(pedal.change("clear " + args.field), indent=2))
        elif args.command == "tap":
            try:
                pedal.change(f"set {args.field} 1")
                time.sleep(.1)
                pedal.change(f"set {args.field} 0")
            finally:
                state = pedal.change("clear " + args.field)
            print(json.dumps(state, indent=2))
        elif args.command == "dfu":
            pedal.send("dfu")
            print("DFU requested; use tools/flash.py to download the firmware.")
        elif args.command == "watch":
            pedal.send(f"watch {args.interval}")
            pedal.receive_until(lambda r: r.get("type") == "ack" and r.get("watch"))
            try:
                while True:
                    try:
                        reply = pedal.read(timeout=.5)
                        if reply.get("type") == "state":
                            print(json.dumps(reply, separators=(",", ":")), flush=True)
                    except TimeoutError:
                        pass
            except KeyboardInterrupt:
                pedal.send("watch 2000")
                print("\nStopped. Settings stay active until physical takeover, clear, or power-off.")
    finally:
        pedal.close()


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError, TimeoutError) as error:
        raise SystemExit(str(error))
