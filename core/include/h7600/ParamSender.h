#pragma once
// Rate-limited, coalescing sender for ParamSet (3C) messages.
//
// While a control is dragged only the latest value per parameter is kept.
// One message is in flight at a time; the next goes out when the unit's 2E
// ack arrives, or after a timeout (the message is then retried a few times,
// unless a newer value for that parameter has replaced it).
// Clock-agnostic: the caller passes "now" in ms and sends what poll() returns.

#include "h7600/Protocol.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <string>

namespace h7600
{
class ParamSender
{
public:
    struct Options
    {
        std::int64_t minIntervalMs = 25; // ~40 messages/s max
        std::int64_t ackTimeoutMs = 400;
        int maxRetries = 2;
    };

    explicit ParamSender(std::uint8_t deviceId = 1) : ParamSender(deviceId, Options {}) {}
    ParamSender(std::uint8_t deviceId, Options o) : device(deviceId), opt(o) {}

    void setDeviceId(std::uint8_t id) { device = id; }

    /// Queues a value; replaces any queued (not yet sent) value for the same id.
    void set(const std::string& paramId, const std::string& value);

    /// Feeds an incoming frame; returns true if it was the ack we waited for.
    bool onFrame(const Frame& f, std::int64_t nowMs);

    /// Returns the next message to send now, if any.
    std::optional<Bytes> poll(std::int64_t nowMs);

    /// Called when a message is given up after all retries.
    std::function<void(const std::string& paramId, const std::string& value)> onFailed;

    bool idle() const { return queue.empty() && ! inFlight; }
    std::size_t queued() const { return queue.size(); }
    void clear() { queue.clear(); inFlight.reset(); }

private:
    struct Item
    {
        std::string id, value;
        int attempts = 0;
    };
    std::uint8_t device;
    Options opt;
    std::deque<Item> queue;
    std::optional<Item> inFlight;
    std::int64_t sentAt = 0;
    std::int64_t lastSend = -1000000;
};
} // namespace h7600
