#pragma once
// Eventide H7600 (DSP4000/7000 family) MIDI System Exclusive protocol.
//
// Frame: F0 1C 70 <device id> <code> <payload...> F7
// Device id 0 is broadcast. Many payloads are "nibblized": each 8-bit byte is
// sent as two 7-bit-safe bytes, high nibble first.
//
// Everything here was verified against a real H7600 running firmware 5.22
// (see tests/fixtures). Codes that would write the unit's memory are listed for
// decoding only; this library deliberately offers no builder for them.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace h7600
{
using Bytes = std::vector<std::uint8_t>;

constexpr std::uint8_t kSysexStart = 0xF0;
constexpr std::uint8_t kSysexEnd = 0xF7;
constexpr std::uint8_t kEventideId = 0x1C;
constexpr std::uint8_t kModelId = 0x70;
constexpr std::uint8_t kBroadcastDevice = 0x00;

enum class Code : std::uint8_t
{
    Ok = 0x00,
    Keypress = 0x01,        // unit -> host when SEQUENCE OUT is on (8 nibbles, active-low key mask)
    UserObject = 0x02,
    BankChange = 0x03,      // host -> unit: 4 nibbles (0=internal/1=card, bank 0..99)
    ProgramWant = 0x06,     // -> ProgramDump (0x15)
    SetupWant = 0x07,       // -> SetupDump (0x16)
    SigfileDump = 0x08,     // WRITES/LOADS a program: never sent by this library
    SigfileWant = 0x09,     // -> SigfileDump (0x08), commented
    SigfileDumpRemote = 0x0A, // reply to SigfileWantQuick (also a write when sent to the unit)
    SigfileWantQuick = 0x0B,  // -> SigfileDumpRemote (0x0A), plain ASCII
    SigdbaseDump = 0x0C,    // reply to SigdbaseWant: the module database, plain ASCII
    Error = 0x0D,
    SigdbaseWant = 0x0E,    // -> SigdbaseDump (0x0C): about 286 KB in about 2 minutes on fw 5.22;
                            // the unit answers nothing else until it has sent it
    ProgramDump = 0x15,
    SetupDump = 0x16,
    ScreenDump = 0x17,
    ScreenWant = 0x18,      // -> ScreenDump (0x17)
    InfoDump = 0x19,
    InfoWant = 0x1A,        // -> InfoDump (0x19), plain ASCII
    ParamAck = 0x2E,        // unit's reply to ParamSet (observed on fw 5.22)
    ParamSet = 0x3C,        // ASCII "<param id> <value> " (observed on fw 5.22)
};

struct Frame
{
    std::uint8_t deviceId = 0;
    std::uint8_t code = 0;
    Bytes payload; // bytes between the code and F7
};

/// Builds F0 1C 70 <dev> <code> <payload> F7.
Bytes buildFrame(std::uint8_t deviceId, Code code, const Bytes& payload = {});

/// Parses a complete Eventide sysex message. Returns nullopt for anything else
/// (including stray real-time bytes such as 0xFF that the interface delivers).
std::optional<Frame> parseFrame(const std::uint8_t* data, std::size_t size);
inline std::optional<Frame> parseFrame(const Bytes& b) { return parseFrame(b.data(), b.size()); }

/// High nibble first. denibblize ignores a trailing odd nibble.
Bytes nibblize(const Bytes& in);
Bytes denibblize(const Bytes& in);

/// Payload as text (non-nibblized ASCII messages), stopping at a NUL.
std::string payloadText(const Frame& f);

// ---- Request builders (read-only / run-time only) ---------------------------

/// A "want" request. Only codes that read from the unit are accepted;
/// anything else throws std::invalid_argument.
Bytes makeWant(std::uint8_t deviceId, Code want);

/// Expected reply code for a want / command, if any.
std::optional<Code> replyCodeFor(Code request);

/// Parameter id as used by ParamSet: the 1-based index of the module in the
/// quick sigfile, as two lowercase hex digits, followed by "0001".
/// e.g. module 30 (Km_dlys in "H3000 Breathing Canyon") -> "1e0001".
std::string paramIdForModule(int moduleIndex);

/// F0 1C 70 <dev> 3C "<paramId> <value> " F7  (run-time change, not saved).
Bytes makeParamSet(std::uint8_t deviceId, std::string_view paramId, std::string_view value);

struct ParamChange
{
    std::string paramId;
    std::string value;
    std::optional<int> moduleIndex() const;
};
std::optional<ParamChange> parseParamSet(const Frame& f);

/// Bank change: internal (or card) bank 0..99. Selects the bank used by a
/// following MIDI Program Change, but only when SETUP > MIDI > "MIDI map" is
/// "none". With a map (default "Factory" = user group 1) the PC goes through
/// the map and the bank is ignored.
Bytes makeBankChange(std::uint8_t deviceId, bool card, int bank);

/// Plain MIDI Program Change, channel 0..15 (H7600 receives on its MIDI channel).
Bytes makeProgramChange(int channel, int program);

/// Loads program `number` (bank*100 + slot, slot 0..99) at run time:
/// SysEx bank change then MIDI Program Change. Requires "MIDI map" = none.
/// Verified on fw 5.22: bank 42 + PC 10 loads 4210, bank 5 + PC 13 loads 513.
std::vector<Bytes> makeProgramSelect(std::uint8_t deviceId, int channel, int number);

// ---- Keypress echo ------------------------------------------------------------

/// Decodes the 32-bit active-low key mask of a Keypress frame.
std::optional<std::uint32_t> keyMask(const Frame& f);
/// Names of the keys pressed in the mask ("SETUP", "SOFT1", ...).
std::vector<std::string> pressedKeys(std::uint32_t mask);

// ---- Program load by number (key-press emulation) ----------------------------
//
// Prefer makeProgramSelect. Partly verified on fw 5.22: PROGRAM, digits, ENTER
// moves the program-list cursor to the program but does not load it; one more
// key (probably SELECT) is needed and is unconfirmed. Only navigation-safe keys
// are exposed (PROGRAM, 0-9, ENTER, SELECT, CXL); keys that can reach setup,
// save or delete screens (SETUP, soft keys, ...) have no builder.

enum class SafeKey : std::uint32_t
{
    None = 0xFFFFFFFF,
    Program = 0xF7FFFFFF,
    Enter = 0xFFFFFFEF,
    Select = 0xFFFFFEFF,
    Cancel = 0xFFFFFFDF,
    Zero = 0xFFEFFFFF,
    One = 0x7FFFFFFF,
    Two = 0xFF7FFFFF,
    Three = 0xFFFF7FFF,
    Four = 0xBFFFFFFF,
    Five = 0xFFBFFFFF,
    Six = 0xFFFFBFFF,
    Seven = 0xDFFFFFFF,
    Eight = 0xFFDFFFFF,
    Nine = 0xFFFFDFFF,
};

/// One Keypress frame (8 nibbles, high first). SafeKey::None is the release.
Bytes makeKeyFrame(std::uint8_t deviceId, SafeKey key);

/// Key sequence to load program `number` (1..9999): PROGRAM, digits, ENTER,
/// optionally SELECT. Each key is a press followed by a release, so the result
/// alternates press/release frames; send them ~100 ms apart.
std::vector<Bytes> makeLoadProgram(std::uint8_t deviceId, int number, bool withSelect = false);

} // namespace h7600
