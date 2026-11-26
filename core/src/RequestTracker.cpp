#include "h7600/RequestTracker.h"

#include <utility>

namespace h7600
{
bool RequestTracker::expect(Code request, std::int64_t nowMs, std::int64_t timeoutMs, ReplyFn onReply, TimeoutFn onTimeout)
{
    auto reply = replyCodeFor(request);
    if (! reply)
        return false;
    queue.push_back({ request, *reply, nowMs + timeoutMs, std::move(onReply), std::move(onTimeout) });
    return true;
}

bool RequestTracker::onFrame(const Frame& f)
{
    for (auto it = queue.begin(); it != queue.end(); ++it)
    {
        if (static_cast<std::uint8_t>(it->reply) == f.code)
        {
            auto cb = std::move(it->onReply);
            queue.erase(it);
            if (cb)
                cb(f);
            return true;
        }
    }
    return false;
}

void RequestTracker::poll(std::int64_t nowMs)
{
    while (! queue.empty())
    {
        auto it = queue.begin();
        for (; it != queue.end(); ++it)
            if (nowMs >= it->deadline)
                break;
        if (it == queue.end())
            return;
        auto p = std::move(*it);
        queue.erase(it);
        if (p.onTimeout)
            p.onTimeout(p.request);
    }
}
} // namespace h7600
