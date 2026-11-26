#pragma once
// Matches replies from the unit to outstanding requests, with timeouts.
// Pure C++ and clock-agnostic: the caller passes "now" in milliseconds, so the
// logic is unit-testable and can be driven from a JUCE timer.

#include "h7600/Protocol.h"

#include <cstdint>
#include <deque>
#include <functional>

namespace h7600
{
class RequestTracker
{
public:
    using ReplyFn = std::function<void(const Frame&)>;
    using TimeoutFn = std::function<void(Code request)>;

    /// Registers a request that expects `replyCodeFor(request)`. The H7600
    /// handles one request at a time, so replies are matched oldest-first.
    /// Returns false if the request has no reply code.
    bool expect(Code request, std::int64_t nowMs, std::int64_t timeoutMs, ReplyFn onReply, TimeoutFn onTimeout = {});

    /// Feeds an incoming frame. Returns true if it satisfied a pending request.
    bool onFrame(const Frame& f);

    /// Fires timeouts for requests older than their deadline.
    void poll(std::int64_t nowMs);

    std::size_t pending() const { return queue.size(); }
    void clear() { queue.clear(); }

private:
    struct Pending
    {
        Code request;
        Code reply;
        std::int64_t deadline;
        ReplyFn onReply;
        TimeoutFn onTimeout;
    };
    std::deque<Pending> queue;
};
} // namespace h7600
