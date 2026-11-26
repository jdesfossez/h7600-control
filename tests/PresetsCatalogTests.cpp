#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <iterator>
#include <string>

static std::size_t count(const std::string& s, const std::string& needle)
{
    std::size_t n = 0;
    for (auto p = s.find(needle); p != std::string::npos; p = s.find(needle, p + 1)) ++n;
    return n;
}

TEST_CASE("presets catalog has 75 banks and 1114 presets")
{
    std::ifstream f(std::string(H7600_RESOURCES_DIR) + "/presets.json");
    REQUIRE(f);
    std::string s{ std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>() };
    auto presets = count(s, "\"max_rate_khz\"");
    CHECK(presets == 1114);
    CHECK(count(s, "\"number\"") - presets == 75);
    CHECK(s.find("\"name\": \"Ambience\"") != std::string::npos);
}
