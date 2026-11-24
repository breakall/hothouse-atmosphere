"""Exercise the real host transport against a fragmented serial peer."""
import importlib.util
import json
import os
from pathlib import Path
import pty
import select
import threading
import unittest

spec = importlib.util.spec_from_file_location("control", Path(__file__).resolve().parents[1] / "tools/control.py")
control = importlib.util.module_from_spec(spec)
spec.loader.exec_module(control)


class ControlTest(unittest.TestCase):
    def test_values(self):
        self.assertEqual(control.value_for("wet_mode", "slip"), "2")
        self.assertEqual(control.value_for("fs1", "pressed"), "1")
        self.assertEqual(control.value_for("mix", ".25"), "0.25")
        for value in ("nan", "inf", "1.5", "-.1", "1\nset mix 0"):
            with self.assertRaises(ValueError):
                control.value_for("mix", value)

    def test_fragmentation_and_applied_reply(self):
        master, slave = pty.openpty()
        pedal = control.Pedal(os.ttyname(slave))
        stop = threading.Event()
        seen = []

        def peer():
            buffer = bytearray()
            applied = 0
            replies = []
            while not stop.is_set():
                if not select.select([master], [], [], .05)[0]:
                    continue
                buffer.extend(os.read(master, 4096))
                while b"\n" in buffer:
                    line, _, buffer = buffer.partition(b"\n")
                    seen.append(line.decode())
                    if line.startswith(b"set "):
                        replies = [{"type": "state", "applied": 0},
                                   {"type": "ack", "id": 7, "queued": True}]
                        applied = 7
                    else:
                        replies = [{"type": "state", "version": "0.2.1", "applied": applied,
                                    "request": int(line.split()[1]),
                                    "physical": {"mix": .25}, "effective": {"mix": .5}}]
                    for reply in replies:
                        data = json.dumps(reply).encode() + b"\r\n"
                        os.write(master, data[:11])
                        os.write(master, data[11:])

        thread = threading.Thread(target=peer)
        thread.start()
        try:
            self.assertEqual(pedal.get()["version"], "0.2.1")
            state = pedal.change("set mix .5")
            self.assertEqual(state["applied"], 7)
            self.assertIn("set mix .5", seen)
        finally:
            stop.set()
            thread.join(timeout=1)
            pedal.close()
            os.close(slave)
            os.close(master)


if __name__ == "__main__":
    unittest.main()
