#pragma once

#include <string>

#include "RateLimitTypes.h"

namespace ratelimiter {

// Strategy interface (Strategy design pattern). Each concrete algorithm
// (Fixed Window, Sliding Window, Token Bucket) implements this interface.
// RateLimiterCore depends only on this abstraction, so new algorithms can be
// added without changing the API layer or the Core dispatch logic.
class IRateLimitStrategy {
public:
    virtual ~IRateLimitStrategy() = default;

    // Evaluates whether a request for `key` is allowed under `config`, and
    // if so, atomically consumes one unit of quota. This is the mutating
    // operation used by POST /check.
    virtual RateLimitDecision checkAndConsume(const std::string& key,
                                               const RateLimitConfig& config) = 0;

    // Read-only view of the current limiter state for `key` under `config`,
    // without consuming any quota. Used by GET /status/{key}. If the key has
    // never been seen, implementations report a fresh/full-quota state.
    virtual RateLimitDecision getStatus(const std::string& key,
                                         const RateLimitConfig& config) const = 0;

    // Human-readable algorithm name, used in logs.
    virtual std::string name() const = 0;
};

}  // namespace ratelimiter
