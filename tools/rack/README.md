# Rack check scripts

Python scripts used at the rack to check protocol details on a real H7600 (firmware 5.22) through a
USB MIDI interface whose port name contains `UM-ONE`, device id 1. They need Python 3.12 with
`mido` and `python-rtmidi` (`python3.12 -m venv venv && venv/bin/pip install mido python-rtmidi`).
Quit any app that has the MIDI port open first.

`h76.py` sends only an allow-list of codes (bank change 03 + Program Change, 0E, 0B, 18, 1A, 3C).
Everything is run-time only; nothing writes the unit's memory. `step4_sigfile_load.py` adds 0A
(load a sigfile into the running program, not saved) for that one test.

| Script | What it does |
|---|---|
| `step1_sigdbase.py` | 0E → 0C module database (about 2 minutes; the unit answers nothing else meanwhile). Saves `tests/fixtures/sigdbase_0c.syx`, which is not committed. |
| `compare_sigdbase.py` | Compares that dump with Eventide's `sigdbase.sbs` (module names and fields). |
| `sigdbase_to_json.py` | Converts the dump to plain text and JSON. |
| `step2_subfield.py` | 3C subfields 0001..0004 on 513 Ducked Delays. |
| `step3_rate.py` | 3C throughput, acked and fire-and-forget. |
| `step4_sigfile_load.py` | Sends a modified 12 Thru with 0A, reads it back with 0B. |
| `fetch_sigfiles.py` | Saves the quick sigfiles of a list of simple factory presets. |
