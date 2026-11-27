#pragma once
// Rate-limited, coalescing sender for ParamSet (3C) messages.
//
// While a control is dragged only the latest value per parameter is kept.
// Messages go out at most every minIntervalMs (7 ms, about 140/s: the MIDI
// cable carries about 190 of these per second and the unit keeps up, measured
// on fw 5.22). Up to maxInFlight messages wait for their 2E ack at once, but a
// parameter is never sent again while its previous value is unacknowledged, so
// a drag coalesces to the latest value. The unit's acks carry no id: they are
// matched oldest first. An unacknowledged message is retried a few times after
// ackTimeoutMs unless a newer value for that parameter replaced it.
// A 2E ack only means the message arrived: the unit acks values it ignores, so
// callers re-read the program (0B) to confirm.
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
        std::int64_t minIntervalMs = 7; // about 140 messages/s max
        std::int64_t ackTimeoutMs = 400;
        int maxRetries = 2;
        std::size_t maxInFlight = 4;
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

    bool idle() const { return queue.empty() && inFlight.empty(); }
    std::size_t queued() const { return queue.size(); }
    std::size_t waiting() const { return inFlight.size(); }
    void clear() { queue.clear(); inFlight.clear(); }

private:
    struct Item
    {
        std::string id, value;
        int attempts = 0;
        std::int64_t sentAt = 0;
    };
    bool inFlightHas(const std::string& id) const;
    bool queuedHas(const std::string& id) const;
    std::uint8_t device;
    Options opt;
    std::deque<Item> queue;
    std::deque<Item> inFlight; // oldest first
    std::int64_t lastSend = -1000000;
};
} // namespace h7600
