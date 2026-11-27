"""Step 4: send a modified '12 Thru' sigfile with 0A (SIGFILE_DUMP_REMOTE: compile and load, run time
only, per Tech Note 34). Approved by Julien for this test only. Then 0B, 1A, one screen dump, reload."""
import os, time, h76
HERE = os.path.dirname(os.path.abspath(__file__)); FIX = os.path.join(HERE, "../../tests/fixtures")
u = h76.Unit(log_path=os.path.join(HERE, "rack.log"))

SIG = ("\r\n"
       'HEADM adc 2 2 highcut1-out highcut2-out "Thru HC test" "" 2 info-obj h-obj\r\n'
       'TEXTBLOCK info 3 "The preset\'s input is electronically" "connected to the output. Stereo in," "stereo out."\r\n'
       'KNOB h "post hicut: %5.0f Hz" "" 2000 20000 1 2000\r\n'
       "HIGHCUT highcut1 adc-in1 h-out\r\n"
       "HIGHCUT highcut2 adc-in2 h-out\r\n"
       "TAIL tail\r\n").encode("latin1") + b"\x00"

u.load(12)
s, _ = u.sigfile(); print("loaded:", u.lines(s)[0])
open(os.path.join(FIX, "sigfile_0a_thru_hc_test_sent.syx"), "wb").write(h76.HDR + b"\x0A" + SIG + b"\xF7")
h76.ALLOWED.add(0x0A)   # this step only
m = u.mark(); t0 = u.sysex(0x0A, SIG); h76.ALLOWED.discard(0x0A)
print("TX 0A sigfile,", len(SIG) + 6, "bytes")
r = u.wait_for(m, lambda b: len(b) >= 6 and b[:4] == h76.HDR and b[4] in (0x00, 0x0D), 15)
time.sleep(0.5)
for t, b in u.since(m):
    print(f"  RX +{t - t0:.3f}s code {b[4]:02x} len {len(b)} ascii {b[5:-1].decode('latin1')!r}" if b[:4] == h76.HDR else f"  RX {b.hex(' ')}")
if not r:
    print("no 00/0D reply in 15 s")
ok = r and r[1][4] == 0x00
s2, _ = u.sigfile(); open(os.path.join(FIX, "sigfile_quick_0a_thru_hc_test_after.syx"), "wb").write(s2)
print("0B after:"); [print("   ", l) for l in u.lines(s2)]
m = u.mark(); u.sysex(0x1A); i = u.wait_for(m, h76.Unit.is_code(0x19), 5)
print("1A:", "reply", len(i[1]), "B" if i else "no reply")
if i: open(os.path.join(FIX, "info_dump_19_after_0a.syx"), "wb").write(i[1])
m = u.mark(); u.sysex(0x18); sc = u.wait_for(m, h76.Unit.is_code(0x17), 6)
if sc:
    open(os.path.join(FIX, "screen_dump_17_thru_hc_test.syx"), "wb").write(sc[1]); print("screen dump", len(sc[1]), "B saved")
before = open(os.path.join(FIX, "sigfile_quick_0a_ducked_delays.syx"), "rb").read()
u.load(513); s3, _ = u.sigfile(); print("reloaded 513; 0B identical to earlier capture:", s3 == before)
u.close()
