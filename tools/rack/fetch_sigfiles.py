"""Load factory presets (bank change + PC) and save each quick sigfile (0B -> 0A). Read-only."""
import os, re, sys, h76
OUT = sys.argv[1] if len(sys.argv) > 1 else "sigfiles"; os.makedirs(OUT, exist_ok=True)
P = [(11, "Mute"), (12, "Thru"), (13, "Oscillator (440)"), (7010, "Empty Program"), (319, "Stereo Filter"),
     (316, "Dual Delays"), (314, "Compressor_S"), (312, "BasicRoom"), (317, "Simple Moddelays"), (311, "2 Pitchshifters")]
norm = lambda s: re.sub(r"[^a-z0-9]", "", s.lower())
u = h76.Unit(log_path=os.path.join(os.path.dirname(os.path.abspath(__file__)), "rack.log"))
for num, name in P:
    for attempt in (1, 2):
        u.load(num); s, _ = u.sigfile()
        title = re.search(r'^HEADM \S+ \S+ \S+ \S+ \S+ "([^"]*)"', u.lines(s)[0]).group(1)
        if norm(title) == norm(name): break
    fn = f"{num:04d}_{re.sub(r'[^A-Za-z0-9]+', '_', name).strip('_')}"
    open(f"{OUT}/{fn}.syx", "wb").write(s)
    open(f"{OUT}/{fn}.sig", "wb").write(s[5:-1].replace(b"\x00", b""))
    print(f"=== {num} {name!r}: HEADM title {title!r} match={norm(title)==norm(name)} attempts={attempt} {len(s)} B -> {fn}.sig")
    for l in u.lines(s): print("   ", l[:150])
u.load(4208); s, _ = u.sigfile(); print("restored:", u.lines(s)[0][:60])
sys.stdout.flush(); os._exit(0)
