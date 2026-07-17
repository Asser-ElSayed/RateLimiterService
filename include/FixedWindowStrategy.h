#pragma once

#include <cstdint>
#include <string>

#include "IRateLimitStrategy.h"
#include "ShardedKeyStore.h"

namespace ratelimiter {

// Fixed Window algorithm: time is divided into consecutive, non-overlapping
// windows of `windowMs`. Each key gets a counter that resets to zero the
// moment a new window starts. Simple and cheap, at the cost of allowing up
// to 2x the limit across a window boundary (a well-known, accepted
// trade-off of this algorithm).
class FixedWindowStrategy : public IRateLimitStrategy {
public:
    RateLimitDecision checkAndConsume(const std::string& key,
                                       const RateLimitConfig& config) override;

    RateLimitDecision getStatus(const std::string& key,
                                 const RateLimitConfig& config) const override;

    std::string name() const override { return "fixed_window"; }

private:
    struct State {
        int64_t windowStart = 0;  // epoch ms marking the start of the current window
        int count = 0;            // requests consumed in the current window
    };

    ShardedKeyStore<State> store_;
};

}  // namespace ratelimiter
