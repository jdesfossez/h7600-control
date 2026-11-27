"""Step 3: 3C rate test on the gliderate KNOB of 513 Ducked Delays. Run-time only."""
import os, statistics, time, h76
u = h76.Unit(log_path=os.path.join(os.path.dirname(os.path.abspath(__file__)), "rack.log"))
u.load(513)
sig, _ = u.sigfile(); L = u.lines(sig)
gi, gl = h76.find_line(L, r"^KNOB gliderate "); pid = f"{gi:02x}0001"
print("gliderate line", gi, "pid", pid)
vals = [i % 101 for i in range(200)]

# a) wait for each ack
lat, acks, errs = [], 0, 0
t0 = time.time()
for v in vals:
    code, l, _ = u.param(pid, v, timeout=1.0)
    if code == 0x2E: acks += 1; lat.append(l)
    elif code == 0x0D: errs += 1
el = time.time() - t0
print(f"acked mode: {acks}/200 acks, {errs} errors, {el:.2f}s, {200/el:.0f} msg/s, "
      f"latency median {statistics.median(lat)*1000:.1f} ms, max {max(lat)*1000:.1f} ms")
s, _ = u.sigfile(); print("   final:", u.lines(s)[gi-1], "(last sent", vals[-1], ")")

# b) fire and forget
for gap in (0.002, 0.005, 0.010):
    m = u.mark(); sends = []
    t0 = time.time()
    for v in vals:
        sends.append(u.sysex(0x3C, f"{pid} {v} ".encode()))
        nxt = t0 + gap * (len(sends))
        while time.time() < nxt: pass
    tx = time.time() - t0
    time.sleep(1.5)
    got = u.since(m)
    a = [t for t, b in got if h76.Unit.is_code(0x2E)(b)]
    e = [b for t, b in got if h76.Unit.is_code(0x0D)(b)]
    other = [b[:8].hex(' ') for t, b in got if not (h76.Unit.is_code(0x2E)(b) or h76.Unit.is_code(0x0D)(b))]
    span = (a[-1] - sends[0]) if a else 0
    print(f"gap {gap*1000:.0f} ms: sent 200 in {tx:.2f}s, {len(a)} acks, {len(e)} errors, "
          f"other {other[:3]}, last ack {span:.2f}s after first send ({len(a)/span if span else 0:.0f} acks/s)")
    if e: print("   error text:", e[0][5:-1].decode('latin1'))
    s, _ = u.sigfile(); print("   final:", u.lines(s)[gi-1], "(last sent", vals[-1], ")")
u.load(513)
s, _ = u.sigfile(); print("after reload, identical to before:", s == sig)
u.close()
