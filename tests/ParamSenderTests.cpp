#include "h7600/ParamSender.h"

#include <catch2/catch_test_macros.hpp>

using namespace h7600;

static Frame ack() { return { 1, static_cast<std::uint8_t>(Code::ParamAck), {} }; }
static std::string text(const Bytes& b) { return std::string(b.begin() + 5, b.end() - 1); }

TEST_CASE("coalesces drag values: only the latest queued value is sent")
{
    ParamSender s(1);
    s.set("1e0001", "50");
    auto first = s.poll(0);
    REQUIRE(first);
    CHECK(text(*first) == "1e0001 50 ");
    // While waiting for the ack the user keeps dragging.
    s.set("1e0001", "51");
    s.set("1e0001", "52");
    s.set("1e0001", "53");
    CHECK(s.queued() == 1);
    CHECK_FALSE(s.poll(10)); // still in flight
    CHECK(s.onFrame(ack(), 12));
    auto next = s.poll(12);
    REQUIRE(next);
    CHECK(text(*next) == "1e0001 53 ");
    CHECK(s.onFrame(ack(), 20));
    CHECK(s.idle());
}

TEST_CASE("different parameters keep their order")
{
    ParamSender s(1);
    s.set("030001", "-12");
    s.set("1e0001", "50");
    auto a = s.poll(0);
    s.onFrame(ack(), 5);
    auto b = s.poll(5);
    REQUIRE(a);
    REQUIRE(b);
    CHECK(text(*a) == "030001 -12 ");
    CHECK(text(*b) == "1e0001 50 ");
}

TEST_CASE("an ack frees the line for the next parameter at once")
{
    ParamSender::Options o;
    o.minIntervalMs = 25;
    ParamSender s(1, o);
    s.set("1e0001", "50");
    s.set("030001", "-12");
    REQUIRE(s.poll(0));
    s.onFrame(ack(), 1);
    CHECK(s.poll(1)); // ack frees the line at once
}

TEST_CASE("timeout retries, then reports failure")
{
    ParamSender::Options o;
    o.ackTimeoutMs = 100;
    o.maxRetries = 2;
    ParamSender s(1, o);
    std::string failed;
    s.onFailed = [&](const std::string& id, const std::string& v) { failed = id + "=" + v; };
    s.set("1e0001", "50");
    REQUIRE(s.poll(0));   // attempt 1
    REQUIRE(s.poll(100)); // attempt 2
    REQUIRE(s.poll(200)); // attempt 3
    CHECK_FALSE(s.poll(300));
    CHECK(failed == "1e0001=50");
    CHECK(s.idle());
}

TEST_CASE("a newer value replaces a timed-out one instead of retrying it")
{
    ParamSender::Options o;
    o.ackTimeoutMs = 100;
    ParamSender s(1, o);
    s.set("1e0001", "50");
    REQUIRE(s.poll(0));
    s.set("1e0001", "60");
    auto next = s.poll(100);
    REQUIRE(next);
    CHECK(text(*next) == "1e0001 60 ");
}

TEST_CASE("unrelated frames are not acks")
{
    ParamSender s(1);
    s.set("1e0001", "50");
    s.poll(0);
    CHECK_FALSE(s.onFrame({ 1, static_cast<std::uint8_t>(Code::Keypress), {} }, 1));
}
