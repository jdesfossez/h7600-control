# H7600 Control

An AU / VST3 / Standalone remote control for the **Eventide H7600** (firmware 5.22) over MIDI SysEx.
The plugin does no audio processing (audio passes through untouched). It opens the
MIDI interface wired to the H7600 itself, because hosts are unreliable at passing large SysEx.

Status: scaffold. The core protocol library is tested against real replies captured from a unit;
the UI is a placeholder that connects, requests the current program's sigfile and lists its knobs.

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
* Bank change `03` + MIDI Program Change: the program change follows the active MIDI map
  (user group), not the bank.

**Safety:** the core library has no builders for codes that write the unit's memory
(`08`, `0A`, `0F`, `11`, `13`, `15`, `16`) or for keypresses; `makeWant` rejects them.

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
Set SEQUENCE OUT to `new` for two-way knob updates.

## Regenerating the preset catalog

```bash
pdftotext -layout H7600V522PresetsManual.pdf presets.txt
python3 tools/parse_presets.py presets.txt resources/presets.json   # prints "75 banks, 1114 presets"
```
