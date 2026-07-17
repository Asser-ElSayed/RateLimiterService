#pragma once

#include <shared_mutex>
#include <string>

#include "RateLimitTypes.h"

namespace ratelimiter {

// Owns the currently active RateLimitConfig (limit, window, algorithm) and
// makes it safe to read from many worker threads while POST /config
// occasionally updates it. Reads take a shared lock (concurrent reads are
// fine); updates take an exclusive lock.
class ConfigManager {
public:
    explicit ConfigManager(RateLimitConfig initialConfig);

    RateLimitConfig get() const;

    // Validates and applies a new configuration. On success returns true and
    // leaves `outError` untouched. On failure returns false and sets
    // `outError` to a human-readable reason (used to build a 400 response).
    bool update(int limit, int64_t windowMs, Algorithm algorithm, std::string& outError);

private:
    mutable std::shared_mutex mutex_;
    RateLimitConfig config_;
};

}  // namespace ratelimiter
