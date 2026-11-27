"""Step 2: 3C subfield test on 513 Ducked Delays. Run-time only; reloads the program at the end."""
import os, re, time, h76
FIX = os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../tests/fixtures")
u = h76.Unit(log_path=os.path.join(os.path.dirname(os.path.abspath(__file__)), "rack.log"))

def show(r):
    code, lat, msg = r
    if code is None: return "no reply (1 s)"
    if code == 0x2E: return f"2E ack in {lat*1000:.1f} ms"
    return f"0D error in {lat*1000:.1f} ms: {msg[5:-1].decode('latin1')!r}"

u.load(513)
sig, dt = u.sigfile(); L = u.lines(sig)
print("loaded:", L[0][:70], f"(0B reply {len(sig)} B in {dt:.2f}s)")
pass  # reference: tests/fixtures/sigfile_quick_0a_ducked_delays.syx
mi, ml = h76.find_line(L, r"^MIX mix1 adc-in1 delay1-out 1\.0000 feedback1-out")
fi, fl = h76.find_line(L, r"^PERCENTKNOB feedback1 ")
gi, gl = h76.find_line(L, r"^KNOB gliderate ")
print(f"MIX mix1 line {mi} (0x{mi:02x}); feedback1 line {fi} (0x{fi:02x}); gliderate line {gi} (0x{gi:02x})")

r = u.param(f"{fi:02x}0001", 30); print(f"feedback1 {fi:02x}0001 30 ->", show(r))
s, _ = u.sigfile(); print("   now:", u.lines(s)[fi-1])

for sub in range(1, 5):
    pid = f"{mi:02x}{sub:04x}"
    r = u.param(pid, 0.5); print(f"MIX {pid} 0.5 ->", show(r))
    s, _ = u.sigfile(); print("   now:", u.lines(s)[mi-1])

pid = f"{gi:02x}0002"
r = u.param(pid, 50); print(f"KNOB gliderate {pid} 50 ->", show(r))
s, _ = u.sigfile(); print("   now:", u.lines(s)[gi-1])

u.load(513)
s, _ = u.sigfile()
print("after reload, identical to first capture:", s == sig)
u.close()
