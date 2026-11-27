#include "h7600/ParamSender.h"

#include <algorithm>

namespace h7600
{
bool ParamSender::inFlightHas(const std::string& id) const
{
    return std::any_of(inFlight.begin(), inFlight.end(), [&](auto& i) { return i.id == id; });
}

bool ParamSender::queuedHas(const std::string& id) const
{
    return std::any_of(queue.begin(), queue.end(), [&](auto& i) { return i.id == id; });
}

void ParamSender::set(const std::string& id, const std::string& value)
{
    for (auto& q : queue)
        if (q.id == id)
        {
            q.value = value;
            q.attempts = 0;
            return;
        }
    queue.push_back({ id, value, 0, 0 });
}

bool ParamSender::onFrame(const Frame& f, std::int64_t)
{
    if (inFlight.empty() || f.code != static_cast<std::uint8_t>(Code::ParamAck))
        return false;
    inFlight.pop_front(); // the rate cap still applies: acks do not speed up sending
    return true;
}

std::optional<Bytes> ParamSender::poll(std::int64_t nowMs)
{
    // Timeouts, oldest first. A timed-out message is retried unless a newer value replaced it.
    while (! inFlight.empty() && nowMs - inFlight.front().sentAt >= opt.ackTimeoutMs)
    {
        Item it = inFlight.front();
        inFlight.pop_front();
        if (queuedHas(it.id))
            continue;
        if (it.attempts <= opt.maxRetries)
            queue.push_front(it);
        else if (onFailed)
            onFailed(it.id, it.value);
    }
    if (inFlight.size() >= opt.maxInFlight || nowMs - lastSend < opt.minIntervalMs)
        return std::nullopt;
    // The first queued parameter that is not already waiting for an ack.
    auto next = std::find_if(queue.begin(), queue.end(), [&](auto& q) { return ! inFlightHas(q.id); });
    if (next == queue.end())
        return std::nullopt;
    Item it = *next;
    queue.erase(next);
    if (it.attempts > opt.maxRetries)
    {
        if (onFailed) onFailed(it.id, it.value);
        return std::nullopt;
    }
    ++it.attempts;
    it.sentAt = lastSend = nowMs;
    inFlight.push_back(it);
    return makeParamSet(device, it.id, it.value);
}
} // namespace h7600
