#include "h7600/ScreenDump.h"

namespace h7600
{
namespace
{
std::uint32_t be32(const Bytes& b, std::size_t at)
{
    return (std::uint32_t(b[at]) << 24) | (std::uint32_t(b[at + 1]) << 16) | (std::uint32_t(b[at + 2]) << 8) | b[at + 3];
}
} // namespace

std::optional<ScreenDump> parseScreenDump(const Frame& f)
{
    if (f.code != static_cast<std::uint8_t>(Code::ScreenDump))
        return std::nullopt;
    auto b = denibblize(f.payload);
    if (b.size() < 12)
        return std::nullopt;
    ScreenDump s;
    s.width = static_cast<int>(be32(b, 0));
    s.height = static_cast<int>(be32(b, 4));
    auto size = be32(b, 8);
    const auto expected = static_cast<std::uint32_t>((s.width + 7) / 8 * s.height);
    if (s.width <= 0 || s.height <= 0 || size != expected || b.size() < 12 + size)
        return std::nullopt;
    // The trailing checksum byte is not validated yet: its rule on fw 5.22
    // does not match the DSP4000 tech note and still needs investigation.
    s.bitmap.assign(b.begin() + 12, b.begin() + 12 + size);
    return s;
}
} // namespace h7600
