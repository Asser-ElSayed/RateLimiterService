#include "TokenBucketStrategy.h"

#include <algorithm>
#include <cmath>

#include "TimeUtil.h"

namespace ratelimiter {

double TokenBucketStrategy::refill(State& state, int64_t now, const RateLimitConfig& config) {
    const double ratePerMs = static_cast<double>(config.limit) / static_cast<double>(config.windowMs);

    const int64_t elapsedMs = now - state.lastRefillMs;
    if (elapsedMs > 0) {
        state.tokens = std::min(static_cast<double>(config.limit),
                                 state.tokens + elapsedMs * ratePerMs);
        state.lastRefillMs = now;
    }
    return ratePerMs;
}

RateLimitDecision TokenBucketStrategy::checkAndConsume(const std::string& key,
                                                         const RateLimitConfig& config) {
    const int64_t now = nowMillis();

    return store_.withState(
        key,
        [&]() {
            // A brand-new key starts with a full bucket, "last refilled now".
            return State{static_cast<double>(config.limit), now};
        },
        [&](State& state) -> RateLimitDecision {
            const double ratePerMs = refill(state, now, config);

            RateLimitDecision decision;
            decision.limit = config.limit;

            if (state.tokens >= 1.0) {
                state.tokens -= 1.0;
                decision.allowed = true;
                decision.remaining = static_cast<int>(std::floor(state.tokens));
                decision.resetSeconds = 0;  // capacity available now
            } else {
                decision.allowed = false;
                decision.remaining = 0;
                const double tokensNeeded = 1.0 - state.tokens;
                const double msUntilNextToken = ratePerMs > 0 ? tokensNeeded / ratePerMs : 0.0;
                decision.resetSeconds = static_cast<int64_t>(std::ceil(msUntilNextToken / 1000.0));
            }
            return decision;
        });
}

RateLimitDecision TokenBucketStrategy::getStatus(const std::string& key,
                                                   const RateLimitConfig& config) const {
    const int64_t now = nowMillis();

    RateLimitDecision decision;
    decision.limit = config.limit;

    const auto existing = store_.peek(key);
    if (!existing.has_value()) {
        // Never seen before: bucket starts full.
        decision.allowed = true;
        decision.remaining = config.limit;
        decision.resetSeconds = 0;
        return decision;
    }

    // Read-only: refill a local copy so /status never mutates stored state.
    State state = *existing;
    const double ratePerMs = refill(state, now, config);

    decision.remaining = static_cast<int>(std::floor(state.tokens));
    decision.allowed = state.tokens >= 1.0;
    if (!decision.allowed) {
        const double tokensNeeded = 1.0 - state.tokens;
        const double msUntilNextToken = ratePerMs > 0 ? tokensNeeded / ratePerMs : 0.0;
        decision.resetSeconds = static_cast<int64_t>(std::ceil(msUntilNextToken / 1000.0));
    } else {
        decision.resetSeconds = 0;
    }
    return decision;
}

}  // namespace ratelimiter
