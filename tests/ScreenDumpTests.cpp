#include "Fixtures.h"
#include "h7600/ScreenDump.h"

#include <catch2/catch_test_macros.hpp>

using namespace h7600;

TEST_CASE("screen dump is a 240x64 1-bit bitmap")
{
    auto s = parseScreenDump(loadFrame("screen_dump_17.syx"));
    REQUIRE(s);
    CHECK(s->width == 240);
    CHECK(s->height == 64);
    CHECK(s->bitmap.size() == 1920);
    // The title bar is drawn inverted (lit) across the top-left.
    CHECK(s->pixel(1, 1));
}
