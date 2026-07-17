#include "SlidingWindowStrategy.h"

#include <algorithm>
#include <cmath>

#include "TimeUtil.h"

namespace ratelimiter {

double SlidingWindowStrategy::advanceAndEstimate(State& state, int64_t now, int64_t windowMs) {
    const int64_t currentWindowStart = (now / windowMs) * windowMs;
    const int64_t elapsedWindows = (currentWindowStart - state.currentWindowStart) / windowMs;

    if (elapsedWindows == 1) {
        // Exactly one window has passed: the old "current" becomes "previous".
        state.previousCount = state.currentCount;
        state.currentCount = 0;
        state.currentWindowStart = currentWindowStart;
    } else if (elapsedWindows > 1) {
        // More than one window elapsed since this key was last seen: no
        // overlap remains with either bucket, so both reset to zero.
        state.previousCount = 0;
        state.currentCount = 0;
        state.currentWindowStart = currentWindowStart;
    }
    // elapsedWindows == 0 means we're still inside the same window; nothing to roll.

    const double elapsedFraction =
        static_cast<double>(now - currentWindowStart) / static_cast<double>(windowMs);
    const double previousWeight = 1.0 - elapsedFraction;

    return state.previousCount * previousWeight + state.currentCount;
}

RateLimitDecision SlidingWindowStrategy::checkAndConsume(const std::string& key,
                                                           const RateLimitConfig& config) {
    const int64_t now = nowMillis();

    return store_.withState(
        key,
        [&]() {
            State state;
            state.currentWindowStart = (now / config.windowMs) * config.windowMs;
            return state;
        },
        [&](State& state) -> RateLimitDecision {
            const double estimated = advanceAndEstimate(state, now, config.windowMs);

            RateLimitDecision decision;
            decision.limit = config.limit;
            const int64_t windowEnd = state.currentWindowStart + config.windowMs;
            decision.resetSeconds = std::max<int64_t>(0, (windowEnd - now + 999) / 1000);

            if (estimated < static_cast<double>(config.limit)) {
                state.currentCount += 1;
                decision.allowed = true;
                const double remainingEstimate = config.limit - (estimated + 1.0);
                decision.remaining = std::max(0, static_cast<int>(std::floor(remainingEstimate)));
            } else {
                decision.allowed = false;
                decision.remaining = 0;
            }
            return decision;
        });
}

RateLimitDecision SlidingWindowStrategy::getStatus(const std::string& key,
                                                     const RateLimitConfig& config) const {
    const int64_t now = nowMillis();

    RateLimitDecision decision;
    decision.limit = config.limit;

    const auto existing = store_.peek(key);
    if (!existing.has_value()) {
        decision.allowed = true;
        decision.remaining = config.limit;
        decision.resetSeconds = config.windowMs / 1000;
        return decision;
    }

    // Work on a local copy: this is a read-only status check and must not
    // mutate the stored state (that would let /status consume quota).
    State state = *existing;
    const double estimated = advanceAndEstimate(state, now, config.windowMs);

    const int64_t windowEnd = state.currentWindowStart + config.windowMs;
    decision.resetSeconds = std::max<int64_t>(0, (windowEnd - now + 999) / 1000);
    decision.remaining = std::max(0, static_cast<int>(std::floor(config.limit - estimated)));
    decision.allowed = decision.remaining > 0;
    return decision;
}

}  // namespace ratelimiter
