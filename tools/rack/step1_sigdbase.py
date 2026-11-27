"""Step 1: request the sigfile database (0E) and capture the 0C reply."""
import os, sys, time, h76
FIX = os.path.join(os.path.dirname(__file__), "../../tests/fixtures")
u = h76.Unit(log_path=os.path.join(FIX, "../../tools/rack/rack.log"))
m = u.mark(); t0 = u.sysex(0x0E); print("TX F0 1C 70 01 0E F7")
last_n, last_change, first = 0, time.time(), None
while True:
    time.sleep(0.5)
    got = u.since(m)
    n = sum(len(b) for _, b in got)
    if got and first is None:
        first = got[0][0]; print(f"first bytes after {first - t0:.2f}s")
    if n != last_n:
        last_n, last_change = n, time.time()
    done = any(h76.Unit.is_code(0x0C)(b) for _, b in got)
    if done or time.time() - last_change > 300:
        break
big = [b for _, b in got if h76.Unit.is_code(0x0C)(b)]
if big:
    open(os.path.join(FIX, "sigdbase_0c.syx"), "wb").write(big[0])
    print("saved", len(big[0]), "bytes; total time", f"{got[-1][0] - t0:.1f}s")
for i, (t, b) in enumerate(got):
    print(f"[{i}] +{t - t0:.2f}s len={len(b)} {b[:16].hex(' ')} ... {b[-4:].hex(' ')}")
u.close()
