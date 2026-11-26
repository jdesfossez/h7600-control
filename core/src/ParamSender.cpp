#include "h7600/ParamSender.h"

namespace h7600
{
void ParamSender::set(const std::string& id, const std::string& value)
{
    for (auto& q : queue)
        if (q.id == id)
        {
            q.value = value;
            q.attempts = 0;
            return;
        }
    queue.push_back({ id, value, 0 });
}

bool ParamSender::onFrame(const Frame& f, std::int64_t nowMs)
{
    if (! inFlight || f.code != static_cast<std::uint8_t>(Code::ParamAck))
        return false;
    inFlight.reset();
    lastSend = nowMs - opt.minIntervalMs; // the ack frees the line immediately
    return true;
}

std::optional<Bytes> ParamSender::poll(std::int64_t nowMs)
{
    if (inFlight)
    {
        if (nowMs - sentAt < opt.ackTimeoutMs)
            return std::nullopt;
        // Timed out. Retry unless a newer value for the same id is queued.
        bool superseded = false;
        for (auto& q : queue)
            if (q.id == inFlight->id) superseded = true;
        if (! superseded && inFlight->attempts <= opt.maxRetries)
            queue.push_front(*inFlight);
        else if (! superseded && onFailed)
            onFailed(inFlight->id, inFlight->value);
        inFlight.reset();
    }
    if (queue.empty() || nowMs - lastSend < opt.minIntervalMs)
        return std::nullopt;
    Item it = queue.front();
    queue.pop_front();
    if (it.attempts > opt.maxRetries)
    {
        if (onFailed) onFailed(it.id, it.value);
        return std::nullopt;
    }
    ++it.attempts;
    inFlight = it;
    sentAt = lastSend = nowMs;
    return makeParamSet(device, it.id, it.value);
}
} // namespace h7600
