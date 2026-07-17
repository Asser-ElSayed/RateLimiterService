#pragma once

#include <cstdint>
#include <string>

#include "IRateLimitStrategy.h"
#include "ShardedKeyStore.h"

namespace ratelimiter {

// Sliding Window (counter-based approximation) algorithm: keeps a count for
// the previous window and the current window, and estimates the number of
// requests in the trailing `windowMs` interval as a weighted blend of the
// two, based on how far into the current window `now` is:
//
//   estimated = previousCount * (1 - elapsedFraction) + currentCount
//
// This smooths out the hard reset-at-boundary problem of Fixed Window while
// staying O(1) in memory per key (no per-request timestamp log to store or
// prune), which keeps it simple and predictable under load.
class SlidingWindowStrategy : public IRateLimitStrategy {
public:
    RateLimitDecision checkAndConsume(const std::string& key,
                                       const RateLimitConfig& config) override;

    RateLimitDecision getStatus(const std::string& key,
                                 const RateLimitConfig& config) const override;

    std::string name() const override { return "sliding_window"; }

private:
    struct State {
        int64_t currentWindowStart = 0;
        int currentCount = 0;
        int previousCount = 0;
    };

    // Shared helper: advances `state` to the window `now` falls in (rolling
    // the current window into "previous" as needed) and returns the
    // estimated request count for the trailing window.
    static double advanceAndEstimate(State& state, int64_t now, int64_t windowMs);

    ShardedKeyStore<State> store_;
};

}  // namespace ratelimiter
