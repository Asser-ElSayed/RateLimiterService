#pragma once

#include <cstdint>
#include <string>

#include "IRateLimitStrategy.h"
#include "ShardedKeyStore.h"

namespace ratelimiter {

// Token Bucket algorithm: each key owns a bucket that holds up to
// `config.limit` tokens. Tokens refill continuously at a rate of
// `limit / windowMs` tokens per millisecond. Each allowed request consumes
// one token. Refill is computed lazily (on demand, from elapsed time) rather
// than via a background timer thread, which keeps the implementation simple
// and avoids an extra thread waking up for every key.
class TokenBucketStrategy : public IRateLimitStrategy {
public:
    RateLimitDecision checkAndConsume(const std::string& key,
                                       const RateLimitConfig& config) override;

    RateLimitDecision getStatus(const std::string& key,
                                 const RateLimitConfig& config) const override;

    std::string name() const override { return "token_bucket"; }

private:
    struct State {
        double tokens = 0.0;
        int64_t lastRefillMs = 0;
    };

    // Refills `state` in place based on elapsed time and returns the
    // refill rate (tokens per millisecond) used, so callers can compute
    // how long until the next token becomes available.
    static double refill(State& state, int64_t now, const RateLimitConfig& config);

    ShardedKeyStore<State> store_;
};

}  // namespace ratelimiter
