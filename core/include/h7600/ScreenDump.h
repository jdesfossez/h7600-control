#pragma once
#include "h7600/Protocol.h"

namespace h7600
{
/// LCD bitmap from a ScreenDump (0x17) frame: 1 bit per pixel, MSB = leftmost,
/// 1 = lit. The H7600 reports 240 x 64.
struct ScreenDump
{
    int width = 0;
    int height = 0;
    Bytes bitmap; // rows of (width + 7) / 8 bytes

    bool pixel(int x, int y) const
    {
        const int stride = (width + 7) / 8;
        return (bitmap[static_cast<std::size_t>(y * stride + x / 8)] >> (7 - x % 8)) & 1;
    }
};

std::optional<ScreenDump> parseScreenDump(const Frame& f);
} // namespace h7600
