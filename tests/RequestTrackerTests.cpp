#include "h7600/RequestTracker.h"

#include <catch2/catch_test_macros.hpp>

using namespace h7600;

static Frame frame(Code c) { return { 1, static_cast<std::uint8_t>(c), {} }; }

TEST_CASE("reply resolves the matching request")
{
    RequestTracker t;
    int got = 0;
    REQUIRE(t.expect(Code::SigfileWantQuick, 0, 5000, [&](const Frame&) { ++got; }));
    CHECK_FALSE(t.onFrame(frame(Code::Keypress))); // unsolicited echo is ignored
    CHECK(t.onFrame(frame(Code::SigfileDumpRemote)));
    CHECK(got == 1);
    CHECK(t.pending() == 0);
}

TEST_CASE("timeouts fire once and only after the deadline")
{
    RequestTracker t;
    int timeouts = 0;
    t.expect(Code::InfoWant, 0, 1000, {}, [&](Code c) { CHECK(c == Code::InfoWant); ++timeouts; });
    t.expect(Code::ParamSet, 0, 3000, {}, [&](Code) { ++timeouts; });
    t.poll(999);
    CHECK(timeouts == 0);
    t.poll(1000);
    CHECK(timeouts == 1);
    CHECK(t.pending() == 1);
    t.poll(5000);
    CHECK(timeouts == 2);
}

TEST_CASE("requests without a reply code are not tracked")
{
    RequestTracker t;
    CHECK_FALSE(t.expect(Code::BankChange, 0, 1000, {}));
}
