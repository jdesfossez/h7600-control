#pragma once
// Data that is not in the repository (Eventide's module database, factory sigfiles): tests that
// need it read it from the environment or the default folder, and SKIP when it is absent.
#include "h7600/Protocol.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

inline std::filesystem::path externalSigfilesDir()
{
    if (auto* d = std::getenv("H7600_SIGFILES_DIR")) return d;
    if (auto* h = std::getenv("HOME")) return std::filesystem::path(h) / "Documents/h7600/sigfiles";
    return {};
}

inline std::optional<std::string> readText(const std::filesystem::path& p)
{
    std::ifstream f(p, std::ios::binary);
    if (! f) return std::nullopt;
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

/// Text of a .sig file, or the payload text of a .syx frame (stops at the NUL).
inline std::optional<std::string> sigfileText(const std::filesystem::path& p)
{
    auto raw = readText(p);
    if (! raw) return std::nullopt;
    if (p.extension() == ".syx")
    {
        auto f = h7600::parseFrame(h7600::Bytes(raw->begin(), raw->end()));
        if (! f) return std::nullopt;
        return h7600::payloadText(*f);
    }
    auto nul = raw->find('\0');
    return nul == std::string::npos ? *raw : raw->substr(0, nul);
}

/// The full module database text: $H7600_SIGDBASE (.txt or .syx), else the default folder's
/// sigdbase_H7600_fw522.txt, else tests/fixtures/sigdbase_0c.syx (git-ignored).
inline std::optional<std::string> fullDatabaseText()
{
    std::vector<std::filesystem::path> c;
    if (auto* e = std::getenv("H7600_SIGDBASE")) c.emplace_back(e);
    c.push_back(externalSigfilesDir() / "sigdbase_H7600_fw522.txt");
    c.push_back(std::filesystem::path(H7600_FIXTURES_DIR) / "sigdbase_0c.syx");
    for (auto& p : c)
        if (std::filesystem::exists(p)) return sigfileText(p);
    return std::nullopt;
}

/// Every factory sigfile available locally (.sig in the external folder, .syx captures in fixtures).
inline std::vector<std::filesystem::path> externalSigfiles()
{
    std::vector<std::filesystem::path> out;
    std::error_code ec;
    for (auto& dir : { externalSigfilesDir(), std::filesystem::path(H7600_FIXTURES_DIR) })
        for (auto& e : std::filesystem::directory_iterator(dir, ec))
        {
            auto n = e.path().filename().string();
            if ((e.path().extension() == ".sig") || (n.rfind("sigfile_", 0) == 0 && e.path().extension() == ".syx"))
                out.push_back(e.path());
        }
    std::sort(out.begin(), out.end());
    return out;
}
