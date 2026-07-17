#include "FixedWindowStrategy.h"

#include <algorithm>

#include "TimeUtil.h"

namespace ratelimiter {

namespace {

// Aligns `now` to the start of the fixed window it falls in.
int64_t windowStartFor(int64_t now, int64_t windowMs) {
    return (now / windowMs) * windowMs;
}

}  // namespace

RateLimitDecision FixedWindowStrategy::checkAndConsume(const std::string& key,
                                                         const RateLimitConfig& config) {
    const int64_t now = nowMillis();
    const int64_t currentWindowStart = windowStartFor(now, config.windowMs);

    return store_.withState(
        key,
        [&]() { return State{currentWindowStart, 0}; },
        [&](State& state) -> RateLimitDecision {
            // A new window has begun since this key was last seen: reset the counter.
            if (state.windowStart != currentWindowStart) {
                state.windowStart = currentWindowStart;
                state.count = 0;
            }

            RateLimitDecision decision;
            decision.limit = config.limit;
            decision.resetSeconds = std::max<int64_t>(
                0, (state.windowStart + config.windowMs - now + 999) / 1000);

            if (state.count < config.limit) {
                state.count += 1;
                decision.allowed = true;
                decision.remaining = config.limit - state.count;
            } else {
                decision.allowed = false;
                decision.remaining = 0;
            }
            return decision;
        });
}

RateLimitDecision FixedWindowStrategy::getStatus(const std::string& key,
                                                   const RateLimitConfig& config) const {
    const int64_t now = nowMillis();
    const int64_t currentWindowStart = windowStartFor(now, config.windowMs);

    RateLimitDecision decision;
    decision.limit = config.limit;

    const auto existing = store_.peek(key);
    if (!existing.has_value() || existing->windowStart != currentWindowStart) {
        // No requests recorded in the current window yet: full quota available.
        decision.allowed = true;
        decision.remaining = config.limit;
        decision.resetSeconds = config.windowMs / 1000;
        return decision;
    }

    decision.remaining = std::max(0, config.limit - existing->count);
    decision.allowed = decision.remaining > 0;
    decision.resetSeconds = std::max<int64_t>(
        0, (existing->windowStart + config.windowMs - now + 999) / 1000);
    return decision;
}

}  // namespace ratelimiter
