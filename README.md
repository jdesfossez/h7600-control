# H7600 Control

An AU / VST3 / Standalone remote control for the **Eventide H7600** (firmware 5.22) over MIDI SysEx.
The plugin does no audio processing (audio passes through untouched). It opens the
MIDI interface wired to the H7600 itself, because hosts are unreliable at passing large SysEx.

Features (all run time only; nothing is ever saved to the unit):

* **Browse**: the 1114 factory presets by bank family, instant ranked search with typo tolerance,
  effect/source filters with live counts, LCD-style details, favorites, your own tags and recents
  (stored on the Mac), Enter or Load to load the preset on the unit (confirmed by reading it back).
* **Parameters**: controls built from the loaded program (its own menu pages, ranges and value
  formats); moves are sent live, coalesced and acknowledged; front-panel changes move the controls.
* **LCD**: a live mirror of the unit's screen.
* Follows program changes made on the front panel, survives the unit power-cycling and the MIDI
  interface being unplugged.

Developer tool: `build/plugin/H7600Snapshot_artefacts/Release/H7600Snapshot <dir>` renders the
editor to PNGs; `--live <number>...` loads programs on the unit through the browser path, and
`--param <knob> <value>` moves a control through the UI path and reads the value back.

## Layout

| Path | What |
|------|------|
| `core/` | Pure C++20 library, no JUCE: SysEx framing, nibble encode/decode, message codes, request/reply matching with timeouts, quick-sigfile parser, parameter set, bank change, keypress echo decode, LCD screen dump decode. |
| `plugin/` | JUCE 8 plugin (AU, VST3, Standalone): `H7600Link` owns the MIDI ports; placeholder editor. |
| `tests/` | Catch2 unit tests; `tests/fixtures/*.syx` are raw messages captured from an H7600 v5.22. |
| `resources/presets.json` | Factory preset catalog (75 banks, 1114 presets) parsed from the v5.22 Presets Manual. |
| `tools/parse_presets.py` | Regenerates the catalog. |

## Protocol notes (verified on fw 5.22)

Frame: `F0 1C 70 <device id> <code> <payload> F7` (device id 0 = broadcast; the test unit uses 1).

| Request | Reply | Notes |
|---|---|---|
| `1A` info want | `19` | ASCII, NUL-terminated |
| `06` program want | `15` | nibblized binary, ~10.8 KB, ~4.5 s |
| `0B` sigfile want quick | `0A` | ASCII sigfile of the loaded program, ~2.3 s |
| `07` setup want | `16` | nibblized |
| `18` screen want | `17` | nibblized: width, height, size (32-bit BE each), 240x64 1-bpp bitmap, checksum |
| `3C` param set | `2E` ack | ASCII `"<param id> <value> "`, e.g. `1e0001 50 ` |

* **Parameter id** = 1-based index of the module in the quick sigfile, two lowercase hex digits,
  then `0001` (meaning of `0001` not yet known). Changes are run-time only (not saved).
* With SETUP > MIDI > SEQUENCE OUT = `new`, the unit emits `3C` messages when knobs are turned
  and `01` keypress messages (active-low 32-bit key mask, as nibbles) for front-panel keys.
* **Loading a program by number** (`makeProgramSelect`): SysEx bank change `03` (nibblized
  `00 00 <bank>`) then MIDI Program Change `<slot>`, e.g. `F0 1C 70 01 03 00 00 02 0A F7` + `C0 0A`
  loads 4210. Requires SETUP > MIDI > **MIDI map = none**; with a map (default "Factory" = user
  group 1) the PC goes through the map and the bank is ignored.

**Safety:** the core library has no builders for codes that write the unit's memory
(`08`, `0A`, `0F`, `11`, `13`, `15`, `16`); `makeWant` rejects them. Key presses (`01`) can only
be built for navigation-safe keys (`SafeKey`: PROGRAM, 0-9, ENTER, SELECT, CXL).

`makeLoadProgram` (key-press emulation) is a fallback for when a MIDI map is active: PROGRAM,
digits, ENTER moves the list cursor to the program; the final load key is unconfirmed. Replaying
the panel's `3C 4005xxxx` messages does not load a given number (they are list-cursor/"load
selection" actions relative to the list on screen).

## Build (macOS)

Requirements: CMake ≥ 3.24, a C++20 compiler (Xcode or Command Line Tools), Ninja (`brew install ninja`) or Xcode.
JUCE 8.0.15 and Catch2 3.16.1 are fetched automatically by CMake.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Or generate an Xcode project: `cmake -S . -B build-xcode -G Xcode` (requires full Xcode).

Artefacts land in `build/plugin/H7600Control_artefacts/Release/`:
`AU/H7600 Control.component`, `VST3/H7600 Control.vst3`, `Standalone/H7600 Control.app`.

Install for Logic: copy the `.component` to `~/Library/Audio/Plug-Ins/Components/`, then

```bash
auval -v aufx H76c H7ct
```

Core + tests only (no JUCE download): `cmake -S . -B build-tests -DH7600_BUILD_PLUGIN=OFF`.

## Unit setup

On the H7600, SETUP > MIDI: MIDI enabled, SYSTEM EXCLUSIVE on, note the DEVICE ID.
Set SEQUENCE OUT to `new` for two-way knob updates, and MIDI map to `none` to load programs by number.

## Regenerating the preset catalog

```bash
pdftotext -layout H7600V522PresetsManual.pdf presets.txt
python3 tools/parse_presets.py presets.txt resources/presets.json   # prints "75 banks, 1114 presets"
```

## Third-party assets

* `resources/fonts/VT323-Regular.ttf`: VT323 by The VT323 Project Authors, from
  [google/fonts](https://github.com/google/fonts/tree/main/ofl/vt323), licensed under the
  SIL Open Font License 1.1 (`resources/fonts/OFL.txt`). Used for the LCD-style text.
