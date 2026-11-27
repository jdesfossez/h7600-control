#include "h7600/Protocol.h"

#include <cstdio>
#include <stdexcept>

namespace h7600
{
Bytes buildFrame(std::uint8_t deviceId, Code code, const Bytes& payload)
{
    Bytes out;
    out.reserve(payload.size() + 6);
    out.push_back(kSysexStart);
    out.push_back(kEventideId);
    out.push_back(kModelId);
    out.push_back(deviceId & 0x7F);
    out.push_back(static_cast<std::uint8_t>(code));
    for (auto b : payload)
    {
        if (b & 0x80)
            throw std::invalid_argument("sysex payload byte has high bit set");
        out.push_back(b);
    }
    out.push_back(kSysexEnd);
    return out;
}

std::optional<Frame> parseFrame(const std::uint8_t* d, std::size_t n)
{
    if (n < 6 || d[0] != kSysexStart || d[n - 1] != kSysexEnd)
        return std::nullopt;
    if (d[1] != kEventideId || d[2] != kModelId)
        return std::nullopt;
    Frame f;
    f.deviceId = d[3];
    f.code = d[4];
    f.payload.assign(d + 5, d + n - 1);
    return f;
}

Bytes nibblize(const Bytes& in)
{
    Bytes out;
    out.reserve(in.size() * 2);
    for (auto b : in)
    {
        out.push_back(static_cast<std::uint8_t>(b >> 4));
        out.push_back(static_cast<std::uint8_t>(b & 0x0F));
    }
    return out;
}

Bytes denibblize(const Bytes& in)
{
    Bytes out;
    out.reserve(in.size() / 2);
    for (std::size_t i = 0; i + 1 < in.size(); i += 2)
        out.push_back(static_cast<std::uint8_t>(((in[i] & 0x0F) << 4) | (in[i + 1] & 0x0F)));
    return out;
}

std::string payloadText(const Frame& f)
{
    std::string s;
    s.reserve(f.payload.size());
    for (auto b : f.payload)
    {
        if (b == 0)
            break;
        s.push_back(static_cast<char>(b));
    }
    return s;
}

Bytes makeWant(std::uint8_t deviceId, Code want)
{
    switch (want)
    {
        case Code::ProgramWant:
        case Code::SetupWant:
        case Code::SigfileWant:
        case Code::SigfileWantQuick:
        case Code::ScreenWant:
        case Code::InfoWant:
        case Code::SigdbaseWant:
            return buildFrame(deviceId, want);
        default:
            throw std::invalid_argument("not a read-only want code");
    }
}

std::optional<Code> replyCodeFor(Code request)
{
    switch (request)
    {
        case Code::ProgramWant: return Code::ProgramDump;
        case Code::SetupWant: return Code::SetupDump;
        case Code::SigfileWant: return Code::SigfileDump;
        case Code::SigfileWantQuick: return Code::SigfileDumpRemote;
        case Code::ScreenWant: return Code::ScreenDump;
        case Code::InfoWant: return Code::InfoDump;
        case Code::SigdbaseWant: return Code::SigdbaseDump;
        case Code::ParamSet: return Code::ParamAck;
        default: return std::nullopt;
    }
}

std::string paramIdForModule(int moduleIndex)
{
    if (moduleIndex < 1 || moduleIndex > 0xFF)
        throw std::out_of_range("module index must be 1..255");
    char buf[8];
    std::snprintf(buf, sizeof buf, "%02x0001", moduleIndex);
    return buf;
}

Bytes makeParamSet(std::uint8_t deviceId, std::string_view paramId, std::string_view value)
{
    Bytes p;
    for (char c : paramId) p.push_back(static_cast<std::uint8_t>(c));
    p.push_back(' ');
    for (char c : value) p.push_back(static_cast<std::uint8_t>(c));
    p.push_back(' ');
    return buildFrame(deviceId, Code::ParamSet, p);
}

std::optional<int> ParamChange::moduleIndex() const
{
    if (paramId.size() < 2)
        return std::nullopt;
    try
    {
        return std::stoi(paramId.substr(0, 2), nullptr, 16);
    }
    catch (...)
    {
        return std::nullopt;
    }
}

std::optional<ParamChange> parseParamSet(const Frame& f)
{
    if (f.code != static_cast<std::uint8_t>(Code::ParamSet))
        return std::nullopt;
    std::string s(f.payload.begin(), f.payload.end());
    auto first = s.find(' ');
    if (first == std::string::npos || first == 0)
        return std::nullopt;
    ParamChange c;
    c.paramId = s.substr(0, first);
    auto rest = s.substr(first + 1);
    while (! rest.empty() && rest.back() == ' ')
        rest.pop_back();
    if (rest.empty())
        return std::nullopt;
    c.value = rest;
    return c;
}

Bytes makeBankChange(std::uint8_t deviceId, bool card, int bank)
{
    if (bank < 0 || bank > 99)
        throw std::out_of_range("bank must be 0..99");
    return buildFrame(deviceId, Code::BankChange,
                      nibblize({ static_cast<std::uint8_t>(card ? 1 : 0), static_cast<std::uint8_t>(bank) }));
}

Bytes makeProgramChange(int channel, int program)
{
    if (channel < 0 || channel > 15 || program < 0 || program > 127)
        throw std::out_of_range("channel 0..15, program 0..127");
    return { static_cast<std::uint8_t>(0xC0 | channel), static_cast<std::uint8_t>(program) };
}

std::vector<Bytes> makeProgramSelect(std::uint8_t deviceId, int channel, int number)
{
    if (number < 0 || number > 9999)
        throw std::out_of_range("program number must be 0..9999");
    return { makeBankChange(deviceId, false, number / 100), makeProgramChange(channel, number % 100) };
}

std::optional<std::uint32_t> keyMask(const Frame& f)
{
    if (f.code != static_cast<std::uint8_t>(Code::Keypress) || f.payload.size() != 8)
        return std::nullopt;
    auto b = denibblize(f.payload);
    return (std::uint32_t(b[0]) << 24) | (std::uint32_t(b[1]) << 16) | (std::uint32_t(b[2]) << 8) | b[3];
}

std::vector<std::string> pressedKeys(std::uint32_t mask)
{
    static const char* names[32] = {
        "LEVELS", "?1", "SOFT4", "PATCH", "ENTER", "CXL", "DOWN", "UP",
        "SELECT", "BYPASS", "SOFT3", "SETUP", "MINUS", "NINE", "SIX", "THREE",
        "LEFT", "USER2", "SOFT2", "PARAMETER", "ZERO", "EIGHT", "FIVE", "TWO",
        "RIGHT", "USER1", "SOFT1", "PROGRAM", "DOT", "SEVEN", "FOUR", "ONE",
    };
    std::vector<std::string> out;
    for (int bit = 0; bit < 32; ++bit)
        if ((mask & (1u << bit)) == 0)
            out.emplace_back(names[bit]);
    return out;
}

Bytes makeKeyFrame(std::uint8_t deviceId, SafeKey key)
{
    const auto m = static_cast<std::uint32_t>(key);
    return buildFrame(deviceId, Code::Keypress,
                      nibblize({ static_cast<std::uint8_t>(m >> 24), static_cast<std::uint8_t>(m >> 16),
                                 static_cast<std::uint8_t>(m >> 8), static_cast<std::uint8_t>(m) }));
}

std::vector<Bytes> makeLoadProgram(std::uint8_t deviceId, int number, bool withSelect)
{
    if (number < 1 || number > 9999)
        throw std::out_of_range("program number must be 1..9999");
    static constexpr SafeKey digits[10] = { SafeKey::Zero, SafeKey::One, SafeKey::Two, SafeKey::Three, SafeKey::Four,
                                            SafeKey::Five, SafeKey::Six, SafeKey::Seven, SafeKey::Eight, SafeKey::Nine };
    std::vector<SafeKey> keys { SafeKey::Program };
    for (char ch : std::to_string(number))
        keys.push_back(digits[ch - '0']);
    keys.push_back(SafeKey::Enter);
    if (withSelect)
        keys.push_back(SafeKey::Select);

    std::vector<Bytes> out;
    for (auto k : keys)
    {
        out.push_back(makeKeyFrame(deviceId, k));
        out.push_back(makeKeyFrame(deviceId, SafeKey::None));
    }
    return out;
}

} // namespace h7600
