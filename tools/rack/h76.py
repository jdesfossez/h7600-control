"""Minimal, guarded SysEx helper for rack checks on the Eventide H7600 (UM-ONE, device 1).

Only a fixed allow-list of message codes can be sent. Nothing here writes the unit's memory.
"""
import os, re, threading, time
import mido

PORT_SUBSTR = "UM-ONE"
DEV = 1
HDR = bytes([0xF0, 0x1C, 0x70, DEV])
# 03 bank change, 0E sigdbase want, 0B sigfile quick want, 18 screen want, 1A info want, 3C param set.
ALLOWED = {0x03, 0x0E, 0x0B, 0x18, 0x1A, 0x3C}
FORBIDDEN = {0x01, 0x08, 0x0F, 0x10, 0x11, 0x13, 0x15, 0x16}

def _port(names):
    for n in names:
        if PORT_SUBSTR in n:
            return n
    raise SystemExit(f"no {PORT_SUBSTR} port in {names}")

class Unit:
    def __init__(self, log_path=None):
        self.rx = []  # (time, bytes)
        self.lock = threading.Lock()
        self.cv = threading.Condition(self.lock)
        self.log = open(log_path, "a") if log_path else None
        self.inp = mido.open_input(_port(mido.get_input_names()), callback=self._cb)
        self.out = mido.open_output(_port(mido.get_output_names()))

    def _cb(self, msg):
        b = bytes(msg.bytes()); t = time.time()
        with self.cv:
            self.rx.append((t, b)); self.cv.notify_all()
        if self.log:
            self.log.write(f"{t:.3f} RX len={len(b)} {b[:48].hex(' ')}\n"); self.log.flush()

    def mark(self):
        with self.lock:
            return len(self.rx)

    def since(self, m):
        with self.lock:
            return list(self.rx[m:])

    def _send(self, frame):
        frame = bytes(frame)
        if frame[0] == 0xF0:
            assert frame[:4] == HDR and frame[-1] == 0xF7, frame.hex(' ')
            code = frame[4]
            assert code in ALLOWED and code not in FORBIDDEN, f"code {code:02x} not allowed"
        else:
            assert frame[0] & 0xF0 == 0xC0 and len(frame) == 2, "only Program Change allowed"
        if self.log:
            self.log.write(f"{time.time():.3f} TX {frame[:48].hex(' ')}\n"); self.log.flush()
        self.out.send(mido.Message.from_bytes(list(frame)))
        return time.time()

    def sysex(self, code, payload=b""):
        return self._send(HDR + bytes([code]) + bytes(payload) + b"\xF7")

    def wait_for(self, m, pred, timeout):
        """Wait until a message after mark m satisfies pred; return (t, bytes) or None."""
        end = time.time() + timeout
        with self.cv:
            while True:
                for t, b in self.rx[m:]:
                    if pred(b):
                        return t, b
                left = end - time.time()
                if left <= 0:
                    return None
                self.cv.wait(min(left, 0.05))

    @staticmethod
    def is_code(code):
        return lambda b: len(b) >= 6 and b[:4] == HDR and b[4] == code

    # ---- high-level -------------------------------------------------------------------
    def sigfile(self, timeout=8, retries=3):
        for _ in range(retries):
            m = self.mark(); t0 = self.sysex(0x0B)
            r = self.wait_for(m, self.is_code(0x0A), timeout)
            if r:
                return r[1], r[0] - t0
        raise TimeoutError("no 0A reply to 0B")

    def lines(self, sig_bytes):
        txt = sig_bytes[5:-1].decode("latin1")
        return [l for l in txt.replace("\r", "").split("\n") if l.strip()]

    def param(self, pid_hex, value, timeout=1.0):
        """Send 3C '<pid> <value> ' and wait for 2E ack or 0D error. Returns (code, latency, msg)."""
        m = self.mark(); t0 = self.sysex(0x3C, f"{pid_hex} {value} ".encode())
        r = self.wait_for(m, lambda b: self.is_code(0x2E)(b) or self.is_code(0x0D)(b), timeout)
        if not r:
            return None, None, None
        return r[1][4], r[0] - t0, r[1]

    def load(self, number, settle=2.5):
        bank, pc = divmod(number, 100)
        self.sysex(0x03, bytes([0, 0, bank >> 4, bank & 0xF]))
        time.sleep(0.05)
        self._send(bytes([0xC0, pc]))
        time.sleep(settle)

    def close(self):
        self.inp.close(); self.out.close()

def find_line(lines, pattern):
    """1-based index of the first line matching regex pattern."""
    for i, l in enumerate(lines, 1):
        if re.search(pattern, l):
            return i, l
    return None, None
