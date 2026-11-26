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
    Error = 0x0D,
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

/// Bank change: internal (or card) bank 0..99. On fw 5.22 a following MIDI
/// Program Change is resolved through the active MIDI map (user group), not
/// this bank.
Bytes makeBankChange(std::uint8_t deviceId, bool card, int bank);

// ---- Keypress echo ------------------------------------------------------------

/// Decodes the 32-bit active-low key mask of a Keypress frame.
std::optional<std::uint32_t> keyMask(const Frame& f);
/// Names of the keys pressed in the mask ("SETUP", "SOFT1", ...).
std::vector<std::string> pressedKeys(std::uint32_t mask);

} // namespace h7600
