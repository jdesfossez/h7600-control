#pragma once
#include "h7600/Protocol.h"

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

inline h7600::Bytes loadFixture(const std::string& name)
{
    std::ifstream f(std::string(H7600_FIXTURES_DIR) + "/" + name, std::ios::binary);
    if (! f)
        throw std::runtime_error("missing fixture " + name);
    return { std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>() };
}

inline h7600::Frame loadFrame(const std::string& name)
{
    auto f = h7600::parseFrame(loadFixture(name));
    if (! f)
        throw std::runtime_error("fixture is not an Eventide frame: " + name);
    return *f;
}
